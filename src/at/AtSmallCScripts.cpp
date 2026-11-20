// The script objects, their factory, and the script system's members that load, start, run and stop scripts. Subsystem at/smallvm; see
// include/at/AtSmallVm.h. Original clusters 0x0006F5A0-0x0006F663, 0x00075340-0x000753F4, 0x00075530-0x0007566F, 0x00075760-0x00075795,
// 0x00075860-0x00075FA8 and 0x0007AF10. Names are inferred.
#include "at/AtSmallVm.h"

#include "at/AtIoTokenizer.h"

void AtCopyName8(char* to, const char* from);  // 0x00013F70 (gm/levels)

void AtPushHeap(void* heap);
void AtPopHeap();
void AtRaise(int code, int file, int line, int unused);

namespace {
struct Placement {};
}
inline void* operator new(decltype(sizeof(0)), void* where, Placement) noexcept { return where; }
inline void operator delete(void*, void*, Placement) noexcept {}

// ---- the script object ------------------------------------------------------------------------------------------------

AtSmallCScript::AtSmallCScript() {
    m_program = 0;
    m_flags = 0;
    m_savedPublic = 0;
    m_savedData = 0;
    m_savedCip = 0;
    m_name[0] = 0;
    m_shared = 0;
    m_valid = 0;
    m_unknown78 = 4;
    m_entry = -1;
    m_retval = 0;
}

AtSmallCScript::~AtSmallCScript() {
    Reset();
}

void AtSmallCScript::SetValid() {
    m_valid = 1;
}

void AtSmallCScript::Reset() {
    if (m_program != 0 && m_shared == 0) {
        operator delete(m_program);
    }
    m_program = 0;
    m_flags = 0;
    m_savedPublic = 0;
    m_savedData = 0;
    m_savedCip = 0;
    m_name[0] = 0;
    m_shared = 0;
    m_valid = 0;
    m_retval = 0;
    m_unknown78 = 4;
    m_entry = -1;
}

// ---- the factory --------------------------------------------------------------------------------------------------------

AtSmallCScript* AtSmallCScriptFactory::Create() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    void* block = operator new(sizeof(AtSmallCScript));
    AtSmallCScript* script = block != 0 ? new (block, Placement()) AtSmallCScript() : 0;
    AtPopHeap();
    if (script == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    script->m_owner = this;
    return script;
}

void AtSmallCScriptFactory::Release(void* object) {
    AtSmallCScript* script = (AtSmallCScript*)object;
    script->Reset();  // ORIGINAL: before the test for null
    if (script != 0) {
        script->~AtSmallCScript();
        operator delete(script);
    }
}

// ---- the system ---------------------------------------------------------------------------------------------------------

AtSmallCSystem* AtSmallCSystem::Instance() {
    if (s_instance == 0) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        void* block = operator new(sizeof(AtSmallCSystem));
        s_instance = block != 0 ? new (block, Placement()) AtSmallCSystem() : 0;
        AtPopHeap();
        if (s_instance == 0) {
            AtRaise(-4, 0, 0, 0);
        }
    }
    return s_instance;
}

int AtSmallCSystem::FindNative(const char* name) {
    for (int i = 0; i < kMaxNatives; i++) {
        const char* a = name;
        const char* b = m_natives[i].m_name;
        while (*a == *b && *a != 0) {
            a++;
            b++;
        }
        if (*a == *b) {
            return i;
        }
    }
    return -1;
}

int AtSmallCSystem::Unregister(const char* name) {
    const int i = FindNative(name);
    if (i == -1) {
        return 0;
    }
    m_natives[i].m_context = 0;
    m_natives[i].m_function = 0;
    m_natives[i].m_name[0] = 0;
    m_nativeCount--;
    return 1;
}

unsigned char* AtSmallCSystem::LoadProgram(AtAmx* amx, const char* file, int) {
    AtIoFileRef ref;
    char path[0x10];
    unsigned char header[0x40];
    unsigned int done;
    AtCopyName9(path, file);
    char* end = path;
    while (*end != 0) {
        end++;
    }
    const char extension[] = ".ASL";
    for (int i = 0; i < 5; i++) {
        end[i] = extension[i];
    }
    const int status = m_programFs != 0 ? ref.OpenOn(m_programFs, path, 2) : ref.Open(path, 2);
    if (status != 0 || ref.Read(header, sizeof header, &done) != 0) {
        return 0;
    }
    // The size and the memory's end, byte by byte as the original assembles them.
    const int size = (int)(header[0] | (header[1] << 8) | (header[2] << 16) | ((unsigned int)header[3] << 24));
    const int memory = (int)(header[0x18] | (header[0x19] << 8) | (header[0x1A] << 16) | ((unsigned int)header[0x1B] << 24));
    if (memory <= 0) {
        return 0;
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    unsigned char* program = (unsigned char*)operator new((unsigned int)memory);  // ORIGINAL: the file's size is not checked against it
    AtPopHeap();
    if (program == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    ref.m_file->Seek(0);
    ref.Read(program, (unsigned int)size, &done);  // ORIGINAL: the result is not looked at
    ref.Close();
    unsigned int* words = (unsigned int*)amx;
    for (unsigned int i = 0; i < sizeof(AtAmx) / sizeof(unsigned int); i++) {
        words[i] = 0;
    }
    if (AtAmxInit(amx, program) != 0) {
        operator delete(program);
        return 0;
    }
    return program;
}

int AtSmallCSystem::Run(AtSmallCSystem*, AtSmallCScript* script, int numparams, const int* params) {
    unsigned int flags = script->m_flags;
    if ((flags & AtSmallCScript::kInCall) != 0) {
        flags &= ~AtSmallCScript::kFresh;  // resume the suspended call
    } else {
        flags = (flags & ~AtSmallCScript::kFinished) | AtSmallCScript::kFresh | AtSmallCScript::kInCall | AtSmallCScript::kStarted;
    }
    script->m_flags = flags;
    if (AtAmxExec(script, &script->m_retval, script->m_entry, numparams, params) == 0) {
        script->m_flags = (script->m_flags & ~(AtSmallCScript::kInCall | AtSmallCScript::kEnabled | AtSmallCScript::kStarted)) | AtSmallCScript::kFinished;
    }
    return 1;  // ORIGINAL: whatever the machine returned; after an error the call stays open, to be resumed from the last suspension's state
}

int AtSmallCSystem::CallPublic(AtSmallCScript* script, int index, int copy) {
    if (copy == 0) {
        script->m_flags &= ~AtSmallCScript::kInCall;
        script->m_entry = index;
        return Run(this, script, 0, 0);
    }
    int result = 0;
    AtSmallCScript call;
    call.SetValid();
    call.m_amx = script->m_amx;
    call.m_program = script->m_program;
    call.m_flags &= ~(AtSmallCScript::kInCall | AtSmallCScript::kEnabled | AtSmallCScript::kStarted);
    call.m_unknown78 = 4;
    call.m_shared = 1;
    call.m_entry = index;
    const int natives = AtAmxRegister(&call.m_amx, m_natives, -1);
    const int variables = AtAmxRegisterHostVars(&call.m_amx, HostVars(), -1);
    if (natives == 0 && variables == 0) {
        call.m_valid = 1;
        result = Run(this, &call, 0, 0);
    }
    call.Reset();
    return result;
}

int AtSmallCSystem::StartScript(const AtSmallCScriptDesc* desc) {
    if ((unsigned int)m_scriptCount >= kMaxScripts) {
        return 3;
    }
    if (m_names.Find(desc->m_name) != 0) {
        return 1;
    }
    char unused[9];
    AtCopyName9(unused, desc->m_file);
    int slot = -1;
    for (int i = 0; i < kMaxScripts && slot < 0; i++) {
        if (m_scripts[i] == 0) {
            m_scripts[i] = m_factory->Create();
            slot = i;
        }
    }
    AtSmallCScript* script = (AtSmallCScript*)m_scripts[slot];  // ORIGINAL: no free slot is not looked for; the count above rules it out
    unsigned char* program;
    if (desc->m_shared != 0) {
        if (m_names.Find(desc->m_file) == 0) {
            m_factory->Release(m_scripts[slot]);
            m_scripts[slot] = 0;
            return 9;
        }
        AtSmallCScript* source = (AtSmallCScript*)m_names.Find(desc->m_file);
        program = source->m_program;
        script->m_amx = source->m_amx;
    } else {
        program = LoadProgram(&script->m_amx, desc->m_file, desc->m_flag40);
    }
    if (program == 0) {
        m_factory->Release(m_scripts[slot]);
        m_scripts[slot] = 0;
        return 2;
    }
    script = (AtSmallCScript*)m_scripts[slot];
    AtCopyName8(script->m_name, desc->m_name);
    script->m_entry = -1;
    if (desc->m_flag40 != 0) {
        script->m_flags |= AtSmallCScript::kFlag40;
    } else {
        script->m_flags &= ~AtSmallCScript::kFlag40;
    }
    script->m_unknown78 = 4;
    script->m_flags &= ~AtSmallCScript::kInCall;
    script->m_program = program;
    script->m_shared = desc->m_shared;
    m_names.Insert(desc->m_name, script);
    const int natives = AtAmxRegister(&script->m_amx, m_natives, -1);
    const int variables = AtAmxRegisterHostVars(&script->m_amx, HostVars(), -1);
    if (natives != 0 || variables != 0) {
        m_factory->Release(m_scripts[slot]);
        m_scripts[slot] = 0;
        m_names.Remove(desc->m_name);
        return 4;
    }
    if (desc->m_enabled != 0) {
        script->m_flags |= AtSmallCScript::kEnabled;
    } else {
        script->m_flags &= ~AtSmallCScript::kEnabled;
    }
    script->m_flags &= ~AtSmallCScript::kStarted;
    if (desc->m_entry[0] != 0) {
        int index;
        AtAmxFindPublic(&script->m_amx, desc->m_entry, &index);  // ORIGINAL: absent, the entry is 0x7FFFFFFF
        script->m_entry = index;
    }
    script->m_valid = 1;
    m_lastStarted = slot;
    m_scriptCount++;
    return 0;
}

int AtSmallCSystem::FindScriptIndex(const char* name) {
    for (int i = 0; i < kMaxScripts; i++) {
        AtSmallCScript* script = (AtSmallCScript*)m_scripts[i];
        if (script != 0 && AtCompareNames8(name, script->m_name) == 0) {
            return i;
        }
    }
    return -1;
}

AtSmallCScript* AtSmallCSystem::FindScript(const char* name) {
    return (AtSmallCScript*)m_names.Find(name);
}

int AtSmallCSystem::StopScript(const char* name) {
    const int i = FindScriptIndex(name);
    if (i == -1) {
        return 0;
    }
    m_names.Remove(name);
    m_factory->Release(m_scripts[i]);
    m_scripts[i] = 0;
    m_scriptCount--;
    return 1;
}

int AtSmallCSystem::Activate(const char* name) {
    AtSmallCScript* script = (AtSmallCScript*)m_names.Find(name);
    if (script == 0 || script->m_valid == 0) {
        return 5;
    }
    if ((script->m_flags & AtSmallCScript::kInCall) != 0) {
        return 7;
    }
    script->m_flags = (script->m_flags & ~(AtSmallCScript::kInCall | AtSmallCScript::kStarted | AtSmallCScript::kFinished)) | AtSmallCScript::kEnabled;
    return 0;
}

int AtSmallCSystem::Cancel(const char* name) {
    AtSmallCScript* script = (AtSmallCScript*)m_names.Find(name);
    if (script == 0 || script->m_valid == 0) {
        return 5;
    }
    if ((script->m_flags & AtSmallCScript::kInCall) == 0) {
        return 8;
    }
    script->m_flags &= ~(AtSmallCScript::kInCall | AtSmallCScript::kEnabled | AtSmallCScript::kStarted);
    return 0;
}
