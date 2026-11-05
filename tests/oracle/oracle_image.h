// Shared by the differential oracles: maps the owner's XBE at its original addresses, and proves function entries are
// reached. Analysis tooling only; see docs/verification.md.
#pragma once
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

// The executable is linked at 0x00010000 with a large executable section, so nearly the whole address range of the XBE
// belongs to this image from process creation, before any stack or heap exists (CMakeLists.txt). The harness's own
// code occupies the start of that range, so original code below the section is unavailable: the code under test starts
// at 0x00075000, and map_image refuses to run if the section does not cover what is needed.
static const uint32_t kNeedLow = 0x00070000, kNeedHigh = 0x003B4000;
#pragma section(".aorig", read, execute)
__declspec(allocate(".aorig")) static unsigned char g_low[0x003A0000] = {1};

static bool map_image(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) { std::printf("cannot open %s\n", path); return false; }
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> d(n);
    if (std::fread(d.data(), 1, n, f) != (size_t)n) { std::fclose(f); return false; }
    std::fclose(f);
    if (n < 0x178 || std::memcmp(d.data(), "XBEH", 4) != 0) { std::printf("not an XBE\n"); return false; }
    const uint32_t base = *(uint32_t*)&d[0x104], nsec = *(uint32_t*)&d[0x11C], sec = *(uint32_t*)&d[0x120] - base;
    if (sec + (uint64_t)nsec * 0x38 > (uint64_t)n) { std::printf("bad section table\n"); return false; }
    const uint32_t low = (uint32_t)(uintptr_t)g_low, high = low + (uint32_t)sizeof g_low;
    if (low > kNeedLow || high < kNeedHigh) {
        std::printf("harness link error: section covers 0x%08X-0x%08X\n", low, high);
        return false;
    }
    std::memset(g_low, 0, sizeof g_low);
    for (uint32_t i = 0; i < nsec; i++) {
        const uint8_t* h = &d[sec + i * 0x38];
        const uint32_t va = *(uint32_t*)(h + 4), raw = *(uint32_t*)(h + 12), rsize = *(uint32_t*)(h + 16);
        if ((uint64_t)raw + rsize > (uint64_t)n) { std::printf("bad section %u\n", i); return false; }
        const uint32_t from = va > low ? va : low, to = va + rsize < high ? va + rsize : high;
        if (from < to) std::memcpy((void*)(uintptr_t)from, &d[raw + (from - va)], to - from);
    }
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    return true;
}

// ---- entry coverage --------------------------------------------------------------------------------------------------
struct Entry { uint32_t va; uint8_t saved; bool hit; };
static std::vector<Entry> g_entries;

static LONG CALLBACK on_breakpoint(EXCEPTION_POINTERS* e) {
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT) return EXCEPTION_CONTINUE_SEARCH;
    const uint32_t at_va = (uint32_t)(uintptr_t)e->ExceptionRecord->ExceptionAddress;
    for (Entry& x : g_entries) {
        if (x.va == at_va && !x.hit) {
            x.hit = true;
            *(uint8_t*)(uintptr_t)x.va = x.saved;
            FlushInstructionCache(GetCurrentProcess(), (void*)(uintptr_t)x.va, 1);
            e->ContextRecord->Eip = at_va;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static bool arm_entries(const char* path) {
    FILE* f = std::fopen(path, "r");
    if (!f) { std::printf("cannot open %s\n", path); return false; }
    unsigned va;
    while (std::fscanf(f, "%x", &va) == 1) {
        bool duplicate = false;  // Two symbols can share one address (identical-code folding).
        for (const Entry& x : g_entries) duplicate = duplicate || x.va == va;
        if (duplicate) continue;
        g_entries.push_back({va, *(uint8_t*)(uintptr_t)va, false});
        *(uint8_t*)(uintptr_t)va = 0xCC;
    }
    std::fclose(f);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    AddVectoredExceptionHandler(1, on_breakpoint);
    return true;
}

static int report_entries() {
    int reached = 0;
    for (const Entry& x : g_entries) {
        if (x.hit) reached++;
        else std::printf("entry not reached: 0x%08X\n", x.va);
    }
    if (!g_entries.empty()) std::printf("entries reached: %d / %d\n", reached, (int)g_entries.size());
    return reached;
}
