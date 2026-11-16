#include "isaacnx/core.hpp"

#include <zlib.h>

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <set>
#include <sys/stat.h>
#include <unistd.h>

namespace isaacnx {
namespace {
constexpr size_t kArchiveLimit = 32 * 1024 * 1024;
constexpr uint64_t kExpandedLimit = 96 * 1024 * 1024;
constexpr uint32_t kSingleFileLimit = 24 * 1024 * 1024;
constexpr size_t kFileCountLimit = 1500;

uint16_t u16(const std::vector<uint8_t>& b, size_t p) {
    return uint16_t(b[p]) | (uint16_t(b[p + 1]) << 8);
}
uint32_t u32(const std::vector<uint8_t>& b, size_t p) {
    return uint32_t(u16(b, p)) | (uint32_t(u16(b, p + 2)) << 16);
}
bool range(const std::vector<uint8_t>& b, uint64_t offset, uint64_t count) {
    return offset <= b.size() && count <= b.size() - offset;
}
std::string lowercase(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
bool safe_entry(const std::string& s) {
    if (s.empty() || s.size() > 220 || s[0] == '/' || s[0] == '\\') return false;
    size_t start = 0;
    int depth = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i < s.size() && (s[i] == '\\' || static_cast<unsigned char>(s[i]) < 32 || s[i] == ':')) return false;
        if (i != s.size() && s[i] != '/') continue;
        std::string component = s.substr(start, i - start);
        if (component == "." || component == ".." || (component.empty() && i != s.size())) return false;
        if (!component.empty() && ++depth > 14) return false;
        start = i + 1;
    }
    return true;
}

bool read_bytes(const std::string& path, std::vector<uint8_t>& data, std::string& error) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { error = "No puedo abrir ZIP"; return false; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); error = "No puedo medir ZIP"; return false; }
    long length = ftell(f);
    if (length <= 0 || static_cast<size_t>(length) > kArchiveLimit) {
        fclose(f); error = "ZIP vacio o supera 32 MiB"; return false;
    }
    rewind(f);
    data.resize(static_cast<size_t>(length));
    bool ok = fread(data.data(), 1, data.size(), f) == data.size();
    fclose(f);
    if (!ok) error = "Lectura ZIP incompleta";
    return ok;
}

bool make_parents(const std::string& staging, const std::string& relative) {
    size_t p = 0;
    while ((p = relative.find('/', p)) != std::string::npos) {
        const std::string path = staging + "/" + relative.substr(0, p);
        if (mkdir(path.c_str(), 0700) != 0 && errno != EEXIST) return false;
        if (!is_directory(path)) return false;
        ++p;
    }
    return true;
}

// Only called for a staging directory that this invocation successfully created.
// lstat prevents following symlinks during rollback.
void discard_staging(const std::string& path) {
    DIR* d = opendir(path.c_str());
    if (!d) return;
    while (dirent* item = readdir(d)) {
        if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, "..")) continue;
        std::string target = path + "/" + item->d_name;
        struct stat s {};
        if (lstat(target.c_str(), &s) != 0) continue;
        if (S_ISDIR(s.st_mode)) discard_staging(target);
        else unlink(target.c_str());
    }
    closedir(d);
    rmdir(path.c_str());
}

bool unpack_one(const std::vector<uint8_t>& b, const ZipEntry& entry,
                const std::string& staging, const std::string& relative, std::string& error) {
    const uint64_t h = entry.local_header_offset;
    if (!range(b, h, 30) || u32(b, h) != 0x04034b50) { error = "Cabecera ZIP inconsistente"; return false; }
    const uint16_t flags = u16(b, h + 6);
    if ((flags & 1) || (flags & ~(uint16_t(8) | uint16_t(2048)))) {
        error = "ZIP cifrado o con flags no admitidos"; return false;
    }
    if (u16(b, h + 8) != entry.compression) { error = "Metodo ZIP inconsistente"; return false; }
    const size_t name_len = u16(b, h + 26);
    const size_t extra_len = u16(b, h + 28);
    const uint64_t data_pos = h + 30 + name_len + extra_len;
    if (!range(b, h + 30, name_len + extra_len) ||
        std::string(reinterpret_cast<const char*>(b.data() + h + 30), name_len) != entry.name ||
        !range(b, data_pos, entry.compressed_size)) {
        error = "Offsets ZIP incorrectos"; return false;
    }
    if (!make_parents(staging, relative)) { error = "No pude crear directorios"; return false; }
    if (entry.directory) {
        std::string folder = staging + "/" + relative;
        if (mkdir(folder.c_str(), 0700) && errno != EEXIST) { error = "No pude crear carpeta"; return false; }
        return true;
    }
    std::vector<uint8_t> out(entry.expanded_size ? entry.expanded_size : 1);
    if (entry.compression == 0) {
        if (entry.compressed_size != entry.expanded_size) { error = "Tamano ZIP stored no coincide"; return false; }
        std::copy_n(b.data() + data_pos, entry.expanded_size, out.data());
    } else {
        z_stream z {};
        if (inflateInit2(&z, -MAX_WBITS) != Z_OK) { error = "No puedo iniciar zlib"; return false; }
        z.next_in = const_cast<Bytef*>(b.data() + data_pos);
        z.avail_in = entry.compressed_size;
        z.next_out = out.data();
        z.avail_out = static_cast<uInt>(out.size());
        int status = inflate(&z, Z_FINISH);
        bool ok = status == Z_STREAM_END && z.total_out == entry.expanded_size && z.total_in == entry.compressed_size;
        inflateEnd(&z);
        if (!ok) { error = "Deflate invalido o longitud incorrecta"; return false; }
    }
    if (crc32(0, out.data(), entry.expanded_size) != entry.crc) {
        error = "CRC32 ZIP incorrecto"; return false;
    }
    const std::string target = staging + "/" + relative;
    FILE* f = fopen(target.c_str(), "wb");
    if (!f) { error = "No pude escribir un archivo"; return false; }
    bool wrote = fwrite(out.data(), 1, entry.expanded_size, f) == entry.expanded_size;
    if (fclose(f) != 0 || !wrote) { error = "Escritura SD incompleta"; return false; }
    return true;
}

} // namespace

bool inspect_zip(const std::vector<uint8_t>& b, std::vector<ZipEntry>& entries,
                 std::string& prefix, std::string& error) {
    entries.clear(); prefix.clear();
    if (b.size() < 22 || b.size() > kArchiveLimit) { error = "ZIP demasiado grande o pequeno"; return false; }
    size_t eocd = b.size();
    const size_t first = b.size() > 65557 ? b.size() - 65557 : 0;
    for (size_t i = b.size() - 22;; --i) {
        if (u32(b, i) == 0x06054b50 && range(b, i, 22 + u16(b, i + 20)) &&
            i + 22 + u16(b, i + 20) == b.size()) { eocd = i; break; }
        if (i == first) break;
    }
    if (eocd == b.size() || u16(b, eocd + 4) || u16(b, eocd + 6) ||
        u16(b, eocd + 8) != u16(b, eocd + 10)) { error = "ZIP multipart/invalid"; return false; }
    const uint16_t total = u16(b, eocd + 10);
    const uint32_t cd_size = u32(b, eocd + 12);
    const uint32_t cd_offset = u32(b, eocd + 16);
    if (!total || total > kFileCountLimit || !range(b, cd_offset, cd_size) ||
        uint64_t(cd_offset) + cd_size > eocd) { error = "Indice ZIP invalido"; return false; }
    uint64_t total_size = 0;
    size_t p = cd_offset;
    std::set<std::string> folded;
    for (size_t i = 0; i < total; ++i) {
        if (p + 46 > uint64_t(cd_offset) + cd_size || u32(b, p) != 0x02014b50) {
            error = "Entrada central ZIP corrupta"; return false;
        }
        const size_t name_len = u16(b, p + 28);
        const size_t extra_len = u16(b, p + 30);
        const size_t comment_len = u16(b, p + 32);
        const uint64_t next = uint64_t(p) + 46 + name_len + extra_len + comment_len;
        if (next > uint64_t(cd_offset) + cd_size) { error = "Indice ZIP truncado"; return false; }
        std::string name(reinterpret_cast<const char*>(b.data() + p + 46), name_len);
        if (!safe_entry(name)) { error = "Ruta ZIP insegura"; return false; }
        const bool directory = name.back() == '/';
        const uint16_t flags = u16(b, p + 8);
        const uint16_t method = u16(b, p + 10);
        const uint32_t comp = u32(b, p + 20);
        const uint32_t expanded = u32(b, p + 24);
        const uint32_t local = u32(b, p + 42);
        const uint32_t unix_mode = u32(b, p + 38) >> 16;
        if ((flags & 1) || (flags & ~(uint16_t(8) | uint16_t(2048))) ||
            (method != 0 && method != 8) || expanded > kSingleFileLimit ||
            (unix_mode & 0170000) == 0120000 ||
            (directory && expanded != 0) ||
            local == 0xffffffffu || comp == 0xffffffffu || expanded == 0xffffffffu) {
            error = "Tipo de archivo ZIP no admitido"; return false;
        }
        total_size += expanded;
        if (total_size > kExpandedLimit) { error = "ZIP expandido supera 96 MiB"; return false; }
        if (!folded.insert(lowercase(name)).second) { error = "Rutas ZIP repetidas"; return false; }
        entries.push_back({name, local, comp, expanded, u32(b, p + 16), method, directory});
        p = size_t(next);
    }
    if (p != uint64_t(cd_offset) + cd_size) { error = "Indice ZIP sobrante"; return false; }
    std::set<std::string> names;
    for (const auto& e : entries) if (!e.directory) names.insert(lowercase(e.name));
    if (names.count("main.lua") && names.count("metadata.xml")) return true;
    std::string candidate;
    for (const auto& e : entries) {
        auto slash = e.name.find('/');
        if (slash == std::string::npos) { error = "ZIP sin raiz de mod unica"; return false; }
        const std::string root = e.name.substr(0, slash + 1);
        if (candidate.empty()) candidate = root;
        if (candidate != root) { error = "ZIP con varios mods o carpetas raiz"; return false; }
    }
    if (!names.count(lowercase(candidate + "main.lua")) ||
        !names.count(lowercase(candidate + "metadata.xml"))) {
        error = "No encontre main.lua y metadata.xml en la raiz"; return false;
    }
    prefix = candidate;
    return true;
}

bool install_zip(const std::string& archive_path, const std::string& mod_root,
                 const std::string& install_name, std::string& error) {
    if (!is_directory(mod_root)) { error = "No encuentro la carpeta de mods del runtime"; return false; }
    const std::string slug = safe_slug(install_name);
    const std::string final_path = mod_root + "/" + slug;
    struct stat statbuf {};
    if (lstat(final_path.c_str(), &statbuf) == 0) { error = "Ya existe: " + slug + ". No sobrescribo."; return false; }
    std::vector<uint8_t> b;
    if (!read_bytes(archive_path, b, error)) return false;
    std::vector<ZipEntry> entries;
    std::string prefix;
    if (!inspect_zip(b, entries, prefix, error)) return false;
    std::string staging;
    for (int attempt = 0; attempt < 100; ++attempt) {
        staging = mod_root + "/.isaacnx_tmp_" + slug + "_" + std::to_string(attempt);
        if (mkdir(staging.c_str(), 0700) == 0) break;
        if (errno != EEXIST) { error = "SD no permite crear staging"; return false; }
        if (attempt == 99) { error = "Staging saturado"; return false; }
    }
    bool success = true;
    for (const auto& entry : entries) {
        if (entry.name == prefix) continue;
        std::string relative = entry.name.substr(prefix.size());
        if (relative.empty()) continue;
        if (!unpack_one(b, entry, staging, relative, error)) { success = false; break; }
    }
    if (success && lstat(final_path.c_str(), &statbuf) == 0) {
        success = false; error = "El destino aparecio durante instalacion";
    }
    if (success && rename(staging.c_str(), final_path.c_str()) != 0) {
        success = false; error = "No pude activar el mod en la SD";
    }
    if (!success) discard_staging(staging);
    return success;
}

} // namespace isaacnx
