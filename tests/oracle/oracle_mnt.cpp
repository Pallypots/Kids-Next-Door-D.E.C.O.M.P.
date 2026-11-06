// Differential oracle for the mount manager (Windows x86, local only). See oracle_tok.cpp and docs/verification.md.
//
//   oracle_mnt <default.xbe> <cases dir> [--entries FILE] [--mutate]
//
// Cases: the synthetic FSTA archive tools/jam.py writes into the archive oracle's directory (synth_fsta.bin). Each side gets its own device, root and
// context; the archive classes beneath are the original's on one side and recovered on the other, already tested by oracle_jam.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtIoMountManager.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 20) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 20) std::printf("DIFF %s: %s: original [%s], recovered [%s]\n", g_context.c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#include "oracle_doubles.h"

// A device that knows its side, reports the archive's size, and logs what it is told.
class Device : public MemFs {
public:
    int side = 0;
    unsigned size = 0;
    int GetFileInfo(const char* path, AtIoFileStat* stat) override {
        g_log[side].add('I');
        strncpy_s(stat->m_name, path, _TRUNCATE);
        stat->m_time = 1;
        stat->m_offset = 0;
        stat->m_size = size;
        return 0;
    }
    void SetRoot(const char* root) override { g_log[side].add('T'); record_message(root, side); }
};

class Factory : public AtIoFileSystemFactory {
public:
    int side = 0;
    Device* made[2] = {};
    void Unknown0() override {}
    void Unknown1() override {}
    void Unknown2() override {}
    void Unknown3() override {}
    void Unknown4() override {}
    AtIoFileSystem* Create(int kind, int a, int b) override {
        g_log[side].add('K', (uint32_t)kind);
        g_log[side].add('a', (uint32_t)a);
        g_log[side].add('b', (uint32_t)b);
        return made[kind == kRoot];
    }
    void Destroy(AtIoFileSystem* fs) override { g_log[side].add('X', fs == made[1]); }
};

class ContextDevice : public AtIoContextDevice {
public:
    int side = 0;
    void Unknown0() override {}
    int Unknown1() override { g_log[side].add('1'); return 0x55; }
};

struct Side {
    Device device, root;
    Factory factory;
    AtIoMountOwner owner;
    AtIoMountParams params;
    AtIoContext context;
    ContextDevice contextDevice;
    AtIoFileSystem* stack[8];
};
static Side* g_sides[2];
static int g_heap_token;

static void setup_side(int k, const std::vector<unsigned char>& archive) {
    Side* s = g_sides[k] = new Side;
    s->device.side = s->root.side = s->factory.side = s->contextDevice.side = k;
    s->device.size = s->root.size = (unsigned)archive.size();
    for (MemFs* m : {(MemFs*)&s->device, (MemFs*)&s->root}) {
        for (int i = 0; i < 2; i++) {
            m->files[i].data = archive.data();
            m->files[i].size = archive.size();
        }
    }
    s->factory.made[0] = &s->device;
    s->factory.made[1] = &s->root;
    std::memset(&s->owner, 0, sizeof s->owner);
    s->owner.m_factory = &s->factory;
    std::memset(&s->params, 0, sizeof s->params);
    s->params.m_owner = &s->owner;
    s->params.m_heap = &g_heap_token;
    s->params.m_context = &s->context;
    *(AtIoContextDevice**)&s->context = &s->contextDevice;
    s->context.m_stack = s->stack;
    s->context.m_stackSize = 8;
}

// The archive header's slots 4 and 5 are empty in the real classes, so nothing shows whether they are called. These replace them in both vftables.
static void __fastcall header_slot4(void*, void*) { if (g_recording) g_log[g_side].add('4'); }
static void __fastcall header_slot5(void*, void*) { if (g_recording) g_log[g_side].add('5'); }

static void patch_header_slots(void* header) {
    void** table = *(void***)header;
    DWORD old;
    VirtualProtect(table, 6 * sizeof(void*), PAGE_READWRITE, &old);
    table[4] = (void*)header_slot4;
    table[5] = (void*)header_slot5;
    VirtualProtect(table, 6 * sizeof(void*), old, &old);
}

typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* FnS)(void*, void*, const char*);
typedef void(__fastcall* FnSS)(void*, void*, const char*, const char*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);

static void same(const char* what, const AtIoMountManager* o, const AtIoMountManager* r) {
    Side* so = g_sides[0];
    Side* sr = g_sides[1];
    eq(what, o->m_owner == &so->owner, r->m_owner == &sr->owner);
    eq(what, o->m_space != nullptr, r->m_space != nullptr);
    eq(what, o->m_region[0], r->m_region[0]);
    eq(what, o->m_context == &so->context, r->m_context == &sr->context);
    eq(what, o->m_factory == &so->factory, r->m_factory == &sr->factory);
    eq(what, o->m_device == &so->device, r->m_device == &sr->device);
    eq(what, o->m_root == &so->root, r->m_root == &sr->root);
    eq(what, o->m_mounted != nullptr, r->m_mounted != nullptr);
    eq(what, o->m_side != nullptr, r->m_side != nullptr);
    eq(what, o->m_unknown28 != nullptr, r->m_unknown28 != nullptr);
    eq(what, o->m_listFile.m_file != nullptr, r->m_listFile.m_file != nullptr);
    eq(what, o->m_listFile.m_fs == &so->root, r->m_listFile.m_fs == &sr->root);
    eq(what, so->context.m_stackCount, sr->context.m_stackCount);
    eq(what, *(intptr_t*)(uintptr_t)0x002E26E4 == (intptr_t)&g_heap_token, AtIoMountManager::s_heap == &g_heap_token);
    for (unsigned i = 0; i < so->context.m_stackCount && i < sr->context.m_stackCount; i++) {
        eq("stack kind", so->stack[i] == &so->device, sr->stack[i] == &sr->device);
        eq("stack kind", so->stack[i] == o->m_mounted, sr->stack[i] == r->m_mounted);
    }
    if (o->m_mounted && r->m_mounted) eqs("mounted name", o->m_mounted->m_archiveName, r->m_mounted->m_archiveName);
    if (o->m_side && r->m_side) eqs("side name", o->m_side->m_archiveName, r->m_side->m_archiveName);
    if (o->m_listFile.m_file && r->m_listFile.m_file) eq("list file is the root's", o->m_listFile.m_file == &so->root.files[0], r->m_listFile.m_file == &sr->root.files[1]);
    eq("list pointer", so->device.m_unknown6B4 != nullptr, sr->device.m_unknown6B4 != nullptr);
    eq("list pointer is the file", so->device.m_unknown6B4 == &o->m_listFile, sr->device.m_unknown6B4 == &r->m_listFile);
}

// Each side runs while its own globals are in place: the two contexts and two instances must not mix.
template <class F> static void both(const char* what, AtIoMountManager* o, AtIoMountManager* r, F f) {
    { On on(0); f(0, o); }
    { On on(1); f(1, r); }
    same(what, o, r);
    compare_logs(what);
}

static void run_lifecycle(const std::vector<unsigned char>& archive) {
    setup_side(0, archive);
    setup_side(1, archive);
    *(AtIoContext**)(uintptr_t)0x002C62F0 = &g_sides[0]->context;
    AtIoContext::s_instance = &g_sides[1]->context;
    *(void**)(uintptr_t)0x00301B50 = *(void**)(uintptr_t)0x00301B54 = *(void**)(uintptr_t)0x00301B5C = *(void**)(uintptr_t)0x00301B60 = &g_heap_token;
    AtIoJamFileSystem::SetHeap(&g_heap_token);
    AtIoJamFileSystem::SetHeaderHeap(&g_heap_token);
    AtIoFastAlignedJamHeader::SetHeap(&g_heap_token);
    AtIoFastJamHeader::SetHeap(&g_heap_token);

    g_context = "construction";
    void* op = std::calloc(1, sizeof(AtIoMountManager));
    std::memset(op, 0xCD, sizeof(AtIoMountManager));  // So a field the constructor must clear shows if it does not.
    at<Fn0>(0x00076860)(op, nullptr);
    eq("vftable", *(intptr_t*)op, 0x00278548);
    eq("instance", *(intptr_t*)(uintptr_t)0x002E26E0, (intptr_t)op);
    AtIoMountManager* o = (AtIoMountManager*)op;
    void* rp = std::malloc(sizeof(AtIoMountManager));
    std::memset(rp, 0xCD, sizeof(AtIoMountManager));
    AtIoMountManager* r = new (rp) AtIoMountManager;
    eq("instance", (intptr_t)AtIoMountManager::s_instance, (intptr_t)r);
    eq("m_owner untouched", (uint32_t)(uintptr_t)o->m_owner, (uint32_t)(uintptr_t)r->m_owner);
    eq("m_context untouched", (uint32_t)(uintptr_t)o->m_context, (uint32_t)(uintptr_t)r->m_context);
    eq("m_space", (intptr_t)o->m_space, (intptr_t)r->m_space);
    eq("m_device", (intptr_t)o->m_device, (intptr_t)r->m_device);
    eq("m_root", (intptr_t)o->m_root, (intptr_t)r->m_root);
    eq("m_unknown28", (intptr_t)o->m_unknown28, (intptr_t)r->m_unknown28);
    eq("m_factory", (intptr_t)o->m_factory, (intptr_t)r->m_factory);
    eq("m_mounted", (intptr_t)o->m_mounted, (intptr_t)r->m_mounted);
    eq("m_side", (intptr_t)o->m_side, (intptr_t)r->m_side);
    eq("m_region", o->m_region[0], r->m_region[0]);
    eq("heap", *(intptr_t*)(uintptr_t)0x002E26E4, (intptr_t)AtIoMountManager::s_heap);
    eq("m_listFile", (intptr_t)o->m_listFile.m_file, (intptr_t)r->m_listFile.m_file);

    g_context = "init";
    both("Init", o, r, [](int side, AtIoMountManager* m) { m->Init(&g_sides[side]->params); });
    eq("space instance", *(intptr_t*)(uintptr_t)0x00301B4C, (intptr_t)o->m_space);
    eq("space instance", (intptr_t)AtIoSpaceManager::s_instance, (intptr_t)r->m_space);
    eq("space heap", *(intptr_t*)(uintptr_t)0x00301B48, (intptr_t)&g_heap_token);
    eq("space heap", (intptr_t)AtIoSpaceManager::s_heap, (intptr_t)&g_heap_token);
    eq("space device", (intptr_t)o->m_space->m_device, (intptr_t)&g_sides[0]->contextDevice);
    eq("space device", (intptr_t)r->m_space->m_device, (intptr_t)&g_sides[1]->contextDevice);
    g_context = "init again";
    both("Init again", o, r, [](int side, AtIoMountManager* m) { m->Init(&g_sides[side]->params); });
    g_sides[0]->context.m_stackCount = g_sides[1]->context.m_stackCount = 1;  // Init does not pop: the second pushed the device again.

    g_context = "region";
    for (const char* name : {"ABCDEFGH", "SHORT\0\0", "E"}) {
        char padded[16] = {0};
        strncpy_s(padded, name, _TRUNCATE);
        both("SetRegion", o, r, [&](int, AtIoMountManager* m) { m->SetRegion(padded); });
        eq("region", std::memcmp(o->m_region, r->m_region, 8), 0);
    }
    both("SetRegion null", o, r, [](int, AtIoMountManager* m) { m->SetRegion(nullptr); });

    g_context = "mount";
    const auto mount = [&](const char* name, const char* dir) {
        { On on(0); at<FnSS>(0x000766E0)(o, nullptr, name, dir); }
        { On on(1); r->Mount(name, dir); }
        same("Mount", o, r);
        compare_logs("Mount");
    };
    const auto unmount = [&]() {
        { On on(0); at<Fn0>(0x00076830)(o, nullptr); }
        { On on(1); r->Unmount(); }
        same("Unmount", o, r);
        compare_logs("Unmount");
    };
    unmount();  // Nothing mounted.
    mount("AppInit", "");
    patch_header_slots(o->m_mounted->m_header);
    patch_header_slots(r->m_mounted->m_header);
    // Directly too, with a mutant-free view of each slot.
    typedef void(__fastcall* FnNotify)(void*, void*);
    { On on(0); at<FnNotify>(0x0007D140)(o->m_mounted, nullptr); at<FnNotify>(0x0007D150)(o->m_mounted, nullptr); }
    { On on(1); AtIoNotifyMounted(r->m_mounted); AtIoNotifyUnmounted(r->m_mounted); }
    compare_logs("notifications");
    unmount();
    mount("Lbkyrd", "Levels");
    unmount();
    unmount();
    g_sides[0]->device.has = g_sides[1]->device.has = false;
    mount("Missing", "");  // Skipped silently.
    unmount();
    g_sides[0]->device.has = g_sides[1]->device.has = true;

    g_context = "mount path";
    const auto mount_path = [&](const std::string& path) {
        { On on(0); at<FnS>(0x00076790)(o, nullptr, path.c_str()); }
        { On on(1); r->MountPath(path.c_str()); }
        same("MountPath", o, r);
        compare_logs("MountPath");
        unmount();
    };
    mount_path("UI\\Xbox\\CoDialog");
    mount_path(std::string(0xFB, 'p'));  // The longest that fits.
    mount_path(std::string(0xFC, 'p'));  // Too long: ignored.
    g_sides[0]->device.has = g_sides[1]->device.has = false;
    mount_path("Missing");
    g_sides[0]->device.has = g_sides[1]->device.has = true;

    g_context = "side archive";
    const auto swap_side = [&](const std::string& path) {
        { On on(0); at<FnS>(0x00076B20)(o, nullptr, path.c_str()); }
        { On on(1); r->SwapSideArchive(path.c_str()); }
        same("SwapSideArchive", o, r);
        compare_logs("SwapSideArchive");
    };
    const auto drop_side = [&]() {
        { On on(0); at<Fn0>(0x00076A80)(o, nullptr); }
        { On on(1); r->DropSideArchive(); }
        same("DropSideArchive", o, r);
        compare_logs("DropSideArchive");
    };
    drop_side();  // None.
    swap_side("One");
    eq("not pushed", g_sides[0]->context.m_stackCount, 1);
    swap_side("Two");  // Drops the first.
    drop_side();
    swap_side(std::string(0xFB, 's'));
    swap_side(std::string(0xFC, 's'));  // Too long: the old one is still dropped.
    swap_side("Three");
    // On top of the stack, it is popped when dropped. Beneath something else, it stays listed; as in the original that is left dangling, so it is removed by hand.
    {
        AtIoFileSystem* pushed[2] = {o->m_side, r->m_side};
        g_sides[0]->context.Push(pushed[0]);
        g_sides[1]->context.Push(pushed[1]);
        drop_side();
        eq("popped", g_sides[0]->context.m_stackCount, 1);
    }
    swap_side("Four");
    {
        AtIoFileSystem* pushed[2] = {o->m_side, r->m_side};
        g_sides[0]->context.Push(pushed[0]);
        g_sides[1]->context.Push(pushed[1]);
        g_sides[0]->context.Push(&g_sides[0]->root);
        g_sides[1]->context.Push(&g_sides[1]->root);
        { On on(0); at<Fn0>(0x00076A80)(o, nullptr); }
        { On on(1); r->DropSideArchive(); }
        compare_logs("DropSideArchive, not on top");
        eq("not popped", g_sides[0]->context.m_stackCount, g_sides[1]->context.m_stackCount);
        eq("not popped", g_sides[1]->context.m_stackCount, 3);
        g_sides[0]->context.m_stackCount = g_sides[1]->context.m_stackCount = 1;
    }

    g_context = "list file";
    std::string written[2];
    g_sides[0]->root.files[0].written = &written[0];
    g_sides[1]->root.files[1].written = &written[1];
    g_sides[0]->root.m_path.Init();
    g_sides[1]->root.m_path.Init();
    g_sides[0]->root.m_path.Push("KEEP");
    g_sides[1]->root.m_path.Push("KEEP");
    g_sides[0]->root.m_path.Push("DROP");
    g_sides[1]->root.m_path.Push("DROP");
    const int top_before = g_sides[1]->root.m_path.m_top;
    const auto open_list = [&](const char* what, const char* name) {
        { On on(0); at<FnS>(0x00076990)(o, nullptr, name); }
        { On on(1); r->OpenListFile(name); }
        same(what, o, r);
        compare_logs(what);
    };
    const auto close_list = [&](const char* what) {
        { On on(0); at<Fn0>(0x000769D0)(o, nullptr); }
        { On on(1); r->CloseListFile(); }
        same(what, o, r);
        compare_logs(what);
    };
    open_list("OpenListFile", "AppInit.txt");
    eq("path popped", g_sides[0]->root.m_path.m_top, g_sides[1]->root.m_path.m_top);
    eq("path popped", g_sides[1]->root.m_path.m_top, top_before - 1);
    eq("flags", g_sides[0]->root.files[0].m_flags, g_sides[1]->root.files[1].m_flags);
    { On on(0); at<FnS>(0x00076670)(o, nullptr, "a line of text"); }
    { On on(1); r->WriteList("a line of text"); }
    same("WriteList", o, r);
    compare_logs("WriteList");
    eq("written", written[0] == written[1] && !written[1].empty(), 1);
    close_list("CloseListFile");
    g_sides[0]->root.open_status = g_sides[1]->root.open_status = 9;
    open_list("OpenListFile, failing", "Bad.txt");
    g_sides[0]->root.open_status = g_sides[1]->root.open_status = 0;
    g_sides[0]->root.files[0].written = nullptr;
    g_sides[1]->root.files[1].written = nullptr;
    close_list("CloseListFile again");

    g_context = "shutdown";
    o->m_unknown28 = &g_sides[0]->root;
    r->m_unknown28 = &g_sides[1]->root;
    o->m_mounted = r->m_mounted = (AtIoJamFileSystem*)(uintptr_t)4;  // Not freed by Shutdown, only forgotten.
    o->m_side = r->m_side = (AtIoJamFileSystem*)(uintptr_t)4;
    o->m_region[0] = r->m_region[0] = 'Q';
    { On on(0); o->Shutdown(); }
    { On on(1); r->Shutdown(); }
    eq("mounted forgotten", (intptr_t)o->m_mounted, (intptr_t)r->m_mounted);
    eq("side forgotten", (intptr_t)o->m_side, (intptr_t)r->m_side);
    eq("region cleared", o->m_region[0], r->m_region[0]);
    same("Shutdown", o, r);
    compare_logs("Shutdown");
    eq("space instance gone", *(intptr_t*)(uintptr_t)0x00301B4C, 0);
    eq("space instance gone", (intptr_t)AtIoSpaceManager::s_instance, 0);
    both("Shutdown again", o, r, [](int, AtIoMountManager* m) { m->Shutdown(); });
    // No factory: nothing destroyed even with file systems set.
    o->m_device = &g_sides[0]->device;
    r->m_device = &g_sides[1]->device;
    both("Shutdown without a factory", o, r, [](int, AtIoMountManager* m) { m->Shutdown(); });

    g_context = "no heap";
    g_sides[0]->params.m_heap = g_sides[1]->params.m_heap = nullptr;
    both("Init without a heap", o, r, [](int side, AtIoMountManager* m) { m->Init(&g_sides[side]->params); });
    both("Shutdown without a heap", o, r, [](int, AtIoMountManager* m) { m->Shutdown(); });
    g_sides[0]->params.m_heap = g_sides[1]->params.m_heap = &g_heap_token;

    g_context = "space manager";
    AtIoSpaceManager a, b;
    a.m_device = b.m_device = (AtIoContextDevice*)(uintptr_t)4;
    a.m_unknown04 = b.m_unknown04 = (void*)(uintptr_t)5;
    a.m_unknown08 = b.m_unknown08 = 6;
    a.m_unknown0C = b.m_unknown0C = 7;
    at<Fn0>(0x0007CEF0)(&a, nullptr);
    b.Clear();
    eq("cleared", std::memcmp(&a, &b, sizeof a), 0);
    eq("cleared to zero", (intptr_t)a.m_unknown04 + a.m_unknown08 + a.m_unknown0C + (intptr_t)a.m_device, 0);
    // Init's result is passed on from the device.
    both("Init", o, r, [](int side, AtIoMountManager* m) { m->Init(&g_sides[side]->params); });
    g_sides[0]->context.m_stackCount = g_sides[1]->context.m_stackCount = 1;
    typedef int(__fastcall* FnI)(void*, void*);
    int x, y;
    { On on(0); x = at<FnI>(0x0007D000)(o->m_space, nullptr); }
    { On on(1); y = r->m_space->Init(); }
    eq("result", x, y);
    eq("result", y, 0x55);
    compare_logs("space Init");
    // An object with a mounted archive through its destructors: the complete form, then the deleting one.
    mount("AppInit", "");
    g_sides[0]->context.m_stackCount = g_sides[1]->context.m_stackCount = 1;
    { On on(0); at<Fn0>(0x00076AC0)(o, nullptr); }
    { On on(1); r->~AtIoMountManager(); }
    compare_logs("destructor");
    eq("instance cleared", *(intptr_t*)(uintptr_t)0x002E26E0, 0);
    eq("instance cleared", (intptr_t)AtIoMountManager::s_instance, 0);
    eq("base vftable", *(intptr_t*)o, 0x00278548);
    void* second = std::calloc(1, sizeof(AtIoMountManager));
    at<Fn0>(0x00076860)(second, nullptr);
    AtIoMountManager* r2 = new AtIoMountManager;
    { On on(0); at<FnDel>(0x00076BF0)(second, nullptr, 1); }
    { On on(1); delete r2; }
    compare_logs("deleting destructor");
    void* third = std::calloc(1, sizeof(AtIoMountManager));
    at<Fn0>(0x00076860)(third, nullptr);
    at<FnDel>(0x00076BF0)(third, nullptr, 0);  // Not freed by the call.
    std::free(third);
    std::free(op);
    std::free(rp);
    // The heap setters and notifications are reached above; the delete helper's null path needs its own call.
    typedef void(__fastcall* FnDelObj)(void*, void*, void*);
    at<FnDelObj>(0x000EB9D0)(nullptr, nullptr, nullptr);
    AtIoDeleteObject(nullptr);
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        char text[256];
        const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p during [%s]\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, g_context.c_str());
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
    if (pos.size() != 2) { std::printf("usage: oracle_mnt <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    if (entries && !arm_entries(entries)) return 2;

    std::vector<unsigned char> archive;
    {
        FILE* f = nullptr;
        const std::string path = std::string(pos[1]) + "\\synth_fsta.bin";
        if (fopen_s(&f, path.c_str(), "rb") || !f) { std::printf("cannot read %s\n", path.c_str()); return 2; }
        std::fseek(f, 0, SEEK_END);
        archive.resize((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        std::fread(archive.data(), 1, archive.size(), f);
        std::fclose(f);
    }
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_lifecycle(archive);
    report_entries();
    std::printf("%s: %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
