/* SPDX-License-Identifier: GPL-2.0-only
 * Adapted dmnt:cht client wrappers: Copyright (c) Atmosphere-NX.
 * Uses libtesla, Copyright (c) WerWolv and contributors.
 */
#define TESLA_INIT_IMPL
#include <tesla.hpp>

#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace {
constexpr u64 kTitleId = UINT64_C(0x010021C000B6A000);
constexpr const char *kLogPath = "sdmc:/switch/TheModdingOfIsaac/module-overlay.log";
constexpr const char *kCheatPath = "sdmc:/atmosphere/contents/010021c000b6a000/cheats/b6e5bdb9dc12e1d1.txt";

constexpr u8 kLoaderBid[0x20] = {
    0xB6,0xE5,0xBD,0xB9,0xDC,0x12,0xE1,0xD1,0xA2,0x5C,0xBF,0xDA,0x17,0xF4,0xBE,0x24,
    0xB4,0x75,0x4E,0xF5,0,0,0,0,0,0,0,0,0,0,0,0
};
constexpr u8 kRepentanceBid[0x20] = {
    0x91,0xC7,0x3F,0xDD,0x57,0x50,0x61,0x31,0x8D,0x68,0x88,0x63,0x16,0xAF,0xEA,0xC7,
    0x23,0x88,0xB2,0xAB,0,0,0,0,0,0,0,0,0,0
};

struct Extents { u64 base, size; };
struct CheatMetadata {
    u64 process_id, title_id;
    Extents main_nso, heap, alias, address_space;
    u8 main_nso_build_id[0x20];
};
static_assert(sizeof(CheatMetadata) == 0x70);
static_assert(sizeof(MemoryInfo) == 0x28);

Service g_cheat{};
bool g_cheat_open = false, g_ldr_open = false, g_pminfo_open = false;
char g_status[192] = "Waiting";
char g_detail[192] = "Open this overlay while Isaac is running.";
FILE *g_log = nullptr;

void set_status(const char *status, const char *detail) {
    std::snprintf(g_status, sizeof(g_status), "%s", status);
    std::snprintf(g_detail, sizeof(g_detail), "%s", detail);
    if (g_log) std::fprintf(g_log, "%s: %s\n", status, detail);
}
bool fail_result(const char *what, Result rc) {
    char detail[160];
    std::snprintf(detail, sizeof(detail), "%s (0x%08" PRIX32 ")", what, static_cast<u32>(rc));
    set_status("Stopped", detail);
    return false;
}
void log_bid(const char *label, const u8 bid[0x20]) {
    if (!g_log) return;
    std::fprintf(g_log, "%s=", label);
    for (size_t i = 0; i < 0x20; ++i) std::fprintf(g_log, "%02X", bid[i]);
    std::fputc('\n', g_log);
}

Result cheat_has_process(bool *out) {
    u8 value = 0; Result rc = serviceDispatchOut(&g_cheat, 65000, value);
    if (R_SUCCEEDED(rc)) *out = (value & 1) != 0;
    return rc;
}
Result cheat_metadata(CheatMetadata *out) { return serviceDispatchOut(&g_cheat, 65002, *out); }
Result cheat_force_open() { return serviceDispatch(&g_cheat, 65003); }
Result cheat_read(u64 address, void *out, size_t size) {
    const struct { u64 address, size; } in{address, size};
    return serviceDispatchIn(&g_cheat, 65102, in,
        .buffer_attrs = { SfBufferAttr_Out | SfBufferAttr_HipcMapAlias },
        .buffers = { { out, size } });
}
Result cheat_query(MemoryInfo *out, u64 address) {
    return serviceDispatchInOut(&g_cheat, 65104, address, *out);
}

bool cheat_file_absent() {
    struct stat info{}; errno = 0;
    if (stat(kCheatPath, &info) == 0) {
        set_status("Stopped", "Cheat file exists; refusing debugger attach.");
        return false;
    }
    if (errno != ENOENT) {
        set_status("Stopped", "Could not prove the exact cheat file is absent.");
        return false;
    }
    return true;
}

bool readable_rx(u64 address, size_t size) {
    MemoryInfo m{}; Result rc = cheat_query(&m, address);
    if (R_FAILED(rc) || (m.perm & Perm_Rx) != Perm_Rx || m.addr > address) return false;
    const u64 delta = address - m.addr;
    return delta <= m.size && size <= m.size - delta;
}

bool run_snapshot() {
    mkdir("sdmc:/switch/TheModdingOfIsaac", 0777);
    g_log = std::fopen(kLogPath, "wb");
    if (!g_log) { set_status("Stopped", "Cannot create module-overlay.log."); return false; }
    std::fprintf(g_log, "IsaacModuleOverlay 0.1.0\nmode=read-only; no game memory writes or hooks\n");

    u64 pid = 0, title = 0;
    Result rc = pmdmntGetApplicationProcessId(&pid);
    if (R_FAILED(rc)) return fail_result("Cannot find running application", rc);
    rc = pminfoInitialize();
    if (R_FAILED(rc)) return fail_result("Cannot open pm:info", rc);
    g_pminfo_open = true;
    rc = pminfoGetProgramId(&title, pid);
    if (R_FAILED(rc)) return fail_result("Cannot read application title", rc);
    std::fprintf(g_log, "pid=0x%016" PRIX64 " title=%016" PRIX64 "\n", pid, title);
    if (title != kTitleId) { set_status("Stopped", "Isaac is not the running application."); return false; }

    rc = ldrDmntInitialize();
    if (R_FAILED(rc)) return fail_result("Cannot open ldr:dmnt", rc);
    g_ldr_open = true;
    LoaderModuleInfo modules[32]{}; s32 count = 0;
    rc = ldrDmntGetProcessModuleInfo(pid, modules, 32, &count);
    if (R_FAILED(rc)) return fail_result("Cannot list Isaac modules", rc);
    if (count <= 0 || count >= 32) { set_status("Stopped", "Module list is empty or truncated."); return false; }

    const LoaderModuleInfo *loader = nullptr, *repentance = nullptr;
    for (s32 i = 0; i < count; ++i) {
        std::fprintf(g_log, "module[%d] base=0x%016" PRIX64 " size=0x%016" PRIX64 "\n", i,
                     modules[i].base_address, modules[i].size);
        log_bid("module_bid", modules[i].build_id);
        if (std::memcmp(modules[i].build_id, kLoaderBid, 0x20) == 0) loader = &modules[i];
        if (std::memcmp(modules[i].build_id, kRepentanceBid, 0x20) == 0) repentance = &modules[i];
    }
    if (!loader || !repentance) { set_status("Stopped", "Expected Isaac modules/builds were not found."); return false; }

    rc = smGetService(&g_cheat, "dmnt:cht");
    if (R_FAILED(rc)) return fail_result("Cannot open dmnt:cht", rc);
    g_cheat_open = true;
    bool attached = false;
    rc = cheat_has_process(&attached);
    if (R_FAILED(rc)) return fail_result("Cannot inspect debugger state", rc);
    std::fprintf(g_log, "preexisting_attachment=%s\n", attached ? "yes" : "no");
    if (!attached) {
        if (!cheat_file_absent()) return false;
        rc = cheat_force_open();
        if (R_FAILED(rc)) return fail_result("Cannot attach to verified Isaac process", rc);
        std::fprintf(g_log, "attachment_created=yes\n");
    }

    CheatMetadata metadata{};
    rc = cheat_metadata(&metadata);
    if (R_FAILED(rc)) return fail_result("Cannot read process metadata", rc);
    if (metadata.process_id != pid || metadata.title_id != kTitleId ||
        std::memcmp(metadata.main_nso_build_id, kLoaderBid, 0x20) != 0 ||
        metadata.main_nso.base != loader->base_address) {
        set_status("Stopped", "Debugger metadata does not match verified Isaac."); return false;
    }

    struct Window { const char *name; u64 offset; u8 bytes[16]; };
    constexpr Window windows[] = {
        {"Manager::Update",0x3F8DB8,{0xFF,0x43,0x01,0xD1,0xFD,0x7B,0x01,0xA9,0xFD,0x43,0x00,0x91,0xF7,0x13,0x00,0xF9}},
        {"Game::Update",0x351884,{0xFF,0xC3,0x07,0xD1,0xFD,0x7B,0x19,0xA9,0xFD,0x43,0x06,0x91,0xFC,0xD3,0x00,0xF9}},
        {"Room::Update",0x46D788,{0xED,0x33,0xB7,0x6D,0xEB,0x2B,0x01,0x6D,0xE9,0x23,0x02,0x6D,0xFD,0x7B,0x03,0xA9}},
        {"EntityList::Update",0x767C0,{0xFF,0x03,0x03,0xD1,0xEC,0x1B,0x00,0xFD,0xEB,0x2B,0x04,0x6D,0xE9,0x23,0x05,0x6D}}
    };
    unsigned matched = 0;
    for (const auto &w : windows) {
        const u64 address = repentance->base_address + w.offset;
        u8 actual[16]{};
        if (!readable_rx(address, sizeof(actual))) { set_status("Stopped", "A code window is not readable/executable."); return false; }
        rc = cheat_read(address, actual, sizeof(actual));
        if (R_FAILED(rc)) return fail_result("Cannot read a code window", rc);
        const bool same = std::memcmp(actual, w.bytes, sizeof(actual)) == 0;
        matched += same ? 1 : 0;
        std::fprintf(g_log, "window=%s address=0x%016" PRIX64 " signature=%s bytes=", w.name, address, same ? "MATCH" : "MISMATCH");
        for (u8 b : actual) std::fprintf(g_log, "%02X", b);
        std::fputc('\n', g_log);
    }
    if (matched != 4) { set_status("Stopped", "One or more code signatures differ; no patch is safe."); return false; }
    set_status("Success", "Both builds and all 4 code signatures match.");
    std::fprintf(g_log, "result=success repentance_base=0x%016" PRIX64 "\n", repentance->base_address);
    return true;
}

void close_services() {
    if (g_log) { std::fclose(g_log); g_log = nullptr; }
    if (g_cheat_open) { serviceClose(&g_cheat); g_cheat_open = false; }
    if (g_ldr_open) { ldrDmntExit(); g_ldr_open = false; }
    if (g_pminfo_open) { pminfoExit(); g_pminfo_open = false; }
}
}

class SnapshotGui : public tsl::Gui {
public:
    tsl::elm::Element *createUI() override {
        auto *frame = new tsl::elm::OverlayFrame("Isaac Module Snapshot", "read-only v0.1.0");
        auto *list = new tsl::elm::List();
        list->addItem(new tsl::elm::ListItem(g_status));
        list->addItem(new tsl::elm::ListItem(g_detail));
        list->addItem(new tsl::elm::ListItem("Log: /switch/TheModdingOfIsaac/module-overlay.log"));
        frame->setContent(list);
        return frame;
    }
};

class SnapshotOverlay : public tsl::Overlay {
public:
    void initServices() override { run_snapshot(); close_services(); }
    void exitServices() override { close_services(); }
    std::unique_ptr<tsl::Gui> loadInitialGui() override { return initially<SnapshotGui>(); }
};

int main(int argc, char **argv) { return tsl::loop<SnapshotOverlay>(argc, argv); }

