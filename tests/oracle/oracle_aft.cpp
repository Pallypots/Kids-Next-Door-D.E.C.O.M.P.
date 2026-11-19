// Differential oracle for the Flash user interface's texts (.AFT, with their fonts and formats) and scripts (.AFA, with the action script machine's state), and for whole .AFD levels through them (Windows x86, local only).
// See docs/verification.md.
//
//   oracle_aft <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Real files: every user interface archive in the JAM cases beside the tokenizer cases (..\oracle-jam): each one's .AFT, .AFX, .AFA and Font.AGF are served from its archive, as the game would find them; the text table, the
// script table and the whole .AFD are loaded on both sides and compared. Then written text for each word, branch and error path, and the values, their stack and the machine's state called directly.
//
// What stands in, the same on both sides, each logged with its arguments: the strings' load (AtTextTable::Load 0x0001FA70, suite txt), the font database's load (AtFontDatabase::Load 0x0002A290, suite agn) and its
// lookup (GmNameMap::Find 0x00025150, suite gm), the graphics database loader and its texture count (0x0001ADC0, 0x0001C6F0, suite agd), and the material owner, its graphics factory, the font loader the factory makes
// and the fonts. The formats' reader (0x00020310) and every sprite and shape reader run for real on both sides.
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "at/AtAudioBank.h"
#include "at/AtDatabaseLoader.h"
#include "at/AtFlashLevel.h"
#include "at/AtHeap.h"
#include "at/AtImageDatabase.h"
#include "at/AtIoTokenizer.h"
#include "at/AtMemory.h"
#include "at/AtTextureDatabase.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate, g_trace;
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
template <class M> static uint32_t addr_of(M m) {
    static_assert(sizeof(M) == 4, "a plain member function");
    union { M m; uint32_t a; } u;
    u.m = m;
    return u.a;
}

#include "oracle_doubles.h"

AtHeap* AtHeapStack::s_contentHeap;

// No room from the Nth heap push on, on each side.
static int g_fail_at_push, g_pushes;
static void __cdecl push_counting(void* h) {
    if (g_recording) g_log[g_side].add('P', (uint32_t)(uintptr_t)h);
    if (g_fail_at_push && ++g_pushes == g_fail_at_push) g_fail_alloc = true;
}

static int g_heap_token, g_heap_a, g_heap_b;

typedef void(__fastcall* Fn0)(void*, void*);
typedef void*(__fastcall* FnCtor)(void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__fastcall* Fn1)(void*, void*, uintptr_t);
typedef int(__fastcall* FnInt1)(void*, void*, uintptr_t);
typedef void(__fastcall* Fn2)(void*, void*, uintptr_t, uintptr_t);
typedef void(__fastcall* Fn3)(void*, void*, uintptr_t, uintptr_t, uintptr_t);
typedef void(__fastcall* Fn4)(void*, void*, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
typedef void(__cdecl* FnStatic)(void*);

// ---- files: an archive's, by path ---------------------------------------------------------------------------------------

static std::map<std::string, std::string> g_files;
static std::string upper(std::string s) {
    for (char& c : s) c = (char)toupper((unsigned char)c);
    return s;
}

class JamFs : public MemFs {
public:
    MemFile pool[2][8];
    int next[2] = {};
    const std::string* find(const char* path) {
        const bool was = g_recording;
        g_recording = false;
        const char* current = m_path.Current();
        auto it = g_files.find(upper(std::string(current ? current : "") + path));
        g_recording = was;
        return it == g_files.end() ? nullptr : &it->second;
    }
    int Open(const char* path, int flags, AtIoFile** out) override {
        g_log[g_side].add('O', (uint32_t)flags);
        record_message(path, g_side);
        const std::string* data = find(path);
        if (open_status || data == nullptr) {
            *out = nullptr;
            return open_status ? open_status : 2;
        }
        MemFile& f = pool[g_side][next[g_side]++ & 7];
        f.side = g_side;
        f.data = (const unsigned char*)data->data();
        f.size = data->size();
        f.m_position = 0;
        f.reads = 0;
        f.m_flags = 2;
        f.mode = kNormal;
        *out = &f;
        return 0;
    }
    int IsFileMissing(const char* path) override {
        g_log[g_side].add('M');
        record_message(path, g_side);
        return find(path) ? 0 : 1;
    }
};

static JamFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];

// ---- what stands in --------------------------------------------------------------------------------------------------

class FakeLoader : public AtDatabaseLoader {
public:
    void MakeTextures() override {}
    void MakeMaterials() override {}
    void MakeShaders() override {}
    void MakeMeshes() override {}
    void MakeLights() override {}
    void Slot5() override {}
    void MakeFonts() override {}
    void MakeImages() override {}
    void MakeVertexProcesses() override {}
    void MakePixelProcesses() override {}
    void MakeLoaders() override {}
    void Unload() override { if (g_recording) g_log[g_side].add('U'); }
    AtTextureDatabase* TextureAt(unsigned int) override { return (AtTextureDatabase*)&g_heap_a; }
    AtMaterialDatabase* MaterialAt(unsigned int) override { return nullptr; }
    AtShaderDatabase* ShaderAt(unsigned int) override { return nullptr; }
    AtMeshDatabase* MeshAt(unsigned int) override { return nullptr; }
    AtLightDatabase* LightAt(unsigned int) override { return nullptr; }
    AtFontDatabase* FontAt(unsigned int) override { return nullptr; }
    AtImageDatabase* ImageAt(unsigned int) override { return nullptr; }
    AtVertexProcessDatabase* VertexProcessAt(unsigned int) override { return nullptr; }
    AtPixelProcessDatabase* PixelProcessAt(unsigned int) override { return nullptr; }
    AtDatabaseLoader* LoaderAt(unsigned int) override { return nullptr; }
};
static FakeLoader* g_loaders[2];

class FakeFontLoader : public AtFlashFontLoader {
public:
    int side = 0;
    void Load(AtMaterialOwner* owner, const char* name, int mode) override;
};
static FakeFontLoader g_font_loaders[2];

// The font database: only a block to hand out and look in; its load and lookup are this file's.
static unsigned char g_font_db[2][0x100];
static unsigned g_font_count = 3;

class FakeFont : public AtFontItem {
public:
    int number = 0;
    void Slot0() override {}
    void Slot1() override {}
    void Slot2() override {}
    void Slot3() override {}
    void Slot4() override {}
    void Slot5() override {}
    void Slot6() override {}
    void Read(AtMaterialOwner*, AtFontDatabase*, const char*, AtIoTokenizer*) override {}
    void Slot8() override {}
    void OnLoaded(AtMaterialOwner*) override {}
    void Unload() override { if (g_recording) g_log[g_side].add('u', (uint32_t)number); }
};
static FakeFont g_fonts[2][2];  // per side: "F_Stand", "F_Big"

class FakeFactory : public AtFlashGraphicsFactory {
public:
    AtDatabaseLoader* MakeLoader() override { if (g_recording) g_log[g_side].add('L'); return g_loaders[g_side]; }
    void FreeLoader(AtDatabaseLoader* loader) override { if (g_recording) g_log[g_side].add('K', loader == g_loaders[g_side]); }
    AtFlashFontLoader* MakeFontLoader() override { if (g_recording) g_log[g_side].add('9'); return &g_font_loaders[g_side]; }
    void FreeFontLoader(AtFlashFontLoader* l) override { if (g_recording) g_log[g_side].add('a', l == &g_font_loaders[g_side]); }
    AtFontDatabase* MakeFontDatabase() override { if (g_recording) g_log[g_side].add('b'); return (AtFontDatabase*)g_font_db[g_side]; }
    void FreeFontDatabase(AtFontDatabase* d) override { if (g_recording) g_log[g_side].add('c', d == (AtFontDatabase*)g_font_db[g_side]); }
};
static FakeFactory g_factory;

struct FakeOwner : AtMaterialOwner {
    AtFlashGraphicsFactory* GraphicsFactory() override { if (g_recording) g_log[g_side].add('G'); return &g_factory; }
};
static FakeOwner* g_owner;
static AtFlashGraphics g_graphics;
static AtFlashApplication g_app;
static AtFlashEnvironment g_env;

void FakeFontLoader::Load(AtMaterialOwner* owner, const char* name, int mode) {
    if (!g_recording) return;
    g_log[g_side].add('l', (uint32_t)mode | (owner == g_owner) << 8 | (this == &g_font_loaders[g_side]) << 9);
    record_message(name, g_side);
}

static void __fastcall strings_load(AtTextTable* self, void*, AtIoContext* context, const char* path, int unused) {
    if (g_recording) {
        g_log[g_side].add('S', (uint32_t)unused);
        g_log[g_side].add('s', context == &g_ctx);
        record_message(path, g_side);
        // The directory it is loaded in.
        const char* current = g_fs->m_path.Current();
        record_message(current ? current : "(none)", g_side);
    }
    // As if loaded, so that a Clear shows.
    self->m_loaded = 1;
    self->m_stringCount = 7;
    self->m_stringsUsed = 7;
}
static void __fastcall font_load(AtFontDatabase* self, void*, AtMaterialOwner* owner, const char* name, int unused) {
    if (g_recording) {
        g_log[g_side].add('f', (uint32_t)unused | (owner == g_owner) << 8 | ((unsigned char*)self == g_font_db[g_side]) << 9);
        record_message(name, g_side);
    }
    self->m_count = g_font_count;
}
static void* __fastcall font_find(GmNameMap* self, void*, const char* name) {
    if (g_recording) {
        g_log[g_side].add('n', (unsigned char*)self == g_font_db[g_side]);
        record_message(name, g_side);
    }
    if (std::strncmp(name, "F_Stand", 8) == 0) return &g_fonts[g_side][0];
    if (std::strncmp(name, "F_Big", 8) == 0) return &g_fonts[g_side][1];
    return nullptr;
}
static unsigned g_texture_dbs = 1;
static void __fastcall loader_load(AtDatabaseLoader* self, void*, AtMaterialOwner* owner, const char* name, int mode, const char*, int) {
    if (g_recording) {
        g_log[g_side].add('A', (uint32_t)mode | (owner == g_owner) << 8);
        record_message(name, g_side);
    }
    self->m_textureCount = (unsigned short)g_texture_dbs;
}
static unsigned __fastcall count_used(void*, void*) { return 3; }

// ---- comparing --------------------------------------------------------------------------------------------------------

static void same_bytes(const char* what, const void* o, const void* r, size_t from, size_t to) {
    const unsigned char* a = (const unsigned char*)o;
    const unsigned char* b = (const unsigned char*)r;
    for (size_t i = from; i < to; i++) {
        if (a[i] != b[i]) {
            eq((std::string(what) + " byte " + std::to_string(i)).c_str(), a[i], b[i]);
            return;
        }
    }
    g_checks++;
}

static void same_base(const AtFlashResourceTable* o, const AtFlashResourceTable* r) {
    eq("initialised", o->m_initialised, r->m_initialised);
    eq("first group", o->m_firstGroup != nullptr, r->m_firstGroup != nullptr);
    eq("last group", o->m_lastGroup != nullptr, r->m_lastGroup != nullptr);
    int n = 0;
    const AtFlashResourceTable::Group* a = o->m_firstGroup;
    const AtFlashResourceTable::Group* b = r->m_firstGroup;
    for (; a && b && n < 64; a = a->m_next, b = b->m_next, n++) {
        eq("group's first", a->m_first != nullptr, b->m_first != nullptr);
        eq("group's back link", a->m_prev == nullptr, b->m_prev == nullptr);
    }
    eq("groups end together", a == nullptr, b == nullptr);
    eq("name", std::memcmp(o->m_name, r->m_name, 8), 0);
    eq("items", o->m_items != nullptr, r->m_items != nullptr);
    eq("item count", o->m_itemCount, r->m_itemCount);
    eq("group count", o->m_groupCount, r->m_groupCount);
    eq("heap", (uintptr_t)o->m_heap, (uintptr_t)r->m_heap);
}

static void same_name(const char* what, const char* o, const char* r) {
    eq(what, o != nullptr, r != nullptr);
    if (o && r) eqs(what, o, r);
}

static unsigned g_texts, g_formats, g_scripts, g_instructions, g_font_tables;

static void same_strings(const AtTextTable* o, const AtTextTable* r) {
    same_bytes("strings", o, r, 4, sizeof(AtTextTable));
}

static void same_fonts(const AtFlashFontTable* o, const AtFlashFontTable* r, bool count) {
    eq("fonts' vftable", *(const uint32_t*)o, 0x0027275C);
    same_base(o, r);
    eq("fonts loaded", o->m_loaded, r->m_loaded);
    eq("font database", o->m_fonts == nullptr ? 0 : (unsigned char*)o->m_fonts == g_font_db[0] ? 1 : 2, r->m_fonts == nullptr ? 0 : (unsigned char*)r->m_fonts == g_font_db[1] ? 1 : 2);
    eq("font loader", o->m_fontLoader == nullptr ? 0 : o->m_fontLoader == &g_font_loaders[0] ? 1 : 2, r->m_fontLoader == nullptr ? 0 : r->m_fontLoader == &g_font_loaders[1] ? 1 : 2);
    eq("owner", (uintptr_t)o->m_owner, (uintptr_t)r->m_owner);
    eq("factory", (uintptr_t)o->m_factory, (uintptr_t)r->m_factory);
    eq("ref count", o->m_refCount, r->m_refCount);
    eq("refs", o->m_refs != nullptr, r->m_refs != nullptr);
    if (o->m_refs && r->m_refs && o->m_refCount == r->m_refCount) {
        for (unsigned i = 0; i < o->m_refCount && i < 64; i++) {
            eq("ref's references", o->m_refs[i].m_references, r->m_refs[i].m_references);
            const void* a = o->m_refs[i].m_font;
            const void* b = r->m_refs[i].m_font;
            eq("ref's font", a == &g_fonts[0][0] ? 1 : a == &g_fonts[0][1] ? 2 : (long long)(uintptr_t)a, b == &g_fonts[1][0] ? 1 : b == &g_fonts[1][1] ? 2 : (long long)(uintptr_t)b);
        }
    }
    if (count && o->m_fonts) g_font_tables++;
}

static void same_formats(const AtFlashTextFormatTable* o, const AtFlashTextFormatTable* r, bool count) {
    eq("formats' vftable", *(const uint32_t*)o, 0x0027294C);
    same_base(o, r);
    eq("formats loaded", o->m_loaded, r->m_loaded);
    if (o->m_items && r->m_items && o->m_itemCount == r->m_itemCount) {
        for (unsigned i = 0; i < o->m_itemCount; i++) {
            eq("format held", o->m_items[i] != nullptr, r->m_items[i] != nullptr);
            if (!o->m_items[i] || !r->m_items[i]) continue;
            eq("format's vftable", *(const uint32_t*)o->m_items[i], 0x002749D0);
            same_bytes("format", o->m_items[i], r->m_items[i], 4, sizeof(AtFlashTextFormat));
            if (count) g_formats++;
        }
    }
}

static void same_text(const AtFlashText* o, const AtFlashText* r, uint32_t vft = 0x00272750) {
    eq("text's vftable", *(const uint32_t*)o, vft);
    same_bytes("text", o, r, 4, offsetof(AtFlashText, m_variable));
    same_bytes("text", o, r, offsetof(AtFlashText, m_variable) + 4, sizeof(AtFlashText));
    same_name("text's variable", o->m_variable, r->m_variable);
}

static void same_texts(const AtFlashTextTable* o, const AtFlashTextTable* r, bool count) {
    eq("texts' vftable", *(const uint32_t*)o, 0x002712D8);
    same_base(o, r);
    eq("texts loaded", o->m_loaded, r->m_loaded);
    eq("texts +2C", o->m_unknown2C, r->m_unknown2C);
    same_strings(&o->m_strings, &r->m_strings);
    same_fonts(&o->m_fonts, &r->m_fonts, count);
    same_formats(&o->m_formats, &r->m_formats, count);
    if (o->m_items && r->m_items && o->m_itemCount == r->m_itemCount && o->m_loaded) {
        for (unsigned i = 0; i < o->m_itemCount; i++) {
            eq("text held", o->m_items[i] != nullptr, r->m_items[i] != nullptr);
            if (!o->m_items[i] || !r->m_items[i]) continue;
            eq("text's place", (char*)o->m_items[i] - (char*)o->m_items[0], (char*)r->m_items[i] - (char*)r->m_items[0]);
            same_text((const AtFlashText*)o->m_items[i], (const AtFlashText*)r->m_items[i]);
            if (count) g_texts++;
        }
    }
}

static void same_value(const AtFpActionScriptValue* o, const AtFpActionScriptValue* r) {
    eq("value's vftable", *(const uint32_t*)o, 0x00272964);
    eq("value's type", o->m_type, r->m_type);
    eq("value's text", o->m_text != nullptr, r->m_text != nullptr);
    if (o->m_text && r->m_text) same_bytes("value's text", o->m_text, r->m_text, 0, std::min<unsigned>(o->m_capacity, 0x100));
    same_bytes("value", o, r, offsetof(AtFpActionScriptValue, m_capacity), offsetof(AtFpActionScriptValue, m_unknown24));
    eq("value's +24", o->m_unknown24 != nullptr, r->m_unknown24 != nullptr);
    eq("value's heap", (uintptr_t)o->m_heap, (uintptr_t)r->m_heap);
}

static void same_stack(const AtFpActionScriptStack* o, const AtFpActionScriptStack* r) {
    eq("stack allocated", o->m_allocated, r->m_allocated);
    eq("stack's heap", (uintptr_t)o->m_heap, (uintptr_t)r->m_heap);
    eq("stack's top", o->m_top, r->m_top);
    eq("stack's capacity", o->m_capacity, r->m_capacity);
    eq("stack's values", o->m_values != nullptr, r->m_values != nullptr);
    if (o->m_values && r->m_values) {
        const unsigned n = ((const unsigned*)o->m_values)[-1];
        eq("values' count", n, ((const unsigned*)r->m_values)[-1]);
        if (n == ((const unsigned*)r->m_values)[-1]) {
            for (unsigned i = 0; i < n && i < 64; i++) same_value(&o->m_values[i], &r->m_values[i]);
        }
    }
}

static void same_vm(const AtFpActionScriptVM* o, const AtFpActionScriptVM* r, const void* ob, const void* rb) {
    eq("vm's strings", o->m_strings ? (char*)o->m_strings - (char*)ob : -1, r->m_strings ? (char*)r->m_strings - (char*)rb : -1);
    eq("vm's application", (uintptr_t)o->m_application, (uintptr_t)r->m_application);
    eq("vm's heap", (uintptr_t)o->m_heap, (uintptr_t)r->m_heap);
    same_stack(&o->m_stack, &r->m_stack);
}

static void same_script(const AtFpScript* o, const AtFpScript* r, bool count, uint32_t vft = 0x002725B0) {
    eq("script's vftable", *(const uint32_t*)o, vft);
    same_bytes("script's resource", o, r, 4, sizeof(AtFlashResource));
    eq("script's instruction count", o->m_instructionCount, r->m_instructionCount);
    eq("script's read", o->m_read, r->m_read);
    eq("script's id", o->m_id, r->m_id);
    eq("script's heap", (uintptr_t)o->m_heap, (uintptr_t)r->m_heap);
    eq("script's bytes", o->m_bytes != nullptr, r->m_bytes != nullptr);
    eq("script's instructions", o->m_instructions != nullptr, r->m_instructions != nullptr);
    if (!o->m_bytes || !r->m_bytes || !o->m_instructions || !r->m_instructions) return;
    size_t last = 0;
    for (unsigned i = 0; i < o->m_read && i < o->m_instructionCount; i++) {
        const long long a = o->m_instructions[i] - o->m_bytes, b = r->m_instructions[i] - r->m_bytes;
        eq("instruction's place", a, b);
        if (a >= 0 && (size_t)a > last) last = (size_t)a;
    }
    // Past the last instruction's start, up to what the heap's padding allows: the bytes made room for are not kept.
    same_bytes("script's bytes", o->m_bytes, r->m_bytes, 0, last + 0x100);
    if (count) {
        g_scripts++;
        g_instructions += o->m_read;
    }
}

static void same_scripts(const AtFlashScriptTable* o, const AtFlashScriptTable* r, bool count) {
    eq("scripts' vftable", *(const uint32_t*)o, 0x0027125C);
    same_base(o, r);
    eq("scripts loaded", o->m_loaded, r->m_loaded);
    eq("application", (uintptr_t)o->m_application, (uintptr_t)r->m_application);
    eq("script count", o->m_count, r->m_count);
    eq("scripts' heap", (uintptr_t)o->m_heap, (uintptr_t)r->m_heap);
    same_strings(&o->m_strings, &r->m_strings);
    same_vm(&o->m_vm, &r->m_vm, o, r);
    eq("scripts' list", o->m_scripts != nullptr, r->m_scripts != nullptr);
    if (o->m_scripts && r->m_scripts && o->m_count == r->m_count && o->m_loaded) {
        for (unsigned i = 0; i < o->m_count; i++) {
            eq("script held", o->m_scripts[i] != nullptr, r->m_scripts[i] != nullptr);
            if (o->m_scripts[i] && r->m_scripts[i]) same_script(o->m_scripts[i], r->m_scripts[i], count);
        }
    }
}

// ---- making and running ----------------------------------------------------------------------------------------------

template <class T> static T* make(uint32_t ctor, unsigned char fill, bool original) {
    unsigned char* p = (unsigned char*)std::malloc(sizeof(T) + 0x40);
    std::memset(p, fill, sizeof(T) + 0x40);
    if (original) {
        eq("constructor returns this", (uintptr_t)at<FnCtor>(ctor)(p, nullptr), (uintptr_t)p);
        return (T*)p;
    }
    return new (p) T;
}

static int g_path_after[2];
template <class F> static bool run_side(int side, F f) {
    if (g_fail_at_push) {
        g_fail_alloc = false;
        g_pushes = 0;
    }
    g_fs->m_path.m_top = 0;
    if (g_trace) std::printf("run %d [%s]\n", side, g_context.c_str());
    const bool crashed = guarded([&] { On on(side); f(); });
    if (g_trace && crashed) std::printf("crashed %d\n", side);
    g_path_after[side] = g_fs->m_path.m_top;
    g_recording = false;
    g_side = 0;
    g_fail_alloc = false;
    return crashed;
}

// Both sides' statics and the heaps they fall back on.
static void set_statics(void* fallback) {
    *(void**)(uintptr_t)0x002C6270 = fallback;
    AtFlashResourceTable::s_defaultHeap = fallback;
    *(void**)(uintptr_t)0x002C60B8 = fallback;
    AtFlashScriptTable::s_defaultHeap = fallback;
    *(void**)(uintptr_t)0x002C62E8 = fallback;
    AtFpScript::s_heap = fallback;
    *(void**)(uintptr_t)0x002C62EC = fallback;
    AtFpActionScriptVM::s_heap = fallback;
    *(void**)(uintptr_t)0x002C6390 = fallback;
    AtFpActionScriptStack::s_heap = fallback;
    *(void**)(uintptr_t)0x002C6318 = fallback;
    AtFpActionScriptValue::s_heap = fallback;
}

static void same_statics() {
    eq("stack's static heap", (uintptr_t)*(void**)(uintptr_t)0x002C6390, (uintptr_t)AtFpActionScriptStack::s_heap);
}

// A member in the first group, counted in no times.
static AtFlashResource g_member[2], g_last_member[2];
static void add_member(AtFlashResourceTable* t, int side) {
    if (!t->m_firstGroup) return;
    std::memset(&g_member[side], 0x44, sizeof g_member[side]);
    g_member[side].m_uses = 0;
    g_member[side].m_groupNext = nullptr;
    g_member[side].m_groupPrev = nullptr;
    t->m_firstGroup->m_first = t->m_firstGroup->m_last = &g_member[side].m_groupNext;
    // And one in the last group, which only a Clear that recycles every group reaches.
    if (t->m_lastGroup && t->m_lastGroup != t->m_firstGroup) {
        std::memset(&g_last_member[side], 0x44, sizeof g_last_member[side]);
        g_last_member[side].m_uses = 0;
        g_last_member[side].m_groupNext = nullptr;
        g_last_member[side].m_groupPrev = nullptr;
        t->m_lastGroup->m_first = t->m_lastGroup->m_last = &g_last_member[side].m_groupNext;
    }
}

// The texts alone: load, compare, load again over it, clear twice, destroy. held: references to give fonts before the clear.
static void run_texts(const char* name, int mode, bool count, bool again = false, bool destroyLoaded = false, bool held = false, uintptr_t heap = 0) {
    g_fill = 0xCD;
    AtFlashTextTable* o = make<AtFlashTextTable>(0x00013870, 0xCD, true);
    AtFlashTextTable* r = make<AtFlashTextTable>(0x00013870, 0xCD, false);
    same_texts(o, r, false);
    compare_logs("texts constructed");
    o->m_heap = r->m_heap = (void*)heap;
    const bool oc = run_side(0, [&] { at<Fn4>(0x00013CB0)(o, nullptr, (uintptr_t)name, (uintptr_t)mode, (uintptr_t)g_owner, 5); });
    const bool rc = run_side(1, [&] { r->Load(name, mode, g_owner, 5); });
    eq("crash", oc, rc);
    same_texts(o, r, count && !oc);
    compare_logs("texts loaded");
    eq("path left", g_path_after[0], g_path_after[1]);
    if (!oc) eq("path left as found", g_path_after[0], 0);
    if (oc || rc) return;
    if (again) {
        const bool oa = run_side(0, [&] { at<Fn4>(0x00013CB0)(o, nullptr, (uintptr_t)name, (uintptr_t)mode, (uintptr_t)g_owner, 0); });
        const bool ra = run_side(1, [&] { r->Load(name, mode, g_owner, 0); });
        eq("crash", oa, ra);
        same_texts(o, r, false);
        compare_logs("texts loaded again");
        if (oa || ra) return;
    }
    if (destroyLoaded) {
        run_side(0, [&] { at<FnDel>(0x00013DD0)(o, nullptr, 1); });
        run_side(1, [&] { delete r; });
        compare_logs("texts destroyed loaded");
        return;
    }
    add_member(o, 0);
    add_member(r, 1);
    if (held && o->m_fonts.m_refs && o->m_fonts.m_refCount > 1) o->m_fonts.m_refs[1].m_references = r->m_fonts.m_refs[1].m_references = 1;
    run_side(0, [&] { at<Fn0>(0x000138F0)(o, nullptr); at<Fn0>(0x000138F0)(o, nullptr); });
    run_side(1, [&] { r->ClearTexts(); r->ClearTexts(); });
    same_texts(o, r, false);
    same_bytes("group member", &g_member[0], &g_member[1], offsetof(AtFlashResource, m_index), sizeof(AtFlashResource));
    same_bytes("last group's member", &g_last_member[0], &g_last_member[1], offsetof(AtFlashResource, m_index), sizeof(AtFlashResource));
    compare_logs("texts cleared");
    run_side(0, [&] { eq("returns this", (uintptr_t)at<FnDel>(0x00013DD0)(o, nullptr, 1), (uintptr_t)o); });
    run_side(1, [&] { delete r; });
    compare_logs("texts destroyed");
}

static void run_scripts(const char* name, int mode, bool count, bool again = false, bool destroyLoaded = false, uintptr_t heap = 0, uintptr_t vmHeap = 0) {
    g_fill = 0xCD;
    AtFlashScriptTable* o = make<AtFlashScriptTable>(0x000133B0, 0xCD, true);
    AtFlashScriptTable* r = make<AtFlashScriptTable>(0x000133B0, 0xCD, false);
    same_scripts(o, r, false);
    compare_logs("scripts constructed");
    // What the constructor leaves alone: the application.
    o->m_application = r->m_application = (AtFlashApplication*)&g_heap_b;
    o->m_heap = r->m_heap = (void*)heap;
    const bool oc = run_side(0, [&] { at<Fn3>(0x000134A0)(o, nullptr, (uintptr_t)name, (uintptr_t)mode, (uintptr_t)&g_app); });
    const bool rc = run_side(1, [&] { r->Load(name, mode, &g_app); });
    eq("crash", oc, rc);
    same_scripts(o, r, count && !oc);
    same_statics();
    compare_logs("scripts loaded");
    if (oc || rc) return;
    if (again) {
        o->m_vm.m_heap = r->m_vm.m_heap = (void*)vmHeap;
        const bool oa = run_side(0, [&] { at<Fn3>(0x000134A0)(o, nullptr, (uintptr_t)name, (uintptr_t)mode, (uintptr_t)&g_app); });
        const bool ra = run_side(1, [&] { r->Load(name, mode, &g_app); });
        eq("crash", oa, ra);
        same_scripts(o, r, false);
        same_statics();
        compare_logs("scripts loaded again");
        if (oa || ra) return;
    }
    if (destroyLoaded) {
        run_side(0, [&] { at<FnDel>(0x00013850)(o, nullptr, 1); });
        run_side(1, [&] { delete r; });
        compare_logs("scripts destroyed loaded");
        return;
    }
    add_member(o, 0);
    add_member(r, 1);
    run_side(0, [&] { at<Fn0>(0x00013420)(o, nullptr); at<Fn0>(0x00013420)(o, nullptr); });
    run_side(1, [&] { r->ClearScripts(); r->ClearScripts(); });
    same_scripts(o, r, false);
    compare_logs("scripts cleared");
    run_side(0, [&] { eq("returns this", (uintptr_t)at<FnDel>(0x00013850)(o, nullptr, 1), (uintptr_t)o); });
    run_side(1, [&] { delete r; });
    compare_logs("scripts destroyed");
}

// ---- a whole level -----------------------------------------------------------------------------------------------------

static unsigned char* g_level[2];
static unsigned g_levels;

static void same_level(const unsigned char* o, const unsigned char* r, bool count) {
    const AtFlashLevel* a = (const AtFlashLevel*)o;
    const AtFlashLevel* b = (const AtFlashLevel*)r;
    same_scripts(&a->m_scripts, &b->m_scripts, count);
    same_texts(&a->m_texts, &b->m_texts, count);
    // The sprites and shapes in depth are the afd suite's: here, that they loaded alike.
    same_base(&a->m_sprites, &b->m_sprites);
    eq("sprites loaded", a->m_sprites.m_loaded, b->m_sprites.m_loaded);
    same_base(&a->m_shapes, &b->m_shapes);
    eq("shapes loaded", a->m_shapes.m_loaded, b->m_shapes.m_loaded);
    eq("block", a->m_block != nullptr, b->m_block != nullptr);
    same_bytes("level, head", o, r, 0, offsetof(AtFlashLevel, m_scripts));
    same_bytes("level, sounds", o, r, offsetof(AtFlashLevel, m_sounds), offsetof(AtFlashLevel, m_sprites));
    same_bytes("level, tail", o, r, offsetof(AtFlashLevel, m_unknown31C), offsetof(AtFlashLevel, m_block));
    same_bytes("level, tail", o, r, offsetof(AtFlashLevel, m_heap), sizeof(AtFlashLevel));
}

static void run_level(const char* name, bool count) {
    g_fill = 0xCD;
    static unsigned char o[sizeof(AtFlashLevel)], r[sizeof(AtFlashLevel)];
    g_level[0] = o;
    g_level[1] = r;
    std::memset(o, 0xCD, sizeof o);
    std::memset(r, 0xCD, sizeof r);
    at<FnCtor>(0x000133B0)(o + offsetof(AtFlashLevel, m_scripts), nullptr);
    at<FnCtor>(0x00012BD0)(o + offsetof(AtFlashLevel, m_shapes), nullptr);
    at<FnCtor>(0x00012280)(o + offsetof(AtFlashLevel, m_sprites), nullptr);
    at<FnCtor>(0x00013870)(o + offsetof(AtFlashLevel, m_texts), nullptr);
    AtFlashLevel* rl = (AtFlashLevel*)r;
    new (&rl->m_scripts) AtFlashScriptTable;
    new (&rl->m_shapes) AtFlashShapeTable;
    new (&rl->m_sprites) AtFlashSpriteTable;
    new (&rl->m_texts) AtFlashTextTable;
    g_env.m_platformSpecificFiles = 1;
    for (unsigned char* p : {o, r}) ((AtFlashLevel*)p)->m_environment = &g_env;
    { On on(0); at<Fn0>(0x000113B0)(o, nullptr); }
    { On on(1); rl->Reset(); }
    compare_logs("level reset");
    for (unsigned char* p : {o, r}) {
        ((AtFlashLevel*)p)->m_environment = &g_env;
        ((AtFlashLevel*)p)->m_heap = nullptr;
        ((AtFlashLevel*)p)->m_scripts.m_application = nullptr;
    }
    const bool oc = run_side(0, [&] { at<Fn2>(0x00011770)(o, nullptr, (uintptr_t)name, 77); });
    const bool rc = run_side(1, [&] { rl->ReadDefinition(name, 77); });
    eq("crash", oc, rc);
    same_level(o, r, false);
    compare_logs("level read");
    if (count && !oc) g_levels++;
    if (oc || rc) return;
    run_side(0, [&] {
        at<Fn0>(0x00013420)(o + offsetof(AtFlashLevel, m_scripts), nullptr);
        at<Fn0>(0x000138F0)(o + offsetof(AtFlashLevel, m_texts), nullptr);
        at<Fn0>(0x000122E0)(o + offsetof(AtFlashLevel, m_sprites), nullptr);
        at<Fn0>(0x00012C00)(o + offsetof(AtFlashLevel, m_shapes), nullptr);
    });
    run_side(1, [&] { rl->m_scripts.ClearScripts(); rl->m_texts.ClearTexts(); rl->m_sprites.ClearSprites(); rl->m_shapes.ClearShapes(); });
    compare_logs("level's tables cleared");
    std::free(((AtFlashLevel*)o)->m_block);
    std::free(rl->m_block);
}

// ---- direct: values, the stack, the machine, a script, a text, the fonts ---------------------------------------------------

static AtFpActionScriptValue* make_value(bool original) {
    return make<AtFpActionScriptValue>(0x00020A50, 0xCD, original);
}

static void run_values() {
    g_context = "values";
    g_fill = 0xCD;
    for (uintptr_t heap : {(uintptr_t)0, (uintptr_t)&g_heap_a}) {
        for (unsigned request : {0u, 0x1Fu, 0x20u, 0x21u, 0x40u, 0x80000000u}) {
            for (int fail = 0; fail < 2; fail++) {
                AtFpActionScriptValue* o = make_value(true);
                AtFpActionScriptValue* r = make_value(false);
                same_value(o, r);
                o->m_heap = r->m_heap = (void*)heap;
                g_fail_at_push = fail;
                run_side(0, [&] { at<Fn1>(0x00020AD0)(o, nullptr, request); at<Fn1>(0x00020AD0)(o, nullptr, request + 1); });
                run_side(1, [&] { r->Reserve(request); r->Reserve(request + 1); });
                g_fail_at_push = 0;
                same_value(o, r);
                compare_logs("value reserved");
                if (fail) { std::free(o); std::free(r); continue; }  // A failed request leaves the text null, which the second Reserve writes through: not reached on success paths.
                AtFpActionScriptValue* o2 = make_value(true);
                AtFpActionScriptValue* r2 = make_value(false);
                o->m_type = r->m_type = 0;
                o->m_unknown10 = r->m_unknown10 = 0x10;
                o->m_unknown14 = r->m_unknown14 = 0x14;
                o->m_unknown18 = r->m_unknown18 = 0x18;
                o->m_unknown1C = r->m_unknown1C = 0x1C;
                o->m_unknown20 = r->m_unknown20 = 0x20;
                o->m_unknown24 = std::malloc(4);
                r->m_unknown24 = std::malloc(4);
                run_side(0, [&] { at<Fn1>(0x000209B0)(o, nullptr, (uintptr_t)o2); });
                run_side(1, [&] { r->MoveTo(r2); });
                same_value(o, r);
                same_value(o2, r2);
                compare_logs("value moved");
                char* bo = (char*)std::malloc(0x40);
                char* br = (char*)std::malloc(0x40);
                std::memset(bo, 0, 0x40); std::strcpy(bo, "abc");
                std::memset(br, 0, 0x40); std::strcpy(br, "abc");
                run_side(0, [&] { at<Fn2>(0x00020A10)(o2, nullptr, (uintptr_t)bo, 8); });
                run_side(1, [&] { r2->SetString(br, 8); });
                same_value(o2, r2);
                compare_logs("value given a string");
                // A string over a value with none.
                char* co = (char*)std::malloc(0x40);
                char* cr = (char*)std::malloc(0x40);
                std::memset(co, 0, 0x40); std::strcpy(co, "de");
                std::memset(cr, 0, 0x40); std::strcpy(cr, "de");
                run_side(0, [&] { at<Fn2>(0x00020A10)(o, nullptr, (uintptr_t)co, 3); });
                run_side(1, [&] { r->SetString(cr, 3); });
                same_value(o, r);
                compare_logs("value given a string, none before");
                o2->m_unknown24 = std::malloc(4);
                r2->m_unknown24 = std::malloc(4);
                run_side(0, [&] { at<Fn0>(0x00020A80)(o2, nullptr); at<Fn0>(0x00020A80)(o2, nullptr); });
                run_side(1, [&] { r2->Reset(); r2->Reset(); });
                same_value(o2, r2);
                compare_logs("value reset");
                o->m_unknown24 = std::malloc(4);
                r->m_unknown24 = std::malloc(4);
                o->m_heap = r->m_heap = &g_heap_b;
                run_side(0, [&] { at<Fn0>(0x00020CC0)(o, nullptr); at<Fn0>(0x00020CC0)(o2, nullptr); });
                run_side(1, [&] { r->~AtFpActionScriptValue(); r2->~AtFpActionScriptValue(); });
                same_value(o, r);
                same_value(o2, r2);
                compare_logs("values destroyed");
                std::free(o); std::free(r); std::free(o2); std::free(r2);
            }
        }
    }
}

static void run_stack_and_vm() {
    g_fill = 0xCD;
    // The stack on its own.
    for (uintptr_t heap : {(uintptr_t)0, (uintptr_t)&g_heap_a}) {
        for (int fail = 0; fail < 3; fail++) {
            g_context = "stack, heap " + std::to_string(heap != 0) + ", failing push " + std::to_string(fail);
            AtFpActionScriptStack* o = make<AtFpActionScriptStack>(0x0002D270, 0xCD, true);
            AtFpActionScriptStack* r = make<AtFpActionScriptStack>(0x0002D270, 0xCD, false);
            same_stack(o, r);
            o->m_heap = r->m_heap = (void*)heap;
            g_fail_at_push = fail;
            run_side(0, [&] { at<Fn1>(0x0002D580)(o, nullptr, 4); });
            run_side(1, [&] { r->Allocate(4); });
            same_stack(o, r);
            compare_logs("stack allocated");
            o->m_heap = r->m_heap = (void*)heap;
            run_side(0, [&] { at<Fn1>(0x0002D580)(o, nullptr, 2); });
            run_side(1, [&] { r->Allocate(2); });
            g_fail_at_push = 0;
            same_stack(o, r);
            compare_logs("stack allocated again");
            o->m_top = r->m_top = 1;
            run_side(0, [&] { at<Fn0>(0x0002D290)(o, nullptr); at<Fn0>(0x0002D290)(o, nullptr); });
            run_side(1, [&] { r->Free(); r->Free(); });
            same_stack(o, r);
            compare_logs("stack freed");
            run_side(0, [&] { at<Fn1>(0x0002D580)(o, nullptr, 1); });
            run_side(1, [&] { r->Allocate(1); });
            run_side(0, [&] { at<Fn0>(0x0002D570)(o, nullptr); });
            run_side(1, [&] { r->~AtFpActionScriptStack(); });
            same_stack(o, r);
            compare_logs("stack destroyed");
            std::free(o); std::free(r);
        }
    }
    // The stack's static heap.
    g_context = "stack's static";
    run_side(0, [&] { at<FnStatic>(0x0002D1A0)(&g_heap_b); });
    run_side(1, [&] { AtFpActionScriptStack::SetHeap(&g_heap_b); });
    same_statics();
    compare_logs("stack's heap set");
    set_statics(&g_heap_token);
    // The machine's state.
    for (uintptr_t heap : {(uintptr_t)0, (uintptr_t)&g_heap_a}) {
        for (uintptr_t fallback : {(uintptr_t)&g_heap_token, (uintptr_t)0}) {
            g_context = "vm, heap " + std::to_string(heap != 0) + ", fallback " + std::to_string(fallback != 0);
            set_statics((void*)fallback);
            AtFpActionScriptVM* o = make<AtFpActionScriptVM>(0x0001CC80, 0xCD, true);
            AtFpActionScriptVM* r = make<AtFpActionScriptVM>(0x0001CC80, 0xCD, false);
            same_vm(o, r, o, r);
            o->m_heap = r->m_heap = (void*)heap;
            run_side(0, [&] { at<Fn2>(0x0001CCC0)(o, nullptr, (uintptr_t)&g_app, (uintptr_t)o + 8); });
            run_side(1, [&] { r->Init(&g_app, (AtTextTable*)((char*)r + 8)); });
            same_vm(o, r, o, r);
            same_statics();
            compare_logs("vm set up");
            // Again: the stack is freed first, which forgets its heap.
            run_side(0, [&] { at<Fn2>(0x0001CCC0)(o, nullptr, (uintptr_t)&g_app, (uintptr_t)o + 8); });
            run_side(1, [&] { r->Init(&g_app, (AtTextTable*)((char*)r + 8)); });
            same_vm(o, r, o, r);
            compare_logs("vm set up again");
            run_side(0, [&] { at<Fn0>(0x0001CCA0)(o, nullptr); });
            run_side(1, [&] { r->Clear(); });
            same_vm(o, r, o, r);
            compare_logs("vm cleared");
            run_side(0, [&] { at<Fn2>(0x0001CCC0)(o, nullptr, (uintptr_t)&g_app, 0); });
            run_side(1, [&] { r->Init(&g_app, nullptr); });
            run_side(0, [&] { at<Fn0>(0x0001EF00)(o, nullptr); });
            run_side(1, [&] { r->~AtFpActionScriptVM(); });
            same_vm(o, r, o, r);
            compare_logs("vm destroyed");
            std::free(o); std::free(r);
        }
    }
    set_statics(&g_heap_token);
}

class MemoryStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};
class DirectTokenizer : public AtIoTextTokenizer {
public:
    void Error(int code) override { if (g_recording) g_log[g_side].add('E', (uint32_t)code); }
};
struct Direct {
    MemoryStream stream;
    DirectTokenizer tokenizer;
    std::string text;
    explicit Direct(const std::string& t, const AtIoKeyword* words, unsigned count) : text(t) {
        stream.m_base = stream.m_cur = &text[0];
        stream.m_limit = &text[0] + text.size();
        stream.m_size = (unsigned)text.size();
        tokenizer.SetStream(&stream);
        tokenizer.SetKeywords(words, count, nullptr);
    }
    ~Direct() { tokenizer.SetStream(nullptr); }
};

static const AtIoKeyword kTextWords[] = {{0x39, "NameSpace"}, {0x3A, "Text"}, {0x3B, "String"}, {0x3C, "Font2D"}, {0x3D, "TextFormat"}, {0x3E, "Variable"}};

// A text read on its own; then released with fonts and formats this side made; then reset, destroyed.
static void run_text_direct(const std::string& text, int keep, int refs, bool demand, int found) {
    g_context = "text direct: " + text.substr(0, 80);
    g_fill = 0xCD;
    AtFlashText* o = make<AtFlashText>(0x0001FCB0, 0xCD, true);
    AtFlashText* r = make<AtFlashText>(0x0001FCB0, 0xCD, false);
    same_text(o, r);
    {
        Direct a(text, kTextWords, 6), b(text, kTextWords, 6);
        const bool oc = run_side(0, [&] { at<Fn1>(0x0001FD30)(o, nullptr, (uintptr_t)&a.tokenizer); });
        const bool rc = run_side(1, [&] { ((AtFlashResource*)r)->Read(&b.tokenizer); });
        eq("crash", oc, rc);
        same_text(o, r);
        compare_logs("text read");
    }
    // Read again, through ReadData alone: the old variable is not freed.
    {
        Direct a(text, kTextWords, 6), b(text, kTextWords, 6);
        o->m_read = r->m_read = 0;
        run_side(0, [&] { at<Fn1>(0x0001FBC0)(o, nullptr, (uintptr_t)&a.tokenizer); });
        run_side(1, [&] { r->ReadData(&b.tokenizer); });
        same_text(o, r);
        compare_logs("text read again");
    }
    // Released: a table of formats with a counting Release, and fonts of this harness.
    AtFlashTextFormatTable* fo = make<AtFlashTextFormatTable>(0x00020280, 0xCD, true);
    AtFlashTextFormatTable* fr = make<AtFlashTextFormatTable>(0x00020280, 0xCD, false);
    AtFlashFontTable* no = make<AtFlashFontTable>(0x0001FE70, 0xCD, true);
    AtFlashFontTable* nr = make<AtFlashFontTable>(0x0001FE70, 0xCD, false);
    same_fonts(no, nr, false);
    for (int side = 0; side < 2; side++) {
        AtFlashTextFormatTable* f = side ? fr : fo;
        // Index m_format, references 1, so the base's Release counts it down.
        static AtFlashResource item[2][8];
        static AtFlashResource* list[2][8];
        for (int i = 0; i < 8; i++) {
            std::memset(&item[side][i], 0, sizeof(AtFlashResource));
            item[side][i].m_references = 2;
            list[side][i] = &item[side][i];
        }
        f->m_items = list[side];
        f->m_itemCount = 8;
        AtFlashFontTable* n = side ? nr : no;
        static AtFlashFontTable::FontRef fr_refs[2][3];
        for (int i = 0; i < 3; i++) {
            fr_refs[side][i].m_references = (unsigned char)(i == found ? refs : 5);
            fr_refs[side][i].m_font = i == found ? (void*)&g_fonts[side][0] : nullptr;
        }
        n->m_refs = fr_refs[side];
        n->m_refCount = 3;
        n->m_fonts = (AtFontDatabase*)g_font_db[side];
        g_fonts[side][0].m_flags = demand ? 8 : 0;
        (side ? r : o)->m_unknown28 = keep;
    }
    if (o->m_format >= 0 && o->m_format < 8) {
        run_side(0, [&] { at<Fn2>(0x0001FC70)(o, nullptr, (uintptr_t)fo, (uintptr_t)no); });
        run_side(1, [&] { r->Release(fr, nr); });
        same_text(o, r);
        same_fonts(no, nr, false);
        for (int i = 0; i < 8; i++) eq("format's references", fo->m_items[i]->m_references, fr->m_items[i]->m_references);
        compare_logs("text released");
    }
    fo->m_items = fr->m_items = nullptr;
    no->m_refs = nr->m_refs = nullptr;
    // Not read: Reset leaves everything.
    const int was = o->m_read;
    o->m_read = r->m_read = 0;
    run_side(0, [&] { at<Fn0>(0x0001FD50)(o, nullptr); });
    run_side(1, [&] { r->Reset(); });
    same_text(o, r);
    compare_logs("text not read, reset");
    o->m_read = r->m_read = was;
    run_side(0, [&] { at<Fn0>(0x0001FD50)(o, nullptr); at<Fn0>(0x0001FD50)(o, nullptr); });
    run_side(1, [&] { r->Reset(); r->Reset(); });
    same_text(o, r);
    compare_logs("text reset");
    o->m_read = r->m_read = 1;
    o->m_variable = (char*)std::malloc(4);
    r->m_variable = (char*)std::malloc(4);
    run_side(0, [&] { at<Fn0>(0x0001FE20)(o, nullptr); });
    run_side(1, [&] { r->~AtFlashText(); });
    same_text(o, r, 0x00272D28);  // the base's, as a destructor leaves it
    compare_logs("text destroyed");
    // The fonts' table on its own, destroyed through its deleting destructor, loaded and not.
    no->m_loaded = nr->m_loaded = keep;
    no->m_fonts = nr->m_fonts = nullptr;
    no->m_fontLoader = nr->m_fontLoader = nullptr;
    run_side(0, [&] { eq("returns this", (uintptr_t)at<FnDel>(0x00020260)(no, nullptr, 1), (uintptr_t)no); });
    run_side(1, [&] { delete nr; });
    compare_logs("fonts destroyed");
    std::free(o); std::free(r); std::free(fo); std::free(fr);
}

// ReleaseFont on its own: each name, each kind, found among the references or not, counting down to none or not.
static void run_release_font() {
    g_fill = 0xCD;
    const char* names[] = {"F_Stand", "F_Big", "F_None"};
    for (const char* name : names) {
        for (int demand = 0; demand < 2; demand++) {
            for (int where = -1; where < 3; where++) {
                for (int count : {1, 2, 0}) {
                    g_context = std::string("release font ") + name + " demand " + std::to_string(demand) + " at " + std::to_string(where) + " count " + std::to_string(count);
                    AtFlashFontTable* o = make<AtFlashFontTable>(0x0001FE70, 0xCD, true);
                    AtFlashFontTable* r = make<AtFlashFontTable>(0x0001FE70, 0xCD, false);
                    static AtFlashFontTable::FontRef refs[2][3];
                    for (int side = 0; side < 2; side++) {
                        AtFlashFontTable* t = side ? r : o;
                        for (int i = 0; i < 3; i++) {
                            refs[side][i].m_references = (unsigned char)(i == where ? count : 3 + i);
                            refs[side][i].m_font = i == where ? (void*)&g_fonts[side][1] : (void*)&g_fonts[side][0];
                            if (i == where && i == 2) refs[side][i].m_font = (void*)&g_fonts[side][0];
                        }
                        t->m_refs = refs[side];
                        t->m_refCount = 3;
                        t->m_fonts = (AtFontDatabase*)g_font_db[side];
                        g_fonts[side][0].m_flags = demand ? 8 : 0;
                        g_fonts[side][1].m_flags = demand ? 0xF : 0;
                    }
                    int ao = 0, ar = 0;
                    run_side(0, [&] { ao = at<FnInt1>(0x0001FFE0)(o, nullptr, (uintptr_t)name); });
                    run_side(1, [&] { ar = r->ReleaseFont(name); });
                    eq("answer", ao, ar);
                    o->m_fonts = r->m_fonts = nullptr;
                    same_fonts(o, r, false);
                    compare_logs("font released");
                    o->m_refs = r->m_refs = nullptr;
                    std::free(o); std::free(r);
                }
            }
        }
    }
}

// A script block from text on its own.
static const AtIoKeyword kNoWords[] = {{0x39, "NameSpace"}};
static void run_script_direct(const std::string& text, unsigned instructions, unsigned size, uintptr_t heap, int fail) {
    g_context = "script direct: " + text.substr(0, 60) + " heap " + std::to_string(heap != 0) + " failing push " + std::to_string(fail);
    g_fill = 0xCD;
    AtFpScript* o = make<AtFpScript>(0x0001C900, 0xCD, true);
    AtFpScript* r = make<AtFpScript>(0x0001C900, 0xCD, false);
    same_script(o, r, false);
    o->m_heap = r->m_heap = (void*)heap;
    g_fail_at_push = fail;
    run_side(0, [&] { at<Fn3>(0x0001C980)(o, nullptr, 0x5300000A, instructions, size); });
    run_side(1, [&] { r->Init(0x5300000A, instructions, size); });
    g_fail_at_push = 0;
    same_script(o, r, false);
    compare_logs("script made room");
    if (fail) { std::free(o); std::free(r); return; }  // A failed request leaves a null list, which the reader would write through.
    {
        Direct a(text, kNoWords, 1), b(text, kNoWords, 1);
        run_side(0, [&] { at<Fn1>(0x0001CA40)(o, nullptr, (uintptr_t)&a.tokenizer); });
        run_side(1, [&] { r->ReadInstructions(&b.tokenizer); });
        same_script(o, r, false);
        compare_logs("script read");
    }
    o->m_heap = r->m_heap = &g_heap_b;
    run_side(0, [&] { at<Fn0>(0x0001C920)(o, nullptr); at<Fn0>(0x0001C920)(o, nullptr); });
    run_side(1, [&] { r->Free(); r->Free(); });
    same_script(o, r, false);
    compare_logs("script freed");
    run_side(0, [&] { at<Fn3>(0x0001C980)(o, nullptr, 1, 2, 3); });
    run_side(1, [&] { r->Init(1, 2, 3); });
    o->m_read = r->m_read = 2;
    run_side(0, [&] { at<Fn0>(0x0001CAE0)(o, nullptr); });
    run_side(1, [&] { r->~AtFpScript(); });
    same_script(o, r, false, 0x00272D28);  // the base's, as a destructor leaves it
    compare_logs("script destroyed");
    std::free(o); std::free(r);
}

// ---- written cases --------------------------------------------------------------------------------------------------

static void serve(const std::string& path, const std::string& data) { g_files[upper(path)] = data; }

static const char* kAfx = "NameSpace \"T\"\nTextFormat[2]\n{\nTextFormat 0 { FontHeight 20.5 TextColor 1 2 3 4 Alignment Center }\nTextFormat 1 { WordWrap 1 LeftMargin 2.25 }\n}\n";
static const char* kAis = "NameSpace \"T\"\nASCIIChar[10]\nASCIIString[1]\n{\n\"~ST00000\" \"Yes\"\n}\n";

static void run_written() {
    g_files.clear();
    serve("TEXT\\T.AFX", kAfx);
    serve("TEXT\\T.AIS", kAis);
    // Texts: each word, two texts, an unknown word, no texts.
    const std::string head = "// Atlas Flash Text Definition File\nNameSpace \"Tname\"\n";
    serve("TEXT\\T.AFT", head + "Text[ 3 ]\n{\nText 0 { String \"~ST00000\" Font2D \"F_Stand\" TextFormat 1 Variable \"_level0:szText\" }\nText 1 { TextFormat 0 String \"~ST00001\" }\nText 2 { Font2D \"F_Big\" }\n}\n");
    g_context = "written texts";
    run_texts("T", 2, false);
    run_texts("T", 0, false, true);
    run_texts("T", 2, false, false, true);
    run_texts("T", 2, false, false, false, false, (uintptr_t)&g_heap_a);
    serve("TEXT\\U.AFT", head + "Text[ 1 ]\n{\nText 4 { String \"~ST00000\" Shape 3 TextFormat 1 }\n}\n");
    serve("TEXT\\U.AFX", kAfx);
    g_context = "written texts, a bad word";
    run_texts("U", 2, false);
    serve("TEXT\\V.AFT", head + "Text[ 0 ]\n{\n}\n");
    serve("TEXT\\V.AFX", kAfx);
    g_context = "written texts, none";
    run_texts("V", 2, false);
    g_context = "written texts, no files";
    run_texts("W", 2, false);
    // A long name: the formats' and strings' readers cut it as they do, the fonts' "Font" never.
    serve("TEXT\\LONGNAMEX.AFT", head + "Text[ 1 ]\n{\nText 0 { String \"~ST00000\" }\n}\n");
    serve("TEXT\\LONGNAMEX.AFX", kAfx);
    g_context = "written texts, a long name";
    run_texts("LONGNAMEX", 2, false);
    // Fonts: none, three, none in the database, held at the clear.
    serve("TEXT\\FONT.AGF", "stands in");
    for (unsigned fonts : {3u, 0u, 1u}) {
        g_font_count = fonts;
        g_context = "written texts with fonts: " + std::to_string(fonts);
        run_texts("T", 2, false);
        run_texts("T", 0, false, true);
        run_texts("T", 2, false, false, true);
        run_texts("T", 2, false, false, false, true);
    }
    g_font_count = 3;
    // Each request failing in turn, with fonts.
    for (int fail = 1; fail <= 40; fail++) {
        g_context = "written texts, failing push " + std::to_string(fail);
        g_fail_at_push = fail;
        run_texts("T", 2, false);
        g_fail_at_push = 0;
    }
    // No heap at all: each request is reported.
    set_statics(nullptr);
    g_context = "written texts, no heap";
    run_texts("T", 2, false);
    set_statics(&g_heap_token);
    g_files.erase("TEXT\\FONT.AGF");

    // A text table's Release on its own: one reference left and more.
    for (int refs : {1, 2}) {
        g_context = "texts' release, references " + std::to_string(refs);
        g_fill = 0xCD;
        serve("TEXT\\FONT.AGF", "stands in");
        AtFlashTextTable* o = make<AtFlashTextTable>(0x00013870, 0xCD, true);
        AtFlashTextTable* r = make<AtFlashTextTable>(0x00013870, 0xCD, false);
        run_side(0, [&] { at<Fn4>(0x00013CB0)(o, nullptr, (uintptr_t)"T", 2, (uintptr_t)g_owner, 0); });
        run_side(1, [&] { r->Load("T", 2, g_owner, 0); });
        compare_logs("texts loaded to release");
        for (AtFlashTextTable* t : {o, r}) {
            ((AtFlashText*)t->m_items[0])->m_references = refs;
            t->m_formats.m_items[1]->m_references = 3;
            t->m_fonts.m_refs[0].m_font = &g_fonts[t == r][0];
            t->m_fonts.m_refs[0].m_references = 2;
        }
        g_fonts[0][0].m_flags = g_fonts[1][0].m_flags = 8;
        run_side(0, [&] { at<Fn1>(0x00013B00)(o, nullptr, 0); });
        run_side(1, [&] { r->Release(0); });
        same_texts(o, r, false);
        eq("format's references", o->m_formats.m_items[1]->m_references, r->m_formats.m_items[1]->m_references);
        compare_logs("text released through the table");
        for (AtFlashTextTable* t : {o, r}) t->m_fonts.m_refs[0].m_references = 0;
        run_side(0, [&] { at<FnDel>(0x00013DD0)(o, nullptr, 1); });
        run_side(1, [&] { delete r; });
        compare_logs("texts destroyed after release");
        g_files.erase("TEXT\\FONT.AGF");
    }

    // A text on its own: each word, release paths.
    const std::string t0 = "Text 3 { String \"~ST00002\" Font2D \"F_Stand\" TextFormat 2 Variable \"v\" }";
    for (int keep = 0; keep < 2; keep++) {
        for (int demand = 0; demand < 2; demand++) {
            for (int refs : {1, 3}) run_text_direct(t0, keep, refs, demand != 0, 0);
        }
    }
    run_text_direct(t0, 0, 1, true, 2);
    run_text_direct(t0, 0, 1, true, -1);
    run_text_direct("Text 9 { TextFormat 5 Font2D \"F_None\" Bogus }", 0, 1, false, 0);
    run_text_direct("Text 9 { }", 0, 1, false, 0);
    run_release_font();

    // Scripts.
    const std::string ahead = "// Atlas Flash Action Script File\nNameSpace \"Sname\"\n";
    const std::string blocks = "ScriptBlocks [2]\n{\nScript\n{\nScriptID 1392508928\nScriptBlockSize 12\nInstructions[3]\n{\n4 150 0 10 0\n6 140 0 2 0 65 66\n2 0 0\n}\n}\n"
                               "Script\n{\nScriptID 1392508929\nScriptBlockSize 3\nInstructions[1]\n{\n3 29 255 256\n}\n}\n}\n";
    serve("ASCRIPT\\S.AIS", kAis);
    serve("ASCRIPT\\S.AFA", ahead + "ActionScriptFileVersion 1.000000\n" + blocks);
    g_context = "written scripts";
    run_scripts("S", 2, false);
    run_scripts("S", 0, false, true);
    run_scripts("S", 2, false, true, false, (uintptr_t)&g_heap_a, (uintptr_t)&g_heap_b);
    run_scripts("S", 2, false, false, true);
    // No version: the first word is taken for it, and every word after is one along. Written so that the count still lands on a number. (A real file with no version would read its blocks one token out and write
    // until it faults, on both sides: not run.)
    serve("ASCRIPT\\T.AFA", ahead + "ScriptBlocks [ 0 1 ]\n{\nScript { ScriptID 5 ScriptBlockSize 2 Instructions[1] { 2 7 8 } }\n}\n");
    serve("ASCRIPT\\T.AIS", kAis);
    g_context = "written scripts, no version";
    run_scripts("T", 2, false);
    serve("ASCRIPT\\U.AFA", ahead + "ActionScriptFileVersion 1.0\nScriptBlocks [0]\n{\n}\n");
    g_context = "written scripts, none";
    run_scripts("U", 2, false);
    serve("ASCRIPT\\V.AFA", "NameSpace \"Sname\"\nActionScriptFileVersion 2\nScriptBlocks [1]\n{\nScript\n{\nScriptID 7\nScriptBlockSize 0\nInstructions[0]\n{\n}\n}\n}\n");
    g_context = "written scripts, a whole number for the version, an empty block";
    run_scripts("V", 2, false);
    g_context = "written scripts, no files";
    run_scripts("W", 2, false);
    serve("ASCRIPT\\LONGNAMEX.AFA", ahead + "ActionScriptFileVersion 1.0\n" + blocks);
    g_context = "written scripts, a long name";
    run_scripts("LONGNAMEX", 2, false);
    for (int fail = 1; fail <= 40; fail++) {
        g_context = "written scripts, failing push " + std::to_string(fail);
        g_fail_at_push = fail;
        run_scripts("S", 2, false);
        g_fail_at_push = 0;
    }
    set_statics(nullptr);
    g_context = "written scripts, no heap";
    run_scripts("U", 2, false);
    set_statics(&g_heap_token);
    // A script block on its own.
    for (uintptr_t heap : {(uintptr_t)0, (uintptr_t)&g_heap_a}) {
        for (int fail = 0; fail < 3; fail++) run_script_direct("{ 2 1 2 3 7 8 9 }", 4, 8, heap, fail);
    }
    run_script_direct("{ }", 1, 1, 0, 0);
    run_script_direct("{ 1 300 }", 1, 1, 0, 0);
}

// ---- more written cases: marks missing, tables on their own, clears of what is not loaded ----------------------------------

static void run_more() {
    g_files.clear();
    const std::string head = "NameSpace \"Tname\"\n";
    // Each mark of the .AFT missing in turn.
    const char* bodies[][2] = {{"M1", "Text 1 ]\n{\nText 0 { }\n}\n"}, {"M2", "Text[ 1\n{\nText 0 { }\n}\n"}, {"M3", "Text[ 1 ]\nText 0 { }\n}\n"}, {"M4", "Text[ 1 ]\n{\nText 0 { }\n"}};
    for (auto& b : bodies) {
        serve(std::string("TEXT\\") + b[0] + ".AFT", head + b[1]);
        serve(std::string("TEXT\\") + b[0] + ".AFX", kAfx);
        g_context = std::string("written texts, a mark missing: ") + b[0];
        run_texts(b[0], 2, false);
    }
    // The formats there, the texts not.
    serve("TEXT\\X.AFX", kAfx);
    g_context = "written texts, no .AFT";
    run_texts("X", 2, false);
    // A text with no brace after its index.
    run_text_direct("Text 9 String \"~ST00001\" }", 0, 1, false, 0);

    // The fonts on their own: a clear of what is not loaded, loaded twice, cleared, destroyed.
    serve("TEXT\\FONT.AGF", "stands in");
    g_context = "fonts on their own";
    g_fill = 0xCD;
    {
        AtFlashFontTable* o = make<AtFlashFontTable>(0x0001FE70, 0xCD, true);
        AtFlashFontTable* r = make<AtFlashFontTable>(0x0001FE70, 0xCD, false);
        for (AtFlashFontTable* t : {o, r}) {
            t->m_refCount = 5;
            t->m_owner = (AtMaterialOwner*)&g_heap_a;
            t->m_heap = &g_heap_b;
        }
        run_side(0, [&] { at<Fn0>(0x0001FEA0)(o, nullptr); });
        run_side(1, [&] { r->ClearFonts(); });
        same_fonts(o, r, false);
        compare_logs("fonts not loaded, cleared");
        for (AtFlashFontTable* t : {o, r}) {
            t->m_refCount = 0;
            t->m_owner = nullptr;
            t->m_heap = nullptr;
        }
        for (int pass = 0; pass < 2; pass++) {
            run_side(0, [&] { at<Fn3>(0x000200B0)(o, nullptr, (uintptr_t)"Font", 2, (uintptr_t)g_owner); });
            run_side(1, [&] { r->Load("Font", 2, g_owner); });
            same_fonts(o, r, false);
            compare_logs(pass ? "fonts loaded again" : "fonts loaded");
            eq("path left", g_path_after[0], g_path_after[1]);
        }
        run_side(0, [&] { at<Fn0>(0x0001FEA0)(o, nullptr); });
        run_side(1, [&] { r->ClearFonts(); });
        same_fonts(o, r, false);
        compare_logs("fonts cleared");
        run_side(0, [&] { at<FnDel>(0x00020260)(o, nullptr, 1); });
        run_side(1, [&] { delete r; });
        compare_logs("fonts destroyed");
    }
    g_files.erase("TEXT\\FONT.AGF");

    // The formats' load on its own, twice.
    serve("TEXT\\T.AFX", kAfx);
    g_context = "formats on their own";
    {
        AtFlashTextFormatTable* o = make<AtFlashTextFormatTable>(0x00020280, 0xCD, true);
        AtFlashTextFormatTable* r = make<AtFlashTextFormatTable>(0x00020280, 0xCD, false);
        for (int pass = 0; pass < 2; pass++) {
            run_side(0, [&] { at<Fn2>(0x00020570)(o, nullptr, (uintptr_t)"T", 2); });
            run_side(1, [&] { r->Load("T", 2); });
            same_formats(o, r, false);
            compare_logs(pass ? "formats loaded again" : "formats loaded");
            eq("path left", g_path_after[0], g_path_after[1]);
        }
        run_side(0, [&] { at<FnDel>(0x00020620)(o, nullptr, 1); });
        run_side(1, [&] { delete r; });
        compare_logs("formats destroyed");
    }

    // Clears of what is not loaded leave what is there.
    g_context = "texts not loaded, cleared";
    {
        AtFlashTextTable* o = make<AtFlashTextTable>(0x00013870, 0xCD, true);
        AtFlashTextTable* r = make<AtFlashTextTable>(0x00013870, 0xCD, false);
        o->m_heap = r->m_heap = &g_heap_a;
        run_side(0, [&] { at<Fn0>(0x000138F0)(o, nullptr); });
        run_side(1, [&] { r->ClearTexts(); });
        same_texts(o, r, false);
        compare_logs("texts not loaded, cleared");
        run_side(0, [&] { at<FnDel>(0x00013DD0)(o, nullptr, 1); });
        run_side(1, [&] { delete r; });
        compare_logs("texts not loaded, destroyed");
    }
    g_context = "scripts not loaded, cleared";
    {
        AtFlashScriptTable* o = make<AtFlashScriptTable>(0x000133B0, 0xCD, true);
        AtFlashScriptTable* r = make<AtFlashScriptTable>(0x000133B0, 0xCD, false);
        o->m_application = r->m_application = &g_app;
        for (AtFlashScriptTable* t : {o, r}) {
            t->m_count = 3;
            t->m_heap = &g_heap_a;
            t->m_scripts = nullptr;
        }
        const bool oc = run_side(0, [&] { at<Fn0>(0x00013420)(o, nullptr); });
        const bool rc = run_side(1, [&] { r->ClearScripts(); });
        eq("crash", oc, rc);
        same_scripts(o, r, false);
        compare_logs("scripts not loaded, cleared");
        for (AtFlashScriptTable* t : {o, r}) t->m_count = 0;
        run_side(0, [&] { at<FnDel>(0x00013850)(o, nullptr, 1); });
        run_side(1, [&] { delete r; });
        compare_logs("scripts not loaded, destroyed");
    }
    g_context = "stack not allocated, freed";
    {
        AtFpActionScriptStack* o = make<AtFpActionScriptStack>(0x0002D270, 0xCD, true);
        AtFpActionScriptStack* r = make<AtFpActionScriptStack>(0x0002D270, 0xCD, false);
        for (AtFpActionScriptStack* t : {o, r}) {
            t->m_top = 3;
            t->m_capacity = 5;
            t->m_heap = &g_heap_a;
        }
        run_side(0, [&] { at<Fn0>(0x0002D290)(o, nullptr); });
        run_side(1, [&] { r->Free(); });
        same_stack(o, r);
        compare_logs("stack not allocated, freed");
        std::free(o);
        std::free(r);
    }
}

// ---- real archives --------------------------------------------------------------------------------------------------

static unsigned g_archives, g_aft, g_afa, g_agf, g_afd;

static bool load_archive(const std::string& list, const std::string& bin, std::string* afd) {
    FILE* f = nullptr;
    if (fopen_s(&f, bin.c_str(), "rb") || !f) return false;
    std::fseek(f, 0, SEEK_END);
    std::string data((size_t)std::ftell(f), '\0');
    std::fseek(f, 0, SEEK_SET);
    if (!data.empty()) std::fread(&data[0], 1, data.size(), f);
    std::fclose(f);
    if (fopen_s(&f, list.c_str(), "r") || !f) return false;
    char line[1024];
    g_files.clear();
    while (std::fgets(line, sizeof line, f)) {
        char kind[8], path[512];
        unsigned offset, size;
        if (std::sscanf(line, "%7s\t%511[^\t]\t%u\t%u", kind, path, &offset, &size) != 4 || std::strcmp(kind, "F") != 0) continue;
        if ((size_t)offset + 32 + size > data.size()) continue;
        const std::string p = upper(path);
        g_files[p] = data.substr(offset + 32, size);
        if (p.size() > 4 && p.compare(p.size() - 4, 4, ".AFD") == 0 && p.find('\\') == std::string::npos) *afd = p.substr(0, p.size() - 4);
    }
    std::fclose(f);
    return true;
}

static std::string word_name(const std::string& text, const char* word) {
    const size_t q = text.find(word);
    if (q == std::string::npos) return "";
    const size_t a = text.find('"', q), b = text.find('"', a + 1);
    return text.substr(a + 1, b - a - 1);
}

static void run_archive(const std::string& afd) {
    const std::string& text = g_files[afd + ".AFD"];
    const std::string tname = word_name(text, "Text"), sname = word_name(text, "Script");
    if (g_files.count(upper("TEXT\\" + tname + ".AFT"))) g_aft++;
    if (g_files.count(upper("ASCRIPT\\" + sname + ".AFA"))) g_afa++;
    if (g_files.count("TEXT\\FONT.AGF")) g_agf++;
    g_context = afd + " texts";
    run_texts(tname.c_str(), 2, true);
    g_context = afd + " scripts";
    run_scripts(sname.c_str(), 2, true);
    g_context = afd + " level";
    run_level(afd.c_str(), true);
    g_afd++;
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
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
        else if (!std::strcmp(argv[i], "--trace")) g_trace = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { std::printf("usage: oracle_aft <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)push_counting);
    redirect((uint32_t)(uintptr_t)&AtPushHeap, (const void*)push_counting);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    // What stands in, on both sides.
    redirect(0x0001FA70, (const void*)strings_load);
    redirect(addr_of(&AtTextTable::Load), (const void*)strings_load);
    redirect(0x0002A290, (const void*)font_load);
    redirect(addr_of(&AtFontDatabase::Load), (const void*)font_load);
    redirect(0x00025150, (const void*)font_find);
    redirect(addr_of(static_cast<void* (GmNameMap::*)(const char*)>(&GmNameMap::Find)), (const void*)font_find);
    redirect(0x0001ADC0, (const void*)loader_load);
    redirect(addr_of(&AtDatabaseLoader::Load), (const void*)loader_load);
    redirect(0x0001C6F0, (const void*)count_used);
    redirect(addr_of(&AtTextureDatabase::CountUsedTextures), (const void*)count_used);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new JamFs;
    g_fs->m_path.m_top = 0;
    g_fs->m_path.m_stack[0][0] = 0;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    *(AtIoContext**)(uintptr_t)0x002C6328 = &g_ctx;
    *(AtIoContext**)(uintptr_t)0x002C62F0 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    AtAudioBankFile::s_context = &g_ctx;
    AtIoContext::s_instance = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C626Cu, 0x002C6324u, 0x002C63D0u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    AtFpSprite::s_heap = &g_heap_token;
    AtFlashFrame::s_heap = &g_heap_token;
    AtFpPlaceObject2Tag::s_heap = &g_heap_token;
    set_statics(&g_heap_token);
    g_owner = new FakeOwner;
    g_loaders[0] = new FakeLoader;
    g_loaders[1] = new FakeLoader;
    g_graphics.m_owner = g_owner;
    g_app.m_graphics = &g_graphics;
    g_env.m_application = &g_app;
    for (int side = 0; side < 2; side++) {
        g_fonts[side][0].number = 1;
        g_fonts[side][1].number = 2;
    }
    *(void**)(uintptr_t)0x002C60A8 = &g_heap_b;
    AtHeapStack::s_contentHeap = (AtHeap*)&g_heap_b;

    g_heap_pad = 0x1000;
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_values();
    run_stack_and_vm();
    run_written();
    run_more();

    std::string dir = pos[1];
    const std::string jams = dir + "\\..\\oracle-jam\\";
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA((jams + "*.txt").c_str(), &found);
    if (h == INVALID_HANDLE_VALUE) { std::printf("cannot list %s\n", jams.c_str()); return 2; }
    std::vector<std::string> lists;
    do lists.push_back(found.cFileName); while (FindNextFileA(h, &found));
    FindClose(h);
    std::sort(lists.begin(), lists.end());
    g_texts = g_formats = g_scripts = g_instructions = g_font_tables = 0;
    for (const std::string& l : lists) {
        std::string afd;
        const std::string stem = l.substr(0, l.size() - 4);
        if (!load_archive(jams + l, jams + stem + ".bin", &afd) || afd.empty()) continue;
        g_archives++;
        run_archive(afd);
    }
    report_entries();
    std::printf("%s: %u archives, %u .AFD, %u .AFT, %u .AFA, %u Font.AGF; %u texts, %u formats, %u font tables, %u scripts, %u instructions, %u levels; %llu comparisons, %d differences\n",
                g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_archives, g_afd, g_aft, g_afa, g_agf, g_texts, g_formats, g_font_tables, g_scripts, g_instructions, g_levels, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
