// What mounts the game's archives. All names here are inferred. Whether this is the class RTTI names AtApDataManagementApplication is not established:
// its vftable has no locator, and it is created as a part of something larger at 0x00140353.
#pragma once
#include "at/AtIoFileSystem.h"
#include "at/AtIoJam.h"
#include "at/AtIoTokenizer.h"

void AtFatal(const char* message, int file, int line, int unused);

class AtIoContextDevice;

// Creates the file systems beneath the archives. The original's is reached at +0x318 of the owner in the Init parameters: the I/O layer's factory, the one
// object of vftable 0x00289BC4 (at/AtApLayer.h). Slots 0-4, 7 and 8 were named by the at/atap work; their bodies here stop, for doubles that do not
// supply them.
class AtIoFileSystemFactory {
public:
    enum Kind { kRoot = 6, kDevice = 7 };  // Names inferred from use: the first is pushed on the context, the second roots the list file.

    virtual AtIoContextDevice* CreateDevice(int requests) { AtFatal("AtIoFileSystemFactory: slot 0 is not supplied", requests, 0, 0); return 0; }  // slot 0
    virtual void DestroyDevice(AtIoContextDevice* device) { AtFatal("AtIoFileSystemFactory: slot 1 is not supplied", device != 0, 0, 0); }       // slot 1
    virtual void DeletingDestructor(unsigned int flags) { AtFatal("AtIoFileSystemFactory: slot 2 is not supplied", (int)flags, 0, 0); }         // slot 2
    virtual void Init(const void* params) { AtFatal("AtIoFileSystemFactory: slot 3 is not supplied", params != 0, 0, 0); }                      // slot 3
    virtual void Shutdown() { AtFatal("AtIoFileSystemFactory: slot 4 is not supplied", 0, 0, 0); }                                               // slot 4
    virtual AtIoFileSystem* Create(int kind, int a, int b) = 0;   // slot 5
    virtual void Destroy(AtIoFileSystem* fs) = 0;                 // slot 6
    virtual unsigned int BlockSize() { AtFatal("AtIoFileSystemFactory: slot 7 is not supplied", 0, 0, 0); return 0; }                         // slot 7
    virtual unsigned int MinimumBlocks() { AtFatal("AtIoFileSystemFactory: slot 8 is not supplied", 0, 0, 0); return 0; }                     // slot 8
};

// The first member of AtIoContext. Only slot 1 is known, and its result is passed on unread.
class AtIoContextDevice {
public:
    virtual void Unknown0() = 0;
    virtual int Unknown1() = 0;     // slot 1
    // Slot 2: start-up (0x0008E907) calls it on the space manager's device with (4, 0) and hands the result to 0x0007D070. Not recovered.
    virtual void* Unknown2(int a, int b) { AtFatal("AtIoContextDevice: slot 2 is not recovered", a, b, 0); return 0; }
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
