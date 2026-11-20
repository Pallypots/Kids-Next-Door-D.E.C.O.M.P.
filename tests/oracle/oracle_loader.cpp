// Differential oracle for the Xbox database loader and the binary meshes and animations it reaches: .XGG and .XKA (Windows x86, local only). See docs/verification.md and docs/worklog/2026-11-07-loader.md.
//
//   oracle_loader <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// The binary files are in <cases>\..\scratch\loader\bin, written by tests/oracle/loader_cases.py.
//
// What it does not show, first. Beneath both sides are doubles for what is not recovered, the same doubles on each: the six kinds of database other than meshes, animations, textures and materials (include/at/AtGrD3DTextures.h, real on both sides) are the seam classes of
// src/at/AtGrD3DUnrecovered.cpp on both sides (the original's constructors are sent to them), so they only log; the items a mesh database makes are those classes too, and their binary readers are this harness's,
// which log the words at each cursor and step the first cursor by the record sizes the original's step it; memory for the graphics processor is the harness's. Text files an .AGD names open as empty text. So real
// files show that the loader, the mesh database and the animation database do the same, in the same order, with the same blocks, pointers and names, as far as those doubles; they show nothing of how an item reads
// its part, or of anything drawn.
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

#include "at/AtGrD3DDatabase.h"
#include "at/AtGrD3DTextureLoad.h"
#include "at/AtAudioDatabase.h"
#include "at/AtHeap.h"
#include "at/AtIoTokenizer.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 150).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 25) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 150).c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#include "oracle_doubles.h"

typedef void(__fastcall* Fn0)(void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__fastcall* Fn1)(void*, void*, void*);
typedef void(__fastcall* FnLoad2)(void*, void*, void*, const char*);
typedef void(__fastcall* FnLoadD)(void*, void*, void*, const char*, int, const char*, int);
typedef void*(__fastcall* FnAt)(void*, void*, unsigned);
typedef void*(__fastcall* FnNew)(void*, void*);

static void slot(char c, uint32_t v = 0) { if (g_recording) g_log[g_side].add(c, v); }

// ---- files ------------------------------------------------------------------------------------------------------------

static std::map<std::string, std::string> g_files;   // "ARCHIVE|PATH" and "PATH", upper case
static std::string g_archive;
static unsigned g_opens;
static unsigned g_binary_found, g_binary_missing;  // binary files an .AGD of the disc names, found and not
static std::string g_bin_dir;

static std::string upper(std::string s) {
    for (char& c : s) c = (char)toupper((unsigned char)c);
    return s;
}

static std::string read_file(const std::string& path) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") || !f) return std::string();
    std::fseek(f, 0, SEEK_END);
    std::string text((size_t)std::ftell(f), 0);
    std::fseek(f, 0, SEEK_SET);
    if (!text.empty()) std::fread(&text[0], 1, text.size(), f);
    std::fclose(f);
    return text;
}

// A file that, after so many reads, makes every allocation fail: so that one allocation after another can be the first to fail.
static int g_fail_after_reads = -1;
static int g_reads_seen;

class CountingFile : public MemFile {
public:
    int Read(void* out, unsigned int n, unsigned int* done, AtIoFileRequest** request) override {
        const int status = MemFile::Read(out, n, done, request);
        if (g_fail_after_reads >= 0 && ++g_reads_seen == g_fail_after_reads) g_fail_alloc = true;
        return status;
    }
};
static CountingFile g_counting[2];

class PathFs : public MemFs {
public:
    int Open(const char* path, int flags, AtIoFile** out) override {
        const int status = OpenPlain(path, flags, out);
        if (g_fail_after_reads >= 0 && *out) {
            static_cast<MemFile&>(g_counting[g_side]) = files[g_side];
            *out = &g_counting[g_side];
        }
        return status;
    }
    int OpenPlain(const char* path, int flags, AtIoFile** out) {
        const bool was = g_recording;
        g_recording = false;
        g_opens++;
        {
            m_path.Resolve(path);
            const std::string key = upper(m_path.m_full);
            auto it = g_files.find(upper(g_archive) + "|" + key);
            if (it == g_files.end()) it = g_files.find(key);
            static const std::string empty;
            const std::string& text = it != g_files.end() ? it->second : empty;
            const bool binary = key.size() > 4 && (key.substr(key.size() - 4) == ".XGG" || key.substr(key.size() - 4) == ".XKA");
            if (binary && !g_archive.empty()) (it != g_files.end() ? g_binary_found : g_binary_missing)++;
            files[g_side].data = (const unsigned char*)text.data();
            files[g_side].size = text.size();
            strncpy_s(files[g_side].m_path, m_path.m_full, _TRUNCATE);
        }
        g_recording = was;
        g_log[g_side].add('T', (uint32_t)m_path.m_top);
        return MemFs::Open(path, flags, out);
    }
};

static PathFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;
static AtHeap g_mesh_heap;
static void* g_sound_table;

// ---- what the original's constructors of unrecovered classes make: the seam classes, on both sides --------------------

// An object of a seam class made on the original's side gets a copy of its table, with the slot the original calls to destroy an array replaced: the original frees through its own delete, which logs.
template <class T, int Slot> struct Side0 {
    static void* s_table[64];
    static bool s_ready;
    static void* __fastcall VecDel(T* self, void*, unsigned flags) {
        if (flags & 2) {
            unsigned* block = (unsigned*)self - 1;
            for (unsigned i = *block; i > 0; i--) self[i - 1].T::~T();
            if (flags & 1) o_delete(block);
            return block;
        }
        self->T::~T();
        if (flags & 1) o_delete(self);
        return self;
    }
    static T* __fastcall Construct(T* self, void*) {
        new (self) T;
        if (!s_ready) {
            std::memcpy(s_table, *(void***)self, sizeof s_table);
            s_table[Slot] = (void*)&VecDel;
            s_ready = true;
        }
        *(void***)self = s_table;
        return self;
    }
};
template <class T, int Slot> void* Side0<T, Slot>::s_table[64];
template <class T, int Slot> bool Side0<T, Slot>::s_ready;

// The eight kinds of database on the original's side: the original's base constructor and destructor, around what the seam logs, so that the base's own code is the original's throughout that side.
// The table: the original concrete class's, where a slot is the base's own code; the seam class's, where the concrete class has its own; and slot 0 destroys as the original's does.
template <class T, uint32_t BaseCtor, uint32_t BaseDtor, uint32_t SeamCtor, uint32_t SeamDtor, uint32_t Table, int Slots> struct Thin {
    static void* s_table[64];
    static bool s_ready;
    static void* __fastcall VecDel(T* self, void*, unsigned flags) {
        unsigned* block = (unsigned*)self - 1;
        const unsigned count = (flags & 2) ? *block : 1;
        for (unsigned i = count; i > 0; i--) {
            AtGrD3DUnrecovered(SeamDtor, &self[i - 1], 0, 0);
            at<Fn0>(BaseDtor)(&self[i - 1], nullptr);
        }
        if (flags & 1) o_delete((flags & 2) ? (void*)block : (void*)self);
        return (flags & 2) ? (void*)block : (void*)self;
    }
    static T* __fastcall Construct(T* self, void*) {
        if (!s_ready) {
            const bool was = g_recording;
            g_recording = false;
            void* model = std::calloc(1, sizeof(T));
            new (model) T;
            void** ours = *(void***)model;
            for (int k = 0; k < Slots; k++) {
                const uint32_t theirs = *(const uint32_t*)(uintptr_t)(Table + 4 * k);
                s_table[k] = theirs >= 0x000E0000 && theirs < 0x00125000 ? ours[k] : (void*)(uintptr_t)theirs;
            }
            s_table[0] = (void*)&VecDel;
            s_ready = true;
            g_recording = was;
        }
        at<Fn0>(BaseCtor)(self, nullptr);
        AtGrD3DUnrecovered(SeamCtor, self, 0, 0);
        *(void***)self = s_table;
        return self;
    }
};
template <class T, uint32_t A, uint32_t B, uint32_t C, uint32_t D, uint32_t E, int F> void* Thin<T, A, B, C, D, E, F>::s_table[64];
template <class T, uint32_t A, uint32_t B, uint32_t C, uint32_t D, uint32_t E, int F> bool Thin<T, A, B, C, D, E, F>::s_ready;
// ---- the seam's functions, the same on both sides ---------------------------------------------------------------------

static void* g_own_any[2];  // the owners, as the seams see them
static const char* g_last[2][4][6];  // by side, kind and cursor: the last value, for the step since

static uint32_t word_at(const char* p) {
    uint32_t v = 0xDEADBEEF;
    __try {
        v = *(const uint32_t*)p;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return v;
}

static void log_cursor(int kind, int which, char** cursor) {
    const char* p = *cursor;
    const char*& last = g_last[g_side][kind][which];
    slot('w', word_at(p));
    slot('s', last ? (uint32_t)(p - last) : 0u);
    last = p;
}

// Each side's seam objects in the order made, so that a log says which one a call was for.
static std::vector<void*> g_objects[2];

static uint32_t object_index(void* object) {
    std::vector<void*>& v = g_objects[g_side];
    for (size_t i = v.size(); i > 0; i--) if (v[i - 1] == object) return (uint32_t)(i - 1);
    return 0xFFFF;
}

static bool is_constructor(unsigned int va) {
    for (unsigned int c : {0x00111E50u, 0x001120B0u, 0x000EC890u, 0x000F8010u, 0x000EECA0u, 0x00122460u}) if (c == va) return true;
    return false;
}

unsigned int AtGrD3DUnrecovered(unsigned int va, void* object, unsigned int a, unsigned int) {
    if (g_recording && is_constructor(va)) {
        g_recording = false;  // The harness's own vector grows on the harness's heap.
        g_objects[g_side].push_back(object);
        g_recording = true;
    }
    slot('u', va);
    slot('a', a);
    slot('#', object_index(object));
    // A new item's array begins a new run of its readers.
    if (va == 0x000EC890) std::memset(g_last[g_side][0], 0, sizeof g_last[0][0]);
    if (va == 0x000F8010) std::memset(g_last[g_side][1], 0, sizeof g_last[0][1]);
    if (va == 0x000EECA0) std::memset(g_last[g_side][2], 0, sizeof g_last[0][2]);
    if (va == 0x00122460) std::memset(g_last[g_side][3], 0, sizeof g_last[0][3]);
    return 0;
}

void* AtGrD3DAllocateContiguous(unsigned int size, unsigned int alignment) {
    slot('G', size);
    slot('g', alignment);
    return filled(size);
}

void AtGrD3DFreeContiguous(void* data) {
    slot('H', data != nullptr);
    std::free(data);
}

void AtGrD3DReadVertexArray(AtGrD3DVertexArray* array, char** records, char** kept, char** contiguous) {
    slot('1', (void*)array->m_owner == g_own_any[g_side]);
    slot('#', object_index(array));
    log_cursor(0, 0, records);
    log_cursor(0, 1, kept);
    log_cursor(0, 2, contiguous);
    *records += 0x3C;
}

void AtGrD3DReadIndexArray(AtGrD3DIndexArray* array, char** records, char** kept) {
    slot('2', object_index(array));
    log_cursor(1, 0, records);
    log_cursor(1, 1, kept);
    *records += 8;
}

void AtGrD3DReadVertexStream(AtGrD3DVertexStream* stream, AtMaterialOwner* owner, char** records, char** kept) {
    slot('3', owner != nullptr);
    slot('#', object_index(stream));
    log_cursor(2, 0, records);
    log_cursor(2, 1, kept);
    *records += 0x48;
}

void AtGrD3DReadMesh(AtGrD3DMesh* mesh, AtMaterialOwner* owner, AtGrD3DMeshDatabase* database, char** triangles, char** parts, char** materials, char** records, char** kept, char** contiguous, char* name) {
    slot('4', owner != nullptr);
    slot('4', database != nullptr);
    slot('#', object_index(mesh));
    log_cursor(3, 0, triangles);
    log_cursor(3, 1, parts);
    log_cursor(3, 2, materials);
    log_cursor(3, 3, records);
    log_cursor(3, 4, kept);
    log_cursor(3, 5, contiguous);
    // The original copies the record's name as a string; twelve bytes at most here, from a record the harness can read.
    const char* from = *records + 0x0C;
    int i = 0;
    __try {
        for (; i < 11 && from[i]; i++) name[i] = from[i];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    name[i] = 0;
    *records += 0x18;
}

static void __fastcall o_read_vertex(AtGrD3DVertexArray* self, void*, char** a, char** b, char** c) { AtGrD3DReadVertexArray(self, a, b, c); }
static void __fastcall o_read_index(AtGrD3DIndexArray* self, void*, char** a, char** b) { AtGrD3DReadIndexArray(self, a, b); }
static void __fastcall o_read_stream(AtGrD3DVertexStream* self, void*, AtMaterialOwner* o, char** a, char** b) { AtGrD3DReadVertexStream(self, o, a, b); }
static void __fastcall o_read_mesh(AtGrD3DMesh* self, void*, AtMaterialOwner* o, AtGrD3DMeshDatabase* d, char** a, char** b, char** c, char** e, char** f, char** g, char* n) {
    AtGrD3DReadMesh(self, o, d, a, b, c, e, f, g, n);
}
static void* __stdcall o_contiguous(unsigned size, unsigned alignment) { return AtGrD3DAllocateContiguous(size, alignment); }
static void __stdcall o_free_contiguous(void* p) { AtGrD3DFreeContiguous(p); }

// What a texture, a material, a font or an image does with the device, and the device's calls: they log, the same on both sides. tests/oracle/oracle_xdb.cpp and oracle_xshade.cpp test them.
void AtGrD3DLoadTexture2D(AtGrD3DTexture2D* t, AtMaterialOwner* owner) { slot('L', *(const uint32_t*)t->m_name); slot('l', owner == g_own_any[g_side]); }
void AtGrD3DLoadMaterial(AtGrD3DMaterial* m, AtMaterialOwner* owner) { slot('M', m->m_flags); slot('m', owner == g_own_any[g_side]); }
void AtGrD3DReleaseStage(void*, unsigned int stage) { slot('G', stage); }
void AtGrD3DBeforeRelease(void*) { slot('B'); }
void AtGrD3DRelease(void*) { slot('X'); }
void AtGrD3DLoadFont2D(AtGrD3DFont2D* f, AtMaterialOwner* owner) { slot('T', *(const uint32_t*)f->m_name); slot('l', owner == g_own_any[g_side]); }
void AtGrD3DLoadImage(AtGrD3DImage* i, AtMaterialOwner* owner) { slot('I', *(const uint32_t*)i->m_name); slot('l', owner == g_own_any[g_side]); }
static void __fastcall o_load_texture(AtGrD3DTexture2D* t, void*, AtMaterialOwner* owner) { AtGrD3DLoadTexture2D(t, owner); }
// A texture's slot 12 is recovered (src/at/AtGrD3DTextureLoad.cpp, tested by tests/oracle/oracle_texload.cpp). Here it stays the double above on both sides: on the recovered side by a jump written at the
// start of the compiled slot 12 (the harness's code is writable), as on the original's at 0x000FCAD0. So the device's calls it makes are not expected.
void* AtGrD3DGetBackBuffer(int) { slot('?', 3); return nullptr; }
void* AtGrD3DCreateTexture(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned) { slot('?', 4); return nullptr; }
void* AtGrD3DSurfaceLevel(void*, unsigned) { slot('?', 5); return nullptr; }
void AtGrD3DDescribe(void*, AtGrD3DSurfaceDesc*) { slot('?', 6); }
void AtGrD3DLockRect(void*, AtGrD3DLockedRect*, const void*, unsigned) { slot('?', 7); }
static void redirect_recovered_slot12(const void* to) {
    AtGrD3DTexture2D probe;
    redirect((uint32_t)(uintptr_t)(*(void***)&probe)[12], to);
}
static void __fastcall o_load_font(AtGrD3DFont2D* f, void*, AtMaterialOwner* owner) { AtGrD3DLoadFont2D(f, owner); }
static void __fastcall o_load_image(AtGrD3DImage* i, void*, AtMaterialOwner* owner) { AtGrD3DLoadImage(i, owner); }
static void __fastcall o_load_material(AtGrD3DMaterial* m, void*, AtMaterialOwner* owner) { AtGrD3DLoadMaterial(m, owner); }
static void __fastcall o_release_stage(void* stages, void*, unsigned stage) { AtGrD3DReleaseStage(stages, stage); }
static void __stdcall o_before_release(void* r) { AtGrD3DBeforeRelease(r); }
static void __stdcall o_release(void* r) { AtGrD3DRelease(r); }

// ---- owners -----------------------------------------------------------------------------------------------------------

class TestPool : public AtMatrixPool {
public:
    void Slot0() override {}
    void Reserve(unsigned int count) override { slot('r', count); }
    void Clear() override {}
    unsigned int Count() override { return 0; }
    AtMatrix* At(unsigned int) override { return nullptr; }
};

class TestMatrices : public AtMatrixDevice {
public:
    TestPool m_pool;
    AtMatrix* NewMatrix() override { return nullptr; }
    AtMatrixPool* NewPool() override { slot('o'); return &m_pool; }
    void Slot2() override {}
    void Slot3() override {}
    void FreeMatrix(AtMatrix*) override {}
    void FreePool(AtMatrixPool*) override { slot('O'); }
};

struct TestOwner : public AtGrD3DOwner {
    TestMatrices m_matrices;
    AtMatrixDevice* MatrixDevice() override { slot('z'); return &m_matrices; }
};

static TestOwner* g_own[2];
static AtAnimationDevice* g_dev[2];

static void make_owners() {
    const bool was = g_recording;
    g_recording = false;
    for (int side = 0; side < 2; side++) {
        g_own[side] = new (std::calloc(1, sizeof(TestOwner))) TestOwner;
        g_own_any[side] = g_own[side];
        TestOwner& w = *g_own[side];
        w.m_animationDevice = g_dev[side];
        w.m_meshSpaces.Init(8);
        w.m_lightAnimationSpaces.Init(8);
        w.m_morphAnimationSpaces.Init(8);
        w.m_textureSpaces.Init(8);
        w.m_materialSpaces.Init(8);
        w.m_shaderSpaces.Init(8);
        w.m_lightSpaces.Init(8);
        w.m_matAssignmentSpaces.Init(8);
        w.m_matAnimationSpaces.Init(8);
        w.m_fontSpaces.Init(8);
        w.m_imageSpaces.Init(8);
    }
    g_recording = was;
}

// ---- comparing --------------------------------------------------------------------------------------------------------

static bool g_trace;

static void show_parting() {
    static int shown;
    int i = 0;
    while (i < g_log[0].n && i < g_log[1].n && g_log[0].v[i] == g_log[1].v[i]) i++;
    if ((i == g_log[0].n && i == g_log[1].n) || shown++ >= 3) return;
    for (int side = 0; side < 2; side++) {
        std::printf("at %d:", i);
        for (int k = g_trace ? 0 : std::max(0, (i & ~1) - 16); k < g_log[side].n && k < i + (g_trace ? 400 : 24); k += 2) std::printf(" %c%X", (char)g_log[side].v[k], g_log[side].v[k + 1]);
        std::printf("\n");
    }
}

static void logs(const char* what) {
    g_recording = false;
    if (g_trace) std::printf("%s: %s\n", g_context.c_str(), what);
    show_parting();
    compare_logs(what);
}

static void same_map(const char* what, GmNameMap& o, GmNameMap& r, const void* obase, const void* rbase) {
    eq(what, o.m_size, r.m_size);
    eq(what, o.m_entries != nullptr, r.m_entries != nullptr);
    eq(what, o.m_group != nullptr, r.m_group != nullptr);
    if (o.m_group && r.m_group) eq(what, std::strncmp(o.m_group->m_name, r.m_group->m_name, 8), 0);
    if (o.m_size == r.m_size && o.m_entries && r.m_entries) {
        for (unsigned i = 0; i < o.m_size; i++) {
            const GmNameMap::Entry& x = o.m_entries[i];
            const GmNameMap::Entry& y = r.m_entries[i];
            eq(what, x.m_value != nullptr, y.m_value != nullptr);
            if (x.m_value && y.m_value) {
                // Up to the terminator: what follows it in a name copied from the stack is the stack's.
                eq(what, std::strncmp(x.m_name, y.m_name, 8), 0);
                if (obase) eq(what, (char*)x.m_value - (char*)obase, (char*)y.m_value - (char*)rbase);
            }
        }
    }
}

static long long rel(const void* p, const void* base) { return p ? (const char*)p - (const char*)base : -1; }

static unsigned g_meshes_compared, g_animations_compared, g_anim_dbs, g_mesh_dbs;

static void same_mesh(const char* what, AtGrD3DMeshDatabase* o, AtGrD3DMeshDatabase* r, unsigned keptSize) {
    g_mesh_dbs++;
    eq("mesh count", o->m_meshCount, r->m_meshCount);
    eq("vertex arrays", o->m_vertexArrayCount, r->m_vertexArrayCount);
    eq("index arrays", o->m_indexArrayCount, r->m_indexArrayCount);
    eq("streams", o->m_streamCount, r->m_streamCount);
    eq("kept size", o->m_keptSize, r->m_keptSize);
    eq("contiguous size", o->m_contiguousSize, r->m_contiguousSize);
    eq("meshes", o->m_meshes != nullptr, r->m_meshes != nullptr);
    eq("vertex array list", o->m_vertexArrays != nullptr, r->m_vertexArrays != nullptr);
    eq("index array list", o->m_indexArrays != nullptr, r->m_indexArrays != nullptr);
    eq("stream list", o->m_streams != nullptr, r->m_streams != nullptr);
    eq("kept", o->m_kept != nullptr, r->m_kept != nullptr);
    eq("contiguous", o->m_contiguous != nullptr, r->m_contiguous != nullptr);
    eq("more", o->m_more != nullptr, r->m_more != nullptr);
    eq("owner", o->m_owner == g_own[0] ? 1 : o->m_owner ? 2 : 0, r->m_owner == g_own[1] ? 1 : r->m_owner ? 2 : 0);
    eq("next", o->m_next != nullptr, r->m_next != nullptr);
    if (o->m_kept && r->m_kept && o->m_keptSize == r->m_keptSize) eq("kept bytes", std::memcmp(o->m_kept, r->m_kept, std::min(o->m_keptSize, keptSize)), 0);
    if (o->m_contiguous && r->m_contiguous && o->m_contiguousSize == r->m_contiguousSize) eq("contiguous bytes", std::memcmp(o->m_contiguous, r->m_contiguous, o->m_contiguousSize), 0);
    if (o->m_vertexArrays && r->m_vertexArrays && o->m_vertexArrayCount == r->m_vertexArrayCount) {
        for (unsigned i = 0; i < o->m_vertexArrayCount; i++) eq("vertex array owner", rel(o->m_vertexArrays[i].m_owner, g_own[0]), rel(r->m_vertexArrays[i].m_owner, g_own[1]));
    }
    if (o->m_indexArrays && r->m_indexArrays && o->m_indexArrayCount == r->m_indexArrayCount) {
        for (unsigned i = 0; i < o->m_indexArrayCount; i++) eq("index array owner", rel(o->m_indexArrays[i].m_owner, g_own[0]), rel(r->m_indexArrays[i].m_owner, g_own[1]));
    }
    same_map(what, *o, *r, o->m_meshes, r->m_meshes);
    g_meshes_compared += r->m_meshCount;
}

static void same_keys(AtGrD3DAnimationKeys* o, AtGrD3DAnimationKeys* r, const char* obase, const char* rbase) {
    eq("keys flags", o->m_flags, r->m_flags);
    eq("keys name", std::memcmp(o->m_name, r->m_name, 8), 0);
    eq("frames", o->m_count, r->m_count);
    eq("rotation keys", o->m_rotationKeyCount, r->m_rotationKeyCount);
    eq("translation keys", o->m_translationKeyCount, r->m_translationKeyCount);
    eq("scale keys", o->m_scaleKeyCount, r->m_scaleKeyCount);
    eq("note keys", o->m_noteKeyCount, r->m_noteKeyCount);
    eq("camera keys", o->m_cameraKeyCount, r->m_cameraKeyCount);
    eq("frame list", rel(o->m_frames, obase), rel(r->m_frames, rbase));
    eq("rotation list", rel(o->m_rotationKeys, obase), rel(r->m_rotationKeys, rbase));
    eq("translation list", rel(o->m_translationKeys, obase), rel(r->m_translationKeys, rbase));
    eq("scale list", rel(o->m_scaleKeys, obase), rel(r->m_scaleKeys, rbase));
    eq("note list", rel(o->m_noteKeys, obase), rel(r->m_noteKeys, rbase));
    eq("camera list", rel(o->m_cameraKeys, obase), rel(r->m_cameraKeys, rbase));
    eq("keys +40", o->m_unknown40, r->m_unknown40);
    eq("keys +44", o->m_unknown44, r->m_unknown44);
}

static void same_anim(const char* what, AtGrD3DAnimationDatabase* o, AtGrD3DAnimationDatabase* r, unsigned keptSize) {
    g_anim_dbs++;
    eq("device", o->m_device == g_dev[0] ? 1 : o->m_device ? 2 : 0, r->m_device == g_dev[1] ? 1 : r->m_device ? 2 : 0);
    eq("data", o->m_data != nullptr, r->m_data != nullptr);
    eq("keys", o->m_keys != nullptr, r->m_keys != nullptr);
    eq("rotation count", o->m_rotationCount, r->m_rotationCount);
    eq("translation count", o->m_translationCount, r->m_translationCount);
    eq("scale count", o->m_scaleCount, r->m_scaleCount);
    eq("note count", o->m_noteCount, r->m_noteCount);
    eq("camera count", o->m_cameraCount, r->m_cameraCount);
    eq("rotation", rel(o->m_rotation, o->m_data), rel(r->m_rotation, r->m_data));
    eq("translation", rel(o->m_translation, o->m_data), rel(r->m_translation, r->m_data));
    eq("scale", rel(o->m_scale, o->m_data), rel(r->m_scale, r->m_data));
    eq("note", rel(o->m_note, o->m_data), rel(r->m_note, r->m_data));
    eq("camera", rel(o->m_camera, o->m_data), rel(r->m_camera, r->m_data));
    eq("animation count", o->m_animationCount, r->m_animationCount);
    eq("animations", o->m_animations != nullptr, r->m_animations != nullptr);
    if (o->m_data && r->m_data) eq("data bytes", std::memcmp(o->m_data, r->m_data, keptSize), 0);
    if (o->m_animations && r->m_animations && o->m_animationCount == r->m_animationCount) {
        for (unsigned i = 0; i < o->m_animationCount; i++) {
            eq("animation's database", o->m_animations[i].m_database == o, r->m_animations[i].m_database == r);
            eq("animation", std::memcmp((char*)&o->m_animations[i] + 4, (char*)&r->m_animations[i] + 4, sizeof(AtAnimation) - 4), 0);
        }
        g_animations_compared += o->m_animationCount;
    }
    if (o->m_keys && r->m_keys) same_keys(static_cast<AtGrD3DAnimationKeys*>(o->m_keys), static_cast<AtGrD3DAnimationKeys*>(r->m_keys), o->m_data, r->m_data);
    same_map(what, *o, *r, o->m_animations, r->m_animations);
}

// ---- making each side's objects --------------------------------------------------------------------------------------

static int g_fill_byte = 0xCD;

template <class T> static T* side1_new() {
    const bool was = g_recording;
    g_recording = false;
    void* p = std::malloc(sizeof(T) + 0x100);
    std::memset(p, g_fill_byte, sizeof(T) + 0x100);
    g_recording = was;
    return new (p) T;
}

static void* side0_new(unsigned size, uint32_t ctor) {
    const bool was = g_recording;
    g_recording = false;
    void* p = std::malloc(size + 0x100);
    std::memset(p, g_fill_byte, size + 0x100);
    g_recording = was;
    at<Fn0>(ctor)(p, nullptr);
    return p;
}

static unsigned header_word(const std::string& file, unsigned at) {
    return file.size() >= at + 4 ? *(const unsigned*)(file.data() + at) : 0;
}

// ---- one .XKA ---------------------------------------------------------------------------------------------------------

static unsigned g_xka_files, g_xgg_files;

static void run_xka(const std::string& path, const std::string& file, Mode mode, int fill) {
    g_context = "xka " + path;
    g_objects[0].clear();
    g_objects[1].clear();
    g_fill = fill;
    g_fill_byte = fill;
    const std::string key = upper(path);
    g_files[key] = file;
    const unsigned keptSize = std::min(header_word(file, 0x10 + 0x1C), (unsigned)(file.size() > 0x88 ? file.size() : 0));
    std::string name = path.substr(0, path.size() - 4);
    g_fs->files[0].mode = mode;
    g_fs->files[1].mode = mode;
    AtGrD3DAnimationDatabase* o;
    AtGrD3DAnimationDatabase* r;
    { On on(0); o = (AtGrD3DAnimationDatabase*)side0_new(0x50, 0x001028D0); }
    { On on(1); r = side1_new<AtGrD3DAnimationDatabase>(); }
    logs("made");
    eq("made", std::memcmp((char*)o + 4, (char*)r + 4, 0x50 - 4), 0);
    const bool oc = guarded([&] { On on(0); at<FnLoad2>(0x00102970)(o, nullptr, g_dev[0], name.c_str()); });
    g_recording = false;
    const bool rc = guarded([&] { On on(1); r->LoadBinaryForm(g_dev[1], name.c_str()); });
    g_recording = false;
    eq("crash reading", oc, rc);
    logs("read");
    g_fs->m_path.m_top = -1;
    if (oc || rc) return;
    same_anim("xka", o, r, keptSize);
    // Each reads the same again over what it read: the base's clear first, through the slot.
    { On on(0); at<Fn0>(0x00102890)(o, nullptr); }
    { On on(1); r->Clear(); }
    logs("cleared");
    eq("cleared data", o->m_data != nullptr, r->m_data != nullptr);
    eq("cleared keys", o->m_keys != nullptr, r->m_keys != nullptr);
    { On on(0); at<FnLoad2>(0x00102970)(o, nullptr, g_dev[0], name.c_str()); }
    { On on(1); r->LoadBinaryForm(g_dev[1], name.c_str()); }
    logs("read again");
    same_anim("xka again", o, r, keptSize);
    { On on(0); at<FnDel>(0x00102D30)(o, nullptr, 0); }
    { On on(1); r->~AtGrD3DAnimationDatabase(); }
    logs("destroyed");
    g_fs->files[0].mode = kNormal;
    g_fs->files[1].mode = kNormal;
    g_fs->m_path.m_top = -1;
}

// ---- one .XGG ---------------------------------------------------------------------------------------------------------

static void run_xgg(const std::string& path, const std::string& file, Mode mode, int fill, unsigned alignShift, bool more) {
    g_context = "xgg " + path;
    g_objects[0].clear();
    g_objects[1].clear();
    g_fill = fill;
    g_fill_byte = fill;
    g_files[upper(path)] = file;
    const unsigned keptSize = header_word(file, 0x10 + 0x30);
    std::string name = path.substr(0, path.size() - 4);
    g_fs->files[0].mode = mode;
    g_fs->files[1].mode = mode;
    make_owners();
    AtGrD3DMeshDatabase* o;
    AtGrD3DMeshDatabase* r;
    { On on(0); o = (AtGrD3DMeshDatabase*)side0_new(0x50, 0x001108B0); }
    { On on(1); r = side1_new<AtGrD3DMeshDatabase>(); }
    logs("made");
    // All but +0x38, which neither constructor sets: there the fill on both sides.
    eq("made", std::memcmp((char*)o + 4, (char*)r + 4, 0x50 - 4), 0);
    unsigned heapAlign[2], heapAlloc[2];
    // Read, and read again over what was read: the second time Allocate clears first.
    for (int pass = 0; pass < 2; pass++)
    for (int side = 0; side < 2; side++) {
        g_mesh_heap.m_alignShift = alignShift;
        g_mesh_heap.m_allocShift = 3;
        const bool c = guarded([&] {
            On on(side);
            if (side == 0) at<FnLoad2>(0x001109E0)(o, nullptr, g_own[0], name.c_str());
            else r->LoadBinaryForm(g_own[1], name.c_str());
        });
        g_recording = false;
        slot('!', c);
        heapAlign[side] = g_mesh_heap.m_alignShift;
        heapAlloc[side] = g_mesh_heap.m_allocShift;
        g_fs->m_path.m_top = -1;
    }
    logs("read");
    eq("heap align", heapAlign[0], heapAlign[1]);
    eq("heap alloc", heapAlloc[0], heapAlloc[1]);
    same_mesh("xgg", o, r, keptSize);
    eq("on the owner's list", g_own[0]->m_meshDatabases == o, g_own[1]->m_meshDatabases == r);
    // Each slot that hands out an item, at a few indices.
    for (unsigned i : {0u, 1u, 3u}) {
        eq("mesh at", rel(at<FnAt>(0x00110470)(o, nullptr, i), o->m_meshes), rel(r->MeshAt(i), r->m_meshes));
        eq("vertex array at", rel(at<FnAt>(0x00110880)(o, nullptr, i), o->m_vertexArrays), rel(r->VertexArrayAt(i), r->m_vertexArrays));
        eq("index array at", rel(at<FnAt>(0x00110890)(o, nullptr, i), o->m_indexArrays), rel(r->IndexArrayAt(i), r->m_indexArrays));
    }
    if (more) {
        { On on(0); (*(*(Fn0**)o + 8))(o, nullptr); }
        { On on(1); r->Slot8(); }
        logs("more");
        eq("more made", o->m_more != nullptr, r->m_more != nullptr);
        if (o->m_more && r->m_more) {
            same_mesh("more", (AtGrD3DMeshDatabase*)o->m_more, (AtGrD3DMeshDatabase*)r->m_more, 0);
            eq("more on the list", g_own[0]->m_meshDatabases == o->m_more, g_own[1]->m_meshDatabases == r->m_more);
            { On on(0); at<FnDel>(0x001125D0)(o->m_more, nullptr, 1); }
            { On on(1); delete (AtGrD3DMeshDatabase*)r->m_more; }
            logs("more destroyed");
            eq("list after", g_own[0]->m_meshDatabases == o, g_own[1]->m_meshDatabases == r);
        }
    }
    { On on(0); at<Fn0>(0x001108F0)(o, nullptr); }
    { On on(1); r->Clear(); }
    logs("cleared");
    same_mesh("cleared", o, r, 0);
    eq("off the list", g_own[0]->m_meshDatabases != nullptr, g_own[1]->m_meshDatabases != nullptr);
    { On on(0); at<FnDel>(0x001125D0)(o, nullptr, 0); }
    { On on(1); r->~AtGrD3DMeshDatabase(); }
    logs("destroyed");
    g_fs->files[0].mode = kNormal;
    g_fs->files[1].mode = kNormal;
}

// ---- one .AGD, through each side's loader -----------------------------------------------------------------------------

static unsigned g_agd_files;

static void same_loader(const char* what, AtGrD3DDatabaseLoader* o, AtGrD3DDatabaseLoader* r) {
    eq("loaded owner", o->m_owner != nullptr, r->m_owner != nullptr);
    eq("counts", std::memcmp((char*)o + 0x80, (char*)r + 0x80, 0xA6 - 0x80), 0);
    eq("owner", o->m_owner == g_own[0] || o->m_owner == nullptr, r->m_owner == g_own[1] || r->m_owner == nullptr);
    eq("textures", o->m_textures != nullptr, r->m_textures != nullptr);
    eq("shaders", o->m_shaders != nullptr, r->m_shaders != nullptr);
    eq("materials", o->m_materials != nullptr, r->m_materials != nullptr);
    eq("meshes", o->m_meshes != nullptr, r->m_meshes != nullptr);
    eq("lights", o->m_lights != nullptr, r->m_lights != nullptr);
    eq("fonts", o->m_fonts != nullptr, r->m_fonts != nullptr);
    eq("images", o->m_images != nullptr, r->m_images != nullptr);
    eq("vertex processes", o->m_vertexProcesses != nullptr, r->m_vertexProcesses != nullptr);
    eq("pixel processes", o->m_pixelProcesses != nullptr, r->m_pixelProcesses != nullptr);
    eq("loaders", o->m_loaders != nullptr, r->m_loaders != nullptr);
    eq("animations", o->m_animations != nullptr, r->m_animations != nullptr);
    if (o->m_meshes && r->m_meshes) {
        for (unsigned i = 0; i < r->m_meshCount; i++) same_mesh(what, &o->m_meshes[i], &r->m_meshes[i], r->m_meshes[i].m_keptSize);
    }
    if (o->m_animations && r->m_animations) {
        for (unsigned i = 0; i < r->m_animationCount; i++) {
            AtGrD3DAnimationDatabase* a = (AtGrD3DAnimationDatabase*)o->m_animations[i];
            AtGrD3DAnimationDatabase* b = (AtGrD3DAnimationDatabase*)r->m_animations[i];
            eq("animation database", a != nullptr, b != nullptr);
            if (a && b) same_anim(what, a, b, 0);
        }
    }
    if (o->m_loaders && r->m_loaders) {
        for (unsigned i = 0; i < r->m_loaderCount; i++) same_loader("nested", &o->m_loaders[i], &r->m_loaders[i]);
    }
    // Each element of the kinds behind the seam: whether its base's load gave it to the owner, and its table's size.
    struct { void* o; void* r; unsigned count; unsigned stride; } kinds[] = {
        {o->m_textures, r->m_textures, r->m_textureCount, 0x58}, {o->m_shaders, r->m_shaders, r->m_shaderCount, 0x1C}, {o->m_materials, r->m_materials, r->m_materialCount, 0x40},
        {o->m_lights, r->m_lights, r->m_lightCount, 0x20}, {o->m_fonts, r->m_fonts, r->m_fontCount, 0x3C}, {o->m_images, r->m_images, r->m_imageCount, 0x34},
        {o->m_vertexProcesses, r->m_vertexProcesses, r->m_vertexProcessCount, 0x24}, {o->m_pixelProcesses, r->m_pixelProcesses, r->m_pixelProcessCount, 0x24},
    };
    for (const auto& k : kinds) {
        if (!k.o || !k.r) continue;
        for (unsigned i = 0; i < k.count; i++) {
            GmNameMap* a = (GmNameMap*)((char*)k.o + i * k.stride);
            GmNameMap* b = (GmNameMap*)((char*)k.r + i * k.stride);
            eq("element in a group", a->m_group != nullptr, b->m_group != nullptr);
            eq("element's table", a->m_size, b->m_size);
            eq("element's entries", a->m_entries != nullptr, b->m_entries != nullptr);
        }
    }
    same_map("animation map", o->m_animationMap, r->m_animationMap, nullptr, nullptr);
    same_map("coord map", o->m_coordSystemMap, r->m_coordSystemMap, o->m_coordSystems, r->m_coordSystems);
    eq("mesh list", g_own[0]->m_meshDatabases != nullptr, g_own[1]->m_meshDatabases != nullptr);
}

static void run_agd(const std::string& archive, const std::string& path, const std::string& text, int mode) {
    g_context = "agd " + path;
    g_objects[0].clear();
    g_objects[1].clear();
    g_archive = archive;
    g_fill = 0xCD;
    g_fill_byte = 0xCD;
    g_files[upper(archive) + "|" + upper(path)] = text;
    const std::string name = path.substr(0, path.size() - 4);
    make_owners();
    AtGrD3DDatabaseLoader* o;
    AtGrD3DDatabaseLoader* r;
    { On on(0); o = (AtGrD3DDatabaseLoader*)side0_new(0xD8, 0x00112D60); }
    { On on(1); r = side1_new<AtGrD3DDatabaseLoader>(); }
    logs("made");
    eq("made", std::memcmp((char*)o + 0x80, (char*)r + 0x80, 0xD8 - 0x80), 0);
    eq("made owner", std::memcmp((char*)o + 4, (char*)r + 4, 0x30 - 4), 0);
    const bool oc = guarded([&] { On on(0); at<FnLoadD>(0x0001ADC0)(o, nullptr, g_own[0], name.c_str(), mode, nullptr, 1); });
    g_recording = false;
    g_fs->m_path.m_top = -1;
    const bool rc = guarded([&] { On on(1); r->Load(g_own[1], name.c_str(), mode, nullptr, 1); });
    g_recording = false;
    g_fs->m_path.m_top = -1;
    eq("crash loading", oc, rc);
    logs("loaded");
    if (!oc && !rc) {
        same_loader("loaded", o, r);
        // Every other one is destroyed with all it holds, without an unload first.
        if (g_agd_files % 2 == 0) {
            { On on(0); at<Fn0>(0x00112DB0)(o, nullptr); }
            { On on(1); r->Unload(); }
            logs("unloaded");
            same_loader("unloaded", o, r);
        }
        { On on(0); at<FnDel>(0x00113090)(o, nullptr, 1); }
        { On on(1); delete r; }
        logs("destroyed");
    }
    g_archive.clear();
    g_agd_files++;
}

// ---- written cases ----------------------------------------------------------------------------------------------------

static void put(std::string& s, size_t at, unsigned v) {
    if (s.size() < at + 4) s.resize(at + 4);
    std::memcpy(&s[at], &v, 4);
}

// A small .XGG: two meshes, vertex arrays and index arrays as asked, one stream, a contiguous block or none, with a name space or none.
static std::string written_xgg(unsigned vertexArrays, unsigned indexArrays, unsigned contiguous, const char* ns) {
    std::string head(0x10, 'h');
    std::string table(0x58, '\0');
    std::memcpy(&table[0], ns, std::strlen(ns));
    put(table, 0x08, 0x200);
    put(table, 0x0C, 0x180); put(table, 0x10, 2);
    put(table, 0x14, 0x150);
    put(table, 0x18, vertexArrays); put(table, 0x1C, 0x00);
    put(table, 0x20, indexArrays); put(table, 0x24, 0x100);
    put(table, 0x28, 1); put(table, 0x2C, 0x110);
    put(table, 0x30, 0x80);
    for (unsigned k = 0x34; k <= 0x48; k += 4) put(table, k, (k - 0x30) * 4);
    put(table, 0x4C, contiguous);
    put(table, 0x50, 4); put(table, 0x54, 8);
    std::string scratch(0x200, '\0');
    for (unsigned i = 0; i < 0x200; i += 4) put(scratch, i, 0x1000 + i);
    std::memcpy(&scratch[0x150 + 0x0C], "MESHA\0\0\0", 8);
    std::memcpy(&scratch[0x168 + 0x0C], "MESHB\0\0\0", 8);
    std::string kept(0x80, 'k');
    std::string contig(contiguous, 'c');
    return head + table + scratch + kept + contig;
}

static std::string written_xka(unsigned animations) {
    std::string head(0x10, 'h');
    std::string table(0x78, '\0');
    put(table, 0x00, 0x10 + animations * 0x60 + 0x10);
    put(table, 0x04, animations);
    put(table, 0x08, 0);
    put(table, 0x0C, 0x10);
    put(table, 0x10, 0x1000);
    put(table, 0x14, 0x3F800000);
    put(table, 0x18, 9);
    put(table, 0x1C, 0x80);
    for (unsigned k = 0x20; k < 0x78; k += 8) { put(table, k, k / 8); put(table, k + 4, k - 0x20); }
    std::string scratch(0x10 + animations * 0x60 + 0x10, '\0');
    for (unsigned i = 0; i < scratch.size(); i += 4) put(scratch, i, 0x7FA00000 + i);  // signalling reals among them
    std::memcpy(&scratch[0], "RUN\0\0\0\0\0WALKWALK", 16);
    std::string kept(0x80, 'k');
    return head + table + scratch + kept;
}

static void run_written() {
    // Each kind of list, with one and with two names, and none.
    g_files["MODEL\\BOX.XGG"] = written_xgg(2, 1, 0x20, "NS");
    g_files["MODEL\\BOX2.XGG"] = written_xgg(0, 0, 0, "");
    g_files["MODEL\\RUN.XKA"] = written_xka(2);
    g_files["MODEL\\RUN2.XKA"] = written_xka(0);
    g_files["MODEL\\SUB.AGD"] = "MeshDatabase[1]\n{\n \"BOX\"\n}\n";
    // Material and image lists that reach their database's own slots, so that a log says which element was loaded.
    for (const char* n : {"MODEL\\A.AGM", "MODEL\\B.AGM"}) g_files[n] = "Material[0]\n{\n}\n";
    for (const char* n : {"MODEL\\A.AGI", "MODEL\\B.AGI"}) g_files[n] = "Image[0]\n{\n}\n";
    // Texture lists with a texture each, named for their file: the recovered texture database loads them, and the double for a texture's slot 12 logs which.
    g_files["MODEL\\A.AGT"] = "Surface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"TA\"\n {\n }\n}\n";
    g_files["MODEL\\B.AGT"] = "Surface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"TB\"\n {\n }\n}\n";
    g_files["MODEL\\SUB2.AGD"] = "AnimationDatabase[1]\n{\n \"RUN\"\n}\nTextureDatabase[2]\n{\n \"A\"\n \"B\"\n}\n";
    struct { const char* word; const char* names; } kinds[] = {
        {"TextureDatabase", "\"A\" \"B\""}, {"ShaderDatabase", "\"A\" \"B\""}, {"MaterialDatabase", "\"A\" \"B\""}, {"MeshDatabase", "\"BOX\" \"BOX2\""},
        {"LightDatabase", "\"A\" \"B\""}, {"FontDatabase", "\"A\" \"B\""}, {"ImageDatabase", "\"A\" \"B\""}, {"VertexProcessDatabase", "\"A\" \"B\""},
        {"PixelProcessDatabase", "\"A\" \"B\""}, {"DatabaseLoader", "\"SUB\" \"SUB2\""}, {"AnimationDatabase", "\"RUN\" \"RUN2\""},
    };
    for (const auto& k : kinds) {
        for (int count : {0, 1, 2}) {
            std::string list = std::string(k.word) + "[" + std::to_string(count) + "]\n{\n";
            if (count >= 1) {
                std::string names = k.names;
                const size_t space = names.find(' ');
                list += " " + (count == 1 || space == std::string::npos ? names.substr(0, space) : names) + "\n";
                if (count == 2 && space == std::string::npos) continue;
            }
            list += "}\n";
            run_agd("", "MODEL\\THING.AGD", list, 2);
            // Text forms, where nothing binary is listed: an .AGG or .AKA reaches the keys' and meshes' text slots, which are not recovered here.
            if (std::strcmp(k.word, "MeshDatabase") != 0 && std::strcmp(k.word, "AnimationDatabase") != 0 && std::strcmp(k.word, "DatabaseLoader") != 0) run_agd("", "MODEL\\THING.AGD", list, 0);
        }
    }
    // Everything at once.
    run_agd("", "MODEL\\ALL.AGD",
            "TextureDatabase[1]\n{\n \"A\"\n}\nMeshDatabase[2]\n{\n \"BOX\"\n \"BOX2\"\n}\nAnimationDatabase[1]\n{\n \"RUN\"\n}\nLightDatabase[1]\n{\n \"L\"\n}\nImageDatabase[1]\n{\n \"I\"\n}\nFontDatabase[1]\n{\n \"F\"\n}\n",
            2);

    // The binary readers on written files: each count and block, with no name space, alignments either side of 5, and reading in pieces.
    for (int fill : {0xCD, 0x00}) {
        for (unsigned shift : {2u, 5u, 6u}) {
            run_xgg("MODEL\\BOX.XGG", g_files["MODEL\\BOX.XGG"], kNormal, fill, shift, true);
            run_xgg("MODEL\\BOX2.XGG", g_files["MODEL\\BOX2.XGG"], kNormal, fill, shift, true);
        }
        run_xka("MODEL\\RUN.XKA", g_files["MODEL\\RUN.XKA"], kNormal, fill);
        run_xka("MODEL\\RUN2.XKA", g_files["MODEL\\RUN2.XKA"], kNormal, fill);
    }
    // Reading in pieces is left out: the table would be read short, and the rest of it is stack, different on each side.
    for (Mode mode : {kPendingIo, kWaitFails}) {
        run_xgg("MODEL\\BOX.XGG", g_files["MODEL\\BOX.XGG"], mode, 0xCD, 2, false);
        run_xka("MODEL\\RUN.XKA", g_files["MODEL\\RUN.XKA"], mode, 0xCD);
    }
    // Cut short: after the table, in each block. What a read could not fill is the heap's, the same fill on both sides.
    const std::string xgg = written_xgg(2, 1, 0x20, "NS");
    for (size_t cut : {(size_t)0x68, (size_t)0x100, (size_t)0x268, (size_t)0x2A0, (size_t)0x2F8}) run_xgg("MODEL\\CUT.XGG", xgg.substr(0, cut), kNormal, 0xCD, 2, false);
    const std::string xka = written_xka(2);
    for (size_t cut : {(size_t)0x88, (size_t)0xC0, (size_t)0x1A8, (size_t)0x1D0}) run_xka("MODEL\\CUT.XKA", xka.substr(0, cut), kNormal, 0xCD);
    // No file at all: the report, then a read through no file, which crashes on both sides.
    g_fs->open_status = 2;
    run_xgg("MODEL\\NONE.XGG", std::string(), kNormal, 0xCD, 2, false);
    run_xka("MODEL\\NONE.XKA", std::string(), kNormal, 0xCD);
    g_fs->open_status = 0;
}

// ---- the small things, called directly ---------------------------------------------------------------------------------

// A token for each heap of this work, so that a heap swapped for another shows in the log.
static int g_heap_own[6];

static void set_heaps(void* h) {
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C63F0u, 0x002C62DCu, 0x002C6374u, 0x002C62E4u, 0x002C6344u, 0x003ADF7Cu, 0x003AE370u, 0x003B13CCu}) *(void**)(uintptr_t)va = h;
    *(void**)(uintptr_t)0x002C637C = &g_mesh_heap;
    AtCoordSysDatabase::s_heap = h;
    AtIoStream::s_heap = h;
    GmNameMap::s_heap = h;
    GmNameSpaceDb::s_heap = h;
    AtMaterialDatabase::s_heap = h;
    AtTextureDatabase::s_heap = h;
    *(void**)(uintptr_t)0x003B1F90 = AtGrD3DMaterialDatabase::s_heap = h;  // the concrete material database's own
    // The shader, light, font and image databases' arrays, and what their items read onto.
    *(void**)(uintptr_t)0x002C6380 = AtShaderDatabase::s_heap = h;
    *(void**)(uintptr_t)0x002C6378 = AtLightDatabase::s_heap = h;
    *(void**)(uintptr_t)0x002C6370 = AtFontDatabase::s_heap = h;
    *(void**)(uintptr_t)0x002C6364 = AtImageDatabase::s_heap = h;
    *(void**)(uintptr_t)0x002C63EC = AtShader::s_heap = h;
    *(void**)(uintptr_t)0x002C63E0 = AtMatAssignment::s_heap = h;
    *(void**)(uintptr_t)0x003B1FCC = AtGrI2DFont::s_heap = h;
    AtGrD3DMeshDatabase::s_heap = &g_mesh_heap;
    // This work's own heaps, each its own token on both sides.
    *(void**)(uintptr_t)0x002C60B0 = AtIoTokenFile::s_heap = &g_heap_own[0];
    *(void**)(uintptr_t)0x002C6374 = AtAnimationDatabase::s_heap = &g_heap_own[1];
    *(void**)(uintptr_t)0x002C62DC = AtDatabaseLoader::s_heap = &g_heap_own[2];
    *(void**)(uintptr_t)0x003ADF7C = AtGrD3DAnimationDevice::s_heap = &g_heap_own[3];
    *(void**)(uintptr_t)0x003AE370 = AtXboxFactory::s_soundHeap = &g_heap_own[4];
    *(void**)(uintptr_t)0x003B13CC = AtXboxFactory::s_loaderHeap = &g_heap_own[5];
}
// Keys of the harness's, so that the base's own functions run over one table on both sides.
class TestKeys : public AtAnimationKeys {
public:
    void Allocate(unsigned int count) override { slot('k', count); }
    void Free() override { slot('f'); }
    void ReadEntries(AtIoTokenizer*) override {}
    void Clear() override { slot('X'); if (g_side == 0) at<Fn0>(0x00036630)(this, nullptr); else AtAnimationKeys::Clear(); }
    void Interpolate(unsigned int, unsigned int, float, int, int*, int*, float*) override {}
    int Find(unsigned int, unsigned int, float, int) override { return 0; }
    void Load() override {}
    unsigned short At(unsigned int) override { return 0; }
    void Slot9(unsigned int c) override { slot('9', c); } void Slot10(unsigned int c) override { slot('a', c); } void Slot11(unsigned int c) override { slot('b', c); }
    void Slot12(unsigned int c) override { slot('c', c); } void Slot13(unsigned int c) override { slot('d', c); }
    void Slot14() override { slot('e'); } void Slot15() override { slot('E'); } void Slot16() override { slot('h'); } void Slot17() override { slot('i'); } void Slot18() override { slot('j'); }
    void ReadRotationKeys(AtIoTokenizer*) override {} void ReadTranslationKeys(AtIoTokenizer*) override {} void ReadScaleKeys(AtIoTokenizer*) override {}
    void ReadNoteKeys(AtIoTokenizer*) override {} void ReadCameraKeys(AtIoTokenizer*) override {}
};

static void run_direct() {
    typedef void(__fastcall* FnInit)(void*, void*, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);
    typedef void(__fastcall* FnName)(void*, void*, const char*);
    // The keys' base: constructor, Init with each count, Clear with each count, the name, the destructor.
    g_context = "keys base";
    for (unsigned mask = 0; mask < 64; mask += 7) {
        TestKeys* k[2];
        for (int side = 0; side < 2; side++) {
            void* p = std::malloc(sizeof(TestKeys));
            std::memset(p, 0x5A, sizeof(TestKeys));
            k[side] = new (p) TestKeys;
            void* table = *(void**)p;
            std::memset(p, 0x5A, sizeof(TestKeys));
            On on(side);
            if (side == 0) at<Fn0>(0x000364A0)(p, nullptr);
            else new (p) TestKeys;
            *(void**)p = table;
        }
        logs("made");
        eq("constructed", std::memcmp((char*)k[0] + 4, (char*)k[1] + 4, sizeof(AtAnimationKeys) - 4), 0);
        for (int side = 0; side < 2; side++) k[side]->m_flags = mask & 1 ? 1u : 0u;
        const unsigned c[6] = {mask & 1u, mask & 2u, mask & 4u, mask & 8u, mask & 16u, mask & 32u};
        { On on(0); at<FnInit>(0x000365A0)(k[0], nullptr, c[0], c[1], c[2], c[3], c[4], c[5]); }
        { On on(1); k[1]->Init(c[0], c[1], c[2], c[3], c[4], c[5]); }
        logs("init");
        eq("after init", std::memcmp((char*)k[0] + 4, (char*)k[1] + 4, sizeof(AtAnimationKeys) - 4), 0);
        { On on(0); at<Fn0>(0x00036630)(k[0], nullptr); }
        { On on(1); k[1]->AtAnimationKeys::Clear(); }
        logs("clear");
        eq("after clear", std::memcmp((char*)k[0] + 4, (char*)k[1] + 4, sizeof(AtAnimationKeys) - 4), 0);
        for (int side = 0; side < 2; side++) { k[side]->m_rotationKeyCount = mask; k[side]->m_cameraKeyCount = mask & 3; k[side]->m_count = mask & 5; }
        { On on(0); at<FnName>(0x00036480)(k[0], nullptr, "NAMEDKEY"); }
        { On on(1); k[1]->AtAnimationKeys::SetExternalName("NAMEDKEY"); }
        logs("name");
        eq("after name", std::memcmp((char*)k[0] + 4, (char*)k[1] + 4, sizeof(AtAnimationKeys) - 4), 0);
        // With a count left, the base's destructor calls a pure slot of its own table, on both sides, which the harness makes a crash: a concrete class's destructor clears first, so that never happens in the
        // game. Every other time the counts are cleared first.
        for (int side = 0; side < 2; side++) {
            if (mask % 2 == 0) k[side]->m_rotationKeyCount = k[side]->m_translationKeyCount = k[side]->m_scaleKeyCount = k[side]->m_noteKeyCount = k[side]->m_cameraKeyCount = 0;
            k[side]->m_count = 0;  // the set's own clear would otherwise reach a pure slot too, and hide the base's
        }
        const bool oc = guarded([&] { On on(0); at<Fn0>(0x00036820)(k[0], nullptr); });
        g_recording = false;
        const bool rc = guarded([&] { On on(1); k[1]->AtAnimationKeys::~AtAnimationKeys(); });
        g_recording = false;
        eq("crash destroying", oc, rc);
        logs("destroyed");
        if (!oc && !rc) eq("after destroying", std::memcmp((char*)k[0] + 4, (char*)k[1] + 4, sizeof(AtAnimationKeys) - 4), 0);
    }
    // An animation from a record, with every kind of bit pattern.
    g_context = "animation record";
    typedef void(__fastcall* FnRec)(void*, void*, void*, const void*);
    for (unsigned seed = 1; seed < 40; seed++) {
        unsigned char rec[0x60];
        for (unsigned i = 0; i < sizeof rec; i++) rec[i] = (unsigned char)(seed * 37 + i * (seed | 1));
        if (seed & 1) *(unsigned*)&rec[4] = 0x7FA00001;
        AtAnimation a, b;
        std::memset(&a, 0x33, sizeof a);
        std::memset(&b, 0x33, sizeof b);
        at<FnRec>(0x000359B0)(&a, nullptr, (void*)0x1234, rec);
        b.ReadBinary((AtAnimationDatabase*)0x1234, (const AtXkaAnimation*)rec);
        eq("animation", std::memcmp(&a, &b, sizeof a), 0);
    }
    // The device: each slot, and its keys by themselves.
    g_context = "device";
    g_fill = 0xCD;
    for (int fail : {0, 1}) {
        AtAnimationKeys* k[2];
        AtAnimationDatabase* d[2];
        g_fail_alloc = fail != 0;
        { On on(0); k[0] = (AtAnimationKeys*)at<FnNew>(0x000C87E0)(g_dev[0], nullptr); }
        { On on(1); k[1] = g_dev[1]->NewKeys(); }
        { On on(0); d[0] = (AtAnimationDatabase*)at<FnNew>(0x000C8880)(g_dev[0], nullptr); }
        { On on(1); d[1] = g_dev[1]->NewDatabase(); }
        g_fail_alloc = false;
        logs("new");
        eq("keys", k[0] != nullptr, k[1] != nullptr);
        if (k[0] && k[1]) {
            eq("new keys", std::memcmp((char*)k[0] + 4, (char*)k[1] + 4, sizeof(AtGrD3DAnimationKeys) - 4), 0);
            // Slots that free each list, with a list to free.
            AtGrD3DAnimationKeys* ko = (AtGrD3DAnimationKeys*)k[0];
            AtGrD3DAnimationKeys* kr = (AtGrD3DAnimationKeys*)k[1];
            const uint32_t frees[] = {0x00101790, 0x00101800, 0x00101870, 0x001018E0, 0x00101950, 0x001019C0};
            for (int i = 0; i < 6; i++) {
                void* bo = filled(8);
                void* br = filled(8);
                ((void**)&ko->m_frames)[i] = bo;
                ((void**)&kr->m_frames)[i] = br;
                { On on(0); at<Fn0>(frees[i])(ko, nullptr); }
                { On on(1);
                    switch (i) {
                    case 0: kr->Free(); break;
                    case 1: kr->Slot14(); break;
                    case 2: kr->Slot15(); break;
                    case 3: kr->Slot16(); break;
                    case 4: kr->Slot17(); break;
                    default: kr->Slot18(); break;
                    }
                }
                logs("a free");
                ((void**)&ko->m_frames)[i] = nullptr;
                ((void**)&kr->m_frames)[i] = nullptr;
            }
            // Clear, with and without the flag that says the lists are a file's.
            for (unsigned flags : {0x400u, 0u}) {
                ko->m_flags = kr->m_flags = flags;
                ko->m_count = kr->m_count = 0;
                ko->m_unknown40 = kr->m_unknown40 = 5;
                { On on(0); at<Fn0>(0x00101F90)(ko, nullptr); }
                { On on(1); kr->Clear(); }
                logs("keys clear");
                eq("cleared keys", std::memcmp((char*)ko + 4, (char*)kr + 4, sizeof(AtGrD3DAnimationKeys) - 4), 0);
            }
        }
        { On on(0); at<Fn1>(0x000C8940)(g_dev[0], nullptr, k[0]); }
        { On on(1); g_dev[1]->FreeKeys(k[1]); }
        logs("free keys");
        eq("database", d[0] != nullptr, d[1] != nullptr);
        if (d[0] && d[1]) eq("new database", std::memcmp((char*)d[0] + 4, (char*)d[1] + 4, sizeof(AtGrD3DAnimationDatabase) - 4), 0);
        { On on(0); at<Fn1>(0x000EB9D0)(g_dev[0], nullptr, d[0]); }
        { On on(1); g_dev[1]->FreeDatabase(d[1]); }
        logs("free database");
    }
    {
        void* a = std::malloc(4);
        AtGrD3DAnimationDevice* b = (AtGrD3DAnimationDevice*)std::malloc(sizeof(AtGrD3DAnimationDevice));
        at<Fn0>(0x000C8920)(a, nullptr);
        new (b) AtGrD3DAnimationDevice;
        at<Fn0>(0x000C8930)(a, nullptr);
        b->~AtGrD3DAnimationDevice();
        eq("device's table", *(uint32_t*)a, 0x0028663C);
    }
    // The factory, with a heap, none, and no memory.
    g_context = "factory";
    for (int k = 0; k < 3; k++) {
        if (k == 1) { *(void**)(uintptr_t)0x003AE370 = nullptr; *(void**)(uintptr_t)0x003B13CC = nullptr; AtXboxFactory::s_soundHeap = nullptr; AtXboxFactory::s_loaderHeap = nullptr; }
        g_fail_alloc = k == 2;
        AtXboxFactory f;
        void* o1;
        void* r1;
        void* o2;
        void* r2;
        { On on(0); o1 = at<FnNew>(0x000E9E90)(&f, nullptr); }
        { On on(1); r1 = f.NewSoundDatabase(); }
        { On on(0); o2 = at<FnNew>(0x000EBB50)(&f, nullptr); }
        { On on(1); r2 = f.NewDatabaseLoader(); }
        g_fail_alloc = false;
        set_heaps(&g_heap_token);
        logs("factory");
        eq("sound database", o1 != nullptr, r1 != nullptr);
        eq("loader", o2 != nullptr, r2 != nullptr);
        if (o1 && r1) {
            eq("sound database's table", *(uint32_t*)o1 == 0x0028A448, *(void**)r1 == g_sound_table);
            eq("sound database's fields", std::memcmp((char*)o1 + 4, (char*)r1 + 4, 0x28 - 4), 0);
        }
        if (o2 && r2) {
            eq("loader's table", *(uint32_t*)o2, 0x0028A7D0);
            eq("loader's fields", std::memcmp((char*)o2 + 0x80, (char*)r2 + 0x80, 0xD8 - 0x80), 0);
        }
    }
    // The binary file: each way of opening.
    g_context = "binary file";
    typedef int(__fastcall* FnOpen)(void*, void*, const char*, int);
    typedef int(__fastcall* FnOpenX)(void*, void*, const char*, const char*, int);
    typedef int(__fastcall* FnRelease)(void*, void*, void*);
    for (int flags : {0, 1, 2, 3}) {
        for (int has : {1, 0}) {
            g_fs->has = has != 0;
            AtAudioBankFile a, b;
            int oa, ra;
            { On on(0); oa = at<FnOpen>(0x000249D0)(&a, nullptr, "DIR\\F.XGG", flags); }
            { On on(1); ra = b.Open("DIR\\F.XGG", flags); }
            eq("open", oa, ra);
            // Again over an open file: the first is closed.
            { On on(0); oa = at<FnOpen>(0x000249D0)(&a, nullptr, "DIR\\G.XGG", flags); }
            { On on(1); ra = b.Open("DIR\\G.XGG", flags); }
            eq("open again", oa, ra);
            for (const char* n : {"DIR\\F", "DIR\\F.X", "D.IR\\F", "F", "F."}) {
                { On on(0); oa = at<FnOpenX>(0x00024AA0)(&a, nullptr, n, "XKA", flags); }
                { On on(1); ra = b.OpenWithExtension(n, "XKA", flags); }
                eq("open with extension", oa, ra);
            }
            { On on(0); oa = at<FnRelease>(0x00024980)(&a, nullptr, nullptr); }
            { On on(1); ra = b.Release(nullptr); }
            eq("release", oa, ra);
            if (a.m_file && b.m_file) {
                typedef int(__fastcall* FnRead)(void*, void*, void*, unsigned, unsigned*, void*);
                char ba[8], bb[8];
                unsigned da = 77, db = 77;
                AtIoFileRequest* qa = nullptr;
                AtIoFileRequest* qb = nullptr;
                { On on(0); oa = at<FnRead>(0x00024950)(&a, nullptr, ba, 4, &da, &qa); }
                { On on(1); ra = b.ReadPending(bb, 4, &db, &qb); }
                eq("read", oa, ra);
                eq("read's count", da, db);
            }
            { On on(0); at<Fn0>(0x000249A0)(&a, nullptr); }
            { On on(1); b.Close(); }
            { On on(0); oa = at<FnRelease>(0x00024980)(&a, nullptr, nullptr); }
            { On on(1); ra = b.Release(nullptr); }
            eq("release with no file", oa, ra);
            logs("binary file");
            g_fs->m_path.m_top = -1;
        }
    }
    g_fs->has = true;
    // The base's Allocate, with counts the binary reader never passes, over nothing and over itself.
    g_context = "allocate";
    typedef void(__fastcall* FnAlloc)(void*, void*, void*, unsigned, unsigned, unsigned, const char*);
    for (const char* ns : {(const char*)nullptr, "", "NS"}) {
        make_owners();
        AtGrD3DMeshDatabase* o;
        AtGrD3DMeshDatabase* r;
        { On on(0); o = (AtGrD3DMeshDatabase*)side0_new(0x50, 0x001108B0); }
        { On on(1); r = side1_new<AtGrD3DMeshDatabase>(); }
        logs("made");
        const unsigned counts[2][3] = {{2, 3, 1}, {1, 0, 2}};
        for (const auto& c : counts) {
            { On on(0); at<FnAlloc>(0x0002B0E0)(o, nullptr, g_own[0], c[0], c[1], c[2], ns); }
            { On on(1); r->Allocate(g_own[1], c[0], c[1], c[2], ns); }
            logs("allocated");
            same_mesh("allocated", o, r, 0);
        }
        { On on(0); at<FnDel>(0x001125D0)(o, nullptr, 0); }
        { On on(1); r->~AtGrD3DMeshDatabase(); }
        logs("destroyed");
    }
    // The owner's list, in each order of removal.
    g_context = "owner's list";
    typedef void(__fastcall* FnList)(void*, void*, void*);
    for (int which = 0; which < 4; which++) {
        make_owners();
        AtGrD3DMeshDatabase* d[2][3];
        for (int side = 0; side < 2; side++) for (int i = 0; i < 3; i++) d[side][i] = side1_new<AtGrD3DMeshDatabase>();
        for (int i = 0; i < 3; i++) {
            at<FnList>(0x000EF140)(g_own[0], nullptr, d[0][i]);
            g_own[1]->AddMeshDatabase(d[1][i]);
        }
        AtGrD3DMeshDatabase stranger;
        at<FnList>(0x000EF160)(g_own[0], nullptr, which < 3 ? d[0][which] : &stranger);
        g_own[1]->RemoveMeshDatabase(which < 3 ? d[1][which] : &stranger);
        for (AtGrD3DMeshDatabase *a = g_own[0]->m_meshDatabases, *b = g_own[1]->m_meshDatabases;; a = a->m_next, b = b->m_next) {
            eq("list", a ? (int)(std::find(d[0], d[0] + 3, a) - d[0]) : -1, b ? (int)(std::find(d[1], d[1] + 3, b) - d[1]) : -1);
            if (!a || !b) break;
        }
        g_own[0]->m_meshDatabases = nullptr;
        g_own[1]->m_meshDatabases = nullptr;
    }
}

// One call on each side that may raise, crash or run out: the same must happen on both, and the logs agree up to there.
template <class O, class R> static void fragile(const char* what, O orig, R rec) {
    g_reads_seen = 0;
    const bool oc = guarded([&] { On on(0); orig(); });
    g_recording = false;
    g_fail_alloc = false;
    g_fs->m_path.m_top = -1;
    g_reads_seen = 0;
    const bool rc = guarded([&] { On on(1); rec(); });
    g_recording = false;
    g_fail_alloc = false;
    g_fs->m_path.m_top = -1;
    eq("crash", oc, rc);
    logs(what);
}

static void fragile_set(const std::string& context) {
    typedef void(__fastcall* FnAlloc)(void*, void*, void*, unsigned, unsigned, unsigned, const char*);
    g_context = context + ": xka";
    {
        AtGrD3DAnimationDatabase* o;
        AtGrD3DAnimationDatabase* r;
        { On on(0); o = (AtGrD3DAnimationDatabase*)side0_new(0x50, 0x001028D0); }
        { On on(1); r = side1_new<AtGrD3DAnimationDatabase>(); }
        logs("made");
        fragile("xka", [&] { at<FnLoad2>(0x00102970)(o, nullptr, g_dev[0], "MODEL\\RUN"); }, [&] { r->LoadBinaryForm(g_dev[1], "MODEL\\RUN"); });
    }
    g_context = context + ": xgg";
    {
        make_owners();
        AtGrD3DMeshDatabase* o;
        AtGrD3DMeshDatabase* r;
        { On on(0); o = (AtGrD3DMeshDatabase*)side0_new(0x50, 0x001108B0); }
        { On on(1); r = side1_new<AtGrD3DMeshDatabase>(); }
        logs("made");
        fragile("xgg", [&] { at<FnLoad2>(0x001109E0)(o, nullptr, g_own[0], "MODEL\\BOX"); }, [&] { r->LoadBinaryForm(g_own[1], "MODEL\\BOX"); });
        fragile("slot 8", [&] { (*(*(Fn0**)o + 8))(o, nullptr); }, [&] { r->Slot8(); });
    }
    g_context = context + ": allocate";
    {
        make_owners();
        AtGrD3DMeshDatabase* o;
        AtGrD3DMeshDatabase* r;
        { On on(0); o = (AtGrD3DMeshDatabase*)side0_new(0x50, 0x001108B0); }
        { On on(1); r = side1_new<AtGrD3DMeshDatabase>(); }
        logs("made");
        fragile("allocate", [&] { at<FnAlloc>(0x0002B0E0)(o, nullptr, g_own[0], 2, 3, 1, "NS"); }, [&] { r->Allocate(g_own[1], 2, 3, 1, "NS"); });
    }
    g_context = context + ": device";
    fragile("new keys", [&] { at<FnNew>(0x000C87E0)(g_dev[0], nullptr); }, [&] { g_dev[1]->NewKeys(); });
    fragile("new database", [&] { at<FnNew>(0x000C8880)(g_dev[0], nullptr); }, [&] { g_dev[1]->NewDatabase(); });
    g_context = context + ": agd";
    {
        make_owners();
        AtGrD3DDatabaseLoader* o;
        AtGrD3DDatabaseLoader* r;
        { On on(0); o = (AtGrD3DDatabaseLoader*)side0_new(0xD8, 0x00112D60); }
        { On on(1); r = side1_new<AtGrD3DDatabaseLoader>(); }
        logs("made");
        fragile("agd", [&] { at<FnLoadD>(0x0001ADC0)(o, nullptr, g_own[0], "MODEL\\ALL", 2, nullptr, 1); }, [&] { r->Load(g_own[1], "MODEL\\ALL", 2, nullptr, 1); });
    }
}

// A device the same on both sides, whose keys are each side's own, and after which memory runs out: so that the animations' allocation is the first to fail.
class StarvingDevice : public AtAnimationDevice {
public:
    AtAnimationKeys* NewKeys() override {
        AtAnimationKeys* keys;
        if (g_side == 0) keys = (AtAnimationKeys*)side0_new(0x48, 0x00101F60);
        else keys = side1_new<AtGrD3DAnimationKeys>();
        g_fail_alloc = true;
        return keys;
    }
    void FreeKeys(AtAnimationKeys*) override {}
    AtAnimationDatabase* NewDatabase() override { return nullptr; }
    void FreeDatabase(AtAnimationDatabase*) override {}
};

static void run_failures() {
    g_context = "out of memory after the keys";
    {
        static StarvingDevice starving;
        g_fill = 0xCD;
        AtGrD3DAnimationDatabase* o;
        AtGrD3DAnimationDatabase* r;
        { On on(0); o = (AtGrD3DAnimationDatabase*)side0_new(0x50, 0x001028D0); }
        { On on(1); r = side1_new<AtGrD3DAnimationDatabase>(); }
        logs("made");
        fragile("xka", [&] { at<FnLoad2>(0x00102970)(o, nullptr, &starving, "MODEL\\RUN"); }, [&] { r->LoadBinaryForm(&starving, "MODEL\\RUN"); });
    }
    g_fill = 0xCD;
    g_files["MODEL\\ALL.AGD"] = "TextureDatabase[1]\n{\n \"A\"\n}\nMeshDatabase[2]\n{\n \"BOX\"\n \"BOX2\"\n}\nAnimationDatabase[1]\n{\n \"RUN\"\n}\nDatabaseLoader[1]\n{\n \"SUB\"\n}\n";
    // Each heap of this work missing in turn.
    struct { uint32_t va; void** ours; const char* name; } heaps[] = {
        {0x002C60B0, &AtIoTokenFile::s_heap, "token heap"}, {0x002C6374, &AtAnimationDatabase::s_heap, "animation heap"}, {0x002C62DC, &AtDatabaseLoader::s_heap, "loader heap"},
        {0x003ADF7C, &AtGrD3DAnimationDevice::s_heap, "device heap"}, {0x002C637C, &AtGrD3DMeshDatabase::s_heap, "mesh heap"},
    };
    for (const auto& h : heaps) {
        *(void**)(uintptr_t)h.va = nullptr;
        *h.ours = nullptr;
        fragile_set(std::string("no ") + h.name);
        set_heaps(&g_heap_token);
    }
    // Memory running out after so many reads of any file.
    for (int reads = 1; reads <= 7; reads++) {
        g_fail_after_reads = reads;
        fragile_set("out of memory after " + std::to_string(reads) + " reads");
    }
    g_fail_after_reads = -1;
}

// A call through a pure slot, on either side, as a crash the guard can catch: the C library's handlers would end the process.
static void __cdecl pure_call() { *(volatile int*)nullptr = 0; }

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

struct IndexRow { std::string file, archive, path; };

static std::vector<IndexRow> read_index(const std::string& dir) {
    std::vector<IndexRow> rows;
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) return rows;
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        char* a = std::strchr(line, '\t');
        if (!a) continue;
        *a++ = 0;
        char* b = std::strchr(a, '\t');
        if (!b) continue;
        *b++ = 0;
        char* c = std::strchr(b, '\t');
        if (c) *c = 0;
        rows.push_back({line, a, b});
    }
    std::fclose(index);
    return rows;
}

static int finish();

int main(int argc, char** argv) {
    AddVectoredExceptionHandler(0, on_crash);
    setvbuf(stdout, nullptr, _IONBF, 0);
    g_trace = std::getenv("ORACLE_TRACE") != nullptr;
    const char* entries = nullptr;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { std::printf("usage: oracle_loader <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    // The seam classes on the original's side.
    // The texture and material databases are recovered (include/at/AtGrD3DTextures.h): the original's run on its side. What their items do with the device is the doubles above.
    redirect(0x000FCAD0, (const void*)o_load_texture);
    redirect_recovered_slot12((const void*)o_load_texture);
    redirect(0x0010B7F0, (const void*)o_load_material);
    redirect(0x000FB630, (const void*)o_release_stage);
    redirect(0x00205E20, (const void*)o_before_release);
    redirect(0x00205D10, (const void*)o_release);
    // The shader, light, font and image databases are recovered (include/at/AtGrD3DItems.h): the original's run on its side. What a font or an image does with the device is the doubles above.
    redirect(0x00123850, (const void*)o_load_font);
    redirect(0x00124CD0, (const void*)o_load_image);
    redirect(0x00111E50, (const void*)&Thin<AtGrD3DVertexProcessDatabase, 0x0002B690, 0x0002B870, 0x00111E50, 0x00111EC0, 0x0028A7AC, 4>::Construct);
    redirect(0x001120B0, (const void*)&Thin<AtGrD3DPixelProcessDatabase, 0x0002B430, 0x0002B610, 0x001120B0, 0x00112120, 0x0028A7BC, 5>::Construct);
    redirect(0x000EC890, (const void*)&Side0<AtGrD3DVertexArray, 2>::Construct);
    redirect(0x000F8010, (const void*)&Side0<AtGrD3DIndexArray, 1>::Construct);
    redirect(0x000EECA0, (const void*)&Side0<AtGrD3DVertexStream, 1>::Construct);
    redirect(0x00122460, (const void*)&Side0<AtGrD3DMesh, 0>::Construct);
    redirect(0x000EC8C0, (const void*)o_read_vertex);
    redirect(0x000F7EE0, (const void*)o_read_index);
    redirect(0x000EE750, (const void*)o_read_stream);
    redirect(0x00122550, (const void*)o_read_mesh);
    redirect(0x00205EA0, (const void*)o_contiguous);
    redirect(0x00205EE0, (const void*)o_free_contiguous);
    redirect(0x00125F25, (const void*)pure_call);
    _set_purecall_handler(pure_call);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new PathFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    *(AtIoContext**)(uintptr_t)0x002C6328 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    AtAudioBankFile::s_context = &g_ctx;
    set_heaps(&g_heap_token);
    g_heap_pad = 0x1000;
    g_fs->m_path.m_top = -1;
    {
        void* d = std::malloc(4);
        at<Fn0>(0x000C8920)(d, nullptr);
        g_dev[0] = (AtAnimationDevice*)d;
        g_dev[1] = new AtGrD3DAnimationDevice;
    }

    {
        AtSoXboxDatabase* ref = new AtSoXboxDatabase;
        g_sound_table = *(void**)ref;
    }
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_direct();
    if (g_failures) return finish();
    run_written();
    if (g_failures) return finish();
    run_failures();
    if (g_failures) return finish();

    // Every distinct .XKA and .XGG of the disc.
    const std::string tok = pos[1];
    g_bin_dir = tok + "\\..\\scratch\\loader\\bin";
    std::vector<IndexRow> bins = read_index(g_bin_dir);
    if (bins.empty()) { std::printf("cannot read %s\\index.txt: the binary files are written by tests/oracle/loader_cases.py\n", g_bin_dir.c_str()); return 2; }
    std::map<std::string, std::string> contents;
    for (const IndexRow& row : bins) {
        auto it = contents.find(row.file);
        if (it == contents.end()) it = contents.emplace(row.file, read_file(g_bin_dir + "\\" + row.file)).first;
        g_files[upper(row.archive) + "|" + upper(row.path)] = it->second;
        g_files[upper(row.path)] = it->second;
    }
    std::map<std::string, bool> done;
    unsigned n = 0;
    for (const IndexRow& row : bins) {
        if (done[row.file]) continue;
        done[row.file] = true;
        const std::string& file = contents[row.file];
        const std::string ext = upper(row.path.substr(row.path.size() - 4));
        n++;
        const Mode mode = n % 7 == 3 ? kPendingIo : n % 7 == 5 ? kWaitFails : kNormal;
        if (ext == ".XKA") {
            run_xka(row.path, file, mode, n % 2 ? 0xCD : 0x00);
            g_xka_files++;
        } else if (ext == ".XGG") {
            run_xgg(row.path, file, mode, n % 2 ? 0xCD : 0x00, n % 3 == 0 ? 6 : 2, n % 5 == 0);
            g_xgg_files++;
        }
        // The path's own entry was replaced by the run: put it back.
        g_files[upper(row.path)] = file;
        if (g_failures) return finish();
    }

    // Every distinct .AGD, through each side's loader, as the game asks: binary forms. The one loose .AGD that the game names with mode 0, as it does.
    for (const IndexRow& row : read_index(tok)) {
        if (upper(row.path).size() < 4 || upper(row.path).substr(row.path.size() - 4) != ".AGD") continue;
        const std::string text = read_file(tok + "\\" + row.file);
        run_agd(row.archive, row.path, text, 2);
        if (g_failures) return finish();
    }
    for (const IndexRow& row : bins) {
        if (upper(row.path).find("LOADER.AGD") != std::string::npos) run_agd("", "DataXbox\\UI\\DirtyDsk\\loader.AGD", read_file(g_bin_dir + "\\" + row.file), 0);
    }

    // Every binary file a real .AGD names must be there, or the runs above compared empty files.
    eq("binary files named and missing", g_binary_missing, 0);
    return finish();
}

// A difference ends the run: what follows would not be read.
static int finish() {
    report_entries();
    std::printf("%s: %u .AGD loaded, %u .XGG and %u .XKA read; %u mesh databases (%u meshes), %u animation databases (%u animations) compared; %u opens (%u binary files named by real .AGD files, all found); %llu comparisons, %d differences\n",
                g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_agd_files, g_xgg_files, g_xka_files, g_mesh_dbs, g_meshes_compared, g_anim_dbs, g_animations_compared, g_opens, g_binary_found, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
