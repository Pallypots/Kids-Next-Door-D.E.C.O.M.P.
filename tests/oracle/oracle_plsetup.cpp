// Differential oracle for FaAcPlayer's set-up (slot 15) and what it alone needs, and for the combat modules GmCmCombat, FaCmCombat and FaCmPlayerCombat (Windows x86, local only).
// See docs/verification.md.
//
//   oracle_plsetup <default.xbe> [cases dir] [--entries FILE] [--mutate]
//
// No file reaches this code: nothing that makes actors from data is recovered. So the cases are objects built by each side's own constructor (the original's at its address, the recovered one by
// placement new), put into chosen states by writing the same values on both sides, and then called directly; return values, every word of the objects and of what they allocated, and the log
// of calls compared.
//
// What stands in, and is the same on both sides:
//  * The design registry's search by path (0x000679E0, a seam): one double, from a table each case fills: each word of the record and of the defaults, its type, element count and elements.
//    The saved game's registry is answered by the same double. Values' ElementAt (slot 5) is a double that logs.
//  * GmAcCharacter's slot 15 (0x00130950, whose actor's slot 15 beneath is not recovered): the original's address and the recovered function, both to one double that logs.
//  * The combat update (0x000A4BB0 and the recovered seam): one double. The weapons in the weapon sets: fakes whose slot 4 logs.
//  * The scene node (slot 2, FinishBody, logs), the materials' slot 21 (logs, answers by case), shared by both sides.
// Real on both sides: FaAcPlayer's constructor and destructor and everything beneath them (recovered by gmplayer, gmchar, gmactor), GmAcNode::FindValueOrDefault and GmBaObject::FindValue,
// GmAcStatSet::Find, the name helpers, sprintf (the image's) and AtFormat, GmNameMap, GmNameMapGroup and GmNameSpaceDb lookups, AtVertexFormat::Set, the heap's push and pop.
#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "at/AtIoTokenizer.h"
#include "at/AtCoordSys.h"
#include "at/AtMaterial.h"
#include "at/AtScene.h"
#include "at/AtSceneActor.h"
#include "gm/FaCmCombat.h"
#include "gm/GmAcPlayer.h"
#include "gm/GmAppSettings.h"
#include "gm/GmLevelManager.h"
#include "gm/GmLevelManifest.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static int g_crashed_alike;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 30) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 200).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 30) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 200).c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#include "oracle_doubles.h"

unsigned int AtPlatformMilliseconds() { return 0; }
// GmHistoryLogs::s_instance is defined beside the settings reader, which asks the platform for these; not called here.
int AtPlatformLanguage() { return 0; }
int AtPlatformGameRegion() { return 0; }

static uint32_t U(const void* p) { return (uint32_t)(uintptr_t)p; }
static void** vptr(void* obj) { return *(void***)obj; }
static uint32_t fbits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static float bitsf(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }

// thiscall through __fastcall: ecx = this, edx unused, the same stack arguments and cleanup.
typedef void(__fastcall* Fn0)(void*, void*);
typedef int(__fastcall* FnI0)(void*, void*);
typedef void*(__fastcall* FnP0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, uintptr_t);
typedef int(__fastcall* FnI1)(void*, void*, uintptr_t);
typedef void*(__fastcall* FnP1)(void*, void*, uintptr_t);
typedef void*(__fastcall* FnP2)(void*, void*, uintptr_t, uintptr_t);
typedef void*(__fastcall* FnP3)(void*, void*, uintptr_t, uintptr_t, uintptr_t);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__fastcall* Fn2)(void*, void*, uintptr_t, uintptr_t);
typedef void(__fastcall* Fn3)(void*, void*, uintptr_t, uintptr_t, uintptr_t);
typedef float(__fastcall* FnF2)(void*, void*, uintptr_t, uintptr_t);
typedef void(__fastcall* FnSet)(void*, void*, const AtVector3*, const AtVector3*, float, float, float, float);
typedef void(__fastcall* Fn7)(void*, void*, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);

// ---- canonical words ---------------------------------------------------------------------------------------------------------
static std::map<uint32_t, uint32_t> g_vft_token;   // original vftable -> token
static std::map<uint32_t, uint32_t> g_host_vft;    // host vftable -> token
static const uint32_t kOrigVfts[] = {0x00285828, 0x0028E850, 0x0028E638, 0x0028E4C8, 0x0028EF44, 0x0028EF50, 0x0028E30C, 0x0028E2F4, 0x0028E308, 0x002845C0, 0x0027C1D8,
                                     0x00278568, 0x0027855C, 0x00278418, 0x0027BCC4, 0x0028E33C, 0x0028C890, 0x0028C980, 0x0028CB48, 0x0027B794, 0x0027B78C, 0x00277A3C, 0x002770F4,
                                     0x0028F5C4, 0x00282EF4, 0x00282F5C};
struct Twin { uint32_t p[2]; uint32_t size; uint32_t id; };
static std::vector<Twin> g_twins;
static void twin(const void* a, const void* b, uint32_t size, uint32_t id) { g_twins.push_back({{U(a), U(b)}, size, id}); }

static uint32_t canon(int side, uint32_t w, const void* base, uint32_t size) {
    if (base && w >= U(base) && w < U(base) + size) return 0xA0000000u | (w - U(base));
    const uint32_t pool0 = 0x002E26F0, pool1 = U(GmLinkedListNode::s_pool), poolSize = 0x20 * GmLinkedListNode::kPoolSize;
    if (side == 0 && w >= pool0 && w < pool0 + poolSize) return 0xB0000000u | (w - pool0);
    if (side == 1 && w >= pool1 && w < pool1 + poolSize) return 0xB0000000u | (w - pool1);
    if (side == 0) {
        auto it = g_vft_token.find(w);
        if (it != g_vft_token.end()) return it->second;
    } else {
        auto it = g_host_vft.find(w);
        if (it != g_host_vft.end()) return it->second;
    }
    for (const Twin& t : g_twins) {
        if (w >= t.p[side] && w < t.p[side] + t.size) return 0xD0000000u | (t.id << 12) | (w - t.p[side]);
    }
    return w;
}

static void same_words(const char* what, const void* o, const void* r, uint32_t size, const void* obase = nullptr, const void* rbase = nullptr, uint32_t bsize = 0) {
    if (!obase) { obase = o; rbase = r; bsize = size; }
    const uint32_t* a = (const uint32_t*)o;
    const uint32_t* b = (const uint32_t*)r;
    for (uint32_t i = 0; i < size / 4; i++) {
        auto it = g_vft_token.find(a[i]);
        if (it != g_vft_token.end() && g_host_vft.find(b[i]) == g_host_vft.end()) {
            bool taken = false;
            for (auto& kv : g_host_vft) taken = taken || kv.second == it->second;
            if (!taken) g_host_vft[b[i]] = it->second;
        }
        const uint32_t ca = canon(0, a[i], obase, bsize), cb = canon(1, b[i], rbase, bsize);
        if (ca != cb) {
            char text[96];
            sprintf_s(text, "%s word +0x%X", what, i * 4);
            eq(text, ca, cb);
            return;
        }
        g_checks++;
    }
}

// ---- logging ---------------------------------------------------------------------------------------------------------------------
static uint32_t hash_str(const char* s) {
    if (!s) return 0xFFFFFFFFu;
    uint32_t h = 2166136261u;
    for (int i = 0; s[i]; i++) h = (h ^ (unsigned char)s[i]) * 16777619u;
    return h;
}
static void log_(uint32_t a, uint32_t b = 0) { if (g_recording) g_log[g_side].add(a, b); }
static uint32_t C(const void* p) { return canon(g_side, U(p), nullptr, 0); }

static uint32_t g_rand;
static uint32_t rnd() { g_rand = g_rand * 1103515245u + 12345u; return (g_rand >> 16) & 0x7FFF; }

// ---- the design registry and its values ------------------------------------------------------------------------------------------
struct Desc { void* vptr; uint32_t w4, w8, type, flags, count; };   // AtDataDescriptor: +0x0C type, +0x14 count
struct Val { void** vptr; Desc* desc; void* reg; char (*data)[8]; uint32_t id; void* elem; };  // AtDataComponent: +0x04 descriptor; a short-name array's data at +0x0C
static void* g_vt_val[8];
static void* __fastcall val_element(Val* v, void*, unsigned index) { log_('E' << 8, v->id); log_('e', index); return v->elem; }
struct LinkF { void* vptr; Val* value; };   // AtDataLink: +0x04 value

enum { kOwn = 1, kDefault = 2 };
struct Word {
    const char* name;
    int where;          // kOwn, kDefault, both, neither
    uint32_t own[4], def[4];
    Desc ownDesc, defDesc;
    Val ownVal, defVal;
    LinkF ownLink, defLink;
};
static Word g_words[] = {
    {"WeaponSets"}, {"Enemies"}, {"UseEnemyPriorities"}, {"EnemyPriorities"}, {"ThreatByRadius"}, {"ThreatBySightCone"}, {"ThreatEnterRange"}, {"ThreatExitRange"}, {"LockOnMinRangeH"},
    {"LockOnMaxRange"}, {"LockOffMinRangeH"}, {"LockOffMaxRange"}, {"MaxNewLockOnAngleH"}, {"MaxNextLockOnAngleH"}, {"MaxAimAngleH"}, {"MaxAimAngleV"}, {"CanAttackSiblings"},
    {"UseStartPositionForAttackRange"}, {"BumpTargetAngle"}, {"MinLookStickMagnitude"}, {"DigitalBumpTargetting"}, {"AnalogBumpTargetting"}, {"UpdateCombatModule"}, {"NumAttackRanges"},
    {"AttackRange0"}, {"AttackRange1"}, {"AttackRange2"}, {"AttackRangeEnterEvent0"}, {"AttackRangeEnterEvent1"}, {"AttackRangeEnterEvent2"}, {"AttackRangeExitEvent0"},
    {"AttackRangeExitEvent1"}, {"AttackRangeExitEvent2"}, {"Blaster1"}, {"Sword"}, {"Fist"}, {"LevelContinuation"}, {"Combat"},
};
static Word* word(const char* name) {
    for (Word& w : g_words) if (!std::strcmp(w.name, name)) return &w;
    return nullptr;
}
static char g_setNames[4][8];   // WeaponSets' elements: "Blaster1" fills all eight, "Nothing" is no word
static void* const kRegistry = (void*)0x5EC00000;
static LinkF g_record;
static GmDesignDefaults g_defaults;
static LinkF g_defaultRecord;

// The saved game: App's registry at +0x65B8 + 4, with a tree searched by name, and the search by path answering under a section or the root.
static unsigned char* g_app;
static AtDataLink* g_nodes[6];      // Level1 { SavedTime }, LEVEL2, SavedTime, Other, and a child of Other
static Desc g_saveDesc[4];
static Val g_saveVal[4];            // the section's SavedTime, the root's SavedTime node, what the search by path finds at the root, Other
static LinkF g_saveLink[2];
static float g_saveElem[4];
static bool g_savedUnderSection, g_savedAtRoot;
static AtDataRegistry* save_registry() { return &((GmPlayerApp*)g_app)->m_saveData.m_registry; }

static AtDataLink* find_impl(void* registry, void* under, const char* path, int parents) {
    log_('F' << 8, C(registry));
    log_('f', C(under));
    log_('f', hash_str(path));
    log_('f', parents);
    if (registry == save_registry()) {
        if (!std::strcmp(path, "SavedTime")) {
            if (under == g_nodes[0] && g_savedUnderSection) return (AtDataLink*)&g_saveLink[0];
            if (under == nullptr && g_savedAtRoot) return (AtDataLink*)&g_saveLink[1];
        }
        return nullptr;
    }
    const int table = under == &g_defaultRecord ? kDefault : under == &g_record ? kOwn : 0;
    for (Word& w : g_words) {
        if (std::strcmp(w.name, path) == 0 && (w.where & table)) return (AtDataLink*)(table == kDefault ? &w.defLink : &w.ownLink);
    }
    return nullptr;
}
static AtDataLink* __fastcall o_find(void* registry, void*, void* under, const char* path, int parents) { return find_impl(registry, under, path, parents); }
static AtDataLink* r_find(AtDataRegistry* registry, AtDataLink* under, const char* path, int parents) { return find_impl(registry, under, path, parents); }

static uint32_t pick_type(uint32_t usual) {
    const uint32_t r = rnd() % 6;
    return r < 4 ? usual : (r == 4 ? 3 : 8 + (rnd() % 4));
}
static void fill_words(uint32_t seed, int density) {
    g_rand = seed * 2654435761u + 1;
    for (Word& w : g_words) {
        const uint32_t r = rnd() % 8;
        w.where = r < (uint32_t)(4 - density) ? 0 : (int)(1 + rnd() % 3);
        for (int i = 0; i < 4; i++) {
            w.own[i] = 0x100 * (rnd() % 7) + i;
            w.def[i] = 0x200 * (rnd() % 7) + i;
        }
        w.ownDesc.type = pick_type(3);
        w.defDesc.type = pick_type(3);
        w.ownDesc.count = rnd() % 4;
        w.defDesc.count = rnd() % 4;
    }
    word("WeaponSets")->ownDesc.type = rnd() % 4 ? 10 : 8;
    word("WeaponSets")->ownDesc.count = rnd() % 5;
    word("EnemyPriorities")->ownDesc.type = rnd() % 4 ? 8 : 10;
    for (const char* n : {"NumAttackRanges", "LevelContinuation", "Combat"}) {
        Word* w = word(n);
        w->own[0] = (uint32_t)((int)(rnd() % 5) - 1);
        w->def[0] = (uint32_t)((int)(rnd() % 5) - 1);
    }
    static const char* const kSets[] = {"Blaster1", "Sword", "Nothing", "Fist"};
    for (int i = 0; i < 4; i++) {
        std::memset(g_setNames[i], 0, 8);
        const char* n = kSets[(i + rnd() % 2) % 4];
        std::memcpy(g_setNames[i], n, std::strlen(n));
    }
}
static void clear_words() { for (Word& w : g_words) w.where = 0; }

static void init_registry() {
    g_vt_val[5] = (void*)val_element;
    uint32_t id = 1;
    for (Word& w : g_words) {
        w.ownVal = {g_vt_val, &w.ownDesc, nullptr, g_setNames, id++, w.own};
        w.defVal = {g_vt_val, &w.defDesc, nullptr, g_setNames, id++, w.def};
        w.ownLink = {nullptr, &w.ownVal};
        w.defLink = {nullptr, &w.defVal};
    }
    g_defaults.m_registry = (AtDataRegistry*)0x0DEF0000;
    g_defaults.m_link = (AtDataLink*)&g_defaultRecord;
    *(void**)(uintptr_t)0x003B3524 = &g_defaults;
    GmDesignDefaults::s_instance = &g_defaults;
    // The saved game.
    g_app = (unsigned char*)std::calloc(1, sizeof(GmPlayerApp));
    *(void**)(uintptr_t)0x00304D58 = g_app;
    GmPlayerApp::s_instance = (GmPlayerApp*)g_app;
    static char names[6][12] = {"Level1", "SavedTime", "LEVEL2", "SavedTime", "Other", "child"};
    for (int i = 0; i < 6; i++) {
        g_nodes[i] = (AtDataLink*)std::calloc(1, sizeof(AtDataLink));
        g_nodes[i]->m_name = names[i];
    }
    for (int i = 0; i < 4; i++) g_saveVal[i] = {g_vt_val, &g_saveDesc[i], nullptr, nullptr, 700u + i, &g_saveElem[i]};
    g_saveLink[0] = {nullptr, &g_saveVal[0]};
    g_saveLink[1] = {nullptr, &g_saveVal[2]};
    AtDataLink* root = &save_registry()->m_root;
    root->m_firstChild = g_nodes[0];
    g_nodes[0]->m_firstChild = g_nodes[1];
    g_nodes[1]->m_value = (AtDataComponent*)&g_saveVal[0];
    g_nodes[0]->m_sibling = g_nodes[2];
    g_nodes[2]->m_sibling = g_nodes[3];
    g_nodes[3]->m_value = (AtDataComponent*)&g_saveVal[1];
    g_nodes[3]->m_sibling = g_nodes[4];
    g_nodes[4]->m_value = (AtDataComponent*)&g_saveVal[3];
    g_nodes[4]->m_firstChild = g_nodes[5];
}
static void fill_save(uint32_t k) {
    for (int i = 0; i < 4; i++) {
        g_saveDesc[i].type = (k >> (i % 3)) & 1 ? 3 : 1 + ((k + i) % 3);
        g_saveElem[i] = 10.5f + i + (float)k;
    }
    g_savedUnderSection = (k >> 3) & 1;
    g_savedAtRoot = (k >> 4) & 1;
    // The root's SavedTime node, and the section's, are in the tree or not, by their names: with neither, a name alone falls back on the search by path.
    g_nodes[3]->m_name[0] = (k >> 5) & 1 ? 'S' : 'X';
    g_nodes[1]->m_name[0] = (k >> 6) & 1 ? 'X' : 'S';
}

// ---- fakes -------------------------------------------------------------------------------------------------------------------------
struct Fake { void** vptr; uint32_t words[0x40]; };
static void* g_vt_weapon[8];
static void* g_vt_node[8];
static void __fastcall d_weapon_release(Fake* f, void*) { log_('W', C(f)); }
static void __fastcall d_node_finish(Fake* f, void*) { log_('S', C(f)); }
static void __fastcall d_char15(void* self, void*) { log_('Q' << 8 | 15, C(self)); }
static void __fastcall d_update(void* self, void*, int a) { log_('U', C(self)); log_('u', a); }
static Fake g_weapons[3];
static Fake* g_node;   // the scene node at +0x9C, shared

// The materials: a host owner whose slot 21 logs and answers by case, with "Target" and "Other" in its plain table and "Target" in name space "sp".
static int g_slot21;
struct FakeMaterials : AtMaterialOwner {
    int Slot21() override { log_('M', 21); return g_slot21; }
};
static FakeMaterials* g_materials;
static GmSpriteScene g_spriteScene;
static GmSystemScenes g_scenes;
static GmHistoryLogs g_system;
static void* const kTargetMaterial = (void*)0x7A7A0001;
static void* const kSpaceTarget = (void*)0x7A7A0002;
static void* const kOtherMaterial = (void*)0x7A7A0003;

// The scene actor's joints: HEADR and others, in or out of the rotation-translation array.
static AtCoordSysDatabase* g_joints;
static unsigned char* g_actor;       // AtSceneJointedModelActor-shaped: only +0xA0 is read
static unsigned char* g_rotTrans;    // four entries of 0x28
static unsigned char* g_outside;
static void set_head(int how) {
    // 0 none, 1 first entry, 2 last entry, 3 one past the end, 4 outside
    g_joints->Remove("HEADR");
    if (how == 1) g_joints->Insert("HEADR", g_rotTrans);
    if (how == 2) g_joints->Insert("HEADR", g_rotTrans + 3 * 0x28);
    if (how == 3) g_joints->Insert("HEADR", g_rotTrans + 4 * 0x28);
    if (how == 4) g_joints->Insert("HEADR", g_outside);
}

static void init_fakes() {
    g_vt_weapon[4] = (void*)d_weapon_release;
    for (Fake& w : g_weapons) w.vptr = g_vt_weapon;
    g_vt_node[2] = (void*)d_node_finish;
    g_node = (Fake*)std::calloc(1, sizeof(Fake));
    g_node->vptr = g_vt_node;
    g_materials = new FakeMaterials;
    GmNameMap* plain = new GmNameMap;
    plain->Init(8);
    plain->Insert("Target", kTargetMaterial);
    plain->Insert("Other", kOtherMaterial);
    g_materials->m_materials.Add(plain);
    GmNameMap* spaced = new GmNameMap;
    spaced->Init(8);
    spaced->Insert("Target", kSpaceTarget);
    g_materials->m_materialSpaces.Init(4);
    g_materials->m_materialSpaces.Add("sp", spaced);
    g_spriteScene.m_materials = g_materials;
    g_scenes.m_spriteScene = &g_spriteScene;
    g_system.m_scenes = &g_scenes;
    *(void**)(uintptr_t)0x002CB0A8 = &g_system;
    GmHistoryLogs::s_instance = &g_system;
    g_joints = new AtCoordSysDatabase;
    g_joints->Init(16);
    g_rotTrans = (unsigned char*)std::calloc(4, 0x28);
    g_outside = (unsigned char*)std::calloc(1, 0x28);
    g_joints->m_rotTrans = (AtRotTransCoordSys*)g_rotTrans;
    g_joints->m_rotTransCount = 4;
    g_joints->Insert("NECK", g_rotTrans + 0x28);
    g_joints->Insert("HEADL", g_rotTrans + 0x50);
    g_actor = (unsigned char*)std::calloc(1, 0x100);
    *(AtCoordSysDatabase**)(g_actor + 0xA0) = g_joints;
}

template <class M> static void* member_address(M m) { void* p; std::memcpy(&p, &m, sizeof p); return p; }
namespace {
struct Placement {};
}  // namespace
#pragma warning(disable : 4291)
inline void* operator new(std::size_t, void* where, Placement) { return where; }

// ---- both sides of one call --------------------------------------------------------------------------------------------------------
template <class FO, class FR> static bool both(const char* what, FO fo, FR fr) {
    const bool oc = guarded([&] { On on(0); fo(); });
    g_recording = false;
    g_side = 0;
    const bool rc = guarded([&] { On on(1); fr(); });
    g_recording = false;
    g_side = 0;
    eq((std::string(what) + ": crashed").c_str(), oc, rc);
    if (oc && rc) {
        g_crashed_alike++;
        if (getenv("PLSETUP_TRACE")) std::printf("crashed alike: %s [%s]\n", what, g_context.c_str());
    }
    compare_logs(what);
    return oc || rc;
}

static void* fresh(uint32_t size) {
    void* p = std::calloc(1, size + 16);
    std::memset(p, 0xCD, size + 16);
    return p;
}

static void pair_host(uint32_t orig, const void* host_obj, uint32_t offset = 0) {
    g_host_vft[*(const uint32_t*)((const char*)host_obj + offset)] = g_vft_token[orig];
}

static void pair_vftables() {
    uint32_t token = 0xC0000000u;
    for (uint32_t v : kOrigVfts) g_vft_token[v] = token++;
    AtEvSubscriber* e = new AtEvSubscriber; pair_host(0x00285828, e);
    GmBaObject* b = new GmBaObject; pair_host(0x0028E850, b);
    GmAcNode* n = new GmAcNode; pair_host(0x0028E638, n);
    GmAcActor* a = new GmAcActor; pair_host(0x0028E4C8, a);
    pair_host(0x0028EF50, &a->m_animation);
    pair_host(0x0028E30C, &a->m_vectors184);
    pair_host(0x0028E2F4, &a->m_items);
    pair_host(0x0028E308, &a->m_items, 0x10);
    pair_host(0x0027C1D8, &a->m_callback);
    AtKiAnimationCallBack* k = new AtKiAnimationCallBack; pair_host(0x002845C0, k);
    GmLinkedList* l = new GmLinkedList; pair_host(0x00278568, l);
    GmPlug* p = new GmPlug; pair_host(0x0027855C, p);
    GmLinkedListIteratorOf<GmAcLinkedItem*>* it = new GmLinkedListIteratorOf<GmAcLinkedItem*>(l); pair_host(0x0028E33C, it);
    GmLinkedListIterator* it2 = new GmLinkedListIterator(l); pair_host(0x0027BCC4, it2);
    GmAcObject* o = new GmAcObject; pair_host(0x0028C890, o);
    GmAcCharacter* c = new GmAcCharacter; pair_host(0x0028C980, c);
    FaAcPlayer* f = new FaAcPlayer; pair_host(0x0027B794, f); pair_host(0x0027B78C, f, 0x32C); pair_host(0x00277A3C, &f->m_target);
    delete f;  // its sight query counted itself and allocated the shared query: given back, so both sides start with none
    GmCmCombat* g = new GmCmCombat; pair_host(0x0028F5C4, g);
    FaCmCombat* fc = new FaCmCombat; pair_host(0x00282EF4, fc);
    FaCmPlayerCombat* pc = new FaCmPlayerCombat; pair_host(0x00282F5C, pc);
}

static int g_cases;
static int g_heap_token, g_combat_heap_token;
static void set_heaps(bool actor, bool combat) {
    *(void**)(uintptr_t)0x003B3930 = actor ? &g_heap_token : nullptr;
    GmAcActor::s_heap = actor ? &g_heap_token : nullptr;
    *(void**)(uintptr_t)0x003B3C5C = combat ? &g_combat_heap_token : nullptr;
    FaCmCombat::s_heap = combat ? &g_combat_heap_token : nullptr;
}

// ---- an owner's stats: each side its own, compared ------------------------------------------------------------------------------------
struct Stats {
    GmAcStatsHolder holder[2];
    GmAcStatSet set[2];
    GmAcStat* list[2][3];
    GmAcStat stat[2][3];
    void fill(uint32_t k) {
        for (int s = 0; s < 2; s++) {
            std::memset(stat[s], 0, sizeof stat[s]);
            std::memcpy(stat[s][0].m_name, "sArmour", 7);
            std::memcpy(stat[s][1].m_name, "sHealth", 7);
            std::memcpy(stat[s][2].m_name, "sHealth", 7);   // the first is found
            static const float bases[] = {5.0f, -3.0f, 150.0f, 0.0f, 50.0f, -0.0f};
            stat[s][1].m_base = (k & 8) ? bitsf(0x7FC00001u) : bases[k % 6];
            stat[s][1].m_value = 77.0f;
            stat[s][1].m_min = (k & 16) ? 60.0f : 0.0f;    // above the maximum in some
            stat[s][1].m_max = (k & 32) ? 40.0f : 100.0f;
            stat[s][1].m_unknown14 = 0x14;
            stat[s][2].m_base = 9.0f;
            list[s][0] = &stat[s][0];
            list[s][1] = (k & 64) ? nullptr : &stat[s][1];
            list[s][2] = &stat[s][2];
            set[s].m_count = (k & 128) ? 1 : 3;
            set[s].m_stats = list[s];
            holder[s].m_stats = &set[s];
        }
        twin(&stat[0], &stat[1], sizeof stat[0], 7);
        twin(&set[0], &set[1], sizeof set[0], 8);
        twin(&holder[0], &holder[1], sizeof holder[0], 9);
        twin(&list[0], &list[1], sizeof list[0], 10);
    }
    void compare(const char* what) {
        same_words(what, stat[0], stat[1], sizeof stat[0]);
        same_words(what, list[0], list[1], sizeof list[0]);
    }
};

// ---- combat objects of both sides -------------------------------------------------------------------------------------------------
enum CombatKind { kGm, kFa, kFaPlayer };
static const uint32_t kCombatSizes[] = {0x0C, 0xE4, 0xE4};
static const uint32_t kCombatCtors[] = {0x001645F0, 0x000A3360, 0x000A4D00};
static const uint32_t kCombatDtors[] = {0x00164620, 0x000A3BE0, 0x000A4D30};
static void construct_combat(CombatKind kind, void* r) {
    if (kind == kGm) new (r, Placement()) GmCmCombat;
    else if (kind == kFa) new (r, Placement()) FaCmCombat;
    else new (r, Placement()) FaCmPlayerCombat;
}
static void destroy_combat(CombatKind kind, void* r) {
    if (kind == kGm) ((GmCmCombat*)r)->GmCmCombat::~GmCmCombat();
    else if (kind == kFa) ((FaCmCombat*)r)->FaCmCombat::~FaCmCombat();
    else ((FaCmPlayerCombat*)r)->FaCmPlayerCombat::~FaCmPlayerCombat();
}

// What a combat object allocated: twinned, so that its words compare, and then compared themselves.
static void compare_combat(const char* what, void* o, void* r, uint32_t size) {
    if (size > 0x0C) {
        FaCmCombat* a = (FaCmCombat*)o;
        FaCmCombat* b = (FaCmCombat*)r;
        // Read only what both sides hold in the same shape; a difference in the counts shows in the words.
        if (a->m_attackRanges && b->m_attackRanges && a->m_attackRangeCount == b->m_attackRangeCount && a->m_attackRangeCount > 0 && a->m_attackRangeCount < 16) {
            twin(a->m_attackRanges, b->m_attackRanges, a->m_attackRangeCount * 12, 20);
            same_words("attack ranges", a->m_attackRanges, b->m_attackRanges, a->m_attackRangeCount * 12);
        }
        if (a->m_weaponSetList && b->m_weaponSetList && a->m_weaponSetCount == b->m_weaponSetCount && a->m_weaponSetCount < 16) {
            twin(a->m_weaponSetList, b->m_weaponSetList, a->m_weaponSetCount * 16, 21);
            for (uint32_t i = 0; i < a->m_weaponSetCount; i++) {
                FaCmWeaponSet* x = &a->m_weaponSetList[i];
                FaCmWeaponSet* y = &b->m_weaponSetList[i];
                if (x->m_weapons && y->m_weapons && x->m_count == y->m_count && x->m_count < 16) {
                    twin(x->m_weapons, y->m_weapons, x->m_count * 4 + 1, 22 + i);
                    same_words("weapons", x->m_weapons, y->m_weapons, x->m_count * 4);
                }
            }
            same_words("weapon sets", a->m_weaponSetList, b->m_weaponSetList, a->m_weaponSetCount * 16);
        }
    }
    same_words(what, o, r, size);
}

struct Owner {
    unsigned char* b[2];
    Stats stats;
    explicit Owner(uint32_t k, bool registry = true, bool record = true) {
        for (int s = 0; s < 2; s++) {
            b[s] = (unsigned char*)fresh(0x304);
            *(void**)(b[s] + 0x10) = registry ? kRegistry : nullptr;
            *(void**)(b[s] + 0x14) = record ? (void*)&g_record : nullptr;
            *(void**)(b[s] + 0x270) = &stats.holder[s];
        }
        twin(b[0], b[1], 0x304, 6);
        stats.fill(k);
    }
};

// The weapons the update would have put in the sets: the same fakes in the same places on both sides.
static void arm_weapons(void* o, void* r, uint32_t k) {
    for (void* p : {o, r}) {
        FaCmCombat* c = (FaCmCombat*)p;
        if (!c->m_weaponSetList) continue;
        for (uint32_t i = 0; i < c->m_weaponSetCount && i < 16; i++) {
            FaCmWeaponSet* set = &c->m_weaponSetList[i];
            for (uint32_t j = 0; j < set->m_count && j < 16; j++) {
                if (set->m_weapons && ((k >> (i + j)) & 1)) set->m_weapons[j] = (GmCmWeapon*)&g_weapons[(i + j) % 3];
            }
        }
    }
}

// ---- combat: lifetimes ---------------------------------------------------------------------------------------------------------------
static void run_combat_lifetimes() {
    for (int kind : {kGm, kFa, kFaPlayer}) {
        for (int how = 0; how < 24; how++) {
            g_context = "combat lifetime " + std::to_string(kind) + " " + std::to_string(how);
            const uint32_t size = kCombatSizes[kind];
            void* o = fresh(size);
            void* r = fresh(size);
            both("construct", [&] { at<FnP0>(kCombatCtors[kind])(o, nullptr); }, [&] { construct_combat((CombatKind)kind, r); });
            same_words("constructed", o, r, size);
            twin(o, r, size, 1);
            Owner owner(how);
            if ((how & 2) && kind != kGm) {
                fill_words(300 + how, 3);
                g_record.vptr = nullptr;
                both("attach", [&] { ((Fn1)vptr(o)[1])(o, nullptr, U(owner.b[0])); }, [&] { ((Fn1)vptr(r)[1])(r, nullptr, U(owner.b[1])); });
                arm_weapons(o, r, 0x55);
                compare_combat("attached", o, r, size);
            }
            if (how & 4) {
                // Every word but the vftable and what a destructor follows.
                for (uint32_t off = 4; off < size; off += 4) {
                    if (off == 0x38 || off == 0x84 || off == 0x88) continue;
                    *(uint32_t*)((char*)o + off) = *(uint32_t*)((char*)r + off) = 0x1000 + off;
                }
            }
            const int form = (how >> 3) % 3;  // 0 complete, 1 deleting with 0, 2 deleting with 1
            if (form == 0) {
                both("destroy", [&] { at<Fn0>(kCombatDtors[kind])(o, nullptr); }, [&] { destroy_combat((CombatKind)kind, r); });
                compare_combat("destroyed", o, r, size);
            } else {
                const unsigned flag = form - 1;
                both("delete", [&] { ((FnDel)vptr(o)[0])(o, nullptr, flag); }, [&] { ((FnDel)vptr(r)[0])(r, nullptr, flag); });
                if (!flag) compare_combat("deleted, flag 0", o, r, size);
            }
            g_twins.clear();
            g_cases++;
        }
    }
}

// ---- combat: attach, the record, and the slots ---------------------------------------------------------------------------------------
static void run_combat() {
    for (int k = 0; k < 480; k++) {
        const int kind = k % 11 == 3 ? kGm : k % 5 == 1 ? kFa : kFaPlayer;
        g_context = "combat " + std::to_string(k) + " kind " + std::to_string(kind);
        const uint32_t size = kCombatSizes[kind];
        void* o = fresh(size);
        void* r = fresh(size);
        at<FnP0>(kCombatCtors[kind])(o, nullptr);
        construct_combat((CombatKind)kind, r);
        twin(o, r, size, 1);
        fill_words(k, k % 5);
        Owner owner(k, k % 13 != 4, k % 17 != 6);
        set_heaps(true, k % 23 != 9);
        uint32_t ro = 0, rr = 0;
        const bool failing = k % 29 == 14;
        bool crashed = both("attach", [&] { g_fail_alloc = failing; ((Fn1)vptr(o)[1])(o, nullptr, U(owner.b[0])); g_fail_alloc = false; },
                            [&] { g_fail_alloc = failing; ((Fn1)vptr(r)[1])(r, nullptr, U(owner.b[1])); g_fail_alloc = false; });
        set_heaps(true, true);
        if (crashed) { g_twins.clear(); g_cases++; continue; }
        compare_combat("attached", o, r, size);
        owner.stats.compare("stats");
        if (k % 3 == 0) {
            // Again: the weapon sets are not made twice; the record is read again.
            both("attach again", [&] { ((Fn1)vptr(o)[1])(o, nullptr, U(owner.b[0])); }, [&] { ((Fn1)vptr(r)[1])(r, nullptr, U(owner.b[1])); });
            compare_combat("attached again", o, r, size);
        }
        both("slot 2", [&] { ro = ((FnI0)vptr(o)[2])(o, nullptr); }, [&] { rr = ((FnI0)vptr(r)[2])(r, nullptr); });
        eq("slot 2", ro, rr);
        both("slot 3", [&] { ((Fn0)vptr(o)[3])(o, nullptr); }, [&] { ((Fn0)vptr(r)[3])(r, nullptr); });
        both("slot 7", [&] { ro = ((FnI0)vptr(o)[7])(o, nullptr); }, [&] { rr = ((FnI0)vptr(r)[7])(r, nullptr); });
        eq("slot 7", ro, rr);
        if (kind != kGm) {
            // The base classes' own, called directly.
            if (k % 4 == 2) {
                both("FaCmCombat attach", [&] { at<Fn1>(0x000A33B0)(o, nullptr, U(owner.b[0])); }, [&] { ((FaCmCombat*)r)->FaCmCombat::Attach((GmAcActor*)owner.b[1]); });
                compare_combat("FaCmCombat attached", o, r, size);
            }
            both("FaCmCombat slot 2", [&] { ro = at<FnI0>(0x000A1F10)(o, nullptr); }, [&] { rr = ((FaCmCombat*)r)->FaCmCombat::ReadRecord(); });
            eq("FaCmCombat slot 2", ro, rr);
            both("GmCmCombat attach", [&] { at<Fn1>(0x001645D0)(o, nullptr, U(owner.b[0])); }, [&] { ((FaCmCombat*)r)->GmCmCombat::Attach((GmAcActor*)owner.b[1]); });
            compare_combat("after reads", o, r, size);
            arm_weapons(o, r, (uint32_t)k * 2654435761u);
            for (uint32_t off : {0x2C, 0x30, 0x78, 0x7C, 0x94, 0x98, 0x9C, 0xA0, 0xA4, 0xB0, 0xCC, 0xD4, 0xDC, 0xE0}) {
                *(uint32_t*)((char*)o + off) = *(uint32_t*)((char*)r + off) = 0x5000 + off;
            }
            *(uint32_t*)((char*)o + 8) = *(uint32_t*)((char*)r + 8) = 0;
        }
        both("slot 5", [&] { ((Fn0)vptr(o)[5])(o, nullptr); }, [&] { ((Fn0)vptr(r)[5])(r, nullptr); });
        compare_combat("slot 5", o, r, size);
        both("slot 6", [&] { ((Fn1)vptr(o)[6])(o, nullptr, 0x66 + k); }, [&] { ((Fn1)vptr(r)[6])(r, nullptr, 0x66 + k); });
        if (kind != kGm) {
            arm_weapons(o, r, ~(uint32_t)k);
            both("ReleaseWeapons", [&] { at<Fn0>(0x000A1C50)(o, nullptr); }, [&] { ((FaCmCombat*)r)->ReleaseWeapons(); });
            compare_combat("released", o, r, size);
            arm_weapons(o, r, (uint32_t)k * 40503u);
        }
        both("slot 4", [&] { ((Fn0)vptr(o)[4])(o, nullptr); }, [&] { ((Fn0)vptr(r)[4])(r, nullptr); });
        compare_combat("slot 4", o, r, size);
        both("delete", [&] { ((FnDel)vptr(o)[0])(o, nullptr, 1); }, [&] { ((FnDel)vptr(r)[0])(r, nullptr, 1); });
        g_twins.clear();
        g_cases++;
    }
    // Slot 7 over health values, and MakeWeaponSets alone from states Attach does not reach.
    const float values[] = {1.0f, 0.0f, -0.0f, -1.0f, 1e-30f, -1e-30f, bitsf(0x7FC00000u), bitsf(0xFFC00000u)};
    for (int k = 0; k < 18; k++) {
        g_context = "slot 7 " + std::to_string(k);
        void* o = fresh(0xE4);
        void* r = fresh(0xE4);
        at<FnP0>(0x000A4D00)(o, nullptr);
        construct_combat(kFaPlayer, r);
        GmAcStat stat = {};
        stat.m_value = values[k % 8];
        ((FaCmCombat*)o)->m_health = ((FaCmCombat*)r)->m_health = k < 16 ? &stat : nullptr;
        uint32_t ro = 0, rr = 0;
        both("slot 7", [&] { ro = ((FnI0)vptr(o)[7])(o, nullptr); }, [&] { rr = ((FnI0)vptr(r)[7])(r, nullptr); });
        eq("slot 7", ro, rr);
        g_cases++;
    }
    for (int k = 0; k < 24; k++) {
        g_context = "weapon sets " + std::to_string(k);
        void* o = fresh(0xE4);
        void* r = fresh(0xE4);
        at<FnP0>(0x000A4D00)(o, nullptr);
        construct_combat(kFaPlayer, r);
        twin(o, r, 0xE4, 1);
        fill_words(900 + k, 4);
        Owner owner(k);
        FaCmCombat* a = (FaCmCombat*)o;
        FaCmCombat* b = (FaCmCombat*)r;
        a->m_owner = (GmAcActor*)owner.b[0];
        b->m_owner = (GmAcActor*)owner.b[1];
        Word* sets = word("WeaponSets");
        sets->ownDesc.count = k % 5;
        a->m_weaponSets = b->m_weaponSets = (k % 6 == 5) ? nullptr : (AtDataComponent*)&sets->ownVal;
        static FaCmWeaponSet dummy[2];
        if (k % 7 == 3) a->m_weaponSetList = b->m_weaponSetList = dummy;
        set_heaps(true, k % 8 != 7);
        both("make weapon sets", [&] { at<Fn0>(0x000A29F0)(o, nullptr); }, [&] { b->MakeWeaponSets(); });
        set_heaps(true, true);
        compare_combat("made", o, r, 0xE4);
        if (a->m_weaponSetList == dummy) a->m_weaponSetList = b->m_weaponSetList = nullptr;
        g_twins.clear();
        g_cases++;
    }
}

// ---- the lookups, directly ---------------------------------------------------------------------------------------------------------
static void run_lookups() {
    // The registry's search by name.
    {
        const char* names[] = {"Level1", "level1", "LEVEL2", "SavedTime", "savedtime", "Other", "child", "CHILD", "absent", ""};
        AtDataLink* unders[] = {nullptr, g_nodes[0], g_nodes[2], g_nodes[4], g_nodes[5]};
        int n = 0;
        for (int tree = 0; tree < 2; tree++) {
            g_nodes[3]->m_name[0] = tree ? 'S' : 'X';
            for (const char* name : names) {
                for (AtDataLink* under : unders) {
                    for (int exact : {0, 1, 7}) {
                        g_context = std::string("find named ") + name + " " + std::to_string(n++);
                        void* ro = nullptr;
                        void* rr = nullptr;
                        both("find named", [&] { ro = at<FnP3>(0x000678A0)(save_registry(), nullptr, U(name), U(under), exact); },
                             [&] { rr = AtDataRegistryFindNamed(save_registry(), name, under, exact); });
                        eq("found", U(ro), U(rr));
                    }
                }
            }
        }
        g_cases++;
    }
    // The saved game's values: by section, by type, and the real reader.
    {
        const char* sections[] = {"Level1", "LEVEL1", "level2", "Nowhere", "", "SavedTime"};
        for (uint32_t k = 0; k < 128; k++) {
            fill_save(k);
            for (const char* section : sections) {
                for (const char* name : {"SavedTime", "Other", "savedTIME"}) {
                    g_context = std::string("saved ") + section + " " + name + " " + std::to_string(k);
                    for (uint32_t type : {1u, 2u, 3u}) {
                        void* ro = nullptr;
                        void* rr = nullptr;
                        both("save find", [&] { ro = at<FnP3>(0x00144A50)(&((GmPlayerApp*)g_app)->m_saveData, nullptr, U(section), U(name), type); },
                             [&] { rr = ((GmPlayerApp*)g_app)->m_saveData.Find(section, name, type); });
                        eq("save find", U(ro), U(rr));
                    }
                    void* ro = nullptr;
                    void* rr = nullptr;
                    both("save real", [&] { ro = at<FnP2>(0x00144B20)(&((GmPlayerApp*)g_app)->m_saveData, nullptr, U(section), U(name)); },
                         [&] { rr = ((GmPlayerApp*)g_app)->m_saveData.FindReal(section, name); });
                    eq("save real", U(ro), U(rr));
                    float fo = 1, fr = 2;
                    both("saved real", [&] { fo = at<FnF2>(0x0009D980)((void*)(uintptr_t)0x12345678, nullptr, U(section), U(name)); },
                         [&] { fr = GmSavedGame::s_instance->GetReal(section, name); });
                    eq("saved real", fbits(fo), fbits(fr));
                }
            }
            g_cases++;
        }
    }
    // The value itself under an object's record.
    for (int k = 0; k < 32; k++) {
        fill_words(500 + k, k % 5);
        Owner owner(k, k % 3 != 1, k % 5 != 2);
        for (const char* name : {"WeaponSets", "Enemies", "Sword", "NumAttackRanges", "Missing", ""}) {
            g_context = std::string("component ") + name + " " + std::to_string(k);
            for (int unused : {0, 1}) {
                void* ro = nullptr;
                void* rr = nullptr;
                both("component", [&] { ro = at<FnP2>(0x0014CB00)(owner.b[0], nullptr, U(name), unused); }, [&] { rr = ((GmBaObject*)owner.b[1])->FindComponent(name, unused); });
                eq("component", U(ro), U(rr));
            }
        }
        g_twins.clear();
        g_cases++;
    }
    // The head joint's lookup.
    for (int how = 0; how < 5; how++) {
        set_head(how);
        for (int count : {4, 3, 0}) {
            g_joints->m_rotTransCount = count;
            for (const char* name : {"HEADR", "NECK", "HEADL", "headr", "NONE"}) {
                g_context = std::string("rot trans ") + name + " " + std::to_string(how) + " " + std::to_string(count);
                void* ro = nullptr;
                void* rr = nullptr;
                both("rot trans", [&] { ro = at<FnP1>(0x00026860)(g_joints, nullptr, U(name)); }, [&] { rr = g_joints->FindRotTrans(name); });
                eq("rot trans", U(ro), U(rr));
            }
            g_cases++;
        }
    }
    g_joints->m_rotTransCount = 4;
    // The sight query's Set.
    {
        const float lengths[] = {10.0f, 0.0f, -3.5f, 1e20f, 1e-25f, 1000.0f, bitsf(0x7FC00000u), 3.3333333f};
        const float angles[] = {0.0f, 25.0f, 20.0f, 90.0f, 180.0f, -45.0f, 1e6f, 33.3f, 359.99f};
        int n = 0;
        for (float length : lengths) {
            for (float yaw : angles) {
                for (int m = 0; m < 3; m++) {
                    const float pitch = angles[(n + m) % 9];
                    const float least = lengths[(n * 3 + m) % 8];
                    g_context = "sight set " + std::to_string(n++);
                    const AtVector3 at3 = {1.5f, -2.0f, bitsf(0x7FC00000u)}, way = {0.0f, 1.0f, -0.0f};
                    void* o = fresh(0x2C);
                    void* r = fresh(0x2C);
                    both("set", [&] { at<FnSet>(0x00152B00)(o, nullptr, &at3, &way, length, yaw, pitch, least); }, [&] { ((GmAcSightQuery*)r)->Set(&at3, &way, length, yaw, pitch, least); });
                    same_words("sight query", o, r, 0x2C);
                    std::free(o);
                    std::free(r);
                }
            }
        }
        g_cases++;
    }
    // The sprite's two set-up slots.
    for (int k = 0; k < 40; k++) {
        g_context = "sprite " + std::to_string(k);
        void* o = fresh(0xA4);
        void* r = fresh(0xA4);
        at<FnP0>(0x0006E1F0)(o, nullptr);
        new (r, Placement()) GmAcSprite;
        std::memset((char*)o + 0x24, 0x24, 0x70);
        std::memset((char*)r + 0x24, 0x24, 0x70);
        *(uint32_t*)((char*)o + 0x9C) = *(uint32_t*)((char*)r + 0x9C) = 0x9C9C;
        g_slot21 = 0x2100 + k;
        static const char* const kNames[] = {"Target", "Other", "Absent", "target"};
        static const char* const kSpaces[] = {nullptr, "", "sp", "nosp", "SP"};
        const char* name = kNames[k % 4];
        const char* ns = kSpaces[(k / 4) % 5];
        if (k < 20) {
            both("setup named", [&] { at<Fn3>(0x0006E260)(o, nullptr, U(&g_spriteScene), U(name), U(ns)); }, [&] { ((GmAcSprite*)r)->GmAcSprite::SetupNamed(&g_spriteScene, name, ns); });
        } else if (k < 30) {
            both("setup named, slot 1", [&] { ((Fn3)vptr(o)[1])(o, nullptr, U(&g_spriteScene), U(name), U(ns)); }, [&] { ((Fn3)vptr(r)[1])(r, nullptr, U(&g_spriteScene), U(name), U(ns)); });
        } else if (k < 35) {
            both("setup", [&] { at<Fn1>(0x0006E2E0)(o, nullptr, U(&g_spriteScene)); }, [&] { ((GmAcSprite*)r)->GmAcSprite::Setup(&g_spriteScene); });
        } else {
            both("setup, slot 0", [&] { ((Fn1)vptr(o)[0])(o, nullptr, U(&g_spriteScene)); }, [&] { ((Fn1)vptr(r)[0])(r, nullptr, U(&g_spriteScene)); });
        }
        same_words("sprite", o, r, 0xA4);
        g_cases++;
    }
}

// ---- players of both sides --------------------------------------------------------------------------------------------------------
static unsigned char* g_level[2];

struct Pair {
    unsigned char* o;
    unsigned char* r;
    Owner* owner;   // only its stats: each side's player holds its own side's
    explicit Pair(uint32_t k) {
        o = (unsigned char*)fresh(0x4D0);
        r = (unsigned char*)fresh(0x4D0);
        at<FnP0>(0x00093FA0)(o, nullptr);
        new (r, Placement()) FaAcPlayer;
        twin(o, r, 0x4D0, 1);
        owner = new Owner(k);
        for (int s = 0; s < 2; s++) {
            unsigned char* p = s ? r : o;
            *(void**)(p + 0x10) = kRegistry;
            *(void**)(p + 0x14) = &g_record;
            *(void**)(p + 0x270) = &owner->stats.holder[s];
            *(void**)(p + 0x9C) = g_node;
        }
    }
    template <class T> void set(uint32_t offset, T value) { *(T*)(o + offset) = value; *(T*)(r + offset) = value; }
    FaAcPlayer* F() { return (FaAcPlayer*)r; }
    void compare(const char* what) {
        FaAcPlayer* a = (FaAcPlayer*)o;
        FaAcPlayer* b = (FaAcPlayer*)r;
        if (a->m_combat && b->m_combat) {
            twin(a->m_combat, b->m_combat, 0xE4, 30);
            compare_combat("combat", a->m_combat, b->m_combat, 0xE4);
        }
        same_words(what, o, r, 0x4D0);
        same_words("level manager", g_level[0], g_level[1], 0x80);
        owner->stats.compare("stats");
    }
    ~Pair() { g_twins.clear(); delete owner; }
};

// ---- the character's sight query set-up ---------------------------------------------------------------------------------------------
static void run_sight_setup() {
    static float measures[2][4] = {{12.0f, 30.0f, 15.0f, 2.0f}, {-1.0f, 0.0f, 90.0f, bitsf(0x7FC00000u)}};
    static float words[4] = {500.0f, 40.0f, 10.0f, 1.5f};
    const AtVector3 at3 = {7.0f, 8.0f, 9.0f}, way = {0.6f, 0.8f, 0.0f};
    for (int k = 0; k < 256; k++) {
        g_context = "sight setup " + std::to_string(k);
        Pair p(k);
        g_node->words[0] = (k & 1) ? AtSceneNode::kMoved : 0x1;   // m_flags at +0x04
        g_node->words[6] = fbits(1.25f);    // m_worldSphere.center at +0x1C
        g_node->words[7] = fbits(-2.5f);
        g_node->words[8] = fbits(3.75f);
        p.set<float>(0x18, 0.25f);
        p.set<float>(0x1C, 0.5f);
        p.set<float>(0x20, -0.75f);
        for (int i = 0; i < 4; i++) p.set<float*>(0x2E8 + 4 * i, ((k >> (2 + i)) & 1) ? &words[i] : nullptr);
        const int given = (k >> 6) & 3;   // 0 none, 1 each from the first set, 2 from the second, 3 some
        const float* m[4];
        for (int i = 0; i < 4; i++) m[i] = given == 0 ? nullptr : given == 3 ? ((k >> i) & 1 ? &measures[0][i] : nullptr) : &measures[given - 1][i];
        const AtVector3* pos = (k & 2) ? &at3 : nullptr;
        const AtVector3* dir = (k & 4) ? &way : nullptr;
        void* qo = fresh(0x2C);
        void* qr = fresh(0x2C);
        both("sight setup", [&] { at<Fn7>(0x00130BF0)(p.o, nullptr, U(qo), U(pos), U(dir), U(m[0]), U(m[1]), U(m[2]), U(m[3])); },
             [&] { p.F()->SetupSightQuery((GmAcSightQuery*)qr, pos, dir, m[0], m[1], m[2], m[3]); });
        same_words("sight query", qo, qr, 0x2C);
        p.compare("player");
        std::free(qo);
        std::free(qr);
        g_cases++;
    }
}

// ---- FaAcPlayer's slot 15 ---------------------------------------------------------------------------------------------------------
static void run_slot15() {
    static float sightWords[4] = {800.0f, 35.0f, 12.0f, 0.5f};
    static const char* const kLevels[] = {"Level1", "LEVEL1", "level2", "Nowhere", "", "SavedTim"};
    for (int k = 0; k < 600; k++) {
        g_context = "slot 15 " + std::to_string(k);
        fill_words(7000 + k, k % 5);
        fill_save((uint32_t)k * 7);
        Pair p(k);
        for (int s = 0; s < 2; s++) {
            std::memset(g_level[s], 0x11, 0x80);
            std::memset(g_level[s] + 8, 0, 9);
            std::strcpy((char*)g_level[s] + 8, kLevels[k % 6]);
        }
        const int actor = (k / 6) % 4;   // none, no joints, joints
        *(AtCoordSysDatabase**)(g_actor + 0xA0) = actor == 1 ? nullptr : g_joints;
        p.set<void*>(0xA0, actor == 0 ? nullptr : g_actor);
        set_head((k / 24) % 5);
        g_node->words[0] = (k & 1) ? AtSceneNode::kMoved : 0;
        for (int i = 0; i < 4; i++) p.set<float*>(0x2E8 + 4 * i, ((k >> (1 + i)) & 1) ? &sightWords[i] : nullptr);
        p.set<uint32_t>(0x3D8, 0x3D83D8);
        // Combat is made more often than not, so that its record reads run.
        Word* combat = word("Combat");
        if (k % 4 != 3) { combat->where |= kOwn; combat->own[0] = 1 + k % 2; }
        g_slot21 = 0x2121 + k;
        const bool noHeap = k % 37 == 20;
        const bool failing = k % 41 == 30;
        set_heaps(!noHeap, k % 31 != 12);
        const bool crashed = both("slot 15", [&] { g_fail_alloc = failing; ((Fn0)vptr(p.o)[15])(p.o, nullptr); g_fail_alloc = false; },
                                  [&] { g_fail_alloc = failing; ((Fn0)vptr(p.r)[15])(p.r, nullptr); g_fail_alloc = false; });
        set_heaps(true, true);
        if (crashed) { g_cases++; continue; }
        p.compare("after slot 15");
        if (k % 2 == 0) {
            // Given back: the shutdown deletes the combat module through its vftable. The actor's shutdown deletes its stats through theirs, which here are the harness's: let go of first.
            p.set<void*>(0x270, nullptr);
            both("destroy", [&] { at<Fn0>(0x00094430)(p.o, nullptr); }, [&] { p.F()->FaAcPlayer::~FaAcPlayer(); });
            same_words("destroyed", p.o, p.r, 0x4D0);
        }
        g_cases++;
    }
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (getenv("PLSETUP_TRACE")) std::printf("crash at 0x%p eax=%08X ecx=%08X esi=%08X [%s]\n", e->ExceptionRecord->ExceptionAddress, (unsigned)e->ContextRecord->Eax, (unsigned)e->ContextRecord->Ecx, (unsigned)e->ContextRecord->Esi, g_context.c_str());
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
    if (pos.size() < 1) { std::printf("usage: oracle_plsetup <default.xbe> [cases dir] [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();
    init_fakes();
    pair_vftables();
    for (int i = 0; i < GmLinkedListNode::kPoolSize; i++) at<Fn0>((uint32_t)0x00076D20)((void*)(uintptr_t)(0x002E26F0 + 0x20 * i), nullptr);

    // The seams and the slots these call directly that are compared elsewhere or not recovered: the original's address and the recovered function, to the same double.
    {
        GmAcCharacter* c = new GmAcCharacter;
        FaCmCombat* f = new FaCmCombat;
        redirect(0x000679E0, (const void*)o_find);
        redirect(U((void*)&AtDataRegistryFind), (const void*)r_find);
        redirect(0x00130950, (const void*)d_char15);
        redirect(U(vptr(c)[15]), (const void*)d_char15);
        redirect(0x000A4BB0, (const void*)d_update);
        redirect(U(vptr(f)[6]), (const void*)d_update);
    }

    // The statics this code reads.
    static int heap_token;
    for (uint32_t va : {0x002C632Cu, 0x002C6274u, 0x002C7F58u, 0x002C7F64u, 0x002C6344u}) *(void**)(uintptr_t)va = &heap_token;
    GmNameMap::s_heap = &heap_token;
    GmNameSpaceDb::s_heap = &heap_token;
    AtSegmentHitList::s_heap = &heap_token;
    AtQueryPool::s_heap = &heap_token;
    AtCoordSysDatabase::s_heap = &heap_token;
    set_heaps(true, true);
    init_registry();
    at<FnP0>(0x00057B90)((void*)(uintptr_t)0x003B39A0, nullptr);   // the sight queries' shared query: the original's static initializer 0x00202460, without its atexit
    // The actor's shutdown looks "Motion" up in the actor's name map: made on both sides.
    at<FnP0>(0x00024C60)((void*)(uintptr_t)0x002CD124, nullptr);
    at<Fn1>(0x00024DA0)((void*)(uintptr_t)0x002CD124, nullptr, 8);
    at<Fn2>(0x00024EE0)((void*)(uintptr_t)0x002CD124, nullptr, (uintptr_t)"Motion", 0x4D4D);
    GmAcActor::s_namedMaps.Init(8);
    GmAcActor::s_namedMaps.Insert("Motion", (void*)0x4D4D);
    for (int s = 0; s < 2; s++) g_level[s] = (unsigned char*)std::calloc(1, 0x600);
    *(void**)(uintptr_t)0x003B3570 = g_level[0];
    GmLevelManager::s_instance = (GmLevelManager*)g_level[1];
    *(void**)(uintptr_t)0x00305A48 = nullptr;
    GmSavedGame::s_instance = nullptr;
    g_fill = 0xCD;
    if (entries && !arm_entries(entries)) return 2;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_combat_lifetimes();
    run_combat();
    run_lookups();
    run_sight_setup();
    run_slot15();
    report_entries();
    std::printf("%s: %d cases, %d calls crashed alike; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_cases, g_crashed_alike, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
