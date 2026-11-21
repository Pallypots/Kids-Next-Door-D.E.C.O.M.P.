// Host test of an emitter's voices on the Xbox classes, down to XACT, on any pointer size. Not evidence of a byte match. Supplies the functions of src/at/AtSoXboxHost.cpp and src/at/AtSoXboxXactHost.cpp.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "at/AtAudioDatabase.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static std::vector<std::string> g_calls;
static void call(const std::string& s) { g_calls.push_back(s); }
static int g_cue_next = 0x100;
static int g_cue_storage[64];
static int g_status = 0;
static AtSoXboxCueProperties g_properties;
static std::vector<std::string> g_variable_names;
static std::vector<float> g_variable_values;
static float g_last_position[3];

int AtSoXboxXactBankLoad(AtSoXboxXactBank*) { return 0; }
void AtSoXboxXactBankProcess(AtSoXboxXactBank*) {}
void AtSoXboxXactBankUpdate(AtSoXboxXactBank*) { call("unload"); }
int AtSoXboxCreateStream(void*, int, void**) { return -1; }
int AtSoXboxSetMixBins(void*, const AtSoXboxMixBins*) { return -1; }
void AtSoXboxStopStream(void*) { call("stop stream"); }
void AtSoXboxGetStreamStatus(void*, unsigned int* status) { *status = 0; }
void AtSoXboxReleaseStream(void*) {}

int AtSoXboxXactPrepareCue(void*, const AtSoXboxCuePlay* play, void** cue) {
    call("prepare " + std::to_string(play->m_flags) + " " + std::to_string(play->m_cueIndex) + (play->m_source ? " source" : ""));
    *cue = &g_cue_storage[g_cue_next++ & 63];
    return g_status;
}
int AtSoXboxXactPlayCue(void*, const AtSoXboxCuePlay* play, void**) {
    call("play " + std::to_string(play->m_flags) + " " + std::to_string(play->m_cueIndex) + (play->m_source ? " source" : ""));
    return g_status;
}
int AtSoXboxXactStopCue(void*, int index, unsigned int flags, void* cue) {
    call("stop " + std::to_string(index) + " " + std::to_string(flags) + (cue ? " cue" : ""));
    return 0;
}
int AtSoXboxXactPauseCue(void*, void*, int pause) { call("pause " + std::to_string(pause)); return 0; }
int AtSoXboxXactGetCueProperties(void*, int, AtSoXboxCueProperties* properties) { *properties = g_properties; return 0; }
int AtSoXboxXactSetVariables(void*, const AtSoXboxVariables* variables) {
    call("variables " + std::to_string(variables->m_count));
    for (unsigned int i = 0; i < variables->m_count; i++) {
        g_variable_names.push_back(variables->m_variables[i].m_name);
        g_variable_values.push_back(variables->m_variables[i].m_value);
        CHECK(variables->m_variables[i].m_type == 1);
    }
    return 0;
}
int AtSoXboxXactSetPosition(void*, float x, float y, float z, int apply) {
    g_last_position[0] = x;
    g_last_position[1] = y;
    g_last_position[2] = z;
    call("position " + std::to_string(apply));
    return 0;
}

static void GetPosition(const AtMatrix*, AtVector3* out) {
    out->x = 2.0f;
    out->y = -4.0f;
    out->z = 8.0f;
}

static std::string calls() {
    std::string s;
    for (const std::string& c : g_calls) s += c + "|";
    g_calls.clear();
    return s;
}

int main() {
    g_atMatrix.GetPosition = GetPosition;

    AtSoXboxSystem* system = new AtSoXboxSystem();
    int engine = 0;
    system->m_directSound = &engine;

    AtSoIFactory factory = {};
    AtSoXboxVoice voices[3];
    for (AtSoXboxVoice& v : voices) factory.FreePlayState(&v);
    CHECK(factory.m_firstFree == &voices[0] && factory.m_lastFree == &voices[2]);

    AtNameTableOwner* owner = new AtNameTableOwner;
    owner->m_device = reinterpret_cast<AtSceneDevice*>(&factory);
    owner->m_loader = system;
    owner->m_unknown14 = 2.0f;

    AtSoXboxXactBank bank;
    int soundBankA = 0, soundBankB = 0;
    void* soundBanks[2] = {&soundBankA, &soundBankB};
    int header[4] = {0, 0, 0, 0};
    bank.m_state |= AtAudioBank::kLoaded;
    bank.m_block1CC = soundBanks;
    bank.m_header = header;
    bank.m_system = system;

    AtSoXboxSegment segment;
    segment.m_bank = &bank;
    segment.m_index = 5;

    int stream = 0;
    AtSoXboxEmitter emitter;
    emitter.m_owner = owner;
    emitter.m_kind = AtAudioEmitterDesc::kEmitter3D;
    emitter.m_stream = &stream;

    // Play with no voice wanted back: prepared, placed, played, released.
    emitter.Play(&segment, nullptr);
    CHECK(calls() == "prepare 8 5|position 1|play 4 5 source|");
    CHECK(g_last_position[0] == 1.0f && g_last_position[1] == -2.0f && g_last_position[2] == 4.0f);
    AtAudioVoice* v = emitter.FirstVoice();
    CHECK(v == &voices[2]);
    CHECK(v->m_state == AtAudioVoice::kStarting);
    CHECK(v->m_flags == (AtAudioVoice::kInitialised | AtAudioVoice::kReleased));
    CHECK(static_cast<AtSoXboxVoice*>(v)->m_cueIndex == 5);
    CHECK(system->m_firstEmitter == &emitter && system->m_lastEmitter == &emitter);
    CHECK(factory.m_lastFree == &voices[1]);

    // Parameters while playing: one at once, one named and ramped.
    v->m_state = AtAudioVoice::kPlaying;
    v->SetParameter(0, 3.0f, 0.0f);
    v->SetParameterName(1, "pitch");
    v->SetParameter(1, 2.0f, 1.0f);
    CHECK(v->GetParameter(0) == 3.0f);
    emitter.Update(0.5f);
    CHECK(calls() == "position 1|variables 2|");
    CHECK(g_variable_names.size() == 2 && g_variable_names[0] == "0" && g_variable_names[1] == "pitch");
    CHECK(g_variable_values.size() == 2 && g_variable_values[0] == 3.0f && g_variable_values[1] == 1.0f);
    emitter.Update(0.75f);  // param 0 not again (its set flag became changed, and that goes); param 1 reaches its end
    CHECK(calls() == "position 1|variables 1|");
    CHECK(v->GetParameter(1) == 2.0f);

    // Pause and resume only a playing voice's cue.
    emitter.PauseAll();
    emitter.ResumeAll();
    CHECK(calls() == "pause 1|pause 0|");

    // A second voice, queued behind the first, and prepared; the first's update starts it.
    AtAudioVoice* second = nullptr;
    emitter.PlayAfter(v, &segment, &second);
    CHECK(second == &voices[1]);
    CHECK(static_cast<AtSoXboxVoice*>(v)->m_queued == second);
    CHECK(second->m_state == AtAudioVoice::kPrepared);
    CHECK(calls() == "prepare 8 5|");
    emitter.Update(0.1f);
    CHECK(calls() == "position 1|position 1|play 4 5 source|");
    CHECK(second->m_state == AtAudioVoice::kQueued);

    // Stopping all: the first stops its cue, and its queued one, which has a cue and is kQueued, only becomes kStopping; then the stream.
    emitter.StopAll();
    CHECK(calls() == "stop -1 0 cue|stop stream|");
    CHECK(v->m_state == AtAudioVoice::kStopping);
    CHECK(second->m_state == AtAudioVoice::kStopping);
    CHECK(static_cast<AtSoXboxVoice*>(v)->m_queued == nullptr);

    // Done and released already: let go on the next update, which then stops, the voice let go having no next.
    AtSoXboxXactBank::Finish(v);
    emitter.Update(0.1f);
    CHECK(emitter.FirstVoice() == second);
    CHECK(factory.m_lastFree == v);
    CHECK(v->m_segment == nullptr && v->m_state == AtAudioVoice::kDone);
    // Done, then released: let go at once.
    AtSoXboxXactBank::Finish(second);
    second->Release();
    CHECK(emitter.FirstVoice() == nullptr);
    CHECK(system->m_firstEmitter == nullptr && system->m_lastEmitter == nullptr);
    calls();

    // A failed prepare leaves the voice done, and released at once it is let go.
    g_status = -5;
    emitter.Play(&segment, nullptr);
    CHECK(emitter.FirstVoice() == nullptr);
    CHECK(calls() == "prepare 8 5|");
    g_status = 0;

    // A cue's properties.
    g_properties.m_looping = 0;
    g_properties.m_milliseconds = 1500;
    CHECK(bank.SegmentLength(3) == (float)(1500.0 * (double)0.001f));  // 0.001f is not a thousandth: 1.50000012, not 1.5
    CHECK(bank.SegmentLength(3) != 1.5f);
    g_properties.m_milliseconds = -1;
    CHECK(bank.SegmentLength(3) == 0.0f);
    g_properties.m_looping = 1;
    CHECK(bank.SegmentLength(3) == 3.40282347e+38f);
    g_properties.m_hasDistance = 1;
    g_properties.m_distance = 12.5f;
    CHECK(bank.Unknown12(3) == 12.5f);
    g_properties.m_hasDistance = 0;
    CHECK(bank.Unknown12(3) == 3.40282347e+38f);

    // The system's list of banks.
    AtSoXboxXactBank other;
    system->Add(&bank);
    system->Add(&other);
    CHECK(system->m_firstBank == &bank && system->m_lastBank == &other && bank.m_nextInSystem == &other && other.m_prevInSystem == &bank);
    system->Remove(&bank);
    CHECK(system->m_firstBank == &other && other.m_prevInSystem == nullptr);
    system->Remove(&other);
    CHECK(system->m_firstBank == nullptr && system->m_lastBank == nullptr);

    bank.m_block1CC = nullptr;
    bank.m_header = nullptr;
    std::printf(g_fail ? "test_emitplay: %d failures\n" : "test_emitplay: all passed\n", g_fail);
    return g_fail ? 1 : 0;
}
