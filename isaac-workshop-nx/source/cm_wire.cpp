#include "isaacnx/cm_wire.hpp"
#include <algorithm>

namespace isaacnx {
namespace {
uint32_t read_le32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8)
        | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
void write_le32(uint8_t* p, uint32_t v) {
    for (size_t i = 0; i < 4; ++i)
        p[i] = static_cast<uint8_t>(v >> (8 * i));
}
}
bool encode_cm_proto_frame(uint32_t emsg,
                           const std::vector<uint8_t>& proto_header,
                           const std::vector<uint8_t>& proto_body,
                           std::vector<uint8_t>& frame,
                           std::string& error) {
    frame.clear();
    if (emsg == 0 || (emsg & kCmProtoFlag)) {
        error = "Steam CM: tipo de mensaje invalido"; return false;
    }
    if (proto_header.size() > kMaxCmHeader ||
        proto_header.size() > kMaxCmFrame - 8 ||
        proto_body.size() > kMaxCmFrame - 8 - proto_header.size()) {
        error = "Steam CM: mensaje demasiado grande"; return false;
    }
    frame.resize(8 + proto_header.size() + proto_body.size());
    write_le32(frame.data(), emsg | kCmProtoFlag);
    write_le32(frame.data() + 4, static_cast<uint32_t>(proto_header.size()));
    std::copy(proto_header.begin(), proto_header.end(), frame.begin() + 8);
    std::copy(proto_body.begin(), proto_body.end(),
              frame.begin() + 8 + proto_header.size());
    error.clear(); return true;
}
bool decode_cm_proto_frame(const uint8_t* bytes, size_t length,
                           CmProtoFrame& frame, std::string& error) {
    frame = {};
    if (!bytes || length < 8 || length > kMaxCmFrame) {
        error = "Steam CM: longitud de trama invalida"; return false;
    }
    const uint32_t emsg = read_le32(bytes);
    const uint32_t header_size = read_le32(bytes + 4);
    if (!(emsg & kCmProtoFlag) || !(emsg & ~kCmProtoFlag) ||
        header_size > kMaxCmHeader || header_size > length - 8) {
        error = "Steam CM: encabezado protobuf invalido"; return false;
    }
    frame.emsg = emsg & ~kCmProtoFlag;
    frame.header.assign(bytes + 8, bytes + 8 + header_size);
    frame.body.assign(bytes + 8 + header_size, bytes + length);
    error.clear(); return true;
}
} // namespace isaacnx
