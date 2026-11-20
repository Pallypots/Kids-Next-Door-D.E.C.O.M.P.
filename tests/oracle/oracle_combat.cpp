// Differential oracle for FaCmCombat's update (slot 6) and what it alone reaches (gm/combat). Windows x86, local only. See docs/verification.md.
//
//   oracle_combat <default.xbe> [--entries FILE] [--mutate]
//
// No file reaches this code, so the cases are combat objects built by each side's own constructor (the original's FaCmCombat or FaCmPlayerCombat at its address, the recovered one by placement
// new), given an owner and a world of actors, and then called directly: each function on its own over many states, and the update over runs of frames that carry its state from one to the next.
// Return values, every word of the combat object, of the actors (their target words above all), of the display, and the log of every call out are compared.
//
// What stands in, and is the same on both sides (so is not tested):
//  * The actors: plain blocks laid out as GmAcCharacter, each side its own copy of one world, with one fake vftable whose slots 4, 6, 22, 24, 27, 32, 42, 49, 53, 55 and 59 log and answer from
//    the case; slot 49 (the events) logs the event's words, and LOCKON? writes the asker's answer word as an enemy's script would. Their scene nodes, aim nodes, controllers, stats (read by the
//    real GmAcStatSet::Find), the record values, enemy lists, attack ranges and weapon sets are plain data in the same world.
//  * The seams, both sides to one double each: the node iterator's Find and Next (a list of actors by name from the case), the sight test, the player's stick and its angle, the attached PCNPC,
//    the aim node, the spawner, the input's button, the node's slot 27 (0x0014C170, gmactor's seam), the registry's search by path (FindComponent beneath it is real).
// Real on both sides: everything recovered here, AtVector3's Distance and Normalize, the name helpers, GmAcStatSet::Find, GmBaObject::FindComponent, the C library's acos (the image's, and the host's).
#define NOMINMAX
#include <windows.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "at/AtScene.h"
#include "at/AtVector3.h"
#include "gm/FaCmCombat.h"
#include "gm/GmAcPlayer.h"
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
int AtPlatformLanguage() { return 0; }
int AtPlatformGameRegion() { return 0; }

static uint32_t U(const void* p) { return (uint32_t)(uintptr_t)p; }
static void** vptr(void* obj) { return *(void***)obj; }
static uint32_t fbits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static float bitsf(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }

// ---- canonical words -------------------------------------------------------------------------------------------------------------
static std::map<uint32_t, uint32_t> g_vft_token;   // original vftable -> token
static std::map<uint32_t, uint32_t> g_host_vft;    // host vftable -> token
struct Twin { uint32_t p[2]; uint32_t size; uint32_t id; };
static std::vector<Twin> g_twins;
static void twin(const void* a, const void* b, uint32_t size, uint32_t id) { g_twins.push_back({{U(a), U(b)}, size, id}); }

static uint32_t canon(int side, uint32_t w) {
    if (side == 0) {
        auto it = g_vft_token.find(w);
        if (it != g_vft_token.end()) return it->second;
    } else {
        auto it = g_host_vft.find(w);
        if (it != g_host_vft.end()) return it->second;
    }
    for (const Twin& t : g_twins) {
        if (w >= t.p[side] && w < t.p[side] + t.size) return 0xD0000000u | (t.id << 16) | (w - t.p[side]);
    }
    return w;
}

static void same_words(const char* what, const void* o, const void* r, uint32_t size) {
    const uint32_t* a = (const uint32_t*)o;
    const uint32_t* b = (const uint32_t*)r;
    for (uint32_t i = 0; i < size / 4; i++) {
        const uint32_t ca = canon(0, a[i]), cb = canon(1, b[i]);
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
static uint32_t hash_name(const char* s, int n = 8) {
    if (!s) return 0xFFFFFFFFu;
    uint32_t h = 2166136261u;
    for (int i = 0; i < n && s[i]; i++) h = (h ^ (unsigned char)s[i]) * 16777619u;
    return h;
}
static void log_(uint32_t a, uint32_t b = 0) { if (g_recording) g_log[g_side].add(a, b); }
static uint32_t C(const void* p) { return canon(g_side, U(p)); }

static uint32_t g_rand;
static uint32_t rnd() { g_rand = g_rand * 1103515245u + 12345u; return (g_rand >> 16) & 0x7FFF; }
static uint32_t pick(uint32_t n) { return rnd() % n; }

// ---- the world: one per side, laid out alike ---------------------------------------------------------------------------------------
enum { kActors = 12, kOwner = 0, kPlayer = 1, kFirstEnemy = 2, kLastEnemy = 8, kFirstWeapon = 9 };
struct FakeDesc { void* vptr; uint32_t w4, w8, type, flags, count; int intFloor, intCeiling; float floor, ceiling; void* format; };   // AtDataDescriptor's layout
struct FakeNames { void* vptr; FakeDesc* desc; void* reg; char (*data)[8]; };    // AtDataShortNameArray: +0x04 descriptor, +0x0C data
struct FakeInts { void* vptr; FakeDesc* desc; void* reg; int* data; };            // AtDataIntArray
struct FakeLink { void* vptr; void* value; };                                     // AtDataLink: +0x04 value
struct FakeNode { void** vptr; uint32_t flags; uint16_t cls; uint16_t pad; float sphere[4]; float world[4]; uint32_t rest[8]; int id; };  // AtSceneNode: +0x1C centre, +0x28 radius

struct World {
    unsigned char actor[kActors][0x400];
    FakeNode node[kActors];
    FakeNode aim[kActors];
    unsigned char tcombat[kActors][0xE4];     // characters' combat modules: only +0xA0 is read
    unsigned char cls[kActors][0x20];         // the actors' +0x04 objects: +0x0C an eight-byte name
    unsigned char controller[0x58];
    GmAcStatsHolder holder[kActors];
    GmAcStatSet set[kActors];
    GmAcStat stat[kActors][2];
    GmAcStat* list[kActors][2];
    float f[40];                              // the record's reals
    int iv[40];                               // the record's integers
    FakeDesc desc[24];
    FakeNames enemies[2];                     // Enemies, and the list used instead (m_unknown78)
    char enemyNames[2][8][8];
    FakeInts priorities[2];
    int priorityData[2][8];
    FaCmAttackRange ranges[4];
    FakeNames rangeValue[4];                  // only the descriptor is read
    char events[4][8];
    FaCmWeaponSet sets[3];
    GmCmWeapon* weapons[3][4];
    FakeLink setLinks[3];
    FakeNames setValues[3];
    char weaponNames[3][4][8];
    GmAcActor* cells[32];                     // what the iterator's items point at
    unsigned char hud[0x12A0];
    int spawned;
    GmAcStat combatHealth;                    // the combat module's own health, for slot 7
};
static World* g_world[2];
static unsigned char* g_combat[2];            // the combat objects

// The case: what both worlds are built from, and what the doubles answer.
struct Actor {
    uint32_t flags;
    int node27, slot27, slot42, slot55, slot59;
    float pos[3], facing[3], start[3];
    int typeName, ownName, className, nameId;
    int target;              // -1 none, -2 a word without the actor flag, else an index
    int stats;               // 0 none, 1 no health, 2 health
    float health;
    int combat;              // 0 none, else the A0 word + 1
    int aim;                 // 0 none, 1 an aim node
    float aimPos[3];
    int sees, seesWrite, lockAnswer;
    float radius;
    int nodeMoved;
};
struct Case {
    Actor a[kActors];
    int pcnpc;               // 0 none, else an actor index
    int button[2];           // the input's state for each lookup
    float stick[2][2];
    float stickAngle[4];
    int nameList[8][6];      // for each enemy type name: actor indices (-1 ends, -2 a null item)
    int zapList[6];
    int spawnNull;
    int exact;               // positions on whole numbers, enemies at distance 500 from the owner, ranges of 500: the comparisons' equal cases
} static g_case;
static bool g_dense;         // every enemy enabled, in reach, answering and seen: the choosing's ties and priorities
static int g_stickCalls[2], g_angleCalls[2], g_buttonCalls[2];

static const char* const kTypes[] = {"Robot", "robot", "Ninja", "Pirate", "Delightf", "DELIGHTF", "Ro", "", "ZapLight", "Robot1x"};
static const char* const kNames[] = {"Alpha", "Bravo", "Charlie", "Delta"};
static const char* const kWeaponTypes[] = {"Blaster", "", "Mustard", "Sword"};

static void* g_vt_actor[72];
static void* g_vt_node[8];

static GmAcActor* actor_ptr(int side, int i) { return (GmAcActor*)g_world[side]->actor[i]; }
static int actor_index(const void* p) {
    for (int i = 0; i < kActors; i++) if (p == g_world[g_side]->actor[i]) return i;
    return -1;
}
template <class T> static T& at_off(void* base, uint32_t off) { return *(T*)((char*)base + off); }

// ---- the fake actors' slots ---------------------------------------------------------------------------------------------------------
static void __fastcall a_slot4(void* self, void*) { log_('4', C(self)); }
static void __fastcall a_slot32(void* self, void*) { log_('3' << 8 | '2', C(self)); }
static const char* __fastcall a_name(void* self, void*) {
    log_('6', C(self));
    const int i = actor_index(self);
    return i < 0 ? "?" : kNames[g_case.a[i].nameId % 4];
}
static void __fastcall a_slot22(void* self, void*, float* a, float* b, void* c, void* d, void* e) {
    log_('2' << 8 | '2', C(self));
    log_('v', (a ? 1 : 0) | (b ? 2 : 0) | (c ? 4 : 0) | (d ? 8 : 0) | (e ? 16 : 0));
    const int i = actor_index(self);
    if (i < 0) return;
    if (a) std::memcpy(a, g_case.a[i].pos, 12);
    if (b) std::memcpy(b, g_case.a[i].facing, 12);
}
static void __fastcall a_slot24(void* self, void*, float* a, int b, int c, int d, int e) {
    log_('2' << 8 | '4', C(self));
    log_('v', b | c | d | e);
    const int i = actor_index(self);
    if (i >= 0 && a) std::memcpy(a, g_case.a[i].start, 12);
}
static int __fastcall a_slot27(void* self, void*) { log_('2' << 8 | '7', C(self)); const int i = actor_index(self); return i < 0 ? 1 : g_case.a[i].slot27; }
static int __fastcall a_slot42(void* self, void*) { log_('4' << 8 | '2', C(self)); const int i = actor_index(self); return i < 0 ? 0 : g_case.a[i].slot42; }
static int __fastcall a_slot55(void* self, void*) { log_('5' << 8 | '5', C(self)); const int i = actor_index(self); return i < 0 ? 1 : g_case.a[i].slot55; }
static int __fastcall a_slot59(void* self, void*) { log_('5' << 8 | '9', C(self)); const int i = actor_index(self); return i < 0 ? 0 : g_case.a[i].slot59; }
static uint32_t __fastcall a_slot53(void* self, void*) { log_('5' << 8 | '3', C(self)); return at_off<uint32_t>(self, 0x17C); }
static void __fastcall a_slot49(void* self, void*, const char* event, int b, int c) {
    log_('E', C(self));
    log_('e', hash_name(event));
    log_('c', c);
    if (b != 0) {
        const uint32_t* words = (const uint32_t*)(uintptr_t)b;
        log_('w', canon(g_side, words[0]));
        for (int k = 1; k < 8; k++) log_('w', canon(g_side, words[k]));
        log_('n', hash_name((const char*)(words + 8)));
    }
    // An enemy's script answers LOCKON? by setting the asker's word.
    const int i = actor_index(self);
    if (i >= 0 && hash_name(event) == hash_name("LOCKON?") && g_case.a[i].lockAnswer != 2) at_off<int>(g_combat[g_side], 0xA8) = g_case.a[i].lockAnswer;
}
static void __fastcall n_finish(FakeNode* self, void*) { log_('S', C(self)); self->flags &= ~4u; self->world[2] += 0.25f; }

// The node's slot 27 called directly (0x0014C170, a seam).
static int __fastcall d_node27(void* self, void*) { log_('N', C(self)); const int i = actor_index(self); return i < 0 ? 1 : g_case.a[i].node27; }

// ---- the seams' doubles -----------------------------------------------------------------------------------------------------------------
static int list_for(const char* name) {
    for (int t = 0; t < 8; t++) if (std::strncmp(name, kTypes[t], 8) == 0) return t;
    return -1;
}
// The iterator: m_bucket holds the list and position, m_entry is set while there is one, m_item points at the world's cell for it.
static void iter_load(GmDynamicNodeIterator* it) {
    const int code = (int)(uintptr_t)it->m_bucket;
    const int list = code >> 8, pos = code & 0xFF;
    const int* l = list == 9 ? g_case.zapList : g_case.nameList[list];
    if (list < 0 || pos >= 6 || l[pos] == -1) { it->m_entry = nullptr; it->m_item = nullptr; return; }
    it->m_entry = (void*)(uintptr_t)0x0E000000u;
    World* w = g_world[g_side];
    const int cell = (list == 9 ? 8 : list) * 3 + (pos % 3);
    w->cells[cell] = l[pos] == -2 ? nullptr : actor_ptr(g_side, l[pos]);
    it->m_item = (pos % 4 == 3 && l[pos] == -2) ? nullptr : &w->cells[cell];
}
static void __fastcall d_find(GmDynamicNodeIterator* it, void*, const char* name) {
    log_('I', hash_name(name));
    log_('i', C(it->m_loader));
    const int list = name && std::strncmp(name, "ZapLight", 8) == 0 ? 9 : list_for(name ? name : "");
    it->m_bucket = (void*)(uintptr_t)(list < 0 ? 0xFF00u : (unsigned)(list << 8));
    if (list < 0) { it->m_entry = nullptr; it->m_item = nullptr; return; }
    iter_load(it);
}
static void __fastcall d_next(GmDynamicNodeIterator* it, void*) {
    log_('J');
    it->m_bucket = (void*)((uintptr_t)it->m_bucket + 1);
    iter_load(it);
}
static int see_impl(void* self, void* target, int a, int b, int c, float* d, int e, int f, int g, int h, int i) {
    log_('V', C(self));
    log_('t', C(target));
    log_('a', a | b << 4 | c << 8 | e << 12 | f << 16 | g << 20 | h << 24 | i << 28);
    log_('d', d ? fbits(*d) : 0xDDDD);
    const int k = actor_index(target);
    if (k < 0) return 1;
    if (d && g_case.a[k].seesWrite) *d = *d * 0.5f;
    return g_case.a[k].sees;
}
static int __fastcall o_see(void* self, void*, void* target, int a, int b, int c, float* d, int e, int f, int g, int h, int i) { return see_impl(self, target, a, b, c, d, e, f, g, h, i); }
static int r_see(GmAcCharacter* self, GmAcActor* target, int a, int b, int c, float* d, int e, int f, int g, int h, int i) { return see_impl(self, target, a, b, c, d, e, f, g, h, i); }
static void stick_impl(void* player, float* out, int x, int y) {
    log_('K', C(player));
    log_('k', x << 16 | y);
    const int n = g_stickCalls[g_side]++ % 2;
    out[0] = g_case.stick[n][0];
    out[1] = g_case.stick[n][1];
}
static void __fastcall o_stick(void* player, void*, float* out, int x, int y) { stick_impl(player, out, x, y); }
static void r_stick(GmAcActor* player, float* out, int x, int y) { stick_impl(player, out, x, y); }
static float angle_impl(void* player, int u, int x, int y) {
    log_('L', C(player));
    log_('l', u << 24 | x << 12 | y);
    return g_case.stickAngle[g_angleCalls[g_side]++ % 4];
}
static float __fastcall o_angle(void* player, void*, int u, int x, int y) { return angle_impl(player, u, x, y); }
static float r_angle(GmAcActor* player, int u, int x, int y) { return angle_impl(player, u, x, y); }
static void* __cdecl d_attached(void* actor, const char* name) {
    log_('P', C(actor));
    log_('p', hash_name(name));
    return g_case.pcnpc ? (void*)actor_ptr(g_side, g_case.pcnpc) : nullptr;
}
static void* aim_impl(void* node) {
    log_('A', C(node));
    const int i = actor_index(node);
    return i >= 0 && g_case.a[i].aim ? &g_world[g_side]->aim[i] : nullptr;
}
static void* __fastcall o_aim(void* node, void*) { return aim_impl(node); }
static void* r_aim(GmAcNode* node) { return aim_impl(node); }
static void* __fastcall d_spawn(void* self, void*, const char* name) {
    log_('W', C(self));
    log_('w', hash_name(name));
    World* w = g_world[g_side];
    if (g_case.spawnNull && (w->spawned & 1)) { w->spawned++; return nullptr; }
    const int i = kFirstWeapon + (w->spawned++ % (kActors - kFirstWeapon));
    return actor_ptr(g_side, i);
}
static GmButtonState* __fastcall d_button(void* self, void*, GmButtonState* out, int controller, const char* name) {
    log_('B', C(self));
    log_('b', controller);
    log_('h', hash_name(name, 32));
    out->m_state = g_case.button[g_buttonCalls[g_side]++ % 2];
    out->m_unknown04 = 0x77;
    return out;
}
// The registry's search by path, beneath FindComponent: the weapon sets' values by their names.
static void* find_impl(void* registry, void* under, const char* path, int parents) {
    log_('F', C(registry));
    log_('f', C(under));
    log_('f', hash_name(path, 32));
    log_('f', parents);
    World* w = g_world[g_side];
    for (int s = 0; s < 3; s++) if (std::strncmp(path, w->sets[s].m_name, 8) == 0 && w->setLinks[s].value) return &w->setLinks[s];
    return nullptr;
}
static void* __fastcall o_find(void* registry, void*, void* under, const char* path, int parents) { return find_impl(registry, under, path, parents); }
static AtDataLink* r_find(AtDataRegistry* registry, AtDataLink* under, const char* path, int parents) { return (AtDataLink*)find_impl(registry, under, path, parents); }

// ---- building the case and the worlds -----------------------------------------------------------------------------------------------
static float pick_float(float lo, float hi) { return lo + (hi - lo) * (float)pick(1001) / 1000.0f; }
static const float kOdd[] = {0.0f, -0.0f, 1.0f, -1.0f, 1e-30f, 3.0e38f};

static void make_case(uint32_t seed, int density) {
    g_rand = seed * 2654435761u + 7;
    Case& c = g_case;
    std::memset(&c, 0, sizeof c);
    for (int i = 0; i < kActors; i++) {
        Actor& a = c.a[i];
        static const uint32_t flags[] = {0x4, 0x6104, 0x2004, 0x404, 0x0, 0x6504, 0x104, 0x4004};
        a.flags = flags[pick(8)];
        if (i == kOwner) a.flags = pick(4) ? 0x2004 : 0x2104;
        a.node27 = pick(9) ? 1 : 0;
        a.slot27 = pick(9) ? 1 : 0;
        a.slot42 = pick(3) ? 1 : 0;
        a.slot55 = pick(6) ? 1 : 0;
        a.slot59 = (int)pick(4) - 1;
        const float spread = density == 0 ? 300.0f : density == 1 ? 1200.0f : 2000.0f;
        for (int k = 0; k < 3; k++) {
            a.pos[k] = pick_float(-spread, spread);
            a.start[k] = pick_float(-spread, spread);
            a.aimPos[k] = pick_float(-spread, spread);
        }
        if (pick(5) == 0) a.pos[2] = 0.0f;
        if (pick(9) == 0) a.pos[0] = bitsf(0x7FC00000u);
        const float yaw = pick_float(0.0f, 6.28f);
        a.facing[0] = (float)std::cos(yaw);
        a.facing[1] = (float)std::sin(yaw);
        a.facing[2] = pick(3) ? 0.0f : pick_float(-0.5f, 0.5f);
        a.typeName = pick(12) == 0 ? -1 : (int)pick(10);
        a.ownName = pick(4) == 0 ? (int)pick(10) : -1;
        a.className = (int)pick(10);
        a.nameId = (int)pick(4);
        const uint32_t t = pick(8);
        a.target = t < 3 ? -1 : t == 3 ? -2 : (int)pick(kFirstWeapon);
        a.stats = (int)pick(3);
        a.health = pick(4) ? pick_float(-5.0f, 50.0f) : kOdd[pick(6)];
        if (pick(13) == 0) a.health = bitsf(0x7FC00000u);
        a.combat = (int)pick(3);
        a.aim = pick(3) == 0 ? 1 : 0;
        a.sees = pick(3) ? 1 : 0;
        a.seesWrite = pick(4) == 0;
        a.lockAnswer = pick(4) ? 1 : (int)pick(3) == 0 ? 2 : 0;
        a.radius = pick_float(0.0f, spread * 1.5f);
        a.nodeMoved = pick(3) == 0;
    }
    // The owner's type and class name are like some enemies', for the sibling test.
    c.a[kOwner].nameId = (int)pick(4);
    c.pcnpc = pick(3) ? kFirstEnemy + (int)pick(4) : 0;
    c.button[0] = (int)pick(5);
    c.button[1] = (int)pick(5);
    for (int n = 0; n < 2; n++) {
        const float m = pick(5) ? pick_float(0.0f, 1.2f) : (pick(2) ? 0.2f : bitsf(0x7FC00000u));
        const float th = pick_float(0.0f, 6.28f);
        c.stick[n][0] = m * (float)std::cos(th);
        c.stick[n][1] = m * (float)std::sin(th);
    }
    static const float angles[] = {45.0f, 90.0f, 135.0f, 270.0f, 225.0f, 315.0f, 0.0f, 359.0f};
    for (int n = 0; n < 4; n++) c.stickAngle[n] = pick(3) ? pick_float(0.0f, 360.0f) : angles[pick(8)];
    for (int t = 0; t < 8; t++) {
        const int len = (int)pick(5);
        for (int k = 0; k < 6; k++) c.nameList[t][k] = -1;
        for (int k = 0; k < len; k++) {
            const uint32_t r = pick(10);
            c.nameList[t][k] = r == 0 ? -2 : r == 1 ? kOwner : r == 2 ? kPlayer : kFirstEnemy + (int)pick(kLastEnemy - kFirstEnemy + 1);
        }
    }
    for (int k = 0; k < 6; k++) c.zapList[k] = -1;
    const int zaps = (int)pick(4);
    for (int k = 0; k < zaps; k++) c.zapList[k] = pick(8) == 0 ? -2 : kFirstEnemy + (int)pick(7);
    c.spawnNull = pick(3) == 0;
    if (g_dense) {
        for (int i = kFirstEnemy; i <= kLastEnemy; i++) {
            Actor& a = c.a[i];
            a.flags = pick(4) ? 0x4 : 0x404;
            a.slot27 = a.slot55 = 1;
            a.sees = pick(8) ? 1 : 0;
            a.lockAnswer = pick(8) ? 1 : 2;
            a.pos[2] = c.a[kOwner].pos[2];
        }
        for (int t = 0; t < 8; t++) {
            for (int k = 0; k < 6; k++) c.nameList[t][k] = k < 4 ? kFirstEnemy + (int)pick(kLastEnemy - kFirstEnemy + 1) : -1;
        }
    }
    c.exact = g_dense || pick(2) == 0;
    if (c.exact) {
        // Whole-number positions; enemies at 500 from the owner (Pythagorean offsets); ZapLight radii of exactly the distance or the float just above it.
        static const float offsets[][3] = {{300, 400, 0}, {400, 300, 0}, {-300, 400, 0}, {0, 500, 0}, {500, 0, 0}, {300, 0, 400}, {-400, -300, 0}, {0, -500, 0}};
        Actor& o = c.a[kOwner];
        for (int k = 0; k < 3; k++) o.pos[k] = (float)(int)(o.pos[k] == o.pos[k] ? o.pos[k] : 7.0f);
        o.nodeMoved = 0;
        if (pick(3) == 0) { o.facing[0] = 0.0f; o.facing[1] = 1.0f; o.facing[2] = 0.0f; }
        for (int i = kFirstEnemy; i <= kLastEnemy; i++) {
            Actor& a = c.a[i];
            if (pick(3) == 0) continue;
            const float* d = offsets[pick(8)];
            for (int k = 0; k < 3; k++) a.pos[k] = o.pos[k] + d[k];
            a.aim = 0;
            a.nodeMoved = 0;
            if (pick(2)) {
                a.radius = 500.0f;
            } else {
                const double dx = (double)a.pos[0] - o.pos[0], dy = (double)a.pos[1] - o.pos[1], dz = (double)a.pos[2] - o.pos[2];
                a.pos[0] += 0.37f;
                const double e = std::sqrt((dx + 0.37) * (dx + 0.37) + dy * dy + dz * dz);
                (void)e;
                const double ex = std::sqrt(((double)a.pos[0] - o.pos[0]) * ((double)a.pos[0] - o.pos[0]) + dy * dy + dz * dz);
                float r = (float)ex;
                if (!((double)r > ex)) r = std::nextafter(r, 3.0e38f);
                a.radius = r;
            }
        }
    }
}

// Pointers to the record's values: null, or one of the world's reals or integers.
static void* rec_real(World* w, int slot, float v, int absent) { if (absent) return nullptr; w->f[slot] = v; return &w->f[slot]; }
static void* rec_int(World* w, int slot, int v, int absent) { if (absent) return nullptr; w->iv[slot] = v; return &w->iv[slot]; }

static void build_world(int side, uint32_t seed) {
    World* w = g_world[side];
    std::memset(w, 0, sizeof *w);
    const Case& c = g_case;
    for (int i = 0; i < kActors; i++) {
        const Actor& a = c.a[i];
        unsigned char* b = w->actor[i];
        std::memset(b, 0xCD, 0x400);
        at_off<void*>(b, 0) = g_vt_actor;
        at_off<void*>(b, 0x04) = w->cls[i];
        std::memset(w->cls[i], 0, 0x20);
        std::strncpy((char*)w->cls[i] + 0x0C, kTypes[a.className], 8);
        if (i % 3 == 0) std::strncpy((char*)w->cls[i] + 0x0C, kTypes[c.a[kOwner].className], 8);
        at_off<void*>(b, 0x10) = (void*)(uintptr_t)0x5EC00000u;    // the registry
        at_off<void*>(b, 0x14) = (void*)(uintptr_t)0x5EC10000u;    // the record
        std::memset(b + 0x90, 0, 8);
        std::strncpy((char*)b + 0x90, kNames[(i + a.nameId) % 4], 8);
        at_off<uint32_t>(b, 0x98) = a.flags;
        FakeNode* n = &w->node[i];
        n->vptr = g_vt_node;
        n->flags = a.nodeMoved ? 4u : 0u;
        n->id = i;
        std::memcpy(n->world, a.pos, 12);
        n->world[3] = a.radius;
        at_off<void*>(b, 0x9C) = n;
        FakeNode* m = &w->aim[i];
        m->vptr = g_vt_node;
        m->flags = (i & 1) ? 4u : 0u;
        std::memcpy(m->world, a.aimPos, 12);
        at_off<uint32_t>(b, 0x17C) = 0;
        at_off<void*>(b, 0x240) = a.typeName < 0 ? nullptr : (void*)kTypes[a.typeName];
        std::memset(b + 0x264, 0, 8);
        if (a.ownName >= 0) std::strncpy((char*)b + 0x264, kTypes[a.ownName], 8);
        if (a.stats) {
            GmAcStat* s = w->stat[i];
            std::memcpy(s[0].m_name, "sArmour", 7);
            std::memcpy(s[1].m_name, a.stats == 2 ? "sHealth" : "sSpeed", 7);
            s[1].m_value = a.health;
            w->list[i][0] = &s[0];
            w->list[i][1] = &s[1];
            w->set[i].m_count = 2;
            w->set[i].m_stats = w->list[i];
            w->holder[i].m_stats = &w->set[i];
            at_off<void*>(b, 0x270) = &w->holder[i];
        } else {
            at_off<void*>(b, 0x270) = nullptr;
        }
        at_off<void*>(b, 0x2DC) = nullptr;
        if (a.combat) {
            std::memset(w->tcombat[i], 0, 0xE4);
            at_off<int>(w->tcombat[i], 0xA0) = a.combat - 1;
            at_off<void*>(b, 0x2E4) = w->tcombat[i];
        } else {
            at_off<void*>(b, 0x2E4) = nullptr;
        }
        at_off<int>(b, 0x3A8) = (int)((seed >> (i % 8)) & 1);
    }
    for (int i = 0; i < kActors; i++) {
        const int t = c.a[i].target;
        at_off<uint32_t>(w->actor[i], 0x17C) = t == -1 ? 0 : t == -2 ? U(w->actor[kActors - 1]) : U(w->actor[t]);
    }
    at_off<uint32_t>(w->actor[kActors - 1], 0x98) = 0;   // the word without the actor flag
    // The owner's controller: its owner is the player block.
    at_off<void*>(w->controller, 0x08) = w->actor[kPlayer];
    if (c.a[kOwner].slot42 || (seed & 3) != 0) at_off<void*>(w->actor[kOwner], 0x2DC) = w->controller;
    // The lists.
    for (int l = 0; l < 2; l++) {
        FakeDesc* d = &w->desc[l];
        d->count = (seed >> (l * 3)) % 6;
        for (int k = 0; k < 8; k++) std::strncpy(w->enemyNames[l][k], kTypes[(seed / 7 + k * (l + 1)) % 8], 8);
        w->enemies[l].desc = d;
        w->enemies[l].data = w->enemyNames[l];
        FakeDesc* p = &w->desc[2 + l];
        p->count = d->count;
        for (int k = 0; k < 8; k++) w->priorityData[l][k] = (int)((seed >> k) % 4) - 1 + l;
        w->priorities[l].desc = p;
        w->priorities[l].data = w->priorityData[l];
    }
    for (int r = 0; r < 4; r++) {
        FakeDesc* d = &w->desc[4 + r];
        d->floor = 200.0f * (float)(4 - r) + (float)((seed >> r) % 3) * 50.0f;
        d->ceiling = d->floor + (float)((seed >> (r + 4)) % 3) * 100.0f;
        if (r == 3 && (seed & 64)) d->floor = bitsf(0x7FC00000u);
        if (c.exact && ((seed >> (r + 16)) & 1)) d->floor = 500.0f;
        if (c.exact && ((seed >> (r + 20)) & 3) == 0) d->ceiling = 500.0f;
        w->rangeValue[r].desc = d;
        w->ranges[r].m_range = (AtDataComponent*)&w->rangeValue[r];
        std::snprintf(w->events[r], 8, "EV%d", r);
        w->ranges[r].m_enterEvent = ((seed >> (r + 8)) & 3) ? w->events[r] : nullptr;
        w->ranges[r].m_exitEvent = nullptr;
    }
    for (int s = 0; s < 3; s++) {
        FaCmWeaponSet* set = &w->sets[s];
        std::memset(set->m_name, 0, 8);
        std::strncpy(set->m_name, s == 0 ? "Hands" : s == 1 ? "Gun" : "Spare", 8);
        set->m_count = (seed >> (s * 2)) % 4;
        set->m_weapons = w->weapons[s];
        for (int k = 0; k < 4; k++) {
            w->weapons[s][k] = ((seed >> (s + k + 3)) % 4 == 0) ? (GmCmWeapon*)w->actor[kFirstWeapon] : nullptr;
            std::strncpy(w->weaponNames[s][k], kWeaponTypes[(seed / 5 + s + k) % 4], 8);
        }
        FakeDesc* d = &w->desc[8 + s];
        d->count = 4;
        w->setValues[s].desc = d;
        w->setValues[s].data = w->weaponNames[s];
        w->setLinks[s].value = (seed >> (s + 12)) & 3 ? &w->setValues[s] : nullptr;
    }
}

// The combat object's words: the record's values from the case, its state words from the seed. The same on both sides.
static void fill_combat(int side, uint32_t seed, int density) {
    World* w = g_world[side];
    FaCmCombat* o = (FaCmCombat*)g_combat[side];
    g_rand = seed * 40503u + 11;
    o->m_owner = actor_ptr(side, kOwner);
    o->m_unknown08 = pick(4) == 0;
    o->m_enemies = pick(9) ? (AtDataComponent*)&w->enemies[0] : nullptr;
    o->m_unknown78 = pick(5) == 0 ? (int)U(&w->enemies[1]) : 0;
    o->m_enemyPriorities = pick(3) ? (AtDataComponent*)&w->priorities[0] : nullptr;
    o->m_unknown7C = pick(5) == 0 ? (int)U(&w->priorities[1]) : 0;
    o->m_useEnemyPriorities = rec_int(w, 0, (int)pick(2), pick(4) == 0);
    const float scale = density == 0 ? 0.5f : 1.0f;
    o->m_threatByRadius = rec_int(w, 1, (int)pick(2), pick(3) == 0);
    o->m_threatBySightCone = rec_int(w, 2, (int)pick(2), pick(3) == 0);
    o->m_threatEnterRange = rec_real(w, 0, pick_float(100.0f, 1500.0f) * scale, pick(3) == 0);
    o->m_threatExitRange = rec_real(w, 1, pick_float(100.0f, 2000.0f) * scale, pick(3) == 0);
    o->m_unknown2C = pick(6) == 0 ? (int)U(rec_real(w, 2, pick_float(100.0f, 1500.0f), 0)) : 0;
    o->m_unknown30 = pick(6) == 0 ? (int)U(rec_real(w, 3, pick_float(100.0f, 1500.0f), 0)) : 0;
    o->m_lockOnMinRangeH = rec_real(w, 4, pick_float(0.0f, 400.0f), pick(3) == 0);
    o->m_lockOnMaxRange = rec_real(w, 5, pick_float(200.0f, 2500.0f), pick(3) == 0);
    o->m_lockOffMinRangeH = rec_real(w, 6, pick_float(0.0f, 300.0f), pick(3) == 0);
    o->m_lockOffMaxRange = rec_real(w, 7, pick_float(200.0f, 2500.0f), pick(3) == 0);
    o->m_maxNewLockOnAngleH = rec_real(w, 8, 30.0f, pick(2) == 0);
    o->m_maxNextLockOnAngleH = rec_real(w, 9, pick_float(5.0f, 180.0f), pick(3) == 0);
    o->m_maxAimAngleH = rec_real(w, 10, pick_float(5.0f, 180.0f), pick(3) == 0);
    o->m_maxAimAngleV = rec_real(w, 11, pick_float(1.0f, 90.0f), pick(3) == 0);
    o->m_canAttackSiblings = rec_int(w, 3, (int)pick(2), pick(3) == 0);
    o->m_useStartPositionForAttackRange = rec_int(w, 4, (int)pick(2), pick(3) == 0);
    o->m_minLookStickMagnitude = rec_real(w, 12, pick_float(0.0f, 1.0f), pick(3) == 0);
    o->m_bumpTargetAngle = rec_real(w, 13, pick_float(10.0f, 200.0f), pick(3) == 0);
    o->m_digitalBumpTargetting = rec_int(w, 5, (int)pick(2), pick(3) == 0);
    o->m_analogBumpTargetting = rec_int(w, 6, (int)pick(2), pick(3) == 0);
    o->m_updateCombatModule = rec_int(w, 7, pick(5) ? 1 : 0, pick(2) == 0);
    o->m_attackRangeCount = (int)pick(5);
    o->m_attackRanges = o->m_attackRangeCount ? w->ranges : nullptr;
    o->m_weaponSetList = pick(4) ? w->sets : nullptr;
    o->m_weaponSetCount = o->m_weaponSetList ? 1 + pick(3) : pick(3);
    static const float healths[] = {5.0f, 0.0f, -1.0f, 0.5f};
    w->combatHealth.m_value = pick(7) == 0 ? bitsf(0x7FC00000u) : healths[pick(4)];
    o->m_health = pick(3) == 0 ? &w->combatHealth : nullptr;
    if (g_case.exact) {
        if (pick(3) == 0) o->m_threatEnterRange = rec_real(w, 0, 500.0f, 0);
        if (pick(3) == 0) o->m_threatExitRange = rec_real(w, 1, 500.0f, 0);
        if (pick(3) == 0) o->m_lockOnMaxRange = rec_real(w, 5, 500.0f, 0);
        if (pick(3) == 0) o->m_lockOffMaxRange = rec_real(w, 7, 500.0f, 0);
    }
    if (g_dense) {
        o->m_enemies = (AtDataComponent*)&w->enemies[0];
        w->desc[0].count = 3 + pick(4);
        w->desc[2].count = w->desc[0].count;
        o->m_unknown78 = 0;
        o->m_lockOnMinRangeH = rec_real(w, 4, 0.0f, 0);
        o->m_lockOnMaxRange = rec_real(w, 5, pick(2) ? 2500.0f : 500.0f, 0);
        o->m_maxAimAngleV = rec_real(w, 11, 90.0f, 0);
        if (pick(2)) o->m_useEnemyPriorities = rec_int(w, 0, 1, 0);
    }
    o->m_unknown94 = (int)pick(2);
    o->m_unknown98 = (int)pick(6) - 1;
    o->m_unknown9C = pick(4) ? 1 : 0;
    o->m_unknownA0 = 1;
    o->m_unknownA4 = (int)pick(4) - 1;
    o->m_unknownA8[0] = 0x5A;
    o->m_unknownA8[1] = (int)pick(2);
    static const char* const tags[] = {"", "LOCKL_DN", "LOCKR_DN", "TAGL_DN", "TAGR_DN"};
    std::strncpy((char*)&o->m_unknownB0[2], tags[pick(5)], 8);
    std::strncpy((char*)&o->m_unknownB0[4], tags[pick(5)], 8);
    std::strncpy((char*)&o->m_unknownB0[6], tags[pick(5)], 8);
    o->m_unknownD0 = (int)pick(2);
    o->m_unknownD4[0] = (int)pick(3);
    o->m_unknownD4[1] = (int)pick(2);
    o->m_unknownD4[2] = (int)pick(2);
    o->m_unknownE0 = (int)pick(2);
}

// ---- both sides of one call -----------------------------------------------------------------------------------------------------------
static void x87_clear() {
    unsigned short cw;
    __asm { fnstcw cw }
    __asm { fninit }
    __asm { fldcw cw }
}
template <class FO, class FR> static bool both(const char* what, FO fo, FR fr) {
    g_stickCalls[0] = g_stickCalls[1] = g_angleCalls[0] = g_angleCalls[1] = g_buttonCalls[0] = g_buttonCalls[1] = 0;
    const bool oc = guarded([&] { On on(0); fo(); });
    x87_clear();
    g_recording = false;
    g_side = 0;
    const bool rc = guarded([&] { On on(1); fr(); });
    x87_clear();
    g_recording = false;
    g_side = 0;
    eq((std::string(what) + ": crashed").c_str(), oc, rc);
    if (oc && rc) {
        g_crashed_alike++;
        if (getenv("COMBAT_TRACE")) std::printf("crashed alike: %s [%s]\n", what, g_context.c_str());
    }
    if (getenv("COMBAT_LOGS")) {
        int i = 0;
        while (i < g_log[0].n && i < g_log[1].n && g_log[0].v[i] == g_log[1].v[i]) i++;
        if (i < g_log[0].n || i < g_log[1].n) {
            for (int side = 0; side < 2; side++) {
                for (int j = i > 40 ? (i - 40) & ~1 : 0; j < g_log[side].n && j < i + 20; j += 2) std::printf("%c%X ", (char)g_log[side].v[j], g_log[side].v[j + 1]);
                std::printf("\n");
            }
        }
    }
    compare_logs(what);
    return oc || rc;
}

static void compare_state(const char* what) {
    same_words(what, g_combat[0], g_combat[1], 0xE4);
    for (int i = 0; i < kActors; i++) {
        char text[64];
        sprintf_s(text, "%s actor %d", what, i);
        same_words(text, g_world[0]->actor[i], g_world[1]->actor[i], 0x400);
    }
    same_words("weapons", g_world[0]->weapons, g_world[1]->weapons, sizeof g_world[0]->weapons);
    same_words("nodes", g_world[0]->node, g_world[1]->node, sizeof g_world[0]->node);
    same_words("aim nodes", g_world[0]->aim, g_world[1]->aim, sizeof g_world[0]->aim);
    same_words("display", g_world[0]->hud + 0x1290, g_world[1]->hud + 0x1290, 0x10);
    eq("spawned", g_world[0]->spawned, g_world[1]->spawned);
}

// The original's own constructors: FaCmCombat 0x000A3360, FaCmPlayerCombat 0x000A4D00.
typedef void*(__fastcall* FnP0)(void*, void*);
static void setup(uint32_t seed, int density, bool player) {
    make_case(seed, density);
    for (int s = 0; s < 2; s++) {
        build_world(s, seed);
        std::memset(g_combat[s], 0xCD, 0xE4);
    }
    if (player) {
        at<FnP0>(0x000A4D00)(g_combat[0], nullptr);
        new (g_combat[1]) FaCmPlayerCombat;
    } else {
        at<FnP0>(0x000A3360)(g_combat[0], nullptr);
        new (g_combat[1]) FaCmCombat;
    }
    same_words("constructed", g_combat[0], g_combat[1], 0xE4);
    for (int s = 0; s < 2; s++) fill_combat(s, seed, density);
}

// ---- the calls, original and recovered --------------------------------------------------------------------------------------------------
typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, uintptr_t);
typedef int(__fastcall* FnI1)(void*, void*, uintptr_t);
typedef void(__fastcall* Fn3)(void*, void*, uintptr_t, uintptr_t, uintptr_t);
typedef void(__fastcall* Fn2)(void*, void*, uintptr_t, uintptr_t);
typedef double(__fastcall* FnD0)(void*, void*);
typedef double(__fastcall* FnD1)(void*, void*, uintptr_t);
typedef void(__fastcall* Fn5)(void*, void*, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
typedef void(__fastcall* Fn7)(void*, void*, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
typedef int(__fastcall* FnRange)(void*, void*, float, float, uintptr_t);
typedef int(__fastcall* FnCone)(void*, void*, uintptr_t, float, uintptr_t);
typedef void*(__fastcall* FnChoose)(void*, void*, uintptr_t, uintptr_t, float, uintptr_t);
typedef int(__cdecl* FnCmp2)(const char*, const char*);

static FaCmCombat* R() { return (FaCmCombat*)g_combat[1]; }
static void* O() { return g_combat[0]; }
static int g_cases;

static GmAcActor* pick_actor(int side, uint32_t k) {
    const uint32_t r = k % 12;
    if (r == 11) return nullptr;
    return actor_ptr(side, (int)(r % kActors));
}

// The owner's centre and a target's position as the code will find them (a moved node's body finishing adds 0.25 to its z).
static void geometry(int i, AtVector3* own, AtVector3* position) {
    const Actor& o = g_case.a[kOwner];
    *own = *(const AtVector3*)o.pos;
    if (o.nodeMoved) own->z = (float)((double)own->z + 0.25f);
    const Actor& a = g_case.a[i];
    if (a.aim) {
        *position = *(const AtVector3*)a.aimPos;
        if (i & 1) position->z = (float)((double)position->z + 0.25f);
    } else {
        *position = *(const AtVector3*)a.pos;
    }
}
static float float_above(double v) {
    float f = (float)v;
    if (!((double)f > v)) f = std::nextafter(f, 3.0e38f);
    return f;
}

// Every function on its own, over states.
static void run_direct(int count) {
    for (int k = 0; k < count; k++) {
        const int density = k % 3;
        setup(1000u + (uint32_t)k, density, k % 4 != 1);
        g_context = "direct " + std::to_string(k);
        uintptr_t ro = 0, rr = 0;
        const uint32_t which = k % 23;
        const uint32_t t = (uint32_t)k / 23;
        GmAcActor* to = pick_actor(0, t * 7 + 3);
        GmAcActor* tr = pick_actor(1, t * 7 + 3);
        bool crashed = false;
        switch (which) {
        case 0: crashed = both("update", [&] { ((Fn1)vptr(O())[6])(O(), nullptr, k); }, [&] { ((Fn1)vptr(R())[6])(R(), nullptr, k); }); break;
        case 1: crashed = both("spawn", [&] { at<Fn0>(0x000A2BB0)(O(), nullptr); }, [&] { R()->SpawnWeapons(); }); break;
        case 2: crashed = both("threat", [&] { at<Fn1>(0x000A4170)(O(), nullptr, k & 8); }, [&] { R()->UpdateThreat(k & 8); }); break;
        case 3: crashed = both("target me", [&] { at<Fn0>(0x000A46F0)(O(), nullptr); }, [&] { R()->UpdateTargetMe(); }); break;
        case 4: crashed = both("lock on", [&] { at<Fn0>(0x000A48B0)(O(), nullptr); }, [&] { R()->UpdateLockOn(); }); break;
        case 5: crashed = both("zap", [&] { at<Fn0>(0x000A30D0)(O(), nullptr); }, [&] { R()->UpdateZap(); }); break;
        case 6: crashed = both("lock tag", [&] { at<Fn0>(0x000A24E0)(O(), nullptr); }, [&] { R()->UpdateLockTag(); }); break;
        case 7: {
            char a[12], b[12];
            std::memset(a, 0x55, 12);
            std::memset(b, 0x55, 12);
            crashed = both("lock stick", [&] { at<Fn3>(0x000A23A0)(O(), nullptr, U(a), 0x89 + (k & 1), 0x88); }, [&] { R()->ReadLockStick(b, 0x89 + (k & 1), 0x88); });
            eqs("lock stick tag", std::string(a, 12).c_str(), std::string(b, 12).c_str());
            same_words("lock stick bytes", a, b, 12);
            break;
        }
        case 8: crashed = both("bump tag", [&] { at<Fn0>(0x000A34F0)(O(), nullptr); }, [&] { R()->UpdateBumpTag(); }); break;
        case 9: {
            int ti[2] = {(int)(k & 1), (int)(k & 1)}, rg[2] = {7, 7};
            float best[2];
            best[0] = best[1] = (k & 2) ? 3.40282347e+38f : pick_float(0.0f, 1500.0f);
            const int was = (k >> 2) & 1, old = (int)(t % 6) - 1, nearest = (k >> 3) & 1;
            crashed = both("test threat", [&] { at<Fn7>(0x000A2620)(O(), nullptr, U(to), was, old, U(&ti[0]), U(&rg[0]), U(&best[0]), nearest); },
                           [&] { R()->TestThreat(tr, was, old, &ti[1], &rg[1], &best[1], nearest); });
            eq("is threat", ti[0], ti[1]);
            eq("range", rg[0], rg[1]);
            eq("best", fbits(best[0]), fbits(best[1]));
            break;
        }
        case 10: crashed = both("alive", [&] { ro = at<FnI1>(0x000A2810)(O(), nullptr, U(to)); }, [&] { rr = R()->IsAlive(tr); }); eq("alive", ro, rr); break;
        case 11: {
            const int changed = k & 1, was = (k >> 1) & 1, old = (int)(t % 5) - 1, prio = (int)(t % 3);
            crashed = both("post threat", [&] { at<Fn5>(0x000A28A0)(O(), nullptr, changed, was, old, U(to), prio); }, [&] { R()->PostThreat(changed, was, old, tr, prio); });
            break;
        }
        case 12: {
            float a[4] = {9, 9, 9, 9}, b[4] = {9, 9, 9, 9};
            crashed = both("target position", [&] { at<Fn2>(0x000A2D40)(O(), nullptr, U(a), U(to)); }, [&] { R()->TargetPosition((AtVector3*)b, tr); });
            same_words("position", a, b, 16);
            break;
        }
        case 13: {
            const int listed = k & 1;
            crashed = both("is enemy", [&] { ro = ((int(__fastcall*)(void*, void*, uintptr_t, uintptr_t))(uintptr_t)0x000A3790)(O(), nullptr, U(to), listed); },
                           [&] { rr = R()->IsEnemy(tr, listed); });
            eq("is enemy", ro, rr);
            break;
        }
        case 14: {
            float mn = pick_float(0.0f, 500.0f), mx = (t & 1) ? pick_float(100.0f, 2500.0f) : bitsf(0x7FC00000u);
            const int ti = (int)((t * 7 + 3) % 12);
            if (ti < kActors && (t % 4) >= 2) {
                // A bound exactly on the value: the horizontal distance's float just above it, or the distance itself.
                AtVector3 own, position;
                geometry(ti, &own, &position);
                const double dx = (double)position.x - own.x, dy = (double)position.y - own.y;
                if (t % 4 == 2) { mn = float_above(std::sqrt(dy * dy + dx * dx)); mx = 3.0e38f; }
                else { mn = 0.0f; mx = own.Distance(position); }
            }
            if (t % 9 == 5) mn = bitsf(0x7FC00000u);
            crashed = both("in range", [&] { ro = at<FnRange>(0x000A38E0)(O(), nullptr, mn, mx, U(to)); }, [&] { rr = R()->InRange(mn, mx, tr); });
            eq("in range", ro, rr);
            break;
        }
        case 15: {
            float mx = pick_float(0.0f, 3.2f);
            const int check = (t % 4) == 2 ? 1 : (t % 4) == 3 ? 0 : (int)(t & 1);
            const int ti = (int)((t * 7 + 3) % 12);
            if (ti < kActors && (t % 4) >= 2) {
                // The bounds exactly on the angles: the horizontal one as it is; the vertical limit between the angle and its float.
                AtVector3 own, position, facing = *(const AtVector3*)g_case.a[kOwner].facing;
                geometry(ti, &own, &position);
                AtVector3 d = {(float)((double)position.x - own.x), (float)((double)position.y - own.y), (float)((double)position.z - own.z)};
                d.Normalize();
                AtVector3 level = {d.x, d.y, 0.0f};
                level.Normalize();
                AtVector3 raised = {facing.x, facing.y, d.z};
                raised.Normalize();
                mx = (float)facing.AngleTo(level);
                const double v = facing.AngleTo(raised);
                FaCmCombat* oc = (FaCmCombat*)g_combat[0];
                FaCmCombat* rc = (FaCmCombat*)g_combat[1];
                if (t % 4 == 3 && oc->m_maxAimAngleV && rc->m_maxAimAngleV) {
                    // A limit strictly between the angle and its float, if one is a whole number of steps of the record's value away; else the angle's own degrees.
                    const double fv = (float)v;
                    const float base = (float)(v / 0.0174532924f);
                    float m = base;
                    for (int n = -12; n <= 12; n++) {
                        float c = base;
                        for (int j = 0; j < (n < 0 ? -n : n); j++) c = std::nextafter(c, n < 0 ? 0.0f : 1000.0f);
                        const double lim = (double)c * 0.0174532924f;
                        if ((lim > v && lim <= fv) || (lim < v && lim >= fv)) { m = c; break; }
                    }
                    if (v == 0.0) m = 0.0f;
                    *(float*)oc->m_maxAimAngleV = m;
                    *(float*)rc->m_maxAimAngleV = m;
                }
            }
            crashed = both("aim cone", [&] { ro = at<FnCone>(0x000A39D0)(O(), nullptr, check, mx, U(to)); }, [&] { rr = R()->InAimCone(check, mx, tr); });
            eq("aim cone", ro, rr);
            break;
        }
        case 16: crashed = both("lock-on range", [&] { ro = at<FnI1>(0x000A3B40)(O(), nullptr, U(to)); }, [&] { rr = R()->InLockOnRange(tr); }); eq("lock-on range", ro, rr); break;
        case 17: crashed = both("lock-off range", [&] { ro = at<FnI1>(0x000A3B90)(O(), nullptr, U(to)); }, [&] { rr = R()->InLockOffRange(tr); }); eq("lock-off range", ro, rr); break;
        case 18: case 19: {
            const int side = (int)(t % 3), keep = (int)((t / 3) & 1), fallback = (int)((t / 6) & 1);
            const float mx = pick_float(0.0f, 3.5f);
            void* a = nullptr;
            GmAcActor* b = nullptr;
            crashed = both("choose", [&] { a = at<FnChoose>(0x000A3C30)(O(), nullptr, side, keep, mx, fallback); }, [&] { b = R()->ChooseTarget(side, keep, mx, fallback); });
            if (!crashed) { g_side = 0; const uint32_t ca = C(a); g_side = 1; const uint32_t cb = C(b); g_side = 0; eq("chosen", ca, cb); }
            break;
        }
        case 20: {
            double a = 0, b = 0;
            crashed = both("max aim", [&] { a = at<FnD0>(0x000A1E90)(O(), nullptr); }, [&] { b = R()->MaxAimAngleH(); });
            eq("max aim", fbits((float)a), fbits((float)b));
            eq("max aim double", *(long long*)&a, *(long long*)&b);
            crashed = both("enter", [&] { a = at<FnD0>(0x000A1ED0)(O(), nullptr); }, [&] { b = R()->ThreatEnterRange(); }) || crashed;
            eq("enter", *(long long*)&a, *(long long*)&b);
            crashed = both("exit", [&] { a = at<FnD0>(0x000A1EF0)(O(), nullptr); }, [&] { b = R()->ThreatExitRange(); }) || crashed;
            eq("exit", *(long long*)&a, *(long long*)&b);
            break;
        }
        case 21: {
            char a[8], b[8];
            if (to == nullptr) break;
            crashed = both("note bank", [&] { at<Fn1>(0x000A1E10)(to, nullptr, U(a)); }, [&] { tr->GetNoteBank(b); });
            same_words("note bank", a, b, 8);
            for (int n = 0; n < 10; n++) {
                crashed = both("type", [&] { ro = at<FnI1>(0x000A1E30)(to, nullptr, U(kTypes[n])); }, [&] { rr = tr->IsOfType(kTypes[n]); }) || crashed;
                eq("type", ro, rr);
            }
            break;
        }
        case 22: {
            if (to == nullptr) break;
            crashed = both("target of", [&] { ro = U(((void*(__fastcall*)(void*, void*))(uintptr_t)0x000920D0)(to, nullptr)); }, [&] { rr = U(GmAcTargetOf(tr)); });
            eq("target of", canon(0, (uint32_t)ro), canon(1, (uint32_t)rr));
            break;
        }
        }
        if (!crashed) compare_state("after");
        g_twins.resize(2);
        g_cases++;
    }
}

// The update over runs of frames: each frame's state is the next one's. Some frames move the actors and change the answers between frames, alike on both sides.
static void run_frames(int runs, int frames) {
    for (int k = 0; k < runs; k++) {
        setup(50000u + (uint32_t)k, k % 3, k % 3 != 2);
        for (int f = 0; f < frames; f++) {
            g_context = "frames " + std::to_string(k) + " frame " + std::to_string(f);
            const bool crashed = both("update", [&] { ((Fn1)vptr(O())[6])(O(), nullptr, f); }, [&] { ((Fn1)vptr(R())[6])(R(), nullptr, f); });
            if (crashed) break;
            compare_state("frame");
            // Move things along: the actors, the answers, the sticks; the same on both sides.
            g_rand = (uint32_t)(k * 977 + f * 31 + 5);
            for (int i = 0; i < kActors; i++) {
                Actor& a = g_case.a[i];
                for (int c = 0; c < 3; c++) a.pos[c] += pick_float(-150.0f, 150.0f);
                if (pick(5) == 0) a.lockAnswer ^= 1;
                if (pick(7) == 0) a.sees ^= 1;
                if (pick(9) == 0) a.slot27 ^= 1;
                for (int s = 0; s < 2; s++) std::memcpy(g_world[s]->node[i].world, a.pos, 12);
            }
            for (int n = 0; n < 4; n++) g_case.stickAngle[n] = pick_float(0.0f, 360.0f);
            for (int n = 0; n < 2; n++) { g_case.stick[n][0] = pick_float(-1.0f, 1.0f); g_case.stick[n][1] = pick_float(-1.0f, 1.0f); }
            g_case.button[0] = (int)pick(5);
            g_case.pcnpc = pick(4) ? g_case.pcnpc : 0;
            g_cases++;
        }
        g_twins.resize(2);
    }
}

// ChooseTarget, TargetMe and the lock-on among many candidates that all answer and are seen: ties of distance, priorities.
static void run_dense(int count) {
    g_dense = true;
    for (int k = 0; k < count; k++) {
        setup(90000u + (uint32_t)k, 0, true);
        g_context = "dense " + std::to_string(k);
        const int side = k % 3, keep = (k / 3) & 1, fallback = (k / 6) & 1;
        const float mx = pick_float(0.5f, 6.3f);
        void* a = nullptr;
        GmAcActor* b = nullptr;
        bool crashed = both("dense choose", [&] { a = at<FnChoose>(0x000A3C30)(O(), nullptr, side, keep, mx, fallback); }, [&] { b = R()->ChooseTarget(side, keep, mx, fallback); });
        if (!crashed) { g_side = 0; const uint32_t ca = C(a); g_side = 1; const uint32_t cb = C(b); g_side = 0; eq("dense chosen", ca, cb); compare_state("dense"); }
        if (k % 4 == 0) {
            crashed = both("dense update", [&] { ((Fn1)vptr(O())[6])(O(), nullptr, k); }, [&] { ((Fn1)vptr(R())[6])(R(), nullptr, k); });
            if (!crashed) compare_state("dense update");
        }
        g_twins.resize(2);
        g_cases++;
    }
    g_dense = false;
}

// A constructed case for ChooseTarget's flattening of the facing: the owner faces upwards at 60 degrees; one enemy at distance 1 at 2 radians round from
// the facing, another at distance 1.9 at 0.5 radians. By side 0 the second gets in only by being nearer than the first's angle, which the flattening decides.
static void run_tilt() {
    g_dense = true;
    for (int k = 0; k < 8; k++) {
        setup(97000u + (uint32_t)k, 0, true);
        g_context = "tilt " + std::to_string(k);
        Actor& o = g_case.a[kOwner];
        const float tilt[3] = {0.0f, 0.5f, 0.866025388f};
        std::memcpy(o.facing, tilt, 12);
        const int e1 = kFirstEnemy, e2 = kFirstEnemy + 1;
        const double th[2] = {2.0, 0.5}, dist[2] = {1.0, 1.9};
        const int who[2] = {e1, e2};
        for (int n = 0; n < 2; n++) {
            Actor& a = g_case.a[who[n]];
            a.pos[0] = (float)(o.pos[0] + dist[n] * std::sin(th[n]) * ((k & 1) ? -1 : 1));
            a.pos[1] = (float)(o.pos[1] + dist[n] * std::cos(th[n]));
            a.pos[2] = o.pos[2];
            a.flags = 4;
            a.slot27 = a.slot55 = a.sees = a.lockAnswer = 1;
            a.aim = 0;
            a.nodeMoved = 0;
            for (int s2 = 0; s2 < 2; s2++) {
                std::memcpy(g_world[s2]->node[who[n]].world, a.pos, 12);
                g_world[s2]->node[who[n]].flags = 0;
                at_off<uint32_t>(g_world[s2]->actor[who[n]], 0x98) = 4;
            }
        }
        for (int t = 0; t < 8; t++) {
            for (int j = 0; j < 6; j++) g_case.nameList[t][j] = j == 0 ? e1 : j == 1 ? e2 : -1;
        }
        for (int s2 = 0; s2 < 2; s2++) {
            FaCmCombat* c = (FaCmCombat*)g_combat[s2];
            c->m_useEnemyPriorities = nullptr;
            g_world[s2]->desc[0].count = 1;
        }
        void* a = nullptr;
        GmAcActor* b = nullptr;
        const bool crashed = both("tilt choose", [&] { a = at<FnChoose>(0x000A3C30)(O(), nullptr, 0, 1, 6.3f, k & 2 ? 1 : 0); }, [&] { b = R()->ChooseTarget(0, 1, 6.3f, k & 2 ? 1 : 0); });
        eq("tilt crashed", crashed, 0);
        if (!crashed) {
            g_side = 0; const uint32_t ca = C(a); g_side = 1; const uint32_t cb = C(b); g_side = 0;
            eq("tilt chosen", ca, cb);
            eq("tilt chose the second", ca, canon(0, U(actor_ptr(0, e2))));
        }
        g_twins.resize(2);
        g_cases++;
    }
    g_dense = false;
}

// The small helpers alone.
static void run_helpers() {
    const char* const texts[] = {nullptr, "", "Robot", "ROBOT", "robot1", "Robot1x9", "RoBoT1X0", "Ab", "aB", "Ac", "@[`{", "`{@[", "Z", "z"};
    const int n = (int)(sizeof texts / sizeof texts[0]);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            g_context = "compare " + std::to_string(i) + " " + std::to_string(j);
            int ro = 0, rr = 0;
            both("compare", [&] { ro = at<FnCmp2>(0x00013F50)(texts[i], texts[j]); }, [&] { rr = AtCompareNoCase8(texts[i], texts[j]); });
            eq("compare", ro, rr);
            g_cases++;
        }
    }
    // The angle between vectors: unit vectors, near-parallel ones, and lengths that push the dot product past 1.
    for (int k = 0; k < 400; k++) {
        g_context = "angle " + std::to_string(k);
        g_rand = (uint32_t)k * 7919u + 3;
        float a[3], b[3];
        const float scale = k % 5 == 0 ? 1.001f : k % 7 == 0 ? -1.0f : 1.0f;
        for (int c = 0; c < 3; c++) { a[c] = pick_float(-1.0f, 1.0f); b[c] = pick_float(-1.0f, 1.0f); }
        ((AtVector3*)a)->Normalize();
        ((AtVector3*)b)->Normalize();
        if (k % 4 == 0) std::memcpy(b, a, 12);
        if (k % 9 == 0) for (int c = 0; c < 3; c++) b[c] = -a[c];
        for (int c = 0; c < 3; c++) b[c] *= scale;
        if (k == 13) b[0] = bitsf(0x7FC00000u);
        if (k % 11 == 1) { a[0] = 1; a[1] = a[2] = 0; b[0] = (k & 2) ? 1.0f : -1.0f; b[1] = b[2] = 0; }
        double ro = 0, rr = 0;
        both("angle", [&] { ro = at<FnD1>(0x00092C50)(a, nullptr, U(b)); }, [&] { rr = ((AtVector3*)a)->AngleTo(*(AtVector3*)b); });
        eq("angle as float", fbits((float)ro), fbits((float)rr));
        const float dot = (float)((double)b[2] * a[2] + (double)b[1] * a[1] + (double)b[0] * a[0]);
        if (!(dot < 1.0f && dot > -1.0f)) eq("angle at or beyond 1", *(long long*)&ro, *(long long*)&rr);
        g_cases++;
    }
    // The event, the iterator, the button state, the display.
    for (int k = 0; k < 16; k++) {
        g_context = "small " + std::to_string(k);
        unsigned char a[0x30], b[0x30];
        std::memset(a, 0xCD, sizeof a);
        std::memset(b, 0xCD, sizeof b);
        both("event", [&] { at<FnP0>(0x00152590)(a, nullptr); }, [&] { new (b) GmGameEvent; });
        same_words("event", a, b, 0x28);
        std::memset(a + 0x20, 0x41, 8);
        std::memset(b + 0x20, 0x41, 8);
        both("event set", [&] { ((void(__fastcall*)(void*, void*, uintptr_t, int, int, int))(uintptr_t)0x00152780)(a, nullptr, 0x1234 + k, k, -k, k - 1); },
             [&] { ((GmGameEvent*)b)->Set((GmAcActor*)(uintptr_t)(0x1234 + k), k, -k, k - 1); });
        same_words("event set", a, b, 0x28);
        std::memset(a, 0xCD, sizeof a);
        std::memset(b, 0xCD, sizeof b);
        both("iterator", [&] { at<FnP0>(0x00164670)(a, nullptr); }, [&] { new (b) GmDynamicNodeIterator; });
        same_words("iterator", a, b, 0x1C);
        if (k & 1) { std::memset(a, 0x33, 0x1C); std::memset(b, 0x33, 0x1C); }
        if (k & 2) { std::memset(a, 0, 4); std::memset(b, 0, 4); }
        int ro = 0, rr = 0;
        both("begin", [&] { ro = at<FnI1>(0x00164690)(a, nullptr, 0x3984 + k); }, [&] { rr = ((GmDynamicNodeIterator*)b)->Begin((GmDynamicNodeLoader*)(uintptr_t)(0x3984 + k)); });
        eq("begin", ro, rr);
        same_words("begun", a, b, 0x1C);
        std::memset(a, 0x33, 0x1C);
        std::memset(b, 0x33, 0x1C);
        both("iterator gone", [&] { at<Fn0>(0x00164740)(a, nullptr); }, [&] { ((GmDynamicNodeIterator*)b)->~GmDynamicNodeIterator(); });
        same_words("iterator gone", a, b, 0x1C);
        *(int*)a = *(int*)b = k % 5;
        both("down", [&] { ro = ((int(__fastcall*)(void*, void*))(uintptr_t)0x00075300)(a, nullptr); }, [&] { rr = ((GmButtonState*)b)->IsDown(); });
        eq("down", ro, rr);
        std::memset(a, 0x33, 8);
        std::memset(b, 0x33, 8);
        both("button gone", [&] { at<Fn0>(0x0014BBB0)(a, nullptr); }, [&] { ((GmButtonState*)b)->~GmButtonState(); });
        same_words("button gone", a, b, 8);
        g_cases++;
    }
    for (int k = 0; k < 6; k++) {
        g_context = "display " + std::to_string(k);
        static unsigned char a[0x12A0], b[0x12A0];
        std::memset(a, 0xCD, sizeof a);
        std::memset(b, 0xCD, sizeof b);
        const char* names[] = {"", "Numbuh1", "Delightf", "x", "", "Z"};
        both("display", [&] { ((void(__fastcall*)(void*, void*, const char*))(uintptr_t)0x000B06D0)(a, nullptr, names[k]); }, [&] { ((FaHud*)b)->SetTargetName(names[k]); });
        same_words("display", a + 0x1288, b + 0x1288, 0x18);
        g_cases++;
    }
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (getenv("COMBAT_TRACE")) std::printf("crash at 0x%p [%s]\n", e->ExceptionRecord->ExceptionAddress, g_context.c_str());
        if (guard_resume(e)) return EXCEPTION_CONTINUE_EXECUTION;
        char text[300];
        const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p during [%.200s]\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, g_context.c_str());
        DWORD ignored;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, (DWORD)len, &ignored, nullptr);
        ExitProcess(3);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

template <class M> static void* member_address(M m) { void* p; std::memcpy(&p, &m, sizeof p); return p; }

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
    if (pos.size() < 1) { std::printf("usage: oracle_combat <default.xbe> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();   // the C library's start-up: its character table for tolower

    // The fake vftables.
    for (int i = 0; i < 72; i++) g_vt_actor[i] = nullptr;
    g_vt_actor[4] = (void*)a_slot4;
    g_vt_actor[6] = (void*)a_name;
    g_vt_actor[22] = (void*)a_slot22;
    g_vt_actor[24] = (void*)a_slot24;
    g_vt_actor[27] = (void*)a_slot27;
    g_vt_actor[32] = (void*)a_slot32;
    g_vt_actor[42] = (void*)a_slot42;
    g_vt_actor[49] = (void*)a_slot49;
    g_vt_actor[53] = (void*)a_slot53;
    g_vt_actor[55] = (void*)a_slot55;
    g_vt_actor[59] = (void*)a_slot59;
    g_vt_node[2] = (void*)n_finish;

    // The vftables the events carry, paired by class.
    g_vft_token[0x0028EBCC] = 0xC0000001u;
    g_vft_token[0x0027B304] = 0xC0000002u;
    {
        GmGameEvent* e = new GmGameEvent;
        g_host_vft[*(uint32_t*)e] = 0xC0000001u;
        GmEvDescription* d = new GmEvDescription;
        g_host_vft[*(uint32_t*)d] = 0xC0000002u;
        FaCmCombat* fc = new FaCmCombat;
        g_vft_token[0x00282EF4] = 0xC0000003u;
        g_host_vft[*(uint32_t*)fc] = 0xC0000003u;
        FaCmPlayerCombat* pc = new FaCmPlayerCombat;
        g_vft_token[0x00282F5C] = 0xC0000004u;
        g_host_vft[*(uint32_t*)pc] = 0xC0000004u;
        g_vft_token[U(g_vt_actor)] = 0xC0000005u;
        g_host_vft[U(g_vt_actor)] = 0xC0000005u;
        g_vft_token[U(g_vt_node)] = 0xC0000006u;
        g_host_vft[U(g_vt_node)] = 0xC0000006u;

        // The seams: the original's address and the recovered function, to one double each.
        GmAcNode* node = new GmAcNode;
        redirect(0x0014C170, (const void*)d_node27);
        redirect(U(vptr(node)[27]), (const void*)d_node27);
        redirect(0x00164760, (const void*)d_find);
        redirect(U(member_address(&GmDynamicNodeIterator::Find)), (const void*)d_find);
        redirect(0x001646C0, (const void*)d_next);
        redirect(U(member_address(&GmDynamicNodeIterator::Next)), (const void*)d_next);
        redirect(0x00130E50, (const void*)o_see);
        redirect(U((void*)&GmAcCharacterCanSee), (const void*)r_see);
        redirect(0x00093300, (const void*)o_stick);
        redirect(U((void*)&FaAcPlayerReadStick), (const void*)r_stick);
        redirect(0x00093350, (const void*)o_angle);
        redirect(U((void*)&FaAcPlayerStickAngle), (const void*)r_angle);
        redirect(0x00169730, (const void*)d_attached);
        redirect(U((void*)&GmAcFindAttached), (const void*)d_attached);
        redirect(0x0014C300, (const void*)o_aim);
        redirect(U((void*)&GmAcNodeAimNode), (const void*)r_aim);
        redirect(0x00153CA0, (const void*)d_spawn);
        redirect(U(member_address(&GmSpawnManager::Spawn)), (const void*)d_spawn);
        redirect(0x0008F5D0, (const void*)d_button);
        redirect(U(member_address(&GmPlayerInput::FindButton)), (const void*)d_button);
        redirect(0x000679E0, (const void*)o_find);
        redirect(U((void*)&AtDataRegistryFind), (const void*)r_find);
    }

    // The worlds and the statics the code reads.
    for (int s = 0; s < 2; s++) {
        g_world[s] = (World*)std::calloc(1, sizeof(World));
        g_combat[s] = (unsigned char*)std::calloc(1, 0x100);
    }
    twin(g_world[0], g_world[1], sizeof(World), 1);
    twin(g_combat[0], g_combat[1], 0xE4, 2);
    static unsigned char app[sizeof(GmPlayerApp)];
    ((GmPlayerApp*)app)->m_input = (GmPlayerInput*)(uintptr_t)0x1A900000u;
    *(void**)(uintptr_t)0x00304D58 = app;
    GmPlayerApp::s_instance = (GmPlayerApp*)app;
    *(void**)(uintptr_t)0x003B3984 = (void*)(uintptr_t)0x10ADE400u;
    GmDynamicNodeLoader::s_instance = (GmDynamicNodeLoader*)(uintptr_t)0x10ADE400u;
    *(void**)(uintptr_t)0x003B3A3C = (void*)(uintptr_t)0x5FA00000u;
    GmSpawnManager::s_instance = (GmSpawnManager*)(uintptr_t)0x5FA00000u;
    *(void**)(uintptr_t)0x003B353C = g_world[0]->hud;
    FaHud::s_instance = (FaHud*)g_world[1]->hud;
    if (entries && !arm_entries(entries)) return 2;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_helpers();
    run_direct(23 * 160);
    run_frames(240, 12);
    run_dense(1200);
    run_tilt();
    report_entries();
    std::printf("%s: %d cases, %d calls crashed alike; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_cases, g_crashed_alike, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
