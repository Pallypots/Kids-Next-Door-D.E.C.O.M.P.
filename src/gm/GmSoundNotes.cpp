// .GON sound notes: the reader, the sixty banks, and the manifest's item. See include/gm/GmSoundNotes.h. Verification status per function: config/functions.d/gon.toml.
#include "gm/GmSoundNotes.h"

#include "at/AtAudioDatabase.h"
#include "at/AtNameTableOwner.h"
#include "at/AtText.h"
#include "gm/GmAudioData.h"
#include "gm/GmLevelManager.h"

extern "C" {
char* strcpy(char* to, const char* from);
}
int AtFormat(char* out, const char* format, ...);  // Stands in for the library's sprintf, 0x00125559: src/at/AtIoFormatHost.cpp

GmAudioData* GmAudioData::s_instance;
AtNameTableOwner* GmAudioData::s_soundOwner;

namespace {

enum Token {
    kNameSpace = 0x39, kSoundNotes = 0x3A, kNote = 0x3B, kAnimationTrigger = 0x3C, kVfxTrigger = 0x3D, kGameEvent = 0x3E, kActorEmitter = 0x3F, kGlobalEmitter = 0x40,
    kConditional = 0x41, kStop = 0x42, kSegments = 0x43, kSingleton = 0x44
};

// 0x002B9090.
const AtIoKeyword kKeywords[] = {
    {kNameSpace, "NameSpace"}, {kSoundNotes, "SoundNotes"}, {kNote, "Note"}, {kAnimationTrigger, "AnimationTrigger"}, {kVfxTrigger, "VFXTrigger"}, {kGameEvent, "GameEvent"},
    {kActorEmitter, "ActorEmitter"}, {kGlobalEmitter, "GlobalEmitter"}, {kConditional, "Conditional"}, {kStop, "Stop"}, {kSegments, "Segments"}, {kSingleton, "Singleton"},
};

void ClearName(char* name) {
    for (int i = 0; i < 8; i++) {
        name[i] = 0;
    }
}

void ReadSegments(GmSoundNote* note, AtIoTokenizer* t) {
    note->m_choiceCount = (unsigned int)t->ExpectCount();
    if (GmAudioData::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(GmAudioData::s_heap);
    note->m_choices = new GmSoundChoice[note->m_choiceCount];
    AtPopHeap();
    if (note->m_choices == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < note->m_choiceCount; i++) {
        GmSoundChoice* choice = &note->m_choices[i];
        t->ExpectName(choice->m_name);
        choice->m_segment = 0;
        if (t->Next() == AtIoTokenizer::kReal) {
            choice->m_weight = t->m_real;
        } else {
            choice->m_weight = 1.0f;
            t->m_pushedBack = 1;
        }
    }
    t->ExpectRightCurly();
}

// So that they sum to 1. Weights that sum to nothing, or to less, are divided by 1.
void ScaleWeights(GmSoundNote* note) {
    double total = 0.0;
    for (unsigned int i = 0; i < note->m_choiceCount; i++) {
        total += note->m_choices[i].m_weight;
    }
    if (note->m_choiceCount == 0 || total <= 0.0) {
        total = 1.0;
    }
    const double scale = 1.0 / total;
    for (unsigned int i = 0; i < note->m_choiceCount; i++) {
        note->m_choices[i].m_weight = (float)(scale * note->m_choices[i].m_weight);
    }
}

}  // namespace

GmSoundChoice::GmSoundChoice() {
    ClearName(m_name);
    m_segment = 0;
    m_weight = 0;
}

GmSoundNote::GmSoundNote() {
    ClearName(m_name);
    ClearName(m_trigger);
    ClearName(m_vfxTrigger);
    ClearName(m_gameEvent);
    ClearName(m_gameEventArgument);
    m_emitter = 0;
    m_conditional = 0;
    ClearName(m_stop);
    m_singleton = 0;
    m_choiceCount = 0;
    m_choices = 0;
}

GmSoundNoteBank::GmSoundNoteBank() {
    ClearName(m_name);
    m_notes = 0;
    m_noteCount = 0;
}

GmSoundNoteBank::~GmSoundNoteBank() {
}

GmSoundNoteBank* GmAudioData::AllocateNoteBank() {
    for (unsigned int i = 0; i < kNoteBanks; i++) {
        if (m_noteBankUsed[i] == 0) {
            m_noteBankUsed[i] = 1;
            return &m_noteBanks[i];
        }
    }
    return 0;
}

void GmAudioData::ReleaseNoteBank(GmSoundNoteBank* bank) {
    for (unsigned int i = 0; i < kNoteBanks; i++) {
        if (&m_noteBanks[i] == bank) {
            m_noteBankUsed[i] = 0;
        }
    }
}

void GmAudioData::FreeNotes(GmSoundNoteBank* bank) {
    if (bank->m_notes == 0) {
        return;
    }
    for (unsigned int i = 0; i < bank->m_noteCount; i++) {
        if (bank->m_notes[i].m_choiceCount > 0) {
            operator delete(bank->m_notes[i].m_choices);
            bank->m_notes[i].m_choices = 0;
        }
    }
    operator delete(bank->m_notes);
    bank->m_notes = 0;
    bank->m_noteCount = 0;
    bank->m_byName.Clear();
    bank->m_byTrigger.Clear();
    bank->m_byVfxTrigger.Clear();
}

AtAudioSegment* GmAudioData::FindSegment(const char* name) {
    for (unsigned int i = 0; i < m_firstDatabaseCount; i++) {
        if (m_firstDatabases[i] != 0) {
            for (unsigned int k = 0; k < m_firstDatabases[i]->m_segmentCount; k++) {
                void* found = m_firstDatabases[i]->SegmentDatabaseAt(k)->Find(name);
                if (found != 0) {
                    return (AtAudioSegment*)found;
                }
            }
        }
    }
    for (unsigned int i = 0; i < kDatabases; i++) {
        if (m_databases[i] != 0) {
            for (unsigned int k = 0; k < m_databases[i]->m_segmentCount; k++) {
                void* found = m_databases[i]->SegmentDatabaseAt(k)->Find(name);
                if (found != 0) {
                    return (AtAudioSegment*)found;
                }
            }
        }
    }
    return 0;
}

void GmAudioData::GetSegmentName(const AtAudioSegment* segment, char* out) {
    out[0] = 0;
    for (unsigned int i = 0; i < m_firstDatabaseCount; i++) {
        if (m_firstDatabases[i] != 0) {
            for (unsigned int k = 0; k < m_firstDatabases[i]->m_segmentCount; k++) {
                m_firstDatabases[i]->SegmentDatabaseAt(k)->GetNameOf(segment, out);
                if (out[0] != 0) {
                    return;
                }
            }
        }
    }
    for (unsigned int i = 0; i < kDatabases; i++) {
        if (m_databases[i] != 0) {
            for (unsigned int k = 0; k < m_databases[i]->m_segmentCount; k++) {
                m_databases[i]->SegmentDatabaseAt(k)->GetNameOf(segment, out);
                if (out[0] != 0) {
                    return;
                }
            }
        }
    }
}

GmSoundNote* GmAudioData::FindNote(const char* name, const char* bankName) {
    for (unsigned int i = 0; i < kNoteBanks; i++) {
        GmSoundNoteBank* bank = &m_noteBanks[i];
        if (bank->m_notes != 0 && (bankName[0] == 0 || AtNamesEqual(bank->m_name, bankName))) {
            void* found = bank->m_byName.Find(name);
            if (found != 0) {
                return (GmSoundNote*)found;
            }
        }
    }
    return 0;
}

GmSoundNote* GmAudioData::FindNoteByTrigger(const char* trigger, const char* bankName) {
    for (unsigned int i = 0; i < kNoteBanks; i++) {
        GmSoundNoteBank* bank = &m_noteBanks[i];
        if (bank->m_notes != 0 && (bankName[0] == 0 || AtNamesEqual(bank->m_name, bankName))) {
            void* found = bank->m_byTrigger.Find(trigger);
            if (found != 0) {
                return (GmSoundNote*)found;
            }
        }
    }
    return 0;
}

void GmAudioData::LoadNotes(GmSoundNoteBank* bank, const char* path) {
    AtIoTokenFile file;
    const int status = file.Open(path, "GON", 2, 0);
    if (status != 0) {
        AtReportFileError(status, path, "GON");
    }
    AtIoTokenizer* t = file.m_tokenizer;
    t->SetKeywords(kKeywords, sizeof kKeywords / sizeof kKeywords[0], 0);
    char space[8];
    if (t->Next() == kNameSpace) {
        t->ExpectName(space);
    } else {
        AtCopyName8(space, "");
        t->m_pushedBack = 1;
    }
    t->ExpectToken(kSoundNotes);
    const unsigned int count = (unsigned int)t->ExpectCount();
    if (count == 0) {
        file.Close();
        return;
    }
    AtCopyName8(bank->m_name, space);
    bank->m_noteCount = count;
    bank->m_byName.Init(count);
    bank->m_byTrigger.Init(count);
    bank->m_byVfxTrigger.Init(count);
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    bank->m_notes = new GmSoundNote[count];
    AtPopHeap();
    if (bank->m_notes == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < count; i++) {
        GmSoundNote* note = &bank->m_notes[i];
        t->ExpectToken(kNote);
        t->ExpectName(note->m_name);
        note->m_trigger[0] = 0;
        note->m_vfxTrigger[0] = 0;
        note->m_gameEvent[0] = 0;
        note->m_gameEventArgument[0] = 0;
        note->m_stop[0] = 0;
        note->m_singleton = 0;
        note->m_choices = 0;
        note->m_choiceCount = 0;
        note->m_emitter = 0;
        note->m_conditional = 0;
        char emitter[8];
        char conditional[8];
        emitter[0] = 0;
        conditional[0] = 0;
        t->ExpectLeftCurly();
        // Only a right curly ends this. At the end of the file the original spins here, if nothing has stopped it first.
        for (int token = t->Next(); token != AtIoTokenizer::kRightCurly; token = t->Next()) {
            switch (token) {
            case kAnimationTrigger: t->ExpectName(note->m_trigger); break;
            case kVfxTrigger: t->ExpectName(note->m_vfxTrigger); break;
            case kGameEvent:
                t->ExpectName(note->m_gameEvent);
                if (t->Next() == AtIoTokenizer::kString) {
                    t->CopyName(note->m_gameEventArgument);
                } else {
                    note->m_gameEventArgument[0] = 0;
                    t->m_pushedBack = 1;
                }
                break;
            case kGlobalEmitter: t->ExpectName(emitter); break;
            case kConditional: t->ExpectName(conditional); break;
            case kStop: t->ExpectName(note->m_stop); break;
            case kSingleton: note->m_singleton = 1; break;
            case kSegments: ReadSegments(note, t); break;
            default: break;  // ActorEmitter among them; and any other word or token goes by without a report.
            }
        }
        ScaleWeights(note);
        if (emitter[0] != 0) {
            note->m_emitter = (AtAudioEmitter*)s_soundOwner->m_nodes.Find(emitter);
        }
        if (conditional[0] != 0) {
            note->m_conditional = (AtAudioEmitter*)s_soundOwner->m_nodes.Find(conditional);
        }
    }
    t->ExpectRightCurly();
    file.Close();
}

void GmAudioData::RegisterNotes(GmSoundNoteBank* bank) {
    for (unsigned int i = 0; i < bank->m_noteCount; i++) {
        GmSoundNote* note = &bank->m_notes[i];
        for (unsigned int k = 0; k < note->m_choiceCount; k++) {
            note->m_choices[k].m_segment = FindSegment(note->m_choices[k].m_name);
        }
        bank->m_byName.Insert(note->m_name, note);
        if (note->m_trigger[0] != 0) {
            bank->m_byTrigger.Insert(note->m_trigger, note);
        }
        if (note->m_vfxTrigger[0] != 0) {
            bank->m_byVfxTrigger.Insert(note->m_vfxTrigger, note);
        }
    }
}

GmSoundNoteItem::GmSoundNoteItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    m_bank = 0;
    AtFormat(m_extension, "");
}

void GmSoundNoteItem::Unknown0() {
    if (m_bank != 0) {
        GmAudioData::s_instance->FreeNotes(m_bank);
        GmAudioData::s_instance->ReleaseNoteBank(m_bank);
    }
    m_bank = 0;
    AtFormat(m_extension, "");
}

GmSoundNoteItem::~GmSoundNoteItem() {
    if (m_bank != 0) {
        GmAudioData::s_instance->FreeNotes(m_bank);
        GmAudioData::s_instance->ReleaseNoteBank(m_bank);
    }
    m_bank = 0;
    AtFormat(m_extension, "");
}

void GmSoundNoteItem::Run() {
    // The original builds this text twice and never uses it.
    char unused[0x200];
    AtFormat(unused, "%3s][%s", m_extension, m_path);
    m_bank = GmAudioData::s_instance->AllocateNoteBank();
    if (m_bank != 0) {
        AtFormat(unused, "%3s][%s", m_extension, m_path);
        GmAudioData::s_instance->LoadNotes(m_bank, m_path);
    }
}

void GmSoundNoteItem::Configure(const Request* request) {
    Unknown0();
    strcpy(m_path, request->m_path);
    AtFormat(m_extension, "GON");
}
