#pragma once

#include "isaacnx/cm_session_proto.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace isaacnx {

constexpr uint32_t kCmGetDepotDecryptionKey = 5438;
constexpr uint32_t kCmGetDepotDecryptionKeyResponse = 5439;
constexpr uint32_t kIsaacWorkshopDepot = 250900;

// Holds an ephemeral Steam depot AES key. Do not log or persist it.
struct CmDepotKeyResult {
    uint32_t eresult = 0;
    uint32_t depot_id = 0;
    bool has_key = false;
    std::array<uint8_t, 32> key {};

    CmDepotKeyResult() = default;
    ~CmDepotKeyResult();
    CmDepotKeyResult(const CmDepotKeyResult&) = delete;
    CmDepotKeyResult& operator=(const CmDepotKeyResult&) = delete;
};

// Must only be sent after a successful Steam CM login. The source job ID is
// required so unrelated/untrusted responses cannot satisfy the request.
bool cm_build_depot_key_request(const CmLogonResult& session,
                                uint32_t app_id, uint32_t depot_id,
                                uint64_t source_job_id,
                                std::vector<uint8_t>& out,
                                std::string& error);

// Strictly validates the message type, target job ID, expected depot,
// EResult and key length. On success result owns the 32-byte key in RAM only.
bool cm_parse_depot_key_response(const uint8_t* data, size_t length,
                                 uint64_t expected_job_id,
                                 uint32_t expected_depot,
                                 CmDepotKeyResult& result,
                                 std::string& error);
} // namespace isaacnx
