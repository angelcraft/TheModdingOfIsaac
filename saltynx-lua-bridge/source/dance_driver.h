#pragma once
#include "dance_contract.h"

// Called only on the serialized post-update callback. See DANCE_CONTRACT.md.
struct DanceWitness {
    u32 magic, version, contract, requested, active, additions;
    u32 removals, failures, conflicts, last_error, target_id, reserved;
};
alignas(8) DanceWitness g_dance={0x44414E31u,1u,0,0,0,0,0,0,0,0,502,0};
static_assert(sizeof(DanceWitness)==48,"Dance witness layout");
uintptr_t g_dance_owner=0;
bool g_dance_checked=false, g_dance_verified=false;

bool dance_contract_ready(uintptr_t base) {
    if(g_dance_checked) return g_dance_verified;
    g_dance_checked=true;
    for(const auto& guard:kDanceGuards) {
        if(base>UINTPTR_MAX-guard.offset || !readable_range(base+guard.offset,guard.size)) return false;
        u64 hash=0xcbf29ce484222325ULL;
        const u8* bytes=reinterpret_cast<const u8*>(base+guard.offset);
        for(size_t i=0;i<guard.size;++i) hash=(hash^bytes[i])*0x100000001b3ULL;
        if(hash!=guard.fingerprint) return false;
    }
    uintptr_t table=0;
    if(!game_read(base,0xAACF38,table) || table!=base+0xA37100) return false;
    g_dance_verified=true; g_dance.contract=1; return true;
}
int dance_error(lua_State* state,u32 code,const char* reason) {
    ++g_dance.failures; g_dance.last_error=code;
    lua_pushnil(state); lua_pushstring(state,reason); return 2;
}
bool costume_present(uintptr_t player,uintptr_t item,bool& present) {
    uintptr_t begin=0,end=0; present=false;
    if(!game_read(player,0x1588,begin) || !game_read(player,0x1590,end) || end<begin ||
       (end-begin)%0x178 || (end-begin)/0x178>128) return false;
    if(begin==end) return true;
    if(!begin || !readable_range(begin,end-begin)) return false;
    for(uintptr_t p=begin;p<end;p+=0x178) {
        uintptr_t config=0;
        if(!game_read(p,0x158,config)) return false;
        if(config==item) present=true;
    }
    return true;
}
int lua_set_specialist_dance(lua_State* state) {
    const bool desired=lua_toboolean(state,1);
    g_dance.requested=desired;
    uintptr_t base=0,manager=0,game=0,room=0,array=0;
    if(!pedestal_contract_ready(base) || !dance_contract_ready(base)) {
        g_dance.contract=2; return dance_error(state,1,"dance code contract mismatch");
    }
    // A third-party hook at a required call-through is a conflict. Lazy imports
    // may still name the NRO's exact resolver; normal native dispatch resolves it.
    for(const auto& binding:kDanceBindings) {
        uintptr_t target=0;
        if(!game_read(base,binding.got,target) ||
           (target!=base+binding.target && target!=base+kLazyResolverOffset))
            return dance_error(state,12,"costume call-through conflict");
    }
    u32 status=0,count=0;
    if(!global_object(base,0xAAC648,manager) || !game_read(manager,8,status))
        return dance_error(state,2,"manager unavailable");
    if(status!=2) {
        // Never dereference a destroyed run's player to perform cleanup.
        g_dance_owner=0; g_dance.active=0; lua_pushboolean(state,1); return 1;
    }
    if(!global_object(base,0xAAC698,game) || !game_read(game,0x21550,room) || !room ||
       room>UINTPTR_MAX-0x1950 || !game_read(room+0x1950,0x78,array) ||
       !game_read(room+0x1950,0x84,count) || count>512 ||
       (count && (!array || array%8 || !readable_range(array,count*8))))
        return dance_error(state,3,"current player list unavailable");
    uintptr_t player=0; u32 players=0;
    for(u32 i=0;i<count;++i) {
        uintptr_t entity=0; u32 type=0,variant=0;
        if(!game_read(array,i*8,entity)) return dance_error(state,4,"entity array inaccessible");
        if(!entity) continue;
        if(!game_read(entity,0x38,type)) return dance_error(state,4,"entity inaccessible");
        if(type!=1) continue;
        ++players;
        uintptr_t vtable=0;
        if(!game_read(entity,0,vtable) || vtable!=base+0xA37110 ||
           !game_read(entity,0x3C,variant) || variant!=0 || !readable_range(entity,0x2850))
            return dance_error(state,5,"unsupported player receiver");
        player=entity;
    }
    if(g_dance_owner && g_dance_owner!=player) { g_dance_owner=0; g_dance.active=0; }
    if(players!=1) return dance_error(state,6,"candidate requires one player");
    uintptr_t items=0,items_end=0,item=0; u32 id=0;
    if(manager>UINTPTR_MAX-0x36538) return dance_error(state,7,"manager overflow");
    const uintptr_t config=manager+0x36538;
    if(!game_read(config,0x30,items) || !game_read(config,0x38,items_end) ||
       items_end<items || (items_end-items)%8 || (items_end-items)/8<5 ||
       (items_end-items)/8>1024 || !game_read(items,4*8,item) || !item ||
       !readable_range(item,0xC8) || !game_read(item,4,id) || id!=4)
        return dance_error(state,7,"null costume 4 unavailable");
    bool present=false;
    if(!costume_present(player,item,present)) return dance_error(state,8,"costume vector unavailable");
    if(desired && present && g_dance_owner!=player) {
        ++g_dance.conflicts;
        return dance_error(state,9,"costume already owned by game");
    }
    using CostumeCall=void (*)(void*,int);
    if(desired && !present) {
        reinterpret_cast<CostumeCall>(base+0x291BBC)(reinterpret_cast<void*>(player),4);
        if(!costume_present(player,item,present) || !present)
            return dance_error(state,10,"native costume addition not observed");
        g_dance_owner=player; ++g_dance.additions;
    } else if(!desired && g_dance_owner==player && present) {
        reinterpret_cast<CostumeCall>(base+0x2F2170)(reinterpret_cast<void*>(player),4);
        if(!costume_present(player,item,present) || present)
            return dance_error(state,11,"native costume removal not observed");
        ++g_dance.removals; g_dance_owner=0;
    }
    if(!present) g_dance_owner=0;
    g_dance.active=(present && g_dance_owner==player)?1:0;
    g_dance.last_error=0;
    lua_pushboolean(state,1); return 1;
}
