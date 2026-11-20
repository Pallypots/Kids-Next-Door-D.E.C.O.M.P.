// What the action script machine calls out to: the Flash player's display manager, its stack of contexts and its remote function calls. (Its clock, 0x00020640, is AtPlatformMilliseconds.) None of it is recovered; only what the machine's handlers
// (0x0001CB30-0x0001EF63, include/at/AtFlashScript.h) call is declared, and each of those stops with a diagnostic (src/at/AtFpDisplaySeams.cpp). AtFpDisplayManager and AtFpDisplayMgrContextStack are the original's
// own names, from its messages ("AtFpDisplayManager::vGotoFrameByNumber ..." at 0x002745FC, "AtFpDisplayMgrContextStack::poGetCurrentTarget ..." at 0x00271EC8), and so are the method names where a message gives one;
// RFC is from "RFC %s not defined. (%s)" at 0x00274888. Everything else is inferred.
#pragma once

class AtFpActionScriptValue;

// The arguments of a remote function call: 0x3C bytes, a list of named values. Only what Trace uses is declared.
class AtFpRfcArgs {
public:
    // The constructor and destructor are the original's 0x00021620 and 0x00021AD0, as named calls so that a harness can stand in for them.
    AtFpRfcArgs() { Construct(); }
    ~AtFpRfcArgs() { Destruct(); }
    void Construct();                  // 0x00021620 not recovered
    void Destruct();                   // 0x00021AD0 not recovered
    void Init();                       // 0x000216E0 not recovered; takes m_heap
    void Add(const char* name, AtFpActionScriptValue* value);  // 0x00021DD0 not recovered
    void Clear();                      // 0x00021680 not recovered

    char m_unknown00[0x38];
    void* m_heap;                      // +0x38 set by the caller before Init
};

// The application's remote function calls, at its +0x264: by name, into game code. Only Call is known.
class AtFpRfcTable {
public:
    void Call(const char* name, AtFpRfcArgs* args);  // 0x0002D9E0 not recovered; reports a name it does not have

    char m_unknown00[4];
};

// At the display manager's +0x38: 50 entries of a context and a target, counted from 1. The machine reads the current context itself.
class AtFpDisplayMgrContextStack {
public:
    struct Entry {
        void* m_context;
        void* m_target;
    };
    void SetTarget(void* target);      // 0x00016130 not recovered; on the current entry

    void* m_unknown000;                // +0x000
    Entry m_entries[50];               // +0x004
    unsigned int m_count;              // +0x194 1 to 50 when valid
};

// At the application's +0x004. Only the methods the machine calls and the context stack are known; its size is not.
class AtFpDisplayManager {
public:
    int GetCurrentFrame();             // 0x0002BF50 not recovered
    void GotoFrame(int frame);         // 0x0002B970 not recovered
    void SetPlaying(int playing);      // 0x0002BFC0 not recovered; its message says vGotoFrameByLabel, but it sets the current target's +0x68
    void StopSounds();                 // 0x0002BF20 not recovered
    void GotoFrameByLabel(const char* label);              // 0x0002BF80 not recovered
    void GotoFrameByExpression(const char* expression);    // 0x0002CB70 not recovered
    void SetTarget(const char* path);                      // 0x0002C3B0 not recovered
    void LoadMovie(const char* name, const char* target);  // 0x0002CFD0 not recovered
    AtFpRfcArgs* ResolveTarget(const char* path);          // 0x0002CC90 not recovered; what the path names, at its +0x7C
    int GetVariable(const char* name, AtFpActionScriptValue* out);    // 0x0002C9C0 not recovered
    int SetVariable(const char* name, AtFpActionScriptValue* value);  // 0x0002CAA0 not recovered
    void GetProperty(const char* target, int index, AtFpActionScriptValue* out);    // 0x0002C320 not recovered
    void SetProperty(const char* target, int index, AtFpActionScriptValue* value);  // 0x0002C360 not recovered
    int ExecuteScripts(int frame);             // 0x0002C150 not recovered; not 0 stops the script that called
    int ExecuteScripts(const char* label);     // 0x0002C1C0 not recovered; the same

    char m_unknown000[0x38];
    AtFpDisplayMgrContextStack m_contexts;     // +0x038
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtFpRfcArgs) == 0x3C, "AtFpRfcArgs");
static_assert(offsetof(AtFpDisplayMgrContextStack, m_count) == 0x194, "AtFpDisplayMgrContextStack::m_count");
static_assert(offsetof(AtFpDisplayManager, m_contexts) == 0x38, "AtFpDisplayManager::m_contexts");
static_assert(sizeof(AtFpDisplayManager) == 0x1D0, "AtFpDisplayManager");
#pragma pop_macro("offsetof")
#endif
