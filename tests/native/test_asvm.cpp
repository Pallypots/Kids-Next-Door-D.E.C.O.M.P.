// Host test of the action script machine on any pointer size: values' forms, the stack's pushes and pops, and whole script blocks through Run, with the display manager's calls answered here.
// Not evidence of a byte match; the differential oracle is tests/oracle/oracle_asvm.cpp.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "at/AtFlashScript.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

int g_heap;
std::string g_calls;

// The display manager's calls this test needs, answered by logging.
}  // namespace


int AtFpDisplayManager::GetCurrentFrame() { g_calls += "cur;"; return 7; }
void AtFpDisplayManager::GotoFrame(int frame) { g_calls += "goto " + std::to_string(frame) + ";"; }
void AtFpDisplayManager::SetPlaying(int playing) { g_calls += "play " + std::to_string(playing) + ";"; }
void AtFpDisplayManager::StopSounds() { g_calls += "stopsounds;"; }
void AtFpDisplayManager::GotoFrameByLabel(const char* label) { g_calls += std::string("label ") + label + ";"; }
void AtFpDisplayManager::GotoFrameByExpression(const char* e) { g_calls += std::string("expr ") + e + ";"; }
void AtFpDisplayManager::SetTarget(const char* path) { g_calls += std::string("target ") + path + ";"; }
void AtFpDisplayManager::LoadMovie(const char* name, const char* target) { g_calls += std::string("load ") + name + "," + target + ";"; }
AtFpRfcArgs* AtFpDisplayManager::ResolveTarget(const char* path) { g_calls += std::string("resolve ") + path + ";"; return nullptr; }
int AtFpDisplayManager::GetVariable(const char* name, AtFpActionScriptValue* out) {
    g_calls += std::string("get ") + name + ";";
    out->SetText("41");
    return 1;
}
int AtFpDisplayManager::SetVariable(const char* name, AtFpActionScriptValue* value) {
    g_calls += std::string("set ") + name + "=" + value->Text() + ";";
    return 1;
}
void AtFpDisplayManager::GetProperty(const char*, int, AtFpActionScriptValue*) {}
void AtFpDisplayManager::SetProperty(const char*, int, AtFpActionScriptValue*) {}
int AtFpDisplayManager::ExecuteScripts(int frame) { g_calls += "exec " + std::to_string(frame) + ";"; return frame == 8; }
int AtFpDisplayManager::ExecuteScripts(const char* label) { g_calls += std::string("exec ") + label + ";"; return 0; }
void AtFpDisplayMgrContextStack::SetTarget(void*) { g_calls += "context;"; }
void AtFpRfcTable::Call(const char* name, AtFpRfcArgs*) { g_calls += std::string("rfc ") + name + ";"; }
void AtFpRfcArgs::Construct() {}
void AtFpRfcArgs::Destruct() {}
void AtFpRfcArgs::Init() {}
void AtFpRfcArgs::Add(const char*, AtFpActionScriptValue*) {}
void AtFpRfcArgs::Clear() {}

namespace {

// A script block written here: each instruction its code, and from 0x80 on its length and data.
struct Script {
    std::vector<std::vector<unsigned char>> code;
    std::vector<unsigned char*> pointers;
    AtFpScript script;
    void op(unsigned c) { code.push_back({(unsigned char)c, 0}); }
    void op(unsigned c, std::vector<unsigned char> data) {
        std::vector<unsigned char> i = {(unsigned char)c, 0, (unsigned char)data.size(), (unsigned char)(data.size() >> 8)};
        i.insert(i.end(), data.begin(), data.end());
        code.push_back(i);
    }
    void pushInt(int v) { op(0x96, {7, 0, (unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24)}); }
    void pushReal(float f) {
        unsigned v;
        std::memcpy(&v, &f, 4);
        op(0x96, {1, 0, (unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24)});
    }
    void pushText(const char* s) {
        std::vector<unsigned char> d = {0, 0};
        d.insert(d.end(), s, s + std::strlen(s) + 1);
        op(0x96, d);
    }
    AtFpScript* finish() {
        op(0);
        for (auto& c : code) pointers.push_back(c.data());
        script.m_instructions = pointers.data();
        script.m_read = (unsigned)pointers.size();
        script.m_instructionCount = script.m_read;
        return &script;
    }
    ~Script() {
        script.m_instructions = nullptr;
        script.m_bytes = nullptr;
        script.m_read = 0;
    }
};

struct Machine {
    AtFlashApplication app;
    AtTextTable strings;
    AtFpActionScriptVM vm;
    Machine() {
        std::memset(&app, 0, sizeof app);
        app.m_display.m_contexts.m_count = 1;
        vm.Init(&app, &strings);
        g_calls.clear();
    }
    AtFpActionScriptValue& top() { return vm.m_stack.m_values[vm.m_stack.m_top]; }
};

}  // namespace

int main() {
    AtFpActionScriptValue::s_heap = &g_heap;
    AtFpActionScriptStack::s_heap = &g_heap;
    AtFpActionScriptVM::s_heap = &g_heap;
    AtFpScript::s_heap = &g_heap;
    AtIoTokenFile::s_heap = &g_heap;
    GmNameMap::s_heap = &g_heap;

    // A value's forms.
    {
        AtFpActionScriptLocalValue v;
        float f = 2.5f;
        v.SetReal(&f);
        CHECK(v.m_type == AtFpActionScriptValue::kTypeReal && v.ToInt() == 2);
        CHECK(std::strcmp(v.ToText(), "2.5") == 0);
        CHECK(v.m_text == v.m_buffer);
        f = 3.0f;
        v.SetReal(&f);
        CHECK(v.m_type == AtFpActionScriptValue::kTypeInt && v.m_int == 3);
        f = 3e9f;
        v.SetReal(&f);
        CHECK(std::strcmp(v.ToText(), "3e+009") == 0);
        f = 1234565.5f;
        v.SetReal(&f);
        CHECK(std::strcmp(v.ToText(), "1.23457e+006") == 0);
        f = 0.0001f;
        v.SetReal(&f);
        CHECK(std::strcmp(v.ToText(), "0.0001") == 0);
        int i = -42;
        v.SetInt(&i);
        CHECK(std::strcmp(v.ToText(), "-42") == 0 && v.Real() == -42.0f);
        v.SetText("12abc");
        CHECK(v.ToInt() == 12 && v.ToReal() == 12.0f && v.IsInteger() == 0);
        v.SetText("-17");
        CHECK(v.IsInteger() == 1);
        v.SetText("0x10");
        CHECK(v.ToReal() == 0.0f);
        v.SetText("inf");
        CHECK(v.ToReal() == 0.0f);
        v.SetText(static_cast<const char*>(nullptr));
        CHECK(v.m_type == AtFpActionScriptValue::kTypeNone && std::strcmp(v.ToText(), "") == 0 && v.Length() == 0);
        // Long text leaves the buffer.
        std::string longText(300, 'x');
        v.SetText(longText.c_str());
        CHECK(v.m_text != v.m_buffer && v.Length() == 300);
    }

    // The stack: 30 places; a push to a full stack does nothing.
    {
        Machine m;
        for (int i = 0; i < 40; i++) m.vm.m_stack.PushInt(i);
        CHECK(m.vm.m_stack.m_top == 29);
        AtFpActionScriptLocalValue v;
        CHECK(m.vm.m_stack.Pop(&v) == 1 && v.m_int == 29);
        while (m.vm.m_stack.Discard()) {
        }
        CHECK(m.vm.m_stack.Pop(&v) == 0 && m.vm.m_stack.m_top == -1);
    }

    // Arithmetic, comparison and strings through Run.
    {
        Machine m;
        Script s;
        s.pushInt(7);
        s.pushInt(2);
        s.op(0x0B);           // 7 - 2
        s.pushReal(0.5f);
        s.op(0x0C);           // 5 * 0.5
        s.pushText("a");
        s.pushText("bc");
        s.op(0x21);           // "abc"
        s.op(0x14);           // 3
        s.op(0x0A);           // 2.5 + 3
        m.vm.Run(s.finish());
        CHECK(m.vm.m_stack.m_top == 0);
        CHECK(m.top().m_type == AtFpActionScriptValue::kTypeReal && m.top().m_real == 5.5f);
    }
    {
        Machine m;
        Script s;
        s.pushText("hello");
        s.pushInt(2);
        s.pushInt(3);
        s.op(0x15);           // "ell"
        s.pushText("hello");
        s.pushInt(5);         // the last character cannot be taken
        s.pushInt(1);
        s.op(0x15);           // ""
        m.vm.Run(s.finish());
        CHECK(m.vm.m_stack.m_top == 1);
        CHECK(std::strcmp(m.vm.m_stack.m_values[0].m_text, "ell") == 0);
        CHECK(std::strcmp(m.vm.m_stack.m_values[1].m_text, "") == 0);
    }
    // Jump and If go by instruction; Call can stop the script.
    {
        Machine m;
        Script s;
        s.pushInt(1);         // 0
        s.op(0x9D, {4, 0});   // 1: if true, to 4
        s.pushInt(99);        // 2
        s.op(0x99, {6, 0});   // 3
        s.pushInt(5);         // 4
        s.op(0x99, {99, 0});  // 5: past m_read, so not taken
        s.pushInt(9);         // 6
        s.op(0x9E);           // 7: exec 8, which stops
        s.pushInt(77);        // 8: not reached
        m.vm.Run(s.finish());
        CHECK(m.vm.m_stack.m_top == 0 && m.top().m_int == 5);
        CHECK(g_calls == "exec 8;");
    }
    // Variables, frames and the target.
    {
        Machine m;
        Script s;
        s.pushText("score");
        s.op(0x1C);           // get: "41"
        s.pushInt(1);
        s.op(0x0A);           // 42
        s.pushText("total");
        s.op(0x4F);           // not an action here: nothing
        s.op(0x04);           // next frame
        s.op(0x07);
        s.pushText("");
        s.op(0x20);           // back to the context
        s.op(0x83, {'a', '.', 's', 'w', 'f', 0, 't', 0});
        m.vm.Run(s.finish());
        CHECK(g_calls == "get score;cur;goto 8;play 0;context;load a,t;");
        CHECK(m.vm.m_stack.m_top == 1 && m.vm.m_stack.m_values[0].m_int == 42);
    }
    std::printf("%s\n", g_fail ? "test_asvm FAILED" : "test_asvm passed");
    return g_fail ? 1 : 0;
}
