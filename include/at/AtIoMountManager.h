// What mounts the game's archives. All names here are inferred. Whether this is the class RTTI names AtApDataManagementApplication is not established:
// its vftable has no locator, and it is created as a part of something larger at 0x00140353.
#pragma once
#include "at/AtIoFileSystem.h"
#include "at/AtIoJam.h"
#include "at/AtIoTokenizer.h"

// Creates the file systems beneath the archives. Only two slots are known. The original's is reached at +0x318 of the owner in the Init parameters.
class AtIoFileSystemFactory {
public:
    enum Kind { kRoot = 6, kDevice = 7 };  // Names inferred from use: the first is pushed on the context, the second roots the list file.

    virtual void Unknown0() = 0;
    virtual void Unknown1() = 0;
    virtual void Unknown2() = 0;
    virtual void Unknown3() = 0;
    virtual void Unknown4() = 0;
    virtual AtIoFileSystem* Create(int kind, int a, int b) = 0;   // slot 5
    virtual void Destroy(AtIoFileSystem* fs) = 0;                 // slot 6
};

// The first member of AtIoContext. Only slot 1 is known, and its result is passed on unread.
class AtIoContextDevice {
public:
    virtual void Unknown0() = 0;
    virtual int Unknown1() = 0;     // slot 1
};

// 0x10 bytes. Its users at 0x0007CF00-0x0007D0E9 are not recovered. Created by the mount manager.
class AtIoSpaceManager {
public:
    AtIoSpaceManager();             // 0x0007CFC0
    ~AtIoSpaceManager();            // 0x0007CFE0
    void Clear();                   // 0x0007CEF0
    int Init();                     // 0x0007D000

    AtIoContextDevice* m_device;    // +0x00
    void* m_unknown04;              // +0x04
    unsigned int m_unknown08;       // +0x08
    unsigned int m_unknown0C;       // +0x0C

    static void SetHeap(void* heap);   // 0x0007CEE0
    static void* s_heap;               // 0x00301B48
    static AtIoSpaceManager* s_instance;  // 0x00301B4C
};

struct AtIoMountOwner {
    char m_unknown[0x318];
    AtIoFileSystemFactory* m_factory;   // +0x318
};

// What Init reads; the rest is not known.
struct AtIoMountParams {
    int m_unknown00;
    AtIoMountOwner* m_owner;   // +0x04
    int m_unknown08;
    int m_unknown0C;
    void* m_heap;              // +0x10
    AtIoContext* m_context;    // +0x14
};

// vftable 0x00278548.
class AtIoMountManager {
public:
    AtIoMountManager();                                  // 0x00076860
    virtual ~AtIoMountManager();                         // slot 0; 0x00076BF0 deleting, 0x00076AC0 complete
    virtual void Init(const AtIoMountParams* params);    // slot 1, 0x000768A0
    virtual void Shutdown();                             // slot 2, 0x000769F0
    virtual void CreateFileSystems();                    // slot 3, 0x00076640
    virtual void SetRegion(const char* name);            // slot 4, 0x000766A0 tells the device its root

    // JamFiles\XBox\<directory\><name>.JAM, if the device has it: replaces nothing, and is searched first from then on.
    void Mount(const char* name, const char* directory); // 0x000766E0
    void MountPath(const char* path);                    // 0x00076790 <path>.JAM; ignored if that would not fit 0x100 bytes
    void Unmount();                                      // 0x00076830
    void SwapSideArchive(const char* path);              // 0x00076B20 a second archive, not pushed by this; drops the previous one
    void DropSideArchive();                              // 0x00076A80
    // A development aid, it appears: every call from the game passes a name like AppInit.txt, and the caller checks a flag first.
    void OpenListFile(const char* name);                 // 0x00076990
    void CloseListFile();                                // 0x000769D0
    void WriteList(const char* text);                    // 0x00076670

    AtIoMountOwner* m_owner;        // +0x04
    AtIoSpaceManager* m_space;      // +0x08
    char m_region[8];               // +0x0C
    AtIoContext* m_context;         // +0x14
    AtIoFileSystemFactory* m_factory;  // +0x18
    AtIoFileSystem* m_device;       // +0x1C
    AtIoJamFileSystem* m_mounted;   // +0x20
    AtIoJamFileSystem* m_side;      // +0x24
    AtIoFileSystem* m_unknown28;    // +0x28 only ever cleared and destroyed in what is recovered
    AtIoFileSystem* m_root;         // +0x2C
    AtIoFileRef m_listFile;         // +0x30

    static AtIoMountManager* s_instance;  // 0x002E26E0
    static void* s_heap;            // 0x002E26E4
};

void AtIoNotifyMounted(AtIoJamFileSystem* fs);       // 0x0007D140 header slot 4
void AtIoNotifyUnmounted(AtIoJamFileSystem* fs);     // 0x0007D150 header slot 5
void AtIoDeleteObject(AtIoFileSystem* fs);           // 0x000EB9D0 deleting destructor, if not null; shared by other classes through folding

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtIoSpaceManager) == 0x10, "AtIoSpaceManager");
static_assert(offsetof(AtIoMountManager, m_context) == 0x14, "AtIoMountManager::m_context");
static_assert(offsetof(AtIoMountManager, m_listFile) == 0x30, "AtIoMountManager::m_listFile");
static_assert(sizeof(AtIoMountManager) == 0x38, "AtIoMountManager");
#pragma pop_macro("offsetof")
#endif
