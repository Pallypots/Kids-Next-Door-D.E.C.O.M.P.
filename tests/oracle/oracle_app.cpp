// Differential oracle for App: main, Init, the main loop, GmApp's and the atlas layer's start-up, and App's small slots (Windows x86, local only). See docs/verification.md.
//
//   oracle_app <default.xbe> [--entries FILE] [--mutate]
//
// No game file is read: start-up's files are reached through members already recovered and checked elsewhere, which are doubles here.
//
// What stands in for what, first. Every function App reaches that is not recovered here is a logging double on both sides: the original's call is redirected, and the
// recovered side's seam (src/gm/GmAppHost.cpp, not linked here) is defined below to call the same double. So are these recovered members of other subsystems, already
// checked by their own suites, whose recovered entry is patched in memory to jump to the double: AtFormat, AtHeapManager's Report, CreateHeap, MakeEquivalent and DestroyHeap,
// GmAppSettings's Load and ReadCommandLine, the mount manager's Mount, Unmount, OpenListFile and CloseListFile, AtDatabaseLoader::Load, AtDataRegistry::Load,
// AtDataLibrary::Load, GmManifestLoader's Configure and Update, and AtIoStream::SetHeap. The objects App calls through vftables (the device, graphics, the atlas layer,
// the game state, the frameworks, the derived mount manager, script modules, the space manager's device, and App's own vftable) get tables of doubles, the same on both
// sides. What runs for real on both sides: what is recovered here, and the settings' constructor, the level manifest's constructor, AtCopyName8/9, AtIoContext::FindFile,
// AtIoPath's Push and Pop, GmNameMap::Init and the C library's memcpy, each the original's code on the original side and recovered code on the other.
// So a green result shows App's own logic agrees, call by call and byte by byte over the whole App object and the globals it writes; it says nothing about the doubles'
// real counterparts.
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtDatabaseLoader.h"
#include "gm/GmApp.h"
#include "gm/GmAudioData.h"
#include "gm/GmLevelManager.h"
#include "gm/GmLevelManifest.h"
#include "gm/GmManifestLoader.h"
#include "oracle_guard.h"
#include "oracle_image.h"

int AtFormat(char* out, const char* format, ...);  // src/at/AtIoFormatHost.cpp

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

// Heap requests fail from this push on, counted per side: every allocation here is preceded by a push.
static int g_pushes[2];
static int g_fail_at_push = -1;
static void count_push() {
    if (g_recording && g_pushes[g_side]++ == g_fail_at_push) g_fail_alloc = true;
}
void AtPushHeap(void* h) { count_push(); AtPushHeapInDoubles(h); }
static void __cdecl s_push(void* h) { count_push(); o_push(h); }

static uint32_t& orig_u32(uint32_t va) { return *(uint32_t*)(uintptr_t)va; }
template <class T> static void* code_of(T f) {
    static_assert(sizeof(T) == sizeof(void*), "a member pointer of more than one word");
    void* p;
    std::memcpy(&p, &f, sizeof p);
    return p;
}

// A hook on a code address, which can be taken off again.
struct Hook {
    uint32_t va = 0;
    unsigned char saved[5];
    void set(const void* where, const void* to) {
        va = (uint32_t)(uintptr_t)where;
        std::memcpy(saved, where, 5);
        redirect(va, to);
        FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    }
    void unset() {
        std::memcpy((void*)(uintptr_t)va, saved, 5);
        FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    }
};
static void patch(const void* where, const void* to) { Hook h; h.set(where, to); }

// ---- the two App objects ----------------------------------------------------------------------------------------------------
static const unsigned kAppSize = 0x6D4C;
static unsigned char* g_base[2];
static App* rec_app() { return (App*)g_base[1]; }
static void* orig_app() { return g_base[0]; }
static uint32_t rel(const void* p) {
    const unsigned char* b = g_base[g_side];
    if (p && (const unsigned char*)p >= b && (const unsigned char*)p < b + kAppSize) return 0x70000000u + (uint32_t)((const unsigned char*)p - b);
    return (uint32_t)(uintptr_t)p;
}
static int g_bad_seams;  // a slot or seam no case should reach; reported, and must stay 0
static void L(uint32_t tag, uint32_t v = 0) {
    if (tag == 'BAD') g_bad_seams++;
    if (g_recording) g_log[g_side].add(tag, v);
}
static void Ls(uint32_t tag, const char* s) {
    if (!g_recording) return;
    uint32_t h = 2166136261u;
    for (const char* c = s; c && *c; c++) h = (h ^ (unsigned char)*c) * 16777619u;
    g_log[g_side].add(tag, s ? h : 0xFFFFFFFF);
    record_message(s ? s : "(null)", g_side);
}

// Tokens: addresses both sides share, for objects only doubles look at.
static unsigned char g_tokens[17][64];
#define TOKEN(i) (g_tokens[i])
static AtHeapManager* g_heap_manager;  // one for both sides

// ---- doubles of free functions ----------------------------------------------------------------------------------------------
static int __cdecl d_language() { L('lang'); return 1; }
static int __cdecl d_region() { L('regn'); return 1; }
int AtPlatformLanguage() { return d_language(); }
int AtPlatformGameRegion() { return d_region(); }

static unsigned g_ms[2], g_ms_step = 7;
static unsigned __cdecl d_ms() { L('ms'); return g_ms[g_side] += g_ms_step; }
unsigned int AtPlatformMilliseconds() { return d_ms(); }

static unsigned g_mem_total, g_mem_avail;
static void __stdcall d_memstatus(AtSdkMemoryStatus* s) {
    L('mem');
    s->m_length = 0x20;
    s->m_memoryLoad = 1;
    s->m_totalPhysical = g_mem_total;
    s->m_availablePhysical = g_mem_avail;
    s->m_totalPageFile = s->m_availablePageFile = s->m_totalVirtual = s->m_availableVirtual = 0x11111111;
}
void AtSdkGlobalMemoryStatus(AtSdkMemoryStatus* s) { d_memstatus(s); }

static int __cdecl d_format(char* out, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int n = vsprintf(out, format, args);
    va_end(args);
    Ls('fmt', out);
    return n;
}

static void __fastcall d_report(void* self, void*, char* out, unsigned capacity) { L('rep', self == g_heap_manager); L('repc', capacity); Ls('rept', out); }
static unsigned g_heap_next[2];
static void* __fastcall d_create_heap(void* self, void*, unsigned size, int align) {
    L('crh', self == g_heap_manager); L('crhs', size); L('crha', align);
    return (void*)(uintptr_t)(0x40000000u + 0x100 * ++g_heap_next[g_side]);
}
static void __fastcall d_equiv(void* self, void*, void* a, void* b) { L('eqv', self == g_heap_manager); L('eqva', (uint32_t)(uintptr_t)a); L('eqvb', (uint32_t)(uintptr_t)b); }
static void __fastcall d_destroy_heap(void* self, void*, void* h) { L('dsh', self == g_heap_manager); L('dshh', (uint32_t)(uintptr_t)h); }

// The command line's double sets what Init tests afterwards, as App.ini and the command line would.
static int g_open_list, g_test_all;
static const char* g_start_level = "ALMOND";
static const char* g_data_folder = "DataXbox";
static void __fastcall d_settings_load(void* self, void*) {
    L('sld', rel(self));
    GmAppSettings* s = (GmAppSettings*)self;
    for (int i = 0; i < 8; i++) s->m_dataFolder[i] = 0;
    for (int i = 0; i < 8 && g_data_folder[i]; i++) s->m_dataFolder[i] = g_data_folder[i];
}
static void __fastcall d_cmdline(void* self, void*) {
    L('cmd', rel(self));
    GmAppSettings* s = (GmAppSettings*)self;
    s->m_openList = g_open_list;
    s->m_testAllLevels = g_test_all;
    for (int i = 0; i < 8; i++) s->m_level[i] = 0;
    for (int i = 0; i < 8 && g_start_level[i]; i++) s->m_level[i] = g_start_level[i];
}

static void __fastcall d_mount(void* self, void*, const char* name, const char* dir) { L('mnt', self == TOKEN(1)); Ls('mntn', name); Ls('mntd', dir); }
static void __fastcall d_unmount(void* self, void*) { L('umnt', self == TOKEN(1)); }
static void __fastcall d_open_list(void* self, void*, const char* name) { L('olst', self == TOKEN(1)); Ls('olsn', name); }
static void __fastcall d_close_list(void* self, void*) { L('clst', self == TOKEN(1)); }

static void __fastcall d_display(void* self, void*) { L('disp', rel(self)); }
static void __fastcall d_matrix(void* self, void*) { L('mtx', rel(self)); }
static void __fastcall d_subsystems(void* self, void*) { L('subs', rel(self)); }
static void __fastcall d_ui(void* self, void*) { L('ui', rel(self)); }
static void __fastcall d_692c_init(void* self, void*) { L('69i', self == TOKEN(2)); }
static int g_clock_time = 0x1234;
static void __fastcall d_692c_update(void* self, void*, int time) { L('69u', self == TOKEN(2)); L('69ut', time); }
static void __fastcall d_volumes(void* self, void*) { L('vol', self == TOKEN(3)); }
static void __fastcall d_97eb0(void* self, void*) { L('97e', (uint32_t)((char*)self - (char*)TOKEN(4))); }
static void __cdecl d_string(char* out, unsigned size, const char* name) {
    Ls('str', name); L('strz', size);
    for (unsigned i = 0; i < size; i++) out[i] = (char)('A' + (i * 7 + name[0]) % 26);  // no terminator: what follows must not depend on one
}
static void __fastcall d_scripts_init(void* self, void*, void* heap) { L('sci', self == TOKEN(5)); L('scih', (uint32_t)(uintptr_t)heap); }
static void* g_modules[2];
static void* __fastcall d_find_module(void* self, void*, const char* name) {
    L('fmod', self == TOKEN(5)); Ls('fmon', name);
    return !std::strcmp(name, "Control") ? g_modules[0] : !std::strcmp(name, "SaveData") ? g_modules[1] : nullptr;
}
static void __cdecl d_cb_app() { L('cba'); }
static void __cdecl d_cb_control() { L('cbc'); }
static void __cdecl d_cb_save() { L('cbs'); }
static void* __fastcall d_space_add(void* self, void*, void* handle, unsigned size, int flag) {
    L('spa', self == TOKEN(6)); L('spah', (uint32_t)(uintptr_t)handle); L('spas', size); L('spaf', flag);
    return TOKEN(8);
}
static int g_update_after = 1, g_updates[2];
static void __fastcall d_configure(void* self, void*, const GmManifestRequest* r) {
    L('cfg', self != nullptr);
    char name[9] = {0};
    std::memcpy(name, r->m_name, 8);
    Ls('cfgn', name); Ls('cfgp', r->m_path); L('cfgh', (uint32_t)(uintptr_t)r->m_heap);
}
static int __fastcall d_update(void* self, void*, unsigned budget) { L('upd', self != nullptr); L('updb', budget); return ++g_updates[g_side] >= g_update_after; }
static void __fastcall d_savedata(void* self, void*) { L('sav', rel(self)); }
static void __fastcall d_loader_load(void* self, void*, void* owner, const char* name, int mode, const char* prefix, int in_directory) {
    L('ldl', self == TOKEN(9)); L('ldlo', (uint32_t)(uintptr_t)owner); Ls('ldln', name); L('ldlm', mode); L('ldlp', prefix == nullptr); L('ldld', in_directory);
    L('ldlh', (uint32_t)(uintptr_t)g_heap_manager->m_override);
}
static int g_registry_result;
static int __fastcall d_registry_load(void* self, void*, void* owner, const char* name, int unused, char* space, void* parent) {
    L('rgl', rel(self)); L('rglo', (uint32_t)((char*)owner - (char*)TOKEN(10))); Ls('rgln', name); L('rglu', unused); L('rgls', space == nullptr); L('rglp', rel(parent));
    return g_registry_result;
}
static void __fastcall d_library_load(void* self, void*, void* owner, const char* name, int unused, void* parent) {
    L('lbl', rel(self)); L('lblo', (uint32_t)((char*)owner - (char*)TOKEN(10))); Ls('lbln', name); L('lblu', unused); L('lblp', rel(parent));
}
static int __fastcall d_attach(void* self, void*, void* parent, const char* path) { L('att', rel(self)); L('attp', rel(parent)); L('attn', path == nullptr); return 1; }
static void __fastcall d_prepare(void* self, void*, void* owner, int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m) {
    L('prp', rel(self)); L('prpo', (uint32_t)((char*)owner - (char*)TOKEN(10)));
    L('prpz', (a | b | c | d | e | f | g | h | i | j | k | l | m) == 0);
}
static void __fastcall d_set_name(void* self, void*, const char* name) { L('snm', rel(self)); Ls('snmn', name); }
static void* __fastcall d_find_path(void* self, void*, void* from, const char* path, int deep) {
    L('fpt', rel(self)); L('fptf', rel(from)); Ls('fptp', path); L('fptd', deep);
    return TOKEN(11);
}
static void __fastcall d_names_add(void* self, void*, void* registry) { L('nma', self == TOKEN(12)); L('nmar', rel(registry)); }
static void* g_fake_mount;
static void* __fastcall d_mount_ctor(void* self, void*) { L('mmc', self != nullptr); return g_fake_mount; }
static void* __fastcall d_logs_ctor(void* self, void*) { L('lgc', self != nullptr); return TOKEN(13); }
static void __fastcall d_logs_init(void* self, void*, const AppLogParams* p) {
    L('lgi', (uint32_t)(uintptr_t)self); L('lgia', rel(p->m_app)); L('lgih', (uint32_t)(uintptr_t)p->m_heap); L('lgi6', rel(p->m_app60E8)); L('lgir', (uint32_t)(uintptr_t)p->m_root);
    L('lgi1', p->m_unknown14); L('lgi2', p->m_unknown18); L('lgi3', p->m_unknown1C);
}
static void __cdecl d_stream_heap(void* heap) { L('sth', (uint32_t)(uintptr_t)heap); }
static void* g_frameworks[3];
static void* __fastcall d_fw0(void* self, void*) { L('fw0', self != nullptr); return g_frameworks[0]; }
static void* __fastcall d_fw1(void* self, void*) { L('fw1', self != nullptr); return g_frameworks[1]; }
static void* __fastcall d_fw2(void* self, void*) { L('fw2', self != nullptr); return g_frameworks[2]; }
static void __fastcall d_viewport(void* self, void*, int w, int h) { L('vpt', self == TOKEN(14)); L('vptw', w); L('vpth', h); }
static void __fastcall d_tick(void* self, void*, int running) { L('tik', self == TOKEN(15)); L('tikr', running); }
static void __cdecl d_90580() { L('905'); }
static int g_script_steps;
static int g_script_left[2];
static int __cdecl d_run_scripts(int frame) {
    L('rsc', frame);
    L('rscg', *(int*)(g_side == 0 ? (void*)(uintptr_t)0x003B361C : (void*)&g_inScripts));
    return g_script_left[g_side]-- > 0;
}
static void __fastcall d_present(void* self, void*, int a) { L('prs', rel(self)); L('prsa', a); }

// Main's three calls.
static void __fastcall d_init(void* self, void*, int a, int b) { L('INI', g_side == 0 ? self == (void*)(uintptr_t)0x002BF310 : self == &g_app); L('INIa', a); L('INIb', b); }
static void __fastcall d_run(void* self, void*) { L('RUN', g_side == 0 ? self == (void*)(uintptr_t)0x002BF310 : self == &g_app); }

// ---- tables of doubles for the objects App calls through vftables ----------------------------------------------------------
template <int Table, int Slot> static void __fastcall d_bad(void*, void*) { L('BAD', Table * 100 + Slot); }  // see g_bad_seams
#define BAD(T, S) (const void*)&d_bad<T, S>
static const void* g_bad_table[8][70];
template <int T, int S> struct BadFill {
    static void run() { g_bad_table[T][S] = BAD(T, S); BadFill<T, S - 1>::run(); }
};
template <int T> struct BadFill<T, -1> {
    static void run() {}
};

// App's vftable: 6 Shutdown, 60 GetAtlasConfig, 61 AfterAtlas and 64 GetHeap are reached; anything else is reported as BAD.
static void __fastcall d_app_shutdown(void* self, void*) { L('SHD', self == (void*)(uintptr_t)0x002BF310 || self == &g_app ? 0x002BF310u : rel(self)); }
static void __fastcall d_app_atlas_config(void* self, void*, AppAtlasConfig* c) {
    L('acf', rel(self));
    const uint32_t* w = (const uint32_t*)c;
    uint32_t sum = 0;
    for (int i = 0; i < 12; i++) sum = sum * 31 + w[i];
    L('acfw', sum);  // what the caller's config held: zeroes
    for (int i = 0; i < 12; i++) ((uint32_t*)c)[i] = 0x100 + i;
}
static void __fastcall d_app_after_atlas(void* self, void*) { L('aft', rel(self)); }
static void* __fastcall d_app_get_heap(void* self, void*, unsigned index) { L('ghp', rel(self)); L('ghpi', index); return TOKEN(7); }
void App::Shutdown() { d_app_shutdown(this, nullptr); }
static const void* g_app_table[69];

static int g_frames_left[2];
static unsigned g_flag_at_frame = 0xFFFFFFFF, g_quit_at_frame = 0xFFFFFFFF, g_3ada50_at_frame = 0xFFFFFFFF;
static unsigned g_frame[2];
static int __fastcall d_run_frame(void* self, void*) {
    L('frm', rel(self));
    return g_frames_left[g_side]-- > 0;
}
static const void* g_device_table[7];
static AtDatabaseLoader* __fastcall d_db_loader(void* self, void*) { L('dbl', self == TOKEN(16)); return (AtDatabaseLoader*)TOKEN(9); }
static const void* g_graphics_table[30];
static void __fastcall d_atlas_init(void* self, void*, const AppAtlasConfig* c, void* ui, void* app04) {
    L('ati', rel(self));
    for (int i = 0; i < 12; i++) L('atiw', ((const uint32_t*)c)[i]);
    L('atiu', rel(ui)); L('ati4', rel(app04));
}
static const void* g_atlas_table[2];
static void __fastcall d_gs1(void* self, void*) { L('gs1', rel(self)); }
static void __fastcall d_gs3(void* self, void*) { L('gs3', rel(self)); }
static void __fastcall d_gs_registry(void* self, void*, void* registry) { L('gsr', rel(self)); L('gsrr', rel(registry)); }
static const void* g_state_table[8];

// The frameworks: Enter records its index, as the real ones would need to for the loop's table to mean anything; slot 3 runs a script of requests per frame.
struct FakeFramework {
    const void* const* vftable;
    void* connections;
    int m_index;
    int m_request;
};
static std::vector<int> g_requests;  // for each frame, the request the current framework makes; -1 none
static void* g_side_app() { return g_base[g_side]; }
static void __fastcall d_fw_enter(void* self, void*, void* app, int index) {
    L('ent', (uint32_t)((FakeFramework*)self - (FakeFramework*)g_frameworks[0])); L('enta', rel(app)); L('enti', index);
    ((FakeFramework*)self)->m_index = index;
}
static void __fastcall d_fw_leave(void* self, void*) { L('lev', (uint32_t)((FakeFramework*)self - (FakeFramework*)g_frameworks[0])); }
static void __fastcall d_fw_slot2(void* self, void*) { L('fs2', (uint32_t)((FakeFramework*)self - (FakeFramework*)g_frameworks[0])); }
static void __fastcall d_fw_slot3(void* self, void*) {
    FakeFramework* f = (FakeFramework*)self;
    L('fs3', (uint32_t)(f - (FakeFramework*)g_frameworks[0]));
    const unsigned frame = g_frame[g_side]++;
    if (frame < g_requests.size() && g_requests[frame] >= 0) f->m_request = g_requests[frame];
    unsigned char* app = (unsigned char*)g_side_app();
    if (frame == g_quit_at_frame) *(int*)(app + 0x6518) = 1;
    if (frame == g_flag_at_frame) *(unsigned*)(app + 0x5F88) |= 8;
    if (frame == g_3ada50_at_frame) *(int*)(g_side == 0 ? (void*)(uintptr_t)0x003ADA50 : (void*)&g_unknown3ADA50) = 1;
}
static const void* g_framework_table[7];

static void __fastcall d_mm_init(void* self, void*, const AtIoMountParams* p) {
    L('mmi', self == g_fake_mount); L('mmio', rel(p->m_owner)); L('mmih', (uint32_t)(uintptr_t)p->m_heap); L('mmic', rel(p->m_context));
}
// An eight-character name: Init hands over its own eight bytes, unterminated when full, so only those are looked at.
static void __fastcall d_mm_region(void* self, void*, const char* name) {
    L('mmr', self == g_fake_mount);
    char text[9] = {0};
    for (int i = 0; i < 8 && name[i]; i++) text[i] = name[i];
    Ls('mmrn', text);
}
static const void* g_mount_table[7];
static void __fastcall d_module_enable(void* self, void*, int on) { L('men', self == g_modules[0] ? 1 : self == g_modules[1] ? 2 : 0); L('meno', on); }
static const void* g_module_table[7];
static void* __fastcall d_space_slot2(void* self, void*, int a, int b) { L('sp2', self == TOKEN(6) + 32); L('sp2a', a); L('sp2b', b); return TOKEN(7) + 8; }
static const void* g_space_device_table[3];

// ---- the recovered side's seams, defined here so that src/gm/GmAppHost.cpp is not linked ------------------------------------
void App::DeletingDestructor(unsigned int) { L('BAD', 9004); }
// The layers' config slots and start-ups are at/atap's, checked by its own suite (oracle_atap); src/gm/GmAppLayers.cpp is not linked here either.
void App::GetIoConfig(AtIoConfig*) { L('BAD', 9015); }
void App::GetMathConfig(AppMathConfig*) { L('BAD', 9019); }
void App::GetDataManagementConfig(AppDataManagementConfig*) { L('BAD', 9023); }
void App::GetScriptConfig(AppScriptConfig*) { L('BAD', 9027); }
void App::GetKinematicsConfig(AppKinematicsConfig*) { L('BAD', 9031); }
void App::GetGraphicsConfig(AppGraphicsConfig*) { L('BAD', 9035); }
void App::Present(int a) { d_present(this, nullptr, a); }
void App::GetControllerConfig(AppControllerConfig*) { L('BAD', 9040); }
void App::GetSoundConfig(AppSoundConfig*) { L('BAD', 9044); }
void App::GetSceneConfig(AppSceneConfig*) { L('BAD', 9052); }
void App::GetUserInterfaceConfig(AppUserInterfaceConfig*) { L('BAD', 9056); }
void App::Slot65() { L('BAD', 9065); }
void App::Slot66() { L('BAD', 9066); }
void App::StartUpUserInterface() { d_ui(this, nullptr); }
void App::SetDisplayMode() { d_display(this, nullptr); }
void App::ReadMatrixDevice() { d_matrix(this, nullptr); }
void App::CreateSubsystems() { d_subsystems(this, nullptr); }
void AppDevice::Slot0() { L('BAD', 9100); }
void AppDevice::Init(const void*, void*, void*) { L('BAD', 9101); }
#define LAYER3(C, n) void C::Slot0() { L('BAD', n); } void C::Init(const void*, void*, void*) { L('BAD', n + 1); }
#define LAYER2(C, n) void C::Slot0() { L('BAD', n); } void C::Init(const void*, void*) { L('BAD', n + 1); }
LAYER3(AppMathLayer, 9700) LAYER3(AppDataManagementLayer, 9702) LAYER3(AppScriptLayer, 9704) LAYER3(AppKinematicsLayer, 9706) LAYER3(AppControllerLayer, 9708)
LAYER2(AppSoundLayer, 9710) LAYER3(AppVideoLayer, 9712) LAYER3(AppSceneLayer, 9714) LAYER2(AppUserInterfaceLayer, 9716)
#undef LAYER3
#undef LAYER2
// App's constructor (g_app, in src/gm/GmMain.cpp) builds the memory system, which needs these; no case reaches them.
void* AtPlatformAllocateRegion(unsigned int) { L('BAD', 9800); return nullptr; }
void AtPlatformFreeRegion(void*) { L('BAD', 9801); }
void AtPlatformInitDevices() { L('BAD', 9802); }
// And the heap stack's query, so that src/at/AtMemory.cpp (whose push and pop are doubled here) is not linked: the doubles never push.
AtHeap* AtCurrentHeap() { return nullptr; }
void AppDevice::Slot2() { L('BAD', 9102); }
void AppDevice::Slot3() { L('BAD', 9103); }
void AppDevice::Slot4() { L('BAD', 9104); }
void AppDevice::Slot5() { L('BAD', 9105); }
int AppDevice::RunFrame() { L('BAD', 9106); return 0; }
#define GSLOT(n) void AppGraphics::Slot##n() { L('BAD', 9200 + n); }
GSLOT(0) GSLOT(1) GSLOT(2) GSLOT(3) GSLOT(4) GSLOT(5) GSLOT(6) GSLOT(7) GSLOT(8) GSLOT(9) GSLOT(10) GSLOT(11) GSLOT(12) GSLOT(13) GSLOT(14) GSLOT(15) GSLOT(16) GSLOT(17)
GSLOT(18) GSLOT(19) GSLOT(20) GSLOT(21) GSLOT(22) GSLOT(23) GSLOT(24) GSLOT(25) GSLOT(26) GSLOT(27) GSLOT(28)
AtDatabaseLoader* AppGraphics::DatabaseLoader() { L('BAD', 9229); return 0; }
void AppGraphics::DeleteLoader(AtDatabaseLoader*) { L('BAD', 9230); }
void AppAtlasLayer::Slot0() { L('BAD', 9300); }
void AppAtlasLayer::Init(const AppAtlasConfig*, void*, void*) { L('BAD', 9301); }
void AppViewport::SetScreenSize(int w, int h) { d_viewport(this, nullptr, w, h); }
void FaAcPlayerGameStateData::Slot0() { L('BAD', 9400); }
void FaAcPlayerGameStateData::Slot1() { L('BAD', 9401); }
void FaAcPlayerGameStateData::Slot2() { L('BAD', 9402); }
void FaAcPlayerGameStateData::Slot3() { L('BAD', 9403); }
void FaAcPlayerGameStateData::Slot4() { L('BAD', 9404); }
void FaAcPlayerGameStateData::SetRegistry(AtDataRegistry*) { L('BAD', 9405); }
void GmMoFramework::Enter(App*, int) { L('BAD', 9501); }
void GmMoFramework::Slot2() { L('BAD', 9502); }
void GmMoFramework::Slot3() { L('BAD', 9503); }
void GmMoFramework::Slot4() { L('BAD', 9504); }
void GmMoFramework::Leave() { L('BAD', 9505); }
void GmMoFramework::Slot6() { L('BAD', 9506); }
GmMoFramework* FaMoIntroFrameworkConstruct(void* block) { return (GmMoFramework*)d_fw0(block, nullptr); }
GmMoFramework* FaMoFlashMainMenuFrameworkConstruct(void* block) { return (GmMoFramework*)d_fw1(block, nullptr); }
GmMoFramework* FaMoGameFrameworkConstruct(void* block) { return (GmMoFramework*)d_fw2(block, nullptr); }
AtIoMountManager* AppMountManagerConstruct(void* block) { return (AtIoMountManager*)d_mount_ctor(block, nullptr); }
GmHistoryLogs* GmHistoryLogsConstruct(void* block) { return (GmHistoryLogs*)d_logs_ctor(block, nullptr); }
void GmHistoryLogsInit(GmHistoryLogs* logs, const AppLogParams* p) { d_logs_init(logs, nullptr, p); }
GmScripts* GmScripts::s_instance;
void GmScriptModule::Slot0() { L('BAD', 9600); }
void GmScriptModule::Slot1() { L('BAD', 9601); }
void GmScriptModule::Slot2() { L('BAD', 9602); }
void GmScriptModule::Slot3() { L('BAD', 9603); }
void GmScriptModule::Slot4() { L('BAD', 9604); }
void GmScriptModule::Slot5() { L('BAD', 9605); }
void GmScriptModule::SetEnabled(int) { L('BAD', 9606); }
void GmScripts::Init(AtHeap* heap) { d_scripts_init(this, nullptr, heap); }
GmScriptModule* GmScripts::FindModule(const char* name) { return (GmScriptModule*)d_find_module(this, nullptr, name); }
void GmRegisterAppCallbacks() { d_cb_app(); }
void GmRegisterControlCallbacks() { d_cb_control(); }
void GmRegisterSaveDataCallbacks() { d_cb_save(); }
int GmRunScripts(int frame) { return d_run_scripts(frame); }
void GmGetStringByName(char* out, unsigned int size, const char* name) { d_string(out, size, name); }
GmClock* GmClock::s_instance;
void GmClock::Tick(int running) { d_tick(this, nullptr, running); }
void AppUnknown692C::Init() { d_692c_init(this, nullptr); }
void AppUnknown692C::Update(int time) { d_692c_update(this, nullptr, time); }
GmDataRoot* GmDataRoot::s_instance;
GmRegistryUser* GmRegistryUser::s_instance;
GmRegistryNames* GmRegistryNames::s_instance;
int AtDataRegistry::AttachUnder(AtDataRegistry* parent, const char* path) { return d_attach(this, nullptr, parent, path); }
int AtDataRegistry::Prepare(AtDataRegistryOwner* owner, unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int e, unsigned int f, unsigned int g,
                            unsigned int h, unsigned int i, unsigned int j, unsigned int k, unsigned int l, AtDataRegistry* parent) {
    d_prepare(this, nullptr, owner, (int)a, (int)b, (int)c, (int)d, (int)e, (int)f, (int)g, (int)h, (int)i, (int)j, (int)k, (int)l, (int)(uintptr_t)parent);
    return 0;
}
void AtDataRegistry::SetFileName(const char* name) { d_set_name(this, nullptr, name); }
AtDataLink* AtDataRegistry::FindPath(AtDataLink* from, const char* path, int deep) { return (AtDataLink*)d_find_path(this, nullptr, from, path, deep); }
void GmRegistryNames::Add(AtDataRegistry* r) { d_names_add(this, nullptr, r); }
char* g_unknown30567C;
void AppSaveDataInit(void* part) { d_savedata(part, nullptr); }
void GmAudioReadSavedVolumes(GmAudioData* audio) { d_volumes(audio, nullptr); }
void GmUnknown97EB0(void* object) { d_97eb0(object, nullptr); }
void* AtIoSpaceManagerAdd(AtIoSpaceManager* m, void* handle, unsigned int size, int flag) { return d_space_add(m, nullptr, handle, size, flag); }
void AppUnknown90580() { d_90580(); }

// ---- files: what FindFile asks --------------------------------------------------------------------------------------------
static bool g_has_generic, g_has_library;
class AskFs : public MemFs {
public:
    int IsFileMissing(const char* path) override {
        L('ask', 0);
        Ls('askp', path);
        Ls('askd', m_path.Current() ? m_path.Current() : "");
        if (!std::strcmp(path, "Generic.ADR")) return g_has_generic ? 0 : 1;
        if (!std::strcmp(path, "Library.ADD")) return g_has_library ? 0 : 1;
        return 1;
    }
};
static AskFs* g_fs[2];
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];

// ---- comparing --------------------------------------------------------------------------------------------------------------
struct Global {
    const char* name;
    uint32_t orig;
    void* rec;
    unsigned size;
    bool pointer;  // compared as an offset into its side's App when it points there
};
static GmLevelManager* g_levels[2];
static GmRegistryUser g_user[2];
static const std::vector<Global>& globals() {
    static std::vector<Global> g = {
        {"App::s_instance", 0x00304D58, &App::s_instance, 4, true},
        {"App::s_heap", 0x00304D6C, &App::s_heap, 4, false},
        {"App::s_heapMemory", 0x00304D68, &App::s_heapMemory, 4, false},
        {"App::s_memoryTotal", 0x003B3BE0, &App::s_memoryTotal, 4, false},
        {"App::s_memoryOverhead", 0x003B3BDC, &App::s_memoryOverhead, 4, false},
        {"App::s_gameState", 0x003B3530, &App::s_gameState, 4, true},
        {"App::s_gmApp", 0x003B360C, &App::s_gmApp, 4, true},
        {"GmMoFramework::s_heap", 0x003B3600, &GmMoFramework::s_heap, 4, false},
        {"g_heap00301B70", 0x00301B70, &g_heap00301B70, 4, false},
        {"g_heap003B3614", 0x003B3614, &g_heap003B3614, 4, false},
        {"g_heap00305914", 0x00305914, &g_heap00305914, 4, false},
        {"g_appShortName", 0x003050E8, g_appShortName, 8, false},
        {"g_appTitle", 0x003050F0, g_appTitle, 0x40, false},
        {"g_unknown305130", 0x00305130, &g_unknown305130, 4, false},
        {"GmSaveTypes", 0x00305140, &GmSaveTypes::s_instance, sizeof(GmSaveTypes), false},
        {"g_enabled3B3514", 0x003B3514, &g_enabled3B3514, 4, false},
        {"g_enabled3056A4", 0x003056A4, &g_enabled3056A4, 4, false},
        {"g_enabled3056C8", 0x003056C8, &g_enabled3056C8, 4, false},
        {"g_enabled3056EC", 0x003056EC, &g_enabled3056EC, 4, false},
        {"g_enabled305710", 0x00305710, &g_enabled305710, 4, false},
        {"g_enabled305734", 0x00305734, &g_enabled305734, 4, false},
        {"g_enabled305758", 0x00305758, &g_enabled305758, 4, false},
        {"g_enabled30577C", 0x0030577C, &g_enabled30577C, 4, false},
        {"g_enabled3057A0", 0x003057A0, &g_enabled3057A0, 4, false},
        {"g_enabled3057E8", 0x003057E8, &g_enabled3057E8, 4, false},
        {"g_enabled30580C", 0x0030580C, &g_enabled30580C, 4, false},
        {"g_enabled305830", 0x00305830, &g_enabled305830, 4, false},
        {"g_unknown304D54", 0x00304D54, &g_unknown304D54, 4, false},
        {"g_unknown3ADA50", 0x003ADA50, &g_unknown3ADA50, 4, false},
        {"g_inScripts", 0x003B361C, &g_inScripts, 4, false},
        {"g_debugChannels", 0x002B93A4, &g_debugChannels, 2, false},
        {"GmAppSettings::s_argc", 0x002BF308, &GmAppSettings::s_argc, 4, false},
        {"GmAppSettings::s_argv", 0x002BF300, &GmAppSettings::s_argv, 4, false},
    };
    return g;
}
static unsigned char g_pattern[kAppSize];

// Both sides start from the same bytes and the same globals.
static void set_globals(unsigned seed) {
    for (const Global& g : globals()) {
        unsigned char* o = (unsigned char*)(uintptr_t)g.orig;
        unsigned char* r = (unsigned char*)g.rec;
        for (unsigned i = 0; i < g.size; i++) o[i] = r[i] = (unsigned char)(seed * 13 + i * 7 + 1);
    }
    g_unknown304D80 = orig_u32(0x00304D80) = seed & 1;
}
static void compare_globals() {
    for (const Global& g : globals()) {
        const unsigned char* o = (const unsigned char*)(uintptr_t)g.orig;
        const unsigned char* r = (const unsigned char*)g.rec;
        if (g.pointer) {
            const uint32_t ov = *(const uint32_t*)o, rv = *(const uint32_t*)r;
            g_side = 0;
            const uint32_t a = rel((void*)(uintptr_t)ov);
            g_side = 1;
            const uint32_t b = rel((void*)(uintptr_t)rv);
            g_side = 0;
            eq(g.name, a, b);
            continue;
        }
        for (unsigned i = 0; i < g.size; i++) {
            if (o[i] != r[i]) { eq(g.name, i << 8 | o[i], i << 8 | r[i]); break; }
        }
        g_checks++;
    }
    // The script names: a map the original keeps at 0x002CD0F8.
    const GmNameMap* om = (const GmNameMap*)(uintptr_t)0x002CD0F8;
    eq("script names' size", om->m_size, g_scriptNames.m_size);
    eq("script names' entries", om->m_entries != nullptr, g_scriptNames.m_entries != nullptr);
}

static void compare_apps() {
    const unsigned char* o = g_base[0];
    const unsigned char* r = g_base[1];
    for (unsigned i = 0; i < kAppSize; i += 4) {
        if (i == 0x6D48) {  // the manifest: each side's own allocation
            eq("App::m_manifest made", *(const uint32_t*)(o + i) != 0, *(const uint32_t*)(r + i) != 0);
            continue;
        }
        uint32_t a = *(const uint32_t*)(o + i), b = *(const uint32_t*)(r + i);
        if (a == b) continue;  // only a difference is looked at as a pointer into each side's own App
        g_side = 0;
        a = rel((void*)(uintptr_t)a);
        g_side = 1;
        b = rel((void*)(uintptr_t)b);
        g_side = 0;
        if (a != b) {
            char what[64];
            std::snprintf(what, sizeof what, "App+0x%04X", i);
            eq(what, a, b);
            return;
        }
    }
    g_checks++;
}

static void compare_levels() {
    eq("levels' current", g_levels[0]->m_currentIndex, g_levels[1]->m_currentIndex);
    eq("levels' previous", g_levels[0]->m_previousIndex, g_levels[1]->m_previousIndex);
    eq("levels' name", std::memcmp(g_levels[0]->m_currentName, g_levels[1]->m_currentName, 9), 0);
    eq("registry user's registry", (uint32_t)(uintptr_t)g_user[0].m_registry, (uint32_t)(uintptr_t)g_user[1].m_registry);
    eq("registry user's root", (uint32_t)(uintptr_t)g_user[0].m_root, (uint32_t)(uintptr_t)g_user[1].m_root);
}

// ---- set-up of a case ------------------------------------------------------------------------------------------------------
struct Scenario {
    unsigned seed = 1;
    int atlas_running = 0;
    int open_list = 0;
    int test_all = 0;
    const char* level = "ALMOND";
    const char* folder = "DataXbox";
    unsigned mem_total = 0x8000000, mem_avail = 0x5000000;
    bool heap0 = true;
    bool has_generic = true, has_library = true;
    int registry_result = 0;
    int save_count = 0;
    int update_after = 1;
    int fail_at_push = -1;
};
static FakeFramework g_fw_state[3];

static void reset_fakes() {
    for (int i = 0; i < 3; i++) {
        FakeFramework* f = (FakeFramework*)g_frameworks[i];
        f->vftable = (const void* const*)g_framework_table;
        f->m_index = 0x55 + i;
        f->m_request = 0;
    }
    g_heap_manager->m_override = (AtHeap*)(uintptr_t)0x5A5A5A5A;
}

static void prepare(const Scenario& s) {
    for (int side = 0; side < 2; side++) {
        unsigned char* b = g_base[side];
        unsigned x = s.seed * 2654435761u;
        for (unsigned i = 0; i < kAppSize; i++) {
            x = x * 1103515245u + 12345u;
            b[i] = (unsigned char)(x >> 16);
        }
        *(const void**)b = g_app_table;
        *(const void**)(b + 0x5F7C) = g_device_table;
        *(unsigned*)(b + 0x5F88) &= ~8u;
        *(void**)(b + 0x5F90) = TOKEN(16);
        *(void**)(b + 0x5F98) = (void*)(uintptr_t)0x0BADF00D;
        *(const void**)(b + 0x6450) = g_atlas_table;
        *(int*)(b + 0x6454) = s.atlas_running;
        *(void**)(b + 0x6458) = TOKEN(14);
        *(void**)(b + 0x6494) = s.heap0 ? (void*)(uintptr_t)0x41000000 : nullptr;
        *(void**)(b + 0x6498) = (void*)(uintptr_t)0x41000100;
        *(const void**)(b + 0x65B8) = g_state_table;
        *(void**)(b + 0x692C) = TOKEN(2);
        for (int i = 0; i < 3; i++) *(void**)(b + 0x64C4 + 4 * i) = g_frameworks[i];
        *(void**)(b + 0x64D0) = nullptr;
        *(int*)(b + 0x6518) = 0;
    }
    std::memset((void*)(uintptr_t)0x002CD0F8, 0, sizeof(GmNameMap));
    std::memset(&g_scriptNames, 0, sizeof(GmNameMap));
    set_globals(s.seed);
    GmSaveTypes::s_instance.m_count = s.save_count;
    orig_u32(0x00305140) = s.save_count;
    for (int side = 0; side < 2; side++) {
        GmLevelManager* m = g_levels[side];
        m->m_currentIndex = 3;
        m->m_previousIndex = 9;
        std::memset(m->m_currentName, 0x77, sizeof m->m_currentName);
        g_user[side].m_registry = (AtDataRegistry*)(uintptr_t)1;
        g_user[side].m_root = (AtDataLink*)(uintptr_t)2;
        g_ms[side] = 1000;
        g_heap_next[side] = 0;
        g_updates[side] = 0;
        g_pushes[side] = 0;
        g_frame[side] = 0;
    }
    g_open_list = s.open_list;
    g_test_all = s.test_all;
    g_start_level = s.level;
    g_data_folder = s.folder;
    g_mem_total = s.mem_total;
    g_mem_avail = s.mem_avail;
    g_has_generic = s.has_generic;
    g_has_library = s.has_library;
    g_registry_result = s.registry_result;
    g_update_after = s.update_after;
    g_fail_at_push = s.fail_at_push;
    g_fail_alloc = false;
    reset_fakes();
}

// Runs fn on one side with that side's level manager and registry user installed. A crash is recorded and compared; both sides must agree on it.
static AtHeap* g_override_after[2];
static FakeFramework g_fw_after[2][3];
template <class F> static bool run_side(int side, F fn) {
    orig_u32(0x003B3570) = (uint32_t)(uintptr_t)g_levels[0];
    GmLevelManager::s_instance = g_levels[1];
    orig_u32(0x003B3524) = (uint32_t)(uintptr_t)&g_user[0];
    GmRegistryUser::s_instance = &g_user[1];
    reset_fakes();
    g_fail_alloc = false;
    bool crashed;
    {
        On on(side);
        crashed = guarded(fn);
    }
    g_override_after[side] = g_heap_manager->m_override;
    for (int i = 0; i < 3; i++) g_fw_after[side][i] = *(FakeFramework*)g_frameworks[i];
    g_fail_alloc = false;
    return crashed;
}

static void compare_frameworks() {
    for (int i = 0; i < 3; i++) {
        eq("framework's index", g_fw_after[0][i].m_index, g_fw_after[1][i].m_index);
        eq("framework's request", g_fw_after[0][i].m_request, g_fw_after[1][i].m_request);
    }
}
static void finish(const char* what, bool crashed0, bool crashed1) {
    compare_frameworks();
    eq("crashed", crashed0, crashed1);
    eq("heap override after", (uint32_t)(uintptr_t)g_override_after[0], (uint32_t)(uintptr_t)g_override_after[1]);
    compare_logs(what);
    compare_apps();
    compare_globals();
    compare_levels();
}

typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, int);
typedef void(__fastcall* FnInit)(void*, void*, int, int);
typedef void(__fastcall* FnPtr)(void*, void*, void*);
typedef void*(__fastcall* FnGetHeap)(void*, void*, unsigned);
typedef unsigned(__fastcall* FnUnsigned)(void*, void*);
typedef int(__fastcall* FnFind)(void*, void*, const char*);
typedef void(__fastcall* FnPath)(void*, void*, const char*);
typedef void(__cdecl* FnSet)(void*);
typedef unsigned(__cdecl* FnSize)();
typedef int(__cdecl* FnMain)(unsigned, char**, char**);

// ---- cases -------------------------------------------------------------------------------------------------------------------
static int g_inits, g_runs, g_frames_total, g_transitions;

static int g_logged;
static void dump_log(const char* what) {
    for (int side = 0; side < 2; side++) {
    std::printf("%s %d:", what, side);
    for (int i = 0; i < g_log[side].n; i += 2) {
        const uint32_t t = g_log[side].v[i];
        char tag[5] = {0};
        int k = 0;
        for (int sh = 24; sh >= 0; sh -= 8)
            if ((t >> sh) & 0xFF) tag[k++] = (char)((t >> sh) & 0xFF);
        std::printf(" %s=%X", tag, g_log[side].v[i + 1]);
    }
    }
    std::printf("\n");
}
static void run_init(const char* what, const Scenario& s) {
    g_context = std::string("Init: ") + what;
    prepare(s);
    const bool c0 = run_side(0, [] { at<FnInit>(0x0008E660)(orig_app(), nullptr, 0x11, 0x22); });
    const bool c1 = run_side(1, [] { rec_app()->Init(0x11, 0x22); });
    g_logged += g_log[0].n / 2;
    if (std::getenv("ORACLE_APP_DUMP")) dump_log(what);
    finish(what, c0, c1);
    g_inits++;
}

static void run_loop(const char* what, const Scenario& s, int frames, const std::vector<int>& requests, unsigned quit_at, unsigned flag_at, unsigned ada_at, int steps,
                     const int* table, bool quit_first = false, bool flag_first = false) {
    g_context = std::string("Run: ") + what;
    prepare(s);
    for (int side = 0; side < 2; side++) {
        unsigned char* b = g_base[side];
        for (int i = 0; i < 16; i++) *(int*)(b + 0x64D8 + 4 * i) = table[i];
        *(void**)(b + 0x64D4) = g_frameworks[0];
        *(int*)(b + 0x6518) = quit_first;
        if (flag_first) *(unsigned*)(b + 0x5F88) |= 8;
        *(int*)(b + 0x651C) = 640 + side * 0;
        *(int*)(b + 0x6520) = 480;
        g_frames_left[side] = frames;
        g_script_left[side] = steps;
    }
    orig_u32(0x003ADA50) = 0;
    g_unknown3ADA50 = 0;
    g_requests = requests;
    g_quit_at_frame = quit_at;
    g_flag_at_frame = flag_at;
    g_3ada50_at_frame = ada_at;
    auto setup_fw = [] {
        for (int i = 0; i < 3; i++) ((FakeFramework*)g_frameworks[i])->m_index = i;
    };
    FakeFramework after[2][3];
    bool c[2];
    for (int side = 0; side < 2; side++) {
        reset_fakes();
        setup_fw();
        {
            On on(side);
            if (side == 0) c[0] = guarded([] { at<Fn0>(0x0008EBE0)(orig_app(), nullptr); });
            else c[1] = guarded([] { rec_app()->Run(); });
        }
        for (int i = 0; i < 3; i++) after[side][i] = *(FakeFramework*)g_frameworks[i];
    }
    for (int i = 0; i < 3; i++) {
        eq("framework's index after", after[0][i].m_index, after[1][i].m_index);
        eq("framework's request after", after[0][i].m_request, after[1][i].m_request);
    }
    eq("crashed", c[0], c[1]);
    compare_logs(what);
    compare_apps();
    compare_globals();
    g_runs++;
    g_frames_total += frames;
}

// One function on its own, with a scenario's starting state.
template <class O, class R> static void run_one(const char* what, const Scenario& s, O orig, R rec, bool levels = true) {
    g_context = what;
    prepare(s);
    const bool c0 = run_side(0, orig);
    const bool c1 = run_side(1, rec);
    compare_frameworks();
    eq("crashed", c0, c1);
    eq("heap override after", (uint32_t)(uintptr_t)g_override_after[0], (uint32_t)(uintptr_t)g_override_after[1]);
    compare_logs(what);
    compare_apps();
    compare_globals();
    if (levels) compare_levels();
}

static void run_find_level() {
    // Real tables: names written by GmLevelName on both sides' shared array; the original's code reads the same memory.
    static const char* names[] = {"ALMOND", "LBKYRD1", "TWELVECH", "ALMOND", "", "A", "TWELVECHARS", "lbkyrd1"};
    GmLevelName items[8];
    for (int i = 0; i < 8; i++) items[i].SetName(names[i]);
    GmLevelManager* m = (GmLevelManager*)std::calloc(1, sizeof(GmLevelManager));
    m->m_names.m_items = items;
    static const char asks[][16] = {"ALMOND", "LBKYRD1", "TWELVECH", "TWELVECHA", "TWELVECH\0X", "", "A", "B", "LBKYRD", "LBKYRD12", "lbkyrd1", "almond", "TWELVECHARS", "ALMOND\0X"};
    for (int count = -1; count <= 8; count++) {
        m->m_levelCount = count;
        for (const char* a : asks) {
            char name[16];
            std::memcpy(name, a, 16);
            g_context = std::string("FindLevel ") + a;
            const int o = at<FnFind>(0x0008E420)(m, nullptr, name);
            const int r = m->FindLevel(name);
            eq("FindLevel", o, r);
        }
    }
    std::free(m);
}

static void run_paths() {
    for (int count = 0; count <= 3; count++) {
        for (int pre = 0; pre < 3; pre++) {
            MemFs* fs[2][3];
            AtIoContext ctx[2];
            AtIoFileSystem* stack[2][3];
            for (int side = 0; side < 2; side++) {
                for (int i = 0; i < 3; i++) {
                    fs[side][i] = new MemFs;
                    fs[side][i]->m_path.Init();
                    for (int k = 0; k < pre + i; k++) fs[side][i]->m_path.Push("pre\\");
                    stack[side][i] = fs[side][i];
                }
                ctx[side].m_stack = stack[side];
                ctx[side].m_stackCount = count;
                ctx[side].m_stackSize = 3;
            }
            auto same = [&](const char* what) {
                g_context = what;
                for (int i = 0; i < 3; i++) {
                    eq("path's top", fs[0][i]->m_path.m_top, fs[1][i]->m_path.m_top);
                    eq("path's entries", std::memcmp(fs[0][i]->m_path.m_stack, fs[1][i]->m_path.m_stack, sizeof fs[0][i]->m_path.m_stack), 0);
                }
            };
            at<FnPath>(0x00019370)(&ctx[0], nullptr, "Design\\Registry\\");
            ctx[1].PushPath("Design\\Registry\\");
            same("PushPath");
            at<FnPath>(0x00019370)(&ctx[0], nullptr, "");
            ctx[1].PushPath("");
            same("PushPath, empty");
            at<Fn0>(0x000193A0)(&ctx[0], nullptr);
            ctx[1].PopPath();
            same("PopPath");
            at<Fn0>(0x000193A0)(&ctx[0], nullptr);
            ctx[1].PopPath();
            same("PopPath again");
            for (int k = 0; k < 10; k++) {  // past the eighth, Push is ignored; past empty, Pop goes on down
                at<FnPath>(0x00019370)(&ctx[0], nullptr, "deep\\");
                ctx[1].PushPath("deep\\");
            }
            same("PushPath past the eighth");
            for (int k = 0; k < 12; k++) {
                at<Fn0>(0x000193A0)(&ctx[0], nullptr);
                ctx[1].PopPath();
            }
            same("PopPath past the bottom");
            for (int side = 0; side < 2; side++)
                for (int i = 0; i < 3; i++) delete fs[side][i];
        }
    }
}

static void run_small_slots() {
    Scenario s;
    for (unsigned seed = 1; seed <= 6; seed++) {
        s.seed = seed;
        run_one("GetMemoryParams", s,
                [] { AtMemoryParams* p = (AtMemoryParams*)TOKEN(4); std::memset(p, 0x33, 12); at<FnPtr>(0x0008C620)(orig_app(), nullptr, p); std::memcpy(TOKEN(3), p, 12); },
                [] { AtMemoryParams* p = (AtMemoryParams*)TOKEN(4); std::memset(p, 0x33, 12); rec_app()->App::GetMemoryParams(p); });
        eq("GetMemoryParams", std::memcmp(TOKEN(3), TOKEN(4), 12), 0);
        run_one("GetConfig07", s,
                [] { std::memset(TOKEN(4), 0x33, 0x30); at<FnPtr>(0x0008C680)(orig_app(), nullptr, TOKEN(4)); std::memcpy(TOKEN(3), TOKEN(4), 0x30); },
                [] { std::memset(TOKEN(4), 0x33, 0x30); rec_app()->App::GetConfig07((AppLayerConfig07*)TOKEN(4)); });
        eq("GetConfig07", std::memcmp(TOKEN(3), TOKEN(4), 0x30), 0);
        run_one("GetAtlasConfig", s,
                [] { std::memset(TOKEN(4), 0x33, 0x30); at<FnPtr>(0x0008C830)(orig_app(), nullptr, TOKEN(4)); std::memcpy(TOKEN(3), TOKEN(4), 0x30); },
                [] { std::memset(TOKEN(4), 0x33, 0x30); rec_app()->App::GetAtlasConfig((AppAtlasConfig*)TOKEN(4)); });
        eq("GetAtlasConfig", std::memcmp(TOKEN(3), TOKEN(4), 0x30), 0);
        for (unsigned index : {0u, 1u, 2u, 3u, 4u, 5u, 0x80000000u, 0xFFFFFFFFu}) {
            prepare(s);
            g_context = "GetHeap";
            eq("GetHeap", (uint32_t)(uintptr_t)at<FnGetHeap>(0x0008C960)(orig_app(), nullptr, index), (uint32_t)(uintptr_t)rec_app()->App::GetHeap(index));
        }
        // The heaps' free and used, over every pattern of missing heaps.
        static AtHeap heaps[4];
        for (int mask = 0; mask < 16; mask++) {
            prepare(s);
            for (int i = 0; i < 4; i++) {
                heaps[i].m_size = 0x10000u * (i + 1) * seed + 3;
                heaps[i].m_used = 0x100u * (i + 2) + seed;
                for (int side = 0; side < 2; side++) *(AtHeap**)(g_base[side] + 0x6494 + 4 * i) = (mask >> i & 1) ? &heaps[i] : nullptr;
            }
            g_context = "HeapsFree";
            eq("HeapsFree", at<FnUnsigned>(0x0008C9C0)(orig_app(), nullptr), rec_app()->App::HeapsFree());
            eq("HeapsUsed", at<FnUnsigned>(0x0008C980)(orig_app(), nullptr), rec_app()->App::HeapsUsed());
        }
    }
    // The heaps made and destroyed, over sizes and partners.
    static const int partners[] = {-2, -1, 0, 1, 2, 3};
    for (unsigned seed = 1; seed <= 40; seed++) {
        Scenario t;
        t.seed = seed;
        for (int pass = 0; pass < 2; pass++) {
            run_one(pass ? "DestroyHeaps" : "CreateHeaps", t,
                    [=] {
                        unsigned char* b = g_base[0];
                        for (int i = 0; i < 4; i++) {
                            *(unsigned*)(b + 0x64A4 + 8 * i) = ((seed >> i) & 1) ? 0 : 0x10000u * (seed + i);
                            *(int*)(b + 0x64A8 + 8 * i) = partners[(seed * 7 + i * 3) % 6];
                        }
                        orig_u32(0x00304D68) = 0x123456u * seed;
                        if (pass) at<Fn0>(0x0008C920)(orig_app(), nullptr);
                        else at<Fn0>(0x0008C860)(orig_app(), nullptr);
                    },
                    [=] {
                        unsigned char* b = g_base[1];
                        for (int i = 0; i < 4; i++) {
                            *(unsigned*)(b + 0x64A4 + 8 * i) = ((seed >> i) & 1) ? 0 : 0x10000u * (seed + i);
                            *(int*)(b + 0x64A8 + 8 * i) = partners[(seed * 7 + i * 3) % 6];
                        }
                        App::s_heapMemory = 0x123456u * seed;
                        if (pass) rec_app()->App::DestroyHeaps();
                        else rec_app()->App::CreateHeaps();
                    });
        }
    }
    // SetDefaults on whatever was there.
    for (unsigned seed = 1; seed <= 4; seed++) {
        s.seed = seed;
        run_one("SetDefaults", s, [] { *(int*)(g_base[0] + 0x6518) = 7; at<Fn0>(0x0008DA10)(orig_app(), nullptr); },
                [] { *(int*)(g_base[1] + 0x6518) = 7; rec_app()->SetDefaults(); });
    }
    // The setters and the save size.
    for (unsigned seed = 1; seed <= 3; seed++) {
        s.seed = seed;
        void* const heap = (void*)(uintptr_t)(0x42000000u + seed);
        static void* h;
        h = heap;
        run_one("setters", s,
                [] { at<FnSet>(0x00083490)(h); at<FnSet>(0x00143DD0)((char*)h + 1); at<FnSet>(0x000980F0)((char*)h + 2); at<FnSet>(0x00140C20)((char*)h + 3); },
                [] { GmSetHeap00083490(h); GmAcPlayerGameStateDataSetHeap((char*)h + 1); GmSetHeap000980F0((char*)h + 2); GmMoFramework::SetHeap((char*)h + 3); });
        g_context = "GmGameSaveSize";
        eq("GmGameSaveSize", at<FnSize>(0x0009D540)(), GmGameSaveSize());
        run_one("GmScriptsInitNames", s, [] { at<void(__cdecl*)()>(0x000703B0)(); }, [] { GmScriptsInitNames(); });
    }
    // The framework table, with the switch both ways.
    for (int all = 0; all < 2; all++) {
        Scenario t;
        t.seed = 9 + all;
        run_one(all ? "SetFrameworkTable, all levels" : "SetFrameworkTable", t,
                [=] { *(int*)(g_base[0] + 0x654C + 0x24) = all; orig_u32(0x00304D58) = (uint32_t)(uintptr_t)g_base[0]; at<Fn0>(0x0008DDF0)(orig_app(), nullptr); },
                [=] { *(int*)(g_base[1] + 0x654C + 0x24) = all; App::s_instance = rec_app(); rec_app()->SetFrameworkTable(); });
    }
}

static void run_parts() {
    for (unsigned seed = 1; seed <= 3; seed++) {
        Scenario s;
        s.seed = seed;
        auto own = [](int side) { if (side == 0) orig_u32(0x00304D58) = (uint32_t)(uintptr_t)g_base[0]; else App::s_instance = rec_app(); };
        run_one("LoadDirtyDiskScreen", s, [&] { own(0); at<Fn0>(0x0008E240)(orig_app(), nullptr); }, [&] { own(1); rec_app()->LoadDirtyDiskScreen(); });
        for (int fail = -1; fail < 3; fail++) {
            s.fail_at_push = fail;
            s.heap0 = fail != 2;
            for (int heap = 0; heap < 2; heap++) {
                // The heap for the allocations is App::s_heap; the one handed on is the first of App's own.
                run_one("CreateMountManager", s,
                        [&] { own(0); orig_u32(0x00304D6C) = heap ? 0x43000000u : 0; at<Fn0>(0x0008CE00)(orig_app(), nullptr); },
                        [&] { own(1); App::s_heap = heap ? (AtHeap*)(uintptr_t)0x43000000u : nullptr; rec_app()->CreateMountManager(); });
            }
            for (int fail2 = -1; fail2 < 3; fail2++) {
                s.fail_at_push = fail2;
                run_one("CreateFrameworks", s, [&] { own(0); at<Fn0>(0x0008DC70)(orig_app(), nullptr); }, [&] { own(1); rec_app()->CreateFrameworks(); });
            }
            s.fail_at_push = -1;
        }
        s.heap0 = true;
        for (int files = 0; files < 4; files++) {
            s.has_generic = files & 1;
            s.has_library = (files & 2) != 0;
            s.registry_result = files == 3 ? 5 : 0;
            run_one("LoadRegistries", s, [&] { own(0); at<Fn0>(0x0008DEA0)(orig_app(), nullptr); }, [&] { own(1); rec_app()->LoadRegistries(); });
        }
        s.has_generic = s.has_library = true;
        for (int running = 0; running < 2; running++) {
            s.atlas_running = running;
            run_one("StartUpAtlas", s, [] { at<Fn0>(0x0009C280)(orig_app(), nullptr); }, [] { rec_app()->StartUpAtlas(); });
        }
        s.atlas_running = 0;
        static const unsigned mems[][2] = {{0x8000000, 0x5000000}, {0x4000000, 0x4000000}, {0x4000000, 0x3FFFFFF}, {0x4000000, 0x4000001}, {0xF0000000, 0x81000000}, {0x1000, 0x2000}, {0, 0}};
        for (const auto& m : mems) {
            s.mem_total = m[0];
            s.mem_avail = m[1];
            run_one("StartUp", s, [] { at<Fn0>(0x00141500)(orig_app(), nullptr); }, [] { rec_app()->App::StartUp(); });
        }
    }
}

static void run_inits() {
    Scenario base;
    run_init("plain", base);
    for (unsigned seed = 2; seed <= 5; seed++) {
        Scenario s;
        s.seed = seed;
        run_init("seed", s);
    }
    {
        Scenario s;
        s.atlas_running = 1;
        run_init("running already", s);
    }
    {
        Scenario s;
        s.open_list = 1;
        run_init("list files", s);
    }
    {
        Scenario s;
        s.test_all = 1;
        run_init("all levels", s);
    }
    for (const char* level : {"LBKYRD1", "NOSUCH", "TWELVECH", "", "LBKYRD"}) {
        Scenario s;
        s.level = level;
        run_init(level, s);
    }
    for (int count : {1, 4, 5, 6, -1}) {
        Scenario s;
        s.save_count = count;
        run_init("save types", s);
    }
    for (int after : {2, 5}) {
        Scenario s;
        s.update_after = after;
        run_init("manifest slices", s);
    }
    for (int files = 0; files < 3; files++) {
        Scenario s;
        s.has_generic = files != 0;
        s.has_library = files != 1;
        run_init("registry files", s);
    }
    {
        Scenario s;
        s.heap0 = false;
        run_init("no first heap", s);
    }
    for (int fail : {1, 3, 5, 6}) {
        Scenario s;
        s.fail_at_push = fail;
        run_init("no memory from a push on", s);
    }
    for (const char* folder : {"", "D", "LongName"}) {
        Scenario s;
        s.folder = folder;
        run_init("data folder", s);
    }
    {
        Scenario s;
        s.mem_avail = 0x3000000;
        run_init("little memory", s);
    }
}

static void run_loops() {
    int table[16];
    const int real[16] = {0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 1, 3, 3, 2, 2};
    Scenario s;
    run_loop("quit before", s, 5, {}, ~0u, ~0u, ~0u, 0, real, true);
    run_loop("device flag before", s, 5, {}, ~0u, ~0u, ~0u, 0, real, false, true);
    run_loop("no frames", s, 0, {}, ~0u, ~0u, ~0u, 0, real);
    run_loop("frames", s, 6, {}, ~0u, ~0u, ~0u, 2, real);
    run_loop("intro to menu to game", s, 12, {-1, 2, -1, -1, 2, -1, 0, -1, 1, -1, -1, -1}, ~0u, ~0u, ~0u, 1, real);
    run_loop("quit on frame 3", s, 12, {}, 3, ~0u, ~0u, 0, real);
    run_loop("flag on frame 2", s, 12, {}, ~0u, 2, ~0u, 0, real);
    run_loop("one-shot on frame 1", s, 6, {}, ~0u, ~0u, 1, 3, real);
    for (unsigned seed = 1; seed <= 30; seed++) {
        unsigned x = seed * 2654435761u;
        auto next = [&] { x = x * 1103515245u + 12345u; return x >> 16; };
        for (int i = 0; i < 16; i++) table[i] = (int)(next() % 3);
        std::vector<int> requests;
        for (int i = 0; i < 20; i++) requests.push_back((int)(next() % 5) - 1);
        Scenario t;
        t.seed = seed;
        run_loop("random", t, 4 + (int)(next() % 16), requests, next() % 3 == 0 ? next() % 20 : ~0u, next() % 4 == 0 ? next() % 20 : ~0u, next() % 3 == 0 ? next() % 20 : ~0u,
                 (int)(next() % 4), table);
        g_transitions++;
    }
}

static void run_main() {
    g_context = "main";
    Hook hi, hr, hs;
    hi.set((void*)(uintptr_t)0x0008E660, (const void*)d_init);
    hr.set((void*)(uintptr_t)0x0008EBE0, (const void*)d_run);
    hs.set((void*)(uintptr_t)0x0008ED60, (const void*)d_app_shutdown);
    Hook ri, rr;
    ri.set(code_of(&App::Init), (const void*)d_init);
    rr.set(code_of(&App::Run), (const void*)d_run);
    char a0[] = "a", a1[] = "b";
    static char* argv[] = {a0, a1, nullptr};
    for (unsigned argc : {0u, 1u, 2u}) {
        set_globals(argc + 3);
        int o, r;
        {
            On on(0);
            o = at<FnMain>(0x00011030)(argc, argc ? argv : nullptr, (char**)(uintptr_t)0x99);
        }
        {
            On on(1);
            r = AppMain(argc, argc ? argv : nullptr, (char**)(uintptr_t)0x99);
        }
        eq("main's result", o, r);
        compare_logs("main");
        compare_globals();
    }
    hi.unset();
    hr.unset();
    hs.unset();
    ri.unset();
    rr.unset();
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO || c == 0xC0000374 || c == 0xC0000005) {
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
    if (pos.size() != 1) { std::printf("usage: oracle_app <default.xbe> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    at<void(__cdecl*)()>(0x00125BC7)();

    // The original's helpers, and what App reaches that is not recovered or is checked elsewhere.
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)s_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    redirect(0x001B7B84, (const void*)d_language);
    redirect(0x001B7B12, (const void*)d_region);
    redirect(0x00020640, (const void*)d_ms);
    redirect(0x00085451, (const void*)d_memstatus);
    redirect(0x00125559, (const void*)d_format);
    redirect(0x000120A0, (const void*)d_report);
    redirect(0x00011D00, (const void*)d_create_heap);
    redirect(0x00011FC0, (const void*)d_equiv);
    redirect(0x000121D0, (const void*)d_destroy_heap);
    redirect(0x00098A50, (const void*)d_settings_load);
    redirect(0x00098410, (const void*)d_cmdline);
    redirect(0x000766E0, (const void*)d_mount);
    redirect(0x00076830, (const void*)d_unmount);
    redirect(0x00076990, (const void*)d_open_list);
    redirect(0x000769D0, (const void*)d_close_list);
    redirect(0x0008DBA0, (const void*)d_display);
    redirect(0x001416A0, (const void*)d_matrix);
    redirect(0x0008CFB0, (const void*)d_subsystems);
    redirect(0x0009C3C0, (const void*)d_ui);
    redirect(0x00090210, (const void*)d_692c_init);
    redirect(0x0006DEA0, (const void*)d_692c_update);
    redirect(0x00139F10, (const void*)d_volumes);
    redirect(0x00097EB0, (const void*)d_97eb0);
    redirect(0x00090950, (const void*)d_string);
    redirect(0x0012EEA0, (const void*)d_scripts_init);
    redirect(0x0012EFC0, (const void*)d_find_module);
    redirect(0x0008E4E0, (const void*)d_cb_app);
    redirect(0x0008C450, (const void*)d_cb_control);
    redirect(0x0008CB30, (const void*)d_cb_save);
    redirect(0x0007D070, (const void*)d_space_add);
    redirect(0x00141E10, (const void*)d_configure);
    redirect(0x00142040, (const void*)d_update);
    redirect(0x0009D9C0, (const void*)d_savedata);
    redirect(0x0001ADC0, (const void*)d_loader_load);
    redirect(0x00068880, (const void*)d_registry_load);
    redirect(0x0009BD50, (const void*)d_library_load);
    redirect(0x00067D40, (const void*)d_attach);
    redirect(0x000675B0, (const void*)d_prepare);
    redirect(0x00066860, (const void*)d_set_name);
    redirect(0x000679E0, (const void*)d_find_path);
    redirect(0x00071F20, (const void*)d_names_add);
    redirect(0x00140350, (const void*)d_mount_ctor);
    redirect(0x0006C2A0, (const void*)d_logs_ctor);
    redirect(0x0006C3E0, (const void*)d_logs_init);
    redirect(0x00031590, (const void*)d_stream_heap);
    redirect(0x0009B6B0, (const void*)d_fw0);
    redirect(0x0009B1E0, (const void*)d_fw1);
    redirect(0x00099520, (const void*)d_fw2);
    redirect(0x0009EA00, (const void*)d_viewport);
    redirect(0x00071A60, (const void*)d_tick);
    redirect(0x00090580, (const void*)d_90580);
    redirect(0x00091540, (const void*)d_run_scripts);
    redirect(0x0009CA90, (const void*)d_present);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    // The recovered members checked by their own suites: their entries jump to the same doubles.
    patch((const void*)&AtFormat, (const void*)d_format);
    patch(code_of(&AtHeapManager::Report), (const void*)d_report);
    patch(code_of(&AtHeapManager::CreateHeap), (const void*)d_create_heap);
    patch(code_of(&AtHeapManager::MakeEquivalent), (const void*)d_equiv);
    patch(code_of(&AtHeapManager::DestroyHeap), (const void*)d_destroy_heap);
    patch(code_of(&GmAppSettings::Load), (const void*)d_settings_load);
    patch(code_of(&GmAppSettings::ReadCommandLine), (const void*)d_cmdline);
    patch(code_of(&AtIoMountManager::Mount), (const void*)d_mount);
    patch(code_of(&AtIoMountManager::Unmount), (const void*)d_unmount);
    patch(code_of(&AtIoMountManager::OpenListFile), (const void*)d_open_list);
    patch(code_of(&AtIoMountManager::CloseListFile), (const void*)d_close_list);
    patch(code_of(&AtDatabaseLoader::Load), (const void*)d_loader_load);
    patch(code_of(&AtDataRegistry::Load), (const void*)d_registry_load);
    patch(code_of(&AtDataLibrary::Load), (const void*)d_library_load);
    patch(code_of(&GmManifestLoader::Configure), (const void*)d_configure);
    patch(code_of(&GmManifestLoader::Update), (const void*)d_update);
    patch((const void*)&AtIoStream::SetHeap, (const void*)d_stream_heap);
    if (entries && !arm_entries(entries)) return 2;

    // Tables of doubles.
    BadFill<0, 69>::run();
    BadFill<1, 69>::run();
    BadFill<2, 69>::run();
    BadFill<3, 69>::run();
    BadFill<4, 69>::run();
    BadFill<5, 69>::run();
    BadFill<6, 69>::run();
    BadFill<7, 69>::run();
    for (int i = 0; i < 69; i++) g_app_table[i] = g_bad_table[0][i];
    g_app_table[6] = (const void*)d_app_shutdown;
    g_app_table[60] = (const void*)d_app_atlas_config;
    g_app_table[61] = (const void*)d_app_after_atlas;
    g_app_table[64] = (const void*)d_app_get_heap;
    for (int i = 0; i < 7; i++) g_device_table[i] = g_bad_table[1][i];
    g_device_table[6] = (const void*)d_run_frame;
    for (int i = 0; i < 30; i++) g_graphics_table[i] = g_bad_table[2][i];
    g_graphics_table[29] = (const void*)d_db_loader;
    g_atlas_table[0] = g_bad_table[3][0];
    g_atlas_table[1] = (const void*)d_atlas_init;
    for (int i = 0; i < 8; i++) g_state_table[i] = g_bad_table[4][i];
    g_state_table[1] = (const void*)d_gs1;
    g_state_table[3] = (const void*)d_gs3;
    g_state_table[5] = (const void*)d_gs_registry;
    for (int i = 0; i < 7; i++) g_framework_table[i] = g_bad_table[5][i];
    g_framework_table[1] = (const void*)d_fw_enter;
    g_framework_table[2] = (const void*)d_fw_slot2;
    g_framework_table[3] = (const void*)d_fw_slot3;
    g_framework_table[5] = (const void*)d_fw_leave;
    for (int i = 0; i < 7; i++) g_mount_table[i] = g_bad_table[6][i];
    g_mount_table[1] = (const void*)d_mm_init;
    g_mount_table[4] = (const void*)d_mm_region;
    for (int i = 0; i < 7; i++) g_module_table[i] = g_bad_table[7][i];
    g_module_table[6] = (const void*)d_module_enable;
    g_space_device_table[0] = g_bad_table[7][10];
    g_space_device_table[1] = g_bad_table[7][11];
    g_space_device_table[2] = (const void*)d_space_slot2;

    // Objects, one for both sides, at both sides' globals.
    g_base[0] = (unsigned char*)std::calloc(1, kAppSize);
    g_base[1] = (unsigned char*)std::calloc(1, kAppSize);
    for (int side = 0; side < 2; side++) {
        g_levels[side] = (GmLevelManager*)std::calloc(1, sizeof(GmLevelManager));
        static GmLevelName names[2][4];
        static const char* text[4] = {"ALMOND", "LBKYRD1", "TWELVECH", "LBKYRD"};
        for (int i = 0; i < 4; i++) names[side][i].SetName(text[i]);
        g_levels[side]->m_names.m_items = names[side];
        g_levels[side]->m_levelCount = 4;
    }
    for (int i = 0; i < 3; i++) g_frameworks[i] = &g_fw_state[i];
    g_fake_mount = std::calloc(1, 0x48);
    *(const void**)g_fake_mount = g_mount_table;
    *(void**)((char*)g_fake_mount + 0x2C) = (void*)(uintptr_t)0x44000000;
    for (int i = 0; i < 2; i++) {
        g_modules[i] = std::calloc(1, 16);
        *(const void**)g_modules[i] = g_module_table;
    }
    *(const void**)TOKEN(16) = g_graphics_table;
    // The space manager: its device at +32 of token 6.
    *(void**)TOKEN(6) = TOKEN(6) + 32;
    *(const void**)(TOKEN(6) + 32) = g_space_device_table;
    orig_u32(0x00301B4C) = (uint32_t)(uintptr_t)TOKEN(6);
    AtIoSpaceManager::s_instance = (AtIoSpaceManager*)TOKEN(6);
    g_heap_manager = (AtHeapManager*)std::calloc(1, sizeof(AtHeapManager));
    AtHeapManager::s_instance = g_heap_manager;
    orig_u32(0x002C60AC) = (uint32_t)(uintptr_t)g_heap_manager;
    // The mount manager the game reaches by its global: its context answers FindFile.
    AtIoMountManager* mounts = (AtIoMountManager*)TOKEN(1);
    g_fs[0] = new AskFs;
    g_fs[0]->m_path.Init();
    g_ctx_stack[0] = g_fs[0];
    g_ctx.m_stack = g_ctx_stack;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    mounts->m_context = &g_ctx;
    orig_u32(0x002E26E0) = (uint32_t)(uintptr_t)mounts;
    AtIoMountManager::s_instance = mounts;
    orig_u32(0x003B3418) = (uint32_t)(uintptr_t)TOKEN(5);
    GmScripts::s_instance = (GmScripts*)TOKEN(5);
    orig_u32(0x003B354C) = (uint32_t)(uintptr_t)TOKEN(3);
    GmAudioData::s_instance = (GmAudioData*)TOKEN(3);
    orig_u32(0x0030567C) = (uint32_t)(uintptr_t)TOKEN(4);
    g_unknown30567C = (char*)TOKEN(4);
    orig_u32(0x002CD11C) = (uint32_t)(uintptr_t)TOKEN(15);
    GmClock::s_instance = (GmClock*)TOKEN(15);
    ((GmClock*)TOKEN(15))->m_time = g_clock_time;
    ((GmClock*)TOKEN(15))->m_frame = 0x77;
    orig_u32(0x002CD138) = (uint32_t)(uintptr_t)TOKEN(12);
    GmRegistryNames::s_instance = (GmRegistryNames*)TOKEN(12);
    // The data root: an owner at +0x394 of what +4 points to (token 10), and a registry at +0x2C.
    GmDataRoot* root = (GmDataRoot*)std::calloc(1, sizeof(GmDataRoot));
    root->m_app = (App*)((char*)TOKEN(10) - 0x394);
    orig_u32(0x003B357C) = (uint32_t)(uintptr_t)root;
    GmDataRoot::s_instance = root;
    // Heaps the original's helpers check.
    static int heap_token;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u}) orig_u32(va) = (uint32_t)(uintptr_t)&heap_token;
    AtIoTokenFile::s_heap = &heap_token;
    AtIoStream::s_heap = &heap_token;
    GmNameMap::s_heap = &heap_token;
    GmNameSpaceDb::s_heap = &heap_token;
    g_fill = 0xCD;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_find_level();
    run_paths();
    run_small_slots();
    run_parts();
    run_inits();
    run_loops();
    run_main();
    report_entries();
    if (g_bad_seams) { std::printf("a slot no case should reach was reached %d times\n", g_bad_seams); g_failures++; }
    std::printf("%s: %d inits (%d calls logged on the original side), %d runs of the loop (%d frames, %d random), main; %d crashes on both sides; %llu comparisons, %d differences\n",
                g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_inits, g_logged, g_runs, g_frames_total, g_transitions, (int)g_crashes, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
