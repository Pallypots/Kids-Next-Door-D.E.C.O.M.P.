// Differential oracle for the math, data management, script and kinematics layers' objects (subsystem at/atlayers; Windows x86, local only). See
// docs/verification.md.
//
//   oracle_atlayers <default.xbe> [--entries FILE] [--mutate]
//
// No game file is read. What stands in for what, first: each layer's Init runs on an object made by the side's own constructor, with the object's
// vftable replaced by a table whose Shutdown and SetHeaps slots log and then run that side's own code for the slot, so the virtual calls are checked
// too. What runs for real on both sides: everything recorded for this subsystem, and the recovered members it calls from other subsystems (the
// namespace groups and databases, the registries' groups and namespaces, the name map, the heap setters of at/adr, at/add, at/asb, at/ase, at/actor,
// at/agd, at/asn, at/agn), each the original's on the original side. The original's heap stack, new, delete and fatal paths are doubles
// (tests/oracle/oracle_doubles.h). Not recovered, and doubles on both sides: the script factory's Release (0x0006F640) and the nineteen built-ins, which
// are only registered, never called: their addresses are compared through a table of which double stands for which original address.
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

#include "at/AtAnimationDatabase.h"
#include "at/AtApMidLayers.h"
#include "at/AtGrD3DDatabase.h"
#include "at/AtCoordSys.h"
#include "at/AtDataLibrary.h"
#include "at/AtIoJam.h"
#include "at/AtMatAnimation.h"
#include "at/AtSceneActor.h"
#include "at/AtSegmentQuery.h"
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

// Requests fail while the heap last pushed is this one, on either side.
static void* g_fail_heap;
void AtPushHeap(void* h) { g_fail_alloc = g_fail_heap != nullptr && h == g_fail_heap; AtPushHeapInDoubles(h); }
static void __cdecl s_push(void* h) { g_fail_alloc = g_fail_heap != nullptr && h == g_fail_heap; o_push(h); }

static void*& orig_ptr(uint32_t va) { return *(void**)(uintptr_t)va; }

static uint32_t g_rng = 24680;
static uint32_t rnd() { g_rng = g_rng * 1103515245u + 12345u; return g_rng >> 8; }

// ---- where pointers point -----------------------------------------------------------------------------------------------
struct Region { const unsigned char* base; uint32_t size; uint32_t tag; };
static std::vector<Region> g_regions[2];
static void add_region(int side, const void* base, uint32_t size, uint32_t tag) { g_regions[side].push_back({(const unsigned char*)base, size, tag}); }
static uint32_t rel_on(int side, const void* p) {
    if (p == nullptr) return 0;
    for (const Region& r : g_regions[side]) {
        if ((const unsigned char*)p >= r.base && (const unsigned char*)p < r.base + r.size) return r.tag + (uint32_t)((const unsigned char*)p - r.base);
    }
    return (uint32_t)(uintptr_t)p;
}
static uint32_t rel(const void* p) { return rel_on(g_side, p); }
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
AtHeap* AtCurrentHeap() { return nullptr; }

static unsigned char g_tokens[16][64];
#define TOKEN(i) ((void*)g_tokens[i])

// ---- the seams of at/atlayers, as doubles (src/at/AtApMidLayersSeams.cpp is not linked) ------------------------------------
void AtMathSystem::DeletingDestructor(unsigned int) { L('BAD', 9001); }
void AtDataManagementSystem::DeletingDestructor(unsigned int) { L('BAD', 9002); }
void AtDataRegistryOwner::DeletingDestructor(unsigned int) { L('BAD', 9003); }
void AtScriptSystem::DeletingDestructor(unsigned int) { L('BAD', 9004); }
void AtKinematicsSystemBase::DeletingDestructor(unsigned int) { L('BAD', 9005); }
void AtKinematicsSystem::DeletingDestructor(unsigned int) { L('BAD', 9006); }
void AtGrD3DAnimationDevice::Slot4() { L('BAD', 9014); }
void AtGrD3DAnimationDevice::Slot5() { L('BAD', 9015); }
void AtGrD3DAnimationDevice::Slot6() { L('BAD', 9016); }
// The factory's Release: the original's is redirected to the same log.
void AtSmallCScriptFactory::Release(void* script) { L('rel', rel(this)); L('rels', rel(script)); }
static void __fastcall o_release(void* self, void*, void* script) { L('rel', rel(self)); L('rels', rel(script)); }

#define NATIVE(Name, Va) \
    int Name(void*, AtAmx*, int*) { L('BAD', 9100); return 0; }
NATIVE(AtSmallCNumArgs, 0x0007CA10)
NATIVE(AtSmallCGetArg, 0x0007CA30)
NATIVE(AtSmallCSetArg, 0x0007CA60)
NATIVE(AtSmallCHeapSpace, 0x0007CAB0)
NATIVE(AtSmallCFuncIdx, 0x0007CAC0)
NATIVE(AtSmallCStrLen, 0x0007CC30)
NATIVE(AtSmallCStrPack, 0x0007CC70)
NATIVE(AtSmallCStrUnpack, 0x0007CD10)
NATIVE(AtSmallCToLower, 0x0007CDB0)
NATIVE(AtSmallCToUpper, 0x0007CDD0)
NATIVE(AtSmallCFloat, 0x0007C800)
NATIVE(AtSmallCFloatStr, 0x0007C810)
NATIVE(AtSmallCFloatMul, 0x0007C870)
NATIVE(AtSmallCFloatDiv, 0x0007C8A0)
NATIVE(AtSmallCFloatAdd, 0x0007C8D0)
NATIVE(AtSmallCFloatSub, 0x0007C900)
NATIVE(AtSmallCFloatFract, 0x0007C930)
NATIVE(AtSmallCFloatRound, 0x0007C960)
NATIVE(AtSmallCFloatCmp, 0x0007C9D0)
#undef NATIVE
// Which double stands for which original built-in, so the system's tables compare.
static const struct { AtSmallCNative rec; uint32_t va; } kNatives[] = {
    {AtSmallCNumArgs, 0x0007CA10}, {AtSmallCGetArg, 0x0007CA30}, {AtSmallCSetArg, 0x0007CA60}, {AtSmallCHeapSpace, 0x0007CAB0},
    {AtSmallCFuncIdx, 0x0007CAC0}, {AtSmallCStrLen, 0x0007CC30}, {AtSmallCStrPack, 0x0007CC70}, {AtSmallCStrUnpack, 0x0007CD10},
    {AtSmallCToLower, 0x0007CDB0}, {AtSmallCToUpper, 0x0007CDD0}, {AtSmallCFloat, 0x0007C800}, {AtSmallCFloatStr, 0x0007C810},
    {AtSmallCFloatMul, 0x0007C870}, {AtSmallCFloatDiv, 0x0007C8A0}, {AtSmallCFloatAdd, 0x0007C8D0}, {AtSmallCFloatSub, 0x0007C900},
    {AtSmallCFloatFract, 0x0007C930}, {AtSmallCFloatRound, 0x0007C960}, {AtSmallCFloatCmp, 0x0007C9D0},
};
static uint32_t native_va(int side, AtSmallCNative f) {
    if (f == nullptr) return 0;
    if (side == 0) return (uint32_t)(uintptr_t)f;
    for (const auto& n : kNatives) {
        if (n.rec == f) return n.va;
    }
    return (uint32_t)(uintptr_t)f;  // an original address handed to both sides, or one a long name has written over
}

// ---- tables of doubles --------------------------------------------------------------------------------------------------
template <int T, int S> static void __fastcall d_bad(void*, void*) { L('BAD', T * 100 + S); }
template <int T, int S> struct BadFill {
    static void run(const void** t) { t[S] = (const void*)&d_bad<T, S>; BadFill<T, S - 1>::run(t); }
};
template <int T> struct BadFill<T, -1> {
    static void run(const void**) {}
};

typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, const void*);
typedef void(__fastcall* Fn3)(void*, void*, const void*, void*, void*);
typedef void(__cdecl* Cfn1)(void*);

static void __fastcall d_math_shutdown(void* self, void* edx) {
    L('msd', rel(self));
    if (g_side == 0) at<Fn0>(0x000E7880)(self, edx);
    else ((AtMathSystem*)self)->AtMathSystem::Shutdown();
}
static void __fastcall d_math_set_heaps(void* self, void* edx, const AtMathConfig* c) {
    L('msh', rel(self));
    if (g_side == 0) at<Fn1>(0x000B3870)(self, edx, c);
    else ((AtMathSystem*)self)->AtMathSystem::SetHeaps(c);
}
static const void* g_math_table[4];
static void __fastcall d_dm_shutdown(void* self, void* edx) {
    L('dsd', rel(self));
    if (g_side == 0) at<Fn0>(0x000B37D0)(self, edx);
    else ((AtDataManagementSystem*)self)->AtDataManagementSystem::Shutdown();
}
static void __fastcall d_dm_set_heaps(void* self, void* edx, const AtDataManagementConfig* c) {
    L('dsh', rel(self));
    if (g_side == 0) at<Fn1>(0x000B3690)(self, edx, c);
    else ((AtDataManagementSystem*)self)->AtDataManagementSystem::SetHeaps(c);
}
static const void* g_dm_table[4];
static void __fastcall d_script_shutdown(void* self, void* edx) {
    L('ssd', rel(self));
    if (g_side == 0) at<Fn0>(0x0009EEC0)(self, edx);
    else ((AtScriptSystem*)self)->AtScriptSystem::Shutdown();
}
static const void* g_script_table[3];
static void __fastcall d_kin_shutdown(void* self, void* edx) {
    L('ksd', rel(self));
    if (g_side == 0) at<Fn0>(0x000B35B0)(self, edx);
    else ((AtKinematicsSystem*)self)->AtKinematicsSystem::Shutdown();
}
static void __fastcall d_kin_set_heaps(void* self, void* edx, const AtKinematicsConfig* c) {
    L('ksh', rel(self));
    if (g_side == 0) at<Fn1>(0x000B3580)(self, edx, c);
    else ((AtKinematicsSystem*)self)->AtKinematicsSystem::SetHeaps(c);
}
static const void* g_kin_table[4];
// The base's own slots, for the base's Init run alone.
static void __fastcall d_kinb_shutdown(void* self, void* edx) {
    L('bsd', rel(self));
    if (g_side == 0) at<Fn0>(0x000C8780)(self, edx);
    else ((AtKinematicsSystemBase*)self)->AtKinematicsSystemBase::Shutdown();
}
static void __fastcall d_kinb_set_heaps(void* self, void* edx, const AtKinematicsConfig* c) {
    L('bsh', rel(self));
    if (g_side == 0) at<Fn1>(0x000C8660)(self, edx, c);
    else ((AtKinematicsSystemBase*)self)->AtKinematicsSystemBase::SetHeaps(c);
}
static const void* g_kinb_table[4];

// ---- the globals both sides write ----------------------------------------------------------------------------------------
struct Global { const char* name; uint32_t va; void** rec; };
static Global g_globals[] = {
    {"AtSegmentHitList heap", 0x002C7F58, &AtSegmentHitList::s_heap},
    {"AtQueryPool heap", 0x002C7F64, &AtQueryPool::s_heap},
    {"AtDataComponent heap", 0x002CB09C, &AtDataComponent::s_heap},
    {"AtDataDescriptor heap", 0x002CB0A0, &AtDataDescriptor::s_heap},
    {"AtDatabaseManager heap", 0x003ADE28, &AtDatabaseManager::s_heap},
    {"AtDataLibrary heap", 0x0030593C, &AtDataLibrary::s_heap},
    {"AtRecordLayout heap", 0x003ADE90, &AtRecordLayout::s_heap},
    {"AtDataRecordDatabase heap", 0x003ADE94, &AtDataRecordDatabase::s_heap},
    {"AtDataRegistry heap", 0x002CB094, &AtDataRegistry::s_heap},
    {"AtDataLink heap", 0x002CB0A4, &AtDataLink::s_heap},
    {"0x003ADF80", 0x003ADF80, &g_heap3ADF80},
    {"AtDataRegistryNameSpaces heap", 0x002C9F9C, &AtDataRegistryNameSpaces::s_heap},
    {"AtScriptHost heap", 0x003ADE54, &AtScriptHost::s_heap},
    {"AtSmallCScriptFactory heap", 0x002CD0E8, &AtSmallCScriptFactory::s_heap},
    {"AtSmallCSystem heap", 0x002CF170, &AtSmallCSystem::s_heap},
    {"AtSceneAnimBlend heap", 0x002C63D8, &AtSceneAnimBlend::s_heap},
    {"AtAnimationDatabase heap", 0x002C6374, &AtAnimationDatabase::s_heap},
    {"AtCoordSysDatabase heap", 0x002C6344, &AtCoordSysDatabase::s_heap},
    {"0x002C6348", 0x002C6348, &g_heap2C6348},
    {"AtGrFrameNumberSet heap", 0x002C63E4, &AtGrFrameNumberSet::s_heap},
    {"0x003B23E8", 0x003B23E8, &g_heap3B23E8},
    {"0x003B23E4", 0x003B23E4, &g_heap3B23E4},
    {"0x002C63DC", 0x002C63DC, &g_heap2C63DC},
    {"0x002C634C", 0x002C634C, &g_heap2C634C},
    {"0x003B23E0", 0x003B23E0, &g_heap3B23E0},
    {"AtGrD3DAnimationDevice heap", 0x003ADF7C, &AtGrD3DAnimationDevice::s_heap},
};
static void set_globals(uint32_t seed) {
    for (Global& g : g_globals) {
        uint32_t r = rnd();
        void* v = (seed & 1) ? nullptr : (r % 3 == 0 ? nullptr : TOKEN(8 + r % 4));
        orig_ptr(g.va) = v;
        *g.rec = v;
    }
    orig_ptr(0x002CF174) = TOKEN(12);
    AtSmallCSystem::s_instance = (AtSmallCSystem*)TOKEN(12);
    // The name maps' and namespace databases' heaps must be set, or both sides raise -2: set to a token both sides share.
    orig_ptr(0x002C632C) = TOKEN(13);
    GmNameMap::s_heap = TOKEN(13);
    orig_ptr(0x002C6274) = TOKEN(13);
    GmNameSpaceDb::s_heap = TOKEN(13);
}
static void compare_globals() {
    for (Global& g : g_globals) eq(g.name, rel_on(0, orig_ptr(g.va)), rel_on(1, *g.rec));
    eq("AtSmallCSystem instance", rel_on(0, orig_ptr(0x002CF174)), rel_on(1, AtSmallCSystem::s_instance));
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

// Configs: each heap word a token or 0, with the default heap at `heap` always set; when `word0` the first word random too.
static uint32_t g_cfg[2][16];
static void random_config(int words, int heap, int pass) {
    for (int i = 0; i < words; i++) {
        const uint32_t r = rnd();
        void* v = (pass == 0 || r % 2 == 0) ? nullptr : TOKEN(1 + r % 7);
        g_cfg[0][i] = g_cfg[1][i] = (uint32_t)(uintptr_t)v;
    }
    g_cfg[0][heap] = g_cfg[1][heap] = (uint32_t)(uintptr_t)TOKEN(15);
}

// ---- objects --------------------------------------------------------------------------------------------------------------
static unsigned char g_obj[2][0x5C00];
static const uint32_t kObjTag = 0x71000000u;

static void compare_group(const char* what, const GmNameMapGroup& o, const GmNameMapGroup& r) {
    eq(what, std::strncmp(o.m_name, r.m_name, 8), 0);  // past the terminator the original copies its stack: not compared
    eq(what, o.m_used, r.m_used);
    eq(what, o.m_first != nullptr, r.m_first != nullptr);
}
static void compare_db(const char* what, const GmNameSpaceDb& o, const GmNameSpaceDb& r) {
    eq(what, o.m_poolSize, r.m_poolSize);
    eq(what, o.m_pool != nullptr, r.m_pool != nullptr);
    eq(what, o.m_size, r.m_size);
    eq(what, o.m_entries != nullptr, r.m_entries != nullptr);
    eq(what, o.m_free == o.m_pool, r.m_free == r.m_pool);
}
static void compare_rgroup(const char* what, const AtDataRegistryGroup& o, const AtDataRegistryGroup& r) {
    eq(what, std::strncmp(o.m_name, r.m_name, 8), 0);
    eq(what, rel_on(0, o.m_first), rel_on(1, r.m_first));
    eq(what, rel_on(0, o.m_database), rel_on(1, r.m_database));
    eq(what, rel_on(0, o.m_nextFree), rel_on(1, r.m_nextFree));
}
static void compare_rspaces(const char* what, const AtDataRegistryNameSpaces& o, const AtDataRegistryNameSpaces& r) {
    eq(what, o.m_poolSize, r.m_poolSize);
    eq(what, o.m_pool != nullptr, r.m_pool != nullptr);
    eq(what, o.m_size, r.m_size);
    eq(what, o.m_entries != nullptr, r.m_entries != nullptr);
    eq(what, o.m_free == o.m_pool, r.m_free == r.m_pool);
    eq(what, rel_on(0, o.m_last), rel_on(1, r.m_last));
}
static void compare_owner(const char* what, const AtDataRegistryOwner& o, const AtDataRegistryOwner& r) {
    eq(what, o.m_unknown04, r.m_unknown04);
    compare_group(what, o.m_databases, r.m_databases);
    compare_db(what, o.m_databaseSpaces, r.m_databaseSpaces);
    compare_rgroup(what, o.m_plain, r.m_plain);
    compare_rspaces(what, o.m_spaces, r.m_spaces);
}

// ---- the setters ---------------------------------------------------------------------------------------------------------
static void run_setters() {
    struct S { uint32_t va; void (*rec)(void*); };
    static const S setters[] = {
        {0x000C8960, AtSetHeap3ADF80}, {0x000B4250, AtScriptHost::SetHeap}, {0x0006F590, AtSmallCScriptFactory::SetHeap},
        {0x00075400, AtSmallCSystem::SetHeap}, {0x000272D0, AtSetHeap2C6348}, {0x001015B0, AtSetHeap3B23E8}, {0x00101390, AtSetHeap3B23E4},
        {0x00034500, AtSetHeap2C63DC}, {0x00027850, AtSetHeap2C634C}, {0x00100FB0, AtSetHeap3B23E0}, {0x000C87D0, AtGrD3DAnimationDevice::SetHeap},
    };
    for (const S& s : setters) {
        set_globals(1);
        run_pair("setter", [&] { at<Cfn1>(s.va)(TOKEN(9)); }, [&] { s.rec(TOKEN(9)); });
        compare_globals();
    }
    // The SetHeaps of whole configs.
    struct C { const char* name; uint32_t va; int words; int heap; void (*rec)(void*, const void*); };
    static const C sets[] = {
        {"AtMathSystem::SetHeaps", 0x000B3870, 4, 1, [](void* s, const void* c) { ((AtMathSystem*)s)->AtMathSystem::SetHeaps((const AtMathConfig*)c); }},
        {"AtDataManagementSystem::SetHeaps", 0x000B3690, 13, 2,
         [](void* s, const void* c) { ((AtDataManagementSystem*)s)->AtDataManagementSystem::SetHeaps((const AtDataManagementConfig*)c); }},
        {"AtScriptSystem::SetHeaps", 0x0009EE00, 5, 1, [](void* s, const void* c) { ((AtScriptSystem*)s)->SetHeaps((const AtScriptConfig*)c); }},
        {"AtKinematicsSystemBase::SetHeaps", 0x000C8660, 13, 1,
         [](void* s, const void* c) { ((AtKinematicsSystemBase*)s)->AtKinematicsSystemBase::SetHeaps((const AtKinematicsConfig*)c); }},
        {"AtKinematicsSystem::SetHeaps", 0x000B3580, 13, 1,
         [](void* s, const void* c) { ((AtKinematicsSystem*)s)->AtKinematicsSystem::SetHeaps((const AtKinematicsConfig*)c); }},
    };
    for (const C& c : sets) {
        for (int pass = 0; pass < 24; pass++) {
            set_globals(pass < 2 ? pass : rnd() & 1);
            random_config(c.words, c.heap, pass);
            run_pair(c.name, [&] { at<Fn1>(c.va)(g_obj[0], nullptr, g_cfg[0]); }, [&] { c.rec(g_obj[1], g_cfg[1]); });
            compare_globals();
        }
    }
}

// ---- the math layer ------------------------------------------------------------------------------------------------------
static void run_math() {
    for (int pass = 0; pass < 12; pass++) {
        set_globals(pass & 1);
        std::memset(g_obj, 0xEE, sizeof g_obj);
        run_pair("AtMathSystem()", [&] { at<Fn0>(0x000B38B0)(g_obj[0], nullptr); }, [&] { new (g_obj[1]) AtMathSystem(); });
        eq("math constructed", std::memcmp(g_obj[0] + 4, g_obj[1] + 4, 8), 0);
        eq("math vftable", *(uint32_t*)g_obj[0], 0x002847FC);
        for (int side = 0; side < 2; side++) *(const void**)g_obj[side] = g_math_table;
        if (pass >= 6) for (int side = 0; side < 2; side++) ((AtMathSystem*)g_obj[side])->m_below = TOKEN(6);
        for (int side = 0; side < 2; side++) ((AtMathSystem*)g_obj[side])->m_unknown08 = 0x1234;
        random_config(4, 1, pass);
        g_cfg[0][0] = g_cfg[1][0] = rnd();
        run_pair("AtMathSystem::Init", [&] { at<Fn3>(0x000B38D0)(g_obj[0], nullptr, g_cfg[0], TOKEN(5), TOKEN(4)); },
                 [&] { ((AtMathSystem*)g_obj[1])->AtMathSystem::Init((const AtMathConfig*)g_cfg[1], TOKEN(5), TOKEN(4)); });
        AtMathSystem* o = (AtMathSystem*)g_obj[0];
        AtMathSystem* r = (AtMathSystem*)g_obj[1];
        eq("math below", rel_on(0, o->m_below), rel_on(1, r->m_below));
        eq("math +8", o->m_unknown08, r->m_unknown08);
        compare_globals();
        // Shutdown alone.
        run_pair("AtMathSystem::Shutdown", [&] { at<Fn0>(0x000E7880)(g_obj[0], nullptr); }, [&] { ((AtMathSystem*)g_obj[1])->AtMathSystem::Shutdown(); });
        eq("math shut down", std::memcmp(g_obj[0] + 4, g_obj[1] + 4, 8), 0);
    }
}

// ---- the data management layer ---------------------------------------------------------------------------------------------
static void run_data_management() {
    for (int pass = 0; pass < 16; pass++) {
        set_globals(pass & 1);
        std::memset(g_obj, 0xEE, sizeof g_obj);
        run_pair("AtDataManagementSystem()", [&] { at<Fn0>(0x000B3770)(g_obj[0], nullptr); }, [&] { new (g_obj[1]) AtDataManagementSystem(); });
        AtDataManagementSystem* o = (AtDataManagementSystem*)g_obj[0];
        AtDataManagementSystem* r = (AtDataManagementSystem*)g_obj[1];
        eq("dm vftable", *(uint32_t*)g_obj[0], 0x002847EC);
        eq("owner vftable", *(uint32_t*)(g_obj[0] + 4), 0x00277420);
        eq("dm below", rel_on(0, o->m_below), rel_on(1, r->m_below));
        compare_owner("constructed owner", o->m_owner, r->m_owner);
        for (int side = 0; side < 2; side++) *(const void**)g_obj[side] = g_dm_table;
        const bool running = pass >= 8;
        if (running) for (int side = 0; side < 2; side++) ((AtDataManagementSystem*)g_obj[side])->m_below = TOKEN(6);
        random_config(13, 2, pass);
        g_cfg[0][0] = g_cfg[1][0] = (pass % 4 == 0) ? 0 : 1 + rnd() % 40;
        g_cfg[0][1] = g_cfg[1][1] = (pass % 3 == 0) ? 0 : 1 + rnd() % 40;
        run_pair("AtDataManagementSystem::Init", [&] { at<Fn3>(0x000B3790)(g_obj[0], nullptr, g_cfg[0], TOKEN(5), TOKEN(4)); },
                 [&] { ((AtDataManagementSystem*)g_obj[1])->AtDataManagementSystem::Init((const AtDataManagementConfig*)g_cfg[1], TOKEN(5), TOKEN(4)); });
        eq("dm below", rel_on(0, o->m_below), rel_on(1, r->m_below));
        compare_owner("owner", o->m_owner, r->m_owner);
        compare_globals();
        // A second Init on top, sizes changed: the running layer clears its owner first.
        g_cfg[0][0] = g_cfg[1][0] = 1 + rnd() % 40;
        g_cfg[0][1] = g_cfg[1][1] = 1 + rnd() % 40;
        run_pair("AtDataManagementSystem::Init again", [&] { at<Fn3>(0x000B3790)(g_obj[0], nullptr, g_cfg[0], TOKEN(7), TOKEN(4)); },
                 [&] { ((AtDataManagementSystem*)g_obj[1])->AtDataManagementSystem::Init((const AtDataManagementConfig*)g_cfg[1], TOKEN(7), TOKEN(4)); });
        eq("dm below", rel_on(0, o->m_below), rel_on(1, r->m_below));
        compare_owner("owner again", o->m_owner, r->m_owner);
        // The owner alone: Clear, then Init with each size given or not, then Clear.
        // The plain groups hold something, so that Clear and Init have something to clear and to rename.
        auto dirty_groups = [&]() {
            for (int side = 0; side < 2; side++) {
                AtDataRegistryOwner* w = (AtDataRegistryOwner*)(g_obj[side] + 4);
                std::memcpy(w->m_plain.m_name, "plain", 6);
                w->m_plain.m_first = (AtDataRegistry*)TOKEN(1);
                w->m_plain.m_nextFree = (AtDataRegistryGroup*)TOKEN(2);
                w->m_plain.m_database = (AtDataRegistryNameSpaces*)TOKEN(3);
                std::memcpy(w->m_databases.m_name, "dbs", 4);
            }
        };
        dirty_groups();
        for (int side = 0; side < 2; side++) ((AtDataRegistryOwner*)(g_obj[side] + 4))->m_unknown04 = 0x5151;
        run_pair("AtDataRegistryOwner::Clear", [&] { at<Fn0>(0x0005F080)(g_obj[0] + 4, nullptr); }, [&] { ((AtDataRegistryOwner*)(g_obj[1] + 4))->Clear(); });
        compare_owner("owner cleared", o->m_owner, r->m_owner);
        dirty_groups();
        const uint32_t a = (pass & 2) ? 0 : 1 + rnd() % 20, b = (pass & 4) ? 0 : 1 + rnd() % 20;
        run_pair("AtDataRegistryOwner::Init", [&] { at<void(__fastcall*)(void*, void*, uint32_t, uint32_t)>(0x0005F030)(g_obj[0] + 4, nullptr, a, b); },
                 [&] { ((AtDataRegistryOwner*)(g_obj[1] + 4))->Init(a, b); });
        compare_owner("owner init", o->m_owner, r->m_owner);
        run_pair("AtDataManagementSystem::Shutdown", [&] { at<Fn0>(0x000B37D0)(g_obj[0], nullptr); },
                 [&] { ((AtDataManagementSystem*)g_obj[1])->AtDataManagementSystem::Shutdown(); });
        eq("dm below", rel_on(0, o->m_below), rel_on(1, r->m_below));
        compare_owner("owner shut down", o->m_owner, r->m_owner);
    }
    // The owner's constructor alone, on a filled buffer: its +0x04 is left.
    std::memset(g_obj, 0x77, sizeof g_obj);
    run_pair("AtDataRegistryOwner()", [&] { at<Fn0>(0x0005EFD0)(g_obj[0], nullptr); }, [&] { new (g_obj[1]) AtDataRegistryOwner(); });
    compare_owner("owner constructed alone", *(AtDataRegistryOwner*)g_obj[0], *(AtDataRegistryOwner*)g_obj[1]);
}

// ---- the script layer ------------------------------------------------------------------------------------------------------
static void compare_system(const char* what, const AtSmallCSystem& o, const AtSmallCSystem& r) {
    for (int i = 0; i < AtSmallCSystem::kMaxNatives; i++) {
        eq(what, std::memcmp(o.m_natives[i].m_name, r.m_natives[i].m_name, 0x14), 0);
        eq(what, native_va(0, o.m_natives[i].m_function), native_va(1, r.m_natives[i].m_function));
        eq(what, rel_on(0, o.m_natives[i].m_context), rel_on(1, r.m_natives[i].m_context));
    }
    eq(what, std::memcmp(o.m_hostVars, r.m_hostVars, sizeof o.m_hostVars), 0);
    for (int i = 0; i < AtSmallCSystem::kMaxScripts; i++) eq(what, rel_on(0, o.m_scripts[i]), rel_on(1, r.m_scripts[i]));
    eq(what, o.m_nativeCount, r.m_nativeCount);
    eq(what, o.m_unknown5B04, r.m_unknown5B04);
    eq(what, o.m_scriptCount, r.m_scriptCount);
    eq(what, o.m_lastStarted, r.m_lastStarted);
    eq(what, o.m_unknown5B10, r.m_unknown5B10);
    eq(what, rel_on(0, o.m_factory), rel_on(1, r.m_factory));
    eq(what, o.m_unknown5B18, r.m_unknown5B18);
    eq(what, o.m_names.m_size, r.m_names.m_size);
    eq(what, o.m_names.m_entries != nullptr, r.m_names.m_entries != nullptr);
    eq(what, rel_on(0, o.m_names.m_next), rel_on(1, r.m_names.m_next));
    eq(what, rel_on(0, o.m_names.m_group), rel_on(1, r.m_names.m_group));
    eq(what, o.m_unknown5B30, r.m_unknown5B30);
    eq(what, rel_on(0, o.m_programFs), rel_on(1, r.m_programFs));
    eq(what, o.m_stepMode, r.m_stepMode);
    eq(what, o.m_unknown5B3C, r.m_unknown5B3C);
    eq(what, o.m_unknown5B40, r.m_unknown5B40);
    eq(what, o.m_running, r.m_running);
}
static void compare_script(const char* what) {
    const AtScriptSystem& o = *(const AtScriptSystem*)g_obj[0];
    const AtScriptSystem& r = *(const AtScriptSystem*)g_obj[1];
    eq(what, o.m_running, r.m_running);
    eq(what, rel_on(0, o.m_below), rel_on(1, r.m_below));
    eq(what, o.m_host.m_running, r.m_host.m_running);
    eq(what, *(const uint32_t*)&o.m_host.m_factory, *(const uint32_t*)&r.m_host.m_factory);  // nothing writes it
    compare_system(what, o.m_host.m_system, r.m_host.m_system);
}
// Fills the parts of both systems no constructor writes, alike, and some fields that Clear and Init should overwrite.
static void dirty_system(uint32_t seed) {
    for (int side = 0; side < 2; side++) {
        AtSmallCSystem& y = ((AtScriptSystem*)g_obj[side])->m_host.m_system;
        y.m_scriptCount = (int)seed;
        y.m_programFs = (AtIoFileSystem*)(uintptr_t)(seed >> 3);  // never followed: only Clear writes it here
        y.m_unknown5B3C = 9;
    }
}
static void put_scripts(int count) {
    for (int k = 0; k < count; k++) {
        const int i = (int)(rnd() % AtSmallCSystem::kMaxScripts);
        for (int side = 0; side < 2; side++) ((AtScriptSystem*)g_obj[side])->m_host.m_system.m_scripts[i] = TOKEN(1 + i % 7);
    }
}

static void run_script() {
    typedef void(__fastcall* Reg)(void*, void*, void*, const char*, AtSmallCNative);
    for (int pass = 0; pass < 10; pass++) {
        set_globals(pass & 1);
        std::memset(g_obj, 0xEE, sizeof g_obj);
        run_pair("AtScriptSystem()", [&] { at<Fn0>(0x0009EE60)(g_obj[0], nullptr); }, [&] { new (g_obj[1]) AtScriptSystem(); });
        eq("script vftable", *(uint32_t*)g_obj[0], 0x0027D03C);
        compare_script("constructed");
        compare_globals();
        for (int side = 0; side < 2; side++) *(const void**)g_obj[side] = g_script_table;
        const bool running = pass >= 5;
        if (running) {
            for (int side = 0; side < 2; side++) {
                AtScriptSystem* s = (AtScriptSystem*)g_obj[side];
                s->m_running = 2;  // a word, not a bit
                s->m_host.m_system.m_running = 1;
                s->m_host.m_system.m_factory = &s->m_host.m_factory;
            }
            put_scripts(pass);
        }
        dirty_system(pass * 77);
        random_config(5, 1, pass);
        g_cfg[0][0] = g_cfg[1][0] = rnd();
        run_pair("AtScriptSystem::Init", [&] { at<Fn3>(0x0009EE80)(g_obj[0], nullptr, g_cfg[0], TOKEN(5), TOKEN(4)); },
                 [&] { ((AtScriptSystem*)g_obj[1])->AtScriptSystem::Init((const AtScriptConfig*)g_cfg[1], TOKEN(5), TOKEN(4)); });
        compare_script("Init");
        compare_globals();
        // The system alone: Register, until the table is full, with long names, and its return.
        for (int k = 0; k < 4; k++) {
            static const char* names[] = {"a", "nineteen_characters", "twenty_characters_xx", "twenty-five_characters_xx"};
            const char* name = names[(pass + k) % 4];
            int got[2];
            AtSmallCNative fn = kNatives[(pass + k) % 19].rec;
            run_pair("AtSmallCSystem::Register",
                     [&] { got[0] = at<int(__fastcall*)(void*, void*, void*, const char*, uint32_t)>(0x000754D0)(&((AtScriptSystem*)g_obj[0])->m_host.m_system, nullptr, TOKEN(3), name, native_va(1, fn)); },
                     [&] { got[1] = ((AtScriptSystem*)g_obj[1])->m_host.m_system.Register(TOKEN(3), name, (AtSmallCNative)(uintptr_t)native_va(1, fn)); });
            eq("Register", got[0], got[1]);
        }
        if (pass == 3) {
            for (int side = 0; side < 2; side++) ((AtScriptSystem*)g_obj[side])->m_host.m_system.m_nativeCount = 0x1F8;
            int got[2];
            for (int k = 0; k < 12; k++) {
                run_pair("Register past the end", [&] { got[0] = at<int(__fastcall*)(void*, void*, void*, const char*, uint32_t)>(0x000754D0)(&((AtScriptSystem*)g_obj[0])->m_host.m_system, nullptr, TOKEN(2), "full", 0x0007C800); },
                         [&] { got[1] = ((AtScriptSystem*)g_obj[1])->m_host.m_system.Register(TOKEN(2), "full", (AtSmallCNative)(uintptr_t)0x0007C800); });
                eq("Register past the end", got[0], got[1]);
            }
            // Init on a nearly full table: some built-ins do not fit.
            for (int side = 0; side < 2; side++) ((AtScriptSystem*)g_obj[side])->m_host.m_system.m_nativeCount = 0x1F0;
            run_pair("AtSmallCSystem::Init nearly full", [&] { at<Fn0>(0x000756E0)(&((AtScriptSystem*)g_obj[0])->m_host.m_system, nullptr); },
                     [&] { ((AtScriptSystem*)g_obj[1])->m_host.m_system.Init(); });
        }
        compare_script("Register");
        // The host's Init on top, then the system's Shutdown with scripts, Clear, Shutdown when not running, and the layer's Shutdown.
        run_pair("AtScriptHost::Init", [&] { at<Fn0>(0x000B42B0)(g_obj[0] + 0xC, nullptr); }, [&] { ((AtScriptSystem*)g_obj[1])->m_host.Init(); });
        compare_script("host Init");
        compare_globals();
        put_scripts(1 + pass % 4);
        run_pair("AtSmallCSystem::Shutdown", [&] { at<Fn0>(0x000757A0)(g_obj[0] + 0x14, nullptr); }, [&] { ((AtScriptSystem*)g_obj[1])->m_host.m_system.Shutdown(); });
        compare_script("system Shutdown");
        compare_globals();
        put_scripts(3);
        dirty_system(pass);
        run_pair("AtSmallCSystem::Shutdown not running", [&] { at<Fn0>(0x000757A0)(g_obj[0] + 0x14, nullptr); },
                 [&] { ((AtScriptSystem*)g_obj[1])->m_host.m_system.Shutdown(); });
        compare_script("system Shutdown not running");
        run_pair("AtSmallCSystem::Clear", [&] { at<Fn0>(0x00075590)(g_obj[0] + 0x14, nullptr); }, [&] { ((AtScriptSystem*)g_obj[1])->m_host.m_system.Clear(); });
        compare_script("system Clear");
        compare_globals();
        run_pair("AtScriptHost::Init again", [&] { at<Fn0>(0x000B42B0)(g_obj[0] + 0xC, nullptr); }, [&] { ((AtScriptSystem*)g_obj[1])->m_host.Init(); });
        put_scripts(2);
        run_pair("AtScriptHost::Shutdown", [&] { at<Fn0>(0x000B42F0)(g_obj[0] + 0xC, nullptr); }, [&] { ((AtScriptSystem*)g_obj[1])->m_host.Shutdown(); });
        compare_script("host Shutdown");
        for (int side = 0; side < 2; side++) ((AtScriptSystem*)g_obj[side])->m_below = TOKEN(6);
        run_pair("AtScriptSystem::Shutdown", [&] { at<Fn0>(0x0009EEC0)(g_obj[0], nullptr); }, [&] { ((AtScriptSystem*)g_obj[1])->AtScriptSystem::Shutdown(); });
        compare_script("layer Shutdown");
        compare_globals();
        // Free what each side's name map holds, outside the logs.
        for (int side = 0; side < 2; side++) {
            GmNameMap& m = ((AtScriptSystem*)g_obj[side])->m_host.m_system.m_names;
            if (m.m_entries) { std::free(m.m_entries); m.m_entries = nullptr; }
        }
    }
    // The host and the system constructed alone.
    std::memset(g_obj, 0x99, sizeof g_obj);
    set_globals(0);
    run_pair("AtScriptHost()", [&] { at<Fn0>(0x000B4260)(g_obj[0] + 0xC, nullptr); }, [&] { new (g_obj[1] + 0xC) AtScriptHost(); });
    compare_script("host constructed");
    compare_globals();
    std::memset(g_obj, 0x99, sizeof g_obj);
    run_pair("AtSmallCSystem()", [&] { at<Fn0>(0x000756C0)(g_obj[0] + 0x14, nullptr); }, [&] { new (g_obj[1] + 0x14) AtSmallCSystem(); });
    compare_script("system constructed");
    compare_globals();
}

// ---- the kinematics layer --------------------------------------------------------------------------------------------------
static const void* g_rec_factory_vftable;
static void compare_kin(const char* what) {
    const AtKinematicsSystem& o = *(const AtKinematicsSystem*)g_obj[0];
    const AtKinematicsSystem& r = *(const AtKinematicsSystem*)g_obj[1];
    eq(what, rel_on(0, o.m_below), rel_on(1, r.m_below));
    eq(what, o.m_factory != nullptr, r.m_factory != nullptr);
    if (o.m_factory && r.m_factory) eq("factory vftable", *(const uint32_t*)o.m_factory == 0x0028663C, *(const void* const*)r.m_factory == g_rec_factory_vftable);
}
static void run_kinematics() {
    {
        AtGrD3DAnimationDevice f;
        g_rec_factory_vftable = *(const void* const*)&f;
    }
    for (int pass = 0; pass < 16; pass++) {
        set_globals(pass & 1);
        std::memset(g_obj, 0xEE, sizeof g_obj);
        const bool base = pass % 4 == 3;
        if (base) run_pair("AtKinematicsSystemBase()", [&] { at<Fn0>(0x000C8740)(g_obj[0], nullptr); }, [&] { new (g_obj[1]) AtKinematicsSystemBase(); });
        else run_pair("AtKinematicsSystem()", [&] { at<Fn0>(0x000B34B0)(g_obj[0], nullptr); }, [&] { new (g_obj[1]) AtKinematicsSystem(); });
        eq("kinematics vftable", *(uint32_t*)g_obj[0], base ? 0x0028662C : 0x002847DC);
        eq("kinematics constructed", std::memcmp(g_obj[0] + 4, g_obj[1] + 4, 8), 0);  // +4 left as it was
        for (int side = 0; side < 2; side++) *(const void**)g_obj[side] = base ? g_kinb_table : g_kin_table;
        for (int side = 0; side < 2; side++) ((AtKinematicsSystem*)g_obj[side])->m_factory = nullptr;
        const bool running = pass >= 8;
        if (running) {
            for (int side = 0; side < 2; side++) {
                AtKinematicsSystem* k = (AtKinematicsSystem*)g_obj[side];
                k->m_below = TOKEN(6);
                if (pass & 1) {  // with a factory to free: each side's own, made by its own constructor
                    void* block = std::calloc(1, 8);
                    if (side == 0) at<Fn0>(0x000C8920)(block, nullptr);
                    else new (block) AtGrD3DAnimationDevice();
                    k->m_factory = (AtGrD3DAnimationDevice*)block;
                }
            }
        }
        random_config(13, 1, pass);
        g_cfg[0][0] = g_cfg[1][0] = rnd();
        if (pass == 5 || pass == 12) g_fail_heap = TOKEN(15);  // the factory cannot be made: both raise, then store null
        if (base) run_pair("AtKinematicsSystemBase::Init", [&] { at<Fn3>(0x000C8750)(g_obj[0], nullptr, g_cfg[0], TOKEN(5), TOKEN(4)); },
                           [&] { ((AtKinematicsSystemBase*)g_obj[1])->AtKinematicsSystemBase::Init((const AtKinematicsConfig*)g_cfg[1], TOKEN(5), TOKEN(4)); });
        else run_pair("AtKinematicsSystem::Init", [&] { at<Fn3>(0x000B34D0)(g_obj[0], nullptr, g_cfg[0], TOKEN(5), TOKEN(4)); },
                      [&] { ((AtKinematicsSystem*)g_obj[1])->AtKinematicsSystem::Init((const AtKinematicsConfig*)g_cfg[1], TOKEN(5), TOKEN(4)); });
        g_fail_heap = nullptr;
        g_fail_alloc = false;
        compare_kin(base ? "base Init" : "Init");
        compare_globals();
        if (base) {
            run_pair("AtKinematicsSystemBase::Shutdown", [&] { at<Fn0>(0x000C8780)(g_obj[0], nullptr); },
                     [&] { ((AtKinematicsSystemBase*)g_obj[1])->AtKinematicsSystemBase::Shutdown(); });
        } else {
            run_pair("AtKinematicsSystem::Shutdown", [&] { at<Fn0>(0x000B35B0)(g_obj[0], nullptr); },
                     [&] { ((AtKinematicsSystem*)g_obj[1])->AtKinematicsSystem::Shutdown(); });
        }
        compare_kin("Shutdown");
        for (int side = 0; side < 2; side++) {  // what the base's Shutdown leaves
            AtKinematicsSystem* k = (AtKinematicsSystem*)g_obj[side];
            if (base && k->m_factory) { std::free(k->m_factory); k->m_factory = nullptr; }
        }
    }
    // The factory's constructor and destructor alone.
    static unsigned char f[2][8];
    std::memset(f, 0x44, sizeof f);
    run_pair("AtGrD3DAnimationDevice()", [&] { at<Fn0>(0x000C8920)(f[0], nullptr); }, [&] { new (f[1]) AtGrD3DAnimationDevice(); });
    eq("factory constructed", *(uint32_t*)f[0] == 0x0028663C, *(const void**)f[1] == g_rec_factory_vftable);
    eq("factory +4", *(uint32_t*)(f[0] + 4), *(uint32_t*)(f[1] + 4));
    std::memset(f, 0x45, sizeof f);
    run_pair("~AtGrD3DAnimationDevice", [&] { at<Fn0>(0x000C8930)(f[0], nullptr); }, [&] { ((AtGrD3DAnimationDevice*)f[1])->~AtGrD3DAnimationDevice(); });
    eq("factory destroyed", *(uint32_t*)f[0] == 0x0028663C, *(const void**)f[1] == g_rec_factory_vftable);
}

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
    if (pos.size() != 1) { std::printf("usage: oracle_atlayers <default.xbe> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    at<void(__cdecl*)()>(0x00125BC7)();

    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)s_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    redirect(0x0006F640, (const void*)o_release);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    if (entries && !arm_entries(entries)) return 2;

    BadFill<1, 3>::run(g_math_table);
    g_math_table[2] = (const void*)d_math_shutdown;
    g_math_table[3] = (const void*)d_math_set_heaps;
    BadFill<2, 3>::run(g_dm_table);
    g_dm_table[2] = (const void*)d_dm_shutdown;
    g_dm_table[3] = (const void*)d_dm_set_heaps;
    BadFill<3, 2>::run(g_script_table);
    g_script_table[2] = (const void*)d_script_shutdown;
    BadFill<4, 3>::run(g_kin_table);
    g_kin_table[2] = (const void*)d_kin_shutdown;
    g_kin_table[3] = (const void*)d_kin_set_heaps;
    BadFill<5, 3>::run(g_kinb_table);
    g_kinb_table[2] = (const void*)d_kinb_shutdown;
    g_kinb_table[3] = (const void*)d_kinb_set_heaps;

    for (int side = 0; side < 2; side++) add_region(side, g_obj[side], sizeof g_obj[side], kObjTag);
    g_fill = 0xCD;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_setters();
    run_math();
    run_data_management();
    run_script();
    run_kinematics();
    report_entries();
    if (g_bad) { std::printf("a slot no case should reach was reached %d times\n", g_bad); g_failures++; }
    std::printf("%s: %d crashes on both sides; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", (int)g_crashes, g_checks,
                g_failures);
    return g_failures ? 1 : 0;
}
