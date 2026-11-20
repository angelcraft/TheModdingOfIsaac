#include "isaacnx/core.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sys/stat.h>

namespace isaacnx {
namespace {

std::string trim(const std::string& s) {
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool digits(const std::string& s) {
    if (s.empty() || s.size() > 20 || s == "0") return false;
    return std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

// Minimal bounded JSON parser for Steam's metadata response. It does not
// expose/execute JSON, and rejects malformed escapes and excessive nesting.
struct Json {
    enum Type { Null, String, Number, Object, Array, Boolean } type = Null;
    std::string text;
    std::map<std::string, Json> members;
    std::vector<Json> entries;
    const Json& at(const std::string& key) const {
        auto p = members.find(key);
        static const Json empty;
        return p == members.end() ? empty : p->second;
    }
};

struct Parser {
    const std::string& s;
    size_t p = 0;
    explicit Parser(const std::string& input) : s(input) {}
    void ws() { while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) ++p; }
    bool take(char c) { ws(); if (p < s.size() && s[p] == c) { ++p; return true; } return false; }
    bool hex4(uint32_t& codepoint) {
        if (p + 4 > s.size()) return false;
        codepoint = 0;
        for (int i = 0; i < 4; ++i) {
            unsigned char h = s[p++];
            if (!std::isxdigit(h)) return false;
            codepoint = codepoint * 16 + (h <= '9' ? h - '0' : (h <= 'F' ? h - 'A' + 10 : h - 'a' + 10));
        }
        return true;
    }
    bool quote(std::string& out) {
        if (!take('"')) return false;
        while (p < s.size()) {
            unsigned char c = s[p++];
            if (c == '"') return true;
            if (c < 0x20) return false;
            if (c != '\\') { out.push_back(static_cast<char>(c)); continue; }
            if (p == s.size()) return false;
            char esc = s[p++];
            switch (esc) {
                case '"': case '\\': case '/': out += esc; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    uint32_t cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xd800 && cp <= 0xdbff) {
                        if (p + 6 > s.size() || s[p++] != '\\' || s[p++] != 'u') return false;
                        uint32_t low = 0;
                        if (!hex4(low) || low < 0xdc00 || low > 0xdfff) return false;
                        cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
                    }
                    if (cp >= 0xdc00 && cp <= 0xdfff) return false;
                    if (cp < 0x80) out += char(cp);
                    else if (cp < 0x800) { out += char(0xc0 | (cp >> 6)); out += char(0x80 | (cp & 63)); }
                    else if (cp < 0x10000) { out += char(0xe0 | (cp >> 12)); out += char(0x80 | ((cp >> 6) & 63)); out += char(0x80 | (cp & 63)); }
                    else { out += char(0xf0 | (cp >> 18)); out += char(0x80 | ((cp >> 12) & 63)); out += char(0x80 | ((cp >> 6) & 63)); out += char(0x80 | (cp & 63)); }
                    break;
                }
                default: return false;
            }
        }
        return false;
    }
    bool parse(Json& j, int depth = 0) {
        if (depth > 24) return false;
        ws(); if (p == s.size()) return false;
        if (s[p] == '"') { j.type = Json::String; return quote(j.text); }
        if (take('{')) {
            j.type = Json::Object;
            if (take('}')) return true;
            do {
                std::string key;
                if (!quote(key) || !take(':')) return false;
                Json child;
                if (!parse(child, depth + 1)) return false;
                if (!j.members.emplace(std::move(key), std::move(child)).second) return false;
                if (take('}')) return true;
            } while (take(','));
            return false;
        }
        if (take('[')) {
            j.type = Json::Array;
            if (take(']')) return true;
            do {
                Json child;
                if (!parse(child, depth + 1) || j.entries.size() >= 1000) return false;
                j.entries.push_back(std::move(child));
                if (take(']')) return true;
            } while (take(','));
            return false;
        }
        const size_t start = p;
        while (p < s.size() && (std::isalnum(static_cast<unsigned char>(s[p])) || s[p] == '-' || s[p] == '.' || s[p] == '+' )) ++p;
        j.text = s.substr(start, p - start);
        if (j.text == "true" || j.text == "false") { j.type = Json::Boolean; return true; }
        if (j.text == "null") { j.type = Json::Null; return true; }
        if (j.text.empty() || j.text.find_first_not_of("-+0123456789.eE") != std::string::npos) return false;
        j.type = Json::Number; return true;
    }
};

uint64_t decimal(const std::string& s) {
    if (s.empty() || s.size() > 20 || s.find_first_not_of("0123456789") != std::string::npos) return 0;
    uint64_t value = 0;
    for (char c : s) {
        uint64_t n = static_cast<unsigned>(c - '0');
        if (value > (UINT64_MAX - n) / 10) return 0;
        value = value * 10 + n;
    }
    return value;
}

bool parse_metadata_document(const std::string& json, Json& root, std::string& error) {
    if (json.size() > 1024 * 1024) { error = "Respuesta demasiado grande"; return false; }
    Parser parser(json);
    if (!parser.parse(root)) { error = "JSON de Steam no valido"; return false; }
    parser.ws();
    if (parser.p != json.size()) { error = "Datos extra despues del JSON"; return false; }
    return true;
}

bool valid_item(const Json& entry, const std::string& requested_id, WorkshopItem& item) {
    if (entry.type != Json::Object || entry.at("result").text != "1" ||
        entry.at("consumer_app_id").text != "250900" ||
        entry.at("publishedfileid").text != requested_id ||
        entry.at("title").type != Json::String || entry.at("title").text.empty()) return false;
    item = {requested_id, entry.at("title").text, entry.at("file_url").text, 250900,
            decimal(entry.at("file_size").text), entry.at("preview_url").text,
            entry.at("file_description").text};
    // Resolve the native SteamPipe content ID. Downloading it still requires
    // Steam CM authentication, a depot key, a manifest code, and CDN chunks.
    item.content_manifest_id = decimal(entry.at("hcontent_file").text);
    const uint64_t file_type = decimal(entry.at("file_type").text);
    item.workshop_file_type = file_type <= UINT32_MAX ? static_cast<uint32_t>(file_type) : UINT32_MAX;
    return true;
}

} // namespace

WorkshopDelivery workshop_delivery(const WorkshopItem& item) {
    // Workshop collections (file type 2) require child resolution first.
    if (item.workshop_file_type == 2) return WorkshopDelivery::Unavailable;
    if (item.download_url.compare(0, 8, "https://") == 0)
        return WorkshopDelivery::DirectHttps;
    if (item.content_manifest_id != 0)
        return WorkshopDelivery::SteamPipeManifest;
    return WorkshopDelivery::Unavailable;
}

bool parse_cm_directory(const std::string& json, std::vector<std::string>& urls,
                        std::string& error) {
    urls.clear();
    Json root;
    if (!parse_metadata_document(json, root, error)) return false;
    const Json& response = root.at("response");
    const Json& servers = response.at("serverlist");
    const std::string success = response.at("success").text;
    if ((success != "true" && success != "1") || servers.type != Json::Array) {
        error = "Steam CM: directorio no disponible";
        return false;
    }
    std::set<std::string> seen;
    for (const auto& server : servers.entries) {
        if (server.at("type").type != Json::String ||
            server.at("type").text != "websockets" ||
            server.at("endpoint").type != Json::String) continue;
        const std::string& endpoint = server.at("endpoint").text;
        if (endpoint.empty() || endpoint.size() > 127) continue;
        const size_t colon = endpoint.rfind(':');
        if (colon == std::string::npos || colon < 1 || colon == endpoint.size() - 1) continue;
        const std::string host = endpoint.substr(0, colon);
        const std::string port = endpoint.substr(colon + 1);
        const uint64_t parsed_port = decimal(port);
        if (!parsed_port || parsed_port > 65535 || port.size() > 5) continue;
        constexpr const char* domains[] = {".steamserver.net", ".steampowered.com"};
        bool trusted = false;
        for (const char* suffix : domains) {
            const std::string domain = suffix;
            if (host.size() > domain.size() &&
                host.compare(host.size() - domain.size(), domain.size(), domain) == 0)
                trusted = true;
        }
        if (!trusted || host.front() == '-' || host.back() == '-' ||
            host.front() == '.' || host.back() == '.' ||
            host.find("..") != std::string::npos ||
            !std::all_of(host.begin(), host.end(), [](char ch) {
                return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
                       ch == '.' || ch == '-';
            })) continue;
        const std::string url = "wss://" + endpoint + "/cmsocket/";
        if (seen.insert(url).second) urls.push_back(url);
        if (urls.size() >= 8) break;
    }
    if (urls.empty()) {
        error = "Steam CM: sin servidores WSS de Valve validos";
        return false;
    }
    error.clear();
    return true;
}

bool parse_workshop_id(const std::string& input, std::string& id) {
    id.clear();
    std::string s = trim(input);
    if (digits(s) && decimal(s)) { id = s; return true; }
    const std::string https = "https://steamcommunity.com/";
    const std::string http = "http://steamcommunity.com/";
    size_t offset = s.compare(0, https.size(), https) == 0 ? https.size()
                  : s.compare(0, http.size(), http) == 0 ? http.size() : 0;
    if (!offset) return false;
    size_t path_end = s.find('?', offset);
    if (path_end == std::string::npos) return false;
    const std::string path = s.substr(offset, path_end - offset);
    if (path != "sharedfiles/filedetails/" && path != "workshop/filedetails/") return false;
    size_t begin = path_end + 1;
    while (begin < s.size()) {
        size_t end = s.find('&', begin);
        if (end == std::string::npos) end = s.size();
        std::string query = s.substr(begin, end - begin);
        if (query.compare(0, 3, "id=") == 0) {
            std::string candidate = query.substr(3);
            if (digits(candidate) && decimal(candidate)) { id = candidate; return true; }
            return false;
        }
        begin = end + 1;
    }
    return false;
}

bool parse_workshop_response(const std::string& json, const std::string& requested_id,
                             WorkshopItem& item, std::string& error) {
    Json root;
    if (!parse_metadata_document(json, root, error)) return false;
    const Json& files = root.at("response").at("publishedfiledetails");
    if (files.type != Json::Array || files.entries.size() != 1) { error = "Steam no devolvio el mod"; return false; }
    const Json& entry = files.entries[0];
    if (entry.at("result").text != "1") { error = "Mod privado/eliminado o no disponible"; return false; }
    if (entry.at("consumer_app_id").text != "250900") { error = "No pertenece a The Binding of Isaac"; return false; }
    if (entry.at("publishedfileid").text != requested_id) { error = "ID inesperado en la respuesta"; return false; }
    if (!valid_item(entry, requested_id, item)) { error = "Metadatos incompletos"; return false; }
    return true;
}

bool parse_steam_qr_begin(const std::string& json, SteamQrChallenge& challenge,
                          std::string& error) {
    if (json.size() > 16384) { error = "Respuesta Steam QR excesiva"; return false; }
    Json root;
    if (!parse_metadata_document(json, root, error)) return false;
    const Json& response = root.at("response");
    const std::string id = response.at("client_id").text;
    const std::string request = response.at("request_id").text;
    const std::string url = response.at("challenge_url").text;
    if ((response.at("client_id").type != Json::Number &&
         response.at("client_id").type != Json::String) || !decimal(id) ||
        response.at("request_id").type != Json::String ||
        request.size() < 4 || request.size() > 512 ||
        request.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=_-") != std::string::npos ||
        response.at("challenge_url").type != Json::String ||
        url.compare(0, 17, "https://s.team/q/") != 0 ||
        url.size() > 200 || url.find_first_of(" \r\n\t#") != std::string::npos) {
        error = "Steam no devolvio una sesion QR valida";
        return false;
    }
    unsigned interval = 5;
    const Json& interval_field = response.at("interval");
    if (interval_field.type == Json::Number) {
        char* end = nullptr;
        const double parsed = std::strtod(interval_field.text.c_str(), &end);
        if (end && *end == '\0' && parsed >= 2 && parsed <= 30) interval = static_cast<unsigned>(parsed + 0.99);
    }
    challenge = {id, request, url, interval};
    return true;
}

bool parse_steam_qr_poll(const std::string& json, SteamQrPoll& poll,
                         std::string& error) {
    if (json.size() > 16384) { error = "Respuesta Steam Guard excesiva"; return false; }
    Json root;
    if (!parse_metadata_document(json, root, error)) return false;
    const Json& response = root.at("response");
    if (response.type != Json::Object) { error = "Steam Guard devolvio una respuesta invalida"; return false; }
    SteamQrPoll candidate;
    const Json& access = response.at("access_token");
    const Json& refresh = response.at("refresh_token");
    if ((access.type != Json::Null && access.type != Json::String) ||
        (refresh.type != Json::Null && refresh.type != Json::String)) {
        error = "Tokens Steam con formato invalido"; return false;
    }
    candidate.confirmed = access.type == Json::String && !access.text.empty() &&
                          refresh.type == Json::String && !refresh.text.empty();
    if (candidate.confirmed) {
        const bool safe = access.text.size() <= 4096 &&
            std::all_of(access.text.begin(), access.text.end(), [](char ch) {
                return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                       (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.';
            });
        if (!safe) { error = "Steam Guard: token QR invalido"; return false; }
        candidate.access_token = access.text;
    }
    candidate.interaction = response.at("had_remote_interaction").text == "true";
    if (response.at("account_name").type == Json::String)
        candidate.account_name = safe_console_text(response.at("account_name").text, 45);
    const std::string id = response.at("new_client_id").text;
    const std::string url = response.at("new_challenge_url").text;
    if (!id.empty()) {
        if (!decimal(id)) { error = "Nuevo ID QR invalido"; return false; }
        candidate.new_client_id = id;
    }
    if (!url.empty()) {
        if (url.compare(0, 17, "https://s.team/q/") != 0 || url.size() > 200 ||
            url.find_first_of(" \r\n\t#") != std::string::npos) {
            error = "Nuevo enlace Steam Guard invalido"; return false;
        }
        candidate.new_challenge_url = url;
    }
    poll = std::move(candidate);
    return true;
}

bool parse_workshop_listing(const std::string& html, std::vector<std::string>& ids,
                            std::string& error) {
    ids.clear();
    if (html.size() > 3 * 1024 * 1024) { error = "Pagina de busqueda demasiado grande"; return false; }
    const std::string marker = "filedetails/?id=";
    std::set<std::string> seen;
    size_t pos = 0;
    while ((pos = html.find(marker, pos)) != std::string::npos) {
        pos += marker.size();
        const size_t start = pos;
        while (pos < html.size() && html[pos] >= '0' && html[pos] <= '9') ++pos;
        const std::string id = html.substr(start, pos - start);
        // IDs must fit uint64_t. The following byte must delimit the ID,
        // not be an alphabetic suffix crafted to spoof a valid link.
        if (!id.empty() && digits(id) && decimal(id) &&
            (pos == html.size() || html[pos] == '&' || html[pos] == '"' ||
             html[pos] == '\'' || html[pos] == '<' || html[pos] == '/' ||
             html[pos] == '?' || std::isspace(static_cast<unsigned char>(html[pos])))) {
            if (seen.insert(id).second) ids.push_back(id);
            if (ids.size() >= 20) break;
        }
        if (pos == start) ++pos;
    }
    if (ids.empty()) { error = "Sin resultados; Steam puede limitar la busqueda"; return false; }
    return true;
}

bool parse_workshop_batch_response(const std::string& json,
                                   const std::vector<std::string>& requested_ids,
                                   std::vector<WorkshopItem>& items, std::string& error) {
    items.clear();
    if (requested_ids.empty() || requested_ids.size() > 20) {
        error = "Cantidad de resultados no valida"; return false;
    }
    std::set<std::string> unique;
    for (const auto& id : requested_ids) {
        if (!digits(id) || !decimal(id) || !unique.insert(id).second) {
            error = "IDs de busqueda no validos"; return false;
        }
    }
    Json root;
    if (!parse_metadata_document(json, root, error)) return false;
    const Json& files = root.at("response").at("publishedfiledetails");
    if (files.type != Json::Array || files.entries.size() != requested_ids.size()) {
        error = "Steam devolvio una lista inesperada"; return false;
    }
    std::map<std::string, WorkshopItem> verified;
    for (const auto& entry : files.entries) {
        const std::string id = entry.at("publishedfileid").text;
        if (!unique.count(id)) { error = "Steam devolvio un ID inesperado"; return false; }
        WorkshopItem item;
        if (valid_item(entry, id, item)) verified.emplace(id, std::move(item));
    }
    for (const auto& id : requested_ids) {
        auto it = verified.find(id);
        if (it != verified.end()) items.push_back(std::move(it->second));
    }
    if (items.empty()) { error = "No hay resultados publicos de Isaac"; return false; }
    return true;
}

bool parse_bridge_endpoint(const std::string& config, std::string& endpoint,
                           std::string& error) {
    endpoint.clear();
    const std::string text = trim(config);
    const std::string scheme = "http://";
    if (text.size() > 120 || text.compare(0, scheme.size(), scheme) != 0) {
        error = "Puente: usa HTTP con IP Wi-Fi privada"; return false;
    }
    const size_t colon = text.find(':', scheme.size());
    if (colon == std::string::npos || text.substr(colon, 6) != ":8765/") {
        error = "Puente: puerto debe ser 8765"; return false;
    }
    const std::string ip = text.substr(scheme.size(), colon - scheme.size());
    std::vector<unsigned> octets;
    size_t start = 0;
    while (start < ip.size()) {
        const size_t dot = ip.find('.', start);
        const size_t end = dot == std::string::npos ? ip.size() : dot;
        const std::string part = ip.substr(start, end - start);
        if (part.empty() || part.size() > 3 ||
            (part.size() > 1 && part[0] == '0') ||
            part.find_first_not_of("0123456789") != std::string::npos) {
            error = "Puente: direccion IPv4 no valida"; return false;
        }
        unsigned octet = 0;
        for (char c : part) octet = octet * 10 + static_cast<unsigned>(c - '0');
        if (octet > 255) { error = "Puente: octeto fuera de rango"; return false; }
        octets.push_back(octet);
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    if (octets.size() != 4 || !(
        octets[0] == 10 || (octets[0] == 172 && octets[1] >= 16 && octets[1] <= 31) ||
        (octets[0] == 192 && octets[1] == 168))) {
        error = "Puente: IP fuera de la red privada permitida"; return false;
    }
    const std::string token = text.substr(colon + 6);
    if (token.size() < 24 || token.size() > 64 ||
        token.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != std::string::npos) {
        error = "Puente: codigo de emparejamiento invalido"; return false;
    }
    endpoint = text;
    return true;
}

std::string safe_console_text(const std::string& text, size_t max_bytes) {
    std::string result;
    for (unsigned char c : text) {
        if (result.size() >= max_bytes) break;
        if (c < 0x20 || c == 0x7f) c = ' ';
        if (c == ' ' && (result.empty() || result.back() == ' ')) continue;
        result += static_cast<char>(c);
    }
    while (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}

std::string safe_slug(const std::string& text) {
    std::string out;
    for (unsigned char c : text) {
        if (out.size() >= 55) break;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_') out += c;
        else if (c >= 'A' && c <= 'Z') out += char(c - 'A' + 'a');
        else if (!out.empty() && out.back() != '_') out += '_';
    }
    while (!out.empty() && (out.back() == '_' || out.back() == '.')) out.pop_back();
    return out.empty() || out == "." || out == ".." ? "mod" : out;
}

bool is_directory(const std::string& path) {
    struct stat s {};
    return stat(path.c_str(), &s) == 0 && S_ISDIR(s.st_mode);
}

std::string select_installed_mod_root(const std::string& sd_root) {
    const std::string sd = sd_root + "/isaac_mods/mods";
    const std::string overlay = sd_root + "/atmosphere/contents/010021C000B6A000/romfs/isaac_mods/mods";
    // The new runtime may choose the SD tree when both exist. Never silently
    // install into either tree if we cannot prove which one the game uses.
    if (is_directory(sd) && is_directory(overlay)) return {};
    // Phylos' current installation uses the LayeredFS overlay path.
    if (is_directory(overlay)) return overlay;
    if (is_directory(sd)) return sd;
    return {};
}

} // namespace isaacnx
