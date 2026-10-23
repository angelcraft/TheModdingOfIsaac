#pragma once
#include "pedestal_contract.h"

// All accesses run on the existing update owner, after the original update.
// Layout sources are documented in PEDESTAL_READ_CONTRACT.md.
struct PedestalWitness {
    u32 magic, version, native_contract, scans, failures, manager_state;
    u32 entities, players, pedestals, peak_pedestals, first_id, last_seen_id;
};
static_assert(sizeof(PedestalWitness) == 48, "Pedestal witness layout");
alignas(8) PedestalWitness g_pedestal = {0x50454431u,1u};
bool g_pedestal_checked = false;
bool g_pedestal_verified = false;

template<typename T> bool game_read(uintptr_t base, size_t offset, T& value) {
    if (!base || base > UINTPTR_MAX - offset) return false;
    const uintptr_t address = base + offset;
    if (address % alignof(T) || !readable_range(address, sizeof(T))) return false;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
    return true;
}

bool pedestal_contract_ready(uintptr_t& base) {
    if (!g_original_update) return false;
    const uintptr_t update = reinterpret_cast<uintptr_t>(g_original_update);
    if (update < kIsaacUpdateOffset) return false;
    base = update - kIsaacUpdateOffset;
    if (g_pedestal_checked) return g_pedestal_verified;
    g_pedestal_checked = true;
    bool valid = validate_image(base);
    for (const auto& guard : kPedestalGuards) {
        if (!valid || base > UINTPTR_MAX - guard.offset ||
            !readable_range(base + guard.offset, guard.size)) { valid = false; break; }
        const u8* bytes = reinterpret_cast<const u8*>(base + guard.offset);
        u64 hash = 0xcbf29ce484222325ULL;
        for (size_t i=0; i<guard.size; ++i) hash = (hash ^ bytes[i]) * 0x100000001b3ULL;
        if (hash != guard.fingerprint) { valid = false; break; }
    }
    g_pedestal_verified = valid;
    g_pedestal.native_contract = valid ? 1u : 2u;
    return valid;
}

bool global_object(uintptr_t base, size_t got_offset, uintptr_t& object) {
    uintptr_t global = 0;
    return game_read(base,got_offset,global) && game_read(global,0,object) && object;
}

int room_read_error(lua_State* state, const char* reason) {
    ++g_pedestal.failures;
    lua_pushnil(state);
    lua_pushstring(state, reason);
    return 2;
}

int lua_get_room_collectibles(lua_State* state) {
    uintptr_t base=0, manager=0, game=0, room=0, array=0;
    if (!pedestal_contract_ready(base))
        return room_read_error(state,"native read contract unavailable");
    u32 manager_state=0, count=0;
    if (!global_object(base,0xAAC648,manager) || !game_read(manager,8,manager_state))
        return room_read_error(state,"manager unavailable");
    g_pedestal.manager_state=manager_state;
    const bool in_run = manager_state == 2;
    u32 players=0, found=0;
    u32 ids[128] = {};
    if (in_run) {
        if (!global_object(base,0xAAC698,game) || !game_read(game,0x21550,room) || !room)
            return room_read_error(state,"current room unavailable");
        if (room > UINTPTR_MAX - 0x1950)
            return room_read_error(state,"room address overflow");
        const uintptr_t list=room+0x1950;
        if (!game_read(list,0x78,array) || !game_read(list,0x84,count) || count>512)
            return room_read_error(state,"entity list exceeds bounded reader");
        if (count && (!array || array % alignof(uintptr_t) ||
                      !readable_range(array,static_cast<size_t>(count)*sizeof(uintptr_t))))
            return room_read_error(state,"entity array inaccessible");
        for (u32 i=0; i<count; ++i) {
            uintptr_t entity=0;
            std::memcpy(&entity,reinterpret_cast<const void*>(array+i*sizeof(uintptr_t)),sizeof(entity));
            if (!entity) continue;
            struct EntityKind { u32 type, variant; int subtype; } kind = {};
            static_assert(sizeof(EntityKind)==12,"Entity classification layout");
            if (!game_read(entity,0x38,kind))
                return room_read_error(state,"entity classification inaccessible");
            if (kind.type==1) ++players;
            // Pickup/collectible enum mapping is a read-only device candidate.
            // Quality, visibility and collection flags are NOT inferred here.
            if (kind.type==5 && kind.variant==100 && kind.subtype>0) {
                if (found>=128) return room_read_error(state,"collectible result limit exceeded");
                ids[found++]=static_cast<u32>(kind.subtype);
            }
        }
    }
    ++g_pedestal.scans;
    g_pedestal.entities=count;
    g_pedestal.players=players;
    g_pedestal.pedestals=found;
    g_pedestal.first_id=found ? ids[0] : 0;
    if (found>g_pedestal.peak_pedestals) g_pedestal.peak_pedestals=found;
    if (found) g_pedestal.last_seen_id=ids[0];
    lua_createtable(state,0,5);
    lua_pushboolean(state,in_run); lua_setfield(state,-2,"in_run");
    lua_pushinteger(state,manager_state); lua_setfield(state,-2,"manager_state");
    lua_pushinteger(state,count); lua_setfield(state,-2,"entity_count");
    lua_pushinteger(state,players); lua_setfield(state,-2,"player_count");
    lua_createtable(state,found,0);
    for (u32 i=0; i<found; ++i) { lua_pushinteger(state,ids[i]); lua_rawseti(state,-2,i+1); }
    lua_setfield(state,-2,"items");
    return 1;
}
