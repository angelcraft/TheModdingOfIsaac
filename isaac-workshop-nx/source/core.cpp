#include "isaacnx/core.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
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
    return true;
}

} // namespace

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
