#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <utility>

namespace isaacnx {

struct WorkshopItem {
    std::string id;
    std::string title;
    std::string download_url;
    uint32_t app_id = 0;
    uint64_t file_size = 0;
    std::string preview_url;
    std::string description;
    // SteamPipe Workshop content uses a manifest ID, not a direct URL.
    uint64_t content_manifest_id = 0;
    uint32_t workshop_file_type = 0;
};

enum class WorkshopDelivery { Unavailable, DirectHttps, SteamPipeManifest };
// Classification only: a manifest ID is not a downloadable URL or CDN ticket.
WorkshopDelivery workshop_delivery(const WorkshopItem& item);

// Parse Steam's public CM directory response; returns verified WSS URLs only.
// This does not authenticate, connect, or download content.
bool parse_cm_directory(const std::string& json, std::vector<std::string>& urls,
                        std::string& error);

// Parse Valve's SteamPipe CDN directory into HTTPS origins. Only trusted
// Valve-owned hostname suffixes are accepted; no arbitrary redirect targets.
bool parse_steam_content_directory(const std::string& json,
                                   std::vector<std::string>& origins,
                                   std::string& error);

// Steam's QR login is a separate, experimental step from content downloading.
// request_id is an opaque secret: never print or include it in the QR.
struct SteamQrChallenge {
    std::string client_id;
    std::string request_id;
    std::string challenge_url;
    unsigned interval_seconds = 5;
};

struct SteamQrPoll {
    bool confirmed = false;
    bool interaction = false;
    std::string account_name;
    std::string new_client_id;
    std::string new_challenge_url;
    std::string access_token; // transient Steam Guard QR client token
    SteamQrPoll() = default;
    SteamQrPoll(const SteamQrPoll&) = delete;
    SteamQrPoll& operator=(const SteamQrPoll&) = delete;
    SteamQrPoll(SteamQrPoll&&) noexcept = default;
    SteamQrPoll& operator=(SteamQrPoll&& other) noexcept {
        if (this != &other) {
            clear_secret();
            confirmed = other.confirmed;
            interaction = other.interaction;
            account_name = std::move(other.account_name);
            new_client_id = std::move(other.new_client_id);
            new_challenge_url = std::move(other.new_challenge_url);
            access_token = std::move(other.access_token);
        }
        return *this;
    }
    void clear_secret() noexcept {
        volatile char* bytes = access_token.data();
        for (size_t i = 0; i < access_token.size(); ++i) bytes[i] = 0;
        access_token.clear();
    }
    ~SteamQrPoll() { clear_secret(); }
};

// Validate bounded auth JSON. QR poll keeps the access token only in memory;
// never log it or persist it. The SteamQrPoll destructor wipes it.
bool parse_steam_qr_begin(const std::string& json, SteamQrChallenge& challenge,
                          std::string& error);
bool parse_steam_qr_poll(const std::string& json, SteamQrPoll& poll,
                         std::string& error);

// Accepts a numeric Workshop ID or a steamcommunity.com sharedfiles URL.
bool parse_workshop_id(const std::string& input, std::string& id);

// Steam Web API response. Rejects errors and items from games other than Isaac.
bool parse_workshop_response(const std::string& json, const std::string& requested_id,
                             WorkshopItem& item, std::string& error);

// Parse IDs from Steam's public Workshop browse HTML. HTML is only used for
// discovery; all results are rechecked against Isaac's AppID by the Web API.
bool parse_workshop_listing(const std::string& html, std::vector<std::string>& ids,
                            std::string& error);

// Metadata from a single bulk Web API response. Results are returned in the
// requested order; unavailable/other-game items are discarded.
bool parse_workshop_batch_response(const std::string& json,
                                   const std::vector<std::string>& requested_ids,
                                   std::vector<WorkshopItem>& items,
                                   std::string& error);

// A local-only Android bridge endpoint, e.g. http://192.168.1.10:8765/TOKEN.
// No DNS, credentials, redirects, public IPs or arbitrary ports are accepted.
bool parse_bridge_endpoint(const std::string& config, std::string& endpoint,
                           std::string& error);

// Strip control characters and bound text returned by the Workshop before
// printing it to libnx's console.
std::string safe_console_text(const std::string& text, size_t max_bytes);

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

