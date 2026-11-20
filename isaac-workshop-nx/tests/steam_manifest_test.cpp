#include "isaacnx/steam_manifest.hpp"

#include <zlib.h>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

namespace {
void u32(std::vector<uint8_t>& b, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (i * 8)));
}
void u16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(uint8_t(v)); b.push_back(uint8_t(v >> 8));
}
void varint(std::vector<uint8_t>& b, uint64_t v) {
    while (v >= 128) { b.push_back(uint8_t(v) | 128); v >>= 7; }
    b.push_back(uint8_t(v));
}
void section(std::vector<uint8_t>& b, uint32_t magic,
             const std::vector<uint8_t>& payload) {
    u32(b, magic); u32(b, static_cast<uint32_t>(payload.size()));
    b.insert(b.end(), payload.begin(), payload.end());
}
std::vector<uint8_t> manifest(uint64_t gid) {
    std::vector<uint8_t> m;
    // Payload: one file mapping (field 1) containing a minimal filename.
    section(m, 0x71F617D0, {0x0a, 0x03, 0x0a, 0x01, 'x'});
    std::vector<uint8_t> metadata = {0x08};
    varint(metadata, 250900);
    metadata.push_back(0x10);
    varint(metadata, gid);
    metadata.push_back(0x20); metadata.push_back(0x01);
    section(m, 0x1F4812BE, metadata);
    section(m, 0x1B81B817, {0x0a, 0x01, 0x42});
    u32(m, 0x32C415AB);
    return m;
}
std::vector<uint8_t> zip_stored(const std::vector<uint8_t>& raw) {
    std::vector<uint8_t> b;
    const auto crc = crc32(0, raw.data(), static_cast<uInt>(raw.size()));
    const uint32_t sz = static_cast<uint32_t>(raw.size());
    u32(b, 0x04034b50); u16(b, 20); u16(b, 0); u16(b, 0);
    u16(b, 0); u16(b, 0); u32(b, crc); u32(b, sz); u32(b, sz);
    u16(b, 1); u16(b, 0); b.push_back('z');
    b.insert(b.end(), raw.begin(), raw.end());
    const uint32_t cdoff = static_cast<uint32_t>(b.size());
    u32(b, 0x02014b50); u16(b, 20); u16(b, 20); u16(b, 0); u16(b, 0);
    u16(b, 0); u16(b, 0); u32(b, crc); u32(b, sz); u32(b, sz);
    u16(b, 1); u16(b, 0); u16(b, 0); u16(b, 0); u16(b, 0);
    u32(b, 0); u32(b, 0); b.push_back('z');
    const uint32_t cdsize = static_cast<uint32_t>(b.size() - cdoff);
    u32(b, 0x06054b50); u16(b, 0); u16(b, 0); u16(b, 1); u16(b, 1);
    u32(b, cdsize); u32(b, cdoff); u16(b, 0);
    return b;
}
bool inspect(const std::vector<uint8_t>& data, uint64_t gid,
             isaacnx::SteamManifestSummary& s, std::string& error) {
    return isaacnx::inspect_steam_manifest(data.data(), data.size(),
                                          250900, gid, s, error);
}
} // namespace

int main() {
    constexpr uint64_t gid = 991234567890123ULL;
    const auto original = manifest(gid);
    isaacnx::SteamManifestSummary summary;
    std::string error;
    assert(inspect(original, gid, summary, error));
    assert(summary.depot_id == 250900 && summary.manifest_id == gid);
    assert(summary.file_count == 1 && summary.filenames_encrypted);
    assert(!inspect(original, gid + 1, summary, error));
    const auto zip = zip_stored(original);
    if (!inspect(zip, gid, summary, error)) {
        std::fprintf(stderr, "ZIP rejected: %s\n", error.c_str());
        return 1;
    }
    auto damaged = zip;
    damaged[36] ^= 1;
    assert(!inspect(damaged, gid, summary, error));
    damaged = original;
    damaged.pop_back();
    assert(!inspect(damaged, gid, summary, error));
    damaged = original;
    damaged[0] ^= 1;
    assert(!inspect(damaged, gid, summary, error));
    // Deterministic malformed-input sweep; acceptance is allowed if a mutated
    // byte belongs to an ignored protobuf field, but crashes are not.
    uint32_t rng = 0x49434141;
    for (unsigned i = 0; i < 20000; ++i) {
        damaged = (i & 1) ? original : zip;
        rng = rng * 1664525u + 1013904223u;
        const size_t offset = rng % damaged.size();
        rng = rng * 1664525u + 1013904223u;
        damaged[offset] ^= uint8_t(rng >> 24);
        inspect(damaged, gid, summary, error);
    }
    return 0;
}
