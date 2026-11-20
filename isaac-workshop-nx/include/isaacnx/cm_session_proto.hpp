#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace isaacnx {

// Steam CM EMsg and protobuf field identifiers verified against SteamTracking
// and SteamRE/SteamKit. Credentials are intentionally not accepted here yet.
constexpr uint32_t kCmClientHello = 9805;
constexpr uint32_t kCmClientLogon = 5514;
constexpr uint32_t kCmClientLogonResponse = 751;
constexpr uint32_t kCmClientHeartbeat = 703;
constexpr uint32_t kSteamProtocolVersion = 65580;

struct CmLogonResult {
    uint32_t result = 0; // Steam EResult::OK == 1
    uint32_t heartbeat_seconds = 0;
    uint64_t steamid = 0;
    int32_t client_sessionid = 0;
};

// Each function returns a complete CM protobuf envelope, to be sent as one
// WebSocket binary message. No passwords or tokens are serialized here.
bool cm_build_hello(std::vector<uint8_t>& out, std::string& error);
bool cm_build_anonymous_logon(std::vector<uint8_t>& out, std::string& error);
// Steam Guard QR must supply a short-lived client access_token. Caller must
// wipe all buffers (including out) as soon as CM sends the login request.
bool cm_build_token_logon(const std::string& account_name,
                          const std::string& access_token,
                          std::vector<uint8_t>& out, std::string& error);
bool cm_build_heartbeat(const CmLogonResult& session,
                        std::vector<uint8_t>& out, std::string& error);

// True only for a correctly bounded EMsg ClientLogOnResponse with a valid
// EResult field. A failed login returns true and result != 1.
bool cm_parse_logon_response(const uint8_t* bytes, size_t count,
                             CmLogonResult& result, std::string& error);

} // namespace isaacnx
