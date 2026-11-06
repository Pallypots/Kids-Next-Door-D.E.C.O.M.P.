// Host test of the recovered allocator, on whatever pointer size this build has. The differential oracle only runs on x86, and a block's header is larger on a 64-bit host,
// so this checks what must hold anywhere: no overlap, data kept, alignment, and everything coming back. These tests are not evidence of a byte match.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

#include "at/AtHeap.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

struct Live {
    unsigned char* p;
    unsigned int size;
    unsigned char fill;
    int heap;
};

void Fuzz(unsigned int seed) {
    std::mt19937 g(seed);
    const unsigned int total = 0x100000;
    void* const region = std::malloc(total + 0x40);
    AtHeapManager manager;
    manager.Init(region, total, 0);
    std::vector<Live> live;
    for (int i = 0; i < AtHeapManager::kHeaps / 2; i++) {
        manager.CreateHeap(0x4000 + g() % 0x8000, 2 + (int)(g() % 5));
    }
    manager.MakeEquivalent(&manager.m_heaps[0], &manager.m_heaps[1]);
    for (int op = 0; op < 20000; op++) {
        if (g() % 100 < 55) {
            const int h = (int)(g() % (AtHeapManager::kHeaps / 2));
            const unsigned int size = g() % 8 == 0 ? g() % 4000 : g() % 200;
            const unsigned int shift = g() % 3 == 0 ? 2 + g() % 7 : 0;
            unsigned char* p;
            if (shift) {
                p = (unsigned char*)manager.m_heaps[h].AllocateAligned(size, shift);
                if (p) CHECK(((uintptr_t)p & ((1u << shift) - 1)) == 0);
            } else {
                p = (unsigned char*)manager.Allocate(&manager.m_heaps[h], size);
            }
            if (p) {
                const Live l = {p, size, (unsigned char)g(), h};
                for (const Live& other : live) CHECK(l.p + l.size <= other.p || other.p + other.size <= l.p || l.size == 0 || other.size == 0);
                std::memset(p, l.fill, size);
                live.push_back(l);
            }
        } else if (!live.empty()) {
            const size_t i = g() % live.size();
            const Live l = live[i];
            for (unsigned int k = 0; k < l.size; k++) CHECK(l.p[k] == l.fill);
            CHECK(manager.Free(l.p) == 1);
            live.erase(live.begin() + (std::ptrdiff_t)i);
        }
    }
    while (!live.empty()) {
        CHECK(manager.Free(live.back().p) == 1);
        live.pop_back();
    }
    // Everything back: one free block in each heap, the size of the heap.
    for (int i = 0; i < AtHeapManager::kHeaps; i++) {
        const AtHeap& h = manager.m_heaps[i];
        if (h.m_size) {
            CHECK(h.m_used == 0);
            CHECK(h.FreeBlockCount() == 1);
            CHECK(h.LargestFree() == h.m_size);
        }
    }
    char report[4096];
    manager.Report(report, sizeof report);
    CHECK(std::strstr(report, "Heap 0:") != nullptr);
    for (int i = 0; i < AtHeapManager::kHeaps; i++) {
        if (manager.m_heaps[i].m_size) manager.DestroyHeap(&manager.m_heaps[i]);
    }
    CHECK(manager.m_chunks != nullptr && manager.m_chunks->m_next == nullptr);
    std::free(region);
}

void Refusals() {
    void* const region = std::malloc(0x10000);
    AtHeapManager manager;
    manager.Init(region, 0x10000 - 0x40, 0);
    AtHeap* const h = manager.CreateHeap(0x1000, 2);
    CHECK(h->Allocate(0x100000) == nullptr);
    CHECK(manager.Allocate(h, 0x100000) == nullptr);
    char outside[64];
    CHECK(manager.Free(outside + 16) == 0);
    void* const p = h->Allocate(16);
    CHECK(((uintptr_t)p % sizeof(void*)) == 0);
    CHECK(h->Owns(p) == 1);
    manager.m_override = h;
    AtHeap* const other = manager.CreateHeap(0x1000, 2);
    void* const q = manager.Allocate(other, 16);
    CHECK(h->Owns(q) == 1);
    CHECK(other->Owns(q) == 0);
    manager.m_override = nullptr;
    std::free(region);
}

}  // namespace

int main() {
    Refusals();
    for (unsigned int seed = 1; seed <= 8; seed++) Fuzz(seed);
    std::printf(g_fail ? "FAILED: %d\n" : "ok\n", g_fail);
    return g_fail ? 1 : 0;
}
