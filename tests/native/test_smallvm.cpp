// Host test of the behaviour scripts' abstract machine and the script system (at/smallvm), on any pointer size. Not evidence of a byte match.
//
//   test_smallvm [cases dir]   with the directory of tokenizer cases, also runs every public of the disc's three compiled scripts.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "at/AtSmallVm.h"
#include "gm/GmNameMap.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static int g_heap;
static int g_calls;
static int g_lastArg;
static void* g_lastContext;
static int native(void* context, AtAmx* amx, int* params) {
    g_calls++;
    g_lastContext = context;
    g_lastArg = params[0] >= 4 ? params[1] : -1;
    int* text = nullptr;
    if (params[0] >= 4 && AtAmxGetAddr(amx, params[1], &text) == 0 && params[1] >= 0 && params[1] < amx->m_hea) {
        int length = 0;
        AtAmxStrLen(text, &length);
        CHECK(length >= 0 && length < 256);
    }
    return g_lastArg * 2;
}
static int g_context[4];

struct Asm {
    std::vector<int> code;
    std::vector<int> data = std::vector<int>(16, 0);
    void op(int o) { code.push_back(o); }
    void op(int o, int a) { code.push_back(o); code.push_back(a); }
    int here() const { return (int)code.size() * 4; }
    std::vector<unsigned char> build(const char* nativeName, const char* hostVar, int stack = 0x400) {
        const int tables = 1 + 1 + (hostVar ? 1 : 0);
        const int cod = 0x40 + tables * 0x1C, dat = cod + (int)code.size() * 4, hea = dat + (int)data.size() * 4;
        std::vector<unsigned char> f((size_t)(hea + stack), 0);
        AtAmxHeader* h = (AtAmxHeader*)f.data();
        h->m_size = hea;
        h->m_magic = 0xF1E0;
        h->m_fileVersion = 3;
        h->m_amxVersion = 1;
        h->m_defsize = 0x1C;
        h->m_cod = cod;
        h->m_dat = dat;
        h->m_hea = hea;
        h->m_stp = hea + stack;
        h->m_cip = 0;
        h->m_numPublics = 1;
        h->m_publics = 0x40;
        h->m_numNatives = 1;
        h->m_natives = 0x40 + 0x1C;
        h->m_libraries = h->m_pubVars = cod;
        h->m_numHostVars = hostVar ? 1 : 0;
        h->m_hostVars = 0x40 + 2 * 0x1C;
        AtAmxFuncStub* s = (AtAmxFuncStub*)(f.data() + 0x40);
        std::strcpy(s[0].m_name, "main2");
        std::strcpy(s[1].m_name, nativeName);
        if (hostVar) std::strcpy(s[2].m_name, hostVar);
        std::memcpy(f.data() + cod, code.data(), code.size() * 4);
        std::memcpy(f.data() + dat, data.data(), data.size() * 4);
        return f;
    }
};

static AtSmallCNativeEntry g_list[3];

static int run(std::vector<unsigned char>& program, AtSmallCScript& script, int index, int* retval) {
    std::memset(&script.m_amx, 0, sizeof script.m_amx);
    CHECK(AtAmxInit(&script.m_amx, program.data()) == 0);
    CHECK(AtAmxRegister(&script.m_amx, g_list, -1) == 0);
    script.m_flags = AtSmallCScript::kFresh;
    return AtAmxExec(&script, retval, index, 0, nullptr);
}

static void test_machine() {
    std::strcpy(g_list[0].m_name, "twice");
    g_list[0].m_function = native;
    g_list[0].m_context = g_context;
    // main: calls a function, a built-in, a switch and the arithmetic.
    Asm a;
    a.op(46);                 // PROC
    a.op(39, 20);             // PUSH_C 20
    a.op(39, 4);              // PUSH_C 4
    a.op(123, 0);             // SYSREQ_C twice -> 40
    a.op(44, 8);              // STACK 8
    a.op(15, 0);              // STOR_PRI [0]
    a.op(11, -7);             // CONST_PRI -7
    a.op(12, 2);              // CONST_ALT 2
    a.op(73);                 // SDIV: rounds down: -4, remainder 1
    a.op(15, 4);              // STOR_PRI [4]
    a.op(16, 8);              // STOR_ALT [8]
    a.op(11, 2);              // CONST_PRI 2
    const int sw = a.here();
    a.op(129, 0);             // SWITCH table (patched)
    const int table = a.here();
    a.op(130, 2);             // CASETBL 2 cases
    a.op(0);                  // default (patched)
    a.op(1); a.op(0);         // case 1 (patched)
    a.op(2); a.op(0);         // case 2 (patched)
    const int c1 = a.here();
    a.op(11, 100);
    const int j1 = a.here();
    a.op(51, 0);              // JUMP done
    const int c2 = a.here();
    a.op(11, 200);
    const int j2 = a.here();
    a.op(51, 0);
    const int def = a.here();
    a.op(11, 300);
    const int done = a.here();
    a.op(15, 12);             // STOR_PRI [12]
    a.op(133);                // suspend
    a.op(1, 0);               // LOAD_PRI [0]
    a.op(2, 12);              // LOAD_ALT [12]
    a.op(78);                 // ADD -> 240
    a.op(48);                 // RETN
    a.code[sw / 4 + 1] = table;
    a.code[table / 4 + 2] = def;
    a.code[table / 4 + 4] = c1;
    a.code[table / 4 + 6] = c2;
    a.code[j1 / 4 + 1] = done;
    a.code[j2 / 4 + 1] = done;
    std::vector<unsigned char> program = a.build("twice", nullptr);
    AtSmallCScript script;
    int retval = 0;
    g_calls = 0;
    CHECK(run(program, script, -1, &retval) == -1);  // suspended
    CHECK(g_calls == 1 && g_lastArg == 20 && g_lastContext == g_context);
    const int* data = (const int*)(program.data() + ((AtAmxHeader*)program.data())->m_dat);
    CHECK(data[0] == 40 && data[1] == -4 && data[2] == 1 && data[3] == 200);
    script.m_flags = 0;
    CHECK(AtAmxExec(&script, &retval, -1, 0, nullptr) == 0);
    CHECK(retval == 240);
    CHECK(AtAmxGlobals::s_littleEndian == 1);

    // Errors: a public that is not there, a built-in that is not registered, a division by zero.
    CHECK(run(program, script, 3, &retval) == kAmxErrIndex);
    int index = 0;
    CHECK(AtAmxFindPublic(&script.m_amx, "main2", &index) == 0 && index == 0);
    CHECK(AtAmxFindPublic(&script.m_amx, "nope", &index) == kAmxErrNotFound && index == 0x7FFFFFFF);
    Asm b;
    b.op(46);
    b.op(11, 5);
    b.op(90);                 // ZERO_ALT
    b.op(73);                 // SDIV by zero
    b.op(48);
    std::vector<unsigned char> divide = b.build("twice", nullptr);
    CHECK(run(divide, script, -1, &retval) == kAmxErrDivide);
}

static void test_host_vars() {
    static int value = 7;
    AtSmallCHostVar vars[2] = {};
    std::strcpy(vars[0].m_name, "hv");
    vars[0].m_address = &value;
    Asm a;
    a.op(46);
    a.op(39, 0);              // PUSH_C variable 0
    a.op(39, 4);
    a.op(131, 0);             // read: pri = 7
    a.op(44, 8);
    a.op(87, 5);              // ADD_C 5
    a.op(36);                 // PUSH_PRI 12
    a.op(39, 0);
    a.op(39, 8);
    a.op(132, 0);             // write 12
    a.op(44, 12);
    a.op(48);
    std::vector<unsigned char> program = a.build("twice", "hv");
    AtSmallCScript script;
    std::memset(&script.m_amx, 0, sizeof script.m_amx);
    CHECK(AtAmxInit(&script.m_amx, program.data()) == 0);
    CHECK(AtAmxRegisterHostVars(&script.m_amx, vars, 1) == 0);
    script.m_flags = AtSmallCScript::kFresh;
    int retval = 0;
    CHECK(AtAmxExec(&script, &retval, -1, 0, nullptr) == 0);
    CHECK(value == 12 && retval == 0);
}

static void test_strings() {
    const int unpacked[] = {'a', 'b', 'c', 0};
    int length = 0;
    CHECK(AtAmxStrLen(unpacked, &length) == 0 && length == 3);
    char text[16];
    AtAmxGetString(text, unpacked);
    CHECK(std::strcmp(text, "abc") == 0);
    const int packed[] = {(int)(('a' << 24) | ('b' << 16) | ('c' << 8) | 'd'), (int)('e' << 24), 0};
    AtAmxGetString(text, packed);
    CHECK(std::strcmp(text, "abcde") == 0);
    CHECK(AtCompareName8("ABCDEFGHX", "ABCDEFGHY") == 0 && AtCompareName8("A", "B") < 0 && AtCompareName8(nullptr, "") == 0);
}

static void test_system() {
    GmNameMap::s_heap = &g_heap;
    AtSmallCSystem::s_heap = &g_heap;
    AtSmallCScriptFactory::s_heap = &g_heap;
    AtSmallCSystem::s_instance = nullptr;
    AtSmallCSystem* system = AtSmallCSystem::Instance();
    CHECK(system != nullptr && system == AtSmallCSystem::Instance());
    static AtSmallCScriptFactory factory;
    system->m_factory = &factory;
    system->m_names.Init(AtSmallCSystem::kMaxScripts);
    system->Register(g_context, "twice", native);
    CHECK(system->FindNative("twice") == 0 && system->FindNative("none") == -1);
    // A script made by hand, as StartScript would after loading.
    Asm a;
    a.op(46);
    a.op(39, 3);
    a.op(39, 4);
    a.op(123, 0);
    a.op(44, 8);
    a.op(48);
    std::vector<unsigned char> program = a.build("twice", nullptr);
    AtSmallCScript* script = factory.Create();
    CHECK(script->m_entry == -1 && script->m_unknown78 == 4 && script->m_owner == &factory);
    std::memset(&script->m_amx, 0, sizeof script->m_amx);  // as LoadProgram does: the constructor leaves the machine alone
    CHECK(AtAmxInit(&script->m_amx, program.data()) == 0);
    CHECK(AtAmxRegister(&script->m_amx, system->m_natives, -1) == 0);
    script->m_program = program.data();
    script->m_shared = 1;
    script->m_valid = 1;  // as StartScript leaves it
    std::memcpy(script->m_name, "HAND\0\0\0\0", 8);
    system->m_scripts[0] = script;
    system->m_scriptCount = 1;
    system->m_names.Insert("HAND", script);
    g_calls = 0;
    CHECK(system->CallPublic(script, -1, 1) == 1 && g_calls == 1);
    CHECK(system->CallPublic(script, -1, 0) == 1 && g_calls == 2 && script->m_retval == 6);
    CHECK((script->m_flags & AtSmallCScript::kFinished) != 0);
    CHECK(system->FindScript("HAND") == script && system->FindScriptIndex("HAND") == 0);
    CHECK(system->Activate("HAND") == 0 && system->Cancel("HAND") == 8 && system->Activate("NONE") == 5);
    CHECK(system->StopScript("HAND") == 1 && system->m_scriptCount == 0 && system->StopScript("HAND") == 0);
    CHECK(system->Unregister("twice") == 1 && system->Unregister("twice") == 0);
}

static void test_disc(const char* dir) {
    std::string index = std::string(dir) + "/index.txt";
    FILE* f = std::fopen(index.c_str(), "r");
    if (!f) { std::printf("FAIL cannot read %s\n", index.c_str()); g_fail++; return; }
    static const char* const names[] = {"bProcessEvent", "vState", "bAnim", "nGetStat", "nGetStatMax", "bSpawnActor", "vSetStat", "bStaticPath",
        "vStartTimer", "bGetActorRegBool", "vStopTimer", "vMotionResume", "vMotion", "vInvalidEvent", "bProcessEventUp", "bIsTimerOn", "bIsAnimOver",
        "vSetActorRegBool", "vSubscribe", "vEnableActorDraw", "vEnableActorCol", "vEnableLineSeg", "vPublishEvent", "vDisableActorDraw", "vDisableActorCol",
        "vDisableLineSeg", "bSendTriggerEvent", "vSetUsable", "vSendTriggerAnim"};
    std::vector<AtSmallCNativeEntry> list(sizeof names / sizeof names[0] + 1);
    for (size_t i = 0; i + 1 < list.size(); i++) {
        std::strcpy(list[i].m_name, names[i]);
        list[i].m_function = native;
        list[i].m_context = g_context;
    }
    char line[1024];
    int files = 0, calls = 0;
    while (std::fgets(line, sizeof line, f)) {
        if (!std::strstr(line, ".ASL\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* g = std::fopen((std::string(dir) + "/" + line).c_str(), "rb");
        if (!g) continue;
        std::vector<unsigned char> file(0x10000, 0);
        const size_t n = std::fread(file.data(), 1, file.size(), g);
        std::fclose(g);
        CHECK(n > 0x40);
        const AtAmxHeader* h = (const AtAmxHeader*)file.data();
        for (int e = -1; e < h->m_numPublics; e++) {
            std::vector<unsigned char> program(file);
            AtSmallCScript script;
            std::memset(&script.m_amx, 0, sizeof script.m_amx);
            CHECK(AtAmxInit(&script.m_amx, program.data()) == 0);
            CHECK(AtAmxRegister(&script.m_amx, list.data(), -1) == 0);
            script.m_flags = AtSmallCScript::kFresh;
            int retval = 0;
            CHECK(AtAmxExec(&script, &retval, e, 0, nullptr) == 0);
            calls++;
        }
        files++;
    }
    std::fclose(f);
    CHECK(files == 3);
    std::printf("disc scripts: %d files, %d calls, %d built-in calls\n", files, calls, g_calls);
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("machine\n");
    test_machine();
    std::printf("host variables\n");
    test_host_vars();
    std::printf("strings\n");
    test_strings();
    std::printf("system\n");
    test_system();
    if (argc > 1) test_disc(argv[1]);
    if (g_fail) { std::printf("%d failures\n", g_fail); return 1; }
    std::printf("test_smallvm: all passed\n");
    return 0;
}
