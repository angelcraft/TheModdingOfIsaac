#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace isaacnx {

struct SteamManifestSummary {
    uint32_t depot_id = 0;
    uint64_t manifest_id = 0;
    uint32_t file_count = 0;
    bool filenames_encrypted = false;
};

// Accepts the raw SteamPipe protobuf envelope or a single-entry ZIP wrapper.
// This is a bounded structural check, NOT a cryptographic signature check.
// It does not download chunks, decrypt file names, or install any files.
bool inspect_steam_manifest(const uint8_t* bytes, size_t size,
                            uint32_t expected_depot, uint64_t expected_manifest,
                            SteamManifestSummary& summary, std::string& error);

} // namespace isaacnx
