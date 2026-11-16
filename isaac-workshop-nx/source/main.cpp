#include "isaacnx/core.hpp"

#include <switch.h>
#include <curl/curl.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {
constexpr const char* kAppRoot = "sdmc:/switch/isaac_workshop_nx";
constexpr const char* kInbox = "sdmc:/switch/isaac_workshop_nx/inbox";
constexpr const char* kBridgeConfig = "sdmc:/switch/isaac_workshop_nx/bridge.txt";
constexpr size_t kMaxDownload = 32 * 1024 * 1024;
constexpr size_t kMaxMetadata = 1024 * 1024;
constexpr size_t kMaxBrowse = 3 * 1024 * 1024;
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

void display(const std::string& target, const std::vector<std::string>& list, size_t selected) {
    consoleClear();
    printf("ISAAC WORKSHOP NX 0.3 | Switch / Repentance\n");
    printf("=========================================\n");
    printf("Steam: buscar o consultar ID; ZIP: local\n\n");
    printf("Ruta activa:\n%.78s\n\n", target.empty() ? "NO ENCONTRADA" : target.c_str());
    printf("[X] Workshop ID o enlace Steam\n");
    printf("[R] Buscar mods por nombre\n");
    printf("[L] En ficha: descarga via Android\n");
    printf("[A] Instalar ZIP seleccionado\n");
    printf("[Y] Actualizar lista  [+] Salir\n");
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
    printf("ISAAC WORKSHOP NX 0.3 | Busqueda Steam\n");
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
                  ": sin URL publica. Usa ZIP en inbox.";
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
    printf("Descarga HTTPS publica: %s\n\n", item.download_url.empty() ? "no" : "si");
    printf("[A] %s\n", item.download_url.empty() ? "Ver estado" : "Descargar ZIP publico");
    printf("[L] Bajar desde Android  [B] Volver\n");
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
            svcSleepThread(33'000'000);
            continue;
        }
        if ((keys & HidNpadButton_Up) && selected) --selected;
        if ((keys & HidNpadButton_Down) && selected + 1 < list.size()) ++selected;
        if ((keys & HidNpadButton_X) && networking) lookup_and_install(&pad, target);
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
