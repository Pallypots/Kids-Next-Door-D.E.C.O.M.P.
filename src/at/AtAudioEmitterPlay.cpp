// A sound emitter's voices: making them, playing them through their segment's bank, pausing, stopping and letting them go; a voice itself; and the factory voices come from. See include/at/AtAudioDatabase.h.
// What each slot does to a voice is the bank's (AtAudioBank slots 3 to 9); on the Xbox that is src/at/AtSoXboxPlay.cpp. Verification status per function: config/functions.d/emitplay.toml. Names inferred.
#include "at/AtAudioDatabase.h"

void AtFatal(const char* message, int a, int b, int c);

namespace {

// The sound owner's +0x04 is its factory of voices; the scene owner, which has the same layout, keeps a scene device there.
AtSoIFactory* FactoryOf(AtNameTableOwner* owner) {
    return reinterpret_cast<AtSoIFactory*>(owner->m_device);
}

bool IsLoaded(const AtAudioBank* bank) {
    return (bank->m_state & AtAudioBank::kLoaded) != 0;
}

}  // namespace

// ---- the factory --------------------------------------------------------------------------------------------------------------------

AtAudioVoice* AtSoIFactory::AllocatePlayState() {
    AtAudioVoice* voice = m_lastFree;
    if (voice == 0) {
        AtFatal("AtSoIFactory::poAllocatePlayState() : Out of playstates!\n", 0, 0, 0);
        return voice;
    }
    m_lastFree = voice->m_prev;
    if (m_lastFree != 0) {
        m_lastFree->m_next = 0;
    } else {
        m_firstFree = 0;
    }
    return voice;
}

void AtSoIFactory::FreePlayState(AtAudioVoice* voice) {
    voice->m_prev = m_lastFree;
    if (m_lastFree != 0) {
        m_lastFree->m_next = voice;
    } else {
        m_firstFree = voice;
    }
    m_lastFree = voice;
    voice->m_next = 0;
}

// ---- a voice ------------------------------------------------------------------------------------------------------------------------

AtAudioVoice::AtAudioVoice() {
    m_segment = 0;
    m_emitter = 0;
    m_flags = 0;
    m_state = kDone;
}

AtAudioVoice::~AtAudioVoice() {
}

void AtAudioVoice::Init(AtAudioSegment* segment, AtAudioEmitter* emitter) {
    m_state = kDone;
    m_segment = segment;
    m_emitter = emitter;
    m_flags = kInitialised;
}

void AtAudioVoice::Reset() {
    m_segment = 0;
    m_emitter = 0;
    m_flags = 0;
    m_state = kDone;
}

void AtAudioVoice::Release() {
    m_flags |= kReleased;
    m_emitter->OnRelease(this);
}

void AtAudioVoice::SetParameterName(int, const char*) {
}

// ---- an emitter's list of voices ----------------------------------------------------------------------------------------------------

AtAudioVoice* AtAudioEmitter::NewVoice(AtAudioSegment* segment) {
    AtAudioVoice* voice = FactoryOf(m_owner)->AllocatePlayState();
    if (voice == 0) {
        return voice;
    }
    voice->Init(segment, this);
    AtAudioVoice* last = (AtAudioVoice*)m_lastVoice;
    voice->m_prev = last;
    if (last != 0) {
        last->m_next = voice;
    } else {
        m_firstVoice = voice;
    }
    m_lastVoice = voice;
    voice->m_next = 0;
    if (m_firstVoice == voice) {
        m_owner->m_loader->AddEmitter(this);
    }
    return voice;
}

void AtAudioEmitter::RemoveVoice(AtAudioVoice* voice) {
    if (voice->m_prev != 0) {
        voice->m_prev->m_next = voice->m_next;
        if (voice->m_prev->m_next != 0) {
            voice->m_next->m_prev = voice->m_prev;
        } else {
            m_lastVoice = voice->m_prev;
        }
    } else {
        m_firstVoice = voice->m_next;
        if (voice->m_next != 0) {
            voice->m_next->m_prev = 0;
        } else {
            m_lastVoice = 0;
        }
    }
    voice->Reset();
    FactoryOf(m_owner)->FreePlayState(voice);
    if (m_firstVoice == 0) {
        m_owner->m_loader->RemoveEmitter(this);
    }
}

// ---- the slots ----------------------------------------------------------------------------------------------------------------------

void AtAudioEmitter::OnRelease(AtAudioVoice* voice) {
    if (voice->m_state == AtAudioVoice::kDone) {
        RemoveVoice(voice);
    }
}

void AtAudioEmitter::UpdateVoice(AtAudioVoice* voice, float seconds) {
    if (voice->m_state != AtAudioVoice::kDone) {
        voice->m_segment->m_bank->UpdateVoice(voice, seconds);
        if (voice->m_state != AtAudioVoice::kDone) {
            return;
        }
    }
    if ((voice->m_flags & AtAudioVoice::kReleased) != 0) {
        RemoveVoice(voice);
    }
}

// The next voice is read from each voice after its update, as in the original; one let go is back in the factory with no next.
void AtAudioEmitter::Update(float seconds) {
    for (AtAudioVoice* voice = (AtAudioVoice*)m_firstVoice; voice != 0; voice = voice->m_next) {
        UpdateVoice(voice, seconds);
    }
}

void AtAudioEmitter::Prepare(AtAudioSegment* segment, AtAudioVoice** voice) {
    AtAudioVoice* made = NewVoice(segment);
    if (made != 0) {
        AtAudioBank* bank = segment->m_bank;
        if (bank != 0 && IsLoaded(bank)) {
            bank->Prepare(made);
        }
    }
    *voice = made;
}

void AtAudioEmitter::Play(AtAudioSegment* segment, AtAudioVoice** voice) {
    AtAudioVoice* made = 0;
    Prepare(segment, &made);
    if (made != 0 && (made->m_state == AtAudioVoice::kPreparing || made->m_state == AtAudioVoice::kPrepared)) {
        Start(made);
    }
    if (voice != 0) {
        *voice = made;
    } else if (made != 0) {
        made->Release();
    }
}

// The original reads the bank before it tests the state, so a voice with no segment crashes whatever its state. The bank is tested first here, which keeps that read first; the
// original tests the state first, which nothing can see.
void AtAudioEmitter::Start(AtAudioVoice* voice) {
    if (voice == 0) {
        return;
    }
    AtAudioBank* bank = voice->m_segment->m_bank;
    if (bank != 0 && (voice->m_state == AtAudioVoice::kPreparing || voice->m_state == AtAudioVoice::kPrepared) && IsLoaded(bank)) {
        bank->Start(voice);
    }
}

// Nothing at all, not even the voice handed back, when there is nothing to play after. The bank is not tested for null.
void AtAudioEmitter::PlayAfter(AtAudioVoice* after, AtAudioSegment* segment, AtAudioVoice** voice) {
    if (after == 0) {
        return;
    }
    AtAudioBank* bank = segment->m_bank;
    AtAudioVoice* made = NewVoice(segment);
    if (made != 0 && IsLoaded(bank)) {
        bank->Queue(after, made);
    }
    if (voice != 0) {
        *voice = made;
    } else if (made != 0) {
        made->Release();
    }
}

void AtAudioEmitter::PauseVoice(AtAudioVoice* voice) {
    if (voice != 0 && voice->m_segment->m_bank != 0) {
        voice->m_segment->m_bank->Pause(voice);
    }
}

void AtAudioEmitter::PauseAll() {
    for (AtAudioVoice* voice = (AtAudioVoice*)m_firstVoice; voice != 0; voice = voice->m_next) {
        PauseVoice(voice);
    }
}

void AtAudioEmitter::ResumeVoice(AtAudioVoice* voice) {
    if (voice != 0 && voice->m_segment->m_bank != 0) {
        voice->m_segment->m_bank->Resume(voice);
    }
}

void AtAudioEmitter::ResumeAll() {
    for (AtAudioVoice* voice = (AtAudioVoice*)m_firstVoice; voice != 0; voice = voice->m_next) {
        ResumeVoice(voice);
    }
}

void AtAudioEmitter::StopSegment(AtAudioSegment* segment) {
    for (AtAudioVoice* voice = (AtAudioVoice*)m_firstVoice; voice != 0; voice = voice->m_next) {
        if (voice->m_segment == segment) {
            StopVoice(voice);
        }
    }
}

void AtAudioEmitter::StopVoice(AtAudioVoice* voice) {
    if (voice != 0 && voice->m_segment != 0 && voice->m_segment->m_bank != 0) {
        voice->m_segment->m_bank->Stop(voice);
    }
}

void AtAudioEmitter::StopAll() {
    for (AtAudioVoice* voice = (AtAudioVoice*)m_firstVoice; voice != 0; voice = voice->m_next) {
        StopVoice(voice);
    }
}

// Copied word by word: the velocity's bits, whatever they are.
void AtAudioEmitter::SetVelocity(const AtVector3* velocity) {
    const int* words = reinterpret_cast<const int*>(velocity);
    m_unknown34 = words[0];
    m_unknown38 = words[1];
    m_unknown3C = words[2];
}
