// Differential oracle for the data library and the database manager (Windows x86, local only). See docs/verification.md.
//
//   oracle_add <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: the one .ADD on the disc, with the eleven real .ADR registries it names under the real GENERIC.ADR as their parent, as start-up loads them; and text written here. No .ADB exists on the disc, so the
// manager rests on written text alone.
//
// Both sides are whole, but for three members of AtDataString that no branch has recovered (0x0006A1D0, 0x0006A4D0, 0x00069FD0): the recovered side reaches the ORIGINAL's code for them through the seams, on the
// recovered side's own objects, with the original's helpers logging to whichever side is running. The registries (.ADR), field descriptors and value classes are each side's own: the original's here, the
// recovered ones of include/at/AtDataRegistry.h there. Every .ADR a written library names is the same short written registry; the parent is a written one, or the real GENERIC.ADR for the disc's library.
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

#include "at/AtDataLibrary.h"
#include "at/AtIoTokenizer.h"
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

#define AtFatal AtFatalInDoubles
#define AtPushHeap AtPushHeapInDoubles
#include "oracle_doubles.h"
#undef AtFatal
#undef AtPushHeap

// A file ending inside a list makes both sides report for ever, since a report comes back here and not in the game. So after this many in one run, a deliberate crash ends it, at the same report on both sides.
static int g_reports;
static void count_report() {
    if (g_expect_crash && ++g_reports > 60) *(volatile int*)nullptr = 0;
}
// Heap requests fail from this push on, counted per side: every allocation is preceded by a push.
static int g_pushes;
static int g_fail_at_push = -1;
static void count_push() {
    if (g_recording && g_pushes++ == g_fail_at_push) g_fail_alloc = true;
}

void AtFatal(const char* m, int a, int b, int c) { AtFatalInDoubles(m, a, b, c); count_report(); }
void AtPushHeap(void* h) { count_push(); AtPushHeapInDoubles(h); }

// The original's helpers log to the side that is running, since the recovered side reaches the original's value classes too.
static void* __cdecl s_new(unsigned n) { if (g_recording) g_log[g_side].add('N', n); return filled(n); }
// While a library runs, nothing freed is given back to the heap on either side. A registry the library deletes stays listed by the owner, as in the original, and the next registry filed points at it;
// whether that stale pointer then names a new block depends on the host's heap, which differs by side. Kept, it names nothing on both.
static bool g_keep_freed;
static void give_back(void* p) { if (!g_keep_freed) std::free(p); }
static void __cdecl s_delete(void* p) { if (g_recording && p) g_log[g_side].add('D'); give_back(p); }
static void host_delete(void* p) { if (g_recording && g_side == 1 && p) g_log[1].add('D'); give_back(p); }
static void host_delete_sized(void* p, std::size_t) { if (g_recording && g_side == 1 && p) g_log[1].add('D'); give_back(p); }
struct KeepFreed {
    KeepFreed() { g_keep_freed = true; }
    ~KeepFreed() { g_keep_freed = false; }
};
static void __cdecl s_push(void* h) { count_push(); if (g_recording) g_log[g_side].add('P', (uint32_t)(uintptr_t)h); }
static void __cdecl s_pop() { if (g_recording) g_log[g_side].add('p'); }
static void __cdecl s_fatal(const char* m, int a, int, int) { if (g_recording) { g_log[g_side].add('F', a); record_message(m, g_side); } count_report(); }
static void __cdecl s_raise(int code, int, int, int) { if (g_recording) g_log[g_side].add('R', code); }

static std::vector<unsigned char> bytes(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

// ---- files ------------------------------------------------------------------------------------------------------------
// An .ADR by its name, upper case and without the extension; any other name is kSmallAdr.
static std::map<std::string, std::vector<unsigned char>> g_adr;
static const char kSmallAdr[] =
    "DataComponentTypes[ 3 ]\n{\n DataComponent { Type Int NumComponents 2 }\n DataComponent { Type Real NumComponents 1 }\n DataComponent { Type ShortName NumComponents 1 }\n}\n"
    "DataRegistry[ 3 ]\n{\n DataLink { Name \"Speed\" Type Int Data 7 }\n DataLink { Name \"Mass\" Type Real Floor 0.5 Ceiling 4.0 Data 9.5 }\n"
    " DataLink { Name \"Tag\" Type ShortName Parent \"Top\\\" Data \"tag\" }\n}\n";
static const char kParentAdr[] =
    "NameSpace \"Gen\"\nDataComponentTypes[ 2 ]\n{\n DataComponent { Type Empty NumComponents 1 }\n DataComponent { Type Int NumComponents 1 }\n}\n"
    "DataRegistry[ 2 ]\n{\n DataLink { Name \"Top\" Type Empty Data }\n DataLink { Name \"Gravity\" Type Int Parent \"Top\\\" Data 3 }\n}\n";
static std::vector<unsigned char> g_small_adr(kSmallAdr, kSmallAdr + sizeof kSmallAdr - 1);
static unsigned g_adr_opened, g_adr_real_opened;

static std::string adr_key(const char* path) {
    std::string k;
    for (const char* p = path; *p; p++) k += (char)((*p >= 'a' && *p <= 'z') ? *p - 32 : *p);
    const size_t dot = k.rfind(".ADR");
    return dot == std::string::npos ? k : k.substr(0, dot);
}

class Fs : public MemFs {
public:
    std::vector<unsigned char> add, adb;
    char base[MAX_PATH] = "DATA\\LIBRARY.ADD";
    int Open(const char* path, int flags, AtIoFile** out) override {
        if (std::strstr(path, ".ADR") || std::strstr(path, ".adr")) {
            // The key's string would log as the recovered side's allocation: made with the log off.
            const bool was = g_recording;
            g_recording = false;
            const auto i = g_adr.find(adr_key(path));
            g_recording = was;
            const std::vector<unsigned char>& d = i != g_adr.end() ? i->second : g_small_adr;
            if (g_side == 1) { g_adr_opened++; if (i != g_adr.end()) g_adr_real_opened++; }
            files[g_side].data = d.data();
            files[g_side].size = d.size();
            strncpy_s(files[g_side].m_path, path, _TRUNCATE);
            return MemFs::Open(path, flags, out);
        }
        const std::vector<unsigned char>& d = std::strstr(path, ".ADB") ? adb : add;
        files[g_side].data = d.data();
        files[g_side].size = d.size();
        strcpy_s(files[g_side].m_path, base);
        return MemFs::Open(path, flags, out);
    }
};
static Fs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;

static void reset_path() { g_fs->m_path.m_top = -1; }
static void set_path(const char* path) {
    strcpy_s(g_fs->base, path);
    for (int side = 0; side < 2; side++) strcpy_s(g_fs->files[side].m_path, path);
}

typedef void(__fastcall* Fn0)(void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__cdecl* FnSet)(void*);
typedef void(__fastcall* FnLibLoad)(void*, void*, void*, const char*, int, void*);
typedef void(__fastcall* FnEntries)(void*, void*, int, void*);
typedef int(__fastcall* FnMgrLoad)(void*, void*, void*, const char*, int, char*, unsigned, int);
typedef int(__fastcall* FnMgrRead)(void*, void*, void*, void*, char*, unsigned, int);
typedef void*(__fastcall* FnFind)(void*, void*, const char*);
typedef void(__fastcall* FnAdd)(void*, void*, void*, const char*);
typedef int(__fastcall* FnLayoutRead)(void*, void*, void*, void*);
typedef void*(__fastcall* FnAt)(void*, void*, unsigned);
typedef int(__fastcall* FnTableLoad)(void*, void*, void*, void*, void*, unsigned, int, int);
typedef int(__fastcall* FnTableRead)(void*, void*, void*, void*, unsigned, int, int);
typedef int(__fastcall* FnRecordSet)(void*, void*, unsigned, void*, void*);
typedef void*(__fastcall* FnFindValue)(void*, void*, unsigned, const void*);
typedef void*(__fastcall* FnTake)(void*, void*, unsigned*);
typedef int(__fastcall* FnSlot7)(void*, void*, int);
typedef void(__fastcall* FnFlags)(void*, void*, unsigned);
typedef int(__fastcall* FnOpen)(void*, void*, const char*, const char*, int, void*);
typedef int(__fastcall* FnDescRead)(void*, void*, void*);

static uint32_t& orig_u32(uint32_t va) { return *(uint32_t*)(uintptr_t)va; }

// ---- the registries: real on both sides -------------------------------------------------------------------------------
static AtDataRegistryOwner* g_owner[2];
static AtDataRegistry* g_parent[2];

// AtDataString's Forget, BindAndRead and Bind (0x0006A1D0, 0x0006A4D0, 0x00069FD0) are at/datalookup's: each side runs its own.

// Each value class, by type: the original's vftable and size, and the recovered class's vftable, read from one made here.
static const uint32_t kOrigVft[12] = {0x00277754, 0x0027777C, 0x002777A4, 0x002777CC, 0x002777F4, 0x0027781C, 0x0027784C, 0x00277874, 0x0027789C, 0x002778C4, 0x002778EC, 0x00277914};
static const unsigned kSize[12] = {0x0C, 0x10, 0x10, 0x10, 0x14, 0x3C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x14};
static const unsigned kHostSize[12] = {sizeof(AtDataComponent), sizeof(AtDataBoolean), sizeof(AtDataInt), sizeof(AtDataReal), sizeof(AtDataShortName), sizeof(AtDataString),
                                       sizeof(AtDataBooleanArray), sizeof(AtDataByteArray), sizeof(AtDataIntArray), sizeof(AtDataRealArray), sizeof(AtDataShortNameArray), sizeof(AtDataStringArray)};
static const void* g_vft[12];
static const void* g_descriptor_vft;
template <class T> static const void* vft_of() {
    alignas(8) static unsigned char space[sizeof(T)];
    T* t = new (space) T;
    return *(const void* const*)t;
}
static void read_vfts() {
    g_vft[0] = vft_of<AtDataComponent>(); g_vft[1] = vft_of<AtDataBoolean>(); g_vft[2] = vft_of<AtDataInt>(); g_vft[3] = vft_of<AtDataReal>();
    g_vft[4] = vft_of<AtDataShortName>(); g_vft[5] = vft_of<AtDataString>(); g_vft[6] = vft_of<AtDataBooleanArray>(); g_vft[7] = vft_of<AtDataByteArray>();
    g_vft[8] = vft_of<AtDataIntArray>(); g_vft[9] = vft_of<AtDataRealArray>(); g_vft[10] = vft_of<AtDataShortNameArray>(); g_vft[11] = vft_of<AtDataStringArray>();
    g_descriptor_vft = vft_of<AtDataDescriptor>();
}
// A value's class by its vftable, on its own side; -1 for anything else.
static int class_of(int side, const void* value) {
    const void* v = *(const void* const*)value;
    for (int i = 0; i < 12; i++) if (side == 0 ? v == (const void*)(uintptr_t)kOrigVft[i] : v == g_vft[i]) return i;
    return -1;
}

// ---- comparing ---------------------------------------------------------------------------------------------------------
static std::string nm(const char* p) { return std::string(p, strnlen(p, 8)); }
static long long index_of(const void* base, const void* p, unsigned size) { return !p ? -1 : !base ? -2 : ((const char*)p - (const char*)base) / (long long)size; }

// How a map's value reads on each side: an offset from the side's own base, kept apart by the caller.
struct Rel {
    const void* base;
    unsigned size;
};
static void same_map(const char* what, const GmNameMap* o, Rel ob, const GmNameMap* r, Rel rb) {
    eq(what, o->m_size, r->m_size);
    eq(what, o->m_entries != nullptr, r->m_entries != nullptr);
    if (!o->m_entries || !r->m_entries || o->m_size != r->m_size) return;
    for (unsigned i = 0; i < o->m_size; i++) {
        const GmNameMap::Entry& x = o->m_entries[i];
        const GmNameMap::Entry& y = r->m_entries[i];
        eq("slot in use", x.m_value != nullptr, y.m_value != nullptr);
        if (!x.m_value || !y.m_value) continue;
        eq("slot's name", std::strncmp(x.m_name, y.m_name, 8), 0);
        eq("slot's value", index_of(ob.base, x.m_value, ob.size), index_of(rb.base, y.m_value, rb.size));
    }
}

static void same_descriptor(const AtDataDescriptor& a, const AtDataDescriptor& b) {
    eqs("descriptor's name", nm(a.m_name).c_str(), nm(b.m_name).c_str());
    eq("descriptor's vftable", *(const uint32_t*)&a == 0x0027793C, *(const void* const*)&b == g_descriptor_vft);
    eq("descriptor's type", a.m_type, b.m_type);
    eq("descriptor's flags", a.m_flags, b.m_flags);
    eq("descriptor's count", a.m_count, b.m_count);
    eq("descriptor's floor", a.m_intFloor, b.m_intFloor);
    eq("descriptor's ceiling", a.m_intCeiling, b.m_intCeiling);
    eq("descriptor's real floor", *(const uint32_t*)&a.m_realFloor, *(const uint32_t*)&b.m_realFloor);
    eq("descriptor's real ceiling", *(const uint32_t*)&a.m_realCeiling, *(const uint32_t*)&b.m_realCeiling);
    eq("descriptor's format", a.m_format != nullptr, b.m_format != nullptr);
    if (a.m_format && b.m_format) eqs("descriptor's format", a.m_format, b.m_format);
}

// An array's elements, compared one by one so that a difference says where.
static void same_array(unsigned type, const char* a, const char* b, unsigned count, unsigned width) {
    const unsigned char* x = *(const unsigned char* const*)(a + 0xC);
    const unsigned char* y = *(const unsigned char* const*)(b + 0xC);
    eq("array", x != nullptr, y != nullptr);
    if (!x || !y) return;
    for (unsigned i = 0; i < count * width; i++) {
        if (x[i] != y[i]) { eq(type == 7 ? "byte array" : "array", x[i] | (i << 8) | (type << 24), y[i] | (i << 8) | (type << 24)); return; }
    }
    g_checks++;
}

static void same_value(unsigned type, const AtDatabaseManager* om, const AtDatabaseManager* rm, const AtDataRecordDatabase* ot, const AtDataRecordDatabase* rt, const char* a, const char* b, unsigned count) {
    const AtDataDescriptor* ad = ((const AtDataComponent*)a)->m_descriptor;
    const AtDataDescriptor* bd = ((const AtDataComponent*)b)->m_descriptor;
    eq("value's class", class_of(0, a), class_of(1, b));
    eq("value's descriptor", index_of(om->m_descriptors, ad, 0x2C), index_of(rm->m_descriptors, bd, 0x2C));
    switch (type) {
        case 1: case 2: case 3:
            eq("value", *(const uint32_t*)(a + 0xC), *(const uint32_t*)(b + 0xC));
            break;
        case 4:
            eq("short name", std::memcmp(a + 0xC, b + 0xC, 8), 0);
            break;
        case 5: {
            // The narrow text's window on the shared characters: capacity, start, end, and where it points.
            for (int word = 0x10; word <= 0x1C; word += 4) {
                const uint32_t x = *(const uint32_t*)(a + word), y = *(const uint32_t*)(b + word);
                const uintptr_t ox = (uintptr_t)ot->m_characters, oy = (uintptr_t)rt->m_characters;
                const bool xin = x >= ox && x < ox + ot->m_characterSize, yin = y >= oy && y < oy + rt->m_characterSize;
                eq("string's word", xin ? x - ox : x, yin ? y - oy : y);
            }
            eq("string's kind", *(const uint32_t*)(a + 0x34), *(const uint32_t*)(b + 0x34));
            break;
        }
        case 6: case 8: case 9:
            same_array(type, a, b, count, 4);
            break;
        case 7:
            same_array(type, a, b, count, 1);
            break;
        case 10:
            // Up to a terminator only: past it each element holds what the original's 0x0006AFB0 left from its own stack, which differs with the caller's frames.
            if (*(const char* const*)(a + 0xC) && *(const char* const*)(b + 0xC)) {
                for (unsigned i = 0; i < count; i++) eq("short name in an array", std::strncmp(*(const char* const*)(a + 0xC) + i * 8, *(const char* const*)(b + 0xC) + i * 8, 8), 0);
            }
            break;
        case 11: {
            const unsigned short* const* x = *(const unsigned short* const* const*)(a + 0xC);
            const unsigned short* const* y = *(const unsigned short* const* const*)(b + 0xC);
            eq("texts", x != nullptr, y != nullptr);
            if (x && y) {
                for (unsigned i = 0; i < count; i++) {
                    eq("text", x[i] != nullptr, y[i] != nullptr);
                    if (x[i] && y[i]) eq("text's characters", wcscmp((const wchar_t*)x[i], (const wchar_t*)y[i]), 0);
                }
            }
            break;
        }
    }
}

static void same_table(const AtDatabaseManager* om, const AtDatabaseManager* rm, const AtDataRecordDatabase* o, const AtDataRecordDatabase* r) {
    eqs("table's name", nm(o->m_name).c_str(), nm(r->m_name).c_str());
    eq("table's description", o->m_description != nullptr, r->m_description != nullptr);
    if (o->m_description && r->m_description) eqs("table's description", o->m_description, r->m_description);
    eq("table's rows", o->m_count, r->m_count);
    eq("table's +24", o->m_unknown24, r->m_unknown24);
    eq("table's +2C", o->m_unknown2C, r->m_unknown2C);
    eq("table's character size", o->m_characterSize, r->m_characterSize);
    eq("table's characters used", o->m_characterUsed, r->m_characterUsed);
    eq("table's layout", index_of(om->m_layouts, o->m_layout, sizeof(AtRecordLayout)), index_of(rm->m_layouts, r->m_layout, sizeof(AtRecordLayout)));
    eq("table's characters", o->m_characters != nullptr, r->m_characters != nullptr);
    if (o->m_characters && r->m_characters && o->m_characterSize == r->m_characterSize) eq("characters' bytes", std::memcmp(o->m_characters, r->m_characters, o->m_characterSize), 0);
    if (o->m_count != r->m_count || !o->m_layout || !r->m_layout || o->m_layout->m_count != r->m_layout->m_count) return;
    for (unsigned i = 0; i < o->m_count; i++) {
        const AtDataRecord& a = o->m_records[i];
        const AtDataRecord& b = r->m_records[i];
        eq("record's vftable", *(const uint32_t*)&a == 0x00285608, 1);
        eq("record's index", a.m_index, b.m_index);
        eq("record's layout", a.m_layout == o->m_layout, b.m_layout == r->m_layout);
        eq("record's columns", a.m_columns == o->m_columns, b.m_columns == r->m_columns);
        eq("record in use", a.m_inUse, b.m_inUse);
    }
    for (unsigned c = 0; c < o->m_layout->m_count; c++) {
        const AtDataDescriptor* field = o->m_layout->m_fields[c];
        const AtDataDescriptor* rf = r->m_layout->m_fields[c];
        if (!field || !rf) continue;
        const unsigned type = field->m_type;
        if (type < 1 || type > 11 || rf->m_type != type) continue;
        const char* a = (const char*)o->m_columns[c];
        const char* b = (const char*)r->m_columns[c];
        for (unsigned i = 0; i < o->m_count; i++) same_value(type, om, rm, o, r, a + i * kSize[type], b + i * kHostSize[type], field->m_count);
    }
}

static void same_manager(const AtDatabaseManager* o, const AtDatabaseManager* r) {
    g_recording = false;
    eq("descriptors", o->m_descriptorCount, r->m_descriptorCount);
    eq("layouts", o->m_layoutCount, r->m_layoutCount);
    eq("tables", o->m_tableCount, r->m_tableCount);
    eq("loaded", o->m_loaded, r->m_loaded);
    eq("scale", o->m_characterScale, r->m_characterScale);
    eq("next", o->m_next != nullptr, r->m_next != nullptr);
    eq("file name", o->m_fileName != nullptr, r->m_fileName != nullptr);
    if (o->m_fileName && r->m_fileName) eqs("file name", o->m_fileName, r->m_fileName);
    eq("has descriptors", o->m_descriptors != nullptr, r->m_descriptors != nullptr);
    eq("has layouts", o->m_layouts != nullptr, r->m_layouts != nullptr);
    eq("has tables", o->m_tables != nullptr, r->m_tables != nullptr);
    same_map("descriptors by name", &o->m_descriptorsByName, {o->m_descriptors, 0x2C}, &r->m_descriptorsByName, {r->m_descriptors, 0x2C});
    if (o->m_descriptors && r->m_descriptors && o->m_descriptorCount == r->m_descriptorCount)
        for (unsigned i = 0; i < o->m_descriptorCount; i++) same_descriptor(o->m_descriptors[i], r->m_descriptors[i]);
    if (o->m_layouts && r->m_layouts && o->m_layoutCount == r->m_layoutCount) {
        for (unsigned i = 0; i < o->m_layoutCount; i++) {
            const AtRecordLayout& a = o->m_layouts[i];
            const AtRecordLayout& b = r->m_layouts[i];
            eqs("layout's name", nm(a.m_name).c_str(), nm(b.m_name).c_str());
            eq("layout's vftable", *(const uint32_t*)&a == 0x002855D8, 1);
            eq("layout's description", a.m_description != nullptr, b.m_description != nullptr);
            if (a.m_description && b.m_description) eqs("layout's description", a.m_description, b.m_description);
            eq("layout's count", a.m_count, b.m_count);
            eq("layout's loaded", a.m_loaded, b.m_loaded);
            same_map("layout's map", &a, {a.m_fields, 4}, &b, {b.m_fields, 4});
            if (a.m_fields && b.m_fields && a.m_count == b.m_count)
                for (unsigned c = 0; c < a.m_count; c++) eq("layout's field", index_of(o->m_descriptors, a.m_fields[c], 0x2C), index_of(r->m_descriptors, b.m_fields[c], 0x2C));
        }
    }
    if (o->m_tables && r->m_tables && o->m_tableCount == r->m_tableCount) {
        for (unsigned i = 0; i < o->m_tableCount; i++) {
            eq("table", o->m_tables[i] != nullptr, r->m_tables[i] != nullptr);
            if (o->m_tables[i] && r->m_tables[i]) same_table(o, r, o->m_tables[i], r->m_tables[i]);
        }
    }
    // The manager's own map holds layouts and tables, which are told apart by name here.
    eq("own map", o->m_size, r->m_size);
    eq("own map", o->m_entries != nullptr, r->m_entries != nullptr);
    if (o->m_entries && r->m_entries && o->m_size == r->m_size) {
        for (unsigned i = 0; i < o->m_size; i++) {
            eq("own slot", o->m_entries[i].m_value != nullptr, r->m_entries[i].m_value != nullptr);
            eq("own slot's name", std::strncmp(o->m_entries[i].m_name, r->m_entries[i].m_name, 8), 0);
            const auto kind = [](const AtDatabaseManager* m, const void* v) -> long long {
                if (!v) return -1;
                for (unsigned t = 0; t < m->m_tableCount; t++) if (m->m_tables && m->m_tables[t] == v) return 1000 + t;
                return index_of(m->m_layouts, v, sizeof(AtRecordLayout));
            };
            eq("own slot's value", kind(o, o->m_entries[i].m_value), kind(r, r->m_entries[i].m_value));
        }
    }
}

// ---- a side at a time --------------------------------------------------------------------------------------------------
static AtDatabaseManager* new_o_manager() {
    void* p = std::calloc(1, 0x50);
    at<Fn0>(0x000B1C90)(p, nullptr);
    return (AtDatabaseManager*)p;
}

// Called at the start of each case: also forgets what the file classes logged outside a side.
static void make_owners() {
    const bool was = g_recording;
    g_recording = false;
    g_log[0].n = g_log[1].n = 0;
    g_message[0][0] = g_message[1][0] = 0;
    for (int side = 0; side < 2; side++) {
        g_owner[side] = (AtDataRegistryOwner*)std::calloc(1, sizeof(AtDataRegistryOwner));
        new (&g_owner[side]->m_databases) GmNameMapGroup;
        new (&g_owner[side]->m_databaseSpaces) GmNameSpaceDb;
        g_owner[side]->m_databaseSpaces.Init(8);
        // The registries' groups, each by its own side's constructors.
        if (side == 0) {
            at<Fn0>(0x000603A0)(&g_owner[0]->m_plain, nullptr);
            at<Fn0>(0x0005FFF0)(&g_owner[0]->m_spaces, nullptr);
            at<FnFlags>(0x00060010)(&g_owner[0]->m_spaces, nullptr, 8);
        } else {
            new (&g_owner[1]->m_plain) AtDataRegistryGroup;
            new (&g_owner[1]->m_spaces) AtDataRegistryNameSpaces;
            g_owner[1]->m_spaces.Init(8);
        }
    }
    g_parent[0] = g_parent[1] = nullptr;
    g_recording = was;
}

// ---- registries ----------------------------------------------------------------------------------------------------------
// What each side's objects are called: their place in a walk of the parent and the library's registries. Anything else is "elsewhere", -1.
struct Ids {
    std::map<const void*, long long> m;
    long long next = 1;
    void add(const void* p) { if (p && !m.count(p)) m[p] = next++; }
    long long of(const void* p) const {
        if (!p) return 0;
        const auto i = m.find(p);
        return i == m.end() ? -1 : i->second;
    }
};
static void walk(Ids& ids, int side, const AtDataRegistry* r) {
    if (!r) return;
    ids.add(r);
    ids.add(&r->m_root);
    for (int p = 0; p < AtDataRegistry::kTypeCount; p++) {
        for (const AtDataComponentPool* pool = &r->m_pools[p]; pool; pool = pool->m_next) {
            ids.add(pool);
            for (unsigned i = 0; i < pool->m_count && i < 4096; i++) {
                if (pool->m_links) ids.add(&pool->m_links[i]);
                if (pool->m_descriptors) ids.add(&pool->m_descriptors[i]);
                if (pool->m_values && pool->m_type < 12) ids.add((const char*)pool->m_values + i * (side == 0 ? kSize[pool->m_type] : kHostSize[pool->m_type]));
            }
        }
    }
}
static std::string text_of(const char* p) { return p ? std::string(p) : std::string("(null)"); }
static unsigned g_links_compared;

static void same_value_of_link(int type, const char* a, const char* b, unsigned count) {
    switch (type) {
        case 1: case 2: case 3:
            eq("registry value", *(const uint32_t*)(a + 0xC), *(const uint32_t*)(b + 0xC));
            break;
        case 4:
            eq("registry short name", std::strncmp(a + 0xC, b + 0xC, 8), 0);
            break;
        case 5:
            eq("registry string's kind", *(const uint32_t*)(a + 0x34), *(const uint32_t*)(b + 0x34));
            break;
        case 6: case 8: case 9: case 7: {
            const unsigned width = type == 7 ? 1 : 4;
            const unsigned char* x = *(const unsigned char* const*)(a + 0xC);
            const unsigned char* y = *(const unsigned char* const*)(b + 0xC);
            eq("registry array", x != nullptr, y != nullptr);
            if (x && y && count < 4096) eq("registry array's bytes", std::memcmp(x, y, count * width), 0);
            break;
        }
        case 10: {
            const char* x = *(const char* const*)(a + 0xC);
            const char* y = *(const char* const*)(b + 0xC);
            eq("registry names", x != nullptr, y != nullptr);
            // Up to a terminator only: past it, stack garbage on both sides (see oracle_adr.cpp).
            if (x && y && count < 4096) for (unsigned i = 0; i < count; i++) eq("registry name in an array", std::strncmp(x + i * 8, y + i * 8, 8), 0);
            break;
        }
        case 11: {
            const unsigned short* const* x = *(const unsigned short* const* const*)(a + 0xC);
            const unsigned short* const* y = *(const unsigned short* const* const*)(b + 0xC);
            eq("registry texts", x != nullptr, y != nullptr);
            if (x && y && count < 4096) {
                for (unsigned i = 0; i < count; i++) {
                    eq("registry text", x[i] != nullptr, y[i] != nullptr);
                    if (x[i] && y[i]) eq("registry text's characters", wcscmp((const wchar_t*)x[i], (const wchar_t*)y[i]), 0);
                }
            }
            break;
        }
    }
}

static void same_registry(const Ids* ids, const AtDataRegistry* o, const AtDataRegistry* r) {
    eq("registry's loaded", o->m_loaded, r->m_loaded);
    eq("registry's owner", o->m_owner == g_owner[0], r->m_owner == g_owner[1]);
    eq("registry's owner is null", o->m_owner == nullptr, r->m_owner == nullptr);
    eq("registry's parent", ids[0].of(o->m_parent), ids[1].of(r->m_parent));
    eq("registry's types", o->m_componentTypes, r->m_componentTypes);
    eq("registry's next", ids[0].of(o->m_next), ids[1].of(r->m_next));
    eq("registry's group", o->m_group != nullptr, r->m_group != nullptr);
    if (o->m_group && r->m_group) {
        eq("registry's group is the plain one", o->m_group == &g_owner[0]->m_plain, r->m_group == &g_owner[1]->m_plain);
        eq("registry's group's name", std::strncmp(o->m_group->m_name, r->m_group->m_name, 8), 0);
    }
    eqs("registry's file name", text_of(o->m_fileName).c_str(), text_of(r->m_fileName).c_str());
    eq("root's parent", ids[0].of(o->m_root.m_parent), ids[1].of(r->m_root.m_parent));
    eq("root's first child", ids[0].of(o->m_root.m_firstChild), ids[1].of(r->m_root.m_firstChild));
    eq("root's sibling", ids[0].of(o->m_root.m_sibling), ids[1].of(r->m_root.m_sibling));
    for (int p = 0; p < AtDataRegistry::kTypeCount; p++) {
        const AtDataComponentPool* a = &o->m_pools[p];
        const AtDataComponentPool* b = &r->m_pools[p];
        for (; a && b; a = a->m_next, b = b->m_next) {
            eq("pool's count", a->m_count, b->m_count);
            eq("pool's capacity", a->m_capacity, b->m_capacity);
            eq("pool's type", a->m_type, b->m_type);
            eq("pool's used", a->m_used, b->m_used);
            eq("pool's values", a->m_values != nullptr, b->m_values != nullptr);
            eq("pool's links", a->m_links != nullptr, b->m_links != nullptr);
            if (a->m_count != b->m_count || !a->m_links || !b->m_links || !a->m_descriptors || !b->m_descriptors || a->m_count > 4096) continue;
            for (unsigned i = 0; i < a->m_count; i++) {
                const AtDataLink& x = a->m_links[i];
                const AtDataLink& y = b->m_links[i];
                eq("link in use", x.m_inUse, y.m_inUse);
                if (!x.m_inUse || !y.m_inUse) continue;
                g_links_compared++;
                eqs("link's name", text_of(x.m_name).c_str(), text_of(y.m_name).c_str());
                eq("link's value", ids[0].of(x.m_value), ids[1].of(y.m_value));
                eq("link's parent", ids[0].of(x.m_parent), ids[1].of(y.m_parent));
                eq("link's sibling", ids[0].of(x.m_sibling), ids[1].of(y.m_sibling));
                eq("link's first child", ids[0].of(x.m_firstChild), ids[1].of(y.m_firstChild));
                same_descriptor(a->m_descriptors[i], b->m_descriptors[i]);
                if (x.m_value && y.m_value) {
                    const int type = class_of(0, x.m_value);
                    eq("link value's class", type, class_of(1, y.m_value));
                    eq("link value's descriptor", ids[0].of(x.m_value->m_descriptor), ids[1].of(y.m_value->m_descriptor));
                    eq("link value's registry", ids[0].of(x.m_value->m_registry), ids[1].of(y.m_value->m_registry));
                    if (type >= 0 && type == class_of(1, y.m_value)) same_value_of_link(type, (const char*)x.m_value, (const char*)y.m_value, a->m_descriptors[i].m_count);
                }
            }
        }
        eq("pool's chain", a != nullptr, b != nullptr);
    }
}

// The parent and the library's registries, and the owner's two registry groups.
static void same_registries(const AtDataLibrary* o, const AtDataLibrary* r) {
    Ids ids[2];
    walk(ids[0], 0, g_parent[0]);
    walk(ids[1], 1, g_parent[1]);
    const bool both = o->m_registries && r->m_registries && o->m_registryCount == r->m_registryCount;
    if (both) {
        for (unsigned i = 0; i < o->m_registryCount; i++) {
            walk(ids[0], 0, o->m_registries[i]);
            walk(ids[1], 1, r->m_registries[i]);
        }
    }
    if (g_parent[0] && g_parent[1]) same_registry(ids, g_parent[0], g_parent[1]);
    if (both) {
        for (unsigned i = 0; i < o->m_registryCount; i++) {
            eq("registry", o->m_registries[i] != nullptr, r->m_registries[i] != nullptr);
            if (o->m_registries[i] && r->m_registries[i]) same_registry(ids, o->m_registries[i], r->m_registries[i]);
        }
    }
    eq("plain group's first", ids[0].of(g_owner[0]->m_plain.m_first), ids[1].of(g_owner[1]->m_plain.m_first));
    const AtDataRegistryNameSpaces& a = g_owner[0]->m_spaces;
    const AtDataRegistryNameSpaces& b = g_owner[1]->m_spaces;
    eq("spaces' size", a.m_size, b.m_size);
    if (a.m_pool && b.m_pool && a.m_poolSize == b.m_poolSize) {
        for (unsigned i = 0; i < a.m_poolSize; i++) {
            eq("space's first", ids[0].of(a.m_pool[i].m_first), ids[1].of(b.m_pool[i].m_first));
            eq("space's name", std::strncmp(a.m_pool[i].m_name, b.m_pool[i].m_name, 8), 0);
        }
    }
}

// A parent registry on each side, from kParentAdr or the real GENERIC.ADR, as start-up's App+0x6950. Loaded and compared before the case, with its logs dropped.
static void make_parents() {
    const int fail = g_fail_at_push, status = g_fs->open_status;
    g_fail_at_push = -1;
    g_fs->open_status = 0;
    char space[2][8] = {{0}, {0}};
    for (int side = 0; side < 2; side++) {
        char* p = (char*)std::malloc(sizeof(AtDataRegistry));
        std::memset(p, g_fill, sizeof(AtDataRegistry));
        g_parent[side] = (AtDataRegistry*)p;
    }
    const std::string base = g_fs->base;
    { On on(0); at<Fn0>(0x000674E0)(g_parent[0], nullptr); at<int(__fastcall*)(void*, void*, void*, const char*, int, char*, void*)>(0x00068880)(g_parent[0], nullptr, g_owner[0], "Design\\Registry\\Generic", 0, space[0], nullptr); }
    { On on(1); new (g_parent[1]) AtDataRegistry; g_parent[1]->Load(g_owner[1], "Design\\Registry\\Generic", 0, space[1], nullptr); }
    set_path(base.c_str());
    reset_path();
    eq("parent's space", std::strncmp(space[0], space[1], 8), 0);
    compare_logs("parent");
    g_log[0].n = g_log[1].n = 0;
    g_message[0][0] = g_message[1][0] = 0;
    g_pushes = 0;
    g_fail_at_push = fail;
    g_fs->open_status = status;
}

static void same_owner(const AtDatabaseManager* o, const AtDatabaseManager* r) {
    const GmNameMapGroup& a = g_owner[0]->m_databases;
    const GmNameMapGroup& b = g_owner[1]->m_databases;
    eq("in the plain group", (const void*)a.m_first == o, (const void*)b.m_first == r);
    eq("group", o->m_group != nullptr, r->m_group != nullptr);
    if (o->m_group && r->m_group) {
        eq("the plain group is its group", o->m_group == &a, r->m_group == &b);
        eqs("its group's name", nm(o->m_group->m_name).c_str(), nm(r->m_group->m_name).c_str());
    }
}

enum Which { kBoth };

// Loads an .ADD on both sides, compares, then clears on both sides and compares.
struct LibRun {
    int unused = 0x1234;
    bool with_parent = true;
    int again = 0;
    bool destroy_only = false;
    bool unload_after = false;  // after each load, every registry's loaded flag cleared by hand, as a registry a Clear has emptied
};

static AtDataLibrary* new_o_library(unsigned char fill) {
    void* p = std::malloc(sizeof(AtDataLibrary));
    std::memset(p, fill, sizeof(AtDataLibrary));
    at<Fn0>(0x0009BB10)(p, nullptr);
    return (AtDataLibrary*)p;
}
static AtDataLibrary* new_r_library(unsigned char fill) {
    void* p = std::malloc(sizeof(AtDataLibrary));
    std::memset(p, fill, sizeof(AtDataLibrary));
    return new (p) AtDataLibrary;
}

static void same_library(const AtDataLibrary* o, const AtDataLibrary* r, bool deep) {
    g_recording = false;
    eq("library's loaded", o->m_loaded, r->m_loaded);
    eq("library's owner", o->m_owner == g_owner[0], r->m_owner == g_owner[1]);
    eq("library's owner is null", o->m_owner == nullptr, r->m_owner == nullptr);
    eq("library's registries", o->m_registries != nullptr, r->m_registries != nullptr);
    eq("library's databases", o->m_databases != nullptr, r->m_databases != nullptr);
    eq("registry count", o->m_registryCount, r->m_registryCount);
    eq("database count", o->m_databaseCount, r->m_databaseCount);
    eq("registry names", orig_u32(0x00305938) != 0, AtDataLibrary::s_registryNames != nullptr);
    eq("database names", orig_u32(0x00305934) != 0, AtDataLibrary::s_databaseNames != nullptr);
    if (deep) same_registries(o, r);
    if (deep && o->m_databases && r->m_databases && o->m_databaseCount == r->m_databaseCount) {
        for (unsigned i = 0; i < o->m_databaseCount; i++) {
            eq("database", o->m_databases[i] != nullptr, r->m_databases[i] != nullptr);
            if (o->m_databases[i] && r->m_databases[i]) same_manager(o->m_databases[i], r->m_databases[i]);
        }
    }
}

static void lib_load(AtDataLibrary* o, AtDataLibrary* r, const LibRun& run, bool& crashed) {
    g_reports = 0;
    g_pushes = 0;
    const bool oc = guarded([&] { On on(0); at<FnLibLoad>(0x0009BD50)(o, nullptr, g_owner[0], "LIBRARY", run.unused, run.with_parent ? g_parent[0] : nullptr); });
    g_recording = false; g_side = 0; g_fail_alloc = false;
    const int otop = g_fs->m_path.m_top;
    reset_path();
    g_reports = 0;
    g_pushes = 0;
    const bool rc = guarded([&] { On on(1); r->Load(g_owner[1], "LIBRARY", run.unused, run.with_parent ? g_parent[1] : nullptr); });
    g_recording = false; g_side = 0; g_fail_alloc = false;
    const int rtop = g_fs->m_path.m_top;
    reset_path();
    eq("crashed", oc, rc);
    eq("depth of the path after", otop, rtop);
    crashed = oc || rc;
}

static void run_library_text(const std::string& add, const std::string& adb, unsigned char fill, const LibRun& run = LibRun()) {
    KeepFreed keep;
    g_fs->add = bytes(add);
    g_fs->adb = bytes(adb);
    set_path("DATA\\LIBRARY.ADD");
    g_fill = fill;
    make_owners();
    make_parents();
    AtDataLibrary* o = new_o_library(fill);
    AtDataLibrary* r = new_r_library(fill);
    bool crashed = false;
    lib_load(o, r, run, crashed);
    if (!crashed) same_library(o, r, true);
    compare_logs("load");
    if (crashed) return;  // What it held is left behind.
    for (int i = 0; i < run.again && !crashed; i++) {
        if (run.unload_after && o->m_registries && r->m_registries) {
            for (unsigned k = 0; k < o->m_registryCount; k++) if (o->m_registries[k]) o->m_registries[k]->m_loaded = 0;
            for (unsigned k = 0; k < r->m_registryCount; k++) if (r->m_registries[k]) r->m_registries[k]->m_loaded = 0;
        }
        lib_load(o, r, run, crashed);
        if (!crashed) same_library(o, r, true);
        compare_logs("load again");
    }
    if (crashed) return;
    const bool oc = guarded([&] { On on(0); at<Fn0>(0x0009BD40)(o, nullptr); });
    g_recording = false; g_side = 0;
    const bool rc = guarded([&] { On on(1); r->~AtDataLibrary(); });
    g_recording = false; g_side = 0;
    eq("crashed in the destructor", oc, rc);
    if (!oc && !rc) same_library(o, r, false);
    compare_logs("destroy");
}

// ---- the text ----------------------------------------------------------------------------------------------------------
static const char kFields[] =
    "DataComponentDescs[ 12 ] {\n"
    " DataComponentDesc { Name \"b\" Type Boolean }\n"
    " DataComponentDesc { Name \"i\" Type Int Floor -5 Ceiling 500 FormatString \"%d!\" }\n"
    " DataComponentDesc { Name \"r\" Type Real Floor 0.5 Ceiling 90.25 }\n"
    " DataComponentDesc { Name \"n\" Type ShortName }\n"
    " DataComponentDesc { Name \"s\" Type String Ceiling 15 }\n"
    " DataComponentDesc { Name \"ba\" Type BooleanArray NumElements 2 }\n"
    " DataComponentDesc { Name \"by\" Type ByteArray NumElements 3 }\n"
    " DataComponentDesc { Name \"ia\" Type IntArray NumElements 2 }\n"
    " DataComponentDesc { Name \"ra\" Type RealArray NumElements 2 Floor 0.5 Ceiling 9.5 }\n"
    " DataComponentDesc { Name \"na\" Type ShortNameArray NumElements 2 }\n"
    " DataComponentDesc { Name \"sa\" Type StringArray NumElements 2 Ceiling 7 }\n"
    " DataComponentDesc { Name \"s2\" Type String Ceiling 3 }\n"
    "}\n";
static const char kLayouts[] =
    "RecordLayoutDatabases[ 2 ] {\n"
    " RecordLayoutDatabase { Name \"All\" Desc \"everything\" DataComponents[ 12 ] { \"b\" \"i\" \"r\" \"n\" \"s\" \"ba\" \"by\" \"ia\" \"ra\" \"na\" \"sa\" \"s2\" } }\n"
    " RecordLayoutDatabase { Name \"Few\" Desc \"some\" DataComponents[ 2 ] { \"i\" \"s\" } }\n"
    "}\n";
static const char kAllRow[] = "1 42 3.5 \"abcdefgh\" \"hello\" 1 0 7 8 9 10 11 1.5 2.5 \"one\" \"two\" \"first\" \"second\" \"zz\" ";
static const char kAllRow2[] = "0 -3 90.25 \"x\" \"bye\" 0 1 1 2 3 -4 5 0.75 1.25 \"three\" \"four\" \"a\" \"b\" \"y\" ";
static std::string tables_text() {
    return std::string("DataRecordDatabases[ 2 ] {\n DataRecordDatabase { Name \"Everyone\" Desc \"all fields\" RecordLayout \"All\" DataRecords[ 2 ] { ") + kAllRow + kAllRow2 + "} }\n"
           " DataRecordDatabase { Name \"Some\" Desc \"a few\" RecordLayout \"Few\" DataRecords[ 3 ] { 1 \"p\" 2 \"qq\" 3 \"rrr\" } }\n}\n";
}
static std::string full_adb() { return std::string(kFields) + kLayouts + tables_text(); }

// ---- the manager, directly ---------------------------------------------------------------------------------------------
struct MgrPair { AtDatabaseManager* o; AtDatabaseManager* r; };

static void* alloc_filled(size_t n, unsigned char fill) { void* p = std::malloc(n); std::memset(p, fill, n); return p; }

static MgrPair new_managers(unsigned char fill) {
    const bool was = g_recording;
    MgrPair m;
    g_recording = false;
    void* a = alloc_filled(0x50, fill);
    at<Fn0>(0x000B1C90)(a, nullptr);
    void* b = alloc_filled(sizeof(AtDatabaseManager), fill);
    m.o = (AtDatabaseManager*)a;
    m.r = new (b) AtDatabaseManager;
    g_recording = was;
    return m;
}

static bool mgr_load(MgrPair m, const char* space_o, char* out_o, char* out_r, unsigned extra, int from_file) {
    (void)space_o;
    g_reports = 0; g_pushes = 0;
    const bool oc = guarded([&] { On on(0); at<FnMgrLoad>(0x000B2330)(m.o, nullptr, g_owner[0], "THING", 9, out_o, extra, from_file); });
    g_recording = false; g_side = 0; g_fail_alloc = false;
    g_reports = 0; g_pushes = 0;
    const bool rc = guarded([&] { On on(1); m.r->Load(g_owner[1], "THING", 9, out_r, extra, from_file); });
    g_recording = false; g_side = 0; g_fail_alloc = false;
    eq("crashed", oc, rc);
    return oc || rc;
}

static void same_list_head() {
    const AtDatabaseManager* o = (const AtDatabaseManager*)(uintptr_t)orig_u32(0x003ADE2C);
    eq("the list's head", o != nullptr, AtDatabaseManager::s_first != nullptr);
}

static void mgr_destroy(MgrPair m, const char* what) {
    const bool oc = guarded([&] { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x000B2460)(m.o, nullptr, 1), (uintptr_t)m.o); });
    g_recording = false; g_side = 0;
    const bool rc = guarded([&] { On on(1); delete m.r; });
    g_recording = false; g_side = 0;
    eq("crashed in the destructor", oc, rc);
    compare_logs(what);
    same_list_head();
}

// A file of text, loaded on each side and left to compare, with fill bytes both ways.
static void run_manager_text(const std::string& context, const std::string& adb, unsigned extra = 0, int from_file = 1, bool reload = false) {
    for (unsigned char fill : {(unsigned char)0xCD, (unsigned char)0}) {
        g_context = context + (fill ? " filled" : " zeroed");
        g_fs->adb = bytes(adb);
        set_path("DATA\\THING.ADB");
        g_fill = fill;
        make_owners();
        MgrPair m = new_managers(fill);
        char out[2][8] = {{'q', 0}, {'q', 0}};
        bool crashed = mgr_load(m, nullptr, out[0], out[1], extra, from_file);
        if (!crashed) {
            same_manager(m.o, m.r);
            same_owner(m.o, m.r);
            same_list_head();
            eqs("space out", nm(out[0]).c_str(), nm(out[1]).c_str());
        }
        compare_logs("load");
        if (crashed) continue;
        if (reload) {
            crashed = mgr_load(m, nullptr, out[0], out[1], extra, from_file);
            if (!crashed) { same_manager(m.o, m.r); same_list_head(); }
            compare_logs("load again");
            if (crashed) continue;
        }
        mgr_destroy(m, "destroy");
        reset_path();
    }
}

// A tokenizer on each side, on text of their own.
struct Tok {
    unsigned char o_file[sizeof(AtIoTokenFile)];
    AtIoTokenFile r_file;
    AtIoTokenizer* o;
    AtIoTokenizer* r;
    explicit Tok(const std::string& text) {
        g_fs->add = bytes(text);
        set_path("DATA\\THING.ADD");
        // The file classes log whether or not a side is recording; what came before is not this case's.
        g_log[0].n = g_log[1].n = 0;
        g_message[0][0] = g_message[1][0] = 0;
        at<Fn0>(0x00015460)(o_file, nullptr);
        { On on(0); at<FnOpen>(0x00015560)(o_file, nullptr, "T", "ADD", 2, nullptr); }
        { On on(1); r_file.Open("T", "ADD", 2, nullptr); }
        compare_logs("open");
        o = ((AtIoTokenFile*)o_file)->m_tokenizer;
        r = r_file.m_tokenizer;
        o->SetKeywords((const AtIoKeyword*)(uintptr_t)0x002B5310, 0x22, nullptr);
        r->SetKeywords(AtDatabaseManager::s_keywords, 0x22, nullptr);
    }
    ~Tok() {
        at<Fn0>(0x00015500)(o_file, nullptr);
        r_file.~AtIoTokenFile();
        new (&r_file) AtIoTokenFile;
        g_log[0].n = g_log[1].n = 0;
        g_message[0][0] = g_message[1][0] = 0;
    }
};

static void run_tables_words() {
    g_context = "keywords";
    const AtIoKeyword* table = (const AtIoKeyword*)(uintptr_t)0x002B47F0;
    for (int i = 0; i < AtDataLibrary::kKeywordCount; i++) {
        eq("keyword's id", table[i].m_id, AtDataLibrary::s_keywords[i].m_id);
        eqs("keyword's word", table[i].m_name, AtDataLibrary::s_keywords[i].m_name);
    }
    table = (const AtIoKeyword*)(uintptr_t)0x002B5310;
    for (int i = 0; i < AtDatabaseManager::kKeywordCount; i++) {
        eq("keyword's id", table[i].m_id, AtDatabaseManager::s_keywords[i].m_id);
        eqs("keyword's word", table[i].m_name, AtDatabaseManager::s_keywords[i].m_name);
    }
    g_context = "set heap";
    int other;
    at<FnSet>(0x0009B8D0)(&other); AtDataLibrary::SetHeap(&other);
    eq("library's heap", orig_u32(0x0030593C), (uint32_t)(uintptr_t)AtDataLibrary::s_heap);
    at<FnSet>(0x000B1C20)(&other); AtDatabaseManager::SetHeap(&other);
    eq("manager's heap", orig_u32(0x003ADE28), (uint32_t)(uintptr_t)AtDatabaseManager::s_heap);
    at<FnSet>(0x000B9AA0)(&other); AtRecordLayout::SetHeap(&other);
    eq("layout's heap", orig_u32(0x003ADE90), (uint32_t)(uintptr_t)AtRecordLayout::s_heap);
    at<FnSet>(0x000B9D90)(&other); AtDataRecordDatabase::SetHeap(&other);
    eq("table's heap", orig_u32(0x003ADE94), (uint32_t)(uintptr_t)AtDataRecordDatabase::s_heap);
    at<FnSet>(0x0009B8D0)(&g_heap_token); AtDataLibrary::SetHeap(&g_heap_token);
    at<FnSet>(0x000B1C20)(&g_heap_token); AtDatabaseManager::SetHeap(&g_heap_token);
    at<FnSet>(0x000B9AA0)(&g_heap_token); AtRecordLayout::SetHeap(&g_heap_token);
    at<FnSet>(0x000B9D90)(&g_heap_token); AtDataRecordDatabase::SetHeap(&g_heap_token);
}

static void run_owner() {
    g_context = "owner";
    make_owners();
    GmNameMap* maps[2][4];
    for (int side = 0; side < 2; side++) for (int i = 0; i < 4; i++) maps[side][i] = new GmNameMap;
    const char* spaces[4] = {nullptr, "", "Space", "Space"};
    for (int i = 0; i < 4; i++) {
        { On on(0); at<FnAdd>(0x000992D0)(g_owner[0], nullptr, maps[0][i], spaces[i]); }
        { On on(1); g_owner[1]->AddDatabase(maps[1][i], spaces[i]); }
        compare_logs("add");
        eq("in the plain group", maps[0][i]->m_group == &g_owner[0]->m_databases, maps[1][i]->m_group == &g_owner[1]->m_databases);
        eq("group", maps[0][i]->m_group != nullptr, maps[1][i]->m_group != nullptr);
        eqs("group's name", nm(maps[0][i]->m_group->m_name).c_str(), nm(maps[1][i]->m_group->m_name).c_str());
    }
}

// Reads one descriptor from text on each side.
static void run_descriptor(const std::string& text, unsigned char fill = 0xCD) {
    g_context = "descriptor: " + text;
    g_fill = fill;
    Tok t(text + " { { { { { { { {");
    void* a = alloc_filled(4 + 0x2C, 0);
    void* b = alloc_filled(4 + 0x2C, 0);
    AtDataDescriptor* d[2] = {(AtDataDescriptor*)((char*)a + 4), (AtDataDescriptor*)((char*)b + 4)};
    at<Fn0>(0x0006BC10)(d[0], nullptr);
    new (d[1]) AtDataDescriptor;
    for (int round = 0; round < 2; round++) {
        g_reports = 0;
        int ret[2] = {-99, -99};
        const bool oc = guarded([&] { On on(0); ret[0] = at<FnDescRead>(0x0006BDB0)(d[0], nullptr, t.o); });
        g_recording = false; g_side = 0;
        g_reports = 0;
        const bool rc = guarded([&] { On on(1); ret[1] = d[1]->Read(t.r); });
        g_recording = false; g_side = 0;
        eq("crashed", oc, rc);
        eq("returns", ret[0], ret[1]);
        same_descriptor(*d[0], *d[1]);
        compare_logs("descriptor");
        if (oc || rc) break;
    }
}

// The record's methods and the table's other slots, on two tables of a loaded manager.
static void run_table_methods() {
    g_context = "table's methods";
    g_fill = 0xCD;
    g_fs->adb = bytes(full_adb());
    set_path("DATA\\THING.ADB");
    make_owners();
    MgrPair m = new_managers(0xCD);
    char out[2][8] = {{0}, {0}};
    if (mgr_load(m, nullptr, out[0], out[1], 2, 1)) return;
    compare_logs("load");
    same_manager(m.o, m.r);
    for (unsigned t = 0; t < 2; t++) {
        AtDataRecordDatabase* a = m.o->m_tables[t];
        AtDataRecordDatabase* b = m.r->m_tables[t];
        eq("rows with two extra", a->m_count, b->m_count);
        for (unsigned i = 0; i < a->m_count; i++) {
            void* ar; { On on(0); ar = at<FnAt>(0x000B9E50)(a, nullptr, i); }
            AtDataRecord* br; { On on(1); br = b->RecordAt(i); }
            eq("record at", index_of(a->m_records, ar, sizeof(AtDataRecord)), index_of(b->m_records, br, sizeof(AtDataRecord)));
            for (unsigned c = 0; c < a->m_layout->m_count; c++) {
                { On on(0); eq("layout's field at", (uintptr_t)at<FnAt>(0x000B9AB0)(a->m_layout, nullptr, c) - (uintptr_t)m.o->m_descriptors, (uintptr_t)b->m_layout->FieldAt(c) - (uintptr_t)m.r->m_descriptors); }
                void* av = at<FnAt>(0x000BAAC0)(ar, nullptr, c);
                AtDataComponent* bv = br->FieldAt(c);
                const unsigned type = a->m_layout->m_fields[c]->m_type;
                eq("field at: row and column", (char*)av - (char*)a->m_columns[c], (char*)bv - (char*)b->m_columns[c]);
                eq("field's size", kSize[type], kHostSize[type]);
                eq("descriptor at", index_of(m.o->m_descriptors, at<FnAt>(0x000BABB0)(ar, nullptr, c), 0x2C), index_of(m.r->m_descriptors, br->DescriptorAt(c), 0x2C));
                char name[8];
                std::memcpy(name, a->m_layout->m_fields[c]->m_name, 8);
                void* an = at<FnFind>(0x000BAC60)(ar, nullptr, name);
                AtDataComponent* bn = br->FieldNamed(name);
                eq("field named", (char*)an - (char*)a->m_columns[c], (char*)bn - (char*)b->m_columns[c]);
            }
        }
        // By value: in every column but strings (slot 7 of every value class is recovered, at/datalookup). Strings are left out: their slot 7 compares
        // what slot 5 answers, which is null, so two texts of one length other than empty crash on both sides, and every real string column has them.
        static const char* const kFoundBy[12] = {"found by an empty value", "found by a boolean", "found by an integer", "found by a real", "found by a short name", "",
                                                 "found by a boolean array", "found by a byte array", "found by an integer array", "found by a real array",
                                                 "found by a short name array", "found by a string array"};
        for (unsigned c = 0; c < a->m_layout->m_count; c++) {
            const unsigned type = a->m_layout->m_fields[c]->m_type;
            if (type == 5 || type >= 12) continue;
            for (unsigned i = 0; i < a->m_count; i++) {
                void* av = at<FnAt>(0x000BAAC0)(&a->m_records[i], nullptr, c);
                AtDataComponent* bv = b->m_records[i].FieldAt(c);
                void* af; { On on(0); af = at<FnFindValue>(0x000BA9A0)(a, nullptr, c, av); }
                AtDataRecord* bf; { On on(1); bf = b->FindByValue(c, bv); }
                eq(kFoundBy[type], index_of(a->m_records, af, sizeof(AtDataRecord)), index_of(b->m_records, bf, sizeof(AtDataRecord)));
            }
        }
        eq("slot 7", at<FnSlot7>(0x000B15B0)(a, nullptr, 3), b->Unknown7(3));
        { On on(0); at<Fn0>(0x001CC9F0)(a, nullptr); }
        { On on(1); b->OnReady(); }
        // The rows no file gave, then none.
        for (int i = 0; i < 4; i++) {
            unsigned ai = 0x77, bi = 0x77;
            void* ar; { On on(0); ar = at<FnTake>(0x000BA960)(a, nullptr, &ai); }
            AtDataRecord* br; { On on(1); br = b->TakeUnused(&bi); }
            eq("took", index_of(a->m_records, ar, sizeof(AtDataRecord)), index_of(b->m_records, br, sizeof(AtDataRecord)));
            eq("took's index", ai, bi);
            if (ar && br) eq("took's flag", ((AtDataRecord*)ar)->m_inUse, br->m_inUse);
        }
    }
    // Sets over what it holds.
    {
        AtDataRecord* a = &m.o->m_tables[0]->m_records[0];
        AtDataRecord* b = &m.r->m_tables[0]->m_records[0];
        int layout, columns;
        for (int in_use : {1, 0}) {
            a->m_inUse = b->m_inUse = in_use;
            eq("set returns", at<FnRecordSet>(0x000BAA90)(a, nullptr, 5, &layout, &columns), b->Set(5, (AtRecordLayout*)&layout, (AtDataComponent**)&columns));
            eq("set: index", a->m_index, b->m_index);
            eq("set: layout", (uintptr_t)a->m_layout, (uintptr_t)b->m_layout);
            eq("set: columns", (uintptr_t)a->m_columns, (uintptr_t)b->m_columns);
            eq("set: in use", a->m_inUse, b->m_inUse);
            a->m_index = b->m_index = 0;
            a->m_layout = m.o->m_tables[0]->m_layout;
            b->m_layout = m.r->m_tables[0]->m_layout;
            a->m_columns = m.o->m_tables[0]->m_columns;
            b->m_columns = m.r->m_tables[0]->m_columns;
        }
    }
    // A table outside a type: one with a field of each kind no table holds has no field at all to ask for.
    {
        AtDataRecord* a = &m.o->m_tables[0]->m_records[0];
        AtDataRecord* b = &m.r->m_tables[0]->m_records[0];
        for (unsigned bad : {0u, 12u, 13u}) {
            const unsigned ot = a->m_layout->m_fields[0]->m_type, rt = b->m_layout->m_fields[0]->m_type;
            a->m_layout->m_fields[0]->m_type = bad;
            b->m_layout->m_fields[0]->m_type = bad;
            eq("field at, no such type", (uintptr_t)at<FnAt>(0x000BAAC0)(a, nullptr, 0), (uintptr_t)b->FieldAt(0));
            eq("descriptor at, no such type", (uintptr_t)at<FnAt>(0x000BABB0)(a, nullptr, 0), (uintptr_t)b->DescriptorAt(0));
            a->m_layout->m_fields[0]->m_type = ot;
            b->m_layout->m_fields[0]->m_type = rt;
        }
    }
    // With no match, the column past the last.
    {
        AtDataRecord* a = &m.o->m_tables[1]->m_records[0];
        AtDataRecord* b = &m.r->m_tables[1]->m_records[0];
        char nope[8] = {'n', 'o', 'p', 'e'};
        void* an = nullptr; AtDataComponent* bn = nullptr;
        const bool oc = guarded([&] { an = at<FnFind>(0x000BAC60)(a, nullptr, nope); });
        const bool rc = guarded([&] { bn = b->FieldNamed(nope); });
        eq("no match crashed", oc, rc);
        (void)an; (void)bn;
    }
    compare_logs("methods");
    mgr_destroy(m, "destroy");
    reset_path();
}

// The list of managers, by the fields, with its head set by hand: the constructor alone never makes a list of two.
static void run_list() {
    g_context = "list";
    g_fill = 0;
    g_fs->adb = bytes(full_adb());
    set_path("DATA\\THING.ADB");
    make_owners();
    MgrPair a = new_managers(0);
    same_list_head();
    MgrPair b = new_managers(0);
    same_list_head();
    eq("the newest is the head", orig_u32(0x003ADE2C) == (uint32_t)(uintptr_t)b.o, AtDatabaseManager::s_first == b.r);
    eq("a new one's next", b.o->m_next != nullptr, b.r->m_next != nullptr);
    eq("the first has no next", a.o->m_next != nullptr, a.r->m_next != nullptr);
    char out[2][8] = {{0}, {0}};
    if (mgr_load(a, nullptr, out[0], out[1], 0, 1)) return;
    compare_logs("load");
    // A map of no size divides by zero in the original's lookup, so each is loaded, with names of its own.
    const auto named_text = [](const char* layout, const char* table) {
        return std::string("DataComponentDescs[ 1 ] { DataComponentDesc { Name \"i\" Type Int } }\nRecordLayoutDatabases[ 1 ] { RecordLayoutDatabase { Name \"") + layout +
               "\" Desc \"d\" DataComponents[ 1 ] { \"i\" } } }\nDataRecordDatabases[ 1 ] { DataRecordDatabase { Name \"" + table + "\" Desc \"d\" RecordLayout \"" + layout + "\" DataRecords[ 1 ] { 4 } } }\n";
    };
    g_fs->adb = bytes(named_text("Bl", "Bt"));
    if (mgr_load(b, nullptr, out[0], out[1], 0, 1)) return;
    compare_logs("load b");
    MgrPair c = new_managers(0);
    g_fs->adb = bytes(named_text("Cl", "Ct"));
    if (mgr_load(c, nullptr, out[0], out[1], 0, 1)) return;
    compare_logs("load c");
    const auto chain = [&](MgrPair head, MgrPair next, bool end) {
        orig_u32(0x003ADE2C) = (uint32_t)(uintptr_t)head.o;
        AtDatabaseManager::s_first = head.r;
        head.o->m_next = next.o;
        head.r->m_next = next.r;
        next.o->m_next = nullptr;
        next.r->m_next = nullptr;
        (void)end;
    };
    // Found here; through the list, at its head or further on; nowhere.
    chain(c, a, true);
    const auto where = [](void* f, MgrPair x, MgrPair y, MgrPair z, int side, const char* name) -> int {
        if (!f) return 0;
        AtDatabaseManager* m[3] = {side ? x.r : x.o, side ? y.r : y.o, side ? z.r : z.o};
        for (int k = 0; k < 3; k++) if (m[k]->Find(name) == f) return k + 1;
        return 9;
    };
    for (const char* name : {"Everyone", "All", "Ct", "Bt", "Nothing"}) {
        void* f; { On on(0); f = at<FnFind>(0x000B2290)(b.o, nullptr, name); }
        void* g; { On on(1); g = b.r->FindEverywhere(name); }
        eq("found in", where(f, a, b, c, 0, name), where(g, a, b, c, 1, name));
    }
    compare_logs("find");
    const auto after = [&](MgrPair head, MgrPair next, const char* what) {
        eq(what, orig_u32(0x003ADE2C) == (uint32_t)(uintptr_t)head.o, AtDatabaseManager::s_first == head.r);
        eq(what, head.o->m_next == next.o, head.r->m_next == next.r);
    };
    // Clear of the second on the list: the first takes its next, and the list is then emptied.
    chain(c, a, true);
    c.o->m_next = b.o; c.r->m_next = b.r;
    b.o->m_next = a.o; b.r->m_next = a.r;
    a.o->m_next = nullptr; a.r->m_next = nullptr;
    { On on(0); at<Fn0>(0x000B1D30)(b.o, nullptr); }
    { On on(1); b.r->Clear(); }
    compare_logs("clear a middle one");
    after(c, a, "after clearing a middle one: head emptied");
    eq("middle's next", c.o->m_next == a.o, c.r->m_next == a.r);
    // Clear of one not on the list: the last takes its next.
    MgrPair d = new_managers(0);
    d.o->m_next = (AtDatabaseManager*)&g_heap_token; d.r->m_next = (AtDatabaseManager*)&g_heap_token;
    chain(c, a, true);
    { On on(0); at<Fn0>(0x000B1D30)(d.o, nullptr); }
    { On on(1); d.r->Clear(); }
    compare_logs("clear one not on the list");
    eq("the last's next", (uintptr_t)a.o->m_next, (uintptr_t)a.r->m_next);
    a.o->m_next = nullptr; a.r->m_next = nullptr;
    // Clear of the head: its next is the head for as long as Clear runs, then none; and with an empty list.
    chain(c, a, true);
    { On on(0); at<Fn0>(0x000B1D30)(c.o, nullptr); }
    { On on(1); c.r->Clear(); }
    compare_logs("clear the head");
    eq("list emptied", orig_u32(0x003ADE2C), (uint32_t)(uintptr_t)AtDatabaseManager::s_first);
    a.o->m_next = nullptr; a.r->m_next = nullptr;
    { On on(0); at<Fn0>(0x000B1D30)(c.o, nullptr); }
    { On on(1); c.r->Clear(); }
    compare_logs("clear with nothing");
    eq("scale after clearing", c.o->m_characterScale, c.r->m_characterScale);
    same_manager(c.o, c.r);
    // Keeps its counts across Clear.
    orig_u32(0x003ADE2C) = 0;
    AtDatabaseManager::s_first = nullptr;
    { On on(0); at<Fn0>(0x000B1D30)(a.o, nullptr); }
    { On on(1); a.r->Clear(); }
    compare_logs("clear what was loaded");
    same_manager(a.o, a.r);
    // The complete destructor without freeing.
    { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x000B2460)(a.o, nullptr, 0), (uintptr_t)a.o); }
    { On on(1); a.r->~AtDatabaseManager(); }
    compare_logs("destroyed in place");
    reset_path();
}

// A layout alone: read twice, read on a name not known, cleared.
static void run_layout() {
    g_context = "layout";
    for (unsigned char fill : {(unsigned char)0xCD, (unsigned char)0}) {
        g_fill = fill;
        Tok t("RecordLayoutDatabase { Name \"Lay\" Desc \"d1\" DataComponents[ 2 ] { \"a\" \"zz\" } }\n"
              "RecordLayoutDatabase { Name \"Lay2\" Desc \"d2\" DataComponents[ 1 ] { \"a\" } }\n"
              "Name { Name \"Bad\" Name \"d3\" Name[ 1 ] { \"a\" } }\n");
        GmNameMap known[2];
        char target[2][4];
        for (int side = 0; side < 2; side++) {
            known[side].Init(4);
            known[side].Insert("a", target[side]);
        }
        void* a = alloc_filled(0x2C, fill);
        void* b = alloc_filled(sizeof(AtRecordLayout), fill);
        at<Fn0>(0x000B9AC0)(a, nullptr);
        AtRecordLayout* o = (AtRecordLayout*)a;
        AtRecordLayout* r = new (b) AtRecordLayout;
        eq("constructed", std::memcmp((char*)o + 0x14 + 8, (char*)r + 0x14 + 8, 0x2C - 0x14 - 8), 0);
        for (int round = 0; round < 3; round++) {
            int ro = -9, rr = -9;
            { On on(0); ro = at<FnLayoutRead>(0x000B9B20)(o, nullptr, t.o, &known[0]); }
            { On on(1); rr = r->Read(t.r, &known[1]); }
            eq("returns", ro, rr);
            eqs("name", nm(o->m_name).c_str(), nm(r->m_name).c_str());
            eqs("description", o->m_description, r->m_description);
            eq("count", o->m_count, r->m_count);
            eq("loaded", o->m_loaded, r->m_loaded);
            same_map("map", o, {o->m_fields, 4}, r, {r->m_fields, 4});
            for (unsigned c = 0; c < o->m_count; c++) eq("field", o->m_fields[c] == nullptr ? 0 : o->m_fields[c] == (void*)target[0] ? 1 : 2, r->m_fields[c] == nullptr ? 0 : r->m_fields[c] == (void*)target[1] ? 1 : 2);
            compare_logs("read");
        }
        { On on(0); at<Fn0>(0x000B9AE0)(o, nullptr); }
        { On on(1); r->Clear(); }
        compare_logs("clear");
        eq("count", o->m_count, r->m_count);
        eq("description", o->m_description != nullptr, r->m_description != nullptr);
        eq("fields", o->m_fields != nullptr, r->m_fields != nullptr);
        eq("loaded", o->m_loaded, r->m_loaded);
        eq("size", o->m_size, r->m_size);
        { On on(0); at<Fn0>(0x000B9AE0)(o, nullptr); }
        { On on(1); r->Clear(); }
        compare_logs("clear nothing");
        // The slot 0 on one object: destroys and, with the low bit, frees.
        { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x000B1C30)(o, nullptr, 0), (uintptr_t)o); }
        { On on(1); r->~AtRecordLayout(); }
        compare_logs("destroy");
    }
}

// A table read on its own, twice over: its slot 2, and the slot 3 it makes.
static void run_table_alone() {
    g_context = "table alone";
    g_fill = 0xCD;
    g_fs->adb = bytes(std::string(kFields) + kLayouts + "DataRecordDatabases[ 0 ] { }\n");
    set_path("DATA\\THING.ADB");
    make_owners();
    MgrPair m = new_managers(0xCD);
    char out[2][8] = {{0}, {0}};
    // The manager's own map is only made with a table, so give it one by hand for the lookup.
    if (mgr_load(m, nullptr, out[0], out[1], 0, 1)) return;
    compare_logs("load");
    m.o->Init(4); m.r->Init(4);
    m.o->Insert("Few", &m.o->m_layouts[1]); m.r->Insert("Few", &m.r->m_layouts[1]);
    Tok t("Name \"T1\" Desc \"one\" RecordLayout \"Few\" DataRecords[ 2 ] { 1 \"p\" 2 \"qq\" } }\n"
          "Name \"T2\" Desc \"two\" RecordLayout \"Few\" DataRecords[ 1 ] { 5 \"z\" } }\n"
          "Name \"T3\" Desc \"three\" RecordLayout \"Few\" DataRecords[ 7 ] { \n");
    void* a = alloc_filled(0x44, 0xCD);
    void* b = alloc_filled(sizeof(AtDataRecordDatabase), 0xCD);
    at<Fn0>(0x000B9E60)(a, nullptr);
    AtDataRecordDatabase* o = (AtDataRecordDatabase*)a;
    AtDataRecordDatabase* r = new (b) AtDataRecordDatabase;
    eq("constructed: vftable", *(uint32_t*)o, 0x002855E0);
    eq("constructed", o->m_count, r->m_count);
    eq("constructed", o->m_unknown24, r->m_unknown24);
    eq("constructed", (uintptr_t)o->m_columns, (uintptr_t)r->m_columns);
    eq("constructed", o->m_unknown2C, r->m_unknown2C);
    eq("constructed", (uintptr_t)o->m_characters, (uintptr_t)r->m_characters);
    eq("constructed", o->m_characterSize, r->m_characterSize);
    eq("constructed", o->m_characterUsed, r->m_characterUsed);
    eq("left by the constructor", *(uint32_t*)&o->m_layout, *(uint32_t*)&r->m_layout);
    eq("left by the constructor", *(uint32_t*)&o->m_records, *(uint32_t*)&r->m_records);
    // Slot 2: says the layout first, and the read then asks by name anyway.
    int ra = -9, rb = -9;
    { On on(0); ra = at<FnTableLoad>(0x000B9DC0)(o, nullptr, m.o, &m.o->m_layouts[0], t.o, 0, 0, 1); }
    { On on(1); rb = r->Load(m.r, &m.r->m_layouts[0], t.r, 0, 0, 1); }
    eq("returns", ra, rb);
    same_table(m.o, m.r, o, r);
    compare_logs("load");
    // Again over what it has.
    { On on(0); ra = at<FnTableRead>(0x000B9F70)(o, nullptr, m.o, t.o, 0, 0, 1); }
    { On on(1); rb = r->Read(m.r, t.r, 0, 0, 1); }
    eq("returns", ra, rb);
    same_table(m.o, m.r, o, r);
    compare_logs("read again");
    // Rows from no file: the count discarded, the curlies unread.
    { On on(0); ra = at<FnTableRead>(0x000B9F70)(o, nullptr, m.o, t.o, 3, 0, 0); }
    { On on(1); rb = r->Read(m.r, t.r, 3, 0, 0); }
    same_table(m.o, m.r, o, r);
    compare_logs("no rows from the file");
    eq("what is next in the text", t.o->Next(), t.r->Next());
    // The reset slot alone, keeping what it would free out of its reach.
    struct Kept { unsigned count; AtDataComponent** columns; char* characters; unsigned size, used; };
    const Kept so = {o->m_count, o->m_columns, o->m_characters, o->m_characterSize, o->m_characterUsed};
    const Kept sr = {r->m_count, r->m_columns, r->m_characters, r->m_characterSize, r->m_characterUsed};
    { On on(0); at<Fn0>(0x000B9DA0)(o, nullptr); }
    { On on(1); r->Reset(); }
    eq("reset: rows", o->m_count, r->m_count);
    eq("reset: +24", o->m_unknown24, r->m_unknown24);
    eq("reset: columns", (uintptr_t)o->m_columns, (uintptr_t)r->m_columns);
    eq("reset: +2C", o->m_unknown2C, r->m_unknown2C);
    eq("reset: characters", (uintptr_t)o->m_characters, (uintptr_t)r->m_characters);
    eq("reset: size", o->m_characterSize, r->m_characterSize);
    eq("reset: used", o->m_characterUsed, r->m_characterUsed);
    o->m_count = so.count; o->m_columns = so.columns; o->m_characters = so.characters; o->m_characterSize = so.size; o->m_characterUsed = so.used;
    r->m_count = sr.count; r->m_columns = sr.columns; r->m_characters = sr.characters; r->m_characterSize = sr.size; r->m_characterUsed = sr.used;
    // Cleared: what it leaves, then the complete destructor on what it left, which crashes on both: m_columns is gone, and the layout is not.
    { On on(0); at<Fn0>(0x000B9E90)(o, nullptr); }
    { On on(1); r->Clear(); }
    compare_logs("clear");
    eq("cleared: rows", o->m_count, r->m_count);
    eq("cleared: columns", (uintptr_t)o->m_columns, (uintptr_t)r->m_columns);
    eq("cleared: characters", (uintptr_t)o->m_characters, (uintptr_t)r->m_characters);
    eq("cleared: size", o->m_characterSize, r->m_characterSize);
    eq("cleared: used", o->m_characterUsed, r->m_characterUsed);
    eq("cleared: records kept", o->m_records != nullptr, r->m_records != nullptr);
    eq("cleared: layout kept", o->m_layout != nullptr, r->m_layout != nullptr);
    const bool oc = guarded([&] { On on(0); at<FnDel>(0x000BAA50)(o, nullptr, 0); });
    g_recording = false; g_side = 0;
    const bool rc = guarded([&] { On on(1); r->~AtDataRecordDatabase(); });
    g_recording = false; g_side = 0;
    eq("destroying after a clear crashes", oc, rc);
    eq("destroying after a clear crashes, for this original", oc, 1);
    compare_logs("destroy after clear");
}

// Fields of no type a table can hold: the original crashes; here is a diagnostic. These cannot be compared past that point.
static void run_no_type() {
    for (const char* word : {"Type Relational", ""}) {
        g_context = std::string("no type: ") + word;
        const std::string adb = std::string("DataComponentDescs[ 1 ] { DataComponentDesc { Name \"x\" ") + word + " } }\n"
                                "RecordLayoutDatabases[ 1 ] { RecordLayoutDatabase { Name \"L\" Desc \"d\" DataComponents[ 1 ] { \"x\" } } }\n"
                                "DataRecordDatabases[ 1 ] { DataRecordDatabase { Name \"T\" Desc \"d\" RecordLayout \"L\" DataRecords[ 1 ] { } } }\n";
        g_fs->adb = bytes(adb);
        set_path("DATA\\THING.ADB");
        g_fill = 0xCD;
        make_owners();
        MgrPair m = new_managers(0xCD);
        char out[2][8] = {{0}, {0}};
        g_reports = 0; g_pushes = 0;
        const bool oc = guarded([&] { On on(0); at<FnMgrLoad>(0x000B2330)(m.o, nullptr, g_owner[0], "THING", 9, out[0], 0, 1); });
        g_recording = false; g_side = 0;
        g_reports = 0; g_pushes = 0;
        const bool rc = guarded([&] { On on(1); m.r->Load(g_owner[1], "THING", 9, out[1], 0, 1); });
        g_recording = false; g_side = 0;
        eq("the original crashes", oc, 1);
        eq("the recovered source reports", rc, 0);
        eq("the report", std::strstr(g_message[1], "a field of no type") != nullptr, 1);
        // Up to the original's crash, the two did the same: the recovered side's calls begin with the original's, and the next is its report. The original makes no call between its
        // last (the column list, at 0x000BA23E) and its crash in the rows, so neither may the recovered side: a column made for the field of no type would show here.
        eq("the calls up to the original's crash", g_log[0].n <= g_log[1].n && std::memcmp(g_log[0].v, g_log[1].v, g_log[0].n * sizeof(uint32_t)) == 0, 1);
        eq("the report comes next", g_log[0].n < g_log[1].n ? (long long)g_log[1].v[g_log[0].n] : -1, 'F');
        g_log[0].n = g_log[1].n = 0;
        g_message[0][0] = g_message[1][0] = 0;
    }
}

// Each word, name and number of a good file replaced in turn, curlies and brackets left. A tail of curlies ends any skip to a left curly; the report count ends the rest.
static void run_perturbed() {
    const std::string good = std::string("NameSpace \"sp\"\n") + kFields + kLayouts + tables_text();
    std::vector<std::pair<size_t, size_t>> tokens;
    size_t i = 0;
    while (i < good.size()) {
        const unsigned char c = good[i];
        if (c == ' ' || c == '\n' || c == '{' || c == '}' || c == '[' || c == ']') { i++; continue; }
        size_t e = i;
        if (c == '"') { e = good.find('"', i + 1) + 1; }
        else { while (e < good.size() && !std::strchr(" \n{}[]", good[e])) e++; }
        tokens.push_back({i, e - i});
        i = e;
    }
    std::string tail;
    for (int k = 0; k < 300; k++) tail += " {";
    unsigned step = 0;
    for (auto t : tokens) {
        const std::string was = good.substr(t.first, t.second);
        const std::string before = good.substr(t.first >= 5 ? t.first - 5 : 0, std::min<size_t>(5, t.first));
        // Type, or a word after it, gone leaves a column of no type, on which the two differ by design: run_no_type.
        if (before == "Type " || was == "Type") continue;
        for (const char* other : {"Database", "\"s\"", "7"}) {
            if (was == other) continue;
            // An enormous count from a stray number never arises: numbers here are small, and a string where a count goes reads as the last number.
            run_manager_text("perturbed " + was + " at " + std::to_string(t.first) + " to " + other, good.substr(0, t.first) + other + good.substr(t.first + t.second) + tail);
            step++;
        }
    }
    std::printf("perturbed texts: %u\n", step);
}

// Heap requests failing from each push on, and each heap unset.
static void run_failures() {
    const std::string add = "DataRegistry[ 2 ] { \"a\" \"b\" }\nDatabaseManager[ 1 ] { \"THING\" }\n";
    for (int push = 0; push < 90; push++) {
        g_context = "fails from push " + std::to_string(push);
        g_fail_at_push = push;
        run_library_text(add, full_adb(), 0xCD);
    }
    g_fail_at_push = -1;
    struct Heap { uint32_t va; void** recovered; } heaps[] = {
        {0x0030593C, &AtDataLibrary::s_heap}, {0x003ADE28, &AtDatabaseManager::s_heap}, {0x003ADE90, &AtRecordLayout::s_heap}, {0x003ADE94, &AtDataRecordDatabase::s_heap}, {0x002C60B0, &AtIoTokenFile::s_heap},
    };
    for (const Heap& h : heaps) {
        g_context = "no heap at " + std::to_string(h.va);
        void* was = *h.recovered;
        orig_u32(h.va) = 0;
        *h.recovered = nullptr;
        run_library_text(add, full_adb(), 0xCD);
        orig_u32(h.va) = (uint32_t)(uintptr_t)was;
        *h.recovered = was;
    }
    // A file that will not open: reported, then the original goes on to what it has not got.
    g_context = "no file";
    g_fs->open_status = 3;
    run_library_text(add, full_adb(), 0xCD);
    // And an .ADB that will not open, loaded on its own: the report, then the crash on the file not there.
    g_context = "no .ADB";
    g_fs->adb = bytes(full_adb());
    make_owners();
    MgrPair m = new_managers(0xCD);
    char out[2][8] = {{0}, {0}};
    g_fs->open_status = 3;
    mgr_load(m, nullptr, out[0], out[1], 0, 1);
    g_fs->open_status = 0;
    compare_logs("no .ADB");
}

static void run_library_written() {
    const std::string longest(255, 'x'), nearly(254, 'y');
    const std::vector<std::string> texts = {
        "",
        "// nothing\n",
        "DataRegistry[ 0 ] { }\n",
        "DatabaseManager[ 0 ] { }\n",
        "DataRegistry[ 1 ] { \"Design\\Registry\\One\" }\n",
        "DataRegistry[ 3 ] { \"a\" \"b\" \"c\" }\nDataRegistry[ 2 ] { \"d\" \"e\" }\n",
        "DatabaseManager[ 2 ] { \"THING\" \"OTHER\" }\n",
        "DatabaseManager[ 1 ] { \"THING\" }\nDatabaseManager[ 1 ] { \"OTHER\" }\n",
        "DatabaseManager[ 1 ] { \"THING\" }\nDataRegistry[ 1 ] { \"a\" }\n",
        "DataRegistry[ 1 ] { \"a\" }\nDatabaseManager[ 1 ] { \"THING\" }\n",
        "Nope\n",
        "\"text\" 12 { }\n",
        "DataRegistry[ 2 ] { \"a\" }\n",
        "DataRegistry[ 1 ] { 5 }\n",
        "DataRegistry[ 1 ] { \"a\" \"b\" }\n",
        "DataRegistry[ 65537 ] { \"a\" }\n",
        "DataRegistry[ 258 ] { \"a\" }\n",
        "DatabaseManager[ 65538 ] { \"THING\" \"OTHER\" }\n",
        "DataRegistry { \"a\" }\n",
        "DataRegistry[ 1 ] { \"" + nearly + "\" }\n",
    };
    for (const std::string& t : texts) {
        for (unsigned char fill : {(unsigned char)0xCD, (unsigned char)0}) {
            for (int parent = 0; parent < 2; parent++) {
                g_context = "library: [" + t + "]";
                LibRun run;
                run.with_parent = parent != 0;
                run.unused = parent ? 0x1234 : 0;
                run_library_text(t, full_adb(), fill, run);
            }
        }
    }
    // The longest string a file can give runs one past the buffer on both sides, so the heap has room here.
    g_heap_pad = 16;
    g_context = "library: longest name";
    run_library_text("DataRegistry[ 1 ] { \"" + longest + "\" }\n", full_adb(), 0xCD);
    g_heap_pad = 0;
    // Loaded twice over, with registries that stay loaded and that do not (their flag cleared by hand before each reload).
    for (int after : {1, 0}) {
        g_context = "library: loaded twice, registries loaded " + std::to_string(after);
        LibRun run;
        run.again = 2;
        run.unload_after = after == 0;
        run_library_text("DataRegistry[ 2 ] { \"a\" \"b\" }\nDatabaseManager[ 2 ] { \"THING\" \"OTHER\" }\n", full_adb(), 0xCD, run);
    }
    // The double this replaced could make a registry loaded from birth; a real one never is. Its case stays, on registries as they are.
    g_context = "library: one registry";
    run_library_text("DataRegistry[ 1 ] { \"a\" }\n", full_adb(), 0xCD);
    // Destroying one never loaded; a Clear with entries already gone or not loaded.
    {
        g_context = "library: never loaded";
        // The constructor forgets the names of any library before it.
        orig_u32(0x00305938) = orig_u32(0x00305934) = 0x1000;
        AtDataLibrary::s_registryNames = AtDataLibrary::s_databaseNames = (char**)(uintptr_t)0x1000;
        AtDataLibrary* o = new_o_library(0xCD);
        AtDataLibrary* r = new_r_library(0xCD);
        same_library(o, r, false);
        { On on(0); at<Fn0>(0x0009BD40)(o, nullptr); }
        { On on(1); r->~AtDataLibrary(); }
        compare_logs("destroy");
        for (int side = 0; side < 2; side++) (void)side;
        g_fs->add = bytes("DataRegistry[ 3 ] { \"a\" \"b\" \"c\" }\nDatabaseManager[ 3 ] { \"THING\" \"OTHER\" \"THIRD\" }\n");
        g_fs->adb = bytes(full_adb());
        set_path("DATA\\LIBRARY.ADD");
        make_owners();
        make_parents();
        o = new_o_library(0xCD);
        r = new_r_library(0xCD);
        bool crashed = false;
        LibRun run;
        lib_load(o, r, run, crashed);
        compare_logs("load");
        if (!crashed) {
            // One entry gone from each list, and one database not loaded.
            at<FnDel>(0x000B2460)(o->m_databases[0], nullptr, 1);
            delete r->m_databases[0];
            o->m_databases[0] = nullptr;
            r->m_databases[0] = nullptr;
            o->m_databases[1]->m_loaded = 0;
            r->m_databases[1]->m_loaded = 0;
            { On on(0); at<FnDel>(0x000687B0)(o->m_registries[0], nullptr, 1); }
            { On on(1); delete r->m_registries[0]; }
            o->m_registries[0] = nullptr;
            r->m_registries[0] = nullptr;
            o->m_registries[1]->m_loaded = 0;
            r->m_registries[1]->m_loaded = 0;
            // What a registry's Clear frees when it is not loaded, its block at +0x38 (set by code not recovered): Clear of the library must not ask it, and its destructor leaves it.
            o->m_registries[1]->m_unknown38 = std::calloc(1, 16);
            r->m_registries[1]->m_unknown38 = std::calloc(1, 16);
            g_log[0].n = g_log[1].n = 0;
            // Names are not freed by Clear: left in their places, then forgotten.
            orig_u32(0x00305938) = 0x1000;
            AtDataLibrary::s_registryNames = (char**)(uintptr_t)0x1000;
            { On on(0); at<Fn0>(0x0009BB40)(o, nullptr); }
            { On on(1); r->Clear(); }
            same_library(o, r, false);
            compare_logs("clear with gaps");
            // And a Clear of what is empty, which only sets fields.
            { On on(0); at<Fn0>(0x0009BB40)(o, nullptr); }
            { On on(1); r->Clear(); }
            same_library(o, r, false);
            compare_logs("clear of nothing");
            eq("destroying a cleared library", o->m_loaded, r->m_loaded);
            { On on(0); at<Fn0>(0x0009BD40)(o, nullptr); }
            { On on(1); r->~AtDataLibrary(); }
            compare_logs("destroy of a cleared library");
        }
    }
    // The two pieces on their own.
    {
        g_context = "library: making";
        for (int fail : {-1, 0, 1, 2, 3}) {
            AtDataLibrary* o = new_o_library(0xCD);
            AtDataLibrary* r = new_r_library(0xCD);
            o->m_registryCount = r->m_registryCount = 2;
            o->m_databaseCount = r->m_databaseCount = 2;
            g_fill = 0xCD;
            g_fail_at_push = fail;
            for (uint32_t va : {0x0009B8E0u, 0x0009BA00u}) {
                g_pushes = 0;
                const bool oc = guarded([&] { On on(0); at<Fn0>(va)(o, nullptr); });
                g_recording = false; g_side = 0; g_fail_alloc = false;
                g_pushes = 0;
                const bool rc = guarded([&] { On on(1); if (va == 0x0009B8E0u) r->MakeRegistries(); else r->MakeDatabases(); });
                g_recording = false; g_side = 0; g_fail_alloc = false;
                eq("crashed", oc, rc);
                compare_logs("making");
            }
            g_fail_at_push = -1;
        }
    }
    // Loading the entries on their own, with no names kept.
    {
        g_context = "library: entries with no names";
        AtDataLibrary* o = new_o_library(0xCD);
        AtDataLibrary* r = new_r_library(0xCD);
        { On on(0); at<FnEntries>(0x0009BC10)(o, nullptr, 1, nullptr); }
        { On on(1); r->LoadEntries(1, nullptr); }
        compare_logs("entries with no names");
    }
}

// ---- the disc ----------------------------------------------------------------------------------------------------------
static unsigned g_files, g_names, g_real_loaded, g_real_links;
static std::map<std::string, std::vector<unsigned char>> g_adr_written, g_adr_real;

// How far the real registries got: how many are loaded, and their links in use, on the recovered side; the two sides are compared besides.
static void count_real(const AtDataLibrary* r) {
    for (unsigned i = 0; r->m_registries && i < r->m_registryCount; i++) {
        const AtDataRegistry* g = r->m_registries[i];
        if (!g || !g->m_loaded) continue;
        g_real_loaded++;
        for (int p = 0; p < AtDataRegistry::kTypeCount; p++)
            for (const AtDataComponentPool* pool = &g->m_pools[p]; pool; pool = pool->m_next)
                for (unsigned k = 0; pool->m_links && k < pool->m_count; k++) g_real_links += pool->m_links[k].m_inUse != 0;
    }
}

static void run_real(const std::vector<unsigned char>& data, const char* leaf) {
    g_adr = g_adr_real;  // The eleven registries it names, and GENERIC.ADR as their parent, from the disc.
    for (unsigned char fill : {(unsigned char)0xCD, (unsigned char)0}) {
        g_context = std::string("real ") + leaf;
        LibRun run;
        run.again = 1;
        run_library_text(std::string(data.begin(), data.end()), "", fill, run);
    }
    // Through what start-up passes.
    g_context = std::string("real, as at start-up ") + leaf;
    g_fs->add = data;
    set_path("DataXbox\\LIBRARY.ADD");
    g_fill = 0xCD;
    make_owners();
    make_parents();
    AtDataLibrary* o = new_o_library(0xCD);
    AtDataLibrary* r = new_r_library(0xCD);
    LibRun run;
    run.unused = 0;
    bool crashed = false;
    const unsigned opened = g_adr_real_opened;
    lib_load(o, r, run, crashed);
    eq("crashed loading the real registries", crashed, 0);
    same_library(o, r, true);
    eq("the registries on the disc", o->m_registryCount, r->m_registryCount);
    g_names += r->m_registryCount;
    eq("no database on the disc", r->m_databaseCount, 0);
    eq("every registry named is a real file", g_adr_real_opened - opened, r->m_registryCount);
    compare_logs("real load");
    if (!crashed) count_real(r);
    g_adr = g_adr_written;
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
    if (pos.size() != 2) { std::printf("usage: oracle_add <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)s_new);
    redirect(0x00011210, (const void*)s_delete);
    redirect(0x00011240, (const void*)s_push);
    redirect(0x00011260, (const void*)s_pop);
    redirect(0x00011070, (const void*)s_fatal);
    redirect(0x00011170, (const void*)s_raise);
    // The host's own deletes, so that KeepFreed holds on this side too. /SECTION:.text,ERW and no incremental link make each address the function itself.
    redirect((uint32_t)(uintptr_t)static_cast<void (*)(void*)>(::operator delete), (const void*)host_delete);
    redirect((uint32_t)(uintptr_t)static_cast<void (*)(void*)>(::operator delete[]), (const void*)host_delete);
    redirect((uint32_t)(uintptr_t)static_cast<void (*)(void*, std::size_t)>(::operator delete), (const void*)host_delete_sized);
    redirect((uint32_t)(uintptr_t)static_cast<void (*)(void*, std::size_t)>(::operator delete[]), (const void*)host_delete_sized);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new Fs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C62E4u, 0x002CB09Cu, 0x002CB0A0u, 0x002CB0A4u, 0x002CB094u, 0x002CB098u, 0x002C9F9Cu}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    AtDataRegistry::SetHeap(&g_heap_token);
    AtDataRegistry::s_poolHeap = &g_heap_token;
    AtDataComponent::SetHeap(&g_heap_token);
    AtDataDescriptor::SetHeap(&g_heap_token);
    AtDataLink::SetHeap(&g_heap_token);
    AtDataRegistryNameSpaces::SetHeap(&g_heap_token);
    read_vfts();

    // The .ADR files: written ones for written libraries, every real one by its path for the disc's.
    g_adr_written["DESIGN\\REGISTRY\\GENERIC"] = std::vector<unsigned char>(kParentAdr, kParentAdr + sizeof kParentAdr - 1);
    {
        FILE* index = nullptr;
        if (fopen_s(&index, (std::string(pos[1]) + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
        char line[1024];
        while (std::fgets(line, sizeof line, index)) {
            if (!std::strstr(line, ".ADR\t")) continue;
            char* tab1 = std::strchr(line, '\t');
            char* tab2 = tab1 ? std::strchr(tab1 + 1, '\t') : nullptr;
            char* tab3 = tab2 ? std::strchr(tab2 + 1, '\t') : nullptr;
            if (!tab3) continue;
            *tab1 = *tab2 = *tab3 = 0;
            const std::string key = adr_key(tab2 + 1);
            if (g_adr_real.count(key) && !std::strstr(tab1 + 1, "AppInit")) continue;  // AppInit.JAM's, where LIBRARY.ADD is, first
            FILE* f = nullptr;
            if (fopen_s(&f, (std::string(pos[1]) + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
            std::fseek(f, 0, SEEK_END);
            std::vector<unsigned char> data((size_t)std::ftell(f));
            std::fseek(f, 0, SEEK_SET);
            if (!data.empty()) std::fread(data.data(), 1, data.size(), f);
            std::fclose(f);
            g_adr_real[key] = data;
        }
        std::fclose(index);
    }
    g_adr = g_adr_written;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_tables_words();
    run_owner();
    for (const char* t : {
             "DataComponentDesc { }",
             "DataComponentDesc { Name \"abc\" }",
             "DataComponentDesc { Name \"name that is long\" Type Int NumElements 4 Floor 1 Ceiling 2 FormatString \"f\" }",
             "DataComponentDesc { Name \"a\" Type Boolean Floor 3 Ceiling 4 }",
             "DataComponentDesc { Name \"a\" Type Real Floor 1.5 Ceiling 2.5 }",
             "DataComponentDesc { Name \"a\" Type RealArray Floor 0.25 Ceiling 7.75 NumElements 9 }",
             "DataComponentDesc { Name \"a\" Type ShortName } DataComponentDesc { Name \"b\" Type String }",
             "DataComponentDesc { Name \"a\" Type BooleanArray } DataComponentDesc { Name \"b\" Type ByteArray }",
             "DataComponentDesc { Name \"a\" Type IntArray } DataComponentDesc { Name \"b\" Type ShortNameArray }",
             "DataComponentDesc { Name \"a\" Type StringArray } DataComponentDesc { Name \"b\" Type Relational }",
             "DataComponentDesc { Type Byte }",
             "DataComponentDesc { Type Name }",
             "DataComponentDesc { Flags 3 }",
             "DataComponentDesc { Desc \"x\" }",
             "DataComponentDesc { Database }",
             "DataComponentDesc { Type }",
             "DataRecords 7 Name { Name \"skipped to here\" }",
             "DataComponentDesc { FormatString \"one\" } DataComponentDesc { FormatString \"two\" }",
             "DataComponentDesc { Floor 1.5 Ceiling 2.5 }",
         }) {
        run_descriptor(t);
        run_descriptor(t, 0);
    }
    run_layout();
    run_table_alone();
    run_table_methods();
    run_list();
    run_no_type();
    run_manager_text("full", full_adb());
    run_manager_text("full, read again", full_adb(), 0, 1, true);
    run_manager_text("two rows extra", full_adb(), 2);
    run_manager_text("from no file", full_adb(), 0, 0);
    run_manager_text("from no file, two rows extra", full_adb(), 2, 0);
    run_manager_text("name space", std::string("NameSpace \"sp\"\n") + full_adb());
    run_manager_text("no lists", "DataComponentDescs[ 0 ] { }\nRecordLayoutDatabases[ 0 ] { }\nDataRecordDatabases[ 0 ] { }\n");
    run_manager_text("no descriptors", "DataComponentDescs[ 0 ] { } RecordLayoutDatabases[ 1 ] { RecordLayoutDatabase { Name \"L\" Desc \"d\" DataComponents[ 0 ] { } } } DataRecordDatabases[ 0 ] { }\n");
    run_manager_text("no rows", std::string(kFields) + kLayouts + "DataRecordDatabases[ 1 ] { DataRecordDatabase { Name \"T\" Desc \"d\" RecordLayout \"Few\" DataRecords[ 1 ] { 1 \"p\" } } }\n", 0, 1, true);
    run_manager_text("a layout no descriptor knows", std::string(kFields) + "RecordLayoutDatabases[ 1 ] { RecordLayoutDatabase { Name \"L\" Desc \"d\" DataComponents[ 1 ] { \"none\" } } } DataRecordDatabases[ 1 ] { DataRecordDatabase { Name \"T\" Desc \"d\" RecordLayout \"L\" DataRecords[ 1 ] { } } }\n");
    run_manager_text("a table on no layout", std::string(kFields) + kLayouts + "DataRecordDatabases[ 1 ] { DataRecordDatabase { Name \"T\" Desc \"d\" RecordLayout \"none\" DataRecords[ 1 ] { } } }\n");
    run_manager_text("a table on a table's name", std::string(kFields) + kLayouts + "DataRecordDatabases[ 2 ] { DataRecordDatabase { Name \"T\" Desc \"d\" RecordLayout \"Few\" DataRecords[ 1 ] { 1 \"p\" } } DataRecordDatabase { Name \"U\" Desc \"d\" RecordLayout \"Few\" DataRecords[ 1 ] { 2 \"q\" } } }\n");
    // Each count of descriptions from 1 to 40, so that the map's size crosses each of the first sizes in its table.
    for (int n = 1; n <= 40; n++) {
        std::string text = "DataComponentDescs[ " + std::to_string(n) + " ] {\n";
        for (int i = 0; i < n; i++) text += " DataComponentDesc { Name \"d" + std::to_string(i) + "\" Type Int }\n";
        text += "}\nRecordLayoutDatabases[ 0 ] { }\nDataRecordDatabases[ 0 ] { }\n";
        run_manager_text("descriptions: " + std::to_string(n), text);
    }
    run_library_written();
    run_failures();
    run_perturbed();

    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".ADD\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::vector<unsigned char> data((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        if (!data.empty()) std::fread(data.data(), 1, data.size(), f);
        std::fclose(f);
        run_real(data, line);
        g_files++;
    }
    std::fclose(index);
    if (g_files == 0) eq("no real file", 0, 1);
    report_entries();
    std::printf("%s: %u real files, %u registries named, %u real .ADR registries loaded under the real GENERIC.ADR with %u links in use; %u .ADR files opened, %u links compared; %llu comparisons, %d differences\n",
                g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_files, g_names, g_real_loaded, g_real_links, g_adr_opened, g_links_compared, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
