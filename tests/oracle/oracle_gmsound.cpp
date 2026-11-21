// Differential oracle for playing sound notes, the voice slots, sound nodes (.GOO), collision (.GOC) and instance (.GOI) sounds, the game's random numbers, and what they need of the Atlas
// classes (Windows x86, local only). See docs/verification.md.
//
//   oracle_gmsound <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Two worlds, one for each side, built alike: a sound manager, its emitters, segments and banks, actors, scene nodes, coordinate systems and matrices. The original's code runs on one and the
// recovered code on the other, call by call, and after each the two worlds are compared, every pointer by the identity of what it points to.
//
// Doubles, shared by both sides and logging each call with the identity of its object: the emitters' voice slots (Play adds a voice; StopVoice and StopAll mark voices, and the loader's Wait
// takes marked ones away), the banks' lengths, the actors' slots, the scene nodes' FinishBody, the matrix table, the sound system's Wait, and the file system. So nothing here plays, and a
// green result says the manager reaches these with the same arguments in the same order and leaves the same state; it says nothing of what the emitter's own slots do, which are not recovered.
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

#include "at/AtAudioDatabase.h"
#include "at/AtCoordSys.h"
#include "at/AtIoTokenizer.h"
#include "at/AtNameTableOwner.h"
#include "at/AtScene.h"
#include "gm/GmApp.h"
#include "gm/GmAppSettings.h"
#include "gm/GmAcActor.h"
#include "gm/GmAudioData.h"
#include "gm/GmRandom.h"
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

static uint32_t bits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }

// The App object's statics live beside the settings reader, which wants these from the platform layer. Never called here.
int AtPlatformLanguage() { return 0; }
int AtPlatformGameRegion() { return 0; }

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token, g_audio_heap_token;

// ---- what stops a side, and what fails an allocation --------------------------------------------------------------------
static bool g_stop_at_fatal;
static void __cdecl o_fatal_stops(const char* m, int a, int b, int c) {
    o_fatal(m, a, b, c);
    if (g_stop_at_fatal) *(volatile int*)8 = 0;
}
static void r_fatal_stops(const char* m, int a, int, int) {
    if (g_recording) { g_log[1].add('F', a); record_message(m, 1); }
    if (g_stop_at_fatal) *(volatile int*)8 = 0;
}
// The nth push of the audio heap (from 1) makes its allocation fail, and the pop that follows ends that.
static int g_fail_push, g_pushes;
static void note_push(void* h) {
    if (h == &g_audio_heap_token && ++g_pushes == g_fail_push) g_fail_alloc = true;
}
static void __cdecl o_push_counting(void* h) { if (g_recording) { g_log[0].add('P', (uint32_t)(uintptr_t)h); note_push(h); } }
static void __cdecl o_pop_counting() { if (g_recording) { g_log[0].add('p'); g_fail_alloc = false; } }
static void r_push_counting(void* h) { if (g_recording) { g_log[1].add('P', (uint32_t)(uintptr_t)h); note_push(h); } }
static void r_pop_counting() { if (g_recording) { g_log[1].add('p'); g_fail_alloc = false; } }

static void lg(char tag, uint32_t value = 0) { if (g_recording) g_log[g_side].add((uint32_t)tag, value); }

// ---- matrices: an id and a position, through the table on both sides --------------------------------------------------------
struct FakeMatrix { int id; float pos[3]; };
static int mid(const AtMatrix* m) { return m ? ((const FakeMatrix*)m)->id : -1; }
static void __cdecl m_copy(AtMatrix* to, const AtMatrix* from) { lg('C', mid(to) * 1000 + mid(from)); std::memcpy(((FakeMatrix*)to)->pos, ((const FakeMatrix*)from)->pos, 12); }
static void __cdecl m_product(AtMatrix* to, const AtMatrix* a, const AtMatrix* b) {
    lg('X', (mid(to) * 1000 + mid(a)) * 1000 + mid(b));
    for (int i = 0; i < 3; i++) ((FakeMatrix*)to)->pos[i] = ((const FakeMatrix*)a)->pos[i] - ((const FakeMatrix*)b)->pos[i];
}
static void __cdecl m_translation(AtMatrix* m, const AtVector3* v) { lg('T', mid(m)); lg('x', bits(v->x)); lg('y', bits(v->y)); lg('z', bits(v->z)); std::memcpy(((FakeMatrix*)m)->pos, v, 12); }
static void __cdecl m_inverse(AtMatrix* to, const AtMatrix* from) { lg('I', mid(to) * 1000 + mid(from)); }
static void __cdecl m_multiply(AtMatrix* m, const AtMatrix* by) { lg('U', mid(m) * 1000 + mid(by)); }
static void __cdecl m_position(const AtMatrix* m, AtVector3* out) { lg('G', mid(m)); std::memcpy(out, ((const FakeMatrix*)m)->pos, 12); }
static void __cdecl m_axis(AtMatrix* m, const AtVector3* axis, float angle) { lg('A', mid(m)); lg('a', bits(axis->x) ^ (bits(axis->y) << 1) ^ (bits(axis->z) << 2)); lg('n', bits(angle)); }
static void __cdecl m_168(AtMatrix* m, const AtVector3* with) { lg('H', mid(m)); lg('h', (uint32_t)(uintptr_t)with); }

static void install_matrices() {
    *(void**)(uintptr_t)0x002C60C8 = (void*)m_copy;
    *(void**)(uintptr_t)0x002C60D4 = (void*)m_translation;
    *(void**)(uintptr_t)0x002C61D4 = (void*)m_inverse;
    *(void**)(uintptr_t)0x002C6228 = (void*)m_multiply;
    *(void**)(uintptr_t)0x002C6234 = (void*)m_product;
    *(void**)(uintptr_t)0x002C61F8 = (void*)m_position;
    *(void**)(uintptr_t)0x002C6104 = (void*)m_axis;
    *(void**)(uintptr_t)0x002C6168 = (void*)m_168;
    g_atMatrix.Copy = m_copy;
    g_atMatrix.SetTranslation = m_translation;
    g_atMatrix.SetInverse = m_inverse;
    g_atMatrix.Multiply = m_multiply;
    g_atMatrix.SetProduct = m_product;
    g_atMatrix.GetPosition = m_position;
    g_atMatrix.MakeAxisAngle = m_axis;
    g_atMatrix.ScaleAxes = m_168;
}

// ---- voices, emitters, banks, segments ----------------------------------------------------------------------------------
enum { kStopping = 99 };
// A voice with nothing behind its slots: the manager never calls them. The world is not constructed (calloc), so its table pointer stays null.
struct TVoiceBody : AtAudioVoice {
    void SetParameter(int, float, float) override {}
    float GetParameter(int) override { return 0.0f; }
};
struct TVoice { TVoiceBody v; int id; };
static int vid(const AtAudioVoice* v) { return v ? ((const TVoice*)v)->id : -1; }

class Segment;
static int sid(const AtAudioSegment* s);

struct World;
static World* g_world[2];
static void take_voice(int side, class FakeEmitter* e, AtAudioSegment* s);

class FakeEmitter : public AtAudioEmitter {
public:
    int m_id = 0;
    int m_side = 0;
    void InitPlatform() override {}
    void Play(AtAudioSegment* s, AtAudioVoice** out) override { lg('p', m_id * 100 + sid(s)); if (out) *out = nullptr; take_voice(m_side, this, s); }
    void ResumeAll() override { lg('u', m_id); }
    void StopSegment(AtAudioSegment* s) override { lg('s', m_id * 100 + sid(s)); }
    void StopVoice(AtAudioVoice* v) override { lg('v', m_id * 1000 + vid(v)); v->m_state = kStopping; }
    void StopAll() override {
        lg('a', m_id);
        for (AtAudioVoice* v = (AtAudioVoice*)m_firstVoice; v; v = v->m_next) v->m_state = kStopping;
    }
};
static int eid(const AtAudioEmitter* e) { return e ? ((const FakeEmitter*)e)->m_id : -1; }

static float g_lengths[8] = {0.0f, 2.5f, 0.0f, 7.0f, 1.0f, 0.0f, 3.0f, 0.5f};
static float g_ranges[8] = {1.0f, 0.25f, 4.0f, 0.0f, 9.0f, 0.75f, -1.0f, 2.0f};

class TestBank : public AtAudioBank {
public:
    int m_id = 0;
    void OnInit(const AtAudioBankDesc&) override {}
    void Prepare(AtAudioVoice*) override {}
    void Start(AtAudioVoice*) override {}
    void Queue(AtAudioVoice*, AtAudioVoice*) override {}
    void Pause(AtAudioVoice*) override {}
    void Resume(AtAudioVoice*) override {}
    void Stop(AtAudioVoice*) override {}
    void UpdateVoice(AtAudioVoice*, float) override {}
    void Process() override {}
    float SegmentLength(int i) override { lg('L', m_id * 100 + i); return g_lengths[i & 7]; }
    float Unknown12(int i) override { lg('K', m_id * 100 + i); return g_ranges[i & 7]; }
};
static int bid(const AtAudioBank* b) { return b ? ((const TestBank*)b)->m_id : -1; }

class Segment : public AtAudioSegment {
public:
    int m_id = 0;
    void OnRead() override {}
};
static int sid(const AtAudioSegment* s) { return s ? ((const Segment*)s)->m_id : -1; }

class TestSegments : public AtAudioSegmentDatabase {
public:
    void OnReady() override {}
    void Unknown3(void*) override {}
    AtAudioSegment* SegmentAt(unsigned int) override { return nullptr; }
};
class TestBankList : public AtAudioBankDatabase {
public:
    void CreateBank(unsigned int, const AtAudioBankDesc&) override {}
    void SetDirectory(const char*) override {}
};
class TestDb : public AtAudioDatabase {
public:
    TestSegments m_list;
    TestBankList m_banks[2];
    int m_id = 0;
    void AllocateBankDatabases() override {}
    void AllocateSegmentDatabases() override {}
    void AllocateEmitterDatabases() override {}
    void AllocateNestedDatabases() override {}
    AtAudioBankDatabase* BankDatabaseAt(unsigned int index) override { lg('d', m_id * 10 + index); return m_id != 0 || index == 1 || index > 2 ? nullptr : &m_banks[index / 2]; }
    AtAudioSegmentDatabase* SegmentDatabaseAt(unsigned int) override { return &m_list; }
    AtAudioEmitterDatabase* EmitterDatabaseAt(unsigned int) override { return nullptr; }
    AtAudioDatabase* NestedDatabaseAt(unsigned int) override { return nullptr; }
};

class TestLoader : public AtAudioBankLoader {
public:
    int m_side = 0;
    int m_waits = 0;
    void AddEmitter(AtAudioEmitter*) override {}
    void RemoveEmitter(AtAudioEmitter*) override {}
    void Add(AtAudioBank*) override {}
    void Remove(AtAudioBank*) override {}
    void Wait(float seconds) override;
};

// ---- actors and scene nodes: raw objects with tables of their own -----------------------------------------------------------
static int g_wild_calls;
static void __fastcall wild_slot(void*, void*) { lg('!'); g_wild_calls++; }
static void __fastcall actor_motion(void* self, void*, AtVector3* position, AtVector3* a, AtVector3* b, AtVector3* c, AtVector3* d) {
    char* p = (char*)self;
    lg('M', *(int*)(p + 4) * 32 + (position != 0) + 2 * (a != 0) + 4 * (b != 0) + 8 * (c != 0) + 16 * (d != 0));
    if (position) std::memcpy(position, p + 0x48, 12);
}
static float __fastcall actor_slot52(void* self, void*) { lg('k', *(int*)((char*)self + 4)); return 3.5f; }
static void* g_actor_table[53];

static void __fastcall node_finish(void* self, void*) {
    AtSceneNode* n = (AtSceneNode*)self;
    lg('B', (uint32_t)n->m_unknown60);
    n->m_worldSphere.radius += 1.0f;
    n->m_flags &= ~(unsigned)AtSceneNode::kMoved;
}
static void* g_node_table[32];
static int nid(const AtSceneNode* n) { return n ? n->m_unknown60 : -1; }
static int aid(const GmAcActor* a) { return a ? *(const int*)((const char*)a + 4) : -1; }

static void build_tables() {
    for (void*& s : g_actor_table) s = (void*)wild_slot;
    g_actor_table[22] = (void*)actor_motion;
    g_actor_table[52] = (void*)actor_slot52;
    for (void*& s : g_node_table) s = (void*)wild_slot;
    g_node_table[2] = (void*)node_finish;
}

// ---- a world ------------------------------------------------------------------------------------------------------------
enum { kEmitters = 48, kSegments = 8, kBanks = 3, kActors = 5, kNodes = 8, kMatrices = 128, kVoices = 3000, kPool = 3, kNotes = 14 };
static const char* const kNamed[10] = {"ui", "sfx", "voice", "music", "ambience", "collsn1", "collsn2", "collsn3", "collsn4", "collsn5"};

struct World {
    int side;
    GmAudioData* mgr;
    AtNameTableOwner* owner;
    GmNameMap* emitterMap;
    FakeEmitter* emitters[kEmitters];
    AtCoordSys* coords[16];
    int coordCount;
    FakeMatrix matrices[kMatrices];
    int matrixCount;
    TVoice voices[kVoices];
    int voiceCount;
    TestBank* banks[kBanks];
    Segment* segments[kSegments];
    TestDb* dbs[2];
    AtSceneNode* nodes[kNodes];
    GmAcActor* actors[kActors];
    GmAcComponent* audio[kActors];
    GmAcAudio* comps[kActors];
    AtCoordSys* listener;
    TestLoader loader;
    App* app;
    GmHistoryLogs logs;
    GmRandom* random;
    GmSoundNote* notes;
    AtAudioBank* bankPointers[kBanks];
    AtAudioBank* secondList[2];
    GmSoundNode beyond;

    AtMatrix* matrix(float x, float y, float z) {
        FakeMatrix* m = &matrices[matrixCount];
        m->id = ++matrixCount;
        m->pos[0] = x; m->pos[1] = y; m->pos[2] = z;
        return (AtMatrix*)m;
    }
    AtCoordSys* coord(float x, float y, float z, AtCoordSys* parent) {
        AtCoordSys* c = new AtCoordSys;
        c->m_local = matrix(x, y, z);
        c->m_world = matrix(x + 0.5f, y, z);
        if (parent) {
            c->m_parent = parent;
            c->m_nextSibling = parent->m_firstChild;
            parent->m_firstChild = c;
        }
        coords[coordCount++] = c;
        return c;
    }
};

static void take_voice(int side, FakeEmitter* e, AtAudioSegment* s) {
    World* w = g_world[side];
    if (!w || w->voiceCount >= kVoices) return;
    TVoice* v = &w->voices[w->voiceCount];
    v->id = ++w->voiceCount;
    v->v.m_segment = s;
    v->v.m_state = 0;
    v->v.m_next = nullptr;
    AtAudioVoice** at_end = (AtAudioVoice**)&e->m_firstVoice;
    while (*at_end) at_end = &(*at_end)->m_next;
    *at_end = &v->v;
}

void TestLoader::Wait(float seconds) {
    lg('w', bits(seconds));
    if (++m_waits > 1000) *(volatile int*)8 = 0;  // A loop that never ends is a difference, not a hang.
    World* w = g_world[m_side];
    for (FakeEmitter* e : w->emitters) {
        AtAudioVoice** link = (AtAudioVoice**)&e->m_firstVoice;
        while (*link) {
            if ((*link)->m_state == kStopping) { *link = (*link)->m_next; return; }
            link = &(*link)->m_next;
        }
    }
}

static void add_voice(World* w, int emitter, int segment) { take_voice(w->side, w->emitters[emitter], segment < 0 ? nullptr : w->segments[segment]); }

typedef void*(__fastcall* FnCtor)(void*, void*);

static World* build_world(int side) {
    World* w = (World*)std::calloc(1, sizeof(World));
    w->side = side;
    new (&w->loader) TestLoader;
    w->loader.m_side = side;
    // The manager: databases, sixty note banks by each side's constructor, forty free voice slots, and a pool of dynamic nodes.
    w->mgr = (GmAudioData*)std::calloc(1, sizeof(GmAudioData));
    for (int i = 0; i < GmAudioData::kNoteBanks; i++) {
        if (side == 0) at<FnCtor>(0x00136CC0)(&w->mgr->m_noteBanks[i], nullptr);
        else new (&w->mgr->m_noteBanks[i]) GmSoundNoteBank;
    }
    for (int i = 0; i < GmAudioData::kVoiceSlots; i++) w->mgr->m_voiceSlots[i].m_state = GmSoundVoiceSlot::kFree;
    w->mgr->m_impulseThresholds[0] = 1.25f;
    w->mgr->m_impulseThresholds[1] = 2.5f;
    w->mgr->m_impulseThresholds[2] = -3.0f;
    for (int i = 0; i < kBanks; i++) {
        w->banks[i] = new TestBank;
        w->banks[i]->m_id = i;
        w->bankPointers[i] = w->banks[i];
    }
    for (int i = 0; i < kSegments; i++) {
        w->segments[i] = new Segment;
        w->segments[i]->m_id = i;
        w->segments[i]->m_bank = i == 6 ? nullptr : w->banks[i % kBanks];
        w->segments[i]->m_index = i;
        w->segments[i]->m_looping = (i == 2 || i == 6) ? 1 : 0;
    }
    for (int d = 0; d < 2; d++) {
        w->dbs[d] = new TestDb;
        w->dbs[d]->m_id = d;
        w->dbs[d]->m_segmentCount = 1;
        w->dbs[d]->m_bankCount = d == 0 ? 3 : 1;
        w->dbs[d]->m_list.Init(16);
    }
    for (int i = 0; i < kSegments; i++) {
        char name[8];
        std::sprintf(name, "seg%d", i);
        w->dbs[i < 5 ? 0 : 1]->m_list.Insert(name, w->segments[i]);
    }
    w->dbs[0]->m_banks[0].m_count = 2;
    w->dbs[0]->m_banks[0].m_banks = &w->bankPointers[0];
    w->dbs[0]->m_banks[1].m_count = 2;
    w->secondList[0] = nullptr;
    w->secondList[1] = w->banks[2];
    w->dbs[0]->m_banks[1].m_banks = w->secondList;
    w->mgr->m_firstDatabases[1] = w->dbs[1];
    w->mgr->m_firstDatabaseCount = 2;
    w->mgr->m_databases[3] = w->dbs[0];
    w->mgr->m_databases[10] = w->dbs[1];
    w->mgr->m_databases[20] = w->dbs[0];
    // The pool.
    GmSoundNodeBank* pool = &w->mgr->m_nodeBanks[0];
    pool->m_nodes = (GmSoundNode*)std::calloc(kPool, sizeof(GmSoundNode));
    pool->m_count = kPool;
    pool->m_used = 1;
    // Emitters: the named ten, actor1 to actor36, and two more that FindEmitters does not ask for.
    w->owner = (AtNameTableOwner*)std::calloc(1, sizeof(AtNameTableOwner));
    new (&w->owner->m_nodes) GmNameMapGroup;
    w->owner->m_loader = &w->loader;
    w->owner->m_unknown14 = 2.0f;
    w->emitterMap = new GmNameMap;
    w->emitterMap->Init(64);
    for (int i = 0; i < kEmitters; i++) {
        FakeEmitter* e = new FakeEmitter;
        e->m_id = i;
        e->m_side = side;
        e->m_kind = (i >= 10 && i % 3 != 0) ? 1 : 0;
        e->m_local = w->matrix(0, (float)i, 0);
        e->m_world = w->matrix(1, (float)i, 0);
        w->emitters[i] = e;
        char name[12];
        if (i < 10) std::strcpy(name, kNamed[i]);
        else if (i == 45) std::strcpy(name, "actor40");
        else if (i < 46) std::sprintf(name, "actor%d", i - 9);
        else std::sprintf(name, "glob%d", i - 45);
        w->emitterMap->Insert(name, e);
    }
    w->owner->m_nodes.Add(w->emitterMap);
    // Emitter 12 (actor3) under a parent, emitter 13 (actor4) with two children.
    AtCoordSys* parent = w->coord(5, 5, 5, nullptr);
    w->emitters[12]->m_parent = parent;
    AtCoordSys* c1 = w->coord(1, 1, 1, nullptr);
    AtCoordSys* c2 = w->coord(2, 2, 2, nullptr);
    w->emitters[13]->m_firstChild = c1;
    c1->m_nextSibling = c2;
    c1->m_dirty = 0x100;
    // Listener at the origin.
    w->listener = w->coord(0, 0, 0, nullptr);
    // Scene nodes: some with a coordinate system, one under a parent, one with none; some moved.
    for (int i = 0; i < kNodes; i++) {
        AtSceneNode* n = (AtSceneNode*)std::calloc(1, sizeof(AtSceneNode));
        *(void***)n = g_node_table;
        n->m_unknown60 = i;
        n->m_flags = (i % 2) ? AtSceneNode::kMoved | 0x10 : 0x10;
        n->m_worldSphere.center.x = (float)i;
        n->m_worldSphere.center.y = 0.5f;
        n->m_worldSphere.center.z = -1.0f;
        n->m_worldSphere.radius = 0.25f * i;
        if (i == 3) n->m_coordSys = nullptr;
        else n->m_coordSys = w->coord((float)i, 1, 2, i == 5 ? parent : nullptr);
        w->nodes[i] = n;
    }
    // Actors: 0 with a node, audio and dynamic nodes; 1 with audio but none; 2 with nothing; 3 with a node without coordinates and dynamic nodes, but no sound source.
    for (int i = 0; i < kActors; i++) {
        char* p = (char*)std::calloc(1, sizeof(GmAcActor));
        *(void***)p = g_actor_table;
        *(int*)(p + 4) = i;
        float* position = (float*)(p + 0x48);
        position[0] = 10.0f + i;
        position[1] = -2.0f;
        position[2] = 0.125f * i;
        GmAcActor* a = (GmAcActor*)p;
        a->m_unknownA0 = i == 4 ? nullptr : (i == 3 ? w->nodes[3] : (i == 2 ? w->nodes[7] : w->nodes[1 + i]));
        a->m_sceneNode = i == 3 ? nullptr : w->nodes[i == 0 ? 4 : 6];
        if (i != 2 && i != 4) {
            w->audio[i] = (GmAcComponent*)std::calloc(1, sizeof(GmAcComponent));
            a->m_component = w->audio[i];
        }
        if (i == 0 || i == 3) {
            GmAcAudio* c = (GmAcAudio*)std::calloc(1, sizeof(GmAcAudio));
            c->m_manager = w->mgr;
            c->m_count = i == 0 ? 2 : 1;
            c->m_nodes = (GmSoundNode**)std::calloc(4, sizeof(GmSoundNode*));
            w->beyond.m_emitter = w->emitters[3];
            c->m_nodes[c->m_count] = &w->beyond;
            c->m_actor = a;
            w->comps[i] = c;
            w->audio[i]->m_audio = c;
        }
        w->actors[i] = a;
    }
    w->app = (App*)std::calloc(1, sizeof(App));
    w->random = (GmRandom*)std::calloc(1, sizeof(GmRandom));
    w->logs.m_random = w->random;
    return w;
}

static uint32_t g_seed = 4357;
static void seed_both() {
    GmRandom model;
    model.Seed(g_seed);
    for (int s = 0; s < 2; s++) *g_world[s]->random = model;
}

static void install(int side) {
    World* w = g_world[side];
    if (side == 0) {
        *(GmAudioData**)(uintptr_t)0x003B354C = w->mgr;
        *(AtNameTableOwner**)(uintptr_t)0x003B3558 = w->owner;
        *(AtCoordSys**)(uintptr_t)0x003B3554 = w->listener;
        *(App**)(uintptr_t)0x003B360C = w->app;
        *(GmHistoryLogs**)(uintptr_t)0x002CB0A8 = &w->logs;
    } else {
        GmAudioData::s_instance = w->mgr;
        GmAudioData::s_soundOwner = w->owner;
        GmAudioData::s_listener = w->listener;
        App::s_gmApp = w->app;
        GmHistoryLogs::s_instance = &w->logs;
    }
}

struct Pair {
    Pair() {
        for (int s = 0; s < 2; s++) { g_world[s] = build_world(s); install(s); }
        seed_both();
    }
    // Worlds are left to leak: what the code under test holds is compared, not freed.
    ~Pair() { g_world[0] = g_world[1] = nullptr; }
};

// ---- comparing two worlds -----------------------------------------------------------------------------------------------
static long long node_place(World* w, const GmSoundNode* n) {
    if (!n) return -1;
    for (int b = 0; b < GmAudioData::kNodeBanks; b++) {
        const GmSoundNodeBank& bank = w->mgr->m_nodeBanks[b];
        if (bank.m_nodes && n >= bank.m_nodes && n < bank.m_nodes + bank.m_count) return b * 10000 + (n - bank.m_nodes);
    }
    return -2;
}

static void same_node(const char* what, const GmSoundNode& a, const GmSoundNode& b) {
    eq(what, std::memcmp(a.m_name, b.m_name, 16), 0);
    eq(what, aid(a.m_owner), aid(b.m_owner));
    eq(what, nid(a.m_source), nid(b.m_source));
    eq(what, eid(a.m_emitter), eid(b.m_emitter));
    eq(what, std::memcmp(a.m_segmentName, b.m_segmentName, 8), 0);
    eq(what, sid(a.m_segment), sid(b.m_segment));
    eq(what, std::memcmp(&a.m_velocityParm, &b.m_velocityParm, sizeof(GmSoundNode) - offsetof(GmSoundNode, m_velocityParm)), 0);
}

static void same_names(const char* what, const char* a, const char* b) { eq(what, std::strncmp(a, b, 8), 0); }

static void compare_worlds(const char* what) {
    World* o = g_world[0];
    World* r = g_world[1];
    GmAudioData* a = o->mgr;
    GmAudioData* b = r->mgr;
    for (int i = 0; i < GmAudioData::kVoiceSlots; i++) {
        eq("voice slot state", a->m_voiceSlots[i].m_state, b->m_voiceSlots[i].m_state);
        eq("voice slot emitter", eid(a->m_voiceSlots[i].m_emitter), eid(b->m_voiceSlots[i].m_emitter));
        eq("voice slot actor", aid(a->m_voiceSlots[i].m_actor), aid(b->m_voiceSlots[i].m_actor));
    }
    eq("last voice slot", a->m_lastVoiceSlot, b->m_lastVoiceSlot);
    AtAudioEmitter** na = &a->m_ambience;
    AtAudioEmitter** nb = &b->m_ambience;
    for (int i = 0; i < 10; i++) eq("named emitters", eid(na[i]), eid(nb[i]));
    for (int i = 0; i < GmAudioData::kDatabases; i++) eq("databases", a->m_databases[i] != nullptr, b->m_databases[i] != nullptr);
    for (int k = 0; k < GmAudioData::kNodeBanks; k++) {
        const GmSoundNodeBank& x = a->m_nodeBanks[k];
        const GmSoundNodeBank& y = b->m_nodeBanks[k];
        eq("node bank used", x.m_used, y.m_used);
        eq("node bank count", x.m_count, y.m_count);
        eq("node bank nodes", x.m_nodes != nullptr, y.m_nodes != nullptr);
        if (x.m_nodes && y.m_nodes && x.m_count == y.m_count)
            for (unsigned i = 0; i < x.m_count; i++) same_node(what, x.m_nodes[i], y.m_nodes[i]);
    }
    for (int k = 0; k < GmAudioData::kSoundBanks; k++) {
        const GmCollisionSoundBank& x = a->m_collisionBanks[k];
        const GmCollisionSoundBank& y = b->m_collisionBanks[k];
        eq("collision bank", x.m_used, y.m_used);
        eq("collision bank", x.m_count, y.m_count);
        eq("collision bank", x.m_sounds != nullptr, y.m_sounds != nullptr);
        if (x.m_sounds && y.m_sounds && x.m_count == y.m_count) {
            for (unsigned i = 0; i < x.m_count; i++) {
                const GmCollisionSound& c = x.m_sounds[i];
                const GmCollisionSound& d = y.m_sounds[i];
                eq("collision", std::memcmp(c.m_name, d.m_name, 8), 0);
                eq("collision emitter", eid(c.m_emitter), eid(d.m_emitter));
                eq("collision segments", std::memcmp(c.m_defaultSegments, d.m_defaultSegments, 24), 0);
                eq("collision materials", c.m_materialCount, d.m_materialCount);
                eq("collision materials", c.m_materials != nullptr, d.m_materials != nullptr);
                eq("collision flags", c.m_playerInitiatedOnly * 2 + c.m_ragdollOnly, d.m_playerInitiatedOnly * 2 + d.m_ragdollOnly);
                eq("collision thresholds", std::memcmp(c.m_impulseThresholds, d.m_impulseThresholds, 12), 0);
                if (c.m_materials && d.m_materials && c.m_materialCount == d.m_materialCount && c.m_materialCount < 100)
                    eq("collision material names", std::memcmp(c.m_materials, d.m_materials, c.m_materialCount * sizeof(GmCollisionMaterial)), 0);
            }
        }
        const GmInstanceSoundBank& p = a->m_instanceBanks[k];
        const GmInstanceSoundBank& q = b->m_instanceBanks[k];
        eq("instance bank", p.m_used, q.m_used);
        eq("instance bank", p.m_count, q.m_count);
        eq("instance bank", p.m_sounds != nullptr, q.m_sounds != nullptr);
        if (p.m_sounds && q.m_sounds && p.m_count == q.m_count) {
            for (unsigned i = 0; i < p.m_count; i++) {
                const GmInstanceSound& c = p.m_sounds[i];
                const GmInstanceSound& d = q.m_sounds[i];
                same_names("instance name", c.m_name, d.m_name);
                long long cc = c.m_collision ? -2 : -1, dd = d.m_collision ? -2 : -1;
                for (int m = 0; m < GmAudioData::kSoundBanks; m++) {
                    const GmCollisionSoundBank& cb = a->m_collisionBanks[m];
                    const GmCollisionSoundBank& db = b->m_collisionBanks[m];
                    if (c.m_collision && cb.m_sounds && c.m_collision >= cb.m_sounds && c.m_collision < cb.m_sounds + cb.m_count) cc = m * 1000 + (c.m_collision - cb.m_sounds);
                    if (d.m_collision && db.m_sounds && d.m_collision >= db.m_sounds && d.m_collision < db.m_sounds + db.m_count) dd = m * 1000 + (d.m_collision - db.m_sounds);
                }
                eq("instance collision", cc, dd);
                eq("instance count", c.m_instanceCount, d.m_instanceCount);
                eq("instance list", c.m_instances != nullptr, d.m_instances != nullptr);
                eq("instance list", c.m_unknown14 != nullptr, d.m_unknown14 != nullptr);
                if (c.m_instances && d.m_instances && c.m_instanceCount == d.m_instanceCount && c.m_instanceCount < 100)
                    for (unsigned m = 0; m < c.m_instanceCount; m++) same_names("instance", c.m_instances[m], d.m_instances[m]);
            }
        }
    }
    for (int i = 0; i < kEmitters; i++) {
        FakeEmitter* x = o->emitters[i];
        FakeEmitter* y = r->emitters[i];
        eq("emitter dirty", x->m_dirty, y->m_dirty);
        eq("emitter local", std::memcmp(((FakeMatrix*)x->m_local)->pos, ((FakeMatrix*)y->m_local)->pos, 12), 0);
        eq("emitter world", std::memcmp(((FakeMatrix*)x->m_world)->pos, ((FakeMatrix*)y->m_world)->pos, 12), 0);
        const AtAudioVoice* u = (AtAudioVoice*)x->m_firstVoice;
        const AtAudioVoice* v = (AtAudioVoice*)y->m_firstVoice;
        for (; u && v; u = u->m_next, v = v->m_next) {
            eq("voice", vid(u), vid(v));
            eq("voice segment", sid(u->m_segment), sid(v->m_segment));
            eq("voice state", u->m_state, v->m_state);
        }
        eq("voice list length", u != nullptr, v != nullptr);
    }
    for (int i = 0; i < o->coordCount; i++) eq("coord dirty", o->coords[i]->m_dirty, r->coords[i]->m_dirty);
    for (int i = 0; i < kNodes; i++) {
        eq("scene node flags", o->nodes[i]->m_flags, r->nodes[i]->m_flags);
        eq("scene node sphere", bits(o->nodes[i]->m_worldSphere.radius), bits(r->nodes[i]->m_worldSphere.radius));
    }
    for (int i = 0; i < kActors; i++) {
        if (!o->comps[i]) continue;
        for (int k = 0; k < 4; k++) eq("actor's nodes", node_place(o, o->comps[i]->m_nodes[k]), node_place(r, r->comps[i]->m_nodes[k]));
    }
    eq("random", std::memcmp(o->random, r->random, sizeof(GmRandom)), 0);
    eq("waits", o->loader.m_waits, r->loader.m_waits);
    (void)what;
}

static int g_crashes_seen;
template <class FO, class FR> static bool both(const char* what, FO fo, FR fr) {
    const bool oc = guarded([&] { On on(0); fo(); });
    g_recording = false;
    g_side = 0;
    g_fail_alloc = false;
    const int pushes = g_pushes;
    g_pushes = 0;
    const bool rc = guarded([&] { On on(1); fr(); });
    g_recording = false;
    g_side = 0;
    g_fail_alloc = false;
    eq(what, pushes, g_pushes);
    g_pushes = 0;
    eq(what, oc, rc);
    if (oc) g_crashes_seen++;
    if ((oc || rc) && std::getenv("GMSOUND_SHOW_STOPS")) std::printf("stop: %s: %s\n", g_context.c_str(), what);
    compare_logs(what);
    return oc || rc;
}

// ---- typedefs for the original's functions ------------------------------------------------------------------------------
typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, void*);
typedef void(__fastcall* Fn2)(void*, void*, void*, void*);
typedef void*(__fastcall* FnP0)(void*, void*);
typedef void*(__fastcall* FnP1)(void*, void*, const void*);
typedef void*(__fastcall* FnPU)(void*, void*, unsigned);
typedef void(__fastcall* FnPath)(void*, void*, void*, const char*);
typedef void(__fastcall* FnNames)(void*, void*, const char*, const char*);
typedef void*(__fastcall* FnTake)(void*, void*, void*, float);
typedef void(__fastcall* FnU)(void*, void*, unsigned);
typedef unsigned(__fastcall* FnRU0)(void*, void*);
typedef unsigned(__fastcall* FnRU1)(void*, void*, unsigned);
typedef float(__fastcall* FnRF0)(void*, void*);
typedef void(__fastcall* FnPlace)(void*, void*, const void*, float, const void*, const void*);
typedef void(__fastcall* FnStopSeg)(void*, void*, void*, int);
typedef void(__fastcall* FnCompPlay)(void*, void*, void*, int, const void*, int);

#define O(w) g_world[0]->w
#define R(w) g_world[1]->w

// ---- the random numbers -------------------------------------------------------------------------------------------------
static int g_randoms_seen;
static void run_random() {
    g_context = "random";
    GmRandom* a = (GmRandom*)std::malloc(sizeof(GmRandom));
    GmRandom* b = (GmRandom*)std::malloc(sizeof(GmRandom));
    std::memset(a, 0x5A, sizeof(GmRandom));
    std::memset(b, 0x5A, sizeof(GmRandom));
    both("random ctor", [&] { at<FnCtor>(0x00071BB0)(a, nullptr); }, [&] { new (b) GmRandom; });
    eq("random ctor", std::memcmp(a, b, sizeof(GmRandom)), 0);
    // Unseeded: the first Next regenerates from what the state holds.
    for (int i = 0; i < 3; i++) {
        unsigned x = 0, y = 0;
        both("random unseeded", [&] { x = at<FnRU0>(0x00071D00)(a, nullptr); }, [&] { y = b->Next(); });
        eq("random unseeded", x, y);
    }
    for (uint32_t seed : {0u, 1u, 4357u, 0x12345678u, 0xFFFFFFFFu}) {
        g_context = "random seed " + std::to_string(seed);
        both("seed", [&] { at<FnU>(0x00071CA0)(a, nullptr, seed); }, [&] { b->Seed(seed); });
        eq("seed state", std::memcmp(a, b, sizeof(GmRandom)), 0);
        for (int i = 0; i < 1400; i++) {
            unsigned x = 0, y = 0;
            const int kind = i % 4;
            if (kind == 0 || kind == 3) {
                both("next", [&] { x = at<FnRU0>(0x00071D00)(a, nullptr); }, [&] { y = b->Next(); });
            } else if (kind == 1) {
                const unsigned bound = (unsigned)(i % 7 == 0 ? 0 : i * 37 + 1);
                both("below", [&] { x = at<FnRU1>(0x00071D50)(a, nullptr, bound); }, [&] { y = b->NextBelow(bound); });
            } else {
                float f = 0, g = 0;
                both("unit", [&] { f = at<FnRF0>(0x00071D70)(a, nullptr); }, [&] { g = b->NextUnit(); });
                x = bits(f);
                y = bits(g);
            }
            eq("random value", x, y);
            g_randoms_seen++;
        }
        eq("random state", std::memcmp(a, b, sizeof(GmRandom)), 0);
    }
    g_context = "random default";
    int x = 0, y = 0;
    both("seed default", [&] { x = (int)at<FnRU0>(0x00071D90)(a, nullptr); }, [&] { y = b->SeedDefault(); });
    eq("seed default", x, y);
    eq("seed default state", std::memcmp(a, b, sizeof(GmRandom)), 0);
    a->m_seed = b->m_seed = 77;
    both("seed default returning", [&] { x = (int)at<FnRU0>(0x00071DA0)(a, nullptr); }, [&] { y = (int)b->SeedDefaultReturningSeed(); });
    eq("seed default returning", x, y);
    eq("seed default returning state", std::memcmp(a, b, sizeof(GmRandom)), 0);
    // Regenerating from a written state, mid-way.
    for (int i = 0; i < GmRandom::kCount; i++) a->m_state[i] = b->m_state[i] = (uint32_t)(i * 2654435761u) ^ (i & 1 ? 0x80000000u : 1u);
    a->m_index = b->m_index = 17;
    both("regenerate", [&] { at<Fn0>(0x00071BD0)(a, nullptr); }, [&] { b->Regenerate(); });
    eq("regenerate state", std::memcmp(a, b, sizeof(GmRandom)), 0);
    std::free(a);
    std::free(b);
}

// ---- the Atlas pieces, directly -----------------------------------------------------------------------------------------
static void run_atlas() {
    Pair p;
    g_context = "atlas: voices";
    for (int s = 0; s < 2; s++) {
        add_voice(g_world[s], 20, 1);
        add_voice(g_world[s], 20, 3);
        add_voice(g_world[s], 20, 1);
        add_voice(g_world[s], 20, -1);
    }
    for (int seg : {1, 3, 5, -1}) {
        void* x = nullptr;
        void* y = nullptr;
        AtAudioSegment* so = seg < 0 ? nullptr : O(segments[seg]);
        AtAudioSegment* sr = seg < 0 ? nullptr : R(segments[seg]);
        both("is playing", [&] { x = at<FnP1>(0x0011F160)(O(emitters[20]), nullptr, so); }, [&] { y = R(emitters[20])->IsPlaying(sr); });
        eq("is playing", vid((AtAudioVoice*)x), vid((AtAudioVoice*)y));
        both("is playing, none", [&] { x = at<FnP1>(0x0011F160)(O(emitters[21]), nullptr, so); }, [&] { y = R(emitters[21])->IsPlaying(sr); });
        eq("is playing, none", vid((AtAudioVoice*)x), vid((AtAudioVoice*)y));
    }
    {
        void* x = nullptr;
        void* y = nullptr;
        both("first voice", [&] { x = at<FnP0>(0x0011EBA0)(O(emitters[20]), nullptr); }, [&] { y = R(emitters[20])->FirstVoice(); });
        eq("first voice", vid((AtAudioVoice*)x), vid((AtAudioVoice*)y));
        while (x && y) {
            void* xo = x;
            void* yo = y;
            both("next voice", [&] { x = at<FnP1>(0x0011EBB0)(nullptr, nullptr, xo); }, [&] { y = R(emitters[21])->NextVoice((AtAudioVoice*)yo); });
            eq("next voice", vid((AtAudioVoice*)x), vid((AtAudioVoice*)y));
        }
    }
    g_context = "atlas: coordinates";
    for (unsigned mask : {0x2Bu, 0x1u, 0x80000000u}) {
        both("mark children", [&] { at<FnU>(0x00032310)(O(emitters[13]), nullptr, mask); }, [&] { R(emitters[13])->MarkChildrenDirty(mask); });
        both("mark children, none", [&] { at<FnU>(0x00032310)(O(emitters[14]), nullptr, mask); }, [&] { R(emitters[14])->MarkChildrenDirty(mask); });
        compare_worlds("mark children");
    }
    for (int s = 0; s < 2; s++) {
        World* w = g_world[s];
        w->emitters[15]->m_parent = w->emitters[12]->m_parent;
        w->emitters[15]->m_firstChild = w->coord(7, 7, 7, nullptr);
    }
    for (int e : {11, 12, 13, 15}) {
        for (int s = 0; s < 2; s++) {
            g_world[s]->emitters[e]->m_dirty = 0x41;
            if (g_world[s]->emitters[e]->m_parent) g_world[s]->emitters[e]->m_parent->m_dirty |= AtCoordSys::kInverse;
        }
        both("local from world", [&] { at<Fn0>(0x00032DF0)(O(emitters[e]), nullptr); }, [&] { R(emitters[e])->LocalFromWorld(); });
        compare_worlds("local from world");
    }
    for (int e : {11, 12}) {
        for (int with : {0, 1}) {
            AtVector3 axis = {0.25f, -1.0f, 2.0f};
            AtVector3 position = {3.0f, 4.0f, -5.5f};
            int extra = 7;
            const AtVector3* w = with ? (const AtVector3*)&extra : nullptr;
            both("set world", [&] { at<FnPlace>(0x00032EA0)(O(emitters[e]), nullptr, &axis, 0.75f, &position, w); },
                 [&] { R(emitters[e])->Place(&axis, 0.75f, &position, w); });
            compare_worlds("set world");
        }
    }
    // A world position from a root, and from one under a parent.
    for (int n = 0; n < kNodes; n++) {
        AtCoordSys* co = O(nodes[n])->m_coordSys;
        AtCoordSys* cr = R(nodes[n])->m_coordSys;
        AtVector3 x = {9, 9, 9}, y = {9, 9, 9};
        if (co) {
            co->m_dirty |= AtCoordSys::kWorld;
            cr->m_dirty |= AtCoordSys::kWorld;
            both("coord position", [&] { at<Fn1>(0x000332B0)(co, nullptr, &x); }, [&] { cr->WorldPosition(&y); });
            eq("coord position", std::memcmp(&x, &y, 12), 0);
        }
        x = y = AtVector3{9, 9, 9};
        if (co) {
            co->m_dirty |= AtCoordSys::kWorld;
            cr->m_dirty |= AtCoordSys::kWorld;
        }
        both("node position", [&] { at<Fn1>(0x00049990)(O(nodes[n]), nullptr, &x); }, [&] { R(nodes[n])->GetWorldPosition(&y); });
        eq("node position", std::memcmp(&x, &y, 12), 0);
        AtSceneSphere u, v;
        std::memset(&u, 0xCD, sizeof u);
        std::memset(&v, 0xCD, sizeof v);
        both("node sphere", [&] { at<Fn1>(0x00136C60)(O(nodes[n]), nullptr, &u); }, [&] { R(nodes[n])->GetWorldSphere(&v); });
        eq("node sphere", std::memcmp(&u, &v, sizeof u), 0);
        compare_worlds("node");
    }
    g_context = "atlas: app";
    void* x = nullptr;
    void* y = nullptr;
    both("app", [&] { x = at<void*(__cdecl*)()>(0x001414F0)(); }, [&] { y = App::GetGmApp(); });
    eq("app", x == O(app), y == R(app));
}

// ---- notes ----------------------------------------------------------------------------------------------------------------
struct NoteSpec {
    int emitter, conditional;  // -1: none
    const char* stop;
    int singleton;
    int count;
    int segments[4];           // -1: a choice with no segment
    float weights[4];
};
// The sentinel past each note's choices is seg7 with a weight of 0.5, alike on both sides, so a choice past the end is seen and compared.
static const NoteSpec kNoteSpecs[kNotes] = {
    {-1, -1, "", 0, 2, {0, 1}, {0.25f, 0.75f}},
    {1, -1, "", 0, 3, {2, 3, 4}, {0.2f, 0.3f, 0.5f}},
    {46, 46, "n0", 0, 1, {5}, {1.0f}},
    {47, -1, "n1", 1, 2, {1, 2}, {0.5f, 0.5f}},
    {-1, 11, "n1", 1, 2, {3, 6}, {0.6f, 0.4f}},
    {-1, -1, "n10", 0, 1, {-1}, {1.0f}},
    {2, -1, "n11", 0, 2, {0, 4}, {0.0f, 0.0f}},
    {-1, -1, "zz", 1, 3, {1, 1, 0}, {0.1f, 0.1f, 0.1f}},
    {1, -1, "n7", 1, 1, {0}, {1.0f}},
    {-1, -1, "n0", 0, 0, {0}, {1.0f}},
    {-1, -1, "", 0, 2, {-1, 4}, {0.5f, 0.5f}},
    {-1, -1, "n5", 1, 4, {0, 1, 2, 3}, {0.4f, 0.1f, -0.2f, 0.7f}},
    {46, -1, "n9", 0, 1, {3}, {1.0f}},
    {1, -1, "n5", 0, 1, {4}, {1.0f}},
};

static void build_notes(World* w) {
    GmSoundNoteBank* bank = &w->mgr->m_noteBanks[2];
    bank->m_notes = (GmSoundNote*)std::calloc(kNotes, sizeof(GmSoundNote));
    bank->m_noteCount = kNotes;
    bank->m_byName.Init(kNotes);
    for (int i = 0; i < kNotes; i++) {
        const NoteSpec& s = kNoteSpecs[i];
        GmSoundNote* n = &bank->m_notes[i];
        std::sprintf(n->m_name, "n%d", i);
        std::strncpy(n->m_stop, s.stop, 8);
        n->m_emitter = s.emitter < 0 ? nullptr : w->emitters[s.emitter];
        n->m_conditional = s.conditional < 0 ? nullptr : w->emitters[s.conditional];
        n->m_singleton = s.singleton;
        n->m_choiceCount = s.count;
        n->m_choices = (GmSoundChoice*)std::calloc(s.count + 1, sizeof(GmSoundChoice));
        for (int k = 0; k < s.count; k++) {
            n->m_choices[k].m_segment = s.segments[k] < 0 ? nullptr : w->segments[s.segments[k]];
            n->m_choices[k].m_weight = s.weights[k];
        }
        n->m_choices[s.count].m_segment = w->segments[7];
        n->m_choices[s.count].m_weight = 0.5f;
        bank->m_byName.Insert(n->m_name, n);
    }
    w->notes = bank->m_notes;
}

static int g_plays_seen, g_note_calls;
static void run_notes() {
    for (uint32_t seed : {4357u, 1u, 99u}) {
        g_seed = seed;
        Pair p;
        for (int s = 0; s < 2; s++) build_notes(g_world[s]);
        both("find emitters", [&] { at<Fn0>(0x00137660)(O(mgr), nullptr); }, [&] { R(mgr)->FindEmitters(); });
        for (int round = 0; round < 14; round++) {
            // NoAudio on, then App gone, now and then.
            const int app = round == 5 ? 1 : 0;
            for (int s = 0; s < 2; s++) g_world[s]->app->m_noAudio = app;
            for (int n = 0; n < kNotes; n++) {
                for (int actor = -1; actor < kActors; actor++) {
                    g_context = "note n" + std::to_string(n) + " actor " + std::to_string(actor) + " round " + std::to_string(round) + " seed " + std::to_string(seed);
                    GmAcActor* ao = actor < 0 ? nullptr : O(actors[actor]);
                    GmAcActor* ar = actor < 0 ? nullptr : R(actors[actor]);
                    both("play note", [&] { at<Fn2>(0x001386D0)(O(mgr), nullptr, ao, &O(notes)[n]); }, [&] { R(mgr)->PlayNote(ar, &R(notes)[n]); });
                    compare_worlds("play note");
                    g_note_calls++;
                }
            }
            // Now and then let voices end, so that slots and conditionals free up.
            if (round % 3 == 2) {
                for (int s = 0; s < 2; s++)
                    for (FakeEmitter* e : g_world[s]->emitters)
                        if (e->m_id % 2 == round % 2) e->m_firstVoice = nullptr;
            }
        }
        for (int s = 0; s < 2; s++) g_plays_seen += g_world[s]->voiceCount * (s == 1);
    }
    g_seed = 4357;
    // A pick exactly equal to a running sum: seed 3308's first draw is 5000, a pick of 0.5 (analysis-local/scratch/gmsound/seedsearch.py). Equal goes on to the next choice.
    {
        g_seed = 3308;
        Pair p;
        for (int s = 0; s < 2; s++) {
            World* w = g_world[s];
            build_notes(w);
            GmSoundNote* n = &w->notes[0];
            n->m_emitter = w->emitters[1];
            n->m_choices[0].m_weight = 0.5f;
            n->m_choices[1].m_weight = 0.5f;
        }
        g_context = "note: pick equal to the sum";
        both("play note", [&] { at<Fn2>(0x001386D0)(O(mgr), nullptr, nullptr, &O(notes)[0]); }, [&] { R(mgr)->PlayNote(nullptr, &R(notes)[0]); });
        compare_worlds("pick equal to the sum");
        g_seed = 4357;
    }
}

// ---- playing directly ---------------------------------------------------------------------------------------------------
static void run_play() {
    Pair p;
    g_context = "find emitters";
    both("find emitters", [&] { at<Fn0>(0x00137660)(O(mgr), nullptr); }, [&] { R(mgr)->FindEmitters(); });
    compare_worlds("find emitters");
    both("play at no actor", [&] { at<Fn2>(0x00138010)(O(mgr), nullptr, nullptr, O(segments[2])); }, [&] { R(mgr)->PlayAtActor(nullptr, R(segments[2])); });
    compare_worlds("play at no actor");
    // Voice slots: take round, release, and run out.
    for (int i = 0; i < 45; i++) {
        g_context = "take voice slot " + std::to_string(i);
        void* x = nullptr;
        void* y = nullptr;
        if (i == 7) for (int s = 0; s < 2; s++) add_voice(g_world[s], 20, 0);
        both("take voice slot", [&] { x = at<FnP0>(0x001388B0)(O(mgr), nullptr); }, [&] { y = R(mgr)->TakeVoiceSlot(); });
        eq("take voice slot", eid((AtAudioEmitter*)x), eid((AtAudioEmitter*)y));
        compare_worlds("take voice slot");
        if (i % 5 == 4) {
            const int e = 10 + (i % 30);
            both("release voice slot", [&] { at<Fn1>(0x001367A0)(O(mgr), nullptr, O(emitters[e])); }, [&] { R(mgr)->ReleaseVoiceSlot(R(emitters[e])); });
            compare_worlds("release voice slot");
        }
    }
    both("release none", [&] { at<Fn1>(0x001367A0)(O(mgr), nullptr, nullptr); }, [&] { R(mgr)->ReleaseVoiceSlot(nullptr); });
    both("release other", [&] { at<Fn1>(0x001367A0)(O(mgr), nullptr, O(emitters[1])); }, [&] { R(mgr)->ReleaseVoiceSlot(R(emitters[1])); });
    compare_worlds("release");
    // Playing on an emitter, by name, at an actor.
    for (int app = 0; app < 3; app++) {
        for (int s = 0; s < 2; s++) {
            g_world[s]->app->m_noAudio = app == 1;
            install(s);
            if (app == 2) { if (s == 0) *(void**)(uintptr_t)0x003B360C = nullptr; else App::s_gmApp = nullptr; }
        }
        g_context = "play on emitter, app " + std::to_string(app);
        for (int e : {-1, 1, 30}) {
            for (int seg : {-1, 0, 5}) {
                AtAudioEmitter* eo = e < 0 ? nullptr : O(emitters[e]);
                AtAudioEmitter* er = e < 0 ? nullptr : R(emitters[e]);
                AtAudioSegment* so = seg < 0 ? nullptr : O(segments[seg]);
                AtAudioSegment* sr = seg < 0 ? nullptr : R(segments[seg]);
                both("play on emitter", [&] { at<Fn2>(0x00137F50)(O(mgr), nullptr, eo, so); }, [&] { R(mgr)->PlayOnEmitter(er, sr); });
                compare_worlds("play on emitter");
            }
        }
        for (const char* e : {"sfx", "glob2", "gone"}) {
            for (const char* seg : {"seg1", "seg6", "none"}) {
                both("play by name", [&] { at<FnNames>(0x00137FA0)(O(mgr), nullptr, e, seg); }, [&] { R(mgr)->PlayByName(e, seg); });
                compare_worlds("play by name");
            }
        }
        if (app == 2) {
            // PlayAtActor reads App without testing for it.
            both("play at actor, no app", [&] { at<Fn2>(0x00138010)(O(mgr), nullptr, O(actors[0]), O(segments[1])); }, [&] { R(mgr)->PlayAtActor(R(actors[0]), R(segments[1])); });
            continue;
        }
        for (int i = 0; i < 44; i++) {
            g_context = "play at actor " + std::to_string(i) + ", app " + std::to_string(app);
            const int actor = i % 5 - 1;
            GmAcActor* ao = actor < 0 ? nullptr : O(actors[actor]);
            GmAcActor* ar = actor < 0 ? nullptr : R(actors[actor]);
            both("play at actor", [&] { at<Fn2>(0x00138010)(O(mgr), nullptr, ao, O(segments[i % 8])); }, [&] { R(mgr)->PlayAtActor(ar, R(segments[i % 8])); });
            compare_worlds("play at actor");
            if (i % 11 == 10) {
                // A slot taken for an actor is not free to take for a node.
                for (int s = 0; s < 2; s++) for (FakeEmitter* e : g_world[s]->emitters) e->m_firstVoice = nullptr;
                void* x = nullptr;
                void* y = nullptr;
                both("take after actors", [&] { x = at<FnP0>(0x001388B0)(O(mgr), nullptr); }, [&] { y = R(mgr)->TakeVoiceSlot(); });
                eq("take after actors", eid((AtAudioEmitter*)x), eid((AtAudioEmitter*)y));
                compare_worlds("take after actors");
            }
        }
    }
    for (int s = 0; s < 2; s++) { g_world[s]->app->m_noAudio = 0; install(s); }
}

// Dynamic nodes, an actor's sound nodes, and playing at a node.
static int g_node_plays;
static void run_nodes_play() {
    for (float scale : {2.0f, 0.5f, 10.0f}) {
        Pair p;
        for (int s = 0; s < 2; s++) g_world[s]->owner->m_unknown14 = scale;
        both("find emitters", [&] { at<Fn0>(0x00137660)(O(mgr), nullptr); }, [&] { R(mgr)->FindEmitters(); });
        {
            g_context = "actor's nodes, null segment first";
            AtVector3 position = {1, 2, 3};
            both("actor's nodes null", [&] { at<FnCompPlay>(0x00194E40)(O(comps[3]), nullptr, nullptr, 0, &position, 0); }, [&] { R(comps[3])->Play(nullptr, 0, &position, 0); });
            compare_worlds("actor's nodes null");
        }
        for (int i = 0; i < 40; i++) {
            g_context = "actor's nodes " + std::to_string(i) + ", scale " + std::to_string(scale);
            const int actor = (i % 3 == 2) ? 3 : 0;
            const int index = i % 4 - 1;
            const int seg = i % 9 - 1;
            const int singleton = (i / 4) % 2;
            AtAudioSegment* so = seg < 0 ? nullptr : O(segments[seg]);
            AtAudioSegment* sr = seg < 0 ? nullptr : R(segments[seg]);
            AtVector3 position = {1, 2, 3};
            if (i == 20) for (int s = 0; s < 2; s++) g_world[s]->app->m_noAudio = 1;
            if (i == 22) for (int s = 0; s < 2; s++) g_world[s]->app->m_noAudio = 0;
            both("actor's nodes play", [&] { at<FnCompPlay>(0x00194E40)(O(comps[actor]), nullptr, so, index, &position, singleton); },
                 [&] { R(comps[actor])->Play(sr, index, &position, singleton); });
            compare_worlds("actor's nodes play");
            both("actor's nodes stop", [&] { at<FnStopSeg>(0x00194BB0)(O(comps[actor]), nullptr, so, index); }, [&] { R(comps[actor])->StopSegment(sr, index); });
            compare_worlds("actor's nodes stop");
            g_node_plays++;
            if (i % 10 == 9) for (int s = 0; s < 2; s++) for (FakeEmitter* e : g_world[s]->emitters) e->m_firstVoice = nullptr;
        }
        // Directly: a node of nowhere, null node or segment, a moved source, a node with a named emitter.
        g_context = "play node directly";
        GmSoundNode* no = &O(mgr)->m_nodeBanks[0].m_nodes[0];
        GmSoundNode* nr = &R(mgr)->m_nodeBanks[0].m_nodes[0];
        both("play node null", [&] { at<Fn2>(0x00138F70)(O(mgr), nullptr, nullptr, O(segments[1])); }, [&] { R(mgr)->PlayNode(nullptr, R(segments[1])); });
        both("play node null segment", [&] { at<Fn2>(0x00138F70)(O(mgr), nullptr, no, nullptr); }, [&] { R(mgr)->PlayNode(nr, nullptr); });
        for (int src = -1; src < kNodes; src++) {
            for (int seg = 0; seg < kSegments; seg++) {
                for (int s = 0; s < 2; s++) {
                    GmSoundNode* n = &g_world[s]->mgr->m_nodeBanks[0].m_nodes[1];
                    n->m_source = src < 0 ? nullptr : g_world[s]->nodes[src];
                    n->m_emitter = seg == 3 ? g_world[s]->emitters[2] : nullptr;
                    n->m_range = seg == 4 ? 100.0f : 0.5f;
                    for (FakeEmitter* e : g_world[s]->emitters) if (seg % 2) e->m_firstVoice = nullptr;
                    g_world[s]->nodes[(src + 8) % kNodes]->m_flags |= AtSceneNode::kMoved;
                }
                g_context = "play node, source " + std::to_string(src) + ", segment " + std::to_string(seg) + ", scale " + std::to_string(scale);
                both("play node", [&] { at<Fn2>(0x00138F70)(O(mgr), nullptr, &O(mgr)->m_nodeBanks[0].m_nodes[1], O(segments[seg])); },
                     [&] { R(mgr)->PlayNode(&R(mgr)->m_nodeBanks[0].m_nodes[1], R(segments[seg])); });
                compare_worlds("play node");
                g_node_plays++;
            }
        }
        // Taking a dynamic node: the pool runs out; the float is not used.
        for (int i = 0; i < 5; i++) {
            g_context = "take dynamic node " + std::to_string(i);
            void* x = nullptr;
            void* y = nullptr;
            both("take dynamic node", [&] { x = at<FnTake>(0x00138F00)(O(mgr), nullptr, O(actors[i % 2]), 1.5f * i); }, [&] { y = R(mgr)->TakeDynamicNode(R(actors[i % 2]), 1.5f * i); });
            eq("take dynamic node", node_place(g_world[0], (GmSoundNode*)x), node_place(g_world[1], (GmSoundNode*)y));
            compare_worlds("take dynamic node");
        }
        g_context = "actor's nodes, pool out, singleton";
        for (int s = 0; s < 2; s++) g_world[s]->comps[3]->m_nodes[0] = nullptr;
        AtVector3 position = {1, 2, 3};
        both("pool out", [&] { at<FnCompPlay>(0x00194E40)(O(comps[3]), nullptr, O(segments[1]), 0, &position, 1); }, [&] { R(comps[3])->Play(R(segments[1]), 0, &position, 1); });
        compare_worlds("pool out");
    }
}

// Stopping a bank's voices, and letting go of a database.
static void run_stop() {
    Pair p;
    both("find emitters", [&] { at<Fn0>(0x00137660)(O(mgr), nullptr); }, [&] { R(mgr)->FindEmitters(); });
    for (int s = 0; s < 2; s++) {
        World* w = g_world[s];
        add_voice(w, 1, 0);
        add_voice(w, 1, 3);
        add_voice(w, 1, 1);
        add_voice(w, 4, 4);
        add_voice(w, 12, 3);
        add_voice(w, 30, 1);
        add_voice(w, 5, 0);   // collsn1: not stopped by RemoveDatabase
        add_voice(w, 47, 0);  // glob2: neither
        add_voice(w, 1, 6);   // a segment with no bank
        add_voice(w, 4, 2);   // bank 2, after the null in the second list
        add_voice(w, 0, 1);   // ui, the fifth named
    }
    g_context = "stop bank voices";
    both("stop null emitter", [&] { at<Fn2>(0x00138290)(O(mgr), nullptr, nullptr, O(banks[0])); }, [&] { R(mgr)->StopBankVoices(nullptr, R(banks[0])); });
    both("stop null bank", [&] { at<Fn2>(0x00138290)(O(mgr), nullptr, O(emitters[1]), nullptr); }, [&] { R(mgr)->StopBankVoices(R(emitters[1]), nullptr); });
    both("stop bank 1", [&] { at<Fn2>(0x00138290)(O(mgr), nullptr, O(emitters[1]), O(banks[1])); }, [&] { R(mgr)->StopBankVoices(R(emitters[1]), R(banks[1])); });
    compare_worlds("stop bank");
    both("stop bank 2, none", [&] { at<Fn2>(0x00138290)(O(mgr), nullptr, O(emitters[1]), O(banks[2])); }, [&] { R(mgr)->StopBankVoices(R(emitters[1]), R(banks[2])); });
    compare_worlds("stop bank none");
    g_context = "remove database";
    both("remove database", [&] { at<Fn1>(0x00139F90)(O(mgr), nullptr, O(dbs[0])); }, [&] { R(mgr)->RemoveDatabase(R(dbs[0])); });
    compare_worlds("remove database");
    both("remove database again", [&] { at<Fn1>(0x00139F90)(O(mgr), nullptr, O(dbs[0])); }, [&] { R(mgr)->RemoveDatabase(R(dbs[0])); });
    both("remove second", [&] { at<Fn1>(0x00139F90)(O(mgr), nullptr, O(dbs[1])); }, [&] { R(mgr)->RemoveDatabase(R(dbs[1])); });
    compare_worlds("remove second");
    // A voice whose stop never takes: the loader gives up, and both sides crash alike.
    g_context = "stop forever";
    for (int s = 0; s < 2; s++) { add_voice(g_world[s], 2, 0); g_world[s]->loader.m_waits = 0; }
    struct Sticky { static void __fastcall stop(void*, void*, AtAudioVoice*) { lg('V'); } };
    void** saved[2];
    for (int s = 0; s < 2; s++) {
        saved[s] = *(void***)g_world[s]->emitters[2];
        void** table = (void**)std::malloc(26 * sizeof(void*));
        std::memcpy(table, saved[s], 26 * sizeof(void*));
        table[23] = (void*)&Sticky::stop;
        *(void***)g_world[s]->emitters[2] = table;
    }
    both("stop forever", [&] { at<Fn2>(0x00138290)(O(mgr), nullptr, O(emitters[2]), O(banks[0])); }, [&] { R(mgr)->StopBankVoices(R(emitters[2]), R(banks[0])); });
    for (int s = 0; s < 2; s++) *(void***)g_world[s]->emitters[2] = saved[s];
}

// ---- the readers ----------------------------------------------------------------------------------------------------------
static int g_goo_files, g_nodes_seen, g_with_segment, g_collisions_seen, g_instances_seen;

static void set_file(const std::vector<unsigned char>& data) {
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
}
static std::vector<unsigned char> bytes(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

static void count_nodes(const GmSoundNodeBank* b) {
    if (!b->m_nodes) return;
    for (unsigned i = 0; i < b->m_count; i++) { g_nodes_seen++; if (b->m_nodes[i].m_segment) g_with_segment++; }
}

// Every quoted name of a text, at most eight bytes, once each.
static std::vector<std::string> strings_of(const std::vector<unsigned char>& data) {
    std::vector<std::string> out;
    std::string now;
    bool in = false;
    for (unsigned char c : data) {
        if (c == '"') {
            if (in && !now.empty() && now.size() <= 8) {
                bool have = false;
                for (const std::string& s : out) have = have || s == now;
                if (!have) out.push_back(now);
            }
            now.clear();
            in = !in;
        } else if (in) {
            now += (char)c;
        }
    }
    return out;
}

// Load into bank 5, register, play each node, free and release. Every quoted name of the text is a segment in the second database, so that real files find theirs.
static void run_goo(const std::string& name, const std::vector<unsigned char>& data, bool stop) {
    g_context = name;
    set_file(data);
    Pair p;
    const std::vector<std::string> names = strings_of(data);
    for (int s = 0; s < 2; s++) {
        TestSegments& list = g_world[s]->dbs[1]->m_list;
        list.Clear();
        list.Init((unsigned)names.size() + 16);
        for (int i = 5; i < kSegments; i++) {
            char n[8];
            std::sprintf(n, "seg%d", i);
            list.Insert(n, g_world[s]->segments[i]);
        }
        int k = 0;
        for (const std::string& n : names) if (n.compare(0, 3, "seg") != 0 && (k++ % 4) != 3) list.Insert(n.c_str(), g_world[s]->segments[k % kSegments]);
    }
    both("find emitters", [&] { at<Fn0>(0x00137660)(O(mgr), nullptr); }, [&] { R(mgr)->FindEmitters(); });
    g_stop_at_fatal = stop;
    GmSoundNodeBank* bo = &O(mgr)->m_nodeBanks[5];
    GmSoundNodeBank* br = &R(mgr)->m_nodeBanks[5];
    const bool crashed = both("load nodes", [&] { at<FnPath>(0x0013AA80)(O(mgr), nullptr, bo, "AUDIO\\FILE"); }, [&] { R(mgr)->LoadNodes(br, "AUDIO\\FILE"); });
    g_stop_at_fatal = false;
    compare_worlds("load nodes");
    if (crashed) { bo->m_nodes = br->m_nodes = nullptr; bo->m_count = br->m_count = 0; return; }
    both("register nodes", [&] { at<Fn1>(0x00138A10)(O(mgr), nullptr, bo); }, [&] { R(mgr)->RegisterNodes(br); });
    compare_worlds("register nodes");
    count_nodes(br);
    // Each node plays its segment, taking a voice slot or using its named emitter.
    for (unsigned i = 0; i < br->m_count && i < 24; i++) {
        for (int s = 0; s < 2; s++) {
            GmSoundNode* n = &g_world[s]->mgr->m_nodeBanks[5].m_nodes[i];
            n->m_source = g_world[s]->nodes[i % kNodes];
        }
        both("play loaded node", [&] { at<Fn2>(0x00138F70)(O(mgr), nullptr, &bo->m_nodes[i], bo->m_nodes[i].m_segment); },
             [&] { R(mgr)->PlayNode(&br->m_nodes[i], br->m_nodes[i].m_segment); });
    }
    compare_worlds("play loaded nodes");
    both("free nodes", [&] { at<Fn1>(0x00138E50)(O(mgr), nullptr, bo); }, [&] { R(mgr)->FreeNodes(br); });
    compare_worlds("free nodes");
    both("free nodes again", [&] { at<Fn1>(0x00138E50)(O(mgr), nullptr, bo); }, [&] { R(mgr)->FreeNodes(br); });
    both("release node bank", [&] { at<Fn1>(0x00136810)(O(mgr), nullptr, bo); }, [&] { R(mgr)->ReleaseNodeBank(br); });
    compare_worlds("release node bank");
}

static void run_goc(const std::string& name, const std::vector<unsigned char>& data, bool stop, const char* goi = nullptr) {
    g_context = name;
    set_file(data);
    Pair p;
    both("find emitters", [&] { at<Fn0>(0x00137660)(O(mgr), nullptr); }, [&] { R(mgr)->FindEmitters(); });
    g_stop_at_fatal = stop;
    void* x = nullptr;
    void* y = nullptr;
    const bool crashed = both("load collisions", [&] { x = at<FnP1>(0x00139610)(O(mgr), nullptr, "AUDIO\\HIT"); }, [&] { y = R(mgr)->LoadCollisionSounds("AUDIO\\HIT"); });
    g_stop_at_fatal = false;
    compare_worlds("load collisions");
    if (crashed) { for (int s = 0; s < 2; s++) g_world[s]->mgr->m_collisionBanks[0].m_sounds = nullptr; return; }
    eq("collision bank", x ? (GmCollisionSoundBank*)x - O(mgr)->m_collisionBanks : -1, y ? (GmCollisionSoundBank*)y - R(mgr)->m_collisionBanks : -1);
    if (y) g_collisions_seen += ((GmCollisionSoundBank*)y)->m_count;
    if (goi) {
        const std::vector<unsigned char> text = bytes(goi);
        set_file(text);
        void* u = nullptr;
        void* v = nullptr;
        g_stop_at_fatal = stop;
        const bool c2 = both("load instances", [&] { u = at<FnP1>(0x00139C40)(O(mgr), nullptr, "AUDIO\\INST"); }, [&] { v = R(mgr)->LoadInstanceSounds("AUDIO\\INST"); });
        g_stop_at_fatal = false;
        compare_worlds("load instances");
        if (!c2 && u && v) {
            g_instances_seen += ((GmInstanceSoundBank*)v)->m_count;
            for (int s = 0; s < 2; s++) {
                GmAudioData* m = g_world[s]->mgr;
                for (int b : {3, 4}) {
                    GmCollisionSound* c = (GmCollisionSound*)std::calloc(1, sizeof(GmCollisionSound));
                    std::strcpy(c->m_name, "crate");
                    m->m_collisionBanks[b].m_sounds = c;
                    m->m_collisionBanks[b].m_count = 1;
                    m->m_collisionBanks[b].m_used = b == 3 ? 1 : 0;
                }
            }
            both("register instances", [&] { at<Fn1>(0x00136A10)(O(mgr), nullptr, u); }, [&] { R(mgr)->RegisterInstanceSounds((GmInstanceSoundBank*)v); });
            compare_worlds("register instances");
            both("free instances", [&] { at<Fn1>(0x00136B30)(O(mgr), nullptr, u); }, [&] { R(mgr)->FreeInstanceSounds((GmInstanceSoundBank*)v); });
            compare_worlds("free instances");
            both("free instances again", [&] { at<Fn1>(0x00136B30)(O(mgr), nullptr, u); }, [&] { R(mgr)->FreeInstanceSounds((GmInstanceSoundBank*)v); });
        }
        if (c2) for (int s = 0; s < 2; s++) g_world[s]->mgr->m_instanceBanks[0].m_sounds = nullptr;
    }
    if (x && y) {
        both("free collisions", [&] { at<Fn1>(0x00139B50)(O(mgr), nullptr, x); }, [&] { R(mgr)->FreeCollisionSounds((GmCollisionSoundBank*)y); });
        compare_worlds("free collisions");
        both("free collisions again", [&] { at<Fn1>(0x00139B50)(O(mgr), nullptr, x); }, [&] { R(mgr)->FreeCollisionSounds((GmCollisionSoundBank*)y); });
        compare_worlds("free collisions again");
    }
}

static void run_goi(const std::string& name, const std::vector<unsigned char>& data, bool stop) {
    g_context = name;
    set_file(data);
    Pair p;
    g_stop_at_fatal = stop;
    void* u = nullptr;
    void* v = nullptr;
    const bool crashed = both("load instances", [&] { u = at<FnP1>(0x00139C40)(O(mgr), nullptr, "AUDIO\\INST"); }, [&] { v = R(mgr)->LoadInstanceSounds("AUDIO\\INST"); });
    g_stop_at_fatal = false;
    compare_worlds("load instances");
    if (crashed) { for (int s = 0; s < 2; s++) g_world[s]->mgr->m_instanceBanks[0].m_sounds = nullptr; return; }
    eq("instance bank", u ? (GmInstanceSoundBank*)u - O(mgr)->m_instanceBanks : -1, v ? (GmInstanceSoundBank*)v - R(mgr)->m_instanceBanks : -1);
    if (u && v) {
        g_instances_seen += ((GmInstanceSoundBank*)v)->m_count;
        both("free instances", [&] { at<Fn1>(0x00136B30)(O(mgr), nullptr, u); }, [&] { R(mgr)->FreeInstanceSounds((GmInstanceSoundBank*)v); });
        compare_worlds("free instances");
    }
}

// Banks: all sixty node banks, ten of each sound kind, then none left.
static void run_banks() {
    Pair p;
    g_context = "node banks";
    for (int s = 0; s < 2; s++) g_world[s]->mgr->m_nodeBanks[1].m_used = 2;
    for (int i = 0; i <= GmAudioData::kNodeBanks; i++) {
        void* x = nullptr;
        void* y = nullptr;
        both("allocate node bank", [&] { x = at<FnP0>(0x001367E0)(O(mgr), nullptr); }, [&] { y = R(mgr)->AllocateNodeBank(); });
        eq("allocate node bank", x ? (GmSoundNodeBank*)x - O(mgr)->m_nodeBanks : -1, y ? (GmSoundNodeBank*)y - R(mgr)->m_nodeBanks : -1);
    }
    for (int i : {0, 33, 59}) both("release node bank", [&] { at<Fn1>(0x00136810)(O(mgr), nullptr, &O(mgr)->m_nodeBanks[i]); }, [&] { R(mgr)->ReleaseNodeBank(&R(mgr)->m_nodeBanks[i]); });
    int outside[3];
    both("release outside", [&] { at<Fn1>(0x00136810)(O(mgr), nullptr, outside); }, [&] { R(mgr)->ReleaseNodeBank((GmSoundNodeBank*)outside); });
    compare_worlds("node banks");
    g_context = "instance banks";
    for (int i = 0; i <= GmAudioData::kSoundBanks; i++) {
        void* x = nullptr;
        void* y = nullptr;
        const unsigned count = i == 3 ? 0 : i + 1;
        both("allocate instance bank", [&] { x = at<FnPU>(0x00139A90)(O(mgr), nullptr, count); }, [&] { y = R(mgr)->AllocateInstanceBank(count); });
        eq("allocate instance bank", x ? (GmInstanceSoundBank*)x - O(mgr)->m_instanceBanks : -1, y ? (GmInstanceSoundBank*)y - R(mgr)->m_instanceBanks : -1);
        compare_worlds("allocate instance bank");
    }
    // No memory: raised, and the bank handed back neither used nor counted. No heap: raised, and allocated regardless.
    for (int s = 0; s < 2; s++) g_world[s]->mgr->m_instanceBanks[4].m_used = 0;
    g_fail_push = 1;
    {
        void* x = nullptr;
        void* y = nullptr;
        both("instance bank, no memory", [&] { x = at<FnPU>(0x00139A90)(O(mgr), nullptr, 3); }, [&] { y = R(mgr)->AllocateInstanceBank(3); });
        eq("instance bank, no memory", x ? (GmInstanceSoundBank*)x - O(mgr)->m_instanceBanks : -1, y ? (GmInstanceSoundBank*)y - R(mgr)->m_instanceBanks : -1);
    }
    g_fail_push = 0;
    compare_worlds("instance bank, no memory");
    *(void**)(uintptr_t)0x003B3548 = nullptr;
    GmAudioData::s_heap = nullptr;
    {
        void* x = nullptr;
        void* y = nullptr;
        both("instance bank, no heap", [&] { x = at<FnPU>(0x00139A90)(O(mgr), nullptr, 2); }, [&] { y = R(mgr)->AllocateInstanceBank(2); });
        eq("instance bank, no heap", x ? (GmInstanceSoundBank*)x - O(mgr)->m_instanceBanks : -1, y ? (GmInstanceSoundBank*)y - R(mgr)->m_instanceBanks : -1);
    }
    *(void**)(uintptr_t)0x003B3548 = &g_audio_heap_token;
    GmAudioData::s_heap = &g_audio_heap_token;
    compare_worlds("instance bank, no heap");
    // A collision bank whose used flag is 2 is in use.
    g_context = "collision bank used 2";
    const std::vector<unsigned char> goc = bytes("CollisionSoundNodes[1]{CollisionSoundNode \"a\"{}}");
    set_file(goc);
    for (int s = 0; s < 2; s++) g_world[s]->mgr->m_collisionBanks[0].m_used = 2;
    {
        void* x = nullptr;
        void* y = nullptr;
        both("collisions, used 2", [&] { x = at<FnP1>(0x00139610)(O(mgr), nullptr, "F"); }, [&] { y = R(mgr)->LoadCollisionSounds("F"); });
        eq("collisions, used 2", x ? (GmCollisionSoundBank*)x - O(mgr)->m_collisionBanks : -1, y ? (GmCollisionSoundBank*)y - R(mgr)->m_collisionBanks : -1);
        compare_worlds("collisions, used 2");
    }
    // Every collision and instance bank in use: the loaders give null, after opening the file.
    g_context = "all sound banks in use";
    for (int s = 0; s < 2; s++) for (int i = 0; i < GmAudioData::kSoundBanks; i++) { g_world[s]->mgr->m_collisionBanks[i].m_used = 1; g_world[s]->mgr->m_instanceBanks[i].m_used = 1; }
    void* x = nullptr;
    void* y = nullptr;
    both("collisions, none free", [&] { x = at<FnP1>(0x00139610)(O(mgr), nullptr, "F"); }, [&] { y = R(mgr)->LoadCollisionSounds("F"); });
    eq("collisions, none free", x != nullptr, y != nullptr);
    const std::vector<unsigned char> goi = bytes("InstanceSoundNodes[1]{InstanceSoundNode \"a\"{}}");
    set_file(goi);
    both("instances, none free", [&] { x = at<FnP1>(0x00139C40)(O(mgr), nullptr, "F"); }, [&] { y = R(mgr)->LoadInstanceSounds("F"); });
    eq("instances, none free", x != nullptr, y != nullptr);
    compare_worlds("all in use");
    // RegisterNodes on null crashes, as the manifest's item may give it.
    g_context = "register null";
    both("register null", [&] { at<Fn1>(0x00138A10)(O(mgr), nullptr, nullptr); }, [&] { R(mgr)->RegisterNodes(nullptr); });
    // Freeing nodes whose emitters still play: a wait for each, and a voice slot given back for 3D ones.
    g_context = "free playing nodes";
    both("find emitters", [&] { at<Fn0>(0x00137660)(O(mgr), nullptr); }, [&] { R(mgr)->FindEmitters(); });
    for (int s = 0; s < 2; s++) {
        World* w = g_world[s];
        GmSoundNodeBank* b = &w->mgr->m_nodeBanks[7];
        b->m_nodes = (GmSoundNode*)std::calloc(4, sizeof(GmSoundNode));
        b->m_count = 4;
        b->m_nodes[0].m_emitter = w->emitters[14];  // kind 1
        b->m_nodes[1].m_emitter = w->emitters[1];   // kind 0
        b->m_nodes[3].m_emitter = w->emitters[15];
        w->emitters[15]->m_kind = 2;  // neither 0 nor 1
        add_voice(w, 14, 1);
        add_voice(w, 14, 2);
        add_voice(w, 1, 3);
        w->mgr->m_voiceSlots[4].m_state = GmSoundVoiceSlot::kNode;
    }
    both("free playing nodes", [&] { at<Fn1>(0x00138E50)(O(mgr), nullptr, &O(mgr)->m_nodeBanks[7]); }, [&] { R(mgr)->FreeNodes(&R(mgr)->m_nodeBanks[7]); });
    compare_worlds("free playing nodes");
}

static void run_texts() {
    static const char* const goo[] = {
        "SoundNodes[0]{}",
        "SoundNodes[1]{Node \"a\"{}}",
        "SoundNodes[1]{Node \"12345678\"{}}",
        "SoundNodes[1]{Node \"a\"{Actor \"act\"}}",
        "SoundNodes[1]{Node \"a\"{Segment \"seg1\"}}",
        "SoundNodes[1]{Node \"a\"{Segment \"seg6\" Emitter \"sfx\"}}",
        "SoundNodes[1]{Node \"a\"{Emitter \"gone\"}}",
        "SoundNodes[1]{Node \"a\"{ActorEmitter}}",
        "SoundNodes[1]{Node \"a\"{VelocityParm \"v\" 1.5 -2.0}}",
        "SoundNodes[1]{Node \"a\"{AngularVelocityParm \"w\" 0.25 8.0}}",
        "SoundNodes[1]{Node \"a\"{CustomParm \"velocity\" 0.0 4.0}}",
        "SoundNodes[1]{Node \"a\"{AnalogInputParm \"x\" \"y\" \"z\" 3.5 -1.5}}",
        "SoundNodes[1]{Node \"a\"{MaxDistanceFromListener 30.0 TriggeredOnly}}",
        "SoundNodes[1]{Node \"a\"{PauseWhenAirborne}}",
        "SoundNodes[1]{Node \"a\"{MaxDistanceFromListener TriggeredOnly}}",
        "SoundNodes[1]{Node \"a\"{TriggeredOnly ActorEmitter}}",
        "SoundNodes[1]{Node \"a\"{MinDistanceFromListener 1.0 Segment \"seg2\"}}",
        "SoundNodes[1]{Node \"a\"{other 1 2.5 \"text\" , ; PauseWhenAirborne}}",
        "SoundNodes[1]{SoundNodes \"a\"{Node SoundNodes Segment \"seg3\"}}",
        "SoundNodes[3]{Node \"a\"{Segment \"seg0\"} Node \"b\"{Segment \"seg4\" Actor \"bb\"} Node \"c\"{Segment \"none\" Emitter \"glob1\" PauseWhenAirborne}}",
        "SoundNodes[1]{Node \"a\"{Segment \"seg1\"}} SoundNodes[2]{Node \"b\"{Segment \"seg2\"} Node \"c\"{Segment \"seg5\"}}",
        "stray words SoundNodes[1]{Node \"a\"{Segment \"seg3\"}} 1 2 3",
        "soundnodes[1]{node \"a\"{segment \"seg1\" velocityparm \"v\" 1.0 2.0}}",
        "SoundNodes[1]{Node \"a\"{Segment \"seg1\"",
        "SoundNodes[2]{Node \"a\"{}}",
        "",
    };
    for (const char* t : goo) run_goo(std::string("goo [") + t + "]", bytes(t), true);
    static const char* const goo_bad[] = {
        "SoundNodes",
        "SoundNodes[x]{}",
        "SoundNodes[1]{Node 5 {}}",
        "SoundNodes[1]{Node \"123456789\"{}}",
        "SoundNodes[1]{Node \"a\" Segment}}",
        "SoundNodes[1]{Node \"a\"{Actor 5}}",
        "SoundNodes[1]{Node \"a\"{Segment 5}}",
        "SoundNodes[1]{Node \"a\"{Emitter 5}}",
        "SoundNodes[1]{Node \"a\"{VelocityParm \"v\" 1}}",
        "SoundNodes[1]{Node \"a\"{VelocityParm 5}}",
        "SoundNodes[1]{Node \"a\"{AngularVelocityParm \"v\" 1.0 \"x\"}}",
        "SoundNodes[1]{Node \"a\"{CustomParm \"v\" \"x\"}}",
        "SoundNodes[1]{Node \"a\"{AnalogInputParm \"x\" \"y\" 1.0 2.0 3.0}}",
        "SoundNodes[1]{Node \"a\"{AnalogInputParm \"x\" \"y\" \"z\" 1.0 \"w\"}}",
    };
    for (const char* t : goo_bad) run_goo(std::string("goo bad [") + t + "]", bytes(t), true);
    // A file that will not open: reported, and then, if that comes back, a null tokenizer.
    g_fs->has = false;
    g_fs->open_status = AtIoFile::kNotFound;
    run_goo("goo: no file, stopped", {}, true);
    run_goo("goo: no file, report comes back", {}, false);
    run_goc("goc: no file", {}, false);
    run_goi("goi: no file", {}, false);
    g_fs->has = true;
    g_fs->open_status = 0;
    // No heap, and memory refused.
    *(void**)(uintptr_t)0x003B3548 = nullptr;
    GmAudioData::s_heap = nullptr;
    run_goo("goo: no heap", bytes("SoundNodes[1]{Node \"a\"{Segment \"seg1\"}}"), true);
    run_goc("goc: no heap", bytes("CollisionSoundNodes[1]{CollisionSoundNode \"a\"{MaterialSegments[1]{\"m\" \"s\"}}}"), true);
    run_goi("goi: no heap", bytes("InstanceSoundNodes[1]{InstanceSoundNode \"a\"{Instances[1]{\"i\"}}}"), true);
    *(void**)(uintptr_t)0x003B3548 = &g_audio_heap_token;
    GmAudioData::s_heap = &g_audio_heap_token;
    g_fail_push = 1;
    run_goo("goo: no memory", bytes("SoundNodes[1]{Node \"a\"{Segment \"seg1\"}}"), true);
    for (int which : {1, 2}) {
        g_fail_push = which;
        run_goc("goc: no memory " + std::to_string(which), bytes("CollisionSoundNodes[1]{CollisionSoundNode \"a\"{MaterialSegments[1]{\"m\" \"s\"}}}"), true);
    }
    for (int which : {1, 2, 3}) {
        g_fail_push = which;
        run_goi("goi: no memory " + std::to_string(which), bytes("InstanceSoundNodes[1]{InstanceSoundNode \"a\"{Instances[1]{\"i\"}}}"), true);
    }
    g_fail_push = 0;

    static const char* const goc[] = {
        "CollisionSoundNodes[0]{}",
        "CollisionSoundNodes[1]{CollisionSoundNode \"crate\"{}}",
        "CollisionSoundNodes[1]{CollisionSoundNode \"crate\"{DedicatedEmitter}}",
        "CollisionSoundNodes[1]{CollisionSoundNode \"crate\"{DefaultSegment \"soft\" \"mid\" \"hard\"}}",
        "CollisionSoundNodes[1]{CollisionSoundNode \"crate\"{MaterialSegments[0]{}}}",
        "CollisionSoundNodes[1]{CollisionSoundNode \"crate\"{MaterialSegments[2]{\"wood\" \"thud\" \"metal\" \"clang\"}}}",
        "CollisionSoundNodes[1]{CollisionSoundNode \"crate\"{PlayerInitiatedOnly}}",
        "CollisionSoundNodes[1]{CollisionSoundNode \"crate\"{RagdollOnly}}",
        "CollisionSoundNodes[1]{CollisionSoundNode \"crate\"{ImpulseThresholds 1.0 -2.0 3.5}}",
        "CollisionSoundNodes[1]{CollisionSoundNode \"crate\"{other 5 \"x\" RagdollOnly}}",
        "CollisionSoundNodes[2]{CollisionSoundNode \"a\"{DedicatedEmitter RagdollOnly} CollisionSoundNode \"b\"{DedicatedEmitter PlayerInitiatedOnly DefaultSegment \"x\" \"y\" \"z\"}}",
        "CollisionSoundNodes[1]{CollisionSoundNode \"a\"{}} CollisionSoundNodes[1]{CollisionSoundNode \"b\"{RagdollOnly}}",
        "words CollisionSoundNodes[1]{Other \"a\"{}} 5",
        "CollisionSoundNodes[1]{CollisionSoundNode \"a\"{RagdollOnly",
        "",
    };
    for (const char* t : goc) run_goc(std::string("goc [") + t + "]", bytes(t), true);
    static const char* const goc_bad[] = {
        "CollisionSoundNodes[x]{}",
        "CollisionSoundNodes[1]{CollisionSoundNode 5 {}}",
        "CollisionSoundNodes[1]{CollisionSoundNode \"a\" RagdollOnly}}",
        "CollisionSoundNodes[1]{CollisionSoundNode \"a\"{DefaultSegment \"x\" 5}}",
        "CollisionSoundNodes[1]{CollisionSoundNode \"a\"{MaterialSegments 2}}",
        "CollisionSoundNodes[1]{CollisionSoundNode \"a\"{MaterialSegments[1]{\"m\" 5}}}",
        "CollisionSoundNodes[1]{CollisionSoundNode \"a\"{MaterialSegments[1]{\"m\" \"s\" \"t\"}}}",
        "CollisionSoundNodes[1]{CollisionSoundNode \"a\"{ImpulseThresholds 1.0 2}}",
    };
    for (const char* t : goc_bad) run_goc(std::string("goc bad [") + t + "]", bytes(t), true);

    // Instance sounds, with collision sounds to find, some of them in a second bank that wins.
    static const char* const goi[] = {
        "InstanceSoundNodes[0]{}",
        "InstanceSoundNodes[1]{InstanceSoundNode \"crate\"{}}",
        "InstanceSoundNodes[1]{InstanceSoundNode \"crate\"{Instances[0]{}}}",
        "InstanceSoundNodes[1]{InstanceSoundNode \"crate\"{Instances[2]{\"crate01\" \"crate02\"}}}",
        "InstanceSoundNodes[3]{InstanceSoundNode \"crate\"{Instances[1]{\"c\"}} InstanceSoundNode \"barrel\"{other 5 Instances[1]{\"b\"}} InstanceSoundNode \"gone\"{}}",
        "InstanceSoundNodes[1]{InstanceSoundNode \"crate\"{Instances[1]{\"a\"} Instances[2]{\"b\" \"c\"}}} trailing",
    };
    const char* collisions = "CollisionSoundNodes[2]{CollisionSoundNode \"crate\"{} CollisionSoundNode \"barrel\"{RagdollOnly}}";
    for (const char* t : goi) run_goc(std::string("goi [") + t + "]", bytes(collisions), true, t);
    static const char* const goi_bad[] = {
        "Other[1]{}",
        "InstanceSoundNodes 1",
        "InstanceSoundNodes[1]{Other \"a\"{}}",
        "InstanceSoundNodes[1]{InstanceSoundNode 5 {}}",
        "InstanceSoundNodes[1]{InstanceSoundNode \"a\" Instances}}",
        "InstanceSoundNodes[1]{InstanceSoundNode \"a\"{Instances 1}}",
        "InstanceSoundNodes[1]{InstanceSoundNode \"a\"{Instances[1]{5}}}",
        "InstanceSoundNodes[1]{InstanceSoundNode \"a\"{Instances[1]{\"x\" \"y\"}}}",
        "InstanceSoundNodes[1]{InstanceSoundNode \"a\"{}",
        "InstanceSoundNodes[2]{InstanceSoundNode \"a\"{}}",
    };
    for (const char* t : goi_bad) run_goi(std::string("goi bad [") + t + "]", bytes(t), true);
    // Text past the last instance block that never closes spins in both: not run. See the notes.
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
    if (pos.size() != 2) { std::printf("usage: oracle_gmsound <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push_counting);
    redirect(0x00011260, (const void*)o_pop_counting);
    redirect(0x00011070, (const void*)o_fatal_stops);
    redirect(0x00011170, (const void*)o_raise);
    redirect((uint32_t)(uintptr_t)(void (*)(const char*, int, int, int))AtFatal, (const void*)r_fatal_stops);
    redirect((uint32_t)(uintptr_t)(void (*)(void*))AtPushHeap, (const void*)r_push_counting);
    redirect((uint32_t)(uintptr_t)(void (*)())AtPopHeap, (const void*)r_pop_counting);
    at<void(__cdecl*)()>(0x00125BC7)();
    // Armed after the redirects, so that no entry is overwritten by one.
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
    *(void**)(uintptr_t)0x003B3548 = &g_audio_heap_token;
    GmAudioData::s_heap = &g_audio_heap_token;
    g_fill = 0xCD;
    install_matrices();
    build_tables();

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_random();
    run_atlas();
    run_notes();
    run_play();
    run_nodes_play();
    run_stop();
    run_banks();
    run_texts();
    const int synthetic_failures = g_failures;

    int files = 0;
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    std::vector<std::vector<unsigned char>> keep;
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".GOO\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::vector<unsigned char> data((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        std::fread(data.data(), 1, data.size(), f);
        std::fclose(f);
        run_goo(line, data, false);
        files++;
    }
    std::fclose(index);
    if (files == 0) { std::printf("no files listed\n"); g_failures++; }
    g_goo_files = files;
    report_entries();
    std::printf("%s: %d .GOO files, %d nodes (%d with a segment), %d collision sounds, %d instance sounds, %d note plays (%d voices made), %d node plays, %d random draws, %d stops at a crash, "
                "%d wild slot calls; %llu comparisons, %d differences (%d in written cases)\n",
                g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_goo_files, g_nodes_seen, g_with_segment, g_collisions_seen, g_instances_seen, g_note_calls, g_plays_seen, g_node_plays,
                g_randoms_seen, g_crashes_seen, g_wild_calls, g_checks, g_failures, synthetic_failures);
    return g_failures ? 1 : 0;
}
