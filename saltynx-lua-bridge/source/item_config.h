#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

constexpr size_t kCollectibleQualityLimit = 2048;
constexpr size_t kCollectibleMetadataMaxBytes = 256 * 1024;
static int8_t g_collectible_quality[kCollectibleQualityLimit];
static bool g_collectible_metadata_loaded = false;
static int g_item_config_table_ref = LUA_NOREF;

bool item_metadata_attribute(const char* begin, const char* end,
                             const char* wanted, int& value) {
    const size_t wanted_len = std::strlen(wanted);
    const char* p = begin;
    while (p < end) {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' ||
                           *p == '\n' || *p == '/' || *p == '?')) ++p;
        const char* name = p;
        while (p < end && ((*p >= 'a' && *p <= 'z') ||
                           (*p >= 'A' && *p <= 'Z') ||
                           (*p >= '0' && *p <= '9') || *p == '_')) ++p;
        if (p == name) { ++p; continue; }
        const size_t name_len = static_cast<size_t>(p - name);
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
        if (p == end || *p != '=') continue;
        ++p;
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
        if (p == end || (*p != '"' && *p != '\'')) return false;
        const char quote = *p++;
        const char* digits = p;
        while (p < end && *p != quote) ++p;
        if (p == end) return false;
        if (name_len == wanted_len && std::memcmp(name, wanted, wanted_len) == 0) {
            if (digits == p) return false;
            int parsed = 0;
            for (const char* q = digits; q < p; ++q) {
                if (*q < '0' || *q > '9') return false;
                parsed = parsed * 10 + (*q - '0');
                if (parsed > 1000000) return false;
            }
            value = parsed;
            return true;
        }
        ++p;
    }
    return false;
}

bool item_metadata_load() {
    std::memset(g_collectible_quality, -1, sizeof(g_collectible_quality));
    g_collectible_metadata_loaded = false;
    static const char path[] = "sdmc:/switch/TheModdingOfIsaac/items_metadata.xml";
    FILE* file = SaltySDCore_fopen(path, "rb");
    if (!file) {
        trace("ItemConfig: items_metadata.xml missing; GetCollectible returns nil");
        return false;
    }
    SaltySDCore_fseek(file, 0, SEEK_END);
    const int64_t length = SaltySDCore_ftell(file);
    SaltySDCore_fseek(file, 0, SEEK_SET);
    if (length <= 0 || length > static_cast<int64_t>(kCollectibleMetadataMaxBytes)) {
        SaltySDCore_fclose(file);
        trace("ItemConfig: invalid items_metadata.xml size");
        return false;
    }
    char* xml = static_cast<char*>(std::malloc(static_cast<size_t>(length) + 1));
    if (!xml) {
        SaltySDCore_fclose(file);
        trace("ItemConfig: insufficient memory for metadata");
        return false;
    }
    const size_t read = SaltySDCore_fread(xml, 1, static_cast<size_t>(length), file);
    SaltySDCore_fclose(file);
    if (read != static_cast<size_t>(length)) {
        std::free(xml);
        trace("ItemConfig: incomplete metadata read");
        return false;
    }
    xml[length] = '\0';

    bool valid = true;
    size_t count = 0;
    const char* cursor = xml;
    while ((cursor = std::strstr(cursor, "<item")) != nullptr) {
        const char* tag = cursor + 5;
        if ((*tag >= 'a' && *tag <= 'z') || (*tag >= 'A' && *tag <= 'Z') ||
            (*tag >= '0' && *tag <= '9') || *tag == '_') {
            cursor = tag;
            continue;
        }
        const char* close = std::strchr(tag, '>');
        int id = 0, quality = 0;
        if (!close ||
            !item_metadata_attribute(tag, close, "id", id) ||
            !item_metadata_attribute(tag, close, "quality", quality) ||
            id <= 0 || id >= static_cast<int>(kCollectibleQualityLimit) ||
            quality < 0 || quality > 4 || g_collectible_quality[id] != -1) {
            valid = false;
            break;
        }
        g_collectible_quality[id] = static_cast<int8_t>(quality);
        ++count;
        cursor = close + 1;
    }
    std::free(xml);
    if (!valid || count == 0) {
        std::memset(g_collectible_quality, -1, sizeof(g_collectible_quality));
        trace("ItemConfig: malformed or unsupported items_metadata.xml");
        return false;
    }
    g_collectible_metadata_loaded = true;
    char message[96] = {};
    std::snprintf(message, sizeof(message), "ItemConfig: loaded %u collectible qualities",
                  static_cast<unsigned>(count));
    trace(message);
    return true;
}

int lua_item_config_get_collectible(lua_State* state) {
    const int index = lua_istable(state, 1) ? 2 : 1;
    if (lua_gettop(state) < index || !lua_isnumber(state, index))
        return luaL_error(state, "GetCollectible(id) requires a numeric collectible id");
    const lua_Number number = lua_tonumber(state, index);
    const lua_Integer id = lua_tointeger(state, index);
    if (number != static_cast<lua_Number>(id))
        return luaL_error(state, "GetCollectible(id) requires an integer collectible id");
    if (!g_collectible_metadata_loaded || id <= 0 ||
        id >= static_cast<lua_Integer>(kCollectibleQualityLimit) ||
        g_collectible_quality[id] < 0) {
        lua_pushnil(state);
        return 1;
    }
    lua_createtable(state, 0, 2);
    lua_pushinteger(state, id);
    lua_setfield(state, -2, "ID");
    lua_pushinteger(state, g_collectible_quality[id]);
    lua_setfield(state, -2, "Quality");
    return 1;
}

int lua_isaac_get_item_config(lua_State* state) {
    if (g_item_config_table_ref == LUA_NOREF) {
        lua_pushnil(state);
        return 1;
    }
    lua_rawgeti(state, LUA_REGISTRYINDEX, g_item_config_table_ref);
    return 1;
}

void register_item_config_api(lua_State* state) {
    item_metadata_load();
    lua_newtable(state);
    lua_pushcfunction(state, lua_item_config_get_collectible);
    lua_setfield(state, -2, "GetCollectible");
    if (g_item_config_table_ref != LUA_NOREF)
        luaL_unref(state, LUA_REGISTRYINDEX, g_item_config_table_ref);
    g_item_config_table_ref = luaL_ref(state, LUA_REGISTRYINDEX);

    lua_newtable(state);
    lua_pushcfunction(state, lua_isaac_get_item_config);
    lua_setfield(state, -2, "GetItemConfig");
    lua_setglobal(state, "Isaac");
}
