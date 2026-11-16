#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace isaacnx {

// RFC 6455 encoding/validation, no networking or Steam credentials here.
// Steam CM's largest supported envelope is four MiB.
constexpr size_t kMaxWsPayload = 4 * 1024 * 1024 + 4096;
constexpr size_t kMaxWsHttpHeader = 8192;

struct WsFrame {
    uint8_t opcode = 0;
    bool fin = false;
    std::vector<uint8_t> payload;
};

enum class WsParseResult { Incomplete, Ready, Invalid };

// The nonce MUST come from an OS-seeded cryptographic source.
std::string ws_client_key(const uint8_t nonce[16]);
std::string ws_expected_accept(const std::string& client_key);

// Requires HTTP 101, Upgrade/Connection tokens, and correct SHA1 accept.
// consumed includes only the HTTP headers; remaining bytes may be WS frames.
bool ws_validate_upgrade(const std::string& raw, const std::string& key,
                         size_t& consumed, std::string& error);

// Every client-to-server frame MUST use a fresh cryptographic mask.
bool ws_encode_client_frame(uint8_t opcode, const std::vector<uint8_t>& payload,
                            const uint8_t mask[4], std::vector<uint8_t>& out,
                            std::string& error);

// The server MUST NOT mask frames. Input may contain truncated or multiple frames.
WsParseResult ws_decode_server_frame(const uint8_t* bytes, size_t length,
                                      WsFrame& frame, size_t& consumed,
                                      std::string& error);

} // namespace isaacnx
