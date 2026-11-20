#include "isaacnx/steam_manifest.hpp"

#include <zlib.h>
#include <vector>

namespace isaacnx {
namespace {
constexpr size_t kMaxWire = 8 * 1024 * 1024;
constexpr size_t kMaxRaw = 24 * 1024 * 1024;
constexpr uint32_t kPayload = 0x71F617D0;
constexpr uint32_t kMetadata = 0x1F4812BE;
constexpr uint32_t kSignature = 0x1B81B817;
constexpr uint32_t kEnd = 0x32C415AB;

bool fits(size_t pos, size_t amount, size_t total) {
    return pos <= total && amount <= total - pos;
}
uint16_t rd16(const uint8_t* b, size_t p) {
    return uint16_t(b[p]) | (uint16_t(b[p + 1]) << 8);
}
uint32_t rd32(const uint8_t* b, size_t p) {
    return uint32_t(rd16(b, p)) | (uint32_t(rd16(b, p + 2)) << 16);
}
bool varint(const uint8_t* b, size_t n, size_t& p, uint64_t& v) {
    v = 0;
    for (unsigned shift = 0; shift < 70; shift += 7) {
        if (p == n) return false;
        const uint8_t ch = b[p++];
        if (shift == 63 && ch > 1) return false;
        v |= uint64_t(ch & 127) << shift;
        if (!(ch & 128)) return true;
    }
    return false;
}
bool field(const uint8_t* b, size_t n, size_t& p, uint32_t& tag,
           uint32_t& wire, uint64_t& value) {
    uint64_t key = 0;
    if (!varint(b, n, p, key) || key < 8 || key / 8 > 536870911) return false;
    tag = uint32_t(key >> 3);
    wire = uint32_t(key & 7);
    value = 0;
    if (wire == 0) return varint(b, n, p, value);
    if (wire == 1) {
        if (!fits(p, 8, n)) return false;
        p += 8;
        return true;
    }
    if (wire == 2) {
        if (!varint(b, n, p, value) || value > n - p) return false;
        p += size_t(value);
        return true;
    }
    if (wire == 5) {
        if (!fits(p, 4, n)) return false;
        p += 4;
        return true;
    }
    return false;
}
bool parse_metadata(const uint8_t* b, size_t n,
                    SteamManifestSummary& result) {
    bool depot = false, manifest = false, encrypted = false;
    for (size_t p = 0; p < n;) {
        uint32_t tag = 0, wire = 0;
        uint64_t value = 0;
        if (!field(b, n, p, tag, wire, value)) return false;
        if (tag == 1) {
            if (depot || wire != 0 || !value || value > UINT32_MAX) return false;
            depot = true;
            result.depot_id = uint32_t(value);
        } else if (tag == 2) {
            if (manifest || wire != 0 || !value) return false;
            manifest = true;
            result.manifest_id = value;
        } else if (tag == 4) {
            if (encrypted || wire != 0 || value > 1) return false;
            encrypted = true;
            result.filenames_encrypted = value == 1;
        }
    }
    return depot && manifest;
}
bool count_files(const uint8_t* b, size_t n, SteamManifestSummary& result) {
    for (size_t p = 0; p < n;) {
        uint32_t tag = 0, wire = 0;
        uint64_t value = 0;
        const size_t start = p;
        if (!field(b, n, p, tag, wire, value)) return false;
        if (tag == 1) {
            // A FileMapping is a protobuf message, not a raw filename.
            if (wire != 2 || !value || ++result.file_count > 20000 ||
                p - start < value) return false;
        }
    }
    return result.file_count != 0;
}
bool decompress_single_zip(const uint8_t* b, size_t size,
                           std::vector<uint8_t>& raw) {
    if (size < 52) return false;
    // Scan for an EOCD with exactly one entry and no archive comment.
    size_t eocd = size;
    const size_t first = size > 65557 ? size - 65557 : 0;
    for (size_t p = size - 22;; --p) {
        if (rd32(b, p) == 0x06054b50 && rd16(b, p + 20) <= size - (p + 22) &&
            p + 22 + rd16(b, p + 20) == size) { eocd = p; break; }
        if (p == first) break;
    }
    if (eocd == size || rd16(b, eocd + 4) != 0 || rd16(b, eocd + 6) != 0 ||
        rd16(b, eocd + 8) != 1 || rd16(b, eocd + 10) != 1) return false;
    const size_t cd = rd32(b, eocd + 16);
    const size_t cd_len = rd32(b, eocd + 12);
    if (cd_len < 46 || !fits(cd, cd_len, eocd) ||
        !fits(cd, 46, eocd) || rd32(b, cd) != 0x02014b50) return false;
    const uint16_t flags = rd16(b, cd + 8);
    const uint16_t method = rd16(b, cd + 10);
    const size_t compressed = rd32(b, cd + 20);
    const size_t expanded = rd32(b, cd + 24);
    const size_t name = rd16(b, cd + 28);
    const size_t extra = rd16(b, cd + 30);
    const size_t comment = rd16(b, cd + 32);
    const size_t local = rd32(b, cd + 42);
    if (flags & 1 || flags & ~(uint16_t(8) | uint16_t(2048)) ||
        (method != 0 && method != 8) || !expanded || expanded > kMaxRaw ||
        !fits(cd + 46, name + extra + comment, eocd) ||
        cd_len != 46 + name + extra + comment ||
        name != 1 || b[cd + 46] != 'z' ||
        !fits(local, 30, size) || rd32(b, local) != 0x04034b50 ||
        rd16(b, local + 8) != method || rd16(b, local + 6) != flags ||
        rd16(b, local + 26) != name) return false;
    const size_t data = local + 30 + rd16(b, local + 26) + rd16(b, local + 28);
    if (!fits(local + 30, rd16(b, local + 26) + rd16(b, local + 28), size) ||
        b[local + 30] != 'z' || !fits(data, compressed, size) ||
        data + compressed > cd) return false;
    raw.resize(expanded);
    if (method == 0) {
        if (compressed != expanded) return false;
        for (size_t i = 0; i < expanded; ++i) raw[i] = b[data + i];
    } else {
        z_stream z {};
        if (inflateInit2(&z, -MAX_WBITS) != Z_OK) return false;
        z.next_in = const_cast<Bytef*>(b + data);
        z.avail_in = static_cast<uInt>(compressed);
        z.next_out = raw.data();
        z.avail_out = static_cast<uInt>(expanded);
        const int status = inflate(&z, Z_FINISH);
        const bool ok = status == Z_STREAM_END &&
                        z.total_out == expanded && z.total_in == compressed;
        inflateEnd(&z);
        if (!ok) return false;
    }
    return crc32(0, raw.data(), static_cast<uInt>(expanded)) == rd32(b, cd + 16);
}
} // namespace

bool inspect_steam_manifest(const uint8_t* bytes, size_t size,
                            uint32_t expected_depot, uint64_t expected_manifest,
                            SteamManifestSummary& summary, std::string& error) {
    summary = {};
    if (!bytes || !expected_depot || !expected_manifest || size < 28 ||
        size > kMaxWire) {
        error = "Steam CDN: tamano de manifest invalido";
        return false;
    }
    std::vector<uint8_t> raw;
    if (rd32(bytes, 0) == 0x04034b50) {
        if (!decompress_single_zip(bytes, size, raw)) {
            error = "Steam CDN: ZIP de manifest invalido";
            return false;
        }
        bytes = raw.data();
        size = raw.size();
    }
    size_t p = 0;
    uint32_t expected[] = {kPayload, kMetadata, kSignature};
    for (uint32_t kind : expected) {
        if (!fits(p, 8, size) || rd32(bytes, p) != kind) {
            error = "Steam CDN: secciones de manifest incorrectas";
            return false;
        }
        const size_t len = rd32(bytes, p + 4);
        p += 8;
        if (!len || !fits(p, len, size)) {
            error = "Steam CDN: seccion truncada";
            return false;
        }
        bool valid = true;
        if (kind == kPayload) valid = count_files(bytes + p, len, summary);
        if (kind == kMetadata) valid = parse_metadata(bytes + p, len, summary);
        if (!valid) {
            error = "Steam CDN: protobuf de manifest invalido";
            return false;
        }
        p += len;
    }
    if (!fits(p, 4, size) || p + 4 != size || rd32(bytes, p) != kEnd ||
        summary.depot_id != expected_depot ||
        summary.manifest_id != expected_manifest) {
        error = "Steam CDN: ID o final del manifest no coincide";
        return false;
    }
    error.clear();
    return true;
}
} // namespace isaacnx
