#include "isaacnx/core.hpp"
#include "isaacnx/cm_transport.hpp"
#include "isaacnx/cm_session_runtime.hpp"
#include "isaacnx/steam_manifest.hpp"
#include "qrcodegen.h"

#include <switch.h>
#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <utility>

namespace {
constexpr const char* kAppRoot = "sdmc:/switch/isaac_workshop_nx";
constexpr const char* kInbox = "sdmc:/switch/isaac_workshop_nx/inbox";
constexpr const char* kBridgeConfig = "sdmc:/switch/isaac_workshop_nx/bridge.txt";
constexpr size_t kMaxDownload = 32 * 1024 * 1024;
constexpr size_t kMaxMetadata = 1024 * 1024;
constexpr size_t kMaxBrowse = 3 * 1024 * 1024;
constexpr size_t kMaxAuthResponse = 16 * 1024;
constexpr size_t kMaxManifestWire = 8 * 1024 * 1024;
std::string message = "Listo. Cierra Isaac antes de instalar.";

struct MemoryBuffer { std::string data; size_t limit = kMaxMetadata; bool excessive = false; };
struct FileWriter { FILE* file; size_t length = 0; bool excessive = false; };

size_t memory_write(char* ptr, size_t size, size_t count, void* user) {
    auto* dst = static_cast<MemoryBuffer*>(user);
    if (count && size > SIZE_MAX / count) return 0;
    const size_t n = size * count;
    if (dst->data.size() > dst->limit || n > dst->limit - dst->data.size()) {
        dst->excessive = true; return 0;
    }
    dst->data.append(ptr, n);
    return n;
}

size_t file_write(char* ptr, size_t size, size_t count, void* user) {
    auto* dst = static_cast<FileWriter*>(user);
    if (count && size > SIZE_MAX / count) return 0;
    const size_t n = size * count;
    if (dst->length > kMaxDownload || n > kMaxDownload - dst->length) {
        dst->excessive = true; return 0;
    }
    const size_t written = fwrite(ptr, 1, n, dst->file);
    dst->length += written;
    return written;
}

void common_curl(CURL* h) {
    curl_easy_setopt(h, CURLOPT_USERAGENT, "IsaacWorkshopNX/0.2 (Switch homebrew)");
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_MAXREDIRS, 3L);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(h, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(h, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    curl_easy_setopt(h, CURLOPT_PROTOCOLS, long(CURLPROTO_HTTPS));
    curl_easy_setopt(h, CURLOPT_REDIR_PROTOCOLS, long(CURLPROTO_HTTPS));
#endif
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(h, CURLOPT_TIMEOUT, 40L);
    curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, "");
}

// Uses Valve's public authentication service; no third-party QR website,
// password, cookies, or personal Steam token is sent to another host.
bool steam_auth_post(const char* url, const std::string& input,
                     std::string& body, std::string& error) {
    CURL* curl = curl_easy_init();
    if (!curl) { error = "Steam Guard: no pude iniciar HTTPS"; return false; }
    char* encoded = curl_easy_escape(curl, input.c_str(), static_cast<int>(input.size()));
    if (!encoded) { curl_easy_cleanup(curl); error = "Steam Guard: fallo al preparar solicitud"; return false; }
    const std::string form = "input_json=" + std::string(encoded);
    curl_free(encoded);
    MemoryBuffer response;
    response.limit = kMaxAuthResponse;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, form.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memory_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    common_curl(curl);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    const CURLcode result = curl_easy_perform(curl);
    long http = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK || http != 200 || response.excessive) {
        error = "Steam Guard: HTTP " + std::to_string(http) +
                " / curl " + std::to_string(static_cast<int>(result));
        return false;
    }
    body.swap(response.data);
    return true;
}

bool steam_qr_begin(isaacnx::SteamQrChallenge& challenge, std::string& error) {
    // Platform 1 = Steam client. Future native CM/CDN support needs a client
    // token, not browser cookies. Do not claim to be Valve's official client.
    const std::string json = R"({"device_friendly_name":"Isaac Workshop NX Switch","platform_type":1,"device_details":{"device_friendly_name":"Isaac Workshop NX Switch","platform_type":1,"os_type":20},"website_id":"Client"})";
    std::string response;
    if (!steam_auth_post("https://api.steampowered.com/IAuthenticationService/BeginAuthSessionViaQR/v1/",
                         json, response, error)) return false;
    const bool ok = isaacnx::parse_steam_qr_begin(response, challenge, error);
    std::fill(response.begin(), response.end(), '\0');
    return ok;
}

bool steam_qr_poll(const isaacnx::SteamQrChallenge& challenge,
                   isaacnx::SteamQrPoll& poll, std::string& error) {
    const std::string json = "{\"client_id\":\"" + challenge.client_id +
                             "\",\"request_id\":\"" + challenge.request_id + "\"}";
    std::string response;
    if (!steam_auth_post("https://api.steampowered.com/IAuthenticationService/PollAuthSessionStatus/v1/",
                         json, response, error)) return false;
    const bool ok = isaacnx::parse_steam_qr_poll(response, poll, error);
    // The response can contain access/refresh tokens. This proof of concept
    // checks approval only; never persists or prints those credentials.
    std::fill(response.begin(), response.end(), '\0');
    return ok;
}

void steam_qr_display(const isaacnx::SteamQrChallenge& challenge) {
    consoleClear();
    printf("ISAAC WORKSHOP NX | STEAM GUARD EXPERIMENTAL\n");
    printf("Abre Steam Guard en el movil y escanea el QR:\n");
    uint8_t temp[qrcodegen_BUFFER_LEN_FOR_VERSION(4)] {};
    uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(4)] {};
    const bool encoded = qrcodegen_encodeText(challenge.challenge_url.c_str(), temp, qr,
        qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN, 4, qrcodegen_Mask_AUTO, false);
    if (encoded) {
        const int size = qrcodegen_getSize(qr);
        const int margin = size <= 29 ? 4 : 3;
        for (int y = -margin; y < size + margin; ++y) {
            for (int x = -margin; x < size + margin; ++x)
                printf("%s", qrcodegen_getModule(qr, x, y) ? "\x1b[40m  " : "\x1b[47m  ");
            printf("\x1b[0m\n");
        }
    } else {
        printf("QR demasiado largo para la pantalla.\n");
        printf("Enlace oficial: %.130s\n", challenge.challenge_url.c_str());
    }
    printf("[B] Cancelar | QR: validar acceso al depot Steam\n");
    consoleUpdate(nullptr);
}

bool fetch_steam_cm_servers(std::vector<std::string>& endpoints, std::string& error);
bool fetch_steam_content_servers(std::vector<std::string>& origins, std::string& error);

bool inspect_remote_steam_manifest(const std::vector<std::string>& origins,
                                   uint64_t manifest_id, uint64_t request_code,
                                   isaacnx::SteamManifestSummary& summary,
                                   std::string& error) {
    if (!manifest_id || !request_code || origins.empty()) {
        error = "Steam CDN: faltan datos del manifiesto";
        return false;
    }
    const std::string suffix = "/depot/250900/manifest/" +
        std::to_string(manifest_id) + "/5/" + std::to_string(request_code);
    for (size_t i = 0; i < origins.size() && i < 4; ++i) {
        CURL* h = curl_easy_init();
        if (!h) { error = "Steam CDN: no pude iniciar HTTPS"; return false; }
        MemoryBuffer body;
        body.limit = kMaxManifestWire;
        const std::string url = origins[i] + suffix;
        curl_easy_setopt(h, CURLOPT_URL, url.c_str());
        curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, memory_write);
        curl_easy_setopt(h, CURLOPT_WRITEDATA, &body);
        common_curl(h);
        curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(h, CURLOPT_TIMEOUT, 60L);
        const CURLcode result = curl_easy_perform(h);
        long http = 0;
        curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &http);
        curl_easy_cleanup(h);
        if (result != CURLE_OK || http != 200 || body.excessive) {
            error = "Steam CDN: servidor " + std::to_string(i + 1) +
                    " HTTP " + std::to_string(http) + " curl " +
                    std::to_string(static_cast<int>(result));
            continue;
        }
        if (isaacnx::inspect_steam_manifest(
                reinterpret_cast<const uint8_t*>(body.data.data()),
                body.data.size(), 250900, manifest_id, summary, error)) return true;
        // Never accept a different manifest just because HTTP returned 200.
    }
    return false;
}

void wipe_session_token(std::string& token) {
    volatile char* bytes = token.data();
    for (size_t i = 0; i < token.size(); ++i) bytes[i] = 0;
    token.clear();
}

void steam_qr_experiment(PadState* pad, uint64_t manifest_id = 0) {
    consoleClear(); printf("Solicitando QR a Steam...\n"); consoleUpdate(nullptr);
    isaacnx::SteamQrChallenge challenge;
    std::string error;
    if (!steam_qr_begin(challenge, error)) { message = error; return; }
    steam_qr_display(challenge);
    bool interaction = false;
    bool completed = false;
    bool cancelled = false;
    std::string approved_access_token;
    std::string approved_account;
    const auto expiry = std::chrono::steady_clock::now() + std::chrono::minutes(5);
    auto next_poll = std::chrono::steady_clock::now() +
                     std::chrono::seconds(challenge.interval_seconds);
    while (appletMainLoop() && std::chrono::steady_clock::now() < expiry) {
        padUpdate(pad);
        if (padGetButtonsDown(pad) & (HidNpadButton_B | HidNpadButton_Plus)) {
            cancelled = true; break;
        }
        if (std::chrono::steady_clock::now() >= next_poll) {
            isaacnx::SteamQrPoll result;
            if (!steam_qr_poll(challenge, result, error)) break;
            interaction = interaction || result.interaction;
            if (result.confirmed) {
                approved_access_token = std::move(result.access_token);
                approved_account = result.account_name;
                completed = true;
                break;
            }
            if (!result.new_client_id.empty()) challenge.client_id = result.new_client_id;
            if (!result.new_challenge_url.empty() && result.new_challenge_url != challenge.challenge_url) {
                challenge.challenge_url = result.new_challenge_url;
                steam_qr_display(challenge);
            }
            next_poll = std::chrono::steady_clock::now() +
                        std::chrono::seconds(challenge.interval_seconds);
        }
        svcSleepThread(100'000'000);
    }
    std::fill(challenge.request_id.begin(), challenge.request_id.end(), '\0');
    if (completed) {
        consoleClear();
        printf("Steam Guard confirmado. Probando Steam CM + depot...\n");
        printf("Token solo en RAM; se borra al terminar.\n");
        consoleUpdate(nullptr);
        std::vector<std::string> urls;
        std::vector<std::string> cdns;
        bool ok = (!manifest_id || fetch_steam_content_servers(cdns, error)) &&
                  fetch_steam_cm_servers(urls, error);
        if (ok) {
            bool connected = false;
            uint32_t depot_result = 0;
            uint32_t manifest_result = 0;
            bool manifest_downloaded = false;
            isaacnx::SteamManifestSummary summary;
            for (size_t i = 0; i < urls.size() && i < 2; ++i) {
                isaacnx::CmManifestTicket ticket;
                const bool tested = manifest_id
                    ? isaacnx::cm_authorize_token_manifest(
                          urls[i], approved_account, approved_access_token,
                          manifest_id, ticket, error)
                    : isaacnx::cm_probe_token_depot_access(
                          urls[i], approved_account, approved_access_token,
                          depot_result, error);
                if (tested) {
                    if (manifest_id) {
                        depot_result = ticket.depot_eresult;
                        manifest_result = ticket.manifest_eresult;
                        if (depot_result == 1 && manifest_result == 1)
                            manifest_downloaded = inspect_remote_steam_manifest(
                                cdns, manifest_id, ticket.request_code, summary, error);
                    }
                    connected = true;
                    break;
                }
            }
            if (connected && manifest_downloaded)
                message = "Steam CDN OK: " + std::to_string(summary.file_count) +
                          " archivos en manifest. Faltan chunks e instalacion.";
            else if (connected && manifest_id && depot_result == 1 &&
                     manifest_result == 1)
                message = "Steam CM OK, pero CDN: " + error;
            else if (connected && manifest_id && depot_result == 1)
                message = "Steam: depot OK, manifest EResult " +
                          std::to_string(manifest_result);
            else if (connected && depot_result == 1)
                message = "Steam CM: depot Workshop autorizado! Falta manifest/CDN.";
            else if (connected)
                message = "Steam CM: login OK; depot denegado, EResult " +
                          std::to_string(depot_result);
            else message = "Steam Guard OK; login CM: " + error;
        } else message = "Steam Guard OK; directorio CM: " + error;
    } else if (cancelled)
        message = "Steam Guard cancelado. No se guardo ninguna sesion.";
    else if (!error.empty())
        message = error;
    else
        message = interaction ? "Steam Guard: hubo interaccion pero no confirmacion." :
                                "Steam Guard: QR agotado tras 5 minutos.";
    wipe_session_token(approved_access_token);
}

bool lookup_many(const std::vector<std::string>& ids,
                 std::vector<isaacnx::WorkshopItem>& items, std::string& error) {
    if (ids.empty() || ids.size() > 20) { error = "Lista de IDs invalida"; return false; }
    CURL* curl = curl_easy_init();
    if (!curl) { error = "No se pudo iniciar curl"; return false; }
    std::string form = "itemcount=" + std::to_string(ids.size());
    for (size_t i = 0; i < ids.size(); ++i)
        form += "&publishedfileids%5B" + std::to_string(i) + "%5D=" + ids[i];
    MemoryBuffer response;
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.steampowered.com/ISteamRemoteStorage/GetPublishedFileDetails/v1/");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, form.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memory_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    common_curl(curl);
    CURLcode status = curl_easy_perform(curl);
    long http = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(curl);
    if (status != CURLE_OK || http != 200) {
        error = response.excessive ? "Lista demasiado grande" :
                "No pude consultar Steam (HTTP " + std::to_string(http) + ")";
        return false;
    }
    return isaacnx::parse_workshop_batch_response(response.data, ids, items, error);
}

bool search_workshop(const std::string& query, std::vector<isaacnx::WorkshopItem>& items,
                     std::string& error) {
    CURL* curl = curl_easy_init();
    if (!curl) { error = "No se pudo iniciar curl"; return false; }
    char* escaped = curl_easy_escape(curl, query.c_str(), static_cast<int>(query.size()));
    if (!escaped) { curl_easy_cleanup(curl); error = "No se pudo codificar busqueda"; return false; }
    const std::string url = "https://steamcommunity.com/workshop/browse/?appid=250900"
                            "&browsesort=textsearch&section=readytouseitems&l=english&searchtext=" +
                            std::string(escaped);
    curl_free(escaped);
    MemoryBuffer response;
    response.limit = kMaxBrowse;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memory_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    common_curl(curl);
    CURLcode status = curl_easy_perform(curl);
    long http = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(curl);
    if (status != CURLE_OK || http != 200) {
        error = response.excessive ? "Pagina Steam demasiado grande" :
                "Steam Workshop no disponible (HTTP " + std::to_string(http) + ")";
        return false;
    }
    std::vector<std::string> ids;
    if (!isaacnx::parse_workshop_listing(response.data, ids, error)) return false;
    return lookup_many(ids, items, error);
}

bool read_workshop(const std::string& id, isaacnx::WorkshopItem& item, std::string& error) {
    CURL* curl = curl_easy_init();
    if (!curl) { error = "No se pudo iniciar curl"; return false; }
    const std::string form = "itemcount=1&publishedfileids%5B0%5D=" + id;
    MemoryBuffer response;
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.steampowered.com/ISteamRemoteStorage/GetPublishedFileDetails/v1/");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, form.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memory_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    common_curl(curl);
    const CURLcode status = curl_easy_perform(curl);
    long http = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(curl);
    if (status != CURLE_OK || http != 200) {
        error = response.excessive ? "Respuesta API demasiado grande" :
                "No pude consultar Steam (HTTP " + std::to_string(http) + ")";
        return false;
    }
    return isaacnx::parse_workshop_response(response.data, id, item, error);
}

bool download_public_zip(const isaacnx::WorkshopItem& item, std::string& path, std::string& error) {
    // Browsing public metadata is not Steam account authentication. Workshop
    // typically does NOT return a directly downloadable file_url.
    if (item.download_url.compare(0, 8, "https://") != 0) {
        error = "Steam no publica descarga directa. Importa ZIP desde inbox.";
        return false;
    }
    path = std::string(kInbox) + "/steam_" + item.id + ".zip";
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) { error = "Ya existe ZIP en inbox, o fallo la SD"; return false; }
    FILE* f = fdopen(fd, "wb");
    if (!f) { close(fd); unlink(path.c_str()); error = "No pude preparar el ZIP"; return false; }
    CURL* curl = curl_easy_init();
    if (!curl) { fclose(f); unlink(path.c_str()); error = "Fallo curl"; return false; }
    FileWriter writer {f};
    curl_easy_setopt(curl, CURLOPT_URL, item.download_url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, file_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writer);
    common_curl(curl);
    CURLcode status = curl_easy_perform(curl);
    long http = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(curl);
    bool close_ok = fclose(f) == 0;
    if (status != CURLE_OK || http != 200 || !close_ok || writer.excessive) {
        unlink(path.c_str()); error = "Descarga fallida, incompleta o >32 MiB"; return false;
    }
    // HTML responses must not be treated as mods.
    f = fopen(path.c_str(), "rb");
    unsigned char signature[4] {};
    bool is_zip = f && fread(signature, 1, 4, f) == 4 &&
                  signature[0] == 'P' && signature[1] == 'K' &&
                  signature[2] == 3 && signature[3] == 4;
    if (f) fclose(f);
    if (!is_zip) { unlink(path.c_str()); error = "El servidor no entrego un ZIP"; return false; }
    return true;
}

bool read_bridge_config(std::string& endpoint, std::string& error) {
    FILE* file = fopen(kBridgeConfig, "rb");
    if (!file) {
        error = "Falta bridge.txt de tu Android en switch/isaac_workshop_nx";
        return false;
    }
    char line[160] {};
    bool ok = fgets(line, sizeof(line), file) != nullptr && fgetc(file) == EOF;
    fclose(file);
    if (!ok) { error = "bridge.txt debe tener una sola URL corta"; return false; }
    return isaacnx::parse_bridge_endpoint(line, endpoint, error);
}

bool download_bridge_zip(const isaacnx::WorkshopItem& item, std::string& path, std::string& error) {
    std::string endpoint;
    if (!read_bridge_config(endpoint, error)) return false;
    std::string validated_id;
    if (!isaacnx::parse_workshop_id(item.id, validated_id)) {
        error = "ID de mod no valido para descarga local"; return false;
    }
    const std::string url = endpoint + "/steam_" + validated_id + ".zip";
    path = std::string(kInbox) + "/steam_" + validated_id + ".zip";
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) { error = "ZIP ya existe en inbox; no lo sobrescribo"; return false; }
    FILE* file = fdopen(fd, "wb");
    if (!file) { close(fd); unlink(path.c_str()); error = "No pude preparar ZIP local"; return false; }
    CURL* curl = curl_easy_init();
    if (!curl) { fclose(file); unlink(path.c_str()); error = "No pude iniciar curl"; return false; }
    FileWriter writer {file};
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, file_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writer);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "IsaacWorkshopNX/0.3 (private LAN bridge)");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    // Steam's download can be running on the Android companion before its
    // HTTP response starts. The server itself stops attempts after 600 s.
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 645L);
    curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, long(CURLPROTO_HTTP));
#endif
    const CURLcode status = curl_easy_perform(curl);
    long http = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(curl);
    bool closed = fclose(file) == 0;
    if (status != CURLE_OK || http != 200 || writer.excessive || !closed) {
        unlink(path.c_str());
        error = "Puente Android: HTTP " + std::to_string(http) +
                (writer.excessive ? ", ZIP >32 MiB" : ", comprueba servidor y Wi-Fi");
        return false;
    }
    file = fopen(path.c_str(), "rb");
    unsigned char signature[4] {};
    const bool is_zip = file && fread(signature, 1, 4, file) == 4 &&
                        signature[0] == 'P' && signature[1] == 'K' &&
                        signature[2] == 3 && signature[3] == 4;
    if (file) fclose(file);
    if (!is_zip) { unlink(path.c_str()); error = "Puente no entrego ZIP valido"; return false; }
    return true;
}

std::vector<std::string> inbox_files() {
    std::vector<std::string> files;
    DIR* d = opendir(kInbox);
    if (!d) return files;
    while (dirent* e = readdir(d)) {
        std::string name = e->d_name;
        if (name.size() < 5 || name.substr(name.size() - 4) != ".zip" || name[0] == '.') continue;
        if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) continue;
        struct stat st {};
        const std::string path = std::string(kInbox) + "/" + name;
        if (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) files.push_back(name);
        if (files.size() >= 100) break;
    }
    closedir(d);
    std::sort(files.begin(), files.end());
    return files;
}

// Native CM connectivity diagnostic; no Steam account tokens are sent.
bool fetch_steam_cm_servers(std::vector<std::string>& endpoints, std::string& error) {
    CURL* curl = curl_easy_init();
    if (!curl) { error = "Steam CM: no pude iniciar HTTPS"; return false; }
    MemoryBuffer response;
    response.limit = 128 * 1024;
    curl_easy_setopt(curl, CURLOPT_URL,
        "https://api.steampowered.com/ISteamDirectory/GetCMListForConnect/v1/");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memory_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    common_curl(curl);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    const CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    if (code != CURLE_OK || status != 200 || response.excessive) {
        error = "Steam CM: directorio HTTP " + std::to_string(status) +
                " curl " + std::to_string(static_cast<int>(code));
        return false;
    }
    return isaacnx::parse_cm_directory(response.data, endpoints, error);
}

bool fetch_steam_content_servers(std::vector<std::string>& origins,
                                 std::string& error) {
    CURL* h = curl_easy_init();
    if (!h) { error = "Steam CDN: no pude iniciar HTTPS"; return false; }
    MemoryBuffer response;
    response.limit = 256 * 1024;
    curl_easy_setopt(h, CURLOPT_URL,
        "https://api.steampowered.com/IContentServerDirectoryService/GetServersForSteamPipe/v1/?cell_id=0&max_servers=16");
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, memory_write);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &response);
    common_curl(h);
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(h, CURLOPT_TIMEOUT, 15L);
    const CURLcode status = curl_easy_perform(h);
    long http = 0;
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(h);
    if (status != CURLE_OK || http != 200 || response.excessive) {
        error = "Steam CDN: directorio HTTP " + std::to_string(http) +
                " curl " + std::to_string(static_cast<int>(status));
        return false;
    }
    return isaacnx::parse_steam_content_directory(response.data, origins, error);
}

void steam_cm_experiment() {
    consoleClear();
    printf("ISAAC WORKSHOP NX 0.9 | Steam CM / MANIFEST\n");
    printf("Solicitando servidores oficiales de Valve...\n");
    consoleUpdate(nullptr);
    std::vector<std::string> urls;
    std::string error;
    if (!fetch_steam_cm_servers(urls, error)) { message = error; return; }
    for (size_t i = 0; i < urls.size() && i < 2; ++i) {
        consoleClear();
        printf("Steam CM: TLS, Hello y login anonimo\n");
        printf("Servidor %zu de %zu\n", i + 1, std::min(urls.size(), size_t(2)));
        printf("%.90s\n\n", urls[i].c_str());
        printf("No envia Steam Guard ni instala mods.\n");
        consoleUpdate(nullptr);
        if (isaacnx::cm_probe_anonymous_login(urls[i], error)) {
            message = "Steam CM: logon anonimo OK. Siguiente: cuenta y CDN.";
            return;
        }
    }
    message = "Steam CM no conectado: " + error;
}

void display(const std::string& target, const std::vector<std::string>& list, size_t selected) {
    consoleClear();
    printf("ISAAC WORKSHOP NX 0.9 EXP | Switch / Repentance\n");
    printf("=========================================\n");
    printf("Steam: buscar o consultar ID; ZIP: local\n\n");
    printf("Ruta activa:\n%.78s\n\n", target.empty() ? "NO ENCONTRADA" : target.c_str());
    printf("[X] Workshop ID o enlace Steam\n");
    printf("[R] Buscar mods por nombre\n");
    printf("[L] En ficha: descarga via Android\n");
    printf("[-] Iniciar sesion Steam (QR)\n");
    printf("[A] Instalar ZIP seleccionado\n");
    printf("[Y] Probar login Steam CM  [+] Salir\n");
    printf("[Arriba/Abajo] Seleccionar ZIP\n\n");
    printf("INBOX: sdmc:/switch/isaac_workshop_nx/inbox\n");
    if (list.empty()) printf("(No hay ZIPs en inbox)\n");
    for (size_t i = 0; i < list.size() && i < 7; ++i) {
        size_t index = i + (selected >= 6 ? selected - 6 : 0);
        if (index >= list.size()) break;
        printf("%c %.66s\n", index == selected ? '>' : ' ', list[index].c_str());
    }
    printf("\nEstado: %.100s\n", isaacnx::safe_console_text(message, 140).c_str());
    printf("\nNo guarda contrasenas Steam. No toca Atmosphere.\n");
    consoleUpdate(nullptr);
}

void display_search(const std::string& query,
                    const std::vector<isaacnx::WorkshopItem>& results, size_t selected) {
    consoleClear();
    printf("ISAAC WORKSHOP NX 0.9 EXP | Busqueda Steam\n");
    printf("=========================================\n");
    printf("Buscando: %.62s\n", isaacnx::safe_console_text(query, 62).c_str());
    printf("[Arriba/Abajo] Elegir  [A] Detalles\n");
    printf("[R] Otra busqueda  [B] Volver  [+] Salir\n\n");
    const size_t base = selected / 7 * 7;
    for (size_t i = base; i < results.size() && i < base + 7; ++i) {
        const auto title = isaacnx::safe_console_text(results[i].title, 66);
        printf("%c %2zu. %.66s\n", i == selected ? '>' : ' ', i + 1, title.c_str());
    }
    if (!results.empty()) {
        const auto& item = results[selected];
        const auto description = isaacnx::safe_console_text(item.description, 180);
        printf("\nID: %s (%zu/%zu)\n", item.id.c_str(), selected + 1, results.size());
        printf("Descripcion: %.180s\n", description.empty() ? "(Sin descripcion)" : description.c_str());
        printf("Imagen Steam: %s\n", item.preview_url.empty() ? "no disponible" : "disponible");
        printf("Descarga directa: %s\n", item.download_url.empty() ? "no" : "si");
    }
    printf("\nEstado: %.100s\n", isaacnx::safe_console_text(message, 120).c_str());
    consoleUpdate(nullptr);
}

bool wait_confirm(PadState* pad, const std::string& title) {
    consoleClear();
    printf("%.85s\n\n[A] Descargar e instalar   [B] Cancelar\n",
           isaacnx::safe_console_text(title, 85).c_str());
    consoleUpdate(nullptr);
    while (appletMainLoop()) {
        padUpdate(pad);
        const u64 keys = padGetButtonsDown(pad);
        if (keys & HidNpadButton_A) return true;
        if (keys & (HidNpadButton_B | HidNpadButton_Plus)) return false;
        svcSleepThread(16'000'000);
    }
    return false;
}

void install_workshop_item(PadState* pad, const std::string& target,
                           const isaacnx::WorkshopItem& item) {
    if (target.empty()) { message = "Sin destino unico: falta mods o hay 2 rutas"; return; }
    if (item.download_url.empty()) {
        message = isaacnx::safe_console_text(item.title, 55) +
                  (item.content_manifest_id ? ": SteamPipe detectado; descarga nativa pendiente." :
                                              ": sin URL publica. Usa ZIP en inbox.");
        return;
    }
    if (!wait_confirm(pad, "Mod: " + item.title)) { message = "Cancelado"; return; }
    consoleClear();
    printf("Descargando %.70s ...\n", isaacnx::safe_console_text(item.title, 70).c_str());
    consoleUpdate(nullptr);
    std::string archive, error;
    if (!download_public_zip(item, archive, error)) { message = error; return; }
    if (!isaacnx::install_zip(archive, target, "steam_" + item.id, error)) {
        message = error + ". ZIP conservado en inbox.";
        return;
    }
    message = "Instalado: " + isaacnx::safe_console_text(item.title, 64) + ". Reinicia Isaac.";
}

void install_from_bridge(PadState* pad, const std::string& target,
                         const isaacnx::WorkshopItem& item) {
    if (target.empty()) { message = "Sin ruta unica de mods. Revisa carpetas."; return; }
    std::string endpoint, error;
    if (!read_bridge_config(endpoint, error)) { message = error; return; }
    if (!wait_confirm(pad, "Android Wi-Fi: " + item.title)) { message = "Cancelado"; return; }
    consoleClear();
    printf("Recibiendo ZIP de Android por Wi-Fi...\n");
    printf("Steam y la contrasena solo se usan en el telefono.\n");
    consoleUpdate(nullptr);
    std::string archive;
    if (!download_bridge_zip(item, archive, error)) { message = error; return; }
    if (!isaacnx::install_zip(archive, target, "steam_" + item.id, error)) {
        message = error + ". ZIP conservado en inbox.";
        return;
    }
    message = "Instalado desde Android: " + isaacnx::safe_console_text(item.title, 55);
}

void show_workshop_details(PadState* pad, const std::string& target,
                           const isaacnx::WorkshopItem& item) {
    consoleClear();
    const std::string title = isaacnx::safe_console_text(item.title, 130);
    const std::string description = isaacnx::safe_console_text(item.description, 380);
    printf("STEAM WORKSHOP | Isaac Repentance\n");
    printf("=======================================\n");
    printf("%.130s\nID: %s\n\n", title.c_str(), item.id.c_str());
    printf("%.380s\n\n", description.empty() ? "Sin descripcion" : description.c_str());
    printf("Preview en Steam: %s\n", item.preview_url.empty() ? "no" : "si");
    printf("Descarga HTTPS publica: %s\n", item.download_url.empty() ? "no" : "si");
    printf("Manifest SteamPipe: %s\n\n", item.content_manifest_id ? "detectado (aun sin descarga)" : "no");
    printf("[A] %s\n", item.download_url.empty() ? "Ver estado" : "Descargar ZIP publico");
    printf("[L] Bajar desde Android  [-] Validar manifest Steam\n");
    printf("[B] Volver\n");
    consoleUpdate(nullptr);
    while (appletMainLoop()) {
        padUpdate(pad);
        const u64 keys = padGetButtonsDown(pad);
        if (keys & (HidNpadButton_B | HidNpadButton_Plus)) return;
        if (keys & HidNpadButton_A) {
            install_workshop_item(pad, target, item);
            return;
        }
        if (keys & HidNpadButton_L) {
            install_from_bridge(pad, target, item);
            return;
        }
        if ((keys & HidNpadButton_Minus) && item.content_manifest_id) {
            steam_qr_experiment(pad, item.content_manifest_id);
            return;
        }
        svcSleepThread(16'000'000);
    }
}

void search_by_name(std::string& query,
                    std::vector<isaacnx::WorkshopItem>& results, size_t& selected,
                    bool& showing_search) {
    SwkbdConfig keyboard;
    Result rc = swkbdCreate(&keyboard, 0);
    if (R_FAILED(rc)) { message = "No abre teclado; revisa dns.mitm"; return; }
    swkbdConfigMakePresetDefault(&keyboard);
    swkbdConfigSetHeaderText(&keyboard, "Buscar mods de Isaac por nombre");
    char typed[128] {};
    rc = swkbdShow(&keyboard, typed, sizeof(typed));
    swkbdClose(&keyboard);
    if (R_FAILED(rc) || !typed[0]) { message = "Busqueda cancelada"; return; }
    const std::string pending_query = typed;
    if (pending_query.size() > 64) { message = "Busqueda demasiado larga (max 64 bytes)"; return; }
    consoleClear();
    printf("Buscando %.64s en Steam Workshop ...\n", isaacnx::safe_console_text(pending_query, 64).c_str());
    consoleUpdate(nullptr);
    std::vector<isaacnx::WorkshopItem> found;
    std::string error;
    if (!search_workshop(pending_query, found, error)) { message = error; return; }
    query = pending_query;
    results = std::move(found);
    selected = 0;
    showing_search = true;
    message = "Resultados verificados para The Binding of Isaac";
}

void lookup_and_install(PadState* pad, const std::string& target) {
    if (target.empty()) { message = "Sin destino unico: falta mods o hay 2 rutas"; return; }
    SwkbdConfig keyboard;
    Result rc = swkbdCreate(&keyboard, 0);
    if (R_FAILED(rc)) { message = "No abre teclado; revisa dns.mitm"; return; }
    swkbdConfigMakePresetDefault(&keyboard);
    swkbdConfigSetHeaderText(&keyboard, "Steam Workshop: ID o enlace");
    char typed[256] {};
    rc = swkbdShow(&keyboard, typed, sizeof(typed));
    swkbdClose(&keyboard);
    if (R_FAILED(rc) || !typed[0]) { message = "Entrada cancelada"; return; }
    std::string id;
    if (!isaacnx::parse_workshop_id(typed, id)) { message = "ID o enlace Steam no valido"; return; }
    isaacnx::WorkshopItem item;
    std::string error;
    consoleClear(); printf("Consultando Steam: %s ...\n", id.c_str()); consoleUpdate(nullptr);
    if (!read_workshop(id, item, error)) { message = error; return; }
    show_workshop_details(pad, target, item);
}

void install_selected(const std::string& target, const std::string& name) {
    if (target.empty()) { message = "Sin destino unico: falta mods o hay 2 rutas"; return; }
    std::string error;
    if (!isaacnx::install_zip(std::string(kInbox) + "/" + name, target,
                              name.substr(0, name.size() - 4), error)) {
        message = error;
    } else {
        message = "Instalado " + name + "; cierra y reabre Isaac";
    }
}
}

int main() {
    consoleInit(nullptr);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    if (mkdir(kAppRoot, 0700) != 0 && !isaacnx::is_directory(kAppRoot))
        message = "Error: falta carpeta de la app en SD";
    if (mkdir(kInbox, 0700) != 0 && !isaacnx::is_directory(kInbox))
        message = "Error: no pude crear inbox";
    const std::string target = isaacnx::select_installed_mod_root("sdmc:");
    if (target.empty()) message = "Sin ruta unica de mods. Revisa ambas carpetas.";
    Result sockets = socketInitializeDefault();
    bool networking = R_SUCCEEDED(sockets) && curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
    if (!networking) message = "Red no disponible; importacion ZIP sigue operativa";
    size_t selected = 0;
    size_t selected_result = 0;
    bool showing_search = false;
    std::string query;
    std::vector<isaacnx::WorkshopItem> results;
    while (appletMainLoop()) {
        auto list = inbox_files();
        if (selected >= list.size()) selected = list.empty() ? 0 : list.size() - 1;
        if (showing_search) display_search(query, results, selected_result);
        else display(target, list, selected);
        padUpdate(&pad);
        u64 keys = padGetButtonsDown(&pad);
        if (keys & HidNpadButton_Plus) break;
        if (showing_search) {
            if ((keys & HidNpadButton_Up) && selected_result) --selected_result;
            if ((keys & HidNpadButton_Down) && selected_result + 1 < results.size()) ++selected_result;
            if (keys & HidNpadButton_B) showing_search = false;
            if ((keys & HidNpadButton_A) && !results.empty())
                show_workshop_details(&pad, target, results[selected_result]);
            if ((keys & HidNpadButton_R) && networking)
                search_by_name(query, results, selected_result, showing_search);
            if ((keys & HidNpadButton_Minus) && networking)
                steam_qr_experiment(&pad);
            svcSleepThread(33'000'000);
            continue;
        }
        if ((keys & HidNpadButton_Y) && networking) steam_cm_experiment();
        if ((keys & HidNpadButton_Y) && !networking) message = "Sin red para Steam CM";
        if ((keys & HidNpadButton_Up) && selected) --selected;
        if ((keys & HidNpadButton_Down) && selected + 1 < list.size()) ++selected;
        if ((keys & HidNpadButton_X) && networking) lookup_and_install(&pad, target);
        if ((keys & HidNpadButton_Minus) && networking) steam_qr_experiment(&pad);
        if ((keys & HidNpadButton_Minus) && !networking) message = "No se pudo iniciar la red";
        if ((keys & HidNpadButton_X) && !networking) message = "No se pudo iniciar la red";
        if ((keys & HidNpadButton_R) && networking)
            search_by_name(query, results, selected_result, showing_search);
        if ((keys & HidNpadButton_R) && !networking) message = "No se pudo iniciar la red";
        if ((keys & HidNpadButton_A) && !list.empty()) install_selected(target, list[selected]);
        svcSleepThread(33'000'000);
    }
    if (networking) curl_global_cleanup();
    if (R_SUCCEEDED(sockets)) socketExit();
    consoleExit(nullptr);
    return 0;
}

