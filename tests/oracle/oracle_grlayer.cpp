// Differential oracle for the graphics layer's object short of the hardware (subsystem at/grlayer; Windows x86, local only). See docs/verification.md.
//
//   oracle_grlayer <default.xbe> [--entries FILE] [--mutate]
//
// No game file is read. What stands in for what, first: the hardware is a double on both sides. The original's factory start (0x000C7C60) and stop
// (0x000C7C90) are redirected to logging doubles, and the recovered side's seams are the same doubles; the factory they hand back is one object both
// sides share, with a table of doubles (slot 0 gives a token, slot 3 a shared stand-in device whose slot 1 logs, slot 4 logs what it is handed). Each
// Init runs on an object made by the side's own constructor, with the object's vftable replaced by a table whose Shutdown and SetHeaps slots log and
// then run that side's own code for the slot. What runs for real on both sides: everything recorded for this subsystem, the memory globals' Save and
// Restore (at/atap, at/memory), the pixel format's constructor and Clear (at/pixelformat), and the heap setters of at/agd, at/matdb, at/ags, at/agn,
// at/agt, at/atap and gm/names, each the original's on the original side. The original's heap stack and fatal paths are doubles
// (tests/oracle/oracle_doubles.h). The matrix table is compared only in the fifteen members the recovered code knows (include/at/AtCoordSys.h).
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

#include "at/AtDatabaseLoader.h"
#include "at/AtGraphicsLayer.h"
#include "at/AtImageDatabase.h"
#include "at/AtIoJam.h"
#include "at/AtMatAnimation.h"
#include "at/AtMatAssignment.h"
#include "at/AtMaterial.h"
#include "at/AtMaterialDatabase.h"
#include "at/AtShader.h"
#include "at/AtShaderDatabase.h"
#include "at/AtTextureDatabase.h"
#include "gm/GmNameMap.h"
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

#include "oracle_doubles.h"

static void*& orig_ptr(uint32_t va) { return *(void**)(uintptr_t)va; }
static uint32_t& orig_u32(uint32_t va) { return *(uint32_t*)(uintptr_t)va; }

static uint32_t g_rng = 13579;
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
// The heap stack is empty on both sides: the original's pushes are doubles, so its own stack is never written.
AtHeap* AtCurrentHeap() { return nullptr; }

static unsigned char g_tokens[48][64];
#define TOKEN(i) ((void*)g_tokens[i])

// ---- the hardware, as doubles both sides share ------------------------------------------------------------------------------
template <int T, int S> static void __fastcall d_bad(void*, void*) { L('BAD', T * 100 + S); }
template <int T, int S> struct BadFill {
    static void run(const void** t) { t[S] = (const void*)&d_bad<T, S>; BadFill<T, S - 1>::run(t); }
};
template <int T> struct BadFill<T, -1> {
    static void run(const void**) {}
};

static const void* g_factory_table[34];
static const void* g_owner_table[18];
static struct { const void* const* vftable; } g_factory = {g_factory_table}, g_owner = {g_owner_table};
static void* __fastcall d_factory_slot0(void* self, void*) { L('fs0', self == &g_factory); return TOKEN(40); }
static void* __fastcall d_factory_slot3(void* self, void*) { L('fs3', self == &g_factory); return &g_owner; }
static void __fastcall d_factory_slot4(void* self, void*, void* object) { L('fs4', self == &g_factory); L('fs4o', rel(object)); }
static void __fastcall d_owner_slot1(void* self, void*) { L('own1', self == &g_owner); }

static void* g_start_returns;  // the factory each start hands back: the shared one, or null
static void log_start(const void* saved, const void* config) {
    L('sf', rel(config));
    const uint32_t* w = (const uint32_t*)saved;
    L('sfc', w[3]);                         // AtIoGlobals' word
    L('sfm', rel((const void*)(uintptr_t)w[5]));  // where the matrix table was saved from
    L('sfr', rel((const void*)(uintptr_t)w[6]));  // where the render table was saved from
}
static void* __cdecl o_start(void* saved, void* config) { log_start(saved, config); return g_start_returns; }
static void __cdecl o_stop() { L('stp'); }
AtGraphicsFactory* AtGraphicsStartFactory(AtGraphicsGlobals* saved, const AtGraphicsConfig* config) {
    log_start(saved, config);
    return (AtGraphicsFactory*)g_start_returns;
}
void AtGraphicsStopFactory() { L('stp'); }

// The rest of the recovered side's seams (src/at/AtGraphicsLayerSeams.cpp is not linked); no case reaches them.
void AtGraphicsSystemBase::DeletingDestructor(unsigned int) { L('BAD', 9001); }
void AtGraphicsSystemBase::SetDisplay(AtDisplayMode*, int, int, int, const AtPixelFormat*, int, int) { L('BAD', 9002); }
void AtGraphicsSystem::DeletingDestructor(unsigned int) { L('BAD', 9003); }
void AtGraphicsSystem::SetDisplay(AtDisplayMode*, int, int, int, const AtPixelFormat*, int, int) { L('BAD', 9004); }
void AtGraphicsSystem::UpdateDisplay() { L('BAD', 9005); }
void* AtGraphicsFactory::Slot0() { L('BAD', 9100); return nullptr; }
AtMaterialOwner* AtGraphicsFactory::CreateDevice() { L('BAD', 9103); return nullptr; }
void AtGraphicsFactory::Destroy(void*) { L('BAD', 9104); }
AtDatabaseLoader* AtGraphicsFactory::DatabaseLoader() { L('BAD', 9129); return nullptr; }
#define SLOT(n) \
    void AtGraphicsFactory::Slot##n() { L('BAD', 9100 + n); }
SLOT(1) SLOT(2) SLOT(5) SLOT(6) SLOT(7) SLOT(8) SLOT(9) SLOT(10) SLOT(11) SLOT(12) SLOT(13) SLOT(14) SLOT(15) SLOT(16) SLOT(17) SLOT(18) SLOT(19)
SLOT(20) SLOT(21) SLOT(22) SLOT(23) SLOT(24) SLOT(25) SLOT(26) SLOT(27) SLOT(28)
#undef SLOT

// ---- tables of doubles for the layer's own slots -----------------------------------------------------------------------------
typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, const void*);
typedef void(__fastcall* Fn3)(void*, void*, const void*, void*, void*);
typedef void(__cdecl* Cfn1)(void*);

static void __fastcall d_shutdown(void* self, void* edx) {
    L('gsd', rel(self));
    if (g_side == 0) at<Fn0>(0x000B30B0)(self, edx);
    else ((AtGraphicsSystem*)self)->AtGraphicsSystem::Shutdown();
}
static void __fastcall d_set_heaps(void* self, void* edx, const AtGraphicsConfig* c) {
    L('gsh', rel(self));
    if (g_side == 0) at<Fn1>(0x000B2F60)(self, edx, c);
    else ((AtGraphicsSystem*)self)->AtGraphicsSystem::SetHeaps(c);
}
static const void* g_layer_table[8];
static void __fastcall d_base_shutdown(void* self, void* edx) {
    L('bsd', rel(self));
    if (g_side == 0) at<Fn0>(0x000C8210)(self, edx);
    else ((AtGraphicsSystemBase*)self)->AtGraphicsSystemBase::Shutdown();
}
static const void* g_base_table[8];

// ---- the globals both sides write ------------------------------------------------------------------------------------------
struct Global { const char* name; uint32_t va; void** rec; };
static Global g_globals[] = {
    {"0x003B23DC", 0x003B23DC, &g_heap3B23DC}, {"0x002C7C38", 0x002C7C38, &g_heap2C7C38}, {"0x003B23D8", 0x003B23D8, &g_heap3B23D8},
    {"0x002C7FF0", 0x002C7FF0, &g_heap2C7FF0}, {"0x002C635C", 0x002C635C, &g_heap2C635C}, {"0x002C63E8", 0x002C63E8, &g_heap2C63E8},
    {"0x003B23D4", 0x003B23D4, &g_heap3B23D4}, {"0x003B1FC4", 0x003B1FC4, &g_heap3B1FC4}, {"0x002C6358", 0x002C6358, &g_heap2C6358},
    {"0x002CB08C", 0x002CB08C, &g_heap2CB08C}, {"0x002CB090", 0x002CB090, &g_heap2CB090}, {"0x002C9C08", 0x002C9C08, &g_heap2C9C08},
    {"0x003B13D4", 0x003B13D4, &g_heap3B13D4}, {"0x002C6378", 0x002C6378, &g_heap2C6378}, {"0x002C637C", 0x002C637C, &g_heap2C637C},
    {"0x003B1F50", 0x003B1F50, &g_heap3B1F50}, {"0x003B23D0", 0x003B23D0, &g_heap3B23D0}, {"0x003B1FA0", 0x003B1FA0, &g_heap3B1FA0},
    {"0x003B1F9C", 0x003B1F9C, &g_heap3B1F9C}, {"0x002C6384", 0x002C6384, &g_heap2C6384}, {"0x002C6388", 0x002C6388, &g_heap2C6388},
    {"0x003B1FC0", 0x003B1FC0, &g_heap3B1FC0}, {"0x003B1FBC", 0x003B1FBC, &g_heap3B1FBC}, {"0x003B1FB8", 0x003B1FB8, &g_heap3B1FB8},
    {"0x003B1FB4", 0x003B1FB4, &g_heap3B1FB4}, {"0x003B1FB0", 0x003B1FB0, &g_heap3B1FB0}, {"0x003B1FAC", 0x003B1FAC, &g_heap3B1FAC},
    {"0x003B1FA8", 0x003B1FA8, &g_heap3B1FA8}, {"0x003B1FA4", 0x003B1FA4, &g_heap3B1FA4}, {"0x003B13CC", 0x003B13CC, &g_heap3B13CC},
    {"0x003B1F98", 0x003B1F98, &g_heap3B1F98}, {"0x003B1F90", 0x003B1F90, &g_heap3B1F90}, {"0x003B1F70", 0x003B1F70, &g_heap3B1F70},
    {"0x003B1F54", 0x003B1F54, &g_heap3B1F54}, {"0x003B13E0", 0x003B13E0, &g_heap3B13E0}, {"0x003B13DC", 0x003B13DC, &g_heap3B13DC},
    {"0x003B13D8", 0x003B13D8, &g_heap3B13D8}, {"0x003B13D0", 0x003B13D0, &g_heap3B13D0},
    // The three stand-ins for setters recovered elsewhere.
    {"0x003B1FCC", 0x003B1FCC, &g_heap3B1FCC}, {"0x003B1F94", 0x003B1F94, &g_heap3B1F94}, {"0x003B1F58", 0x003B1F58, &g_heap3B1F58},
    // The classes of other subsystems.
    {"AtDatabaseLoader heap", 0x002C62DC, &AtDatabaseLoader::s_heap},
    {"AtMaterialDatabase heap", 0x002C63F0, &AtMaterialDatabase::s_heap},
    {"AtShader heap", 0x002C63EC, &AtShader::s_heap},
    {"AtMatAnimationDatabase heap", 0x002C6354, &AtMatAnimationDatabase::s_heap},
    {"AtMatAssignment heap", 0x002C63E0, &AtMatAssignment::s_heap},
    {"AtMatAssignmentDatabase heap", 0x002C6350, &AtMatAssignmentDatabase::s_heap},
    {"AtTextureDatabase heap", 0x002C62E4, &AtTextureDatabase::s_heap},
    {"AtImageDatabase heap", 0x002C6364, &AtImageDatabase::s_heap},
    {"AtShaderDatabase heap", 0x002C6380, &AtShaderDatabase::s_heap},
    {"AtFontDatabase heap", 0x002C6370, &AtFontDatabase::s_heap},
    {"AtProgShaderComponent heap", 0x002C7C44, &AtProgShaderComponent::s_heap},
    {"GmNameMap heap", 0x002C632C, &GmNameMap::s_heap},
    {"GmNameSpaceDb heap", 0x002C6274, &GmNameSpaceDb::s_heap},
    // What the memory globals save and put back.
    {"AtHeapManager instance", 0x002C60AC, (void**)&AtHeapManager::s_instance},
    {"AtIoTokenFile heap", 0x002C60B0, &AtIoTokenFile::s_heap},
    {"fatal handler", 0x002B48E4, &AtMemoryGlobals::s_fatal},
};
static void set_globals(uint32_t seed) {
    for (Global& g : g_globals) {
        uint32_t r = rnd();
        void* v = (seed & 1) ? nullptr : (r % 3 == 0 ? nullptr : TOKEN(30 + r % 6));
        orig_ptr(g.va) = v;
        *g.rec = v;
    }
}
static void compare_globals() {
    for (Global& g : g_globals) eq(g.name, rel_on(0, orig_ptr(g.va)), rel_on(1, *g.rec));
}

// ---- the function tables: the original's at their addresses, the recovered's by member ------------------------------------
static const uint32_t kMatrix = 0x002C60C8, kRender = 0x002C7C48;
struct Member { uint32_t orig; uint32_t rec; };
static const Member kMembers[] = {
    {0x000, offsetof(AtMatrixTable, Copy)},          {0x004, offsetof(AtMatrixTable, Identity)},     {0x00C, offsetof(AtMatrixTable, SetTranslation)},
    {0x018, offsetof(AtMatrixTable, SetRotation)},   {0x02C, offsetof(AtMatrixTable, SetLookAt)},    {0x058, offsetof(AtMatrixTable, SetScale)},
    {0x10C, offsetof(AtMatrixTable, SetInverse)},    {0x160, offsetof(AtMatrixTable, Multiply)},     {0x16C, offsetof(AtMatrixTable, SetProduct)},
    {0x188, offsetof(AtMatrixTable, TransformPoint)}, {0x054, offsetof(AtMatrixTable, SetUniformScale)}, {0x19C, offsetof(AtMatrixTable, Read)},
    {0x130, offsetof(AtMatrixTable, GetPosition)},   {0x134, offsetof(AtMatrixTable, GetAxes)},      {0x194, offsetof(AtMatrixTable, TransformDirection)},
};
static uint32_t& word_at(void* base, uint32_t offset) { return *(uint32_t*)((unsigned char*)base + offset); }
// Fills a table the original's way (0x1A0 or 0x1BC bytes at orig) and the recovered's way (rec), alike where the recovered code knows the members.
static void fill_matrix(void* orig, AtMatrixTable* rec, uint32_t seed) {
    for (uint32_t i = 0; i < 0x1A0; i += 4) word_at(orig, i) = 0xAB000000u + seed * 0x1000 + i;  // the members not known: not compared
    for (const Member& m : kMembers) {
        const uint32_t v = (rnd() % 4 == 0) ? 0 : (uint32_t)(uintptr_t)TOKEN(rnd() % 48);
        word_at(orig, m.orig) = v;
        word_at(rec, m.rec) = v;
    }
}
static void fill_render(void* orig, AtRenderTable* rec, uint32_t seed) {
    fill_matrix(orig, &rec->m_matrix, seed);
    for (int i = 0; i < 7; i++) {
        const uint32_t v = (rnd() % 4 == 0) ? 0 : (uint32_t)(uintptr_t)TOKEN(rnd() % 48);
        word_at(orig, 0x1A0 + 4 * i) = v;
        rec->m_extended[i] = (void*)(uintptr_t)v;
    }
}
static void compare_matrix(const char* what, void* orig, const AtMatrixTable* rec) {
    for (const Member& m : kMembers) eq(what, word_at(orig, m.orig), word_at((void*)rec, m.rec));
}
static void compare_render(const char* what, void* orig, const AtRenderTable* rec) {
    compare_matrix(what, orig, &rec->m_matrix);
    for (int i = 0; i < 7; i++) eq(what, word_at(orig, 0x1A0 + 4 * i), (uint32_t)(uintptr_t)rec->m_extended[i]);
}
static void fill_tables(uint32_t seed) {
    fill_render((void*)(uintptr_t)kRender, &g_atRender, seed);
    fill_matrix((void*)(uintptr_t)kMatrix, &g_atMatrix, seed + 1);
}
static void compare_tables(const char* what) {
    compare_matrix(what, (void*)(uintptr_t)kMatrix, &g_atMatrix);
    compare_render(what, (void*)(uintptr_t)kRender, &g_atRender);
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

// Configs (60 words): each heap word (+0x0C to +0xD0) a token or 0, the default heap at +0x08 always set; the kind and the other words random.
static uint32_t g_cfg[2][64];
static const uint32_t kCfgTag = 0x72000000u;
static void random_config(int pass, uint32_t kind) {
    for (int i = 0; i < 60; i++) {
        const uint32_t r = rnd();
        uint32_t v;
        if (i >= 3 && i <= 52) v = (pass == 0 || r % 2 == 0) ? 0 : (uint32_t)(uintptr_t)TOKEN(1 + r % 28);
        else v = rnd();
        g_cfg[0][i] = g_cfg[1][i] = v;
    }
    g_cfg[0][0] = g_cfg[1][0] = kind;
    g_cfg[0][2] = g_cfg[1][2] = (uint32_t)(uintptr_t)TOKEN(29);
    for (int i = 60; i < 64; i++) g_cfg[0][i] = g_cfg[1][i] = 0x5A5A5A5Au;
}
static void compare_config(const char* what) {
    for (int i = 0; i < 64; i++) eq(what, g_cfg[0][i], g_cfg[1][i]);
}

// ---- objects --------------------------------------------------------------------------------------------------------------
static unsigned char g_obj[2][0x100];
static const uint32_t kObjTag = 0x71000000u;
static void compare_object(const char* what, uint32_t size) {
    for (uint32_t i = 4; i < size; i += 4) eq(what, rel_on(0, (void*)(uintptr_t)word_at(g_obj[0], i)), rel_on(1, (void*)(uintptr_t)word_at(g_obj[1], i)));
}

// ---- the setters ---------------------------------------------------------------------------------------------------------
static void run_setters() {
    struct S { uint32_t va; void (*rec)(void*); };
    static const S setters[] = {
        {0x00100DB0, AtSetHeap3B23DC}, {0x00043760, AtSetHeap2C7C38}, {0x00100CF0, AtSetHeap3B23D8}, {0x00057DB0, AtSetHeap2C7FF0},
        {0x00028CD0, AtSetHeap2C635C}, {0x00036AB0, AtSetHeap2C63E8}, {0x00100350, AtSetHeap3B23D4}, {0x000FE500, AtSetHeap3B1FC4},
        {0x000283E0, AtSetHeap2C6358}, {0x00060590, AtSetHeap2CB08C}, {0x00065B00, AtSetHeap2CB090}, {0x00059580, AtSetHeap2C9C08},
        {0x000ED2B0, AtSetHeap3B13D4}, {0x0002AAF0, AtSetHeap2C6378}, {0x0002AD50, AtSetHeap2C637C}, {0x000F5610, AtSetHeap3B1F50},
        {0x00100340, AtSetHeap3B23D0}, {0x000F93C0, AtSetHeap3B1FA0}, {0x000F8500, AtSetHeap3B1F9C}, {0x0002B420, AtSetHeap2C6384},
        {0x0002B680, AtSetHeap2C6388}, {0x000FCE60, AtSetHeap3B1FC0}, {0x000FCD20, AtSetHeap3B1FBC}, {0x000FC550, AtSetHeap3B1FB8},
        {0x000FBA60, AtSetHeap3B1FB4}, {0x000FAD30, AtSetHeap3B1FB0}, {0x000FA930, AtSetHeap3B1FAC}, {0x000FA4F0, AtSetHeap3B1FA8},
        {0x000F98E0, AtSetHeap3B1FA4}, {0x000EB510, AtSetHeap3B13CC}, {0x000F7ED0, AtSetHeap3B1F98}, {0x000F7780, AtSetHeap3B1F90},
        {0x000F61C0, AtSetHeap3B1F70}, {0x000F58D0, AtSetHeap3B1F54}, {0x000EEF90, AtSetHeap3B13E0}, {0x000EE550, AtSetHeap3B13DC},
        {0x000ED5E0, AtSetHeap3B13D8}, {0x000EC6D0, AtSetHeap3B13D0},
        {0x000FE870, AtSetHeap3B1FCC}, {0x000F7BD0, AtSetHeap3B1F94}, {0x000F5FC0, AtSetHeap3B1F58},
    };
    for (const S& s : setters) {
        set_globals(1);
        run_pair("setter", [&] { at<Cfn1>(s.va)(TOKEN(9)); }, [&] { s.rec(TOKEN(9)); });
        compare_globals();
    }
    // Whole configs: the base's half (a member that does not read this), the free half, and the layer's, which runs both.
    struct C { const char* name; uint32_t va; bool member; void (*rec)(void*, const void*); };
    static const C sets[] = {
        {"AtGraphicsSystemBase::SetHeaps", 0x000C8240, true,
         [](void* s, const void* c) { ((AtGraphicsSystemBase*)s)->AtGraphicsSystemBase::SetHeaps((const AtGraphicsConfig*)c); }},
        {"AtGraphicsSetHeaps", 0x000C7CA0, false, [](void*, const void* c) { AtGraphicsSetHeaps((const AtGraphicsConfig*)c); }},
        {"AtGraphicsSystem::SetHeaps", 0x000B2F60, true,
         [](void* s, const void* c) { ((AtGraphicsSystem*)s)->AtGraphicsSystem::SetHeaps((const AtGraphicsConfig*)c); }},
    };
    for (const C& c : sets) {
        for (int pass = 0; pass < 40; pass++) {
            set_globals(pass < 2 ? pass : rnd() & 1);
            random_config(pass, rnd());
            if (pass >= 30) {  // each heap word alone, set against the default: a wrong field or a dropped call shows in one global
                for (int i = 3; i <= 52; i++) g_cfg[0][i] = g_cfg[1][i] = 0;
                for (int k = 0; k < 6; k++) {
                    const int i = 3 + (int)(rnd() % 50);
                    g_cfg[0][i] = g_cfg[1][i] = (uint32_t)(uintptr_t)TOKEN(1 + (i % 28));
                }
            }
            if (c.member) run_pair(c.name, [&] { at<Fn1>(c.va)(g_obj[0], nullptr, g_cfg[0]); }, [&] { c.rec(g_obj[1], g_cfg[1]); });
            else run_pair(c.name, [&] { at<Cfn1>(c.va)(g_cfg[0]); }, [&] { c.rec(nullptr, g_cfg[1]); });
            compare_globals();
            compare_config(c.name);
        }
    }
}

// ---- the function tables and the saved globals -------------------------------------------------------------------------------
static unsigned char g_table_buf[2][0x200];
static void run_tables() {
    for (int pass = 0; pass < 6; pass++) {
        fill_tables(pass);
        void* got[2];
        run_pair("AtMatrixTableGet", [&] { got[0] = at<void*(__cdecl*)()>(0x000140C0)(); }, [&] { got[1] = AtMatrixTableGet(); });
        eq("AtMatrixTableGet", rel_on(0, got[0]), rel_on(1, got[1]));
        run_pair("AtRenderTableGet", [&] { got[0] = at<void*(__cdecl*)()>(0x000455D0)(); }, [&] { got[1] = AtRenderTableGet(); });
        eq("AtRenderTableGet", rel_on(0, got[0]), rel_on(1, got[1]));
        // From a table elsewhere: it is copied whole.
        fill_matrix(g_table_buf[0], (AtMatrixTable*)g_table_buf[1], 100 + pass);
        run_pair("AtMatrixTableSet", [&] { at<Cfn1>(0x000140A0)(g_table_buf[0]); }, [&] { AtMatrixTableSet((AtMatrixTable*)g_table_buf[1]); });
        compare_tables("AtMatrixTableSet");
        fill_render(g_table_buf[0], (AtRenderTable*)g_table_buf[1], 200 + pass);
        run_pair("AtRenderTableSet", [&] { at<Cfn1>(0x000455B0)(g_table_buf[0]); }, [&] { AtRenderTableSet((AtRenderTable*)g_table_buf[1]); });
        compare_tables("AtRenderTableSet");
        // From the live tables themselves, as Restore does: the render table's matrix part goes over the matrix table.
        fill_tables(300 + pass);
        run_pair("AtRenderTableSet live", [&] { at<Cfn1>(0x000455B0)((void*)(uintptr_t)kRender); }, [&] { AtRenderTableSet(&g_atRender); });
        compare_tables("AtRenderTableSet live");
        fill_tables(400 + pass);
        run_pair("AtMatrixTableSet live", [&] { at<Cfn1>(0x000140A0)((void*)(uintptr_t)kMatrix); }, [&] { AtMatrixTableSet(&g_atMatrix); });
        compare_tables("AtMatrixTableSet live");
    }
}

static unsigned char g_glob[2][0x20];
static void run_globals() {
    struct Level { const char* name; uint32_t ctor, save, restore; uint32_t size; void (*make)(void*); void (*save_r)(void*); void (*restore_r)(void*); };
    static const Level levels[] = {
        {"AtMathGlobals", 0x00125000, 0x00125020, 0x00125040, 0x18, [](void* p) { new (p) AtMathGlobals(); },
         [](void* p) { ((AtMathGlobals*)p)->Save(); }, [](void* p) { ((AtMathGlobals*)p)->Restore(); }},
        {"AtDataGlobals", 0x0011E370, 0x0011E380, 0x0011E390, 0x18, [](void* p) { new (p) AtDataGlobals(); },
         [](void* p) { ((AtDataGlobals*)p)->Save(); }, [](void* p) { ((AtDataGlobals*)p)->Restore(); }},
        {"AtKinematicsGlobals", 0x00100F80, 0x00100F90, 0x00100FA0, 0x18, [](void* p) { new (p) AtKinematicsGlobals(); },
         [](void* p) { ((AtKinematicsGlobals*)p)->Save(); }, [](void* p) { ((AtKinematicsGlobals*)p)->Restore(); }},
        {"AtGraphicsGlobals", 0x000C8600, 0x000C8620, 0x000C8640, 0x1C, [](void* p) { new (p) AtGraphicsGlobals(); },
         [](void* p) { ((AtGraphicsGlobals*)p)->Save(); }, [](void* p) { ((AtGraphicsGlobals*)p)->Restore(); }},
    };
    for (const Level& l : levels) {
        for (int pass = 0; pass < 4; pass++) {
            std::string name = std::string(l.name) + "()";
            std::memset(g_glob, 0x66 + pass, sizeof g_glob);
            run_pair(name.c_str(), [&] { at<Fn0>(l.ctor)(g_glob[0], nullptr); }, [&] { l.make(g_glob[1]); });
            eq(name.c_str(), std::memcmp(g_glob[0], g_glob[1], sizeof g_glob[0]), 0);  // the memory globals' word +4, and +0x10, left as they were
            // Save: the memory globals, and where each table is.
            set_globals(pass);
            fill_tables(pass);
            name = std::string(l.name) + "::Save";
            run_pair(name.c_str(), [&] { at<Fn0>(l.save)(g_glob[0], nullptr); }, [&] { l.save_r(g_glob[1]); });
            for (uint32_t i = 0; i < 0x20; i += 4) eq(name.c_str(), rel_on(0, (void*)(uintptr_t)word_at(g_glob[0], i)), rel_on(1, (void*)(uintptr_t)word_at(g_glob[1], i)));
            compare_globals();
            compare_tables(name.c_str());
            // Restore: from the live tables as saved (passes 0, 1), or from tables elsewhere (2, 3), which shows which way each copy goes.
            set_globals(pass + 1);
            fill_tables(10 + pass);
            if (pass >= 2) {
                if (l.size == 0x1C) {
                    fill_render(g_table_buf[0], (AtRenderTable*)g_table_buf[1], 20 + pass);
                    word_at(g_glob[0], 0x18) = (uint32_t)(uintptr_t)g_table_buf[0];
                    word_at(g_glob[1], 0x18) = (uint32_t)(uintptr_t)g_table_buf[1];
                    word_at(g_glob[0], 0x14) = kMatrix;  // the matrix table as saved
                    word_at(g_glob[1], 0x14) = (uint32_t)(uintptr_t)&g_atMatrix;
                } else {
                    fill_matrix(g_table_buf[0], (AtMatrixTable*)g_table_buf[1], 20 + pass);
                    word_at(g_glob[0], 0x14) = (uint32_t)(uintptr_t)g_table_buf[0];
                    word_at(g_glob[1], 0x14) = (uint32_t)(uintptr_t)g_table_buf[1];
                }
            }
            if (pass == 1) for (int side = 0; side < 2; side++) word_at(g_glob[side], 4) = 0;  // no default heap: nothing pushed
            name = std::string(l.name) + "::Restore";
            run_pair(name.c_str(), [&] { at<Fn0>(l.restore)(g_glob[0], nullptr); }, [&] { l.restore_r(g_glob[1]); });
            compare_globals();
            compare_tables(name.c_str());
        }
    }
}

// ---- the layer -------------------------------------------------------------------------------------------------------------
static void make_layer(bool base) {
    std::memset(g_obj, 0xEE, sizeof g_obj);
    if (base) run_pair("AtGraphicsSystemBase()", [&] { at<Fn0>(0x000C81B0)(g_obj[0], nullptr); }, [&] { new (g_obj[1]) AtGraphicsSystemBase(); });
    else run_pair("AtGraphicsSystem()", [&] { at<Fn0>(0x000B2F80)(g_obj[0], nullptr); }, [&] { new (g_obj[1]) AtGraphicsSystem(); });
    eq("graphics vftable", *(uint32_t*)g_obj[0], base ? 0x00286608 : 0x0028478C);
    compare_object(base ? "base constructed" : "constructed", base ? 0x30 : 0xA4);
    for (int side = 0; side < 2; side++) *(const void**)g_obj[side] = base ? g_base_table : g_layer_table;
}

// Puts the layer in a state: running or not, with a device and a factory or not, the kind given.
static void set_state(uint32_t flags, bool device, bool factory, uint32_t kind) {
    for (int side = 0; side < 2; side++) {
        AtGraphicsSystem* g = (AtGraphicsSystem*)g_obj[side];
        g->m_flags = flags;
        g->m_materialOwner = device ? (AtMaterialOwner*)&g_owner : nullptr;
        g->m_factory = factory ? (AtGraphicsFactory*)&g_factory : nullptr;
        g->m_kind = (int)kind;
        g->m_below = (void*)TOKEN(6);
        g->m_format.m_flags = 3;
        g->m_format.m_mask2 = 0x1234;
        for (uint32_t i = 0x34; i < 0x54; i += 4) word_at(g, i) = 0x100 + i;
        for (uint32_t i = 0x8C; i < 0x98; i += 4) word_at(g, i) = 0x200 + i;
        g->m_unknown24 = 0x24;
        g->m_unknown20 = 0x20;
        g->m_unknown2C = 0x2C;
        g->m_unknown04 = 0x04;
    }
}

static void run_layer() {
    static const uint32_t kinds[] = {0, 1, 2, 5};
    for (int pass = 0; pass < 32; pass++) {
        const bool base = pass % 8 == 7;
        set_globals(pass & 1);
        fill_tables(pass);
        make_layer(base);
        // Running (bit 0 alone, or other bits without it), with or without what Shutdown hands back.
        const uint32_t flags = (pass / 2) % 4 == 0 ? 0 : (pass / 2) % 4 == 1 ? 1 : (pass / 2) % 4 == 2 ? 0xE : 0xF;
        set_state(flags, pass % 3 != 0, pass % 5 != 0, kinds[(pass / 3) % 4]);
        random_config(pass, kinds[pass % 4]);
        g_start_returns = &g_factory;
        if (base) {
            run_pair("AtGraphicsSystemBase::Init", [&] { at<Fn3>(0x000C81E0)(g_obj[0], nullptr, g_cfg[0], TOKEN(5), TOKEN(4)); },
                     [&] { ((AtGraphicsSystemBase*)g_obj[1])->AtGraphicsSystemBase::Init((AtGraphicsConfig*)g_cfg[1], TOKEN(5), TOKEN(4)); });
        } else {
            run_pair("AtGraphicsSystem::Init", [&] { at<Fn3>(0x000B3000)(g_obj[0], nullptr, g_cfg[0], TOKEN(5), TOKEN(4)); },
                     [&] { ((AtGraphicsSystem*)g_obj[1])->AtGraphicsSystem::Init((AtGraphicsConfig*)g_cfg[1], TOKEN(5), TOKEN(4)); });
        }
        compare_object(base ? "base Init" : "Init", base ? 0x30 : 0xA4);
        compare_config(base ? "base Init config" : "Init config");
        compare_globals();
        compare_tables(base ? "base Init" : "Init");
        // Shutdown alone, from a state of its own.
        set_state(pass & 1, pass % 4 < 2, pass % 3 == 0, kinds[(pass + 1) % 4]);
        if (base) run_pair("AtGraphicsSystemBase::Shutdown", [&] { at<Fn0>(0x000C8210)(g_obj[0], nullptr); },
                           [&] { ((AtGraphicsSystemBase*)g_obj[1])->AtGraphicsSystemBase::Shutdown(); });
        else run_pair("AtGraphicsSystem::Shutdown", [&] { at<Fn0>(0x000B30B0)(g_obj[0], nullptr); },
                      [&] { ((AtGraphicsSystem*)g_obj[1])->AtGraphicsSystem::Shutdown(); });
        compare_object(base ? "base Shutdown" : "Shutdown", base ? 0x30 : 0xA4);
        // Slot 7.
        if (!base) {
            for (int side = 0; side < 2; side++) {
                ((AtGraphicsSystem*)g_obj[side])->m_unknown38 = (pass & 1) ? 0 : 0x38 + pass;
                ((AtGraphicsSystem*)g_obj[side])->m_unknown3C = (pass & 2) ? 0 : 0x3C + pass;
            }
            int got[2];
            run_pair("AtGraphicsSystem::Slot7", [&] { got[0] = at<int(__fastcall*)(void*, void*)>(0x000B2F50)(g_obj[0], nullptr); },
                     [&] { got[1] = ((AtGraphicsSystem*)g_obj[1])->AtGraphicsSystem::Slot7(); });
            eq("Slot7", got[0], got[1]);
        }
    }
    // The display mode's constructor alone.
    std::memset(g_obj, 0x31, sizeof g_obj);
    run_pair("AtDisplayMode()", [&] { at<Fn0>(0x00043480)(g_obj[0], nullptr); }, [&] { new (g_obj[1]) AtDisplayMode(); });
    eq("display mode", std::memcmp(g_obj[0], g_obj[1], 0x28), 0);
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
    if (pos.size() < 1) { std::printf("usage: oracle_grlayer <default.xbe> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    at<void(__cdecl*)()>(0x00125BC7)();

    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    redirect(0x000C7C60, (const void*)o_start);
    redirect(0x000C7C90, (const void*)o_stop);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    if (entries && !arm_entries(entries)) return 2;

    BadFill<1, 33>::run(g_factory_table);
    g_factory_table[0] = (const void*)d_factory_slot0;
    g_factory_table[3] = (const void*)d_factory_slot3;
    g_factory_table[4] = (const void*)d_factory_slot4;
    BadFill<2, 17>::run(g_owner_table);
    g_owner_table[1] = (const void*)d_owner_slot1;
    BadFill<3, 7>::run(g_layer_table);
    g_layer_table[2] = (const void*)d_shutdown;
    g_layer_table[3] = (const void*)d_set_heaps;
    BadFill<4, 7>::run(g_base_table);
    g_base_table[2] = (const void*)d_base_shutdown;

    for (int side = 0; side < 2; side++) {
        add_region(side, g_obj[side], sizeof g_obj[side], kObjTag);
        add_region(side, g_cfg[side], sizeof g_cfg[side], kCfgTag);
        add_region(side, g_table_buf[side], sizeof g_table_buf[side], 0x73000000u);
    }
    add_region(0, (void*)(uintptr_t)kMatrix, 0x1A0, 0x74000000u);
    add_region(1, &g_atMatrix, sizeof g_atMatrix, 0x74000000u);
    add_region(0, (void*)(uintptr_t)kRender, 0x1BC, 0x75000000u);
    add_region(1, &g_atRender, sizeof g_atRender, 0x75000000u);

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_setters();
    run_tables();
    run_globals();
    run_layer();
    report_entries();
    if (g_bad) { std::printf("a slot no case should reach was reached %d times\n", g_bad); g_failures++; }
    std::printf("%s: %d crashes on both sides; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", (int)g_crashes, g_checks,
                g_failures);
    return g_failures ? 1 : 0;
}
