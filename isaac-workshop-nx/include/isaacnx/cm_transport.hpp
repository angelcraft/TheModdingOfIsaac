#pragma once
#include <string>
#include <cstdint>
#include <vector>

namespace isaacnx {

// Read-only diagnostic: opens a TLS socket to a validated Steam CM address,
// performs the RFC 6455 HTTP Upgrade, and closes it. No credentials, login,
// SteamPipe download, or filesystem changes are made.
bool probe_steam_cm_ws(const std::string& wss_url, std::string& error);

// Native binary-message transport for Steam CM. It retains libcurl's
// certificate-verified TLS connection until close() or destruction.
class CmWebSocket {
public:
    CmWebSocket() = default;
    ~CmWebSocket();
    CmWebSocket(const CmWebSocket&) = delete;
    CmWebSocket& operator=(const CmWebSocket&) = delete;

    bool connect(const std::string& wss_url, std::string& error);
    bool send_binary(const std::vector<uint8_t>& payload, std::string& error);
    bool recv_binary(std::vector<uint8_t>& payload, unsigned timeout_seconds,
                     std::string& error);
    void close();

private:
    bool send_frame(uint8_t opcode, const std::vector<uint8_t>& payload,
                    std::string& error);
    void* handle_ = nullptr; // owned CURL*; curl header stays private to .cpp
    std::vector<uint8_t> incoming_;
    std::vector<uint8_t> fragmented_;
    bool binary_fragment_open_ = false;
};

} // namespace isaacnx

