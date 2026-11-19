// The Xbox SDK's GlobalMemoryStatus (0x00085451), answered from the host. Port code, never recovered code: a bounded adapter, as platform/fatal.cpp
// answers the clock. The SDK's fields are 32 bits wide, so each amount is capped at 0xFFFFFFFF bytes. App's start-up only reports what it is told and
// keeps the total and the overhead (total less available); nothing in the recovered code sizes anything by them.
#include <cstdint>

#include "gm/GmApp.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#endif

namespace {
unsigned int Cap(std::uint64_t bytes) { return bytes > 0xFFFFFFFFull ? 0xFFFFFFFFu : (unsigned int)bytes; }
}

void AtSdkGlobalMemoryStatus(AtSdkMemoryStatus* status) {
    std::uint64_t totalPhysical = 0, availablePhysical = 0, totalPageFile = 0, availablePageFile = 0, totalVirtual = 0, availableVirtual = 0;
    unsigned int load = 0;
#if defined(_WIN32)
    MEMORYSTATUSEX m;
    m.dwLength = sizeof m;
    if (GlobalMemoryStatusEx(&m)) {
        load = m.dwMemoryLoad;
        totalPhysical = m.ullTotalPhys;
        availablePhysical = m.ullAvailPhys;
        totalPageFile = m.ullTotalPageFile;
        availablePageFile = m.ullAvailPageFile;
        totalVirtual = m.ullTotalVirtual;
        availableVirtual = m.ullAvailVirtual;
    }
#elif defined(__linux__)
    struct sysinfo s;
    if (sysinfo(&s) == 0) {
        totalPhysical = (std::uint64_t)s.totalram * s.mem_unit;
        availablePhysical = (std::uint64_t)s.freeram * s.mem_unit;
        totalPageFile = totalPhysical + (std::uint64_t)s.totalswap * s.mem_unit;
        availablePageFile = availablePhysical + (std::uint64_t)s.freeswap * s.mem_unit;
        totalVirtual = totalPageFile;
        availableVirtual = availablePageFile;
        load = totalPhysical ? (unsigned int)(100 - availablePhysical * 100 / totalPhysical) : 0;
    }
#endif
    status->m_length = sizeof(AtSdkMemoryStatus);
    status->m_memoryLoad = load;
    status->m_totalPhysical = Cap(totalPhysical);
    status->m_availablePhysical = Cap(availablePhysical);
    status->m_totalPageFile = Cap(totalPageFile);
    status->m_availablePageFile = Cap(availablePageFile);
    status->m_totalVirtual = Cap(totalVirtual);
    status->m_availableVirtual = Cap(availableVirtual);
}
