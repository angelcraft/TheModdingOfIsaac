#pragma once

// Exact-build Music API calls only. Music is restored through Room::PlayMusic
// when the Specialist condition ends; no music pointer is retained between calls.
static const struct { uintptr_t offset; size_t size; u64 fingerprint; } kMusicGuards[] = {
    {0x426C38,0x8,0x66B59D3B5EF852F1ULL}, // Music::GetCurrentMusicID
    {0x42721C,0x1C,0x067930D904FBB95BULL}, // Music::Play(eMusic)
    {0x427908,0x38,0xAE66EF5054C801CBULL}, // Music::UpdateVolume
    {0x451834,0x6C4,0xD9501A63968105D4ULL}, // Room::PlayMusic
    {0x425218,0x1948,0x878E70E632C9A65DULL}, // Music::LoadConfig(path, preserve existing entries)
};

struct SpecialistMusicWitness {
    u32 magic, version, contract, requested, active, starts, restores;
    u32 errors, last_error, track_id, wait_callbacks, attempts;
};
// LoadConfig accepts only explicit IDs 1..118. Slot 19 is absent from the
// extracted XML; also verify that the live slot is empty before claiming it.
alignas(8) SpecialistMusicWitness g_specialist_music={0x534D5531u,1u,0,0,0,0,0,0,0,19,0,0};
bool g_specialist_music_started=false;
bool g_specialist_music_config_loaded=false;
bool g_music_contract_checked=false, g_music_contract_verified=false;

bool music_contract_ready(uintptr_t base) {
    if(g_music_contract_checked) return g_music_contract_verified;
    g_music_contract_checked=true;
    for(const auto& guard:kMusicGuards) {
        if(base>UINTPTR_MAX-guard.offset || !readable_range(base+guard.offset,guard.size)) return false;
        u64 hash=0xcbf29ce484222325ULL;
        const u8* bytes=reinterpret_cast<const u8*>(base+guard.offset);
        for(size_t i=0;i<guard.size;++i) hash=(hash^bytes[i])*0x100000001b3ULL;
        if(hash!=guard.fingerprint) return false;
    }
    g_music_contract_verified=true; g_specialist_music.contract=1;
    return true;
}

bool libcpp_string_contains(uintptr_t object,const char* needle) {
    u8 tag=0;
    if(!game_read(object,0,tag)) return false;
    uintptr_t data=0; u64 length=0;
    if(tag&1) {
        u64 capacity=0;
        if(!game_read(object,0,capacity) || !game_read(object,8,length) || !game_read(object,16,data)) return false;
        capacity&=~1ULL;
        if(length>capacity || length>256) return false;
    } else {
        length=tag>>1; data=object+1;
        if(length>22) return false;
    }
    if(!length || !data || !readable_range(data,static_cast<size_t>(length))) return false;
    const size_t needle_size=std::strlen(needle);
    if(needle_size>length) return false;
    const char* bytes=reinterpret_cast<const char*>(data);
    for(size_t i=0;i+needle_size<=length;++i)
        if(std::memcmp(bytes+i,needle,needle_size)==0) return true;
    return false;
}

bool specialist_music_entry(uintptr_t music,uintptr_t& entry) {
    uintptr_t begin=0,end=0;
    if(!readable_range(music,0x3E8) || !game_read(music,0x3D8,begin) ||
       !game_read(music,0x3E0,end) || end<begin || (end-begin)%0x70 ||
       (end-begin)/0x70>1024 || (end-begin)/0x70<=g_specialist_music.track_id) return false;
    entry=begin+static_cast<uintptr_t>(g_specialist_music.track_id)*0x70;
    return readable_range(entry,0x70);
}

bool specialist_music_configured(uintptr_t music) {
    uintptr_t entry=0;
    if(!specialist_music_entry(music,entry)) return false;
    // This is the path member read by the verified Music::Play implementation.
    return libcpp_string_contains(entry+0x38,"specialist_switch.ogg");
}

bool libcpp_string_empty(uintptr_t object) {
    u8 tag=0; u64 length=0;
    if(!game_read(object,0,tag)) return false;
    if(!(tag&1)) return (tag>>1)==0;
    return game_read(object,8,length) && length==0;
}

bool specialist_music_slot_unused(uintptr_t music) {
    uintptr_t entry=0,layers=0,layers_end=0;
    if(!specialist_music_entry(music,entry)) return false;
    return libcpp_string_empty(entry+8) && libcpp_string_empty(entry+0x20) &&
           libcpp_string_empty(entry+0x38) && game_read(entry,0x50,layers) &&
           game_read(entry,0x58,layers_end) && layers==layers_end;
}

int lua_set_specialist_music(lua_State* state) {
    const bool desired=lua_toboolean(state,1);
    g_specialist_music.requested=desired;
    uintptr_t base=0,manager=0,game=0,room=0;
    if(!pedestal_contract_ready(base) || !music_contract_ready(base)) {
        ++g_specialist_music.errors; g_specialist_music.last_error=1;
        lua_pushnil(state); lua_pushstring(state,"music code contract mismatch"); return 2;
    }
    if(!global_object(base,0xAAC648,manager)) {
        ++g_specialist_music.errors; g_specialist_music.last_error=2;
        lua_pushnil(state); lua_pushstring(state,"music manager unavailable"); return 2;
    }
    u32 run_state=0;
    if(!game_read(manager,8,run_state)) {
        ++g_specialist_music.errors; g_specialist_music.last_error=2;
        lua_pushnil(state); lua_pushstring(state,"music manager state unavailable"); return 2;
    }
    if(!global_object(base,0xAAC698,game) || !game_read(game,0x21550,room) || !room ||
       !readable_range(room,0x1950)) {
        g_specialist_music.active=0; g_specialist_music_started=false;
        lua_pushboolean(state,1); return 1;
    }
    if(manager>UINTPTR_MAX-0x36068) {
        ++g_specialist_music.errors; g_specialist_music.last_error=3;
        lua_pushnil(state); lua_pushstring(state,"music object address overflow"); return 2;
    }
    uintptr_t music=manager+0x36068;
    if(!readable_range(music,0x3E8)) {
        ++g_specialist_music.errors; g_specialist_music.last_error=3;
        lua_pushnil(state); lua_pushstring(state,"music object unavailable"); return 2;
    }
    using CurrentCall=int (*)(void*);
    using PlayCall=void (*)(void*,int);
    using VolumeCall=void (*)(void*);
    using RoomMusicCall=void (*)(void*);
    const int current=reinterpret_cast<CurrentCall>(base+0x426C38)(reinterpret_cast<void*>(music));
    if(!desired || run_state!=2) {
        if(g_specialist_music_started && current==static_cast<int>(g_specialist_music.track_id)) {
            reinterpret_cast<RoomMusicCall>(base+0x451834)(reinterpret_cast<void*>(room));
            ++g_specialist_music.restores;
        }
        g_specialist_music.active=0; g_specialist_music_started=false;
        g_specialist_music.wait_callbacks=0; g_specialist_music.attempts=0;
        g_specialist_music.last_error=0;
        lua_pushboolean(state,1); return 1;
    }
    if(!specialist_music_configured(music)) {
        if(!g_specialist_music_config_loaded) {
            if(!specialist_music_slot_unused(music)) {
                ++g_specialist_music.errors; g_specialist_music.last_error=6;
                lua_pushnil(state); lua_pushstring(state,"Specialist music slot 19 is occupied"); return 2;
            }
            using LoadConfigCall=void (*)(void*,const char*,bool);
            reinterpret_cast<LoadConfigCall>(base+0x425218)(
                reinterpret_cast<void*>(music),"resources/specialist_music.xml",true);
            g_specialist_music_config_loaded=true;
        }
    }
    if(!specialist_music_configured(music)) {
        ++g_specialist_music.errors; g_specialist_music.last_error=4;
        lua_pushnil(state); lua_pushstring(state,"music track 19 is not loaded"); return 2;
    }
    if(current==static_cast<int>(g_specialist_music.track_id)) {
        g_specialist_music.active=1; g_specialist_music_started=true;
        g_specialist_music.wait_callbacks=0; g_specialist_music.attempts=0;
        lua_pushboolean(state,1); return 1;
    }
    if(g_specialist_music_started) {
        // Let an in-flight crossfade settle before retrying the native API.
        if(++g_specialist_music.wait_callbacks<20) {
            lua_pushboolean(state,1); return 1;
        }
        if(g_specialist_music.attempts>=2) {
            ++g_specialist_music.errors; g_specialist_music.last_error=5;
            g_specialist_music.active=0; g_specialist_music_started=false;
            lua_pushnil(state); lua_pushstring(state,"Specialist soundtrack did not start"); return 2;
        }
    }
    reinterpret_cast<PlayCall>(base+0x42721C)(reinterpret_cast<void*>(music),static_cast<int>(g_specialist_music.track_id));
    reinterpret_cast<VolumeCall>(base+0x427908)(reinterpret_cast<void*>(music));
    ++g_specialist_music.starts; ++g_specialist_music.attempts;
    g_specialist_music.wait_callbacks=0; g_specialist_music_started=true;
    g_specialist_music.active=0; g_specialist_music.last_error=0;
    lua_pushboolean(state,1); return 1;
}
