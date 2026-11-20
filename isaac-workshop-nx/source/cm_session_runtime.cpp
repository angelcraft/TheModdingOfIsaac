#include "isaacnx/cm_session_runtime.hpp"
#include "isaacnx/cm_session_proto.hpp"
#include "isaacnx/cm_transport.hpp"
#include "isaacnx/cm_wire.hpp"
#include "isaacnx/cm_multi.hpp"
#include "isaacnx/cm_depot.hpp"
#include "isaacnx/cm_manifest_code.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace isaacnx {
namespace {
void wipe(std::vector<uint8_t>& bytes) {
    volatile uint8_t* data = bytes.data();
    for (size_t i = 0; i < bytes.size(); ++i) data[i] = 0;
    bytes.clear();
}

bool probe_manifest_code(CmWebSocket& socket,
                         const CmLogonResult& session,
                         uint64_t manifest_id,
                         uint32_t& manifest_eresult,
                         std::string& error) {
    manifest_eresult = 0;
    constexpr uint64_t job_id = 0x49534141434e5832ULL;
    std::vector<uint8_t> request;
    if (!cm_build_manifest_code_request(
            session, manifest_id, job_id, request, error) ||
        !socket.send_binary(request, error))
        return false;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(20);
    unsigned received = 0;
    while (received < 24 && std::chrono::steady_clock::now() < deadline) {
        std::vector<uint8_t> bytes;
        if (!socket.recv_binary(bytes, 4, error)) {
            if (error == "Steam CM: sin mensajes antes del timeout") continue;
            return false;
        }
        ++received;
        std::vector<std::vector<uint8_t>> messages;
        if (!cm_unpack_messages(bytes.data(), bytes.size(), messages, error))
            return false;
        for (const auto& msg : messages) {
            if (msg.size() < 4) {
                error = "Steam CM: respuesta manifest truncada";
                return false;
            }
            const uint32_t type = uint32_t(msg[0]) |
                (uint32_t(msg[1]) << 8) | (uint32_t(msg[2]) << 16) |
                (uint32_t(msg[3]) << 24);
            if (type != (kCmServiceMethodResponse | kCmProtoFlag)) continue;
            CmManifestCodeReply reply;
            if (!cm_parse_manifest_code_reply(
                    msg.data(), msg.size(), job_id, reply, error))
                return false;
            manifest_eresult = reply.eresult;
            // Do not print, write or retain the Steam CDN request code.
            volatile uint64_t* secret = &reply.request_code;
            *secret = 0;
            error.clear();
            return true;
        }
    }
    error = "Steam CM: falta respuesta de manifest code";
    return false;
}

bool probe_login(const std::string& wss_url, const std::string& account,
                 const std::string& token, bool authenticated,
                 uint32_t* depot_eresult, uint64_t manifest_id,
                 uint32_t* manifest_eresult,
                 std::string& error) {
    if (depot_eresult) *depot_eresult = 0;
    if (manifest_eresult) *manifest_eresult = 0;
    CmWebSocket socket;
    if (!socket.connect(wss_url, error)) return false;
    std::vector<uint8_t> request;
    if (!cm_build_hello(request, error) ||
        !socket.send_binary(request, error)) return false;
    const bool built = authenticated
        ? cm_build_token_logon(account, token, request, error)
        : cm_build_anonymous_logon(request, error);
    if (!built) { wipe(request); return false; }
    const bool sent = socket.send_binary(request, error);
    wipe(request);
    if (!sent) return false;

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(25);
    unsigned received = 0;
    while (received < 32 && std::chrono::steady_clock::now() < deadline) {
        std::vector<uint8_t> bytes;
        if (!socket.recv_binary(bytes, 4, error)) {
            if (error == "Steam CM: sin mensajes antes del timeout") continue;
            return false;
        }
        ++received;
        std::vector<std::vector<uint8_t>> messages;
        if (!cm_unpack_messages(bytes.data(), bytes.size(), messages, error))
            return false;
        for (const auto& msg : messages) {
            CmProtoFrame frame;
            if (!decode_cm_proto_frame(msg.data(), msg.size(), frame, error))
                return false;
            if (frame.emsg != kCmClientLogonResponse) continue;
            CmLogonResult session;
            if (!cm_parse_logon_response(msg.data(), msg.size(), session, error))
                return false;
            if (session.result != 1) {
                error = "Steam CM: logon denegado, EResult " +
                        std::to_string(session.result);
                return false;
            }
            if (!depot_eresult) {
                error.clear();
                return true;
            }
            // Request the Isaac Workshop depot key while the authenticated
            // connection is still alive. A fixed job id is sufficient for
            // this single outstanding request on a one-shot connection.
            constexpr uint64_t job_id = 0x49534141434e5831ULL;
            if (!cm_build_depot_key_request(
                    session, 250900, kIsaacWorkshopDepot, job_id, request, error))
                return false;
            if (!socket.send_binary(request, error)) return false;
            request.clear();
            const auto key_deadline = std::chrono::steady_clock::now() +
                                      std::chrono::seconds(20);
            unsigned key_frames = 0;
            while (key_frames < 24 && std::chrono::steady_clock::now() < key_deadline) {
                std::vector<uint8_t> key_bytes;
                if (!socket.recv_binary(key_bytes, 4, error)) {
                    if (error == "Steam CM: sin mensajes antes del timeout") continue;
                    return false;
                }
                ++key_frames;
                std::vector<std::vector<uint8_t>> key_messages;
                if (!cm_unpack_messages(key_bytes.data(), key_bytes.size(),
                                        key_messages, error))
                    return false;
                for (const auto& key_msg : key_messages) {
                    if (key_msg.size() < 4) {
                        error = "Steam CM: respuesta depot truncada";
                        return false;
                    }
                    const uint32_t type = uint32_t(key_msg[0]) |
                        (uint32_t(key_msg[1]) << 8) |
                        (uint32_t(key_msg[2]) << 16) |
                        (uint32_t(key_msg[3]) << 24);
                    if (type != (kCmGetDepotDecryptionKeyResponse | kCmProtoFlag))
                        continue;
                    CmDepotKeyResult key;
                    if (!cm_parse_depot_key_response(key_msg.data(), key_msg.size(),
                                                     job_id, kIsaacWorkshopDepot,
                                                     key, error)) {
                        for (auto& msg : key_messages) wipe(msg);
                        wipe(key_bytes);
                        return false;
                    }
                    *depot_eresult = key.eresult;
                    const bool authorized_depot = key.has_key;
                    // The incoming CM frame can contain the key in several
                    // intermediate copies. Scrub all of them before return.
                    for (auto& msg : key_messages) wipe(msg);
                    wipe(key_bytes);
                    volatile uint8_t* key_copy = key.key.data();
                    for (size_t j = 0; j < key.key.size(); ++j) key_copy[j] = 0;
                    error.clear();
                    if (manifest_id && manifest_eresult && authorized_depot)
                        return probe_manifest_code(socket, session, manifest_id,
                                                   *manifest_eresult, error);
                    return true;
                }
            }
            error = "Steam CM: falta respuesta de clave depot";
            return false;
        }
    }
    error = "Steam CM: no llego respuesta de login";
    return false;
}

} // namespace

bool cm_probe_anonymous_login(const std::string& wss_url, std::string& error) {
    return probe_login(wss_url, {}, {}, false, nullptr, 0, nullptr, error);
}

bool cm_probe_token_login(const std::string& wss_url,
                          const std::string& account_name,
                          const std::string& access_token,
                          std::string& error) {
    return probe_login(wss_url, account_name, access_token, true,
                       nullptr, 0, nullptr, error);
}

bool cm_probe_token_depot_access(const std::string& wss_url,
                                 const std::string& account_name,
                                 const std::string& access_token,
                                 uint32_t& depot_eresult,
                                 std::string& error) {
    return probe_login(wss_url, account_name, access_token, true,
                       &depot_eresult, 0, nullptr, error);
}

bool cm_probe_token_manifest_access(const std::string& wss_url,
                                    const std::string& account_name,
                                    const std::string& access_token,
                                    uint64_t manifest_id,
                                    uint32_t& depot_eresult,
                                    uint32_t& manifest_eresult,
                                    std::string& error) {
    if (!manifest_id) {
        error = "Steam CM: ID manifest no disponible";
        return false;
    }
    return probe_login(wss_url, account_name, access_token, true,
                       &depot_eresult, manifest_id, &manifest_eresult, error);
}
} // namespace isaacnx

