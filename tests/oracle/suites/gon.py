# Mutants of the .GON sound notes. Each text occurs exactly once in the source.
F = "GmSoundNotes.cpp"

# Dropped as equivalent, with the reason:
# * The resets of a note's fields at the top of each Note in LoadNotes: new GmSoundNote[] has just run the constructor, which clears them all.
# * choice->m_segment = 0 in ReadSegments: likewise the choice's constructor.
# * bank->m_notes[i].m_choices = 0 in FreeNotes: the notes are freed on the next lines and nothing reads them.
# * m_extension[0] = 0 and AtFormat(m_extension, "") in the item's constructor: each alone leaves the same first byte, and nothing reads past it.
# * if (emitter[0] != 0) and if (conditional[0] != 0): finding an empty name in the emitters' table gives null, which is what the note already holds.
# * Dividing each weight by the sum instead of multiplying by its reciprocal: both in double, then rounded to float. They differ by at most a rounding of the double, and a search of 400,000 random
#   weight sets (analysis-local/scratch/gon/weights.py) found no set where the float differs. Not distinguishable by any text found; storing the reciprocal as a float is tested instead.
# Not compared: emitter[0] = 0 and conditional[0] = 0. Without them the names are uninitialised stack.

_UNKNOWN0 = (
    "    if (m_bank != 0) {\n        GmAudioData::s_instance->FreeNotes(m_bank);\n        GmAudioData::s_instance->ReleaseNoteBank(m_bank);\n    }\n"
    "    m_bank = 0;\n    AtFormat(m_extension, \"\");\n}\n\nGmSoundNoteItem::~"
)
_DTOR = (
    "    if (m_bank != 0) {\n        GmAudioData::s_instance->FreeNotes(m_bank);\n        GmAudioData::s_instance->ReleaseNoteBank(m_bank);\n    }\n"
    "    m_bank = 0;\n    AtFormat(m_extension, \"\");\n}\n\nvoid GmSoundNoteItem::Run"
)


def _cut(block, line):
    return block, block.replace(line, "", 1)


_FIND_FIRST = "i < m_firstDatabaseCount; i++) {\n        if (m_firstDatabases[i] != 0) {\n            for (unsigned int k = 0; k < m_firstDatabases[i]->m_segmentCount; k++) {\n                void* found"
_FIND_REST = "i < kDatabases; i++) {\n        if (m_databases[i] != 0) {\n            for (unsigned int k = 0; k < m_databases[i]->m_segmentCount; k++) {\n                void* found"
_NOTE_IF = "if (bank->m_notes != 0 && (bankName[0] == 0 || AtNamesEqual(bank->m_name, bankName))) {\n            void* found = bank->"
_NOTE_LOOP = "for (unsigned int i = 0; i < kNoteBanks; i++) {\n        GmSoundNoteBank* bank = &m_noteBanks[i];\n        " + _NOTE_IF

MUTANTS = [
    # The keyword table.
    ("keywords: an id", F, '{kStop, "Stop"}', '{kSingleton, "Stop"}'),
    ("keywords: a word", F, '{kGlobalEmitter, "GlobalEmitter"}', '{kGlobalEmitter, "GlobalEmiter"}'),
    ("keywords: segments", F, '{kSegments, "Segments"}', '{kSegments, "Segment"}'),
    ("keywords: count", F, "SetKeywords(kKeywords, sizeof kKeywords / sizeof kKeywords[0], 0)", "SetKeywords(kKeywords, 11, 0)"),
    # Constructors.
    ("name clear: half", F, "for (int i = 0; i < 8; i++) {\n        name[i] = 0;", "for (int i = 0; i < 4; i++) {\n        name[i] = 0;"),
    ("choice: segment", F, "    m_segment = 0;\n    m_weight = 0;\n", "    m_weight = 0;\n"),
    ("choice: weight", F, "    m_weight = 0;\n", "    m_weight = 1;\n"),
    ("note: event argument", F, "    ClearName(m_gameEventArgument);\n", ""),
    ("note: emitter", F, "    m_emitter = 0;\n    m_conditional = 0;\n", "    m_conditional = 0;\n"),
    ("note: conditional", F, "    m_emitter = 0;\n    m_conditional = 0;\n", "    m_emitter = 0;\n"),
    ("note: stop", F, "    ClearName(m_stop);\n", ""),
    ("note: singleton", F, "    m_singleton = 0;\n    m_choiceCount = 0;\n", "    m_choiceCount = 0;\n"),
    ("note: choices", F, "    m_choiceCount = 0;\n    m_choices = 0;\n}", "    m_choiceCount = 0;\n}"),
    ("bank: name", F, "    ClearName(m_name);\n    m_notes = 0;", "    m_notes = 0;"),
    ("bank: count", F, "    m_noteCount = 0;\n}\n\nGmSoundNoteBank::~", "}\n\nGmSoundNoteBank::~"),
    # The sixty banks.
    ("allocate: test", F, "if (m_noteBankUsed[i] == 0) {", "if (m_noteBankUsed[i] != 0) {"),
    ("allocate: mark", F, "            m_noteBankUsed[i] = 1;\n", "            m_noteBankUsed[i] = 2;\n"),
    ("allocate: which", F, "            return &m_noteBanks[i];\n", "            return &m_noteBanks[i ^ 1];\n"),
    ("allocate: bound", F, "for (unsigned int i = 0; i < kNoteBanks; i++) {\n        if (m_noteBankUsed[i] == 0) {", "for (unsigned int i = 0; i < kNoteBanks - 1; i++) {\n        if (m_noteBankUsed[i] == 0) {"),
    ("release: test", F, "if (&m_noteBanks[i] == bank) {", "if (&m_noteBanks[i] != bank) {"),
    ("release: mark", F, "            m_noteBankUsed[i] = 0;\n", "            m_noteBankUsed[i] = 2;\n"),
    ("free: no early return", F, "    if (bank->m_notes == 0) {\n        return;\n    }\n    for", "    for"),
    ("free: empty choices", F, "if (bank->m_notes[i].m_choiceCount > 0) {", "if (bank->m_notes[i].m_choiceCount >= 0) {"),
    ("free: choices kept", F, "            operator delete(bank->m_notes[i].m_choices);\n", ""),
    ("free: notes kept", F, "    operator delete(bank->m_notes);\n", ""),
    ("free: notes pointer", F, "    operator delete(bank->m_notes);\n    bank->m_notes = 0;\n", "    operator delete(bank->m_notes);\n"),
    ("free: count", F, "    bank->m_noteCount = 0;\n    bank->m_byName.Clear();\n", "    bank->m_byName.Clear();\n"),
    ("free: names map", F, "    bank->m_byName.Clear();\n", ""),
    ("free: trigger map", F, "    bank->m_byTrigger.Clear();\n", ""),
    ("free: vfx map", F, "    bank->m_byVfxTrigger.Clear();\n", ""),
    # Segments by name.
    ("segment: first count", F, _FIND_FIRST, _FIND_FIRST.replace("i < m_firstDatabaseCount", "i <= m_firstDatabaseCount", 1)),
    ("segment: first null", F, _FIND_FIRST, _FIND_FIRST.replace("if (m_firstDatabases[i] != 0) {", "if (1) {", 1)),
    ("segment: first list", F, "void* found = m_firstDatabases[i]->SegmentDatabaseAt(k)->Find(name);", "void* found = m_firstDatabases[i]->SegmentDatabaseAt(0)->Find(name);"),
    ("segment: rest bound", F, _FIND_REST, _FIND_REST.replace("i < kDatabases", "i < kDatabases - 1", 1)),
    ("segment: rest list", F, "void* found = m_databases[i]->SegmentDatabaseAt(k)->Find(name);", "void* found = m_databases[i]->SegmentDatabaseAt(1)->Find(name);"),
    ("segment: rest skipped", F, _FIND_REST, _FIND_REST.replace("if (m_databases[i] != 0) {", "if (0) {", 1)),
    ("segment name: not cleared", F, "    out[0] = 0;\n", ""),
    ("segment name: first goes on", F, "m_firstDatabases[i]->SegmentDatabaseAt(k)->GetNameOf(segment, out);\n                if (out[0] != 0) {\n                    return;\n                }",
     "m_firstDatabases[i]->SegmentDatabaseAt(k)->GetNameOf(segment, out);"),
    ("segment name: first list", F, "m_firstDatabases[i]->SegmentDatabaseAt(k)->GetNameOf(segment, out);", "m_firstDatabases[i]->SegmentDatabaseAt(0)->GetNameOf(segment, out);"),
    ("segment name: rest list", F, "m_databases[i]->SegmentDatabaseAt(k)->GetNameOf(segment, out);", "m_databases[i]->SegmentDatabaseAt(0)->GetNameOf(segment, out);"),
    # Notes by name and by trigger.
    ("find note: unused banks", F, _NOTE_IF + "m_byName", _NOTE_IF.replace("bank->m_notes != 0 && ", "", 1) + "m_byName"),
    ("find note: no filter", F, _NOTE_IF + "m_byName", _NOTE_IF.replace("(bankName[0] == 0 || AtNamesEqual(bank->m_name, bankName))", "1", 1) + "m_byName"),
    ("find note: filter reversed", F, _NOTE_IF + "m_byName", _NOTE_IF.replace("|| AtNamesEqual", "|| !AtNamesEqual", 1) + "m_byName"),
    ("find note: always filtered", F, _NOTE_IF + "m_byName", _NOTE_IF.replace("bankName[0] == 0 || ", "", 1) + "m_byName"),
    ("find note: map", F, "void* found = bank->m_byName.Find(name);", "void* found = bank->m_byTrigger.Find(name);"),
    ("find note: last", F, "return (GmSoundNote*)found;\n            }\n        }\n    }\n    return 0;\n}\n\nGmSoundNote* GmAudioData::FindNoteByTrigger",
     "(void)found;\n            }\n        }\n    }\n    return 0;\n}\n\nGmSoundNote* GmAudioData::FindNoteByTrigger"),
    ("find note: bound", F, _NOTE_LOOP + "m_byName", _NOTE_LOOP.replace("i < kNoteBanks", "i < kNoteBanks - 1", 1) + "m_byName"),
    ("find trigger: unused banks", F, _NOTE_IF + "m_byTrigger", _NOTE_IF.replace("bank->m_notes != 0 && ", "", 1) + "m_byTrigger"),
    ("find trigger: no filter", F, _NOTE_IF + "m_byTrigger", _NOTE_IF.replace("(bankName[0] == 0 || AtNamesEqual(bank->m_name, bankName))", "1", 1) + "m_byTrigger"),
    ("find trigger: map", F, "void* found = bank->m_byTrigger.Find(trigger);", "void* found = bank->m_byName.Find(trigger);"),
    ("find trigger: bound", F, _NOTE_LOOP + "m_byTrigger", _NOTE_LOOP.replace("i < kNoteBanks", "i < kNoteBanks - 1", 1) + "m_byTrigger"),
    # The reader.
    ("load: extension", F, 'file.Open(path, "GON", 2, 0)', 'file.Open(path, "GOX", 2, 0)'),
    ("load: flags", F, 'file.Open(path, "GON", 2, 0)', 'file.Open(path, "GON", 4, 0)'),
    ("load: no report", F, '        AtReportFileError(status, path, "GON");\n', ""),
    ("load: report text", F, 'AtReportFileError(status, path, "GON")', 'AtReportFileError(status, path, "GOX")'),
    ("load: name space ignored", F, "    if (t->Next() == kNameSpace) {\n        t->ExpectName(space);", "    if (0) {\n        t->ExpectName(space);"),
    ("load: name space not read", F, "        t->ExpectName(space);\n", ""),
    ("load: no push back", F, "        t->m_pushedBack = 1;\n    }\n    t->ExpectToken(kSoundNotes);", "    }\n    t->ExpectToken(kSoundNotes);"),
    ("load: first word", F, "    t->ExpectToken(kSoundNotes);\n", "    t->Next();\n"),
    ("load: empty goes on", F, "    if (count == 0) {\n        file.Close();\n        return;\n    }\n", ""),
    ("load: bank name", F, "    AtCopyName8(bank->m_name, space);\n", ""),
    ("load: bank count", F, "    bank->m_noteCount = count;\n", ""),
    ("load: names map size", F, "    bank->m_byName.Init(count);\n", "    bank->m_byName.Init(count + 1);\n"),
    ("load: trigger map size", F, "    bank->m_byTrigger.Init(count);\n", "    bank->m_byTrigger.Init(count + 1);\n"),
    ("load: vfx map size", F, "    bank->m_byVfxTrigger.Init(count);\n", "    bank->m_byVfxTrigger.Init(count + 1);\n"),
    ("load: heap check", F, "    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n", ""),
    ("load: other heap", F, "    AtPushHeap(s_heap);\n", "    AtPushHeap(0);\n"),
    ("load: heap not popped", F, "    bank->m_notes = new GmSoundNote[count];\n    AtPopHeap();\n", "    bank->m_notes = new GmSoundNote[count];\n"),
    ("load: notes size", F, "new GmSoundNote[count]", "new GmSoundNote[count + 1]"),
    ("load: no raise without memory", F, "    if (bank->m_notes == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("load: note word", F, "        t->ExpectToken(kNote);\n", "        t->Next();\n"),
    ("load: note name", F, "        t->ExpectName(note->m_name);\n", ""),
    ("load: note curly", F, "        t->ExpectLeftCurly();\n        // Only", "        t->Next();\n        // Only"),
    ("load: trigger", F, "case kAnimationTrigger: t->ExpectName(note->m_trigger); break;", "case kAnimationTrigger: t->ExpectName(note->m_vfxTrigger); break;"),
    ("load: vfx trigger", F, "case kVfxTrigger: t->ExpectName(note->m_vfxTrigger); break;", "case kVfxTrigger: t->ExpectName(note->m_trigger); break;"),
    ("load: event", F, "                t->ExpectName(note->m_gameEvent);\n", "                t->ExpectName(note->m_stop);\n"),
    ("load: event argument kind", F, "if (t->Next() == AtIoTokenizer::kString) {", "if (t->Next() == AtIoTokenizer::kReal) {"),
    ("load: event argument", F, "                    t->CopyName(note->m_gameEventArgument);\n", ""),
    ("load: event argument cleared", F, "                    note->m_gameEventArgument[0] = 0;\n", ""),
    ("load: event no push back", F, "                    t->m_pushedBack = 1;\n                }\n                break;", "                }\n                break;"),
    ("load: global emitter", F, "case kGlobalEmitter: t->ExpectName(emitter); break;", "case kGlobalEmitter: t->ExpectName(conditional); break;"),
    ("load: conditional", F, "case kConditional: t->ExpectName(conditional); break;", "case kConditional: t->ExpectName(emitter); break;"),
    ("load: stop", F, "case kStop: t->ExpectName(note->m_stop); break;", "case kStop: break;"),
    ("load: singleton", F, "case kSingleton: note->m_singleton = 1; break;", "case kSingleton: note->m_singleton = 2; break;"),
    ("load: segments", F, "case kSegments: ReadSegments(note, t); break;", "case kSegments: break;"),
    ("load: not scaled", F, "        ScaleWeights(note);\n", ""),
    ("load: emitter name", F, "note->m_emitter = (AtAudioEmitter*)s_soundOwner->m_nodes.Find(emitter);", "note->m_emitter = (AtAudioEmitter*)s_soundOwner->m_nodes.Find(conditional);"),
    ("load: conditional name", F, "note->m_conditional = (AtAudioEmitter*)s_soundOwner->m_nodes.Find(conditional);", "note->m_conditional = (AtAudioEmitter*)s_soundOwner->m_nodes.Find(emitter);"),
    ("load: last curly", F, "    t->ExpectRightCurly();\n    file.Close();\n}\n\nvoid GmAudioData::RegisterNotes", "    file.Close();\n}\n\nvoid GmAudioData::RegisterNotes"),
    # Segments blocks.
    ("segments: count", F, "note->m_choiceCount = (unsigned int)t->ExpectCount();", "note->m_choiceCount = (unsigned int)t->ExpectCount() / 2;"),
    ("segments: heap check", F, "    if (GmAudioData::s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n", ""),
    ("segments: other heap", F, "AtPushHeap(GmAudioData::s_heap);", "AtPushHeap(0);"),
    ("segments: heap not popped", F, "    note->m_choices = new GmSoundChoice[note->m_choiceCount];\n    AtPopHeap();\n", "    note->m_choices = new GmSoundChoice[note->m_choiceCount];\n"),
    ("segments: no raise without memory", F, "    if (note->m_choices == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("segments: name", F, "        t->ExpectName(choice->m_name);\n", ""),
    ("segments: weight kind", F, "if (t->Next() == AtIoTokenizer::kReal) {", "if (t->Next() == AtIoTokenizer::kInt) {"),
    ("segments: weight", F, "            choice->m_weight = t->m_real;\n", "            choice->m_weight = t->m_real * 2;\n"),
    ("segments: default weight", F, "            choice->m_weight = 1.0f;\n", "            choice->m_weight = 2.0f;\n"),
    ("segments: no push back", F, "            t->m_pushedBack = 1;\n        }\n    }\n    t->ExpectRightCurly();", "        }\n    }\n    t->ExpectRightCurly();"),
    ("segments: no curly", F, "    t->ExpectRightCurly();\n}\n\n// So that", "}\n\n// So that"),
    # Weights.
    ("weights: sum", F, "        total += note->m_choices[i].m_weight;\n", "        total += 1.0;\n"),
    ("weights: zero sum", F, "if (note->m_choiceCount == 0 || total <= 0.0) {", "if (note->m_choiceCount == 0 || total < 0.0) {"),
    ("weights: negative sum", F, "if (note->m_choiceCount == 0 || total <= 0.0) {", "if (note->m_choiceCount == 0) {"),
    ("weights: fallback", F, "        total = 1.0;\n", "        total = 2.0;\n"),
    ("weights: scale as float", F, "    const double scale = 1.0 / total;\n", "    const float scale = (float)(1.0 / total);\n"),
    ("weights: not stored", F, "        note->m_choices[i].m_weight = (float)(scale * note->m_choices[i].m_weight);\n", ""),
    # Registering.
    ("register: segment", F, "note->m_choices[k].m_segment = FindSegment(note->m_choices[k].m_name);", "note->m_choices[k].m_segment = 0;"),
    ("register: choice bound", F, "k < note->m_choiceCount; k++) {\n            note->m_choices[k].m_segment", "k + 1 < note->m_choiceCount; k++) {\n            note->m_choices[k].m_segment"),
    ("register: name", F, "bank->m_byName.Insert(note->m_name, note);", "bank->m_byName.Insert(note->m_trigger, note);"),
    ("register: empty trigger", F, "if (note->m_trigger[0] != 0) {", "if (1) {"),
    ("register: trigger map", F, "bank->m_byTrigger.Insert(note->m_trigger, note);", "bank->m_byVfxTrigger.Insert(note->m_trigger, note);"),
    ("register: empty vfx", F, "if (note->m_vfxTrigger[0] != 0) {", "if (1) {"),
    ("register: vfx", F, "            bank->m_byVfxTrigger.Insert(note->m_vfxTrigger, note);\n", ""),
    ("register: note bound", F, "i < bank->m_noteCount; i++) {\n        GmSoundNote* note = &bank->m_notes[i];\n        for", "i + 1 < bank->m_noteCount; i++) {\n        GmSoundNote* note = &bank->m_notes[i];\n        for"),
    # The item.
    ("item: path", F, "    m_path[0] = 0;\n", ""),
    ("item: link", F, "    m_next = 0;\n", ""),
    ("item: bank", F, "    m_bank = 0;\n    AtFormat(m_extension, \"\");\n}\n\nvoid GmSoundNoteItem::Unknown0", "    AtFormat(m_extension, \"\");\n}\n\nvoid GmSoundNoteItem::Unknown0"),
    ("release: not freed",) + (F,) + _cut(_UNKNOWN0, "        GmAudioData::s_instance->FreeNotes(m_bank);\n"),
    ("release: not given back",) + (F,) + _cut(_UNKNOWN0, "        GmAudioData::s_instance->ReleaseNoteBank(m_bank);\n"),
    ("release: bank kept",) + (F,) + _cut(_UNKNOWN0, "    m_bank = 0;\n"),
    ("release: extension kept",) + (F,) + _cut(_UNKNOWN0, "    AtFormat(m_extension, \"\");\n"),
    ("dtor: not freed",) + (F,) + _cut(_DTOR, "        GmAudioData::s_instance->FreeNotes(m_bank);\n"),
    ("dtor: not given back",) + (F,) + _cut(_DTOR, "        GmAudioData::s_instance->ReleaseNoteBank(m_bank);\n"),
    ("dtor: bank kept",) + (F,) + _cut(_DTOR, "    m_bank = 0;\n"),
    ("dtor: extension kept",) + (F,) + _cut(_DTOR, "    AtFormat(m_extension, \"\");\n"),
    ("run: no allocation", F, "    m_bank = GmAudioData::s_instance->AllocateNoteBank();\n", "    m_bank = &GmAudioData::s_instance->m_noteBanks[0];\n"),
    ("run: no test", F, "    if (m_bank != 0) {\n        AtFormat(unused", "    if (1) {\n        AtFormat(unused"),
    ("run: no load", F, "        GmAudioData::s_instance->LoadNotes(m_bank, m_path);\n", ""),
    ("configure: no release", F, "    Unknown0();\n    strcpy", "    strcpy"),
    ("configure: path", F, "strcpy(m_path, request->m_path);", "strcpy(m_path, \"x\");"),
    ("configure: extension", F, 'AtFormat(m_extension, "GON");', 'AtFormat(m_extension, "GOX");'),
]

SUITE = {"target": "oracle_gon", "subsystem": "gm/gon", "src": "gm", "var": "KND_GM_SRC", "cases": "tok", "mutants": MUTANTS}
