#pragma once
#include "isaacnx/cm_session_proto.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace isaacnx {
constexpr uint32_t kCmServiceMethodCall = 151;
constexpr uint32_t kCmServiceMethodResponse = 147;

struct CmManifestCodeReply {
    uint32_t eresult = 0; // Steam EResult
    uint64_t request_code = 0; // short-lived, do not persist
};

// Uses the currently logged-in Steam CM connection; job IDs must be unique
// among outstanding requests. Workshop app and depot are both 250900.
bool cm_build_manifest_code_request(const CmLogonResult& session,
                                    uint64_t manifest_id, uint64_t job_id,
                                    std::vector<uint8_t>& out,
                                    std::string& error);
bool cm_parse_manifest_code_reply(const uint8_t* bytes, size_t length,
                                  uint64_t expected_job_id,
                                  CmManifestCodeReply& reply,
                                  std::string& error);
} // namespace isaacnx
