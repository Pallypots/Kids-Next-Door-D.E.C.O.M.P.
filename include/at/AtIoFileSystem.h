// AtIoPath, AtIoFileSystem, AtIoContext — recovered types (partial).
//
// Reference build: knd-xbox-usa-5454008a-v1. Evidence classes as in at/AtIoJam.h.
//   ORIGINAL  AtIoContext: the game's own name, from the error string at 0x002721C0.
//   INFERRED  AtIoPath, AtIoFileSystem, AtIoFileStat, and every method/member name.
//
// A file system is one source of files: a device, or a mounted archive. AtIoContext keeps a stack of them and asks each in turn.
#pragma once

class AtIoFile;

// What GetFileInfo fills in. 0x118 bytes: see 0x0001F4A0 and 0x0007D5A0.
struct AtIoFileStat {
    char m_name[0x10C];        // +0x000 the archive's own name, for a file inside one
    unsigned int m_time;       // +0x10C meaning inferred
    unsigned int m_offset;     // +0x110
    unsigned int m_size;       // +0x114
};

// A root directory plus a stack of relative directories, and the full path last built from them.
// Original behaviour preserved: no length checks in Resolve or Push.
class AtIoPath {
public:
    AtIoPath();                                                      // 0x00018340
    ~AtIoPath();                                                     // 0x00018760
    void Clear();                                                    // 0x00018760 (same code: folded)
    void Init();                                                     // 0x000187B0 clears if used, marks as used
    void SetRoot(const char* root);                                  // 0x00018390 adds a trailing backslash if missing
    void Resolve(const char* relative);                              // 0x00018410 builds m_full: root, current, relative
    int GetCurrentPath(char* out, unsigned int size);                // 0x000184A0 root and current; 0 if it does not fit
    int Join(char* out, unsigned int size, const char* relative);    // 0x00018540 current and relative; 0 if it does not fit
    const char* Current();                                           // 0x00018610 null when m_top is -1
    void Push(const char* directory);                                // 0x00018630 ignored past the eighth
    void PushFromFile(const char* path);                             // 0x000186B0 pushes the directory part, '/' becomes '\\'
    void Pop(const char* last);                                      // 0x00018720 first overwrites the top entry with `last`, if given

    char m_root[0x80];         // +0x000
    char m_full[0x10C];        // +0x080
    char m_unknown18C[0x10C];  // +0x18C
    char m_stack[8][0x80];     // +0x298
    int m_top;                 // +0x698
    int m_initialised;         // +0x69C
};

// vftable 0x00272644. Pure slots are the ones a real file system must supply.
class AtIoFileSystem {
public:
    enum { kNotImplemented = 0x15 };

    AtIoFileSystem();                                                // 0x0001F4D0
    virtual ~AtIoFileSystem() = 0;                                   // slot 0; 0x0001F450
    virtual int OpenWithCallback(const char* path, int flags, void* callback, AtIoFile** out);  // slot 1, 0x0001F470; names inferred
    virtual int Open(const char* path, int flags, AtIoFile** out) = 0;      // slot 2
    virtual int Close(AtIoFile* file) = 0;                           // slot 3
    virtual int IsDirectoryMissing(const char* path) = 0;            // slot 4
    virtual int IsFileMissing(const char* path) = 0;                 // slot 5, 0 when the file exists
    virtual int GetEntryType(const char* path, int* type) = 0;       // slot 6
    virtual int Unknown7(void* a) = 0;                               // slot 7
    virtual int GetFileInfo(const char* path, AtIoFileStat* stat);   // slot 8, 0x0001F4A0
    virtual void* List(const char* pattern) = 0;                     // slot 9
    virtual void FreeList(void* list) = 0;                           // slot 10
    virtual int Unknown11(void* a) = 0;                              // slot 11
    virtual int Unknown12() = 0;                                     // slot 12
    virtual void SetRoot(const char* root);                          // slot 13, 0x000817D0
    virtual int Unknown14(void* a);                                  // slot 14, 0x0001F490

    void InitPath();                                                 // 0x0001F460
    void ResetFileSystem();                                          // 0x0001F510

    AtIoPath m_path;           // +0x004
    int m_kind;                // +0x6A4 5 for an archive
    int m_openFiles;           // +0x6A8
    unsigned int m_maxOpen;    // +0x6AC
    int m_peakOpenFiles;       // +0x6B0
    void* m_unknown6B4;        // +0x6B4 the mount manager stores its list file here
    int m_unknown6B8;          // +0x6B8 an archive copies this from its device
};

// Only the part that manages file systems. The class is larger; see 0x00019220. m_stackCount is at +0x58.
class AtIoContext {
public:
    void Push(AtIoFileSystem* fs);                                   // 0x00019290
    void Pop();                                                      // 0x000192C0
    AtIoFileSystem* Top();                                           // 0x000192E0
    // Searches from the top. 0 and writes *fs when one has the file; 1 otherwise, with *fs null if fs was given.
    int FindFile(const char* path, AtIoFileSystem** fs);             // 0x00019300

    char m_unknown00[0x58];
    unsigned int m_stackCount;   // +0x58
    unsigned int m_stackSize;    // +0x5C
    AtIoFileSystem** m_stack;    // +0x60

    static AtIoContext* s_instance;  // 0x002C62F0
    static void (*s_openHook)();     // 0x002C62F4
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtIoFileStat) == 0x118, "AtIoFileStat");
static_assert(offsetof(AtIoPath, m_stack) == 0x298, "AtIoPath::m_stack");
static_assert(offsetof(AtIoPath, m_top) == 0x698, "AtIoPath::m_top");
static_assert(sizeof(AtIoPath) == 0x6A0, "AtIoPath");
static_assert(offsetof(AtIoFileSystem, m_kind) == 0x6A4, "AtIoFileSystem::m_kind");
static_assert(offsetof(AtIoFileSystem, m_unknown6B8) == 0x6B8, "AtIoFileSystem::m_unknown6B8");
static_assert(sizeof(AtIoFileSystem) == 0x6BC, "AtIoFileSystem");
static_assert(offsetof(AtIoContext, m_stack) == 0x60, "AtIoContext::m_stack");
#pragma pop_macro("offsetof")
#endif
