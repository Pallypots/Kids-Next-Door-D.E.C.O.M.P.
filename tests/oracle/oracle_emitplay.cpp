// Differential oracle for a sound emitter's voices, the voice classes, the voice factory, the sound system's lists and the Xbox bank's play slots (Windows x86, local only). See docs/verification.md.
//
//   oracle_emitplay <default.xbe> [--entries FILE] [--mutate]
//
// Two worlds, one for each side, built alike: a sound owner, its factory with twelve voices, the system, three emitters, three banks and six segments. On the original's side every object is made by
// the original's constructor and runs the original's code through its own vftables; on the recovered side, the recovered classes. Each call runs on both, and after each the worlds are compared
// field by field, every pointer by the identity of what it points to.
//
// Beneath both sides, doubles: XACT's seven entry points that a voice reaches, and the stream's stop (0x002422BF), each logging its arguments, the play description's and the variables' fields
// and the variable names, and handing back cues and statuses the case chose; the matrix table's GetPosition. So nothing here plays: a green result says both sides reach XACT with the same
// arguments in the same order and leave the same state. What XACT then does is not modelled, nor what the system's update (not recovered) does; the cases set the voice states it would set.
#define NOMINMAX
#include <windows.h>

#include <float.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <vector>

#include "at/AtAudioDatabase.h"
#include "at/AtCoordSys.h"
#include "at/AtNameTableOwner.h"
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
// NaNs differing only in sign count as alike, and are counted: which sign comes out depends on operand order and x87 or SSE rules (docs/verification.md, the vec suite). In the call logs
// a NaN's sign is cleared.
static unsigned g_nan_signs;
static bool is_nan(uint32_t u) { return (u & 0x7F800000u) == 0x7F800000u && (u & 0x007FFFFFu) != 0; }
static uint32_t lbits(float f) { const uint32_t u = bits(f); return is_nan(u) ? u & 0x7FFFFFFFu : u; }
static void eqf(const char* what, float o, float r) {
    const uint32_t a = bits(o), b = bits(r);
    if (a != b && is_nan(a) && is_nan(b) && (a & 0x7FFFFFFFu) == (b & 0x7FFFFFFFu)) {
        g_nan_signs++;
        g_checks++;
        return;
    }
    eq(what, a, b);
}
static void lg(char tag, uint32_t value = 0) { if (g_recording) g_log[g_side].add((uint32_t)tag, value); }

// ---- the worlds ---------------------------------------------------------------------------------------------------------------------
enum { kVoices = 12, kEmitters = 3, kBanks = 3, kSegments = 6, kCues = 4096 };

struct FakeMatrix { int id; float pos[3]; };

struct World {
    int side;
    AtNameTableOwner* owner;
    AtSoIFactory* factory;
    AtSoXboxSystem* system;  // On the original's side, the original's vftable on the recovered layout.
    AtSoXboxVoice* voices[kVoices];
    AtSoXboxEmitter* emitters[kEmitters];
    AtSoXboxXactBank* banks[kBanks];
    AtSoXboxSegment* segments[kSegments];
    FakeMatrix mats[kEmitters];
    int header[kBanks][4];
    void* soundBanks[kBanks][2];
};
static World* W[2];

static int g_soundbank_handles[kBanks * 2];
static int g_cues[2][kCues];
static unsigned g_cue_next[2];
static int g_streams[2][kEmitters];
static int g_engine[2];

static long long find(const void* p, void* const* list, int n) {
    if (!p) return -1;
    for (int i = 0; i < n; i++) if (list[i] == p) return i;
    return 0x100000000LL + (uint32_t)(uintptr_t)p;  // Not one of the world's: stack fill or a foreign pointer, compared by value.
}
static long long vid(int s, const void* p) { return find(p, (void* const*)W[s]->voices, kVoices); }
static long long eid(int s, const void* p) { return find(p, (void* const*)W[s]->emitters, kEmitters); }
static long long bid(int s, const void* p) { return find(p, (void* const*)W[s]->banks, kBanks); }
static long long sid(int s, const void* p) { return find(p, (void* const*)W[s]->segments, kSegments); }
static long long cue_id(int s, const void* p) {
    if (!p) return -1;
    const int* c = (const int*)p;
    if (c >= g_cues[s] && c < g_cues[s] + kCues) return c - g_cues[s];
    return 0x100000000LL + (uint32_t)(uintptr_t)p;
}
static long long stream_id(int s, const void* p) {
    if (!p) return -1;
    const int* c = (const int*)p;
    if (c >= g_streams[s] && c < g_streams[s] + kEmitters) return c - g_streams[s];
    return 0x100000000LL + (uint32_t)(uintptr_t)p;
}
static long long sb_id(const void* p) {
    if (!p) return -1;
    const int* c = (const int*)p;
    if (c >= g_soundbank_handles && c < g_soundbank_handles + kBanks * 2) return c - g_soundbank_handles;
    return 0x100000000LL + (uint32_t)(uintptr_t)p;
}

static void compare_worlds(const char* what) {
    char name[160];
    for (int i = 0; i < kVoices; i++) {
        const AtSoXboxVoice* a = W[0]->voices[i];
        const AtSoXboxVoice* b = W[1]->voices[i];
        sprintf_s(name, "%s: voice %d", what, i);
        eq(name, sid(0, a->m_segment), sid(1, b->m_segment));
        eq(name, eid(0, a->m_emitter), eid(1, b->m_emitter));
        eq(name, a->m_flags, b->m_flags);
        eq(name, a->m_state, b->m_state);
        eq(name, vid(0, a->m_next), vid(1, b->m_next));
        eq(name, vid(0, a->m_prev), vid(1, b->m_prev));
        eq(name, a->m_unknown1C, b->m_unknown1C);
        eq(name, a->m_unknown20, b->m_unknown20);
        eq(name, vid(0, a->m_queued), vid(1, b->m_queued));
        eq(name, a->m_unknown28, b->m_unknown28);
        eq(name, a->m_unknown2C, b->m_unknown2C);
        eq(name, a->m_unknown30, b->m_unknown30);
        eq(name, a->m_playFlags, b->m_playFlags);
        eq(name, a->m_soundBank, b->m_soundBank);
        eq(name, a->m_cueIndex, b->m_cueIndex);
        eq(name, cue_id(0, a->m_cue), cue_id(1, b->m_cue));
        eq(name, a->m_unknown44, b->m_unknown44);
        for (int k = 0; k < AtSoXboxVoice::kParameterCount; k++) {
            const AtSoXboxVoiceParameter& p = a->m_parameters[k];
            const AtSoXboxVoiceParameter& q = b->m_parameters[k];
            eq(name, p.m_unknown00, q.m_unknown00);
            eqf(name, p.m_value, q.m_value);
            eqf(name, p.m_remaining, q.m_remaining);
            eqf(name, p.m_rate, q.m_rate);
            eq(name, p.m_flags, q.m_flags);
            eq(name, std::memcmp(p.m_name, q.m_name, 8), 0);
        }
    }
    for (int i = 0; i < kEmitters; i++) {
        const AtSoXboxEmitter* a = W[0]->emitters[i];
        const AtSoXboxEmitter* b = W[1]->emitters[i];
        sprintf_s(name, "%s: emitter %d", what, i);
        eq(name, vid(0, a->m_firstVoice), vid(1, b->m_firstVoice));
        eq(name, vid(0, a->m_lastVoice), vid(1, b->m_lastVoice));
        eq(name, eid(0, a->m_nextWithVoices), eid(1, b->m_nextWithVoices));
        eq(name, eid(0, a->m_prevWithVoices), eid(1, b->m_prevWithVoices));
        eq(name, a->m_unknown30, b->m_unknown30);
        eq(name, a->m_unknown34, b->m_unknown34);
        eq(name, a->m_unknown38, b->m_unknown38);
        eq(name, a->m_unknown3C, b->m_unknown3C);
        eq(name, a->m_kind, b->m_kind);
        eq(name, stream_id(0, a->m_stream), stream_id(1, b->m_stream));
        eq(name, a->m_unknown54, b->m_unknown54);
    }
    sprintf_s(name, "%s: factory", what);
    eq(name, vid(0, W[0]->factory->m_firstFree), vid(1, W[1]->factory->m_firstFree));
    eq(name, vid(0, W[0]->factory->m_lastFree), vid(1, W[1]->factory->m_lastFree));
    sprintf_s(name, "%s: system", what);
    eq(name, eid(0, W[0]->system->m_firstEmitter), eid(1, W[1]->system->m_firstEmitter));
    eq(name, eid(0, W[0]->system->m_lastEmitter), eid(1, W[1]->system->m_lastEmitter));
    eq(name, bid(0, W[0]->system->m_firstBank), bid(1, W[1]->system->m_firstBank));
    eq(name, bid(0, W[0]->system->m_lastBank), bid(1, W[1]->system->m_lastBank));
    for (int i = 0; i < kBanks; i++) {
        const AtSoXboxXactBank* a = W[0]->banks[i];
        const AtSoXboxXactBank* b = W[1]->banks[i];
        sprintf_s(name, "%s: bank %d", what, i);
        eq(name, a->m_state, b->m_state);
        eq(name, bid(0, a->m_nextInSystem), bid(1, b->m_nextInSystem));
        eq(name, bid(0, a->m_prevInSystem), bid(1, b->m_prevInSystem));
    }
}

// ---- XACT, the stream and the matrices, beneath both sides --------------------------------------------------------------------------
static int g_prepare_status, g_play_status, g_properties_status;
static AtSoXboxCueProperties g_properties[8];
static unsigned g_xact_calls, g_variable_sets, g_variables_sent, g_zero_types;

static void log_play(const AtSoXboxCuePlay* play) {
    lg('f', play->m_flags);
    lg('i', (uint32_t)play->m_cueIndex);
    lg('o', (uint32_t)stream_id(g_side, play->m_source));
    lg('k', (uint32_t)play->m_unknown0C);
}
static void hand_out_cue(void** cue) {
    *cue = &g_cues[g_side][g_cue_next[g_side]++ % kCues];
}
static int xact_prepare(void* sb, const AtSoXboxCuePlay* play, void** cue) {
    g_xact_calls++;
    lg('R', (uint32_t)sb_id(sb));
    log_play(play);
    hand_out_cue(cue);
    return g_prepare_status;
}
static int xact_play(void* sb, const AtSoXboxCuePlay* play, void** cue) {
    g_xact_calls++;
    lg('Y', (uint32_t)sb_id(sb));
    log_play(play);
    lg('c', (uint32_t)cue_id(g_side, *cue));
    if (g_play_status == 0) hand_out_cue(cue);
    return g_play_status;
}
static int xact_stop(void* sb, int index, unsigned flags, void* cue) {
    g_xact_calls++;
    lg('S', (uint32_t)sb_id(sb));
    lg('i', (uint32_t)index);
    lg('f', flags);
    lg('c', (uint32_t)cue_id(g_side, cue));
    return 0;
}
static int xact_pause(void* sb, void* cue, int pause) {
    g_xact_calls++;
    lg('Z', (uint32_t)sb_id(sb));
    lg('c', (uint32_t)cue_id(g_side, cue));
    lg('z', (uint32_t)pause);
    return 0;
}
static int xact_properties(void* sb, int index, AtSoXboxCueProperties* p) {
    g_xact_calls++;
    lg('Q', (uint32_t)sb_id(sb));
    lg('i', (uint32_t)index);
    *p = g_properties[index & 7];
    return g_properties_status;
}
static int xact_variables(void* engine, const AtSoXboxVariables* v) {
    g_xact_calls++;
    g_variable_sets++;
    lg('V', engine == &g_engine[g_side] ? 1u : 0xDEADu);
    lg('n', v->m_count);
    for (unsigned i = 0; i < v->m_count && i < 4; i++) {
        const AtSoXboxVariable& x = v->m_variables[i];
        g_variables_sent++;
        if (x.m_type == 0) g_zero_types++;
        lg('t', x.m_type);
        lg('2', x.m_unknown02);
        lg('4', x.m_unknown04);
        lg('8', (uint32_t)x.m_unknown08);
        lg('v', lbits(x.m_value));
        lg('c', (uint32_t)cue_id(g_side, x.m_cue));
        char name[10] = {0};
        std::memcpy(name, x.m_name, 9);
        uint32_t h = 17;
        for (int k = 0; k < 9 && name[k]; k++) h = h * 31 + (unsigned char)name[k];
        lg('m', h);
        lg('d', (uint32_t)(x.m_name - (const char*)v->m_variables));  // Where in the frame each name was written.
    }
    return 0;
}
static int xact_position(void* stream, float x, float y, float z, int apply) {
    g_xact_calls++;
    lg('P', (uint32_t)stream_id(g_side, stream));
    lg('x', lbits(x));
    lg('y', lbits(y));
    lg('z', lbits(z));
    lg('a', (uint32_t)apply);
    return 0;
}
static void stream_stop(void* stream) { lg('T', (uint32_t)stream_id(g_side, stream)); }

int AtSoXboxXactPrepareCue(void* sb, const AtSoXboxCuePlay* p, void** c) { return xact_prepare(sb, p, c); }
int AtSoXboxXactPlayCue(void* sb, const AtSoXboxCuePlay* p, void** c) { return xact_play(sb, p, c); }
int AtSoXboxXactStopCue(void* sb, int i, unsigned int f, void* c) { return xact_stop(sb, i, f, c); }
int AtSoXboxXactPauseCue(void* sb, void* c, int p) { return xact_pause(sb, c, p); }
int AtSoXboxXactGetCueProperties(void* sb, int i, AtSoXboxCueProperties* p) { return xact_properties(sb, i, p); }
int AtSoXboxXactSetVariables(void* e, const AtSoXboxVariables* v) { return xact_variables(e, v); }
int AtSoXboxXactSetPosition(void* s, float x, float y, float z, int a) { return xact_position(s, x, y, z, a); }
static int __stdcall o_prepare(void* sb, const AtSoXboxCuePlay* p, void** c) { return xact_prepare(sb, p, c); }
static int __stdcall o_play(void* sb, const AtSoXboxCuePlay* p, void** c) { return xact_play(sb, p, c); }
static int __stdcall o_stop(void* sb, int i, unsigned f, void* c) { return xact_stop(sb, i, f, c); }
static int __stdcall o_pause(void* sb, void* c, int p) { return xact_pause(sb, c, p); }
static int __stdcall o_properties(void* sb, int i, AtSoXboxCueProperties* p) { return xact_properties(sb, i, p); }
static int __stdcall o_variables(void* e, const AtSoXboxVariables* v) { return xact_variables(e, v); }
static int __stdcall o_position(void* s, float x, float y, float z, int a) { return xact_position(s, x, y, z, a); }
static void __stdcall o_stream_stop(void* s) { stream_stop(s); }

// The rest of src/at/AtSoXboxHost.cpp, supplied so that its defaults are not linked. None is reached here but the stream's stop.
int AtSoXboxXactBankLoad(AtSoXboxXactBank*) { lg('!', 1); return 3; }
void AtSoXboxXactBankProcess(AtSoXboxXactBank*) { lg('!', 2); }
void AtSoXboxXactBankUpdate(AtSoXboxXactBank*) { lg('!', 3); }
int AtSoXboxCreateStream(void*, int, void**) { lg('!', 4); return -1; }
int AtSoXboxSetMixBins(void*, const AtSoXboxMixBins*) { lg('!', 5); return -1; }
void AtSoXboxStopStream(void* s) { stream_stop(s); }
void AtSoXboxGetStreamStatus(void*, unsigned int* status) { lg('!', 6); *status = 0; }
void AtSoXboxReleaseStream(void*) { lg('!', 7); }

static int mid(const AtMatrix* m) { return m ? ((const FakeMatrix*)m)->id : -1; }
static void __cdecl m_position(const AtMatrix* m, AtVector3* out) { lg('G', (uint32_t)mid(m)); std::memcpy(out, ((const FakeMatrix*)m)->pos, 12); }

// ---- calling the original -----------------------------------------------------------------------------------------------------------
typedef void*(__fastcall* FnCtor)(void*, void*);
typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* FnP)(void*, void*, const void*);
typedef void(__fastcall* FnPP)(void*, void*, const void*, const void*);
typedef void(__fastcall* FnPPP)(void*, void*, const void*, const void*, const void*);
typedef void(__fastcall* FnPF)(void*, void*, const void*, float);
typedef void(__fastcall* FnF)(void*, void*, float);
typedef void*(__fastcall* FnRP)(void*, void*, const void*);
typedef void*(__fastcall* FnR0)(void*, void*);
typedef void*(__fastcall* FnRI)(void*, void*, unsigned);
typedef float(__fastcall* FnFI)(void*, void*, int);
typedef void(__fastcall* FnIFF)(void*, void*, int, float, float);
typedef void(__fastcall* FnIS)(void*, void*, int, const char*);
typedef void(__stdcall* FnS)(void*);

template <class A, class B> static bool both(const char* what, A a, B b) {
    bool ca, cb;
    { On on(0); ca = guarded(a); }
    { On on(1); cb = guarded(b); }
    eq("crashed", ca, cb);
    compare_logs(what);
    compare_worlds(what);
    return ca;
}

static void* raw(size_t n) {
    void* p = std::malloc(n + 16);
    std::memset(p, 0xA5, n + 16);
    return p;
}

static World* build(int side) {
    World* w = new World();
    std::memset(w, 0, sizeof *w);
    w->side = side;
    w->owner = (AtNameTableOwner*)raw(sizeof(AtNameTableOwner));
    w->factory = (AtSoIFactory*)raw(sizeof(AtSoIFactory));
    w->factory->m_vftable = nullptr;
    w->factory->m_firstFree = nullptr;
    w->factory->m_lastFree = nullptr;
    void* sys = raw(sizeof(AtSoXboxSystem));
    if (side == 0) *(uint32_t*)sys = 0x0028A338;
    else new (sys) AtSoXboxSystem;
    w->system = (AtSoXboxSystem*)sys;
    w->system->m_firstEmitter = w->system->m_lastEmitter = nullptr;
    w->system->m_firstBank = w->system->m_lastBank = nullptr;
    w->system->m_directSound = &g_engine[side];
    w->owner->m_device = (AtSceneDevice*)w->factory;
    w->owner->m_loader = w->system;
    w->owner->m_unknown14 = 2.0f;
    for (int i = 0; i < kVoices; i++) {
        void* p = raw(sizeof(AtSoXboxVoice));
        if (side == 0) at<FnCtor>(0x0010D950)(p, nullptr);
        else new (p) AtSoXboxVoice;
        w->voices[i] = (AtSoXboxVoice*)p;
    }
    for (int i = 0; i < kBanks; i++) {
        void* p = raw(sizeof(AtSoXboxXactBank));
        if (side == 0) at<FnCtor>(0x00120550)(p, nullptr);
        else new (p) AtSoXboxXactBank;
        AtSoXboxXactBank* b = (AtSoXboxXactBank*)p;
        w->soundBanks[i][0] = &g_soundbank_handles[2 * i];
        w->soundBanks[i][1] = &g_soundbank_handles[2 * i + 1];
        b->m_block1CC = w->soundBanks[i];
        w->header[i][3] = i == 1 ? 1 : 0;
        b->m_header = w->header[i];
        b->m_system = w->system;
        b->m_state = i == 2 ? AtAudioBank::kInitialised : AtAudioBank::kInitialised | AtAudioBank::kLoaded;
        b->m_nextInSystem = b->m_prevInSystem = nullptr;
        w->banks[i] = b;
    }
    // Segment 3 has no bank; 2's bank is not loaded; 1's leaves a prepared voice kPreparing.
    static const int bank_of[kSegments] = {0, 1, 2, -1, 0, 1};
    for (int i = 0; i < kSegments; i++) {
        void* p = raw(sizeof(AtSoXboxSegment));
        if (side == 0) at<FnCtor>(0x00120650)(p, nullptr);
        else new (p) AtSoXboxSegment;
        AtSoXboxSegment* s = (AtSoXboxSegment*)p;
        s->m_bank = bank_of[i] < 0 ? nullptr : w->banks[bank_of[i]];
        s->m_index = 3 + i * 7;
        s->m_looping = i & 1;
        w->segments[i] = s;
    }
    static const int kinds[kEmitters] = {AtAudioEmitterDesc::kEmitter3D, AtAudioEmitterDesc::kCenter, AtAudioEmitterDesc::kPlain};
    for (int i = 0; i < kEmitters; i++) {
        void* p = raw(sizeof(AtSoXboxEmitter));
        if (side == 0) at<FnCtor>(0x0010EF80)(p, nullptr);
        else new (p) AtSoXboxEmitter;
        AtSoXboxEmitter* e = (AtSoXboxEmitter*)p;
        e->m_owner = w->owner;
        e->m_kind = kinds[i];
        e->m_stream = i == 2 ? nullptr : &g_streams[side][i];
        w->mats[i].id = 10 + i;
        w->mats[i].pos[0] = 1.5f + i;
        w->mats[i].pos[1] = -3.25f * (i + 1);
        w->mats[i].pos[2] = 0.1f * (i + 7);
        e->m_parent = nullptr;
        e->m_local = (AtMatrix*)&w->mats[i];
        w->emitters[i] = e;
    }
    return w;
}

static void fresh() {
    g_recording = false;
    g_prepare_status = g_play_status = g_properties_status = 0;
    g_cue_next[0] = g_cue_next[1] = 0;
    W[0] = build(0);
    W[1] = build(1);
    for (int i = 0; i < kVoices; i++) {
        both("free voices", [&] { at<FnP>(0x0010D7B0)(W[0]->factory, nullptr, W[0]->voices[i]); }, [&] { W[1]->factory->FreePlayState(W[1]->voices[i]); });
    }
}

// Identically on both sides, outside the code under test: what the system's update would do, or a case's setting.
template <class F> static void set(F f) { f(W[0]); f(W[1]); }

// ---- each slot, on both sides --------------------------------------------------------------------------------------------------------
static void* O(int k, int i) { return k == 0 ? (void*)W[0]->emitters[i] : (void*)W[0]->voices[i]; }
static AtSoXboxSegment* seg(int s, int i) { return i < 0 ? nullptr : W[s]->segments[i]; }
static AtAudioVoice* voice(int s, int i) { return i < 0 ? nullptr : W[s]->voices[i]; }

static long long g_out[2];
static void play(int e, int s, bool out) {
    g_context = "play " + std::to_string(e) + " " + std::to_string(s) + (out ? " out" : "");
    AtAudioVoice* vo = (AtAudioVoice*)(uintptr_t)0x77;
    AtAudioVoice* vr = (AtAudioVoice*)(uintptr_t)0x77;
    both("play", [&] { at<FnPP>(0x0011F010)(W[0]->emitters[e], nullptr, seg(0, s), out ? &vo : nullptr); }, [&] { W[1]->emitters[e]->AtAudioEmitter::Play(seg(1, s), out ? &vr : nullptr); });
    eq("played voice", vid(0, vo), vid(1, vr));
}
static void prepare(int e, int s) {
    g_context = "prepare " + std::to_string(e) + " " + std::to_string(s);
    AtAudioVoice* vo = (AtAudioVoice*)(uintptr_t)0x77;
    AtAudioVoice* vr = (AtAudioVoice*)(uintptr_t)0x77;
    both("prepare", [&] { at<FnPP>(0x0011EF90)(W[0]->emitters[e], nullptr, seg(0, s), &vo); }, [&] { W[1]->emitters[e]->AtAudioEmitter::Prepare(seg(1, s), &vr); });
    eq("prepared voice", vid(0, vo), vid(1, vr));
}
static void play_after(int e, int after, int s, bool out) {
    g_context = "play after " + std::to_string(e) + " " + std::to_string(after) + " " + std::to_string(s) + (out ? " out" : "");
    AtAudioVoice* vo = (AtAudioVoice*)(uintptr_t)0x77;
    AtAudioVoice* vr = (AtAudioVoice*)(uintptr_t)0x77;
    both("play after", [&] { at<FnPPP>(0x0011F070)(W[0]->emitters[e], nullptr, voice(0, after), seg(0, s), out ? &vo : nullptr); },
         [&] { W[1]->emitters[e]->AtAudioEmitter::PlayAfter(voice(1, after), seg(1, s), out ? &vr : nullptr); });
    eq("queued voice", vid(0, vo), vid(1, vr));
}
static void on_voice(const char* what, int e, int v) {
    g_context = std::string(what) + " " + std::to_string(e) + " " + std::to_string(v);
    struct Slot { const char* name; uint32_t va; void (AtAudioEmitter::*fn)(AtAudioVoice*); };
    static const Slot slots[] = {
        {"release", 0x0011EF70, &AtAudioEmitter::OnRelease}, {"start", 0x0011EFE0, &AtAudioEmitter::Start}, {"pause", 0x0011F0C0, &AtAudioEmitter::PauseVoice},
        {"resume", 0x0011F0E0, &AtAudioEmitter::ResumeVoice}, {"stop", 0x0011F100, &AtAudioEmitter::StopVoice}, {"remove", 0x0011EEF0, &AtAudioEmitter::RemoveVoice},
    };
    for (const Slot& s : slots) {
        if (std::strcmp(s.name, what) != 0) continue;
        AtSoXboxEmitter* re = W[1]->emitters[e];
        both(what, [&] { at<FnP>(s.va)(W[0]->emitters[e], nullptr, voice(0, v)); }, [&] { (re->*s.fn)(voice(1, v)); });
        return;
    }
    std::printf("no slot %s\n", what);
    g_failures++;
}
static void on_all(const char* what, int e) {
    g_context = std::string(what) + " " + std::to_string(e);
    AtSoXboxEmitter* re = W[1]->emitters[e];
    void* oe = W[0]->emitters[e];
    if (!std::strcmp(what, "pause all")) both(what, [&] { at<Fn0>(0x0011EBC0)(oe, nullptr); }, [&] { re->AtAudioEmitter::PauseAll(); });
    else if (!std::strcmp(what, "resume all")) both(what, [&] { at<Fn0>(0x0011EBF0)(oe, nullptr); }, [&] { re->AtAudioEmitter::ResumeAll(); });
    else if (!std::strcmp(what, "stop all")) both(what, [&] { at<Fn0>(0x0011EC20)(oe, nullptr); }, [&] { re->AtAudioEmitter::StopAll(); });
    else if (!std::strcmp(what, "xbox stop all")) both(what, [&] { at<Fn0>(0x0010F140)(oe, nullptr); }, [&] { re->StopAll(); });
    else if (!std::strcmp(what, "position")) both(what, [&] { at<Fn0>(0x0010F0C0)(oe, nullptr); }, [&] { re->UpdatePosition(); });
    else { std::printf("no slot %s\n", what); g_failures++; }
}
static void update(int e, float seconds, bool xbox) {
    g_context = "update " + std::to_string(e) + (xbox ? " xbox " : " ") + std::to_string(seconds);
    AtSoXboxEmitter* re = W[1]->emitters[e];
    if (xbox) both("xbox update", [&] { at<FnF>(0x0010F120)(W[0]->emitters[e], nullptr, seconds); }, [&] { re->Update(seconds); });
    else both("update", [&] { at<FnF>(0x0011EB70)(W[0]->emitters[e], nullptr, seconds); }, [&] { re->AtAudioEmitter::Update(seconds); });
}
static void update_voice(int e, int v, float seconds) {
    g_context = "update voice " + std::to_string(e) + " " + std::to_string(v);
    both("update voice", [&] { at<FnPF>(0x0011F1E0)(W[0]->emitters[e], nullptr, voice(0, v), seconds); }, [&] { W[1]->emitters[e]->AtAudioEmitter::UpdateVoice(voice(1, v), seconds); });
}
static void stop_segment(int e, int s) {
    g_context = "stop segment " + std::to_string(e) + " " + std::to_string(s);
    both("stop segment", [&] { at<FnP>(0x0011F130)(W[0]->emitters[e], nullptr, seg(0, s)); }, [&] { W[1]->emitters[e]->AtAudioEmitter::StopSegment(seg(1, s)); });
}
static void velocity(int e, float x, float y, float z) {
    g_context = "velocity";
    AtVector3 v = {x, y, z};
    both("velocity", [&] { at<FnP>(0x0011F190)(W[0]->emitters[e], nullptr, &v); }, [&] { W[1]->emitters[e]->AtAudioEmitter::SetVelocity(&v); });
}
static void new_voice(int e, int s) {
    g_context = "new voice";
    void* vo = (void*)(uintptr_t)0x77;
    void* vr = (void*)(uintptr_t)0x77;
    both("new voice", [&] { vo = at<FnRP>(0x0011EE90)(W[0]->emitters[e], nullptr, seg(0, s)); }, [&] { vr = W[1]->emitters[e]->NewVoice(seg(1, s)); });
    eq("new voice", vid(0, vo), vid(1, vr));
}
static void voice_release(int v) {
    g_context = "voice release " + std::to_string(v);
    both("voice release", [&] { at<Fn0>(0x0011EB10)(W[0]->voices[v], nullptr); }, [&] { W[1]->voices[v]->AtAudioVoice::Release(); });
}
static void set_parameter(int v, int k, float value, float seconds) {
    g_context = "set parameter";
    both("set parameter", [&] { at<FnIFF>(0x0010D8D0)(W[0]->voices[v], nullptr, k, value, seconds); }, [&] { W[1]->voices[v]->SetParameter(k, value, seconds); });
    float a = 0, b = 0;
    both("get parameter", [&] { a = at<FnFI>(0x0010D940)(W[0]->voices[v], nullptr, k); }, [&] { b = W[1]->voices[v]->GetParameter(k); });
    eqf("parameter", a, b);
}
static void name_parameter(int v, int k, const char* n) {
    g_context = std::string("name parameter ") + (n ? n : "(null)");
    both("name parameter", [&] { at<FnIS>(0x0010D920)(W[0]->voices[v], nullptr, k, n); }, [&] { W[1]->voices[v]->SetParameterName(k, n); });
}
static int bank_of_voice(int s, int v) {
    const AtAudioSegment* sg = W[s]->voices[v]->m_segment;
    return sg && sg->m_bank ? (int)bid(s, sg->m_bank) : 0;
}
static void bank_slot(const char* what, int b, int v, int after = -1, float seconds = 0) {
    g_context = std::string("bank ") + what + " " + std::to_string(b) + " " + std::to_string(v);
    AtSoXboxXactBank* rb = W[1]->banks[b];
    void* ob = W[0]->banks[b];
    if (!std::strcmp(what, "prepare")) both(what, [&] { at<FnP>(0x00120150)(ob, nullptr, voice(0, v)); }, [&] { rb->Prepare(voice(1, v)); });
    else if (!std::strcmp(what, "start")) both(what, [&] { at<FnP>(0x001201D0)(ob, nullptr, voice(0, v)); }, [&] { rb->Start(voice(1, v)); });
    else if (!std::strcmp(what, "queue")) both(what, [&] { at<FnPP>(0x00120260)(ob, nullptr, voice(0, after), voice(0, v)); }, [&] { rb->Queue(voice(1, after), voice(1, v)); });
    else if (!std::strcmp(what, "pause")) both(what, [&] { at<FnP>(0x001202A0)(ob, nullptr, voice(0, v)); }, [&] { rb->Pause(voice(1, v)); });
    else if (!std::strcmp(what, "resume")) both(what, [&] { at<FnP>(0x001202E0)(ob, nullptr, voice(0, v)); }, [&] { rb->Resume(voice(1, v)); });
    else if (!std::strcmp(what, "stop")) both(what, [&] { at<FnP>(0x00120590)(ob, nullptr, voice(0, v)); }, [&] { rb->Stop(voice(1, v)); });
    else if (!std::strcmp(what, "update")) both(what, [&] { at<FnPF>(0x00120340)(ob, nullptr, voice(0, v), seconds); }, [&] { rb->UpdateVoice(voice(1, v), seconds); });
    else if (!std::strcmp(what, "finish")) both(what, [&] { at<FnS>(0x00120320)(voice(0, v)); }, [&] { AtSoXboxXactBank::Finish(voice(1, v)); });
    else { std::printf("no bank slot %s\n", what); g_failures++; }
}

// A voice made by hand on an emitter: its fields as a case wants them, on both sides alike.
static void force(int v, int state, unsigned flags) {
    set([&](World* w) {
        w->voices[v]->m_state = state;
        w->voices[v]->m_flags = flags;
    });
}

// Voices on the emitters' lists, by index; the same on both sides when the worlds agree.
static std::vector<int> in_use() {
    std::vector<int> out;
    for (int e = 0; e < kEmitters; e++)
        for (AtAudioVoice* v = (AtAudioVoice*)W[1]->emitters[e]->m_firstVoice; v && out.size() < 64; v = v->m_next) out.push_back((int)vid(1, v));
    return out;
}
static int emitter_of(int v) { return (int)eid(1, W[1]->voices[v]->m_emitter); }

// ---- the cases ----------------------------------------------------------------------------------------------------------------------
static unsigned g_seed = 1;
static unsigned rnd() { g_seed = g_seed * 1103515245u + 12345u; return (g_seed >> 8) & 0xFFFFFF; }
static float rfloat() {
    static const float edge[] = {0.0f, -0.0f, 1.0f, 0.5f, 4.99999987e-6f, 4.9e-6f, 5.1e-6f, 1e-30f, 1e30f, -2.5f, 3.0e38f, 0.1f};
    if (rnd() % 4 == 0) return edge[rnd() % 12];
    uint32_t u = (rnd() << 8) ^ rnd();
    float f;
    std::memcpy(&f, &u, 4);
    if (f != f || f > 1e20f || f < -1e20f) return (float)(int)(rnd() % 20000 - 10000) / 64.0f;  // NaNs are in the edge cases of their own
    return f;
}

static unsigned g_plays, g_stops_seen, g_rounds, g_crashes_seen;

static void run_basics() {
    fresh();
    // Every emitter, every segment but the one with no bank; voices wanted back and not.
    for (int e = 0; e < kEmitters; e++)
        for (int s = 0; s < kSegments; s++)
            if (s != 3) { play(e, s, (s + e) & 1); g_plays++; }
    // The prepare and play failing.
    g_prepare_status = -1;
    play(0, 0, true);
    g_prepare_status = 0;
    g_play_status = 0x8007;
    play(1, 0, true);
    play(0, 4, false);
    g_play_status = 0;
    // The factory running out, then a play with nothing to play.
    for (int i = 0; i < 4; i++) play(2, 0, true);
    prepare(0, 1);
    play_after(0, 0, 0, true);
    new_voice(1, 0);
    // Nothing to play after; null voices to each slot that takes one.
    play_after(1, -1, 0, true);
    for (const char* slot : {"release", "start", "pause", "resume", "stop"}) on_voice(slot, 1, -1);
    // Taking everything away again: stop, finish, release, update.
    for (int e = 0; e < kEmitters; e++) on_all("xbox stop all", e);
    for (int v = 0; v < kVoices; v++) bank_slot("finish", 0, v);
    for (int v : in_use()) voice_release(v);
    for (int e = 0; e < kEmitters; e++) for (int i = 0; i < kVoices; i++) update(e, 0.25f, i & 1);
    eq("all let go", (long long)in_use().size(), 0);
    // A voice of a segment with no bank: made, and each slot that tests the bank passes it by.
    for (int state = 0; state <= 6; state++) {
        fresh();
        prepare(0, 3);
        play(0, 3, true);
        for (int v : in_use()) {
            force(v, state, 1);
            for (const char* slot : {"start", "pause", "resume", "stop"}) on_voice(slot, 0, v);
        }
        on_all("pause all", 0);
        on_all("resume all", 0);
        on_all("stop all", 0);
        stop_segment(0, 3);
    }
}

static void run_states() {
    // Each slot of the emitter and the bank on a voice in each state and with each flag.
    for (int state = 0; state <= 7; state++) {
        for (unsigned flags = 0; flags < 8; flags++) {
            fresh();
            prepare(0, 0);
            prepare(1, 1);
            prepare(2, 2);
            play_after(0, 11, 4, true);  // voice 8 queued behind 11
            const std::vector<int> use = in_use();
            for (int v : use) force(v, state, flags);
            set([&](World* w) { w->voices[use[0]]->m_playFlags = 0x30 + state; w->voices[use[0]]->m_soundBank = flags & 1; });
            for (int v : use) {
                const int e = emitter_of(v);
                on_voice("start", e, v);
                force(v, state, flags);
                on_voice("pause", e, v);
                on_voice("resume", e, v);
                force(v, state, flags);
                on_voice("stop", e, v);
                force(v, state, flags);
                update_voice(e, v, 0.125f);
            }
            for (int v : in_use()) force(v, state, flags);
            for (int e = 0; e < kEmitters; e++) {
                on_all("pause all", e);
                on_all("resume all", e);
                stop_segment(e, (state + e) % kSegments);
                on_all("stop all", e);
            }
            for (int v : in_use()) force(v, state, flags);
            for (int e = 0; e < kEmitters; e++) update(e, 0.5f, state & 1);
            for (int v : in_use()) {
                force(v, state, flags);
                on_voice("release", emitter_of(v), v);
            }
            for (int v : in_use()) {
                force(v, state, flags);
                voice_release(v);
            }
            // The bank's slots straight, on voices now wherever they are, the null voice too.
            for (int v : in_use()) {
                for (const char* slot : {"prepare", "start", "pause", "resume", "stop", "update", "finish"}) {
                    force(v, state, flags);
                    bank_slot(slot, bank_of_voice(1, v), v, -1, 0.3f);
                }
            }
            for (const char* slot : {"prepare", "start", "pause", "resume", "stop", "update", "finish"}) {
                if (std::strcmp(slot, "update") != 0) bank_slot(slot, 0, -1);
            }
        }
    }
    // Queue with a null on either side, and through a voice whose bank is not loaded or is absent.
    fresh();
    prepare(0, 0);
    prepare(0, 0);
    bank_slot("queue", 0, 11, -1);
    bank_slot("queue", 0, -1, 11);
    bank_slot("queue", 2, 11, 10);
    set([&](World* w) { w->voices[11]->m_segment = w->segments[2]; });
    bank_slot("queue", 0, 11, 10);
    set([&](World* w) { w->voices[11]->m_segment = w->segments[3]; });
    bank_slot("queue", 0, 11, 10);
    // Stop's queued voice, in each state that reaches it.
    for (int state : {0, 1, 2, 4}) {
        for (int cue = 0; cue < 2; cue++) {
            fresh();
            prepare(0, 0);
            prepare(0, 1);
            set([&](World* w) { ((AtSoXboxVoice*)w->voices[11])->m_queued = w->voices[10]; if (!cue) w->voices[11]->m_cue = nullptr; });
            force(11, state, 1);
            force(10, 4, 1);
            bank_slot("stop", 0, 11);
        }
    }
}

static void run_start_paths() {
    // The bank's Start from every state, from a 3D, a centre and a plain emitter, at several scales, failing and not.
    for (int e = 0; e < kEmitters; e++) {
        for (float scale : {2.0f, 0.3f, 1.0f, -7.0f, 0.0f, 3.0e-39f}) {
            for (int status = 0; status < 2; status++) {
                fresh();
                set([&](World* w) { w->owner->m_unknown14 = scale; });
                g_play_status = status ? 0x80004005 : 0;
                prepare(e, 0);
                for (int state = 0; state <= 7; state++) {
                    force(11, state, 1);
                    bank_slot("start", 0, 11);
                }
                on_all("position", e);
                update(e, 0.0f, true);
                g_play_status = 0;
            }
        }
    }
}

static void run_variables() {
    // A playing voice's parameters to XACT: each set of changed ones, named and not, in each order; the fourth name's terminator landing on the first variable.
    static const char* names[] = {nullptr, "", "pitch", "volume01", "abcdefgh", "q"};
    for (unsigned mask = 0; mask < 16; mask++) {
        for (int n = 0; n < 6; n++) {
            fresh();
            prepare(0, 0);
            prepare(0, 0);
            force(11, 4, 1);
            for (int k = 0; k < 4; k++) {
                name_parameter(11, k, names[(n + k) % 6]);
                if (mask & (1u << k)) set_parameter(11, k, 1.5f * (k + 1) + n, (k & 1) ? 0.0f : 0.75f);
            }
            for (int step = 0; step < 4; step++) bank_slot("update", 0, 11, -1, 0.25f + 0.125f * step);
            // A queued voice started by the update, prepared or not.
            set([&](World* w) { ((AtSoXboxVoice*)w->voices[11])->m_queued = w->voices[10]; });
            force(10, (mask & 1) ? 1 : 0, 1);
            bank_slot("update", 0, 11, -1, 0.1f);
            update(0, 0.1f, true);
        }
    }
}

static void run_parameters() {
    fresh();
    prepare(0, 0);
    for (int i = 0; i < 20000; i++) {
        const int k = (int)(rnd() % 4);
        float value = rfloat(), seconds = rfloat();
        if (i % 97 == 0) seconds = std::numeric_limits<float>::quiet_NaN();
        if (i % 89 == 0) value = std::numeric_limits<float>::infinity();
        set_parameter(11, k, value, seconds);
        if (rnd() % 3 == 0) {
            const float remaining = rfloat();
            set([&](World* w) { w->voices[11]->m_parameters[k].m_remaining = remaining; });
        }
        if (rnd() % 5 == 0) {
            const unsigned flags = rnd() & 0x1FF;
            set([&](World* w) { w->voices[11]->m_parameters[k].m_flags = flags; });
        }
        force(11, 4, 1);
        bank_slot("update", 0, 11, -1, rfloat());
        if (g_failures) break;
    }
}

static void run_properties() {
    fresh();
    for (int i = 0; i < 8; i++) {
        AtSoXboxCueProperties& p = g_properties[i];
        std::memset(&p, 0x5C, sizeof p);
        p.m_looping = i == 1;
        p.m_milliseconds = i == 2 ? -1 : i == 3 ? (int)0x80000001 : i == 4 ? 0 : 1000 * i + 7;
        p.m_hasDistance = i & 1;
        p.m_distance = 3.5f * i - 1.0f;
    }
    for (int b = 0; b < kBanks; b++) {
        for (int status = 0; status < 2; status++) {
            g_properties_status = status ? -3 : 0;
            for (int i = 0; i < 9; i++) {
                g_context = "properties";
                float a = 0, c = 0;
                both("length", [&] { a = at<FnFI>(0x00120470)(W[0]->banks[b], nullptr, i); }, [&] { c = W[1]->banks[b]->SegmentLength(i); });
                eqf("length", a, c);
                both("distance", [&] { a = at<FnFI>(0x001204F0)(W[0]->banks[b], nullptr, i); }, [&] { c = W[1]->banks[b]->Unknown12(i); });
                eqf("distance", a, c);
            }
        }
    }
    g_properties_status = 0;
    // No sound banks, and a first one missing.
    for (int shape = 0; shape < 2; shape++) {
        set([&](World* w) {
            if (shape == 0) w->banks[0]->m_block1CC = nullptr;
            else { w->banks[0]->m_block1CC = w->soundBanks[0]; w->soundBanks[0][0] = nullptr; }
        });
        float a = 0, c = 0;
        both("length, none", [&] { a = at<FnFI>(0x00120470)(W[0]->banks[0], nullptr, 5); }, [&] { c = W[1]->banks[0]->SegmentLength(5); });
        eqf("length, none", a, c);
        both("distance, none", [&] { a = at<FnFI>(0x001204F0)(W[0]->banks[0], nullptr, 5); }, [&] { c = W[1]->banks[0]->Unknown12(5); });
        eqf("distance, none", a, c);
    }
}

static void run_lists() {
    // The system's lists straight, in every order of adding and taking away three.
    static const int orders[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    for (int a = 0; a < 6; a++) {
        for (int r = 0; r < 6; r++) {
            fresh();
            g_context = "lists";
            for (int i = 0; i < 3; i++) {
                const int x = orders[a][i];
                set([&](World* w) {  // Links not null beforehand: add sets them.
                    w->emitters[x]->m_nextWithVoices = w->emitters[(x + 1) % kEmitters];
                    w->banks[x]->m_nextInSystem = w->banks[(x + 1) % kBanks];
                });
                both("add emitter", [&] { at<FnP>(0x000EA600)(W[0]->system, nullptr, W[0]->emitters[x]); }, [&] { W[1]->system->AddEmitter(W[1]->emitters[x]); });
                both("add bank", [&] { at<FnP>(0x000EA6A0)(W[0]->system, nullptr, W[0]->banks[x]); }, [&] { W[1]->system->Add(W[1]->banks[x]); });
            }
            for (int i = 0; i < 3; i++) {
                const int x = orders[r][i];
                both("remove emitter", [&] { at<FnP>(0x000EA640)(W[0]->system, nullptr, W[0]->emitters[x]); }, [&] { W[1]->system->RemoveEmitter(W[1]->emitters[x]); });
                both("remove bank", [&] { at<FnP>(0x000EA6E0)(W[0]->system, nullptr, W[0]->banks[x]); }, [&] { W[1]->system->Remove(W[1]->banks[x]); });
            }
        }
    }
    // The emitter's own list: remove voices from the front, middle and back.
    for (int which = 0; which < 3; which++) {
        fresh();
        for (int i = 0; i < 3; i++) prepare(1, 0);
        const std::vector<int> use = in_use();
        on_voice("remove", 1, use[which]);
        on_voice("remove", 1, use[(which + 1) % 3]);
        on_voice("remove", 1, use[(which + 2) % 3]);
    }
    // NewVoice with the last free voice's next link not null, as a factory filled some other way might leave it.
    fresh();
    set([&](World* w) { w->factory->m_lastFree->m_next = w->voices[0]; });
    new_voice(0, 0);
    // The factory straight: allocate all and one more, and free in a different order.
    fresh();
    g_context = "factory";
    for (int i = 0; i <= kVoices; i++) {
        void* a = (void*)1;
        void* b = (void*)1;
        both("allocate", [&] { a = at<FnR0>(0x0010D770)(W[0]->factory, nullptr); }, [&] { b = W[1]->factory->AllocatePlayState(); });
        eq("allocated", vid(0, a), vid(1, b));
    }
    for (int i = 0; i < kVoices; i++) {
        const int v = (i * 5) % kVoices;
        both("free", [&] { at<FnP>(0x0010D7B0)(W[0]->factory, nullptr, W[0]->voices[v]); }, [&] { W[1]->factory->FreePlayState(W[1]->voices[v]); });
    }
}

static void run_voice_objects() {
    // Voices made, set up, reset and destroyed by each side's code; the base's slots on an Xbox voice.
    fresh();
    g_context = "voice objects";
    prepare(0, 0);
    for (int k = 0; k < 4; k++) name_parameter(11, k, "nm");
    set_parameter(11, 1, 2.0f, 1.0f);
    force(11, 4, 6);  // Init sets the flags; it does not add to them.
    both("base init", [&] { at<FnPP>(0x0011EAD0)(W[0]->voices[11], nullptr, W[0]->segments[4], W[0]->emitters[2]); }, [&] { W[1]->voices[11]->AtAudioVoice::Init(W[1]->segments[4], W[1]->emitters[2]); });
    // A voice not cleared before its Init, as the factory's unrecovered constructor might leave one.
    set_parameter(10, 0, 3.0f, 2.0f);
    name_parameter(10, 2, "dirty");
    set([&](World* w) {
        w->voices[10]->m_queued = w->voices[3];
        w->voices[10]->m_playFlags = 0x55;
        w->voices[10]->m_soundBank = 1;
        w->voices[10]->m_cue = &g_cues[w->side][7];
        w->voices[10]->m_unknown1C = 0x1C;
    });
    both("xbox init", [&] { at<FnPP>(0x0010D890)(W[0]->voices[10], nullptr, W[0]->segments[5], W[0]->emitters[1]); }, [&] { W[1]->voices[10]->Init(W[1]->segments[5], W[1]->emitters[1]); });
    set_parameter(10, 2, 7.0f, 0.0f);
    both("base reset", [&] { at<Fn0>(0x0011EAF0)(W[0]->voices[10], nullptr); }, [&] { W[1]->voices[10]->AtAudioVoice::Reset(); });
    both("clear fields", [&] { at<Fn0>(0x0010D840)(W[0]->voices[11], nullptr); }, [&] { W[1]->voices[11]->ClearFields(); });
    set_parameter(9, 3, 7.0f, 0.0f);
    both("xbox reset", [&] { at<Fn0>(0x0010D8C0)(W[0]->voices[9], nullptr); }, [&] { W[1]->voices[9]->Reset(); });
    // A parameter's step alone.
    for (int i = 0; i < 2000; i++) {
        AtSoXboxVoiceParameter p[2];
        std::memset(p, 0x3C, sizeof p);
        p[0].m_value = p[1].m_value = rfloat();
        p[0].m_remaining = p[1].m_remaining = rfloat();
        p[0].m_rate = p[1].m_rate = rfloat();
        p[0].m_flags = p[1].m_flags = rnd() & 0x3FF;
        const float s = i % 50 == 0 ? std::numeric_limits<float>::quiet_NaN() : rfloat();
        both("step", [&] { at<FnF>(0x000EAB80)(&p[0], nullptr, s); }, [&] { p[1].Step(s); });
        eqf("step value", p[0].m_value, p[1].m_value);
        eqf("step remaining", p[0].m_remaining, p[1].m_remaining);
        eqf("step rate", p[0].m_rate, p[1].m_rate);
        eq("step flags", p[0].m_flags, p[1].m_flags);
        eq("step first word", p[0].m_unknown00, p[1].m_unknown00);
    }
    // Constructors and destructors on fresh memory. The base's constructor through a class adding nothing.
    struct Bare : AtAudioVoice {
        void SetParameter(int, float, float) override {}
        float GetParameter(int) override { return 0.0f; }
    };
    void* a = raw(sizeof(AtSoXboxVoice));
    void* b = raw(sizeof(AtSoXboxVoice));
    both("base constructor", [&] { at<FnCtor>(0x0011EB20)(a, nullptr); }, [&] { new (b) Bare; });
    eq("base constructor", std::memcmp((char*)a + 4, (char*)b + 4, sizeof(AtSoXboxVoice) - 4 + 16), 0);
    both("base deleting destructor", [&] { at<FnRI>(0x0011EB40)(a, nullptr, 1); }, [&] { delete (Bare*)b; });
    a = raw(sizeof(AtSoXboxVoice));
    b = raw(sizeof(AtSoXboxVoice));
    both("xbox constructor", [&] { at<FnCtor>(0x0010D950)(a, nullptr); }, [&] { new (b) AtSoXboxVoice; });
    eq("xbox constructor", std::memcmp((char*)a + 4, (char*)b + 4, sizeof(AtSoXboxVoice) - 4 + 16), 0);
    both("xbox deleting destructor", [&] { at<FnRI>(0x000E9CC0)(a, nullptr, 1); }, [&] { delete (AtSoXboxVoice*)b; });
    // An array of three, by the vector deleting destructor, as delete[] would.
    {
        char* oa = (char*)raw(4 + 3 * sizeof(AtSoXboxVoice));
        *(uint32_t*)oa = 3;
        for (int i = 0; i < 3; i++) at<FnCtor>(0x0010D950)(oa + 4 + i * sizeof(AtSoXboxVoice), nullptr);
        AtSoXboxVoice* ra = nullptr;
        { const bool was = g_recording; g_recording = false; ra = new AtSoXboxVoice[3]; g_recording = was; }
        both("xbox array destructor", [&] { at<FnRI>(0x000E9CC0)(oa + 4, nullptr, 3); }, [&] { delete[] ra; });
    }
}

static void run_crashes() {
    // What crashes on both sides: a voice with no segment, a queued play on a segment with no bank, a released voice let go twice, a stopped voice's queued voice let go.
    fresh();
    prepare(0, 0);
    set([&](World* w) { w->voices[11]->m_segment = nullptr; });
    force(11, 3, 1);
    on_voice("start", 0, 11);
    on_voice("pause", 0, 11);
    on_voice("stop", 0, 11);
    fresh();
    prepare(0, 0);
    play_after(0, 11, 3, true);
    fresh();
    prepare(0, 0);
    force(11, 6, 3);
    on_voice("release", 0, 11);
    voice_release(11);
    fresh();
    prepare(0, 0);
    prepare(0, 0);
    set([&](World* w) { ((AtSoXboxVoice*)w->voices[11])->m_queued = w->voices[10]; w->voices[10]->m_segment = nullptr; });
    force(11, 4, 1);
    bank_slot("stop", 0, 11);
    update_voice(0, 9, 0.1f);  // a voice in the factory: its segment is null
    g_crashes_seen = g_crashes;
}

// Random play: everything above in any order, with the states and statuses the system and XACT would give.
static void run_random(int rounds) {
    for (int seed = 0; seed < 6; seed++) {
        g_seed = 0x1234 + seed * 7919;
        fresh();
        for (int r = 0; r < rounds; r++) {
            g_rounds++;
            const std::vector<int> use = in_use();
            const int e = (int)(rnd() % kEmitters);
            int s = (int)(rnd() % kSegments);
            if (s == 3) s = 0;
            const int v = use.empty() ? -1 : use[rnd() % use.size()];
            switch (rnd() % 22) {
            case 0: case 1: play(e, s, rnd() & 1); g_plays++; break;
            case 2: prepare(e, s); break;
            case 3: if (v >= 0) play_after(e, v, s, rnd() & 1); break;
            case 4: if (v >= 0) on_voice("start", emitter_of(v), v); break;
            case 5: if (v >= 0) on_voice("pause", emitter_of(v), v); break;
            case 6: if (v >= 0) on_voice("resume", emitter_of(v), v); break;
            case 7: if (v >= 0) { on_voice("stop", emitter_of(v), v); g_stops_seen++; } break;
            case 8: on_all((rnd() & 1) ? "pause all" : "resume all", e); break;
            case 9: on_all((rnd() & 1) ? "stop all" : "xbox stop all", e); break;
            case 10: stop_segment(e, s); break;
            case 11: update(e, rfloat() * 0.001f, rnd() & 1); break;
            case 12: if (v >= 0) update_voice(emitter_of(v), v, 0.05f); break;
            case 13: if (v >= 0) voice_release(v); break;
            case 14: if (v >= 0) on_voice("release", emitter_of(v), v); break;
            case 15: if (v >= 0) force(v, (int)(rnd() % 7), W[1]->voices[v]->m_flags ^ (rnd() & 6)); break;
            case 16: if (v >= 0) set_parameter(v, (int)(rnd() % 4), rfloat(), (rnd() & 1) ? 0.0f : rfloat()); break;
            case 17: if (v >= 0) name_parameter(v, (int)(rnd() % 4), (rnd() & 1) ? "nm" : ""); break;
            case 18: g_prepare_status = (rnd() % 5 == 0) ? -1 : 0; g_play_status = (rnd() % 5 == 0) ? -1 : 0; break;
            case 19: { const int b = (rnd() % 2) ? 0 : 2; set([&](World* w) { w->banks[b]->m_state ^= AtAudioBank::kLoaded; }); } break;
            case 20: { const int b = (int)(rnd() % kBanks); set([&](World* w) { w->header[b][3] ^= 1; }); } break;
            case 21: velocity(e, rfloat(), rfloat(), rfloat()); break;
            }
            if (g_failures) return;
        }
    }
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO || c == EXCEPTION_STACK_OVERFLOW) {
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
    if (pos.size() != 1) { std::printf("usage: oracle_emitplay <default.xbe> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    unsigned cw;
    _controlfp_s(&cw, _PC_53, _MCW_PC);  // The game's main thread: docs/portability.md
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    redirect(0x00242189, (const void*)o_prepare);
    redirect(0x002421BC, (const void*)o_play);
    redirect(0x002421EF, (const void*)o_stop);
    redirect(0x00242259, (const void*)o_pause);
    redirect(0x00242226, (const void*)o_properties);
    redirect(0x00242121, (const void*)o_variables);
    redirect(0x00242291, (const void*)o_position);
    redirect(0x002422BF, (const void*)o_stream_stop);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;
    *(void**)(uintptr_t)0x002C61F8 = (void*)m_position;
    g_atMatrix.GetPosition = m_position;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    // Each part in turn; the first with a difference ends the run, which is all a mutant run needs.
    void (*parts[])() = {run_basics, run_states, run_start_paths, run_variables, run_parameters, run_properties, run_lists, run_voice_objects, run_crashes, [] { run_random(3000); }};
    for (auto part : parts) {
        if (g_failures) break;
        part();
    }
    report_entries();
    std::printf("%s: %u plays, %u stops in random play, %u random rounds, %u XACT calls (%u variable sets, %u variables, %u sent with type 0), %u crashes alike, %u NaNs alike but for sign; %llu comparisons, %d differences\n",
                g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_plays, g_stops_seen, g_rounds, g_xact_calls, g_variable_sets, g_variables_sent, g_zero_types, g_crashes, g_nan_signs, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
