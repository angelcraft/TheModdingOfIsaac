#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace isaacnx {

struct WorkshopItem {
    std::string id;
    std::string title;
    std::string download_url;
    uint32_t app_id = 0;
    uint64_t file_size = 0;
};

// Accepts a numeric Workshop ID or a steamcommunity.com sharedfiles URL.
bool parse_workshop_id(const std::string& input, std::string& id);

// Steam Web API response. Rejects errors and items from games other than Isaac.
bool parse_workshop_response(const std::string& json, const std::string& requested_id,
                             WorkshopItem& item, std::string& error);

// Returns a safe basename for an imported archive. Never a path component.
std::string safe_slug(const std::string& text);

bool is_directory(const std::string& path);
std::string select_installed_mod_root(const std::string& sd_root);

struct ZipEntry {
    std::string name;
    uint32_t local_header_offset = 0;
    uint32_t compressed_size = 0;
    uint32_t expanded_size = 0;
    uint32_t crc = 0;
    uint16_t compression = 0;
    bool directory = false;
};

// An intentionally strict ZIP importer: only stored/deflated files, no Zip64,
// encryption, traversal, symlinks, duplicates, or overwrites.
bool inspect_zip(const std::vector<uint8_t>& bytes, std::vector<ZipEntry>& files,
                 std::string& prefix, std::string& error);

bool install_zip(const std::string& archive_path, const std::string& mod_root,
                 const std::string& install_name, std::string& error);

} // namespace isaacnx
