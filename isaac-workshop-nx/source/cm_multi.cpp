#include "isaacnx/cm_multi.hpp"
#include "isaacnx/cm_wire.hpp"

#include <zlib.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace isaacnx {
namespace {
constexpr size_t kMaxMessages = 128;
constexpr unsigned kMaxDepth = 3;

bool varint(const std::vector<uint8_t>& buf, size_t& at, uint64_t& out) {
    out = 0;
    for (unsigned i = 0; i < 10; ++i) {
        if (at >= buf.size()) return false;
        uint8_t b = buf[at++];
        if (i == 9 && b > 1) return false;
        out |= uint64_t(b & 127) << (i * 7);
        if (!(b & 128)) return true;
    }
    return false;
}

bool read_multi_body(const std::vector<uint8_t>& body,
                     std::vector<uint8_t>& raw, uint64_t& expanded,
                     std::string& error) {
    bool found_data = false;
    bool found_size = false;
    expanded = 0;
    for (size_t at = 0; at < body.size();) {
        uint64_t key = 0;
        if (!varint(body, at, key) || key < 8 || key / 8 > 536870911) {
            error = "Steam CM: etiqueta Multi invalida"; return false;
        }
        const uint64_t field = key / 8, wire = key & 7;
        if (wire == 0) {
            uint64_t value = 0;
            if (!varint(body, at, value)) {
                error = "Steam CM: varint Multi invalido"; return false;
            }
            if (field == 1) {
                if (found_size || value > kMaxCmFrame) {
                    error = "Steam CM: tamano Multi invalido"; return false;
                }
                found_size = true;
                expanded = value;
            }
        } else if (wire == 2) {
            uint64_t size = 0;
            if (!varint(body, at, size) || size > body.size() - at) {
                error = "Steam CM: bytes Multi invalidos"; return false;
            }
            if (field == 2) {
                if (found_data || size > kMaxCmFrame) {
                    error = "Steam CM: payload Multi invalido"; return false;
                }
                found_data = true;
                raw.assign(body.begin() + at, body.begin() + at + size);
            }
            at += static_cast<size_t>(size);
        } else if (wire == 1 || wire == 5) {
            const size_t count = wire == 1 ? 8 : 4;
            if (body.size() - at < count) {
                error = "Steam CM: campo Multi truncado"; return false;
            }
            at += count;
        } else {
            error = "Steam CM: wire type Multi invalido"; return false;
        }
    }
    if (!found_data) { error = "Steam CM: sin payload Multi"; return false; }
    return true;
}

bool unzip_exact(const std::vector<uint8_t>& packed, size_t expected,
                 std::vector<uint8_t>& plain) {
    if (!expected || expected > kMaxCmFrame || packed.empty()) return false;
    plain.assign(expected, 0);
    z_stream zs {};
    zs.next_in = const_cast<Bytef*>(packed.data());
    zs.avail_in = static_cast<uInt>(packed.size());
    zs.next_out = plain.data();
    zs.avail_out = static_cast<uInt>(plain.size());
    if (inflateInit2(&zs, 15 + 16) != Z_OK) return false; // gzip only
    const int code = inflate(&zs, Z_FINISH);
    const bool valid = code == Z_STREAM_END &&
                       zs.total_out == expected && zs.avail_in == 0;
    inflateEnd(&zs);
    if (!valid) plain.clear();
    return valid;
}

uint32_t little32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
           uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

bool unpack(const uint8_t* bytes, size_t length, unsigned depth,
            std::vector<std::vector<uint8_t>>& messages, size_t& budget,
            std::string& error) {
    if (depth > kMaxDepth || messages.size() >= kMaxMessages ||
        length > kMaxCmFrame || length > budget) {
        error = "Steam CM: Multi excesivo"; return false;
    }
    CmProtoFrame envelope;
    if (!decode_cm_proto_frame(bytes, length, envelope, error)) return false;
    if (envelope.emsg != 1) {
        budget -= length;
        messages.emplace_back(bytes, bytes + length);
        return true;
    }
    std::vector<uint8_t> packed, plain;
    uint64_t expanded = 0;
    if (!read_multi_body(envelope.body, packed, expanded, error)) return false;
    if (expanded) {
        if (expanded > budget || !unzip_exact(packed, static_cast<size_t>(expanded), plain)) {
            error = "Steam CM: gzip Multi invalido"; return false;
        }
    } else plain.swap(packed);
    if (plain.size() > budget) {
        error = "Steam CM: Multi demasiado grande"; return false;
    }
    size_t at = 0;
    while (at < plain.size()) {
        if (plain.size() - at < 4) {
            error = "Steam CM: longitud de submensaje truncada"; return false;
        }
        const uint32_t n = little32(plain.data() + at);
        at += 4;
        if (n < 8 || n > plain.size() - at) {
            error = "Steam CM: submensaje Multi invalido"; return false;
        }
        if (!unpack(plain.data() + at, n, depth + 1, messages, budget, error))
            return false;
        at += n;
    }
    return true;
}
} // namespace

bool cm_unpack_messages(const uint8_t* frame, size_t length,
                        std::vector<std::vector<uint8_t>>& messages,
                        std::string& error) {
    messages.clear();
    size_t budget = kMaxCmFrame;
    if (!frame || length < 8) { error = "Steam CM: trama Multi vacia"; return false; }
    if (!unpack(frame, length, 0, messages, budget, error)) {
        messages.clear();
        return false;
    }
    error.clear();
    return true;
}
} // namespace isaacnx
