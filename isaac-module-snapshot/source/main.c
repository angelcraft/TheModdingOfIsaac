/* SPDX-License-Identifier: GPL-2.0-only
 * Adapted dmnt:cht client wrappers: Copyright (c) Atmosphère-NX.
 * IsaacModuleSnapshot: bounded, read-only observation of Isaac's loaded modules.
 * dmnt:cht IPC layouts follow Atmosphere-NX/Atmosphere commit
 * 6b0c077b4c608a3a2ee712b8dc35fee87ceb62e8 (GPL-2.0).
 * No game hooks, game memory writes, cheat toggles, or debugger pause commands.
 */
#define NX_SERVICE_ASSUME_NON_DOMAIN
#include <switch.h>

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "bridge_contract.h"

#define EXPECTED_TITLE_ID UINT64_C(0x010021C000B6A000)
#define LOG_PATH "sdmc:/switch/TheModdingOfIsaac/module-snapshot.log"
#define BRIDGE_STARTUP_PATH "sdmc:/SaltySD/plugins/010021C000B6A000/bridge-startup.log"
#define CHEAT_PATH "sdmc:/atmosphere/contents/010021c000b6a000/cheats/b6e5bdb9dc12e1d1.txt"
#define MAX_MAPPINGS 4096
#define MAPPING_BATCH 32
#define HEADER_BYTES 0x80

static const u8 k_expected_build_id[0x20] = {
    0xB6, 0xE5, 0xBD, 0xB9, 0xDC, 0x12, 0xE1, 0xD1,
    0xA2, 0x5C, 0xBF, 0xDA, 0x17, 0xF4, 0xBE, 0x24,
    0xB4, 0x75, 0x4E, 0xF5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const u8 k_repentance_build_id[0x20] = {
    0x91, 0xC7, 0x3F, 0xDD, 0x57, 0x50, 0x61, 0x31,
    0x8D, 0x68, 0x88, 0x63, 0x16, 0xAF, 0xEA, 0xC7,
    0x23, 0x88, 0xB2, 0xAB, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

typedef struct { u64 base, size; } MemoryExtents;
typedef struct {
    u64 process_id;
    u64 title_id;
    MemoryExtents main_nso, heap, alias, address_space;
    u8 main_nso_build_id[0x20];
} CheatProcessMetadata;

_Static_assert(sizeof(CheatProcessMetadata) == 0x70, "dmnt metadata ABI");
_Static_assert(sizeof(MemoryInfo) == 0x28, "dmnt mapping ABI");

static FILE *g_log;
static Service g_cheat_service;
static bool g_cheat_open, g_pm_dmnt_open, g_pm_info_open, g_ldr_open;
static size_t g_verified_repentance;
static u64 g_loader_run_pointer;
static u64 g_bridge_stage_address;
static u64 g_bridge_hook_address;
static bool g_bridge_hook_observed;
static bool g_unexpected_bindings;

static void log_line(const char *format, ...) {
    if (!g_log) return;
    va_list args;
    va_start(args, format);
    vfprintf(g_log, format, args);
    va_end(args);
    fputc('\n', g_log);
    fflush(g_log);
}

static bool fail_message(const char *message) {
    printf("Stopped: %s\n", message);
    log_line("ERROR: %s", message);
    return false;
}

static bool fail_result(const char *operation, Result result) {
    printf("Stopped: %s (0x%08" PRIX32 ")\n", operation, (uint32_t)result);
    log_line("ERROR: %s result=0x%08" PRIX32, operation, (uint32_t)result);
    return false;
}

static void log_build_id(const char *label, const u8 id[0x20]) {
    char hex[0x41];
    for (size_t i = 0; i < 0x20; ++i)
        snprintf(hex + i * 2, sizeof(hex) - i * 2, "%02X", id[i]);
    log_line("%s=%s", label, hex);
}

/* Read only the two marker addresses that our own SaltyNX plugin wrote during
 * startup; do not perform file IPC from an injected update callback. */
static void load_bridge_marker(void) {
    FILE *file = fopen(BRIDGE_STARTUP_PATH, "rb");
    if (!file) {
        log_line("bridge_marker=unavailable (no startup log)");
        return;
    }
    char line[192];
    while (fgets(line, sizeof(line), file)) {
        uint64_t parsed = 0;
        if (sscanf(line, "bridge_stage_address=0x%" SCNx64, &parsed) == 1)
            g_bridge_stage_address = parsed;
        else if (sscanf(line, "bridge_hook_address=0x%" SCNx64, &parsed) == 1)
            g_bridge_hook_address = parsed;
    }
    fclose(file);
    log_line("bridge_marker=%s", (g_bridge_stage_address && g_bridge_hook_address)
             ? "present" : "incomplete");
}

// Declared here because the observation helpers follow the service wrappers.
static Result cheat_read(u64 address, void *out, size_t size);
static Result cheat_query(MemoryInfo *out, u64 address);

static void report_bridge_stage(void) {
    if (!g_bridge_stage_address) {
        log_line("bridge_callback=unknown (stage address unavailable)");
        return;
    }
    // A stale or mistyped marker may point anywhere. Only accept a candidate
    // if the hook pointer itself belongs to an executable mapping.
    if (!g_bridge_hook_address) {
        log_line("bridge_callback=unknown (hook address unavailable)");
        return;
    }
    MemoryInfo hook_region = {0};
    Result rc = cheat_query(&hook_region, g_bridge_hook_address);
    if (R_FAILED(rc) || (hook_region.perm & Perm_Rx) != Perm_Rx ||
        hook_region.addr > g_bridge_hook_address ||
        g_bridge_hook_address - hook_region.addr >= hook_region.size) {
        log_line("bridge_callback=unknown (hook pointer not in executable mapping)");
        return;
    }
    MemoryInfo region = {0};
    rc = cheat_query(&region, g_bridge_stage_address);
    if (R_FAILED(rc) || !(region.perm & Perm_R) ||
        region.addr > g_bridge_stage_address ||
        g_bridge_stage_address - region.addr > region.size ||
        sizeof(u32) > region.size - (g_bridge_stage_address - region.addr)) {
        log_line("bridge_callback=unknown (stage memory inaccessible)");
        return;
    }
    u32 stage = 0;
    rc = cheat_read(g_bridge_stage_address, &stage, sizeof(stage));
    if (R_FAILED(rc)) {
        log_line("bridge_callback=unknown (stage read failed)");
        return;
    }
    log_line("bridge_stage=%" PRIu32 " bridge_got_hook=%s",
             (uint32_t)stage, g_bridge_hook_observed ? "yes" : "no");
    if (stage == 8 && g_bridge_hook_observed) {
        log_line("bridge_callback=SUCCESS (Lua on_update executed in game)");
        printf("Lua on_update(): SUCCESS!\n");
    } else if (stage == 9 && g_bridge_hook_observed) {
        log_line("bridge_callback=ERROR (Lua on_update failed)");
        printf("Lua callback: ERROR (stage 9).\n");
    } else {
        log_line("bridge_callback=NOT_CONFIRMED");
        printf("Lua callback not confirmed (stage %" PRIu32 ", hook %s).\n",
               (uint32_t)stage, g_bridge_hook_observed ? "observed" : "absent");
    }
}

/* Minimal client: deliberately includes only observation and guarded attach. */
static Result cheat_has_process(bool *out) {
    u8 attached = 0;
    Result rc = serviceDispatchOut(&g_cheat_service, 65000, attached);
    if (R_SUCCEEDED(rc)) *out = (attached & 1) != 0;
    return rc;
}

static Result cheat_metadata(CheatProcessMetadata *out) {
    return serviceDispatchOut(&g_cheat_service, 65002, *out);
}

static Result cheat_force_open(void) {
    return serviceDispatch(&g_cheat_service, 65003);
}

static Result cheat_mapping_count(u64 *out) {
    return serviceDispatchOut(&g_cheat_service, 65100, *out);
}

static Result cheat_mappings(MemoryInfo *out, size_t capacity, u64 offset, u64 *out_count) {
    return serviceDispatchInOut(&g_cheat_service, 65101, offset, *out_count,
        .buffer_attrs = { SfBufferAttr_Out | SfBufferAttr_HipcMapAlias },
        .buffers = { { out, capacity * sizeof(*out) } },
    );
}

static Result cheat_read(u64 address, void *out, size_t size) {
    const struct { u64 address, size; } in = { address, size };
    return serviceDispatchIn(&g_cheat_service, 65102, in,
        .buffer_attrs = { SfBufferAttr_Out | SfBufferAttr_HipcMapAlias },
        .buffers = { { out, size } },
    );
}

static Result cheat_query(MemoryInfo *out, u64 address) {
    return serviceDispatchInOut(&g_cheat_service, 65104, address, *out);
}

static bool current_application(u64 *pid) {
    Result rc = pmdmntInitialize();
    if (R_FAILED(rc)) return fail_result("open pm:dmnt", rc);
    g_pm_dmnt_open = true;
    rc = pminfoInitialize();
    if (R_FAILED(rc)) return fail_result("open pm:info", rc);
    g_pm_info_open = true;
    rc = pmdmntGetApplicationProcessId(pid);
    if (R_FAILED(rc)) return fail_result("find running application", rc);
    u64 title_id = 0;
    rc = pminfoGetProgramId(&title_id, *pid);
    if (R_FAILED(rc)) return fail_result("read application title ID", rc);
    log_line("application_pid=0x%016" PRIX64 " title_id=%016" PRIX64,
             (uint64_t)*pid, (uint64_t)title_id);
    if (title_id != EXPECTED_TITLE_ID)
        return fail_message("Application title differs from the required Isaac title.");
    return true;
}

/* Establish build identity before attaching: ForceOpen may load saved cheats. */
static bool verify_loader_build(u64 pid, LoaderModuleInfo *matched) {
    Result rc = ldrDmntInitialize();
    if (R_FAILED(rc)) return fail_result("open ldr:dmnt", rc);
    g_ldr_open = true;
    LoaderModuleInfo modules[16] = {0};
    s32 count = 0;
    rc = ldrDmntGetProcessModuleInfo(pid, modules, 16, &count);
    if (R_FAILED(rc)) return fail_result("read loader module identities", rc);
    if (count < 1 || count >= 16)
        return fail_message("Loader returned an empty or potentially truncated module list.");
    bool found = false;
    for (s32 i = 0; i < count; ++i) {
        log_line("loader_module[%" PRId32 "] base=0x%016" PRIX64 " size=0x%016" PRIX64,
                 (int32_t)i, (uint64_t)modules[i].base_address, (uint64_t)modules[i].size);
        log_build_id("loader_module_build_id", modules[i].build_id);
        if (memcmp(modules[i].build_id, k_expected_build_id, 0x20) == 0) {
            if (found) return fail_message("Duplicate expected loader module identity.");
            *matched = modules[i];
            found = true;
        }
    }
    if (!found) return fail_message("Running loader build differs from the verified 1.7.9b build.");
    return true;
}

static bool exact_cheat_file_absent(void) {
    struct stat info;
    errno = 0;
    if (stat(CHEAT_PATH, &info) == 0)
        return fail_message("Build cheat file exists; refusing to activate the cheat process.");
    const int error = errno;
    if (error != ENOENT) {
        log_line("cheat_file_stat_errno=%d", error);
        return fail_message("Cannot prove the build cheat file is absent.");
    }
    log_line("exact_build_cheat_file=absent (ENOENT)");
    return true;
}

static bool verify_metadata(const CheatProcessMetadata *metadata, u64 pid,
                            const LoaderModuleInfo *loader) {
    if (metadata->process_id != pid || metadata->title_id != EXPECTED_TITLE_ID ||
        memcmp(metadata->main_nso_build_id, k_expected_build_id, 0x20) != 0 ||
        metadata->main_nso.base != loader->base_address || metadata->main_nso.size != loader->size)
        return fail_message("Attached process metadata does not match the verified application/build.");
    return true;
}

static void log_header(const u8 *header, size_t size) {
    for (size_t offset = 0; offset < size; offset += 16) {
        char row[16 * 3 + 1] = {0};
        const size_t count = size - offset < 16 ? size - offset : 16;
        for (size_t i = 0; i < count; ++i)
            snprintf(row + i * 3, sizeof(row) - i * 3, "%02X ", header[offset + i]);
        log_line("  header+0x%02zx: %s", offset, row);
    }
}

static bool bounded_read(u64 address, void *out, size_t size, u32 permissions) {
    MemoryInfo mapping = {0};
    Result rc = cheat_query(&mapping, address);
    if (R_FAILED(rc)) return fail_result("query contract memory", rc);
    if ((mapping.perm & permissions) != permissions || mapping.addr > address ||
        address - mapping.addr > mapping.size || size > mapping.size - (address - mapping.addr))
        return fail_message("Contract range is not wholly readable with the required permissions.");
    rc = cheat_read(address, out, size);
    return R_SUCCEEDED(rc) ? true : fail_result("read contract memory", rc);
}

static bool verify_bridge_contract(u64 base) {
    static u8 buffer[0x4000];
    if (base > UINT64_MAX - 0x4C0840 || g_loader_run_pointer != base + 0x4C0840)
        return fail_message("Loader entry binding does not lead to the verified Repentance module.");
    log_line("bridge_loader_entry=MATCH");
    for (size_t i = 0; i < sizeof(k_contract_code) / sizeof(k_contract_code[0]); ++i) {
        const size_t size = k_contract_code[i].size;
        if (size > sizeof(buffer) || base > UINT64_MAX - k_contract_code[i].offset)
            return fail_message("Invalid contract code extent.");
        if (!bounded_read(base + k_contract_code[i].offset, buffer, size, Perm_Rx)) return false;
        u8 hash[32];
        sha256CalculateHash(hash, buffer, size);
        const bool match = memcmp(hash, k_contract_code[i].hash, sizeof(hash)) == 0;
        log_line("bridge_code=%s bytes=%zu sha256=%s", k_contract_code[i].name, size, match ? "MATCH" : "MISMATCH");
        if (!match) return fail_message("A complete bridge function/PLT differs from the verified build.");
    }
    // Observe the live IsaacUpdate relocation without changing game memory.
    if (base > UINT64_MAX - UINT64_C(0xAA5C20))
        return fail_message("IsaacUpdate GOT address overflow.");
    MemoryInfo isaac_got_mapping = {0};
    const u64 isaac_got_address = base + UINT64_C(0xAA5C20);
    Result isaac_got_result = cheat_query(&isaac_got_mapping, isaac_got_address);
    if (R_FAILED(isaac_got_result))
        return fail_result("query IsaacUpdate GOT mapping", isaac_got_result);
    if (isaac_got_mapping.addr > isaac_got_address ||
        isaac_got_address - isaac_got_mapping.addr > isaac_got_mapping.size ||
        sizeof(u64) > isaac_got_mapping.size - (isaac_got_address - isaac_got_mapping.addr))
        return fail_message("IsaacUpdate GOT slot crosses its mapping.");
    log_line("bridge_isaac_update_got_permission=0x%08" PRIX32 " writable=%s",
             (uint32_t)isaac_got_mapping.perm,
             (isaac_got_mapping.perm & Perm_W) ? "yes" : "no");
    for (size_t i = 0; i < sizeof(k_contract_bindings) / sizeof(k_contract_bindings[0]); ++i) {
        if (base > UINT64_MAX - k_contract_bindings[i].got || base > UINT64_MAX - k_contract_bindings[i].target)
            return fail_message("Contract GOT address overflow.");
        u64 pointer = 0;
        if (!bounded_read(base + k_contract_bindings[i].got, &pointer, sizeof(pointer), Perm_R)) return false;
        const u64 expected = base + k_contract_bindings[i].target;
        const bool match = pointer == expected;
        const bool is_our_hook = !match && strcmp(k_contract_bindings[i].name, "IsaacUpdate") == 0 &&
                                 g_bridge_hook_address && pointer == g_bridge_hook_address;
        if (is_our_hook) g_bridge_hook_observed = true;
        log_line("bridge_binding=%s actual=0x%016" PRIX64 " expected=0x%016" PRIX64 " result=%s",
                 k_contract_bindings[i].name, (uint64_t)pointer, (uint64_t)expected,
                 match ? "MATCH" : (is_our_hook ? "EXPECTED_HOOK" : "OTHER"));
        if (!match && !is_our_hook) {
            // A secondary import may point at a resolver or another valid
            // module-specific target. Preserve the mismatch instead of
            // abandoning the independent callback-stage observation.
            g_unexpected_bindings = true;
            log_line("bridge_binding_warning=%s unexpected_target=0x%016" PRIX64,
                     k_contract_bindings[i].name, (uint64_t)pointer);
        }
    }
    ++g_verified_repentance;
    log_line("bridge_observation=%s secondary_binding_warnings=%s",
             g_bridge_hook_observed ? "expected_bridge_hook_observed" :
             "IsaacUpdate_not_hooked",
             g_unexpected_bindings ? "yes" : "no");
    return true;
}

static bool log_repentance_windows(u64 base) {
    static const struct {
        const char *name;
        u64 offset;
        u8 expected[16];
    } windows[] = {
        { "Manager::Update", 0x3F8DB8, {0xFF,0x43,0x01,0xD1,0xFD,0x7B,0x01,0xA9,0xFD,0x43,0x00,0x91,0xF7,0x13,0x00,0xF9} },
        { "Game::Update", 0x351884, {0xFF,0xC3,0x07,0xD1,0xFD,0x7B,0x19,0xA9,0xFD,0x43,0x06,0x91,0xFC,0xD3,0x00,0xF9} },
        { "Room::Update", 0x46D788, {0xED,0x33,0xB7,0x6D,0xEB,0x2B,0x01,0x6D,0xE9,0x23,0x02,0x6D,0xFD,0x7B,0x03,0xA9} },
        { "EntityList::Update", 0x767C0, {0xFF,0x03,0x03,0xD1,0xEC,0x1B,0x00,0xFD,0xEB,0x2B,0x04,0x6D,0xE9,0x23,0x05,0x6D} },
    };
    log_line("verified_repentance_header_base=0x%016" PRIX64, (uint64_t)base);
    for (size_t i = 0; i < sizeof(windows) / sizeof(windows[0]); ++i) {
        if (base > UINT64_MAX - windows[i].offset)
            return fail_message("Repentance code address overflow.");
        const u64 address = base + windows[i].offset;
        MemoryInfo mapping = {0};
        Result rc = cheat_query(&mapping, address);
        if (R_FAILED(rc)) return fail_result("query Repentance code window", rc);
        if ((mapping.perm & Perm_Rx) != Perm_Rx || mapping.addr > address ||
            address - mapping.addr > mapping.size ||
            sizeof(windows[i].expected) > mapping.size - (address - mapping.addr))
            return fail_message("Repentance code window is not wholly readable/executable.");
        u8 bytes[16] = {0};
        rc = cheat_read(address, bytes, sizeof(bytes));
        if (R_FAILED(rc)) return fail_result("read Repentance code window", rc);
        const bool match = memcmp(bytes, windows[i].expected, sizeof(bytes)) == 0;
        log_line("code_window=%s address=0x%016" PRIX64 " signature=%s",
                 windows[i].name, (uint64_t)address, match ? "MATCH" : "MISMATCH");
        log_header(bytes, sizeof(bytes));
        if (!match) return fail_message("A Repentance code window differs.");
    }
    return true;
}

static void log_loader_got(const CheatProcessMetadata *metadata) {
    const u64 offset = 0x10CC78;
    if (metadata->main_nso.base > UINT64_MAX - offset ||
        metadata->main_nso.size < offset + sizeof(u64)) {
        log_line("run_repentance_GOT=unavailable (outside verified main module)");
        return;
    }
    const u64 address = metadata->main_nso.base + offset;
    MemoryInfo mapping = {0};
    Result rc = cheat_query(&mapping, address);
    if (R_FAILED(rc) || !(mapping.perm & Perm_R) || mapping.addr > address ||
        address - mapping.addr > mapping.size || sizeof(u64) > mapping.size - (address - mapping.addr)) {
        log_line("run_repentance_GOT=unavailable (query/bounds/permissions) result=0x%08" PRIX32, (uint32_t)rc);
        return;
    }
    u64 pointer = 0;
    rc = cheat_read(address, &pointer, sizeof(pointer));
    if (R_FAILED(rc)) {
        log_line("run_repentance_GOT=unavailable (read) result=0x%08" PRIX32, (uint32_t)rc);
        return;
    }
    log_line("run_repentance_GOT_address=0x%016" PRIX64 " pointer=0x%016" PRIX64,
             (uint64_t)address, (uint64_t)pointer);
    g_loader_run_pointer = pointer;
    if (pointer >= 0x4C0840)
        log_line("run_repentance_base_candidate=0x%016" PRIX64 " (pointer minus 0x4C0840; unverified)",
                 (uint64_t)(pointer - 0x4C0840));
}

static bool snapshot_mappings(size_t *readable_regions, size_t *nro_regions) {
    u64 count = 0;
    Result rc = cheat_mapping_count(&count);
    if (R_FAILED(rc)) return fail_result("count application mappings", rc);
    log_line("mapped_region_count=%" PRIu64, (uint64_t)count);
    if (!count || count > MAX_MAPPINGS)
        return fail_message("Application mapping count is outside the diagnostic limit.");
    u64 offset = 0;
    while (offset < count) {
        MemoryInfo mappings[MAPPING_BATCH] = {0};
        u64 received = 0;
        const size_t capacity = count - offset < MAPPING_BATCH ? (size_t)(count - offset) : MAPPING_BATCH;
        rc = cheat_mappings(mappings, capacity, offset, &received);
        if (R_FAILED(rc)) return fail_result("list application mappings", rc);
        if (!received || received > capacity)
            return fail_message("Mapping list changed or returned an invalid batch.");
        for (u64 i = 0; i < received; ++i) {
            const MemoryInfo *mapping = &mappings[i];
            if (!(mapping->perm & Perm_X)) continue;
            log_line("executable_mapping[%" PRIu64 "] base=0x%016" PRIX64
                     " size=0x%016" PRIX64 " type=0x%08" PRIX32 " perm=0x%08" PRIX32,
                     (uint64_t)(offset + i), (uint64_t)mapping->addr, (uint64_t)mapping->size,
                     (uint32_t)mapping->type, (uint32_t)mapping->perm);
            if (!(mapping->perm & Perm_R)) {
                log_line("  header=skipped (no read permission)");
                continue;
            }
            if (!mapping->size || mapping->addr > UINT64_MAX - mapping->size)
                return fail_message("Invalid executable mapping bounds.");
            MemoryInfo current = {0};
            rc = cheat_query(&current, mapping->addr);
            if (R_FAILED(rc)) return fail_result("recheck executable mapping", rc);
            if (current.addr != mapping->addr || current.size != mapping->size || current.perm != mapping->perm)
                return fail_message("Executable mapping changed during observation.");
            const size_t bytes = mapping->size < HEADER_BYTES ? (size_t)mapping->size : HEADER_BYTES;
            u8 header[HEADER_BYTES] = {0};
            rc = cheat_read(mapping->addr, header, bytes);
            if (R_FAILED(rc)) return fail_result("read bounded executable header", rc);
            ++*readable_regions;
            log_line("  header_bytes=%zu", bytes);
            log_header(header, bytes);
            if (bytes >= 0x60 && memcmp(header + 0x10, "NRO0", 4) == 0) {
                ++*nro_regions;
                log_build_id("  nro_build_id", header + 0x40);
                if (memcmp(header + 0x40, k_repentance_build_id, 0x20) == 0 &&
                    (!log_repentance_windows(mapping->addr) || !verify_bridge_contract(mapping->addr))) return false;
            } else {
                log_line("  NRO0_magic=not observed; this does not exclude a loaded NRO");
            }
        }
        offset += received;
    }
    u64 after_count = 0;
    rc = cheat_mapping_count(&after_count);
    if (R_FAILED(rc)) return fail_result("recheck mapping count", rc);
    if (after_count != count) return fail_message("Mapping count changed during observation.");
    return true;
}

static bool take_snapshot(void) {
    u64 pid = 0;
    LoaderModuleInfo loader = {0};
    if (!current_application(&pid) || !verify_loader_build(pid, &loader)) return false;
    Result rc = smGetService(&g_cheat_service, "dmnt:cht");
    if (R_FAILED(rc)) return fail_result("open Atmosphere dmnt:cht", rc);
    g_cheat_open = true;
    bool attached = false;
    rc = cheat_has_process(&attached);
    if (R_FAILED(rc)) return fail_result("check debugger attachment", rc);
    log_line("preexisting_attachment=%s", attached ? "yes" : "no");
    if (!attached) {
        if (!exact_cheat_file_absent()) return false;
        rc = cheat_force_open();
        if (R_FAILED(rc)) return fail_result("attach to the verified application", rc);
        log_line("attachment_created=yes; retained until normal process exit");
    }
    CheatProcessMetadata before = {0}, after = {0};
    rc = cheat_metadata(&before);
    if (R_FAILED(rc)) return fail_result("read initial process metadata", rc);
    if (!verify_metadata(&before, pid, &loader)) return false;
    log_build_id("verified_main_nso_build_id", before.main_nso_build_id);
    log_loader_got(&before);
    load_bridge_marker();
    size_t readable_regions = 0, nro_regions = 0;
    const bool snapshot_ok = snapshot_mappings(&readable_regions, &nro_regions);
    // Inspect callback state even if a later, unrelated observation failed.
    report_bridge_stage();
    if (!snapshot_ok) return false;
    rc = cheat_metadata(&after);
    if (R_FAILED(rc)) return fail_result("read final process metadata", rc);
    if (!verify_metadata(&after, pid, &loader)) return false;
    u64 final_pid = 0;
    rc = pmdmntGetApplicationProcessId(&final_pid);
    if (R_FAILED(rc)) return fail_result("recheck running application", rc);
    if (final_pid != pid) return fail_message("Application changed during observation.");
    if (g_verified_repentance != 1) return fail_message("Expected exactly one verified Repentance module.");
    log_line("snapshot_complete=yes readable_executable_regions=%zu NRO0_regions=%zu",
             readable_regions, nro_regions);
    printf("Snapshot complete: %zu executable headers, %zu NRO headers.\n", readable_regions, nro_regions);
    return true;
}

static bool prepare_log(void) {
    const char *directories[] = { "sdmc:/switch", "sdmc:/switch/TheModdingOfIsaac" };
    for (size_t i = 0; i < sizeof(directories) / sizeof(directories[0]); ++i) {
        if (mkdir(directories[i], 0777) != 0 && errno != EEXIST) {
            printf("Cannot create log directory (errno %d).\n", errno);
            return false;
        }
    }
    g_log = fopen(LOG_PATH, "wb");
    if (!g_log) {
        printf("Cannot open log (errno %d).\n", errno);
        return false;
    }
    log_line("IsaacModuleSnapshot 0.3.1 - callback witness");
    log_line("observation=bounded executable headers; game memory is never written");
    return true;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    consoleInit(NULL);
    printf("The Modding of Isaac - Module Snapshot\n\n");
    printf("Keep Isaac running; use the Homebrew route that worked for you.\n");
    bool ok = prepare_log();
    if (ok) ok = take_snapshot();
    if (g_log) {
        log_line("result=%s", !ok ? "stopped; partial diagnostics only" :
                 (g_unexpected_bindings ? "observation_complete_with_binding_warnings" : "success"));
        const bool write_failed = ferror(g_log) != 0;
        const int close_result = fclose(g_log);
        g_log = NULL;
        if (write_failed || close_result != 0) {
            printf("Log write failed; snapshot file may be incomplete.\n");
            ok = false;
        } else {
            printf("\nLog: /switch/TheModdingOfIsaac/module-snapshot.log\n");
        }
    }
    if (g_cheat_open) serviceClose(&g_cheat_service);
    if (g_ldr_open) ldrDmntExit();
    if (g_pm_info_open) pminfoExit();
    if (g_pm_dmnt_open) pmdmntExit();
    printf("\n%s\nPress PLUS to exit.\n", ok ? "Ready to return to the game." : "Diagnostic stopped; see the message above.");
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return ok ? 0 : 1;
}
