#include "isaacnx/cm_session_runtime.hpp"
#include "isaacnx/cm_session_proto.hpp"
#include "isaacnx/cm_transport.hpp"
#include "isaacnx/cm_wire.hpp"
#include "isaacnx/cm_multi.hpp"

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

bool probe_login(const std::string& wss_url, const std::string& account,
                 const std::string& token, bool authenticated,
                 std::string& error) {
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
            error.clear();
            return true;
        }
    }
    error = "Steam CM: no llego respuesta de login";
    return false;
}

} // namespace

bool cm_probe_anonymous_login(const std::string& wss_url, std::string& error) {
    return probe_login(wss_url, {}, {}, false, error);
}

bool cm_probe_token_login(const std::string& wss_url,
                          const std::string& account_name,
                          const std::string& access_token,
                          std::string& error) {
    return probe_login(wss_url, account_name, access_token, true, error);
}
} // namespace isaacnx
