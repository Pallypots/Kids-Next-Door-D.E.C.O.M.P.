// Differential oracle for the input mappers, .ANM (Windows x86, local only). See docs/verification.md.
//
//   oracle_ana <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: every .ANM file listed in index.txt, loaded through the file code on both sides; text written here, through a tokenizer of the harness's whose Error can stop a reader that would otherwise never
// finish; and the methods called directly. Binding is not recovered, so the links of a router are bound here by hand, the same on both sides, for the unbinding code to undo. Controllers are this file's fakes.
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include <string>
#include <vector>

#include "at/AtInputMapper.h"
#include "at/AtIoTokenizer.h"
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

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;
static int g_device_token;

typedef void(__fastcall* Fn0)(void*, void*);
typedef void*(__fastcall* FnCtor)(void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__fastcall* FnDbRead)(void*, void*, void*, void*);
typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);
typedef void(__fastcall* FnAdd)(void*, void*, void*, const char*);
typedef void(__fastcall* FnTok)(void*, void*, void*);
typedef void(__fastcall* FnSizes)(void*, void*, const void*);
typedef void(__fastcall* FnCopy)(void*, void*, const void*);
typedef void(__fastcall* FnRouterAlloc)(void*, void*, void*, unsigned, unsigned);
typedef void(__fastcall* FnCount)(void*, void*, unsigned);
typedef void(__fastcall* FnP1)(void*, void*, void*);
typedef void(__fastcall* FnP2)(void*, void*, void*, void*);
typedef void(__fastcall* FnUnbind)(void*, void*, void*, int);
typedef int(__fastcall* FnLink)(void*, void*, void*);

// ---- comparing --------------------------------------------------------------------------------------------------------

static long long link_index(const AtInputRouter* router, const AtInputLink* link) {
    if (!link) return -1;
    const unsigned buttons = router->m_boundButtons.m_capacity, axes = router->m_boundAxes.m_capacity;
    if (router->m_buttonLinks && link >= router->m_buttonLinks && link < router->m_buttonLinks + buttons) return link - router->m_buttonLinks;
    if (router->m_axisLinks && link >= router->m_axisLinks && link < router->m_axisLinks + axes) return 0x1000 + (link - router->m_axisLinks);
    return -2;
}

static void same_links(const AtInputRouter* o, const AtInputRouter* r, const AtInputLink* a, const AtInputLink* b, unsigned count) {
    eq("links", a != nullptr, b != nullptr);
    if (!a || !b) return;
    for (unsigned i = 0; i < count; i++) {
        eq("link's next", link_index(o, a[i].m_next), link_index(r, b[i].m_next));
        eq("link's prev", link_index(o, a[i].m_prev), link_index(r, b[i].m_prev));
        eq("link's id", a[i].m_id, b[i].m_id);
        eq("link's 0C", a[i].m_unknown0C, b[i].m_unknown0C);
        eq("link's 10", a[i].m_unknown10, b[i].m_unknown10);
        eq("link's 18", a[i].m_unknown18, b[i].m_unknown18);
    }
}

static void same_list(const AtInputRouter* o, const AtInputRouter* r, const AtInputLinkList& a, const AtInputLinkList& b) {
    eq("list's capacity", a.m_capacity, b.m_capacity);
    eq("list's count", a.m_count, b.m_count);
    eq("list's room", a.m_items != nullptr, b.m_items != nullptr);
    if (!a.m_items || !b.m_items || a.m_capacity != b.m_capacity) return;
    for (unsigned i = 0; i < a.m_capacity; i++) eq("list's item", link_index(o, a.m_items[i]), link_index(r, b.m_items[i]));
}

static void same_router(const AtInputRouter* o, const AtInputRouter* r) {
    eq("router's device", (uintptr_t)o->m_device, (uintptr_t)r->m_device);
    eq("router's function", (uintptr_t)o->m_bindFunction, (uintptr_t)r->m_bindFunction);
    eq("router's chain", o->m_passOnTo != nullptr, r->m_passOnTo != nullptr);
    eq("router's count", o->m_linkCount, r->m_linkCount);
    eq("router's 44", o->m_unknown44, r->m_unknown44);
    same_list(o, r, o->m_boundButtons, r->m_boundButtons);
    same_list(o, r, o->m_boundAxes, r->m_boundAxes);
    // After a release the free list's pointers are stale: only that they are held can be compared.
    eq("free first held", o->m_freeFirst != nullptr, r->m_freeFirst != nullptr);
    eq("free last held", o->m_freeLast != nullptr, r->m_freeLast != nullptr);
    eq("button links", o->m_buttonLinks != nullptr, r->m_buttonLinks != nullptr);
    eq("axis links", o->m_axisLinks != nullptr, r->m_axisLinks != nullptr);
    if (o->m_buttonLinks || o->m_axisLinks) {
        eq("free first", link_index(o, o->m_freeFirst), link_index(r, r->m_freeFirst));
        eq("free last", link_index(o, o->m_freeLast), link_index(r, r->m_freeLast));
    }
    if (o->m_boundButtons.m_capacity == r->m_boundButtons.m_capacity && o->m_buttonLinks) same_links(o, r, o->m_buttonLinks, r->m_buttonLinks, o->m_boundButtons.m_capacity);
    if (o->m_boundAxes.m_capacity == r->m_boundAxes.m_capacity && o->m_axisLinks) same_links(o, r, o->m_axisLinks, r->m_axisLinks, o->m_boundAxes.m_capacity);
}

static void same_mapper(const AtInputMapper* o, const AtInputMapper* r) {
    eq("device", (uintptr_t)o->m_device, (uintptr_t)r->m_device);
    eq("button capacity", o->m_buttonMapCapacity, r->m_buttonMapCapacity);
    eq("axis capacity", o->m_axisMapCapacity, r->m_axisMapCapacity);
    eq("buttons used", o->m_buttonMapsUsed, r->m_buttonMapsUsed);
    eq("axes used", o->m_axisMapsUsed, r->m_axisMapsUsed);
    eq("button maps", o->m_buttonMaps != nullptr, r->m_buttonMaps != nullptr);
    eq("axis maps", o->m_axisMaps != nullptr, r->m_axisMaps != nullptr);
    if (o->m_buttonMaps && r->m_buttonMaps && o->m_buttonMapCapacity == r->m_buttonMapCapacity) {
        eq("button map bytes", std::memcmp(o->m_buttonMaps, r->m_buttonMaps, o->m_buttonMapCapacity * sizeof(AtInputButtonMap)), 0);
    }
    if (o->m_axisMaps && r->m_axisMaps && o->m_axisMapCapacity == r->m_axisMapCapacity) {
        eq("axis map bytes", std::memcmp(o->m_axisMaps, r->m_axisMaps, o->m_axisMapCapacity * sizeof(AtInputAxisMap)), 0);
    }
    same_router(&o->m_router, &r->m_router);
}

static std::string group_name(GmNameMapGroup* g) {
    if (!g) return "(none)";
    return std::string(g->m_name, strnlen(g->m_name, 8));
}

static unsigned g_mappers, g_button_maps, g_axis_maps;

static void same_db(const char* what, const AtInputMapperDatabase* o, const AtInputMapperDatabase* r, AtInputOwner* oo, AtInputOwner* ro) {
    g_recording = false;
    eq("owner kept", o->m_owner == oo, r->m_owner == ro);
    eq("owner null", o->m_owner == nullptr, r->m_owner == nullptr);
    eq("1C", o->m_unknown1C, r->m_unknown1C);
    eq("items", o->m_items != nullptr, r->m_items != nullptr);
    eq("size", o->m_size, r->m_size);
    eq("in a group", o->m_group != nullptr, r->m_group != nullptr);
    eq("in the plain group", o->m_group == &oo->m_mappers, r->m_group == &ro->m_mappers);
    eqs("group's name", group_name(o->m_group).c_str(), group_name(r->m_group).c_str());
    eq("entries", o->m_entries != nullptr, r->m_entries != nullptr);
    if (o->m_items && r->m_items) {
        const unsigned count = ((unsigned*)o->m_items)[-1];
        eq("cookie", count, ((unsigned*)r->m_items)[-1]);
        if (count == ((unsigned*)r->m_items)[-1]) {
            for (unsigned i = 0; i < count; i++) {
                eq("mapper's vftable", *(uint32_t*)&o->m_items[i], 0x00277BC0);
                same_mapper(&o->m_items[i], &r->m_items[i]);
            }
            if (o->m_entries && r->m_entries && o->m_size == r->m_size) {
                for (unsigned i = 0; i < o->m_size; i++) {
                    const GmNameMap::Entry& x = o->m_entries[i];
                    const GmNameMap::Entry& y = r->m_entries[i];
                    eq("slot used", x.m_value != nullptr, y.m_value != nullptr);
                    if (x.m_value && y.m_value) {
                        eq("slot's name", std::strncmp(x.m_name, y.m_name, 8), 0);
                        eq("slot's mapper", (AtInputMapper*)x.m_value - o->m_items, (AtInputMapper*)y.m_value - r->m_items);
                    }
                }
            }
        }
    }
    compare_logs(what);
}

// ---- the sides --------------------------------------------------------------------------------------------------------

struct Owners {
    AtInputOwner* side[2];
    explicit Owners(void* device = &g_device_token) {
        for (int i = 0; i < 2; i++) {
            side[i] = new AtInputOwner;
            side[i]->m_unknown00 = 0;
            side[i]->m_device = device;
            side[i]->m_mapperSpaces.Init(8);
            side[i]->m_controllerSpaces.Init(2);
        }
    }
    ~Owners() {
        g_recording = false;
        delete side[0];
        delete side[1];
    }
};

static AtInputMapperDatabase* make_o(unsigned char fill) {
    unsigned char* o = (unsigned char*)std::malloc(sizeof(AtInputMapperDatabase));
    std::memset(o, fill, sizeof(AtInputMapperDatabase));
    eq("constructor returns this", (uintptr_t)at<FnCtor>(0x000737F0)(o, nullptr), (uintptr_t)o);
    eq("database vftable", *(uint32_t*)o, 0x00277D4C);
    return (AtInputMapperDatabase*)o;
}
static AtInputMapperDatabase* make_r(unsigned char fill) {
    unsigned char* r = (unsigned char*)std::malloc(sizeof(AtInputMapperDatabase));
    std::memset(r, fill, sizeof(AtInputMapperDatabase));
    return new (r) AtInputMapperDatabase;
}
static void destroy(AtInputMapperDatabase* o, AtInputMapperDatabase* r, const char* what, int flags) {
    { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x00073B70)(o, nullptr, flags), (uintptr_t)o); }
    { On on(1); if (flags & 1) { delete r; } else { r->~AtInputMapperDatabase(); } }
    compare_logs(what);
    if (!(flags & 1)) { std::free(o); std::free(r); }
}

// ---- through the file code ------------------------------------------------------------------------------------------------

static unsigned g_files;

static void note_counts(const AtInputMapperDatabase* r) {
    if (!r->m_items) return;
    const unsigned count = ((unsigned*)r->m_items)[-1];
    g_mappers += count;
    for (unsigned i = 0; i < count; i++) {
        g_button_maps += r->m_items[i].m_buttonMapsUsed;
        g_axis_maps += r->m_items[i].m_axisMapsUsed;
    }
}

static void run_file(const std::string& data, const std::string& context, unsigned char fill) {
    g_context = context;
    Owners own;
    g_fs->files[0].data = g_fs->files[1].data = (const unsigned char*)data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    g_fill = fill;
    AtInputMapperDatabase* o = make_o(fill);
    AtInputMapperDatabase* r = make_r(fill);
    eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(AtInputMapperDatabase) - 4), 0);
    for (int pass = 0; pass < 2; pass++) {
        { On on(0); at<FnLoad>(0x00073AD0)(o, nullptr, own.side[0], "DESIGN\\THING", 77); }
        { On on(1); r->Load(own.side[1], "DESIGN\\THING", 77); }
        same_db("load", o, r, own.side[0], own.side[1]);
    }
    if (fill == 0xCD) note_counts(r);  // Each file runs on two fills, and counts once.
    o->m_unknown1C = r->m_unknown1C = 7;  // Nothing in the code sets it, so it is set here, to see it cleared.
    { On on(0); ((AtInputMapperDatabase*)o)->Clear(); ((AtInputMapperDatabase*)o)->Clear(); }
    { On on(1); r->Clear(); r->Clear(); }
    same_db("clear", o, r, own.side[0], own.side[1]);
    g_recording = false;
    GmNameSpaceDb::Remove(o);
    GmNameSpaceDb::Remove(r);
    destroy(o, r, "destroy", 0);
}

// ---- written text, through a tokenizer that can stop --------------------------------------------------------------------------

class MemoryStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};

class DirectTokenizer : public AtIoTextTokenizer {
public:
    int m_errors = 0;
    void Error(int code) override {
        g_log[g_side].add('E', code);
        if (++m_errors > 12) *(volatile int*)8 = 0;  // Does not come back, as in the game. guarded() takes it from here.
    }
};
struct Direct {
    MemoryStream stream;
    DirectTokenizer tokenizer;
    AtIoTextTokenizer codes;
    std::string text;
    explicit Direct(const std::string& t) : text(t) {
        stream.m_base = (char*)text.data();
        stream.m_cur = stream.m_base;
        stream.m_limit = stream.m_base + text.size();
        stream.m_size = (unsigned)text.size();
        tokenizer.SetStream(&stream);
        codes.SetKeywords(AtInputMapperDatabase::s_inputCodes, AtInputMapperDatabase::kInputCodeCount, nullptr);
        tokenizer.SetKeywords(AtInputMapperDatabase::s_keywords, AtInputMapperDatabase::kKeywordCount, &codes);
    }
    ~Direct() {
        stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

static void run_text(const std::string& text, unsigned char fill) {
    g_context = "text [" + text + "]";
    Owners own;
    g_fill = fill;
    AtInputMapperDatabase* o = make_o(fill);
    AtInputMapperDatabase* r = make_r(fill);
    bool crashed = false;
    for (int pass = 0; pass < 2 && !crashed; pass++) {
        Direct da(text), db(text);
        const bool oc = guarded([&] { On on(0); at<FnDbRead>(0x00073810)(o, nullptr, own.side[0], &da.tokenizer); });
        g_recording = false;
        g_side = 0;
        const bool rc = guarded([&] { On on(1); r->Read(own.side[1], &db.tokenizer); });
        g_recording = false;
        g_side = 0;
        eq("crash", oc, rc);
        crashed = oc || rc;
        same_db("read", o, r, own.side[0], own.side[1]);
        eq("left at", da.stream.m_cur - da.stream.m_base, db.stream.m_cur - db.stream.m_base);
    }
    if (crashed) return;  // What was half made stays: a crash here has no way back in the game.
    note_counts(r);
    g_fs->files[0].data = g_fs->files[1].data = nullptr;
    GmNameSpaceDb::Remove(o);
    GmNameSpaceDb::Remove(r);
    destroy(o, r, "destroy", 0);
}

static std::string mapper(const std::string& body) { return "AbstractMapper[1]\n{\nAbstractMapper \"A\"\n{\n" + body + "\n}\n}\n"; }
static std::string button(const std::string& body) { return mapper("ButtonMap[1]\n{\nButtonMap\n{\n" + body + "\n}\n}\n"); }
static std::string axis(const std::string& body) { return mapper("AxisMap[1]\n{\nAxisMap\n{\n" + body + "\n}\n}\n"); }

static const char* const kIds = "PhysicalID JoystickButton1 AbstractID JoystickButton2 ";

static void run_written() {
    std::vector<std::string> texts = {
        "AbstractMapper[0]\n{\n}\n",
        "AbstractMapper[1]\n{\nAbstractMapper \"A\"\n{\n}\n}\n",
        "NameSpace \"Space\"\nAbstractMapper[1]\n{\nAbstractMapper \"A\"\n{\n}\n}\n",
        "NameSpace \"\"\nAbstractMapper[1]\n{\nAbstractMapper \"A\"\n{\n}\n}\n",
        "NameSpace \"Space\"\nButtonMap[1]\n{\n}\n",
        "ButtonMap[1]\n{\n}\n",
        "AbstractMapper[1]\n{\nButtonMap \"A\"\n{\n}\n}\n",
        // Not here: a mapper with no string for its name. ExpectName then leaves the name's stack bytes as they were, which differ between the sides, so the slot it hashes to cannot be compared.
        "AbstractMapper[2]\n{\nAbstractMapper \"LongerThanEight\"\n{\n}\nAbstractMapper \"LongerThanEightToo\"\n{\n}\n}\n",
        "AbstractMapper[2]\n{\nAbstractMapper \"A\"\n{\n}\nAbstractMapper \"A\"\n{\n}\n}\n",
        "AbstractMapper[1]\n{\nAbstractMapper \"A\"\n{\n}\n}\nExtra",
        "AbstractMapper[1]\n{\nAbstractMapper \"A\"\n{\n}\n",
        "AbstractMapper[1]\n{\nAbstractMapper \"A\"\n}\n",
        "AbstractMapper[1]\n",
        "",
        mapper("Bogus"),
        mapper("PhysicalID"),
        mapper("ButtonMap[0]\n{\n}\n"),
        mapper("AxisMap[0]\n{\n}\n"),
        mapper("ButtonMap[0]\n{\n}\nAxisMap[1]\n{\nAxisMap\n{\n}\n}\n"),
        mapper("AxisMap[0]\n{\n}\nButtonMap[1]\n{\nButtonMap\n{\n}\n}\n"),
        mapper("ButtonMap[2]\n{\nButtonMap\n{\nPhysicalID JoystickButton1 AbstractID JoystickButton2\n}\nButtonMap\n{\nPhysicalID KeyA AbstractID AxisV\n}\n}\nAxisMap[3]\n{\nAxisMap\n{\nPhysicalID AxisX AbstractID AxisY\n}\nAxisMap\n{\nPhysicalID AxisZ AbstractID JoystickButton3 AbstractPOVAxis AxisY\n}\nAxisMap\n{\nPhysicalID POV AbstractID AxisU PhysicalPOVAxis AxisX\n}\n}\n"),
        mapper("AxisMap[1]\n{\nAxisMap\n{\nPhysicalID AxisX AbstractID AxisY\n}\n}\nButtonMap[1]\n{\nButtonMap\n{\nPhysicalID JoystickButton1 AbstractID JoystickButton2\n}\n}\n"),
        mapper("ButtonMap[2]\n{\nButtonMap\n{\nPhysicalID JoystickButton1 AbstractID JoystickButton2\n}\n}\n"),
        mapper("ButtonMap[1]\n{\nButtonMap\n{\nPhysicalID JoystickButton1 AbstractID JoystickButton2\n}\nButtonMap\n{\n}\n}\n"),
        mapper("ButtonMap[1]\n{\nAxisMap\n{\nPhysicalID JoystickButton1 AbstractID JoystickButton2\n}\n}\n"),
        mapper("AxisMap[1]\n{\nButtonMap\n{\nPhysicalID JoystickButton1 AbstractID JoystickButton2\n}\n}\n"),
        mapper("AxisMap[2]\n{\nAxisMap\n{\nPhysicalID AxisX AbstractID AxisY\n}\n}\n"),
        mapper("AxisMap[1]\n{\nAxisMap\n{\nPhysicalID AxisX AbstractID AxisY\n}\nAxisMap\n{\n}\n}\n"),
        button(""),
        button("PhysicalID JoystickButton1"),
        button("AbstractID JoystickButton1"),
        button("PhysicalID ButtonMap AbstractID JoystickButton1"),
        button("PhysicalID JoystickButton1 AbstractID ButtonMap"),
        button("PhysicalID 7 AbstractID JoystickButton1"),
        button("PhysicalID KeyEscape AbstractID MouseLeftButton"),
        button("PhysicalID Axis15 AbstractID POV"),
        button(std::string(kIds) + "PhysicalPOVAxis AxisX"),
        button(std::string(kIds) + "AbstractPOVAxis AxisX"),
        button(std::string(kIds) + "ButtonMap"),
        button(std::string(kIds) + "Bogus"),
        axis(""),
        axis("PhysicalID AxisX"),
        axis("AbstractID AxisX"),
        axis("PhysicalID ButtonMap AbstractID AxisX"),
        axis("PhysicalID AxisX AbstractID ButtonMap"),
        axis("PhysicalID AxisX AbstractID AxisY Bogus"),
        axis("PhysicalID AxisX AbstractID AxisY PhysicalButtonState AxisPositive"),
        axis("PhysicalID AxisX AbstractID AxisY AbstractAxisState AxisNegative"),
        axis("PhysicalID AxisX AbstractID AxisY AbstractPOVState POVUp"),
        axis("PhysicalID AxisX AbstractID AxisY PhysicalPOVAxis AxisX"),
        axis("PhysicalID AxisX AbstractID AxisY PhysicalPOVAxis AxisY"),
        axis("PhysicalID AxisX AbstractID AxisY PhysicalPOVAxis AxisZ"),
        axis("PhysicalID AxisX AbstractID AxisY AbstractPOVAxis AxisX"),
        axis("PhysicalID AxisX AbstractID AxisY AbstractPOVAxis AxisY"),
        axis("PhysicalID AxisX AbstractID AxisY AbstractPOVAxis AxisZ"),
        axis("PhysicalID AxisX AbstractID AxisY AbstractPOVAxis AxisY PhysicalPOVAxis AxisX"),
        // A copy of a mapper before it, with a name space or without.
        "AbstractMapper[2]\n{\nAbstractMapper \"A\"\n{\nButtonMap[1]\n{\nButtonMap\n{\nPhysicalID JoystickButton1 AbstractID JoystickButton2\n}\n}\nAxisMap[1]\n{\nAxisMap\n{\nPhysicalID AxisX AbstractID AxisY\n}\n}\n}\nAbstractMapper \"B\"\n{\nAbstractMapper \"A\" \"\"\n}\n}\n",
        "NameSpace \"Space\"\nAbstractMapper[2]\n{\nAbstractMapper \"A\"\n{\nButtonMap[1]\n{\nButtonMap\n{\nPhysicalID JoystickButton1 AbstractID JoystickButton2\n}\n}\nAxisMap[1]\n{\nAxisMap\n{\nPhysicalID AxisX AbstractID AxisY\n}\n}\n}\nAbstractMapper \"B\"\n{\nAbstractMapper \"A\" \"Space\"\n}\n}\n",
        "AbstractMapper[2]\n{\nAbstractMapper \"A\"\n{\nButtonMap[1]\n{\nButtonMap\n{\nPhysicalID JoystickButton1 AbstractID JoystickButton2\n}\n}\n}\nAbstractMapper \"B\"\n{\nAbstractMapper \"A\" \"\"\nButtonMap\n}\n}\n",
        "AbstractMapper[2]\n{\nAbstractMapper \"A\"\n{\nAxisMap[1]\n{\nAxisMap\n{\nPhysicalID AxisX AbstractID AxisY\n}\n}\n}\nAbstractMapper \"B\"\n{\nButtonMap[1]\n{\nButtonMap\n{\nPhysicalID JoystickButton4 AbstractID JoystickButton5\n}\n}\nAbstractMapper \"A\" \"\"\n}\n}\n",
        "AbstractMapper[1]\n{\nAbstractMapper \"B\"\n{\nAbstractMapper \"Nobody\" \"\"\n}\n}\n",
        "AbstractMapper[1]\n{\nAbstractMapper \"B\"\n{\nAbstractMapper \"Nobody\" \"Space\"\n}\n}\n",
        "AbstractMapper[1]\n{\nAbstractMapper \"B\"\n{\nAbstractMapper 5 \"\"\n}\n}\n",
    };
    for (const char* word : {"AxisPositive", "AxisNegative", "POVUp", "POVUpRight", "POVRight", "POVDownRight", "POVDown", "POVDownLeft", "POVLeft", "POVUpLeft", "ButtonUp", "ButtonDown", "AxisX", "NameSpace", "7", "\"text\"", "]"}) {
        texts.push_back(button(std::string(kIds) + "PhysicalButtonState " + word));
        texts.push_back(button(std::string(kIds) + "AbstractAxisState " + word));
        texts.push_back(button(std::string(kIds) + "AbstractPOVState " + word));
        texts.push_back(button(std::string(kIds) + "AbstractAxisState AxisNegative AbstractPOVState " + word));
    }
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        for (const std::string& t : texts) run_text(t, fill);
    }
}

// Memory runs out: a raise, then what the readers do with null.
static void run_no_room() {
    for (const std::string& text : {mapper("ButtonMap[1]\n{\nButtonMap\n{\nPhysicalID JoystickButton1 AbstractID JoystickButton2\n}\n}\n"), mapper("AxisMap[1]\n{\nAxisMap\n{\nPhysicalID AxisX AbstractID AxisY\n}\n}\n"), std::string("AbstractMapper[0]\n{\n}\n")}) {
        g_context = "no room [" + text + "]";
        Owners own;
        AtInputMapperDatabase* o = make_o(0xCD);
        AtInputMapperDatabase* r = make_r(0xCD);
        Direct da(text), db(text);
        g_fail_alloc = true;
        const bool oc = guarded([&] { On on(0); at<FnDbRead>(0x00073810)(o, nullptr, own.side[0], &da.tokenizer); });
        g_recording = false;
        g_side = 0;
        const bool rc = guarded([&] { On on(1); r->Read(own.side[1], &db.tokenizer); });
        g_recording = false;
        g_side = 0;
        g_fail_alloc = false;
        eq("crash", oc, rc);
        compare_logs("no room");
    }
}

static void set_mapper_heaps(void* mapper, void* db, void* list, void* router) {
    at<void(__cdecl*)(void*)>(0x00072B70)(mapper);
    AtInputMapper::SetHeap(mapper);
    eq("mapper heap", (uintptr_t)*(void**)(uintptr_t)0x002CF14C, (uintptr_t)AtInputMapper::s_heap);
    at<void(__cdecl*)(void*)>(0x00073780)(db);
    AtInputMapperDatabase::SetHeap(db);
    eq("database heap", (uintptr_t)*(void**)(uintptr_t)0x002CF154, (uintptr_t)AtInputMapperDatabase::s_heap);
    at<void(__cdecl*)(void*)>(0x00078940)(list);
    AtInputLinkList::SetHeap(list);
    eq("list heap", (uintptr_t)*(void**)(uintptr_t)0x00301B00, (uintptr_t)AtInputLinkList::s_heap);
    at<void(__cdecl*)(void*)>(0x00078B50)(router);
    AtInputRouter::SetHeap(router);
    eq("link heap", (uintptr_t)*(void**)(uintptr_t)0x00301B04, (uintptr_t)AtInputRouter::s_heap);
}

static void run_no_heap() {
    int other[4];
    set_mapper_heaps(&other[0], &other[1], &other[2], &other[3]);
    run_text(mapper("ButtonMap[1]\n{\nButtonMap\n{\nPhysicalID JoystickButton1 AbstractID JoystickButton2\n}\n}\nAxisMap[1]\n{\nAxisMap\n{\nPhysicalID AxisX AbstractID AxisY\n}\n}\n"), 0xCD);
    void* one[4] = {&other[0], &other[1], &other[2], &other[3]};
    for (int missing = 0; missing < 4; missing++) {
        void* h[4] = {one[0], one[1], one[2], one[3]};
        h[missing] = nullptr;
        set_mapper_heaps(h[0], h[1], h[2], h[3]);
        // Each heap's raise. The list and links are asked for only after the maps, and an empty mapper asks only for the database's.
        run_text("AbstractMapper[1]\n{\nAbstractMapper \"A\"\n{\nButtonMap[1]\n{\nButtonMap\n{\nPhysicalID JoystickButton1 AbstractID JoystickButton2\n}\n}\nAxisMap[1]\n{\nAxisMap\n{\nPhysicalID AxisX AbstractID AxisY\n}\n}\n}\n}\n", 0xCD);
        // The heaps of a copy of a mapper: the cleared kind.
        run_text("AbstractMapper[2]\n{\nAbstractMapper \"A\"\n{\nButtonMap[1]\n{\nButtonMap\n{\nPhysicalID JoystickButton1 AbstractID JoystickButton2\n}\n}\nAxisMap[1]\n{\nAxisMap\n{\nPhysicalID AxisX AbstractID AxisY\n}\n}\n}\nAbstractMapper \"B\"\n{\nAbstractMapper \"A\" \"\"\n}\n}\n", 0xCD);
    }
    set_mapper_heaps(&g_heap_token, &g_heap_token, &g_heap_token, &g_heap_token);
}

// ---- a router, bound by hand ------------------------------------------------------------------------------------------

// What a router's slots pass on to: the same object on both sides, noting them for the side at work.
class Next : public AtInputRouter {
public:
    void Slot0(void* a) override { g_log[g_side].add('0', (uint32_t)(uintptr_t)a); }
    void Slot1(void* a) override { g_log[g_side].add('1', (uint32_t)(uintptr_t)a); }
    void Slot2(void* a) override { g_log[g_side].add('2', (uint32_t)(uintptr_t)a); }
    void Slot3(void* a, void* b) override { g_log[g_side].add('3', (uint32_t)(uintptr_t)a); g_log[g_side].add('3', (uint32_t)(uintptr_t)b); }
    void Unbind(AtInputController* c, int id) override { g_log[g_side].add('4', c != nullptr); g_log[g_side].add('4', id); }
};

struct Fake {
    AtInputController c;
    AtInputController::Input buttons[4], axes[4];
    AtInputController::Chain buttonChains[4], axisChains[4];
    Fake() {
        std::memset((void*)this, 0, sizeof *this);
        c.m_buttonInputs = buttons;
        c.m_axisInputs = axes;
        c.m_buttonChains = buttonChains;
        c.m_axisChains = axisChains;
        c.m_buttonInputCount = 4;
        c.m_axisInputCount = 4;
        for (int i = 0; i < 4; i++) {
            buttons[i].m_id = 0x30000000 + i;
            axes[i].m_id = 0x40000000 + i;
        }
    }
};

static AtInputLink* take_free(AtInputRouter* r) {
    AtInputLink* l = r->m_freeFirst;
    r->m_freeFirst = l->m_next;
    if (r->m_freeFirst) r->m_freeFirst->m_prev = nullptr;
    else r->m_freeLast = nullptr;
    l->m_next = l->m_prev = nullptr;
    return l;
}

static void chain_on(AtInputController::Chain& chain, AtInputLink* l) {
    l->m_next = chain.m_first;
    if (chain.m_first) chain.m_first->m_prev = l;
    else chain.m_last = l;
    chain.m_first = l;
    l->m_prev = nullptr;
}

static void bind_button(AtInputRouter* r, Fake& f, int id, int chain) {
    AtInputLink* l = take_free(r);
    l->m_id = id;
    l->m_controller = &f.c;
    chain_on(f.buttonChains[chain], l);
    r->m_boundButtons.m_items[r->m_boundButtons.m_count++] = l;
}

static void bind_axis(AtInputRouter* r, Fake& f, int id, int chain) {
    AtInputLink* l = take_free(r);
    l->m_id = id;
    l->m_controller = &f.c;
    chain_on(f.axisChains[chain], l);
    r->m_boundAxes.m_items[r->m_boundAxes.m_count++] = l;
}

static long long index_or_stray(const AtInputRouter* r, const AtInputLink* l) { return link_index(r, l); }

static void same_fakes(const AtInputRouter* a, const AtInputRouter* b, const Fake& x, const Fake& y) {
    for (int i = 0; i < 4; i++) {
        eq("chain's first", index_or_stray(a, x.buttonChains[i].m_first), index_or_stray(b, y.buttonChains[i].m_first));
        eq("chain's last", index_or_stray(a, x.buttonChains[i].m_last), index_or_stray(b, y.buttonChains[i].m_last));
        eq("axis chain's first", index_or_stray(a, x.axisChains[i].m_first), index_or_stray(b, y.axisChains[i].m_first));
        eq("axis chain's last", index_or_stray(a, x.axisChains[i].m_last), index_or_stray(b, y.axisChains[i].m_last));
    }
}

static unsigned char g_router_o[sizeof(AtInputRouter)], g_router_r[sizeof(AtInputRouter)];

// A router a side, with links bound here: several on one chain, so a link has a middle and two ends, and two to a chain of the second kind.
static void bound_routers(AtInputRouter*& a, AtInputRouter*& b, Fake& x, Fake& y) {
    std::memset(g_router_o, 0xCD, sizeof g_router_o);
    std::memset(g_router_r, 0xCD, sizeof g_router_r);
    { On on(0); at<FnCtor>(0x00078BD0)(g_router_o, nullptr); }
    { On on(1); new (g_router_r) AtInputRouter; }
    a = (AtInputRouter*)g_router_o;
    b = (AtInputRouter*)g_router_r;
    eq("router's vftable", *(uint32_t*)a, 0x002785FC);
    a->m_bindFunction = b->m_bindFunction = &g_device_token;  // For Allocate's release to clear.
    { On on(0); at<FnRouterAlloc>(0x00079350)(a, nullptr, &g_device_token, 8, 6); }
    { On on(1); b->Allocate(&g_device_token, 8, 6); }
    same_router(a, b);
    compare_logs("router allocated");
    int side = 0;
    for (AtInputRouter* r : {a, b}) {
        Fake& f = side++ ? y : x;
        bind_button(r, f, 0x30000001, 1);
        bind_button(r, f, 0x30000001, 1);
        bind_button(r, f, 0x30000001, 1);
        bind_button(r, f, 0x30000003, 3);
        bind_button(r, f, 0x40000002, 2);
        bind_button(r, f, 0x30000000, 0);
        bind_axis(r, f, 0x40000001, 1);
        bind_axis(r, f, 0x40000001, 1);
        bind_axis(r, f, 0x40000003, 3);
    }
    same_router(a, b);
    same_fakes(a, b, x, y);
    compare_logs("bound");
}

static void finish_routers(AtInputRouter* a, AtInputRouter* b, const char* what) {
    { On on(0); at<Fn0>(0x000792F0)(a, nullptr); }
    { On on(1); b->~AtInputRouter(); }
    same_router(a, b);
    compare_logs(what);
}

static void run_router() {
    g_fill = 0xCD;
    g_context = "router: all unbound";
    {
        Fake x, y;
        AtInputRouter* a;
        AtInputRouter* b;
        bound_routers(a, b, x, y);
        { On on(0); at<Fn0>(0x00078DC0)(a, nullptr); }
        { On on(1); b->UnbindAll(); }
        same_router(a, b);
        same_fakes(a, b, x, y);
        compare_logs("unbind all");
        finish_routers(a, b, "destroy");
    }
    g_context = "router: bound when destroyed";
    {
        Fake x, y;
        AtInputRouter* a;
        AtInputRouter* b;
        bound_routers(a, b, x, y);
        a->m_bindFunction = b->m_bindFunction = &g_device_token;
        { On on(0); at<Fn0>(0x000792A0)(a, nullptr); }
        { On on(1); b->Release(); }
        same_router(a, b);
        same_fakes(a, b, x, y);  // With the links gone, a chain still holding one shows as a stray.
        compare_logs("released");
        finish_routers(a, b, "destroy");
    }
    // Each pair: buttons alone, both lists, a controller with none, an id with none, and then with and without a router to pass on to.
    const int ids[] = {0x30000001, 0x30000003, 0x30000000, 0x30000002, 0x40000002, 0x40000001, 0x40000003, 0x40000000, 0x50000000};
    for (const int id : ids) {
        for (int next = 0; next < 2; next++) {
            g_context = "router: unbind " + std::to_string(id);
            Fake x, y, other;
            AtInputRouter* a;
            AtInputRouter* b;
            bound_routers(a, b, x, y);
            static Next chain;
            if (next) a->m_passOnTo = b->m_passOnTo = &chain;
            { On on(0); at<FnUnbind>(0x000791B0)(a, nullptr, &other.c, id); }
            { On on(1); b->Unbind(&other.c, id); }
            same_router(a, b);
            same_fakes(a, b, x, y);
            compare_logs("unbind, another controller");
            { On on(0); at<FnUnbind>(0x000791B0)(a, nullptr, &x.c, id); }
            { On on(1); b->Unbind(&y.c, id); }
            same_router(a, b);
            same_fakes(a, b, x, y);
            compare_logs("unbind");
            if (next) {
                int token;
                { On on(0); at<FnP1>(0x00078B60)(a, nullptr, &token); at<FnP1>(0x00078B70)(a, nullptr, &token); at<FnP1>(0x00078B90)(a, nullptr, &token); at<FnP2>(0x00078BB0)(a, nullptr, &token, &x); }
                { On on(1); b->Slot0(&token); b->Slot1(&token); b->Slot2(&token); b->Slot3(&token, &x); }
                compare_logs("passed on");
            } else {
                int token;
                { On on(0); at<FnP1>(0x00078B60)(a, nullptr, &token); at<FnP1>(0x00078B70)(a, nullptr, &token); at<FnP1>(0x00078B90)(a, nullptr, &token); at<FnP2>(0x00078BB0)(a, nullptr, &token, &x); }
                { On on(1); b->Slot0(&token); b->Slot1(&token); b->Slot2(&token); b->Slot3(&token, &x); }
                compare_logs("nothing to pass on to");
            }
            finish_routers(a, b, "destroy");
        }
    }

    // Only the oldest link of a chain unbinds, the others being another controller's: it is the last onto the free list, and its prev is still set when it gets there.
    g_context = "router: a link with a neighbour";
    {
        Fake x, y, ox, oy;
        AtInputRouter* a;
        AtInputRouter* b;
        bound_routers(a, b, x, y);
        for (int i = 1; i < 3; i++) {
            a->m_boundButtons.m_items[i]->m_controller = &ox.c;
            b->m_boundButtons.m_items[i]->m_controller = &oy.c;
        }
        { On on(0); at<FnUnbind>(0x000791B0)(a, nullptr, &x.c, 0x30000001); }
        { On on(1); b->Unbind(&y.c, 0x30000001); }
        same_router(a, b);
        same_fakes(a, b, x, y);
        compare_logs("unbound with a neighbour");
        finish_routers(a, b, "destroy");
    }

    // A link's id that the controller does not know: nothing is unchained, in either kind of chain.
    g_context = "router: id not the controller's";
    {
        Fake x, y;
        AtInputRouter* a;
        AtInputRouter* b;
        bound_routers(a, b, x, y);
        for (AtInputRouter* r : {a, b}) {
            r->m_boundButtons.m_items[1]->m_id = 0x12345;
            r->m_boundAxes.m_items[1]->m_id = 0x45000000;
            r->m_boundAxes.m_items[0]->m_id = 0x12345;  // No bit 30: the second list is not looked at for it.
        }
        { On on(0); at<FnUnbind>(0x000791B0)(a, nullptr, &x.c, 0x12345); at<FnUnbind>(0x000791B0)(a, nullptr, &x.c, 0x45000000); }
        { On on(1); b->Unbind(&y.c, 0x12345); b->Unbind(&y.c, 0x45000000); }
        same_router(a, b);
        same_fakes(a, b, x, y);
        compare_logs("not known");
        finish_routers(a, b, "destroy");
    }

    // Unbinding by itself, for what it returns, on every position in a chain.
    g_context = "controller";
    for (int kind = 0; kind < 2; kind++) {
        for (int which = 0; which < 3; which++) {
            AtInputLink links[2][3];
            Fake f[2];
            for (int side = 0; side < 2; side++) {
                std::memset(links[side], 0, sizeof links[side]);
                AtInputController::Chain& chain = kind ? f[side].axisChains[2] : f[side].buttonChains[2];
                for (int i = 0; i < 3; i++) {
                    links[side][i].m_id = (kind ? 0x40000000 : 0x30000000) + 2;
                    chain_on(chain, &links[side][i]);
                }
            }
            const int o = at<FnLink>(kind ? 0x000779A0 : 0x00077870)(&f[0].c, nullptr, &links[0][which]);
            const int r = kind ? f[1].c.UnbindAxis(&links[1][which]) : f[1].c.UnbindButton(&links[1][which]);
            eq("returned", o, r);
            for (int i = 0; i < 3; i++) {
                eq("next", links[0][i].m_next ? links[0][i].m_next - links[0] : -1, links[1][i].m_next ? links[1][i].m_next - links[1] : -1);
                eq("prev", links[0][i].m_prev ? links[0][i].m_prev - links[0] : -1, links[1][i].m_prev ? links[1][i].m_prev - links[1] : -1);
            }
            const AtInputController::Chain& ca = kind ? f[0].axisChains[2] : f[0].buttonChains[2];
            const AtInputController::Chain& cb = kind ? f[1].axisChains[2] : f[1].buttonChains[2];
            eq("first", ca.m_first ? ca.m_first - links[0] : -1, cb.m_first ? cb.m_first - links[1] : -1);
            eq("last", ca.m_last ? ca.m_last - links[0] : -1, cb.m_last ? cb.m_last - links[1] : -1);
        }
        // A chain with one link: both ends go.
        AtInputLink one[2];
        Fake f[2];
        for (int side = 0; side < 2; side++) {
            std::memset(&one[side], 0, sizeof one[side]);
            one[side].m_id = (kind ? 0x40000000 : 0x30000000) + 3;
            chain_on(kind ? f[side].axisChains[3] : f[side].buttonChains[3], &one[side]);
        }
        const int o = at<FnLink>(kind ? 0x000779A0 : 0x00077870)(&f[0].c, nullptr, &one[0]);
        const int r = kind ? f[1].c.UnbindAxis(&one[1]) : f[1].c.UnbindButton(&one[1]);
        eq("returned", o, r);
        const AtInputController::Chain& ca = kind ? f[0].axisChains[3] : f[0].buttonChains[3];
        const AtInputController::Chain& cb = kind ? f[1].axisChains[3] : f[1].buttonChains[3];
        eq("first gone", ca.m_first != nullptr, cb.m_first != nullptr);
        eq("last gone", ca.m_last != nullptr, cb.m_last != nullptr);
        // No inputs, and one it does not have.
        one[0].m_id = one[1].m_id = 0x777;
        eq("unknown", at<FnLink>(kind ? 0x000779A0 : 0x00077870)(&f[0].c, nullptr, &one[0]), kind ? f[1].c.UnbindAxis(&one[1]) : f[1].c.UnbindButton(&one[1]));
        f[0].c.m_buttonInputCount = f[1].c.m_buttonInputCount = 0;
        f[0].c.m_axisInputCount = f[1].c.m_axisInputCount = 0;
        eq("no inputs", at<FnLink>(kind ? 0x000779A0 : 0x00077870)(&f[0].c, nullptr, &one[0]), kind ? f[1].c.UnbindAxis(&one[1]) : f[1].c.UnbindButton(&one[1]));
    }

    // No room for the links: a raise, then the clearing crashes on null.
    g_context = "router: no room";
    for (int kind = 0; kind < 2; kind++) {
        std::memset(g_router_o, 0xCD, sizeof g_router_o);
        std::memset(g_router_r, 0xCD, sizeof g_router_r);
        at<FnCtor>(0x00078BD0)(g_router_o, nullptr);
        new (g_router_r) AtInputRouter;
        g_fail_alloc = true;
        const bool oc = guarded([&] { On on(0); at<FnCount>(kind ? 0x00078D00 : 0x00078C40)(g_router_o, nullptr, 2); });
        g_recording = false;
        g_side = 0;
        const bool rc = guarded([&] { On on(1); AtInputRouter* r = (AtInputRouter*)g_router_r; if (kind) r->AllocateAxisLinks(2); else r->AllocateButtonLinks(2); });
        g_recording = false;
        g_side = 0;
        g_fail_alloc = false;
        eq("crash", oc, rc);
        compare_logs("no room for links");
    }

    // The list by itself.
    g_context = "list";
    {
        unsigned char ol[sizeof(AtInputLinkList)], rl[sizeof(AtInputLinkList)];
        std::memset(ol, 0xCD, sizeof ol);
        std::memset(rl, 0xCD, sizeof rl);
        at<FnCtor>(0x000789F0)(ol, nullptr);
        AtInputLinkList* a = (AtInputLinkList*)ol;
        AtInputLinkList* b = new (rl) AtInputLinkList;
        eq("list's vftable", *(uint32_t*)a, 0x002785F8);
        AtInputLink links[6];
        const auto same = [&](const char* what) {
            eq("capacity", a->m_capacity, b->m_capacity);
            eq("count", a->m_count, b->m_count);
            eq("room", a->m_items != nullptr, b->m_items != nullptr);
            if (a->m_items && b->m_items) eq("items", std::memcmp(a->m_items, b->m_items, a->m_capacity * 4), 0);
            compare_logs(what);
        };
        for (int round = 0; round < 2; round++) {
            a->m_count = b->m_count = 3;  // For Allocate to reset.
            { On on(0); at<FnCount>(0x00078A40)(a, nullptr, 6); }
            { On on(1); b->Allocate(6); }
            same("allocated");
            for (int i = 0; i < 5; i++) {
                a->m_items[i] = b->m_items[i] = &links[i];
            }
            a->m_count = b->m_count = 5;
            same("filled");
            for (const int i : {2, 0, 4, 5, 1, 3}) {
                eq("removed", at<FnLink>(0x00078980)(a, nullptr, &links[i]), b->Remove(&links[i]));
                same("removed");
            }
        }
        // A link just past the count is found, and the count goes down for it.
        a->m_items[0] = b->m_items[0] = &links[0];
        a->m_items[1] = b->m_items[1] = &links[1];
        a->m_items[2] = b->m_items[2] = &links[2];
        a->m_count = b->m_count = 2;
        eq("removed from past the count", at<FnLink>(0x00078980)(a, nullptr, &links[2]), b->Remove(&links[2]));
        same("removed from past the count");
        a->m_count = b->m_count = 2;  // For Clear to reset.
        { On on(0); at<Fn0>(0x00078950)(a, nullptr); at<Fn0>(0x00078950)(a, nullptr); }
        { On on(1); b->Clear(); b->Clear(); }
        same("cleared");
        { On on(0); at<FnCount>(0x00078A40)(a, nullptr, 2); }
        { On on(1); b->Allocate(2); }
        same("again");
        a->m_count = b->m_count = 1;  // For the destructor to reset.
        { On on(0); at<Fn0>(0x00078A10)(a, nullptr); }
        { On on(1); b->~AtInputLinkList(); }
        same("destroyed");
        // No room: a raise, then the clearing crashes on null.
        at<FnCtor>(0x000789F0)(ol, nullptr);
        new (rl) AtInputLinkList;
        g_fail_alloc = true;
        const bool oc = guarded([&] { On on(0); at<FnCount>(0x00078A40)(a, nullptr, 2); });
        g_recording = false;
        g_side = 0;
        const bool rc = guarded([&] { On on(1); b->Allocate(2); });
        g_recording = false;
        g_side = 0;
        g_fail_alloc = false;
        eq("crash", oc, rc);
        compare_logs("no room for a list");
        // The deleting form, from the heap.
        void* p = std::malloc(sizeof(AtInputLinkList));
        void* q = new AtInputLinkList;
        at<FnCtor>(0x000789F0)(p, nullptr);
        { On on(0); eq("deleting returns this", (uintptr_t)at<FnDel>(0x00078B00)(p, nullptr, 1), (uintptr_t)p); }
        { On on(1); delete (AtInputLinkList*)q; }
        compare_logs("deleted");
        void* s = std::malloc(sizeof(AtInputLinkList));
        at<FnCtor>(0x000789F0)(s, nullptr);
        { On on(0); eq("deleting with no free returns this", (uintptr_t)at<FnDel>(0x00078B00)(s, nullptr, 0), (uintptr_t)s); }
        std::free(s);
        compare_logs("not freed");
        g_log[0].n = g_log[1].n = 0;
    }
}

// ---- a mapper, called directly -----------------------------------------------------------------------------------------

static unsigned char g_mapper_o[sizeof(AtInputMapper)], g_mapper_r[sizeof(AtInputMapper)];

static void run_mapper() {
    g_fill = 0xCD;
    g_context = "mapper";
    const auto make = [&](AtInputMapper*& a, AtInputMapper*& b, unsigned char fill) {
        std::memset(g_mapper_o, fill, sizeof g_mapper_o);
        std::memset(g_mapper_r, fill, sizeof g_mapper_r);
        { On on(0); eq("mapper ctor returns this", (uintptr_t)at<FnCtor>(0x00072B90)(g_mapper_o, nullptr), (uintptr_t)g_mapper_o); }
        { On on(1); new (g_mapper_r) AtInputMapper; }
        a = (AtInputMapper*)g_mapper_o;
        b = (AtInputMapper*)g_mapper_r;
        eq("mapper's vftable", *(uint32_t*)a, 0x00277BC0);
        eq("constructed mapper", std::memcmp(g_mapper_o + 0x10, g_mapper_r + 0x10, 0x18), 0);
        eq("constructed mapper's end", std::memcmp(g_mapper_o + 0x4C, g_mapper_r + 0x4C, 0x1C), 0);
    };
    AtInputMapper* a;
    AtInputMapper* b;
    for (unsigned buttons : {0u, 3u}) {
        for (unsigned axes : {0u, 4u}) {
            make(a, b, 0x33);
            same_router(&a->m_router, &b->m_router);
            AtInputMapper::Sizes sizes = {&g_device_token, buttons, axes};
            { On on(0); at<FnSizes>(0x00073100)(a, nullptr, &sizes); }
            { On on(1); b->Allocate(sizes); }
            same_mapper(a, b);
            compare_logs("allocated");
            // What it holds, and a copy of that. The second copy is of a mapper that has one of its own already: it clears it first.
            if (buttons) {
                a->m_buttonMapsUsed = b->m_buttonMapsUsed = 3;
                for (int i = 0; i < 3; i++) {
                    a->m_buttonMaps[i].m_physical = b->m_buttonMaps[i].m_physical = i + 1;
                    a->m_buttonMaps[i].m_abstract = b->m_buttonMaps[i].m_abstract = i + 20;
                    a->m_buttonMaps[i].m_state = b->m_buttonMaps[i].m_state = i + 30;
                }
            }
            if (axes) {
                a->m_axisMapsUsed = b->m_axisMapsUsed = 2;
                for (int i = 0; i < 2; i++) {
                    a->m_axisMaps[i].m_physical = b->m_axisMaps[i].m_physical = i + 40;
                    a->m_axisMaps[i].m_abstract = b->m_axisMaps[i].m_abstract = i + 50;
                }
            }
            if (!buttons) a->m_buttonMapsUsed = b->m_buttonMapsUsed = 2;  // With no capacity the count is not taken, or looked behind.
            if (!axes) a->m_axisMapsUsed = b->m_axisMapsUsed = 2;
            static unsigned char copy_o[sizeof(AtInputMapper)], copy_r[sizeof(AtInputMapper)];
            std::memset(copy_o, 0x44, sizeof copy_o);
            std::memset(copy_r, 0x44, sizeof copy_r);
            at<FnCtor>(0x00072B90)(copy_o, nullptr);
            new (copy_r) AtInputMapper;
            AtInputMapper* ca = (AtInputMapper*)copy_o;
            AtInputMapper* cb = (AtInputMapper*)copy_r;
            { On on(0); at<FnCopy>(0x00073220)(ca, nullptr, a); }
            { On on(1); cb->CopyFrom(*b); }
            same_mapper(ca, cb);
            compare_logs("copied");
            { On on(0); at<Fn0>(0x000730B0)(ca, nullptr); }
            { On on(1); cb->~AtInputMapper(); }
            compare_logs("copy destroyed");
            // Holding with no device: Clear leaves all of it, and so holds on to it.
            void* device = a->m_device;
            a->m_device = b->m_device = nullptr;
            { On on(0); at<Fn0>(0x00072BC0)(a, nullptr); }
            { On on(1); b->Clear(); }
            same_mapper(a, b);
            compare_logs("clear with no device");
            a->m_device = b->m_device = device;
            { On on(0); at<Fn0>(0x00072BC0)(a, nullptr); at<Fn0>(0x00072BC0)(a, nullptr); }
            { On on(1); b->Clear(); b->Clear(); }
            same_mapper(a, b);
            compare_logs("cleared");
            { On on(0); at<Fn0>(0x000730B0)(a, nullptr); }
            { On on(1); b->~AtInputMapper(); }
            same_router(&a->m_router, &b->m_router);
            compare_logs("destroyed");
        }
    }
    // The readers, one at a time, on what they find. A capacity of 0 reads nothing; the file's brace stays.
    for (const std::string& text : {std::string("ButtonMap\n{\nPhysicalID KeyA AbstractID KeyB\n}\n}\n"), std::string("AxisMap\n{\nPhysicalID AxisX AbstractID AxisY\n}\n}\n"), std::string("}")}) {
        for (unsigned capacity : {0u, 2u}) {
            for (int reader = 0; reader < 2; reader++) {
                make(a, b, 0x33);
                a->m_buttonMapCapacity = b->m_buttonMapCapacity = reader ? 0 : capacity;
                a->m_axisMapCapacity = b->m_axisMapCapacity = reader ? capacity / 2 : 0;
                Direct da(text), db(text);
                const bool oc = guarded([&] { On on(0); at<FnTok>(reader ? 0x00072E80 : 0x00072C10)(a, nullptr, &da.tokenizer); });
                g_recording = false;
                g_side = 0;
                const bool rc = guarded([&] { On on(1); if (reader) b->ReadAxisMaps(&db.tokenizer); else b->ReadButtonMaps(&db.tokenizer); });
                g_recording = false;
                g_side = 0;
                eq("crash", oc, rc);
                same_mapper(a, b);
                compare_logs("reader");
                eq("left at", da.stream.m_cur - da.stream.m_base, db.stream.m_cur - db.stream.m_base);
            }
        }
    }
    // No room for the maps: a raise, then the store crashes on null.
    for (int reader = 0; reader < 2; reader++) {
        const std::string text = reader ? "AxisMap\n{\nPhysicalID AxisX AbstractID AxisY\n}\n}\n" : "ButtonMap\n{\nPhysicalID KeyA AbstractID KeyB\n}\n}\n";
        make(a, b, 0x33);
        a->m_buttonMapCapacity = b->m_buttonMapCapacity = reader ? 0 : 2;
        a->m_axisMapCapacity = b->m_axisMapCapacity = reader ? 1 : 0;
        Direct da(text), db(text);
        g_fail_alloc = true;
        const bool oc = guarded([&] { On on(0); at<FnTok>(reader ? 0x00072E80 : 0x00072C10)(a, nullptr, &da.tokenizer); });
        g_recording = false;
        g_side = 0;
        const bool rc = guarded([&] { On on(1); if (reader) b->ReadAxisMaps(&db.tokenizer); else b->ReadButtonMaps(&db.tokenizer); });
        g_recording = false;
        g_side = 0;
        g_fail_alloc = false;
        eq("crash", oc, rc);
        compare_logs("no room for maps");
    }
    // A mapper read directly, with the owner's device null: Clear finds nothing, and the router is given null.
    for (void* device : {(void*)nullptr, (void*)&g_device_token}) {
        Owners own(device);
        make(a, b, 0x33);
        const std::string text = "{\nButtonMap[1]\n{\nButtonMap\n{\nPhysicalID KeyA AbstractID KeyB\n}\n}\n}\n";
        for (int pass = 0; pass < 2; pass++) {
            Direct da(text), db(text);
            { On on(0); at<FnDbRead>(0x000732A0)(a, nullptr, own.side[0], &da.tokenizer); }
            { On on(1); b->Read(own.side[1], &db.tokenizer); }
            same_mapper(a, b);
            compare_logs("read directly");
            // The second pass: the first left all it took, if there is no device to clear by. That is the original's leak, and its stale free list writes into what was freed, so it stops here.
            if (device) { a->m_router.m_freeFirst = b->m_router.m_freeFirst = nullptr; a->m_router.m_freeLast = b->m_router.m_freeLast = nullptr; }
            else break;
        }
        a->m_device = b->m_device = &g_device_token;
        { On on(0); at<Fn0>(0x000730B0)(a, nullptr); }
        { On on(1); b->~AtInputMapper(); }
        compare_logs("destroyed");
    }
    // The deleting destructor, for one and for an array. The recovered side's arrays go by AtDeleteArray, so the array form is held against the database's own Clear in the files above; this is the original's alone against the count.
    g_context = "mapper deleting";
    {
        void* p = std::malloc(sizeof(AtInputMapper));
        at<FnCtor>(0x00072B90)(p, nullptr);
        void* q = new AtInputMapper;
        { On on(0); eq("deleting returns this", (uintptr_t)at<FnDel>(0x00073790)(p, nullptr, 1), (uintptr_t)p); }
        { On on(1); delete (AtInputMapper*)q; }
        compare_logs("mapper deleted");
        void* s = std::malloc(sizeof(AtInputMapper));
        at<FnCtor>(0x00072B90)(s, nullptr);
        { On on(0); eq("deleting, no free", (uintptr_t)at<FnDel>(0x00073790)(s, nullptr, 0), (uintptr_t)s); }
        std::free(s);
        g_log[0].n = 0;
    }
    // The owner's.
    g_context = "owner";
    {
        Owners own;
        GmNameMap ma, mb, mc, md;
        for (const char* ns : {(const char*)nullptr, "", "Space", "Fresh"}) {
            at<FnAdd>(0x00073BC0)(own.side[0], nullptr, &ma, ns);
            own.side[1]->AddMappers(&mb, ns);
            eq("group", ma.m_group == &own.side[0]->m_mappers, mb.m_group == &own.side[1]->m_mappers);
            eqs("group's name", group_name(ma.m_group).c_str(), group_name(mb.m_group).c_str());
            eq("first", own.side[0]->m_mappers.m_first == &ma, own.side[1]->m_mappers.m_first == &mb);
            at<FnAdd>(0x00073BC0)(own.side[0], nullptr, &mc, ns);
            own.side[1]->AddMappers(&md, ns);
            eq("second's next", mc.m_next == &ma, md.m_next == &mb);
        }
    }
    // Clear with nothing read: all left as found.
    g_context = "database clear with nothing";
    {
        Owners own;
        AtInputMapperDatabase* o = make_o(0xCD);
        AtInputMapperDatabase* r = make_r(0xCD);
        o->m_owner = own.side[0];
        r->m_owner = own.side[1];
        o->m_unknown1C = r->m_unknown1C = 9;
        { On on(0); at<Fn0>(0x00073A00)(o, nullptr); }
        { On on(1); r->Clear(); }
        same_db("cleared with nothing", o, r, own.side[0], own.side[1]);
        std::free(o);
        std::free(r);
    }
    // The deleting destructor of the database, from the heap, holding a file.
    g_context = "database deleting";
    {
        Owners own;
        AtInputMapperDatabase* o = make_o(0xCD);
        AtInputMapperDatabase* r = make_r(0xCD);
        const std::string text = mapper("ButtonMap[1]\n{\nButtonMap\n{\nPhysicalID KeyA AbstractID KeyB\n}\n}\n");
        Direct da(text), db(text);
        { On on(0); at<FnDbRead>(0x00073810)(o, nullptr, own.side[0], &da.tokenizer); }
        { On on(1); r->Read(own.side[1], &db.tokenizer); }
        same_db("read", o, r, own.side[0], own.side[1]);
        destroy(o, r, "destroyed holding a file", 0);
        o = make_o(0xCD);
        void* q = operator new(sizeof(AtInputMapperDatabase));
        AtInputMapperDatabase* s = new (q) AtInputMapperDatabase;
        { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x00073B70)(o, nullptr, 1), (uintptr_t)o); }
        { On on(1); delete s; }
        compare_logs("deleted");
    }
}

// A file that will not open: the report, and then the reader on no tokenizer. In the game the report does not come back.
static void run_no_file() {
    g_context = "no file";
    Owners own;
    for (int status : {1, 2, 5}) {
        g_fs->open_status = status;
        AtInputMapperDatabase* o = make_o(0xCD);
        AtInputMapperDatabase* r = make_r(0xCD);
        const bool oc = guarded([&] { On on(0); at<FnLoad>(0x00073AD0)(o, nullptr, own.side[0], "DESIGN\\THING", 77); });
        g_recording = false;
        const bool rc = guarded([&] { On on(1); r->Load(own.side[1], "DESIGN\\THING", 77); });
        g_recording = false;
        eq("crash on no tokenizer", oc, rc);
        compare_logs("no file");
        g_fs->open_status = 0;
    }
}

static void run_tables() {
    g_context = "tables";
    const AtIoKeyword* image = (const AtIoKeyword*)(uintptr_t)0x002B2878;
    for (int i = 0; i < AtInputMapperDatabase::kKeywordCount; i++) {
        eq("keyword id", image[i].m_id, AtInputMapperDatabase::s_keywords[i].m_id);
        eqs("keyword", image[i].m_name, AtInputMapperDatabase::s_keywords[i].m_name);
    }
    image = (const AtIoKeyword*)(uintptr_t)0x002B2C10;
    eq("code count", *(uint32_t*)(uintptr_t)0x002B3368, AtInputMapperDatabase::kInputCodeCount);
    for (int i = 0; i < AtInputMapperDatabase::kInputCodeCount; i++) {
        eq("code id", image[i].m_id, AtInputMapperDatabase::s_inputCodes[i].m_id);
        eqs("code", image[i].m_name, AtInputMapperDatabase::s_inputCodes[i].m_name);
    }
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
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { std::printf("usage: oracle_ana <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new MemFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    set_mapper_heaps(&g_heap_token, &g_heap_token, &g_heap_token, &g_heap_token);

    g_heap_pad = 0x1000;
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_tables();
    run_written();
    run_no_room();
    run_no_heap();
    run_router();
    run_mapper();
    run_no_file();

    g_mappers = g_button_maps = g_axis_maps = 0;
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".ANM\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::string data((size_t)std::ftell(f), '\0');
        std::fseek(f, 0, SEEK_SET);
        if (!data.empty()) std::fread(&data[0], 1, data.size(), f);
        std::fclose(f);
        run_file(data, line, 0xCD);
        run_file(data, line, 0x33);
        g_files++;
    }
    std::fclose(index);
    report_entries();
    std::printf("%s: %u files, %u mappers, %u button and %u axis maps, each file on two fills and read twice on each; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_files, g_mappers, g_button_maps, g_axis_maps, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
