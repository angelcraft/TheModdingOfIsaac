#include "isaacnx/cm_session_runtime.hpp"
#include "isaacnx/cm_session_proto.hpp"
#include "isaacnx/cm_transport.hpp"
#include "isaacnx/cm_wire.hpp"
#include "isaacnx/cm_multi.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace isaacnx {

bool cm_probe_anonymous_login(const std::string& wss_url, std::string& error) {
    CmWebSocket socket;
    if (!socket.connect(wss_url, error)) return false;
    std::vector<uint8_t> request;
    if (!cm_build_hello(request, error) ||
        !socket.send_binary(request, error)) return false;
    if (!cm_build_anonymous_logon(request, error) ||
        !socket.send_binary(request, error)) return false;

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
                error = "Steam CM: logon anonimo denegado, EResult " +
                        std::to_string(session.result);
                return false;
            }
            error.clear();
            return true;
        }
    }
    error = "Steam CM: no llego respuesta de login anonimo";
    return false;
}
} // namespace isaacnx
