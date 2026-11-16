#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace isaacnx {
// Steam CM's binary protobuf envelope, independent of the transport.
struct CmProtoFrame {
    uint32_t emsg = 0;
    std::vector<uint8_t> header;
    std::vector<uint8_t> body;
};
constexpr uint32_t kCmProtoFlag = 0x80000000u;
constexpr size_t kMaxCmHeader = 4096;
constexpr size_t kMaxCmFrame = 4 * 1024 * 1024;
bool encode_cm_proto_frame(uint32_t emsg,
                           const std::vector<uint8_t>& proto_header,
                           const std::vector<uint8_t>& proto_body,
                           std::vector<uint8_t>& frame,
                           std::string& error);
bool decode_cm_proto_frame(const uint8_t* bytes, size_t length,
                           CmProtoFrame& frame, std::string& error);
} // namespace isaacnx
