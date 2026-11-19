// See include/at/AtFlashScript.h.
#include "at/AtFlashScript.h"

#include "AtSceneArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
int AtFormat(char* out, const char* format, ...);

void* AtFpActionScriptValue::s_heap;
void* AtFpActionScriptStack::s_heap;
void* AtFpActionScriptVM::s_heap;
void* AtFpScript::s_heap;
void* AtFlashScriptTable::s_defaultHeap;

namespace {

enum Word { kNameSpace = 0x39, kActionScriptFileVersion = 0x3A, kScriptBlocks = 0x3B, kScript = 0x3C, kScriptID = 0x3D, kScriptBlockSize = 0x3E, kInstructions = 0x3F };

// At 0x002711D0. Only NameSpace and ActionScriptFileVersion are looked for; the rest are read past whatever they are.
const AtIoKeyword s_keywords[] = {
    {kNameSpace, "NameSpace"},
    {kActionScriptFileVersion, "ActionScriptFileVersion"},
    {kScriptBlocks, "ScriptBlocks"},
    {kScript, "Script"},
    {kScriptID, "ScriptID"},
    {kScriptBlockSize, "ScriptBlockSize"},
    {kInstructions, "Instructions"},
};

// What a heap of its own or a class's fallback gives: reported if neither, then pushed.
void PushOrRaise(void* own, void* fallback) {
    void* heap = own != 0 ? own : fallback;
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
}

}  // namespace

// ---- a value ----------------------------------------------------------------------------------------------------------

AtFpActionScriptValue::AtFpActionScriptValue() {
    m_text = 0;
    m_unknown24 = 0;
    m_capacity = 0;
    m_type = kTypeNone;
    m_unknown10 = 0;
    m_unknown14 = 0;
    m_unknown18 = 0;
    m_unknown1C = 0;
    m_unknown20 = 0;
    m_heap = 0;
}

AtFpActionScriptValue::~AtFpActionScriptValue() {
    if (m_text != 0) {
        operator delete(m_text);
    }
    if (m_unknown24 != 0) {
        operator delete(m_unknown24);
    }
    m_text = 0;
    m_unknown24 = 0;
    m_capacity = 0;
    m_unknown10 = 0;
    m_unknown14 = 0;
    m_unknown18 = 0;
    m_unknown1C = 0;
    m_unknown20 = 0;
    m_heap = 0;
    m_type = kTypeNone;
}

void AtFpActionScriptValue::Reserve(unsigned int size) {
    if (size < 0x20) {
        size = 0x20;
    }
    // The original compares signed.
    if ((int)size > (int)m_capacity) {
        if (m_text != 0) {
            operator delete(m_text);
        }
        PushOrRaise(m_heap, s_heap);
        m_text = (char*)operator new(size);
        AtPopHeap();
        if (m_text == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        m_capacity = size;
    }
    m_text[0] = 0;
}

void AtFpActionScriptValue::MoveTo(AtFpActionScriptValue* to) {
    to->m_type = m_type;
    m_type = kTypeNone;
    to->m_text = m_text;
    to->m_capacity = m_capacity;
    m_text = 0;
    m_capacity = 0;
    to->m_unknown10 = m_unknown10;
    to->m_unknown14 = m_unknown14;
    to->m_unknown18 = m_unknown18;
    to->m_unknown20 = m_unknown20;
    to->m_unknown1C = m_unknown1C;
    to->m_unknown24 = m_unknown24;
    m_unknown24 = 0;
}

void AtFpActionScriptValue::SetString(char* text, unsigned int capacity) {
    m_type = kTypeString;
    if (m_text != 0) {
        operator delete(m_text);
    }
    m_unknown18 = 0;
    m_unknown20 = 0;
    m_unknown1C = 0;
    m_text = text;
    m_capacity = capacity;
}

void AtFpActionScriptValue::Reset() {
    if (m_text != 0) {
        operator delete(m_text);
    }
    if (m_unknown24 != 0) {
        operator delete(m_unknown24);
    }
    m_text = 0;
    m_unknown24 = 0;
    m_capacity = 0;
    m_unknown10 = 0;
    m_unknown14 = 0;
    m_unknown18 = 0;
    m_unknown1C = 0;
    m_unknown20 = 0;
    m_heap = 0;
    m_type = kTypeNone;
}

// ---- the stack ----------------------------------------------------------------------------------------------------------

AtFpActionScriptStack::AtFpActionScriptStack() {
    m_allocated = 0;
    m_top = -1;
    m_capacity = 0;
    m_values = 0;
    m_heap = 0;
}

AtFpActionScriptStack::~AtFpActionScriptStack() {
    Free();
}

void AtFpActionScriptStack::SetHeap(void* heap) {
    s_heap = heap;
}

void AtFpActionScriptStack::Free() {
    if (m_allocated == 0) {
        return;
    }
    if (m_values != 0) {
        AtDeleteArray(m_values);
    }
    m_allocated = 0;
    m_top = -1;
    m_capacity = 0;
    m_values = 0;
    m_heap = 0;
}

void AtFpActionScriptStack::Allocate(unsigned int count) {
    if (m_allocated != 0) {
        Free();
    }
    m_capacity = count;
    PushOrRaise(m_heap, s_heap);
    m_values = AtNewArray<AtFpActionScriptValue>(count);
    AtPopHeap();
    if (m_values == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_allocated = 1;
}

// ---- the machine's state -------------------------------------------------------------------------------------------------

AtFpActionScriptVM::AtFpActionScriptVM() {
    m_strings = 0;
    m_application = 0;
    m_heap = 0;
}

AtFpActionScriptVM::~AtFpActionScriptVM() {
    m_stack.Free();
    m_strings = 0;
    m_application = 0;
    m_heap = 0;
}

void AtFpActionScriptVM::Clear() {
    m_stack.Free();
    m_strings = 0;
    m_application = 0;
    m_heap = 0;
}

void AtFpActionScriptVM::Init(AtFlashApplication* application, AtTextTable* strings) {
    AtFpActionScriptStack::SetHeap(m_heap != 0 ? m_heap : s_heap);
    m_application = application;
    m_strings = strings;
    m_stack.m_heap = m_heap != 0 ? m_heap : s_heap;
    m_stack.Allocate(30);
}

// ---- a script ------------------------------------------------------------------------------------------------------------

AtFpScript::AtFpScript() {
    m_read = 0;
    m_instructions = 0;
    m_bytes = 0;
    m_heap = 0;
}

AtFpScript::~AtFpScript() {
    Free();
}

void AtFpScript::Free() {
    for (unsigned int i = 0; i < m_read; i++) {
        m_instructions[i] = 0;
    }
    if (m_instructions != 0) {
        operator delete(m_instructions);
        m_instructions = 0;
    }
    if (m_bytes != 0) {
        operator delete(m_bytes);
        m_bytes = 0;
    }
    m_read = 0;
    m_instructions = 0;
    m_bytes = 0;
    m_heap = 0;
}

void AtFpScript::Init(int id, unsigned int instructions, unsigned int size) {
    m_id = id;
    PushOrRaise(m_heap, s_heap);
    m_instructions = (unsigned char**)operator new(instructions * sizeof(unsigned char*));
    AtPopHeap();
    if (m_instructions == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    PushOrRaise(m_heap, s_heap);
    m_bytes = (unsigned char*)operator new(size);
    AtPopHeap();
    if (m_bytes == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_instructionCount = instructions;
}

void AtFpScript::ReadInstructions(AtIoTokenizer* tokenizer) {
    unsigned int offset = 0;
    tokenizer->Next();
    unsigned int index = 0;
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        const unsigned int length = (unsigned int)tokenizer->m_int;
        m_instructions[index] = m_bytes + offset;
        for (unsigned int i = 0; i < length; i++) {
            tokenizer->Next();
            m_instructions[index][i] = (unsigned char)tokenizer->m_int;
        }
        index++;
        m_read++;
        offset += length;
    }
}

// ---- the scripts ---------------------------------------------------------------------------------------------------------

AtFlashScriptTable::AtFlashScriptTable() {
    m_loaded = 0;
    m_scripts = 0;
    m_count = 0;
    m_heap = 0;
}

AtFlashScriptTable::~AtFlashScriptTable() {
    ClearScripts();
}

void AtFlashScriptTable::ClearScripts() {
    if (m_loaded == 0) {
        return;
    }
    // A list of no scripts is not freed.
    if (m_scripts != 0 && m_count > 0) {
        for (unsigned int i = 0; i < m_count; i++) {
            m_scripts[i]->Free();
            AtFpScript* script = m_scripts[i];
            if (script != 0) {
                script->~AtFpScript();
                operator delete(script);
            }
        }
        operator delete(m_scripts);
    }
    m_strings.Clear();
    m_vm.Clear();
    m_loaded = 0;
    m_scripts = 0;
    m_count = 0;
    m_heap = 0;
}

void AtFlashScriptTable::Load(const char* name, int, AtFlashApplication* application) {
    if (m_loaded != 0) {
        ClearScripts();
    }
    AtIoTokenFile file;
    m_application = application;
    char path[0x100];
    AtFormat(path, "%s\\%s", "AScript", name);
    m_strings.Load(AtIoContext::s_instance, path, 0);
    m_vm.m_heap = m_heap != 0 ? m_heap : s_defaultHeap;
    m_vm.Init(application, &m_strings);
    AtFormat(path, "%s\\%s", "AScript", name);
    const int status = file.Open(path, "AFA", 2, 0);
    if (status != 0) {
        AtReportFileError(status, path, "AFA");
    }
    AtIoTokenizer* tokenizer = file.m_tokenizer;
    tokenizer->SetKeywords(s_keywords, sizeof(s_keywords) / sizeof(s_keywords[0]), 0);
    tokenizer->ExpectToken(kNameSpace);
    char* space;
    tokenizer->ExpectStringCopy(&space);
    AtCopyName(space, m_name);
    operator delete(space);
    // ActionScriptFileVersion and its number, if there; then ScriptBlocks [ n ].
    if (tokenizer->Next() == kActionScriptFileVersion) {
        tokenizer->Next();
    }
    tokenizer->Next();
    tokenizer->Next();
    tokenizer->Next();
    m_count = (unsigned int)tokenizer->m_int;
    tokenizer->Next();
    PushOrRaise(m_heap, s_defaultHeap);
    m_scripts = (AtFpScript**)operator new(m_count * sizeof(AtFpScript*));
    AtPopHeap();
    if (m_scripts == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    tokenizer->Next();
    for (unsigned int i = 0; i < m_count; i++) {
        // Script { ScriptID id
        tokenizer->Next();
        tokenizer->Next();
        tokenizer->Next();
        tokenizer->Next();
        const int id = tokenizer->m_int;
        // ScriptBlockSize size
        tokenizer->Next();
        tokenizer->Next();
        const unsigned int size = (unsigned int)tokenizer->m_int;
        // Instructions [ n ]
        tokenizer->Next();
        tokenizer->Next();
        tokenizer->Next();
        const unsigned int instructions = (unsigned int)tokenizer->m_int;
        tokenizer->Next();
        PushOrRaise(m_heap, s_defaultHeap);
        m_scripts[i] = AtNewObject<AtFpScript>();
        AtPopHeap();
        if (m_scripts[i] == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        m_scripts[i]->m_heap = m_heap != 0 ? m_heap : s_defaultHeap;
        m_scripts[i]->Init(id, instructions, size);
        m_scripts[i]->ReadInstructions(tokenizer);
        // The script's closing brace.
        tokenizer->Next();
    }
    file.Close();
    m_loaded = 1;
}
