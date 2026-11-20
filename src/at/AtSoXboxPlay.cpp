// Playing on the Xbox: a voice is an XACT cue, an emitter's stream is placed in the world, the bank prepares, plays, pauses and stops cues and sends a voice's parameters as XACT variables, and the system
// keeps lists of emitters and banks. See include/at/AtAudioDatabase.h. Everything ends at XACT, behind src/at/AtSoXboxXactHost.cpp. Verification status per function: config/functions.d/emitplay.toml.
// Names inferred, except AtSoXboxXactBank's.
#include "at/AtAudioDatabase.h"

void AtCopyName8(char* to, const char* from);
void AtCopyName9(char* to, const char* from);
int AtFormat(char* out, const char* format, ...);  // the C library's sprintf, 0x00125559: src/at/AtIoFormatHost.cpp

namespace {

const float kLargest = 3.40282347e+38f;    // 0x00274A00
const float kMillisecond = 0.001f;         // 0x002779DC
const float kInstant = 4.99999987e-6f;     // 0x00272AC8 a parameter set over less time than this is set at once
const unsigned int kPrepareFlag = 8;
const unsigned int kPlayPreparedFlag = 4;
const int kApplyNow = 1;                   // Inferred: the last argument of 0x00242291
const int kAnyCue = -1;

bool IsLoaded(const AtAudioBank* bank) {
    return (bank->m_state & AtAudioBank::kLoaded) != 0;
}

AtSoXboxVoice* XboxVoice(AtAudioVoice* voice) {
    return static_cast<AtSoXboxVoice*>(voice);
}

}  // namespace

// ---- the system's lists ------------------------------------------------------------------------------------------------------------

void AtSoXboxSystem::AddEmitter(AtAudioEmitter* emitter) {
    emitter->m_prevWithVoices = m_lastEmitter;
    if (m_lastEmitter != 0) {
        m_lastEmitter->m_nextWithVoices = emitter;
    } else {
        m_firstEmitter = emitter;
    }
    m_lastEmitter = emitter;
    emitter->m_nextWithVoices = 0;
}

void AtSoXboxSystem::RemoveEmitter(AtAudioEmitter* emitter) {
    if (emitter->m_prevWithVoices != 0) {
        emitter->m_prevWithVoices->m_nextWithVoices = emitter->m_nextWithVoices;
        if (emitter->m_prevWithVoices->m_nextWithVoices != 0) {
            emitter->m_nextWithVoices->m_prevWithVoices = emitter->m_prevWithVoices;
        } else {
            m_lastEmitter = emitter->m_prevWithVoices;
        }
    } else {
        m_firstEmitter = emitter->m_nextWithVoices;
        if (emitter->m_nextWithVoices != 0) {
            emitter->m_nextWithVoices->m_prevWithVoices = 0;
        } else {
            m_lastEmitter = 0;
        }
    }
}

void AtSoXboxSystem::Add(AtAudioBank* added) {
    AtSoXboxXactBank* bank = static_cast<AtSoXboxXactBank*>(added);
    bank->m_prevInSystem = m_lastBank;
    if (m_lastBank != 0) {
        m_lastBank->m_nextInSystem = bank;
    } else {
        m_firstBank = bank;
    }
    m_lastBank = bank;
    bank->m_nextInSystem = 0;
}

void AtSoXboxSystem::Remove(AtAudioBank* removed) {
    AtSoXboxXactBank* bank = static_cast<AtSoXboxXactBank*>(removed);
    if (bank->m_prevInSystem != 0) {
        bank->m_prevInSystem->m_nextInSystem = bank->m_nextInSystem;
        if (bank->m_prevInSystem->m_nextInSystem != 0) {
            bank->m_nextInSystem->m_prevInSystem = bank->m_prevInSystem;
        } else {
            m_lastBank = bank->m_prevInSystem;
        }
    } else {
        m_firstBank = bank->m_nextInSystem;
        if (bank->m_nextInSystem != 0) {
            bank->m_nextInSystem->m_prevInSystem = 0;
        } else {
            m_lastBank = 0;
        }
    }
}

// ---- a voice's parameters ----------------------------------------------------------------------------------------------------------

void AtSoXboxVoiceParameter::Step(float seconds) {
    if (!(m_remaining > 0.0f)) {
        m_flags = (m_flags & kSet) != 0 ? kChanged : 0;
        return;
    }
    if (!(m_remaining > seconds)) {
        m_flags = kChanged;
        m_value = (float)((double)m_rate * m_remaining + m_value);
        m_remaining = 0.0f;
        return;
    }
    m_flags = kChanged;
    m_value = (float)((double)seconds * m_rate + m_value);
    m_remaining = (float)((double)m_remaining - seconds);
}

// ---- a voice -----------------------------------------------------------------------------------------------------------------------

AtSoXboxVoice::AtSoXboxVoice() {
    ClearFields();
}

AtSoXboxVoice::~AtSoXboxVoice() {
}

void AtSoXboxVoice::ClearFields() {
    m_unknown1C = 0;
    m_unknown20 = 0;
    m_queued = 0;
    m_playFlags = 0;
    m_soundBank = 0;
    m_cueIndex = 0;
    m_cue = 0;
    m_unknown44 = 0;
    for (int i = 0; i < kParameterCount; i++) {
        AtSoXboxVoiceParameter& parameter = m_parameters[i];
        parameter.m_rate = 0.0f;
        parameter.m_remaining = 0.0f;
        parameter.m_value = 0.0f;
        parameter.m_flags = 0;
        for (int j = 0; j < 8; j++) {
            parameter.m_name[j] = 0;
        }
    }
}

void AtSoXboxVoice::Init(AtAudioSegment* segment, AtAudioEmitter* emitter) {
    AtAudioVoice::Init(segment, emitter);
    ClearFields();
    m_cueIndex = segment->m_index;
}

void AtSoXboxVoice::Reset() {
    ClearFields();
    AtAudioVoice::Reset();
}

void AtSoXboxVoice::SetParameter(int index, float value, float seconds) {
    AtSoXboxVoiceParameter& parameter = m_parameters[index];
    parameter.m_flags |= AtSoXboxVoiceParameter::kSet;
    if (seconds < kInstant) {
        parameter.m_value = value;
        parameter.m_rate = 0.0f;
        parameter.m_remaining = 0.0f;
        return;
    }
    parameter.m_remaining = seconds;
    parameter.m_rate = (float)(((double)value - parameter.m_value) / seconds);
}

float AtSoXboxVoice::GetParameter(int index) {
    return m_parameters[index].m_value;
}

void AtSoXboxVoice::SetParameterName(int index, const char* name) {
    AtCopyName8(m_parameters[index].m_name, name);
}

// ---- an emitter --------------------------------------------------------------------------------------------------------------------

void AtSoXboxEmitter::UpdatePosition() {
    if (m_kind != AtAudioEmitterDesc::kEmitter3D) {
        return;
    }
    AtVector3 position;
    GetWorldPosition(&position);
    const double scale = 1.0 / m_owner->m_unknown14;  // Kept unrounded, as on the x87.
    AtSoXboxXactSetPosition(m_stream, (float)(position.x * scale), (float)(position.y * scale), (float)(position.z * scale), kApplyNow);
}

void AtSoXboxEmitter::Update(float seconds) {
    UpdatePosition();
    AtAudioEmitter::Update(seconds);
}

void AtSoXboxEmitter::StopAll() {
    AtAudioEmitter::StopAll();
    if (m_stream != 0) {
        AtSoXboxStopStream(m_stream);
    }
}

// ---- the bank's voices -------------------------------------------------------------------------------------------------------------

namespace {

void* SoundBankOf(const AtSoXboxXactBank* bank, const AtSoXboxVoice* voice) {
    return static_cast<void**>(bank->m_block1CC)[voice->m_soundBank];
}

void* FirstSoundBank(const AtSoXboxXactBank* bank) {
    void** banks = static_cast<void**>(bank->m_block1CC);
    return banks != 0 ? banks[0] : 0;
}

void StopQueued(AtSoXboxVoice* voice) {
    AtAudioVoice* queued = voice->m_queued;
    if (queued != 0) {
        queued->m_segment->m_bank->Stop(queued);
        voice->m_queued = 0;
    }
}

}  // namespace

void AtSoXboxXactBank::Finish(AtAudioVoice* voice) {
    if (voice != 0) {
        voice->m_state = AtAudioVoice::kDone;
        XboxVoice(voice)->m_cue = 0;
    }
}

void AtSoXboxXactBank::Prepare(AtAudioVoice* prepared) {
    if (prepared == 0) {
        return;
    }
    AtSoXboxVoice* voice = XboxVoice(prepared);
    AtSoXboxCuePlay play;
    play.m_flags = voice->m_playFlags | kPrepareFlag;
    play.m_cueIndex = voice->m_cueIndex;
    play.m_source = 0;
    play.m_unknown0C = 0;
    if (AtSoXboxXactPrepareCue(SoundBankOf(this, voice), &play, &voice->m_cue) != 0) {
        voice->m_state = AtAudioVoice::kDone;
        return;
    }
    voice->m_state = static_cast<const int*>(m_header)[3] != 0 ? AtAudioVoice::kPreparing : AtAudioVoice::kPrepared;
}

// A voice still kPreparing is only marked kStarting; what plays it then is not here.
void AtSoXboxXactBank::Start(AtAudioVoice* started) {
    if (started == 0) {
        return;
    }
    AtSoXboxVoice* voice = XboxVoice(started);
    unsigned int flags = voice->m_playFlags;
    if (voice->m_state == AtAudioVoice::kPrepared) {
        flags |= kPlayPreparedFlag;
    } else if (voice->m_state == AtAudioVoice::kPreparing) {
        voice->m_state = AtAudioVoice::kStarting;
        return;
    }
    AtSoXboxEmitter* emitter = static_cast<AtSoXboxEmitter*>(voice->m_emitter);
    AtSoXboxCuePlay play;
    play.m_flags = flags;
    play.m_cueIndex = voice->m_cueIndex;
    play.m_source = emitter->m_stream;
    play.m_unknown0C = 0;
    emitter->UpdatePosition();
    const int status = AtSoXboxXactPlayCue(SoundBankOf(this, voice), &play, &voice->m_cue);
    voice->m_state = status != 0 ? AtAudioVoice::kDone : AtAudioVoice::kStarting;
}

void AtSoXboxXactBank::Queue(AtAudioVoice* after, AtAudioVoice* voice) {
    if (after == 0 || voice == 0) {
        return;
    }
    XboxVoice(after)->m_queued = voice;
    voice->m_state = AtAudioVoice::kQueued;
    AtAudioBank* bank = voice->m_segment->m_bank;
    if (bank != 0 && IsLoaded(bank)) {
        bank->Prepare(voice);
    }
}

void AtSoXboxXactBank::Pause(AtAudioVoice* paused) {
    if (paused == 0) {
        return;
    }
    AtSoXboxVoice* voice = XboxVoice(paused);
    if (voice->m_state == AtAudioVoice::kPlaying) {
        AtSoXboxXactPauseCue(SoundBankOf(this, voice), voice->m_cue, 1);
    }
    if (voice->m_state == AtAudioVoice::kPreparing || voice->m_state == AtAudioVoice::kStarting || voice->m_state == AtAudioVoice::kPlaying) {
        voice->m_flags |= AtAudioVoice::kPaused;
    }
}

void AtSoXboxXactBank::Resume(AtAudioVoice* resumed) {
    if (resumed == 0 || (resumed->m_flags & AtAudioVoice::kPaused) == 0) {
        return;
    }
    AtSoXboxVoice* voice = XboxVoice(resumed);
    if (voice->m_state == AtAudioVoice::kPlaying) {
        AtSoXboxXactPauseCue(SoundBankOf(this, voice), voice->m_cue, 0);
    }
    voice->m_flags &= ~(unsigned int)AtAudioVoice::kPaused;
}

void AtSoXboxXactBank::Stop(AtAudioVoice* stopped) {
    if (stopped == 0) {
        return;
    }
    AtSoXboxVoice* voice = XboxVoice(stopped);
    switch (voice->m_state) {
    case AtAudioVoice::kPreparing:
    case AtAudioVoice::kPrepared:
    case AtAudioVoice::kPlaying:
        AtSoXboxXactStopCue(SoundBankOf(this, voice), kAnyCue, 0, voice->m_cue);
        voice->m_state = AtAudioVoice::kStopping;
        StopQueued(voice);
        break;
    case AtAudioVoice::kQueued:
        if (voice->m_cue != 0) {
            voice->m_state = AtAudioVoice::kStopping;
        } else {
            Finish(voice);
        }
        StopQueued(voice);
        break;
    case AtAudioVoice::kStarting:
        voice->m_state = AtAudioVoice::kStopping;
        break;
    default:
        break;
    }
}

// The names are written one after another into eight bytes each, and AtCopyName9 writes nine: a fourth named variable's terminator lands on the first byte of the first variable, whose type then
// goes to XACT as 0. The original's stack has the same order, and so does this frame.
void AtSoXboxXactBank::UpdateVoice(AtAudioVoice* updated, float seconds) {
    AtSoXboxVoice* voice = XboxVoice(updated);
    if (voice->m_state != AtAudioVoice::kPlaying) {
        return;
    }
    struct Frame {
        char names[AtSoXboxVoice::kParameterCount][8];
        AtSoXboxVariable variables[AtSoXboxVoice::kParameterCount];
    } frame;
    AtSoXboxVariables list;
    list.m_count = 0;
    list.m_variables = frame.variables;
    for (int i = 0; i < AtSoXboxVoice::kParameterCount; i++) {
        AtSoXboxVoiceParameter& parameter = voice->m_parameters[i];
        parameter.Step(seconds);
        if ((parameter.m_flags & AtSoXboxVoiceParameter::kChanged) == 0) {
            continue;
        }
        AtSoXboxVariable& variable = frame.variables[list.m_count];
        variable.m_unknown02 = 0;
        variable.m_unknown04 = 0;
        variable.m_unknown08 = 0;
        variable.m_value = parameter.m_value;
        variable.m_type = 1;
        variable.m_cue = voice->m_cue;
        char name[8];
        AtCopyName8(name, parameter.m_name);
        char* to = frame.names[list.m_count];
        if (name[0] == 0) {
            AtFormat(to, "%d", i);
        } else {
            AtCopyName9(to, name);
        }
        variable.m_name = to;
        list.m_count++;
    }
    if (list.m_count > 0) {
        AtSoXboxXactSetVariables(static_cast<AtSoXboxSystem*>(m_system)->m_directSound, &list);
    }
    AtAudioVoice* queued = voice->m_queued;
    if (queued != 0 && queued->m_state == AtAudioVoice::kPrepared) {
        voice->m_emitter->Start(queued);
        queued->m_state = AtAudioVoice::kQueued;
    }
}

float AtSoXboxXactBank::SegmentLength(int index) {
    void* soundBank = FirstSoundBank(this);
    if (soundBank != 0) {
        AtSoXboxCueProperties properties;
        if (AtSoXboxXactGetCueProperties(soundBank, index, &properties) == 0) {
            if (properties.m_looping != 0) {
                return kLargest;
            }
            if (properties.m_milliseconds != -1) {
                return (float)((double)(unsigned int)properties.m_milliseconds * kMillisecond);
            }
        }
    }
    return 0.0f;
}

float AtSoXboxXactBank::Unknown12(int index) {
    void* soundBank = FirstSoundBank(this);
    if (soundBank != 0) {
        AtSoXboxCueProperties properties;
        if (AtSoXboxXactGetCueProperties(soundBank, index, &properties) == 0) {
            if (properties.m_hasDistance != 0) {
                return properties.m_distance;
            }
            return kLargest;
        }
    }
    return 0.0f;
}
