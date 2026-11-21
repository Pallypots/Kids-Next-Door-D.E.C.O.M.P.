// Differential oracle for the AtAp layers' start-ups below the atlas layer, App's configs for them, and the event, network and I/O layers' objects, the I/O
// context's Init and read buffer, the factory and the memory globals (subsystems gm/atap and at/atap; Windows x86, local only). See docs/verification.md.
//
//   oracle_atap <default.xbe> [--entries FILE] [--mutate]
//
// No game file is read. What stands in for what, first: the start-ups are run with App's vftable and every layer object's vftable replaced by tables of
// doubles, the same on both sides, so the chain is checked call by call (which slot, on which object, with which arguments and what config) but no layer's
// Init runs there. The objects' own Inits are checked apart, each on an object made by the side's own constructor, with the slots they call virtually
// (Shutdown, SetHeaps, the factory's) again tables whose entries either log or run that side's own code for the slot. What runs for real on both sides:
// everything recorded for these subsystems, and the recovered members they call from other subsystems (the namespace groups and databases, the name
// maps, the heap setters of at/jam, at/text and at/memory, AtMemoryGlobals's Save and Restore, AtHeapStack), each the original's on the original side.
// The original's heap stack, new, delete and fatal paths are doubles (tests/oracle/oracle_doubles.h). The factory's device and file systems are doubles:
// the Xbox's are not recovered.
#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtApLayer.h"
#include "at/AtEvent.h"
#include "at/AtIoJam.h"
#include "at/AtIoTokenizer.h"
#include "at/AtText.h"
#include "gm/GmApp.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 160).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 25) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 160).c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#define AtPushHeap AtPushHeapInDoubles
#include "oracle_doubles.h"
#undef AtPushHeap

// Requests fail while the heap last pushed is this one, on either side: every request here follows a push of its heap.
static void* g_fail_heap;
void AtPushHeap(void* h) { g_fail_alloc = g_fail_heap != nullptr && h == g_fail_heap; AtPushHeapInDoubles(h); }
static void __cdecl s_push(void* h) { g_fail_alloc = g_fail_heap != nullptr && h == g_fail_heap; o_push(h); }

static uint32_t& orig_u32(uint32_t va) { return *(uint32_t*)(uintptr_t)va; }
static void*& orig_ptr(uint32_t va) { return *(void**)(uintptr_t)va; }

// A deterministic generator, so a run can be repeated.
static uint32_t g_rng = 12345;
static uint32_t rnd() { g_rng = g_rng * 1103515245u + 12345u; return g_rng >> 8; }

// ---- where pointers point: each side's objects, as offsets both sides share ---------------------------------------------
struct Region { const unsigned char* base; uint32_t size; uint32_t tag; };
static std::vector<Region> g_regions[2];
static void add_region(int side, const void* base, uint32_t size, uint32_t tag) { g_regions[side].push_back({(const unsigned char*)base, size, tag}); }
static const void* g_factory[2];  // the factory each side's code reaches
static uint32_t rel_on(int side, const void* p) {
    if (p == nullptr) return 0;
    if (p == g_factory[side]) return 0x7F000000u;
    for (const Region& r : g_regions[side]) {
        if ((const unsigned char*)p >= r.base && (const unsigned char*)p < r.base + r.size) return r.tag + (uint32_t)((const unsigned char*)p - r.base);
    }
    return (uint32_t)(uintptr_t)p;
}
static uint32_t rel(const void* p) { return rel_on(g_side, p); }
static uint32_t hash(const void* p, int words) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < words; i++) h = (h ^ ((const uint32_t*)p)[i]) * 16777619u;
    return h;
}
static int g_bad;
static void L(uint32_t tag, uint32_t v = 0) {
    if (tag == 'BAD') g_bad++;
    if (g_recording) g_log[g_side].add(tag, v);
}

// What the recovered code linked here needs of the platform; no case reaches it.
void* AtPlatformAllocateRegion(unsigned int) { L('BAD', 9800); return nullptr; }
void AtPlatformFreeRegion(void*) { L('BAD', 9801); }
void AtPlatformInitDevices() { L('BAD', 9802); }
void AtLibraryFree(void*) { L('BAD', 9803); }
int AtPlatformLanguage() { L('BAD', 9804); return 1; }
int AtPlatformGameRegion() { L('BAD', 9805); return 1; }
unsigned int AtPlatformMilliseconds() { L('BAD', 9806); return 0; }
// The heap stack's query, so that src/at/AtMemory.cpp (whose push and pop are doubled) is not linked. The doubles never push, so the stack stays
// empty, as the original's own, which its real code reads, does.
AtHeap* AtCurrentHeap() { return nullptr; }

// Tokens: addresses both sides hand out alike.
static unsigned char g_tokens[16][64];
#define TOKEN(i) ((void*)g_tokens[i])

// ---- tables of doubles --------------------------------------------------------------------------------------------------
template <int T, int S> static void __fastcall d_bad(void*, void*) { L('BAD', T * 100 + S); }
template <int T, int S> struct BadFill {
    static void run(const void** t) { t[S] = (const void*)&d_bad<T, S>; BadFill<T, S - 1>::run(t); }
};
template <int T> struct BadFill<T, -1> {
    static void run(const void**) {}
};

// App's slots.
static void __fastcall d_gmp(void* self, void*, uint32_t* p) {
    L('gmp', rel(self)); L('gmpz', hash(p, 3));
    p[0] = 0x1000; p[1] = 0x20; p[2] = 0x300;
}
static void __fastcall d_create_heaps(void* self, void*) { L('crh', rel(self)); }
static void __fastcall d_shutdown(void* self, void*) { L('shd', rel(self)); }
template <int Slot, int Words> static void __fastcall d_cfg(void* self, void*, uint32_t* c) {
    L('cfg', Slot); L('cfgs', rel(self)); L('cfgz', hash(c, Words));  // the caller cleared it
    for (int i = 0; i < Words; i++) c[i] = (Slot << 16) | i;
}
template <int Slot> static void __fastcall d_after(void* self, void*) { L('aft', Slot); L('afts', rel(self)); }
static const void* g_app_table[69];

// The layers' objects: slot 1 is Init, with two or three arguments.
template <int Words> static void __fastcall d_init2(void* self, void*, uint32_t* c, void* below) {
    L('ini2', rel(self)); L('inib', rel(below)); L('inic', hash(c, Words));
}
template <int Words> static void __fastcall d_init3(void* self, void*, uint32_t* c, void* below, void* memory) {
    L('ini3', rel(self)); L('inib', rel(below)); L('inim', rel(memory)); L('inic', hash(c, Words));
}
static void __fastcall d_memory_startup(void* self, void*, uint32_t* p) { L('mst', rel(self)); L('msth', hash(p, 3)); }
static const void* g_memory_table[4];
static const void* g_layer_table[14][2];

struct Layer { uint32_t offset; uint32_t flag; int table; };
// The objects at their offsets in App, from the base up, each with the offset of its running flag.
static const Layer kLayers[] = {
    {0x268, 0x2EC, 0}, {0x2F0, 0x308, 1}, {0x310, 0x314, 2}, {0x384, 0x388, 3}, {0x390, 0x410, 4}, {0x414, 0x418, 5}, {0x5F70, 0x5F78, 6},
    {0x5F7C, 0x5F88, 7}, {0x6020, 0x60A4, 8}, {0x60B0, 0x60B4, 9}, {0x60CC, 0x60D0, 10}, {0x60DC, 0x60E0, 11}, {0x6358, 0x644C, 12},
};

// The objects' own slots, for the Inits run for real: each logs, and SetHeaps runs the side's own.
static void __fastcall d_obj_shutdown(void* self, void*) { L('osd', rel(self)); }
static void __fastcall d_event_set_heaps(void* self, void* edx, const AtEventConfig* c) {
    L('esh', rel(self));
    if (g_side == 0) at<void(__fastcall*)(void*, void*, const AtEventConfig*)>(0x000B3C90)(self, edx, c);
    else ((AtEventSystem*)self)->AtEventSystem::SetHeaps(c);
}
static const void* g_event_table[4];
static const void* g_network_table[4];
static void __fastcall d_io_set_heaps(void* self, void* edx, const AtIoConfig* c) {
    L('ish', rel(self));
    if (g_side == 0) at<void(__fastcall*)(void*, void*, const AtIoConfig*)>(0x000B3A40)(self, edx, c);
    else ((AtIoSystem*)self)->AtIoSystem::SetHeaps(c);
}
static const void* g_io_table[5];

// The factory: its device is a token; Shutdown, BlockSize and MinimumBlocks are the side's own.
static void* __fastcall d_create_device(void* self, void*, int requests) { L('cdv', rel(self)); L('cdvr', requests); return TOKEN(1); }
static void __fastcall d_destroy_device(void* self, void*, void* device) { L('ddv', rel(self)); L('ddvd', rel(device)); }
static void __fastcall d_factory_shutdown(void* self, void* edx) {
    L('fsd', rel(self));
    if (g_side == 0) at<void(__fastcall*)(void*, void*)>(0x00102EF0)(self, edx);
    else ((AtIoSystemFactory*)self)->AtIoSystemFactory::Shutdown();
}
static unsigned __fastcall d_block_size(void* self, void* edx) {
    L('fbs', rel(self));
    if (g_side == 0) return at<unsigned(__fastcall*)(void*, void*)>(0x00102EB0)(self, edx);
    return ((AtIoSystemFactory*)self)->AtIoSystemFactory::BlockSize();
}
static unsigned __fastcall d_min_blocks(void* self, void* edx) {
    L('fmb', rel(self));
    if (g_side == 0) return at<unsigned(__fastcall*)(void*, void*)>(0x0010F4B0)(self, edx);
    return ((AtIoSystemFactory*)self)->AtIoSystemFactory::MinimumBlocks();
}
static const void* g_factory_table[9];
// A factory of the harness's for the read buffer alone, with sizes a case chooses.
static unsigned g_fake_block = 0x8000, g_fake_min = 4;
static unsigned __fastcall d_fake_block(void* self, void*) { L('kbs', rel(self)); return g_fake_block; }
static unsigned __fastcall d_fake_min(void* self, void*) { L('kmb', rel(self)); return g_fake_min; }
static const void* g_fake_factory_table[9];
static struct { const void* vftable; int running; } g_fake_factory;

// ---- the globals both sides write ----------------------------------------------------------------------------------------
struct Global { const char* name; uint32_t va; void** rec; };
static Global g_globals[] = {
    {"AtIoJamFileSystem heap", 0x00301B54, &AtIoJamFileSystem::s_heap},
    {"AtIoJamFileSystem header heap", 0x00301B50, &AtIoJamFileSystem::s_headerHeap},
    {"AtIoContext heap", 0x002C6290, &AtIoContext::s_heap},
    {"buffer heap", 0x002C6340, &AtIoBufferPool::s_heap},
    {"AtTextTable heap", 0x002C62F8, &AtTextTable::s_heap},
    {"AtIoFastJamHeader heap", 0x00301B60, &AtIoFastJamHeader::s_heap},
    {"0x00301B64", 0x00301B64, &g_heap301B64},
    {"0x00301B70", 0x00301B70, &g_heap00301B70},
    {"AtIoFastAlignedJamHeader heap", 0x00301B5C, &AtIoFastAlignedJamHeader::s_heap},
    {"AtIoStream heap", 0x002C63CC, &AtIoStream::s_heap},
    {"GmNameMap heap", 0x002C632C, &GmNameMap::s_heap},
    {"GmNameSpaceDb heap", 0x002C6274, &GmNameSpaceDb::s_heap},
    {"0x003B23EC", 0x003B23EC, &g_heap3B23EC},
    {"0x00301B6C", 0x00301B6C, &g_heap301B6C},
    {"0x003ADF9C", 0x003ADF9C, &AtEventDatabase::s_heap},
    {"0x00305918", 0x00305918, &AtEventSystem::s_heap305918},
    {"0x003ADF98", 0x003ADF98, &AtEventList::s_heap},
    {"0x003ADF94", 0x003ADF94, &AtEventSystem::s_heap3ADF94},
    {"0x003ADF90", 0x003ADF90, &AtEventSystem::s_heap3ADF90},
    {"0x003ADF8C", 0x003ADF8C, &AtEventSystem::s_heap3ADF8C},
    {"AtHeapManager instance", 0x002C60AC, (void**)&AtHeapManager::s_instance},
    {"AtIoTokenFile heap", 0x002C60B0, &AtIoTokenFile::s_heap},
    {"fatal handler", 0x002B48E4, &AtMemoryGlobals::s_fatal},
};
static Global g_context_globals[] = {
    {"AtIoFileRef context", 0x002C6278, (void**)&AtIoFileRef::s_context},
    {"context 0x002C6328", 0x002C6328, (void**)&AtIoContext::s_context2C6328},
    {"AtIoContext instance", 0x002C62F0, (void**)&AtIoContext::s_instance},
    {"context 0x002C633C", 0x002C633C, (void**)&AtIoContext::s_context2C633C},
};
static void set_globals(uint32_t seed) {
    for (Global& g : g_globals) {
        uint32_t r = rnd();
        void* v = (seed & 1) ? nullptr : (r % 3 == 0 ? nullptr : TOKEN(8 + r % 4));
        orig_ptr(g.va) = v;
        *g.rec = v;
    }
    for (Global& g : g_context_globals) {
        orig_ptr(g.va) = TOKEN(12);
        *g.rec = TOKEN(12);
    }
}
static void compare_globals() {
    for (Global& g : g_globals) eq(g.name, rel_on(0, orig_ptr(g.va)), rel_on(1, *g.rec));
    for (Global& g : g_context_globals) eq(g.name, rel_on(0, orig_ptr(g.va)), rel_on(1, *g.rec));
}

template <class O, class R> static bool run_pair(const char* what, O orig, R rec) {
    g_context = what;
    bool c[2];
    { On on(0); c[0] = guarded(orig); }
    { On on(1); c[1] = guarded(rec); }
    eq("crashed", c[0], c[1]);
    compare_logs(what);
    if (c[0] || c[1]) std::printf("crashed in [%s]: original %d, recovered %d\n", what, (int)c[0], (int)c[1]);
    return c[0] || c[1];
}

// ---- the start-ups ---------------------------------------------------------------------------------------------------------
static const unsigned kAppSize = 0x6D4C;
static unsigned char* g_apps[2];
static int g_chains;

static void prepare_apps(uint32_t flags_seed) {
    for (int side = 0; side < 2; side++) {
        unsigned char* b = g_apps[side];
        std::memset(b, 0, kAppSize);
        *(const void**)b = g_app_table;
        *(const void**)(b + 4) = g_memory_table;
        for (const Layer& l : kLayers) *(const void**)(b + l.offset) = g_layer_table[l.table];
    }
    // Running flags: each clear, set, or set with only other bits.
    uint32_t r = flags_seed;
    auto pick = [&]() -> uint32_t { r = r * 1103515245u + 12345u; const uint32_t k = (r >> 16) % 7; return k < 4 ? 0 : k == 4 ? 1 : k == 5 ? 2 : 0x80000001u; };
    const uint32_t memory = flags_seed == 0 ? 0 : pick();
    for (int side = 0; side < 2; side++) *(uint32_t*)(g_apps[side] + 8) = memory;
    for (const Layer& l : kLayers) {
        const uint32_t v = flags_seed == 0 ? 0 : pick();
        for (int side = 0; side < 2; side++) *(uint32_t*)(g_apps[side] + l.flag) = v;
    }
}

typedef void(__fastcall* Fn0)(void*, void*);
struct StartUp { const char* name; uint32_t va; void (App::*rec)(); };
static const StartUp kStartUps[] = {
    {"StartUpUserInterface", 0x0009C3C0, &App::StartUpUserInterface}, {"StartUpScene", 0x0009C510, &App::StartUpScene},
    {"StartUpVideo", 0x0009C680, &App::StartUpVideo}, {"StartUpSound", 0x0009C7D0, &App::StartUpSound},
    {"StartUpController", 0x0009C940, &App::StartUpController}, {"StartUpGraphics", 0x0009CAB0, &App::StartUpGraphics},
    {"StartUpKinematics", 0x0009CC10, &App::StartUpKinematics}, {"StartUpScript", 0x0009CD50, &App::StartUpScript},
    {"StartUpDataManagement", 0x0009CEB0, &App::StartUpDataManagement}, {"StartUpMath", 0x0009CFF0, &App::StartUpMath},
    {"StartUpIo", 0x0009D140, &App::StartUpIo}, {"StartUpNetwork", 0x0009D2A0, &App::StartUpNetwork},
    {"StartUpEvent", 0x0009D3D0, &App::StartUpEvent}, {"StartUpBase", 0x0009D4F0, &App::StartUpBase},
};

static void run_chain(const StartUp& s, uint32_t seed) {
    prepare_apps(seed);
    char what[96];
    sprintf_s(what, "%s flags %08X", s.name, seed);
    run_pair(what, [&] { at<Fn0>(s.va)(g_apps[0], nullptr); }, [&] { (((App*)g_apps[1])->*s.rec)(); });
    eq("App after", std::memcmp(g_apps[0], g_apps[1], kAppSize), 0);
    g_chains++;
}

static void run_start_ups() {
    for (const StartUp& s : kStartUps) run_chain(s, 0);
    // Each layer's flag alone, set two ways.
    for (const Layer& l : kLayers) {
        for (uint32_t v : {1u, 2u}) {
            prepare_apps(0);
            for (int side = 0; side < 2; side++) *(uint32_t*)(g_apps[side] + l.flag) = v;
            g_context = "flag at " + std::to_string(l.flag) + " = " + std::to_string(v);
            run_pair(g_context.c_str(), [&] { at<Fn0>(0x0009C3C0)(g_apps[0], nullptr); }, [&] { ((App*)g_apps[1])->StartUpUserInterface(); });
            g_chains++;
        }
    }
    for (uint32_t v : {1u, 2u}) {
        prepare_apps(0);
        for (int side = 0; side < 2; side++) *(uint32_t*)(g_apps[side] + 8) = v;
        run_pair("the memory system's flag", [&] { at<Fn0>(0x0009C3C0)(g_apps[0], nullptr); }, [&] { ((App*)g_apps[1])->StartUpUserInterface(); });
        g_chains++;
    }
    for (int i = 0; i < 40; i++) run_chain(kStartUps[rnd() % 14], 1 + rnd());
}

// ---- App's configs ---------------------------------------------------------------------------------------------------------
typedef void(__fastcall* Fn1)(void*, void*, void*);
static void run_configs() {
    struct Slot { const char* name; uint32_t va; int words; void (*rec)(App*, void*); };
    static const Slot slots[] = {
        {"GetIoConfig", 0x0008C650, 17, [](App* a, void* c) { a->App::GetIoConfig((AtIoConfig*)c); }},
        {"GetMathConfig", 0x0008C6B0, 4, [](App* a, void* c) { a->App::GetMathConfig((AppMathConfig*)c); }},
        {"GetDataManagementConfig", 0x0008C6E0, 13, [](App* a, void* c) { a->App::GetDataManagementConfig((AppDataManagementConfig*)c); }},
        {"GetScriptConfig", 0x0008C800, 5, [](App* a, void* c) { a->App::GetScriptConfig((AppScriptConfig*)c); }},
        {"GetKinematicsConfig", 0x0008C710, 13, [](App* a, void* c) { a->App::GetKinematicsConfig((AppKinematicsConfig*)c); }},
        {"GetGraphicsConfig", 0x0008C740, 60, [](App* a, void* c) { a->App::GetGraphicsConfig((AppGraphicsConfig*)c); }},
        {"GetControllerConfig", 0x0008C770, 12, [](App* a, void* c) { a->App::GetControllerConfig((AppControllerConfig*)c); }},
        {"GetSoundConfig", 0x0008C7A0, 12, [](App* a, void* c) { a->App::GetSoundConfig((AppSoundConfig*)c); }},
        {"GetSceneConfig", 0x0008C7D0, 35, [](App* a, void* c) { a->App::GetSceneConfig((AppSceneConfig*)c); }},
    };
    static uint32_t cfg[2][64];
    for (const Slot& s : slots) {
        for (int pass = 0; pass < 3; pass++) {
            prepare_apps(0);
            for (int side = 0; side < 2; side++) {
                for (int i = 0; i < 4; i++) *(void**)(g_apps[side] + 0x6494 + 4 * i) = pass == 2 && i == 1 ? nullptr : TOKEN(4 + i);
                for (int i = 0; i < 64; i++) cfg[side][i] = 0x33330000u + (uint32_t)i * (pass + 1);
            }
            run_pair(s.name, [&] { at<Fn1>(s.va)(g_apps[0], nullptr, cfg[0]); }, [&] { s.rec((App*)g_apps[1], cfg[1]); });
            for (int i = 0; i < 64; i++) eq(s.name, cfg[0][i], cfg[1][i]);  // the words past the config too: nothing writes there
        }
    }
}

// ---- the memory globals ----------------------------------------------------------------------------------------------------
static void run_globals() {
    static unsigned char buf[2][32];
    struct Ctor { const char* name; uint32_t va; void (*rec)(void*); };
    static const Ctor ctors[] = {
        {"AtMemoryGlobals()", 0x001038B0, [](void* p) { new (p) AtMemoryGlobals(); }},
        {"AtApGlobals()", 0x000C93A0, [](void* p) { new (p) AtApGlobals(); }},
        {"AtIoGlobals()", 0x000C9200, [](void* p) { new (p) AtIoGlobals(); }},
    };
    for (const Ctor& c : ctors) {
        std::memset(buf, 0x77, sizeof buf);
        run_pair(c.name, [&] { at<Fn0>(c.va)(buf[0], nullptr); }, [&] { c.rec(buf[1]); });
        eq(c.name, std::memcmp(buf[0], buf[1], 32), 0);
    }
    struct Op { const char* name; uint32_t va; void (*rec)(void*); };
    static const Op ops[] = {
        {"AtApGlobals::Save", 0x000C93B0, [](void* p) { ((AtApGlobals*)p)->Save(); }},
        {"AtApGlobals::Restore", 0x000C93C0, [](void* p) { ((AtApGlobals*)p)->Restore(); }},
        {"AtIoGlobals::Save", 0x000C9220, [](void* p) { ((AtIoGlobals*)p)->Save(); }},
        {"AtIoGlobals::Restore", 0x000C9230, [](void* p) { ((AtIoGlobals*)p)->Restore(); }},
    };
    for (const Op& o : ops) {
        for (int pass = 0; pass < 3; pass++) {
            set_globals(pass);
            for (int side = 0; side < 2; side++) {
                std::memset(buf[side], 0x55, 32);
                ((void**)buf[side])[0] = pass == 1 ? nullptr : TOKEN(2);
                ((void**)buf[side])[1] = pass == 2 ? nullptr : TOKEN(3);
                ((void**)buf[side])[2] = TOKEN(4);
            }
            run_pair(o.name, [&] { at<Fn0>(o.va)(buf[0], nullptr); }, [&] { o.rec(buf[1]); });
            eq(o.name, std::memcmp(buf[0], buf[1], 32), 0);
            compare_globals();
        }
    }
}

// ---- the setters -----------------------------------------------------------------------------------------------------------
typedef void(__cdecl* Cfn1)(void*);
static void run_setters() {
    struct S { uint32_t va; void (*rec)(void*); };
    static const S setters[] = {
        {0x000C9650, AtEventDatabase::SetHeap}, {0x000991E0, AtSetHeap305918}, {0x000C9460, AtEventList::SetHeap}, {0x000C93F0, AtSetHeap3ADF94},
        {0x000C93E0, AtSetHeap3ADF90}, {0x000C93D0, AtSetHeap3ADF8C}, {0x00024B60, GmNameMap::SetHeap}, {0x00019180, AtIoContext::SetHeap},
        {0x00025C10, AtIoBufferPool::SetHeap}, {0x0001F550, AtTextTable::SetHeap}, {0x00080CC0, AtSetHeap301B64}, {0x00103050, AtSetHeap3B23EC},
        {0x00081760, AtSetHeap301B6C},
    };
    for (const S& s : setters) {
        set_globals(1);
        run_pair("setter", [&] { at<Cfn1>(s.va)(TOKEN(9)); }, [&] { s.rec(TOKEN(9)); });
        compare_globals();
    }
    // The setters of whole configs, over configs with each heap present or not, and the three conditional heaps set before or not.
    static uint32_t cfg[2][17];
    struct C { const char* name; uint32_t va; int kind; int words; };
    static const C sets[] = {
        {"AtEventSystem::SetHeaps", 0x000B3C90, 0, 12}, {"AtIoSystemBase::SetHeaps", 0x000C9040, 1, 17}, {"AtIoSystem::SetHeaps", 0x000B3A40, 2, 17},
        {"AtIoSetDeviceHeaps", 0x000C9280, 3, 17}, {"AtIoSetFactoryHeaps", 0x00102F40, 4, 17},
    };
    static unsigned char self[2][0x90];
    for (const C& c : sets) {
        for (int pass = 0; pass < 24; pass++) {
            set_globals(pass < 2 ? pass : rnd() & 1);
            for (int i = 0; i < c.words; i++) {
                const uint32_t r = rnd();
                void* v = (pass == 0 || r % 2 == 0) ? nullptr : TOKEN(1 + r % 7);
                if (i == (c.kind == 0 ? 2 : 5)) v = TOKEN(15);  // the default heap
                cfg[0][i] = cfg[1][i] = (uint32_t)(uintptr_t)v;
            }
            run_pair(c.name,
                     [&] {
                         if (c.kind <= 2) at<Fn1>(c.va)(self[0], nullptr, cfg[0]);
                         else at<Cfn1>(c.va)(cfg[0]);
                     },
                     [&] {
                         if (c.kind == 0) ((AtEventSystem*)self[1])->AtEventSystem::SetHeaps((AtEventConfig*)cfg[1]);
                         else if (c.kind == 1) ((AtIoSystemBase*)self[1])->AtIoSystemBase::SetHeaps((AtIoConfig*)cfg[1]);
                         else if (c.kind == 2) ((AtIoSystem*)self[1])->AtIoSystem::SetHeaps((AtIoConfig*)cfg[1]);
                         else if (c.kind == 3) AtIoSetDeviceHeaps((AtIoConfig*)cfg[1]);
                         else AtIoSetFactoryHeaps((AtIoConfig*)cfg[1]);
                     });
            compare_globals();
        }
    }
}

// ---- the event and network layers ----------------------------------------------------------------------------------------
static unsigned char g_obj[2][0x100];
static void compare_group(const char* what, const GmNameMapGroup& o, const GmNameMapGroup& r) {
    eq(what, std::strncmp(o.m_name, r.m_name, 8), 0);  // past the terminator, the original copies its stack: not compared
    eq(what, o.m_used, r.m_used);
    eq(what, o.m_first != nullptr, r.m_first != nullptr);
}
static void compare_db(const char* what, const GmNameSpaceDb& o, const GmNameSpaceDb& r) {
    eq(what, o.m_poolSize, r.m_poolSize);
    eq(what, o.m_pool != nullptr, r.m_pool != nullptr);
    eq(what, o.m_size, r.m_size);
    eq(what, o.m_free == o.m_pool, r.m_free == r.m_pool);
}

typedef void(__fastcall* Fn2)(void*, void*, void*, void*);
typedef void(__fastcall* Fn3)(void*, void*, void*, void*, void*);
static void run_event() {
    static uint32_t cfg[2][12];
    for (int pass = 0; pass < 12; pass++) {
        set_globals(pass & 1);
        std::memset(g_obj, 0xEE, sizeof g_obj);
        run_pair("AtEventSystem()", [&] { at<Fn0>(0x000B3BD0)(g_obj[0], nullptr); }, [&] { new (g_obj[1]) AtEventSystem(); });
        AtEventSystem* o = (AtEventSystem*)g_obj[0];
        AtEventSystem* r = (AtEventSystem*)g_obj[1];
        eq("ctor below", rel_on(0, o->m_below), rel_on(1, r->m_below));
        eq("ctor +4", *(uint32_t*)(g_obj[0] + 8), *(uint32_t*)(g_obj[1] + 8));  // m_unknown04 of the names: left
        compare_group("ctor group", o->m_names.m_group, r->m_names.m_group);
        compare_db("ctor spaces", o->m_names.m_spaces, r->m_names.m_spaces);
        for (int side = 0; side < 2; side++) *(const void**)g_obj[side] = g_event_table;
        if (pass >= 8) for (int side = 0; side < 2; side++) ((AtEventSystem*)g_obj[side])->m_below = TOKEN(6);  // running: Shutdown first
        for (int i = 0; i < 12; i++) cfg[0][i] = cfg[1][i] = (rnd() & 1) ? (uint32_t)(uintptr_t)TOKEN(1 + i % 7) : 0;
        cfg[0][0] = cfg[1][0] = (pass % 4 == 0) ? 0 : 1 + rnd() % 9;
        cfg[0][1] = cfg[1][1] = (pass % 3 == 0) ? 0 : 1 + rnd() % 9;
        cfg[0][2] = cfg[1][2] = (uint32_t)(uintptr_t)TOKEN(15);
        run_pair("AtEventSystem::Init", [&] { at<Fn2>(0x000B3C50)(g_obj[0], nullptr, cfg[0], TOKEN(5)); },
                 [&] { ((AtEventSystem*)g_obj[1])->AtEventSystem::Init((AtEventConfig*)cfg[1], TOKEN(5)); });
        eq("below", rel_on(0, o->m_below), rel_on(1, r->m_below));
        compare_group("group", o->m_names.m_group, r->m_names.m_group);
        compare_group("group 2", o->m_names.m_group2, r->m_names.m_group2);
        compare_db("spaces", o->m_names.m_spaces, r->m_names.m_spaces);
        compare_db("spaces 2", o->m_names.m_spaces2, r->m_names.m_spaces2);
        compare_globals();
    }
}

static void run_network() {
    static uint32_t cfg[2][27];
    for (int pass = 0; pass < 8; pass++) {
        std::memset(g_obj, 0xEE, sizeof g_obj);
        const bool base = pass & 1;
        if (base) run_pair("AtNetworkSystemBase()", [&] { at<Fn0>(0x000C92E0)(g_obj[0], nullptr); }, [&] { new (g_obj[1]) AtNetworkSystemBase(); });
        else run_pair("AtNetworkSystem()", [&] { at<Fn0>(0x000B3AE0)(g_obj[0], nullptr); }, [&] { new (g_obj[1]) AtNetworkSystem(); });
        eq("network constructed", std::memcmp(g_obj[0] + 4, g_obj[1] + 4, 0x1C), 0);
        for (int side = 0; side < 2; side++) *(const void**)g_obj[side] = g_network_table;
        if (pass >= 4) for (int side = 0; side < 2; side++) ((AtNetworkSystemBase*)g_obj[side])->m_below = TOKEN(6);
        for (int i = 0; i < 27; i++) cfg[0][i] = cfg[1][i] = rnd();
        if (pass == 2) cfg[0][0] = cfg[1][0] = 0;
        if (base) run_pair("AtNetworkSystemBase::Init", [&] { at<Fn2>(0x000C9300)(g_obj[0], nullptr, cfg[0], TOKEN(5)); },
                           [&] { ((AtNetworkSystemBase*)g_obj[1])->AtNetworkSystemBase::Init((AtNetworkConfig*)cfg[1], TOKEN(5)); });
        else run_pair("AtNetworkSystem::Init", [&] { at<Fn2>(0x000B3B00)(g_obj[0], nullptr, cfg[0], TOKEN(5)); },
                      [&] { ((AtNetworkSystem*)g_obj[1])->AtNetworkSystem::Init((AtNetworkConfig*)cfg[1], TOKEN(5)); });
        // The settings are random words, compared as they are; only m_below (+0x18) is a pointer.
        for (int i = 1; i < 8; i++) {
            if (i == 6) eq("network below", rel_on(0, ((void**)g_obj[0])[i]), rel_on(1, ((void**)g_obj[1])[i]));
            else eq("network", ((uint32_t*)g_obj[0])[i], ((uint32_t*)g_obj[1])[i]);
        }
        eq("network config", std::memcmp(cfg[0], cfg[1], sizeof cfg[0]), 0);
    }
}

// ---- the factory, the read buffer and the context ------------------------------------------------------------------------
static void set_factories(int running) {
    orig_ptr(0x003ADF84) = (void*)g_factory_table;
    orig_u32(0x003ADF88) = (uint32_t)running;
    *(const void**)&AtIoSystemFactory::s_instance = g_factory_table;
    AtIoSystemFactory::s_instance.m_running = running;
}
static void compare_factories(const char* what) { eq(what, orig_u32(0x003ADF88), (uint32_t)AtIoSystemFactory::s_instance.m_running); }

static void run_factory() {
    static unsigned char buf[2][8];
    std::memset(buf, 0x66, sizeof buf);
    run_pair("AtIoSystemFactory()", [&] { at<Fn0>(0x00102EC0)(buf[0], nullptr); }, [&] { new (buf[1]) AtIoSystemFactory(); });
    eq("factory constructed", *(uint32_t*)(buf[0] + 4), *(uint32_t*)(buf[1] + 4));
    for (int running : {0, 1, 7}) {
        set_factories(running);
        char params = 0;
        run_pair("AtIoSystemFactory::Init", [&] { at<Fn1>(0x00102ED0)((void*)(uintptr_t)0x003ADF84, nullptr, &params); },
                 [&] { AtIoSystemFactory::s_instance.AtIoSystemFactory::Init(&params); });
        compare_factories("Init");
        set_factories(running);
        run_pair("AtIoSystemFactory::Shutdown", [&] { at<Fn0>(0x00102EF0)((void*)(uintptr_t)0x003ADF84, nullptr); },
                 [&] { AtIoSystemFactory::s_instance.AtIoSystemFactory::Shutdown(); });
        compare_factories("Shutdown");
        unsigned v[2];
        run_pair("BlockSize", [&] { v[0] = at<unsigned(__fastcall*)(void*, void*)>(0x00102EB0)((void*)(uintptr_t)0x003ADF84, nullptr); },
                 [&] { v[1] = AtIoSystemFactory::s_instance.AtIoSystemFactory::BlockSize(); });
        eq("BlockSize", v[0], v[1]);
        run_pair("MinimumBlocks", [&] { v[0] = at<unsigned(__fastcall*)(void*, void*)>(0x0010F4B0)((void*)(uintptr_t)0x003ADF84, nullptr); },
                 [&] { v[1] = AtIoSystemFactory::s_instance.AtIoSystemFactory::MinimumBlocks(); });
        eq("MinimumBlocks", v[0], v[1]);
        // How the layer reaches it: the saved globals put back, the factory started, the globals saved again.
        set_factories(running);
        set_globals(running == 7);
        static unsigned char saved[2][16];
        for (int side = 0; side < 2; side++) {
            std::memset(saved[side], 0, 16);
            ((void**)saved[side])[0] = TOKEN(2);
            ((void**)saved[side])[1] = running == 1 ? nullptr : TOKEN(3);
            ((void**)saved[side])[2] = TOKEN(4);
        }
        void* got[2];
        run_pair("AtIoGetFactory", [&] { got[0] = at<void*(__cdecl*)(void*, void*)>(0x000C9240)(saved[0], TOKEN(7)); },
                 [&] { got[1] = AtIoGetFactory((AtIoGlobals*)saved[1], (AtIoConfig*)TOKEN(7)); });
        eq("AtIoGetFactory", rel_on(0, got[0]), rel_on(1, got[1]));
        eq("saved", std::memcmp(saved[0], saved[1], 16), 0);
        compare_factories("AtIoGetFactory");
        compare_globals();
    }
}

static void compare_pool(const char* what, const AtIoBufferPool& o, const AtIoBufferPool& r) {
    eq(what, o.m_size, r.m_size);
    eq(what, o.m_blockSize, r.m_blockSize);
    eq(what, o.m_count, r.m_count);
    eq(what, o.m_data != nullptr, r.m_data != nullptr);
    eq(what, o.m_blocks != nullptr, r.m_blocks != nullptr);
    if (o.m_blocks && r.m_blocks && o.m_count == r.m_count) {
        eq(what, ((const uint32_t*)o.m_blocks)[-1], ((const uint32_t*)r.m_blocks)[-1]);
        for (unsigned i = 0; i < o.m_count; i++) {
            const AtIoBuffer& a = o.m_blocks[i];
            const AtIoBuffer& b = r.m_blocks[i];
            eq(what, (uint32_t)(a.m_data - o.m_data), (uint32_t)(b.m_data - r.m_data));
            eq(what, a.m_size, b.m_size);
            eq(what, *(const uint32_t*)&a.m_flags, *(const uint32_t*)&b.m_flags);
            eq(what, a.m_unknown00, b.m_unknown00);
            eq(what, a.m_unknown0C, b.m_unknown0C);
            eq(what, a.m_unknown14, b.m_unknown14);
        }
    }
}

typedef void(__fastcall* FnPool)(void*, void*, void*, unsigned);
static void run_buffers() {
    static unsigned char pool[2][0x14];
    const unsigned sizes[][2] = {{0x8000, 4}, {0x8000, 4}, {0x1000, 1}, {0x10, 8}, {0x8000, 0}, {0x2000, 3}};
    const unsigned bytes[] = {0, 1, 0x7FFF, 0x8000, 0x1FFFF, 0x20000, 0x20001, 0x40000, 0x47FFF, 0x100000};
    for (int s = 0; s < 6; s++) {
        for (unsigned b : bytes) {
            g_fake_block = sizes[s][0];
            g_fake_min = sizes[s][1];
            for (int again = 0; again < 2; again++) {
                if (!again) std::memset(pool, 0, sizeof pool);
                void* heap = (s == 1 && b == 1) ? nullptr : TOKEN(10);
                orig_ptr(0x002C6340) = heap;
                AtIoBufferPool::s_heap = heap;
                run_pair("AtIoBufferPool::Init", [&] { at<FnPool>(0x00025CE0)(pool[0], nullptr, &g_fake_factory, b); },
                         [&] { ((AtIoBufferPool*)pool[1])->Init((AtIoFileSystemFactory*)&g_fake_factory, b); });
                compare_pool("pool", *(AtIoBufferPool*)pool[0], *(AtIoBufferPool*)pool[1]);
            }
            run_pair("AtIoBufferPool::Clear", [&] { at<Fn0>(0x00025C20)(pool[0], nullptr); }, [&] { ((AtIoBufferPool*)pool[1])->Clear(); });
            eq("cleared", std::memcmp(pool[0], pool[1], 0x14), 0);
        }
    }
    // Out of memory: each request fails. The original raises and goes on to write the blocks through null: a crash on both sides, after the raises.
    for (unsigned b : {0x10000u, 0x40000u}) {
        std::memset(pool, 0, sizeof pool);
        orig_ptr(0x002C6340) = TOKEN(10);
        AtIoBufferPool::s_heap = TOKEN(10);
        g_fake_block = 0x8000;
        g_fake_min = 4;
        g_fail_heap = TOKEN(10);
        run_pair("AtIoBufferPool::Init out of memory", [&] { at<FnPool>(0x00025CE0)(pool[0], nullptr, &g_fake_factory, b); },
                 [&] { ((AtIoBufferPool*)pool[1])->Init((AtIoFileSystemFactory*)&g_fake_factory, b); });
        g_fail_heap = nullptr;
        g_fail_alloc = false;
    }
    std::memset(pool, 0, sizeof pool);
    run_pair("AtIoBufferPool::Clear empty",[&] { at<Fn0>(0x00025C20)(pool[0], nullptr); }, [&] { ((AtIoBufferPool*)pool[1])->Clear(); });
    static unsigned char block[2][0x18];
    for (int pass = 0; pass < 64; pass++) {
        for (int i = 0; i < 0x18; i++) block[0][i] = block[1][i] = (unsigned char)rnd();
        if (pass < 4) block[0][8] = block[1][8] = (unsigned char)(pass == 0 ? 0 : pass == 1 ? 4 : pass == 2 ? 0xFB : 0x04 | 0x80);
        const unsigned n = rnd();
        run_pair("AtIoBuffer::Init", [&] { at<void(__fastcall*)(void*, void*, void*, unsigned)>(0x00032160)(block[0], nullptr, TOKEN(3), n); },
                 [&] { ((AtIoBuffer*)block[1])->Init((unsigned char*)TOKEN(3), n); });
        eq("block", std::memcmp(block[0], block[1], 0x18), 0);
    }
}

static unsigned char g_ctx[2][0x80];
static void compare_context(const char* what, const AtIoContext& o, const AtIoContext& r) {
    eq(what, rel_on(0, o.m_device), rel_on(1, r.m_device));
    eq(what, rel_on(0, o.m_unknown04), rel_on(1, r.m_unknown04));
    eq(what, o.m_stackCount, r.m_stackCount);
    eq(what, o.m_stackSize, r.m_stackSize);
    eq(what, o.m_stack != nullptr, r.m_stack != nullptr);
    if (o.m_stack && r.m_stack && o.m_stackSize == r.m_stackSize && o.m_stackSize < 64)
        for (unsigned i = 0; i < o.m_stackSize; i++) eq(what, rel_on(0, o.m_stack[i]), rel_on(1, r.m_stack[i]));
    compare_group(what, o.m_defaultGroup, r.m_defaultGroup);
    compare_db(what, o.m_spaces, r.m_spaces);
    compare_pool(what, o.m_buffers, r.m_buffers);
}

typedef void(__fastcall* FnCtx)(void*, void*, void*, unsigned, unsigned, unsigned, unsigned);
static void run_context() {
    for (int pass = 0; pass < 30; pass++) {
        std::memset(g_ctx, 0xDD, sizeof g_ctx);
        run_pair("AtIoContext()", [&] { at<Fn0>(0x000191C0)(g_ctx[0], nullptr); }, [&] { new (g_ctx[1]) AtIoContext(); });
        AtIoContext* o = (AtIoContext*)g_ctx[0];
        AtIoContext* r = (AtIoContext*)g_ctx[1];
        compare_context("constructed", *o, *r);
        set_globals(1);
        for (int k = 0; k < (pass < 10 ? 1 : 2); k++) {
            const unsigned stack = pass == 0 ? 0 : 1 + rnd() % 6, spaces = 1 + rnd() % 9, requests = (pass % 3 == 0) ? 0 : rnd() % 20;
            const unsigned bytes = (rnd() % 3 == 0) ? rnd() % 0x10000 : rnd() % 0x80000;
            void* heap = (pass == 5) ? nullptr : TOKEN(11);
            orig_ptr(0x002C6290) = heap;
            AtIoContext::s_heap = heap;
            orig_ptr(0x002C6340) = TOKEN(10);
            AtIoBufferPool::s_heap = TOKEN(10);
            orig_ptr(0x002C6274) = TOKEN(9);
            GmNameSpaceDb::s_heap = TOKEN(9);
            orig_ptr(0x002C632C) = TOKEN(9);
            GmNameMap::s_heap = TOKEN(9);
            void* factory = (pass & 1) ? (void*)&g_fake_factory : nullptr;
            void* f[2] = {factory ? factory : (void*)(uintptr_t)0x003ADF84, factory ? factory : (void*)&AtIoSystemFactory::s_instance};
            set_factories(1);
            g_fake_block = 0x1000 << (rnd() % 4);
            g_fake_min = 1 + rnd() % 5;
            run_pair("AtIoContext::Init", [&] { at<FnCtx>(0x00019440)(g_ctx[0], nullptr, f[0], stack, bytes, spaces, requests); },
                     [&] { r->Init((AtIoFileSystemFactory*)f[1], stack, bytes, spaces, requests); });
            compare_context("Init", *o, *r);
            compare_globals();
        }
        if (pass % 4 != 3) {
            if (pass % 4 == 1) for (int side = 0; side < 2; side++) ((AtIoContext*)g_ctx[side])->m_device = TOKEN(14);
            for (int side = 0; side < 2; side++) ((AtIoContext*)g_ctx[side])->m_stackCount = (unsigned)pass;  // Clear clears it; Init leaves it
            run_pair("AtIoContext::Clear", [&] { at<Fn0>(0x00019220)(g_ctx[0], nullptr); }, [&] { r->Clear(); });
            compare_context("Clear", *o, *r);
            compare_globals();
        }
    }
}

// Out of memory in the context's Init: the stack request fails and the original raises, then clears the stack through null (a crash on both sides), or,
// with no stack, goes on to the read buffer, whose requests fail too.
static void run_context_out_of_memory() {
    for (unsigned stack : {0u, 3u}) {
        std::memset(g_ctx, 0, sizeof g_ctx);
        run_pair("AtIoContext() again", [&] { at<Fn0>(0x000191C0)(g_ctx[0], nullptr); }, [&] { new (g_ctx[1]) AtIoContext(); });
        set_globals(1);
        orig_ptr(0x002C6290) = TOKEN(11);
        AtIoContext::s_heap = TOKEN(11);
        orig_ptr(0x002C6340) = TOKEN(10);
        AtIoBufferPool::s_heap = TOKEN(10);
        orig_ptr(0x002C6274) = TOKEN(9);
        GmNameSpaceDb::s_heap = TOKEN(9);
        orig_ptr(0x002C632C) = TOKEN(9);
        GmNameMap::s_heap = TOKEN(9);
        g_fake_block = 0x8000;
        g_fake_min = 4;
        g_fail_heap = TOKEN(11);
        run_pair("AtIoContext::Init out of memory", [&] { at<FnCtx>(0x00019440)(g_ctx[0], nullptr, &g_fake_factory, stack, 0x10000, 1, 0); },
                 [&] { ((AtIoContext*)g_ctx[1])->Init((AtIoFileSystemFactory*)&g_fake_factory, stack, 0x10000, 1, 0); });
        g_fail_heap = nullptr;
        g_fail_alloc = false;
    }
}

static void run_io() {
    static uint32_t cfg[2][17];
    for (int pass = 0; pass < 40; pass++) {
        std::memset(g_obj, 0xEE, sizeof g_obj);
        const bool base = pass % 5 == 4;
        if (base) run_pair("AtIoSystemBase()", [&] { at<Fn0>(0x000C8FC0)(g_obj[0], nullptr); }, [&] { new (g_obj[1]) AtIoSystemBase(); });
        else run_pair("AtIoSystem()", [&] { at<Fn0>(0x000B3940)(g_obj[0], nullptr); }, [&] { new (g_obj[1]) AtIoSystem(); });
        AtIoSystemBase* o = (AtIoSystemBase*)g_obj[0];
        AtIoSystemBase* r = (AtIoSystemBase*)g_obj[1];
        eq("io below", rel_on(0, o->m_below), rel_on(1, r->m_below));
        eq("io factory", rel_on(0, o->m_factory), rel_on(1, r->m_factory));
        eq("io kind", o->m_kind, r->m_kind);
        compare_context("io constructed", o->m_context, r->m_context);
        for (int side = 0; side < 2; side++) *(const void**)g_obj[side] = g_io_table;
        if (pass >= 30) for (int side = 0; side < 2; side++) ((AtIoSystemBase*)g_obj[side])->m_below = TOKEN(6);
        set_globals(pass < 10 ? 1 : rnd() & 1);
        set_factories(pass % 3 == 0 ? 1 : 0);
        for (int i = 0; i < 17; i++) cfg[0][i] = cfg[1][i] = (rnd() & 1) ? (uint32_t)(uintptr_t)TOKEN(1 + rnd() % 7) : 0;
        cfg[0][0] = cfg[1][0] = pass % 4 == 0 ? 0 : pass % 4 == 1 ? 1 : pass % 4 == 2 ? 2 : 5;
        cfg[0][1] = cfg[1][1] = pass % 6 == 0 ? 0 : 1 + rnd() % 5;
        cfg[0][2] = cfg[1][2] = pass % 7 == 0 ? 0 : 1 + rnd() % 12;
        cfg[0][3] = cfg[1][3] = rnd() % 0x50000;
        cfg[0][4] = cfg[1][4] = pass % 5 == 0 ? 0 : rnd() % 32;
        cfg[0][5] = cfg[1][5] = (uint32_t)(uintptr_t)TOKEN(15);
        if (base) run_pair("AtIoSystemBase::Init", [&] { at<Fn3>(0x000C8FF0)(g_obj[0], nullptr, cfg[0], TOKEN(5), TOKEN(4)); },
                           [&] { r->AtIoSystemBase::Init((AtIoConfig*)cfg[1], TOKEN(5), TOKEN(4)); });
        else run_pair("AtIoSystem::Init", [&] { at<Fn3>(0x000B3960)(g_obj[0], nullptr, cfg[0], TOKEN(5), TOKEN(4)); },
                      [&] { ((AtIoSystem*)r)->AtIoSystem::Init((AtIoConfig*)cfg[1], TOKEN(5), TOKEN(4)); });
        eq("io config", std::memcmp(cfg[0], cfg[1], sizeof cfg[0]), 0);
        eq("io below", rel_on(0, o->m_below), rel_on(1, r->m_below));
        eq("io factory", rel_on(0, o->m_factory), rel_on(1, r->m_factory));
        eq("io kind", o->m_kind, r->m_kind);
        compare_context("io", o->m_context, r->m_context);
        compare_factories("io");
        compare_globals();
        if (!base && cfg[1][2] != 0) {  // a database of no namespaces writes before its pool, on both sides
            AtIoConfig* c = (AtIoConfig*)cfg[1];
            run_pair("AtIoSystemBase::InitContext", [&] { at<Fn2>(0x000C8F90)(g_obj[0], nullptr, (void*)&g_fake_factory, cfg[0]); },
                     [&] { r->InitContext((AtIoSystemFactory*)(void*)&g_fake_factory, c); });
            compare_context("InitContext", o->m_context, r->m_context);
        }
    }
}

// ---- main -------------------------------------------------------------------------------------------------------------------
static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO || c == 0xC0000374) {
        if (guard_resume(e)) return EXCEPTION_CONTINUE_EXECUTION;
        char text[300];
        const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p during [%.200s]\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, g_context.c_str());
        DWORD ignored;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, (DWORD)len, &ignored, nullptr);
        ExitProcess(3);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char** argv) {
    AddVectoredExceptionHandler(0, on_crash);
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* entries = nullptr;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 1) { std::printf("usage: oracle_atap <default.xbe> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    at<void(__cdecl*)()>(0x00125BC7)();

    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)s_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    if (entries && !arm_entries(entries)) return 2;

    // Tables.
    BadFill<0, 68>::run(g_app_table);
    g_app_table[0] = (const void*)d_gmp;
    g_app_table[1] = (const void*)d_create_heaps;
    g_app_table[6] = (const void*)d_shutdown;
    g_app_table[7] = (const void*)&d_cfg<7, 12>;
    g_app_table[11] = (const void*)&d_cfg<11, 27>;
    g_app_table[15] = (const void*)&d_cfg<15, 17>;
    g_app_table[19] = (const void*)&d_cfg<19, 4>;
    g_app_table[23] = (const void*)&d_cfg<23, 13>;
    g_app_table[27] = (const void*)&d_cfg<27, 5>;
    g_app_table[31] = (const void*)&d_cfg<31, 13>;
    g_app_table[35] = (const void*)&d_cfg<35, 60>;
    g_app_table[40] = (const void*)&d_cfg<40, 12>;
    g_app_table[44] = (const void*)&d_cfg<44, 12>;
    g_app_table[48] = (const void*)&d_cfg<48, 5>;
    g_app_table[52] = (const void*)&d_cfg<52, 35>;
    g_app_table[56] = (const void*)&d_cfg<56, 26>;
    g_app_table[8] = (const void*)&d_after<8>;
    g_app_table[12] = (const void*)&d_after<12>;
    g_app_table[16] = (const void*)&d_after<16>;
    g_app_table[20] = (const void*)&d_after<20>;
    g_app_table[24] = (const void*)&d_after<24>;
    g_app_table[28] = (const void*)&d_after<28>;
    g_app_table[32] = (const void*)&d_after<32>;
    g_app_table[36] = (const void*)&d_after<36>;
    g_app_table[41] = (const void*)&d_after<41>;
    g_app_table[45] = (const void*)&d_after<45>;
    g_app_table[49] = (const void*)&d_after<49>;
    g_app_table[53] = (const void*)&d_after<53>;
    g_app_table[57] = (const void*)&d_after<57>;
    BadFill<1, 3>::run(g_memory_table);
    g_memory_table[2] = (const void*)d_memory_startup;
    static const void* bad2[2];
    BadFill<2, 1>::run(bad2);
    for (int i = 0; i < 14; i++) g_layer_table[i][0] = bad2[0];
    g_layer_table[0][1] = (const void*)&d_init2<12>;  // events
    g_layer_table[1][1] = (const void*)&d_init2<27>;  // network
    g_layer_table[2][1] = (const void*)&d_init3<17>;  // I/O
    g_layer_table[3][1] = (const void*)&d_init3<4>;   // math
    g_layer_table[4][1] = (const void*)&d_init3<13>;  // data management
    g_layer_table[5][1] = (const void*)&d_init3<5>;   // script
    g_layer_table[6][1] = (const void*)&d_init3<13>;  // kinematics
    g_layer_table[7][1] = (const void*)&d_init3<60>;  // graphics
    g_layer_table[8][1] = (const void*)&d_init3<12>;  // controller
    g_layer_table[9][1] = (const void*)&d_init2<12>;  // sound
    g_layer_table[10][1] = (const void*)&d_init3<5>;  // video
    g_layer_table[11][1] = (const void*)&d_init3<35>; // scene
    g_layer_table[12][1] = (const void*)&d_init2<26>; // user interface
    BadFill<3, 3>::run(g_event_table);
    g_event_table[2] = (const void*)d_obj_shutdown;
    g_event_table[3] = (const void*)d_event_set_heaps;
    BadFill<4, 3>::run(g_network_table);
    g_network_table[2] = (const void*)d_obj_shutdown;
    BadFill<5, 4>::run(g_io_table);
    g_io_table[3] = (const void*)d_obj_shutdown;
    g_io_table[4] = (const void*)d_io_set_heaps;
    BadFill<6, 8>::run(g_factory_table);
    g_factory_table[0] = (const void*)d_create_device;
    g_factory_table[1] = (const void*)d_destroy_device;
    g_factory_table[4] = (const void*)d_factory_shutdown;
    g_factory_table[7] = (const void*)d_block_size;
    g_factory_table[8] = (const void*)d_min_blocks;
    BadFill<7, 8>::run(g_fake_factory_table);
    g_fake_factory_table[1] = (const void*)d_destroy_device;
    g_fake_factory_table[0] = (const void*)d_create_device;
    g_fake_factory_table[7] = (const void*)d_fake_block;
    g_fake_factory_table[8] = (const void*)d_fake_min;
    g_fake_factory.vftable = g_fake_factory_table;
    g_factory[0] = (const void*)(uintptr_t)0x003ADF84;
    g_factory[1] = &AtIoSystemFactory::s_instance;

    g_apps[0] = (unsigned char*)std::calloc(1, kAppSize);
    g_apps[1] = (unsigned char*)std::calloc(1, kAppSize);
    for (int side = 0; side < 2; side++) {
        add_region(side, g_apps[side], kAppSize, 0x70000000u);
        add_region(side, g_obj[side], sizeof g_obj[side], 0x71000000u);
        add_region(side, g_ctx[side], sizeof g_ctx[side], 0x72000000u);
    }
    g_fill = 0xCD;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_start_ups();
    run_configs();
    run_globals();
    run_setters();
    run_event();
    run_network();
    run_factory();
    run_buffers();
    run_context();
    run_context_out_of_memory();
    run_io();
    report_entries();
    if (g_bad) { std::printf("a slot no case should reach was reached %d times\n", g_bad); g_failures++; }
    std::printf("%s: %d start-up chains, %d crashes on both sides; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED",
                g_chains, (int)g_crashes, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
