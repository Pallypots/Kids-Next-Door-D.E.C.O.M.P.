// The objects the lower AtAp layers bring up at start-up: the event layer's, the network layer's and the I/O layer's, the I/O layer's file system factory,
// and the saved memory globals each keeps on the stack while it works. Subsystem at/atap. Reference build knd-xbox-usa-5454008a-v1.
//
// RTTI names sixteen AtAp*Application classes beneath GmApp, but none of these objects: their vftables have no locator. Every class and member name here
// is inferred. Which layer owns which object is inferred from the order of the layers' constructors (0x00141830 makes the memory system and the object
// at +0x268 of App, 0x00141910 the one at +0x2F0, 0x00141970 the one at +0x310) against RTTI's order of the classes; see docs/worklog/2026-11-08-atap.md.
// App's constructor writes a vftable of its own over the network's and the I/O layer's (0x0028DC90, 0x0028DC7C): those differ only in the destructor.
//
// The layers' Inits take a config that App fills by a virtual slot (include/gm/GmApp.h), the layer below's object, and for most App's memory system.
#pragma once
#include "at/AtIoMountManager.h"
#include "at/AtMemory.h"
#include "gm/GmNameMap.h"

// ---- the memory globals, saved and put back ---------------------------------------------------------------------------

// AtMemoryGlobals as the layers keep it. Save and Restore are the base's, through a jump each.
struct AtApGlobals : AtMemoryGlobals {
    AtApGlobals();                   // 0x000C93A0
    void Save();                     // 0x000C93B0
    void Restore();                  // 0x000C93C0
};

// What the I/O layer keeps: one word more, cleared.
struct AtIoGlobals : AtApGlobals {
    AtIoGlobals();                   // 0x000C9200
    void Save();                     // 0x000C9220
    void Restore();                  // 0x000C9230
    int m_unknown0C;                 // +0x0C never read
};

// ---- the event layer --------------------------------------------------------------------------------------------------

// What App's slot 7 fills in (0x30 bytes). Each heap that is 0 falls back to m_heap.
struct AtEventConfig {
    unsigned int m_spaces;           // +0x00 the first namespace database's size, if not 0
    unsigned int m_spaces2;          // +0x04 the second's
    AtHeap* m_heap;                  // +0x08
    int m_unknown0C;                 // +0x0C not read
    void* m_heap10;                  // +0x10 for the class at 0x000C97B3, by 0x000C9650
    void* m_heap14;                  // +0x14 for 0x00305918, by 0x000991E0
    void* m_heap18;                  // +0x18 for the class at 0x000C9541, by 0x000C9460
    void* m_heap1C;                  // +0x1C for 0x003ADF94, by 0x000C93F0
    void* m_heap20;                  // +0x20 for 0x003ADF90, by 0x000C93E0
    void* m_heap24;                  // +0x24 for 0x003ADF8C, by 0x000C93D0
    void* m_nameMapHeap;             // +0x28 GmNameMap's, unless already set
    void* m_nameSpaceHeap;           // +0x2C GmNameSpaceDb's, unless already set
};

// Two namespace databases, each with a default group: what the event layer keeps by name. vftable 0x0027C1B8, 0x80 bytes. App's slot 56 initialises
// the same object again, with 16 and 16.
class AtEventNames {
public:
    AtEventNames();                                       // 0x000991F0
    virtual void DeletingDestructor(unsigned int flags);  // slot 0, 0x00099390 not recovered
    void Init(unsigned int spaces, unsigned int spaces2); // 0x00099250 names both groups empty, then sizes each database that is given a size

    int m_unknown04;                 // +0x04 not set here
    GmNameMapGroup m_group;          // +0x08
    GmNameSpaceDb m_spaces;          // +0x20
    GmNameMapGroup m_group2;         // +0x44
    GmNameSpaceDb m_spaces2;         // +0x5C
};

// The event layer's object, at +0x268 of App (0x88 bytes, vftable 0x00284830).
class AtEventSystem {
public:
    AtEventSystem();                                      // 0x000B3BD0
    virtual void DeletingDestructor(unsigned int flags);  // slot 0, 0x000B3D60 not recovered
    virtual void Init(const AtEventConfig* config, void* below);  // slot 1, 0x000B3C50 below is App's memory system
    virtual void Shutdown();                              // slot 2, 0x000B3BB0 not recovered
    virtual void SetHeaps(const AtEventConfig* config);   // slot 3, 0x000B3C90

    AtEventNames m_names;            // +0x04
    void* m_below;                   // +0x84 set while running

    static void* s_heap305918;       // 0x00305918
    static void* s_heap3ADF94;       // 0x003ADF94
    static void* s_heap3ADF90;       // 0x003ADF90
    static void* s_heap3ADF8C;       // 0x003ADF8C
};
// 0x000C9650 and 0x000C9460, the first and third setters, are at/aee's AtEventDatabase::SetHeap and AtEventList::SetHeap (include/at/AtEvent.h).
void AtSetHeap305918(void* heap);    // 0x000991E0
void AtSetHeap3ADF94(void* heap);    // 0x000C93F0
void AtSetHeap3ADF90(void* heap);    // 0x000C93E0
void AtSetHeap3ADF8C(void* heap);    // 0x000C93D0

// ---- the network layer ------------------------------------------------------------------------------------------------

// What the network layer's Init takes (0x6C bytes). App leaves it as start-up cleared it (slot 11 is the empty 0x000CBF30), and the layer turns it off
// before it reads it, so nothing of the Xbox's network is brought up.
struct AtNetworkConfig {
    int m_enabled;                   // +0x00 written 0 by AtNetworkSystem::Init
    int m_settings[4];               // +0x04 copied, not read here
    int m_unknown14[22];             // +0x14 not read
};

// vftable 0x00286664 (0x20 bytes).
class AtNetworkSystemBase {
public:
    AtNetworkSystemBase();                                // 0x000C92E0
    virtual void DeletingDestructor(unsigned int flags);  // slot 0, 0x000C9370 not recovered
    virtual void Init(AtNetworkConfig* config, void* below);  // slot 1, 0x000C9300
    virtual void Shutdown();                              // slot 2, 0x000C9340 not recovered
    virtual void SetHeaps(const AtNetworkConfig* config) {}  // slot 3, 0x000CBF30 empty (folded)

    int m_enabled;                   // +0x04
    int m_settings[4];               // +0x08
    void* m_below;                   // +0x18 set while running
    int m_unknown1C;                 // +0x1C
};

// The network layer's object, at +0x2F0 of App (vftable 0x00284820).
class AtNetworkSystem : public AtNetworkSystemBase {
public:
    AtNetworkSystem();                                    // 0x000B3AE0
    void Init(AtNetworkConfig* config, void* below) override;  // slot 1, 0x000B3B00 below is the event layer's object
    void Shutdown() override;                             // slot 2, 0x000B3B30 not recovered: a jump to the base's
};

// ---- the I/O layer ----------------------------------------------------------------------------------------------------

// What App's slot 15 fills in (0x44 bytes). Each heap from +0x18 on that is 0 falls back to m_heap.
struct AtIoConfig {
    int m_kind;                      // +0x00 0 brings nothing up; 1 is taken as 2
    unsigned int m_stackSize;        // +0x04 file systems the context can stack; 0 means 1
    unsigned int m_spaces;           // +0x08 the context's namespace database; 0 means 8
    unsigned int m_bufferBytes;      // +0x0C the read buffer
    unsigned int m_requests;         // +0x10 handed to the factory's device; none is made for 0
    AtHeap* m_heap;                  // +0x14
    void* m_jamHeap;                 // +0x18 AtIoJamFileSystem's, which the factory also makes its own from
    void* m_jamHeaderHeap;           // +0x1C AtIoJamFileSystem's headers, and the device's at 0x00301B6C and 0x003B23EC
    void* m_contextHeap;             // +0x20 AtIoContext's
    void* m_bufferHeap;              // +0x24 the read buffer's
    void* m_textHeap;                // +0x28 AtTextTable's
    void* m_fastJamHeap;             // +0x2C AtIoFastJamHeader's, 0x00301B64's, 0x00083490's and AtIoFastAlignedJamHeader's
    int m_unknown30;                 // +0x30 not read
    int m_unknown34;                 // +0x34 not read
    void* m_streamHeap;              // +0x38 AtIoStream's, unless already set
    void* m_nameMapHeap;             // +0x3C GmNameMap's, unless already set
    void* m_nameSpaceHeap;           // +0x40 GmNameSpaceDb's, unless already set
};
void AtIoSetDeviceHeaps(const AtIoConfig* config);   // 0x000C9280 then the factory's
void AtIoSetFactoryHeaps(const AtIoConfig* config);  // 0x00102F40 all of AtIoSystemBase::SetHeaps's again, but 0x00083490's
void AtSetHeap301B64(void* heap);    // 0x00080CC0 users at 0x00081134
void AtSetHeap3B23EC(void* heap);    // 0x00103050 the device file system's (0x00103797)
void AtSetHeap301B6C(void* heap);    // 0x00081760 users at 0x00082335
extern void* g_heap301B64;           // 0x00301B64
extern void* g_heap3B23EC;           // 0x003B23EC
extern void* g_heap301B6C;           // 0x00301B6C

// The file system factory: one object, at 0x003ADF84, made by a static constructor (0x00201AF0). vftable 0x00289BC4.
// Slots 0 and 5 make the Xbox's I/O device (0x5AE0 bytes, 0x0011E4E0) and its file system (0x6C0 bytes, 0x001030C0), neither recovered: they are
// defined apart, as a stop in src/at/AtIoSystemDevices.cpp, and by a port in its platform layer (platform/host_io.cpp), which replaces them.
class AtIoSystemFactory : public AtIoFileSystemFactory {
public:
    AtIoSystemFactory();                                  // 0x00102EC0
    AtIoContextDevice* CreateDevice(int requests) override;  // slot 0, 0x00102D50: the platform's
    void DestroyDevice(AtIoContextDevice* device) override;  // slot 1, 0x000EB9D0 (folded), the device's deleting destructor: the platform's
    void Init(const void* params) override;               // slot 3, 0x00102ED0 params is not read
    void Shutdown() override;                             // slot 4, 0x00102EF0
    AtIoFileSystem* Create(int kind, int a, int b) override;  // slot 5, 0x00102E00: the platform's
    void Destroy(AtIoFileSystem* fs) override;            // slot 6, 0x000EB9D0 AtIoDeleteObject (at/mount; folded)
    unsigned int BlockSize() override;                    // slot 7, 0x00102EB0 0x8000
    unsigned int MinimumBlocks() override;                // slot 8, 0x0010F4B0 4

    int m_running;                   // +0x04

    static AtIoSystemFactory s_instance;  // 0x003ADF84
};
// Puts the saved globals back, starts the factory, saves them again, and returns it. config is not read.
AtIoSystemFactory* AtIoGetFactory(AtIoGlobals* saved, const AtIoConfig* config);  // 0x000C9240

// vftable 0x00286650 (0x74 bytes).
class AtIoSystemBase {
public:
    AtIoSystemBase();                                     // 0x000C8FC0 not 0x000191C0 for the context: see AtIoContext()
    virtual void DeletingDestructor(unsigned int flags);  // slot 0, 0x000C91E0 not recovered
    virtual void Init(AtIoConfig* config, void* below, void* memory);  // slot 1, 0x000C8FF0 memory is not read
    virtual void Slot2() {}                               // slot 2, 0x001CC9F0 empty (folded)
    virtual void Shutdown();                              // slot 3, 0x000C9020 not recovered
    virtual void SetHeaps(const AtIoConfig* config);      // slot 4, 0x000C9040

    void InitContext(AtIoSystemFactory* factory, const AtIoConfig* config);  // 0x000C8F90

    void* m_below;                   // +0x04 set while running
    AtIoSystemFactory* m_factory;    // +0x08 App+0x318: what the mount manager creates file systems with
    AtIoContext m_context;           // +0x0C App+0x31C: the context everything opens files through
    int m_kind;                      // +0x70 1 until Init
};

// The I/O layer's object, at +0x310 of App (vftable 0x0028480C).
class AtIoSystem : public AtIoSystemBase {
public:
    AtIoSystem();                                         // 0x000B3940
    void Init(AtIoConfig* config, void* below, void* memory) override;  // slot 1, 0x000B3960 below is the network layer's object
    void Shutdown() override;                             // slot 3, 0x000B3A20 not recovered
    void SetHeaps(const AtIoConfig* config) override;     // slot 4, 0x000B3A40
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtIoGlobals) == 0x10, "AtIoGlobals");
static_assert(sizeof(AtEventConfig) == 0x30, "AtEventConfig");
static_assert(offsetof(AtEventNames, m_spaces2) == 0x5C, "AtEventNames::m_spaces2");
static_assert(sizeof(AtEventNames) == 0x80, "AtEventNames");
static_assert(offsetof(AtEventSystem, m_below) == 0x84, "AtEventSystem::m_below");
static_assert(sizeof(AtEventSystem) == 0x88, "AtEventSystem");
static_assert(sizeof(AtNetworkConfig) == 0x6C, "AtNetworkConfig");
static_assert(offsetof(AtNetworkSystemBase, m_below) == 0x18, "AtNetworkSystemBase::m_below");
static_assert(sizeof(AtNetworkSystem) == 0x20, "AtNetworkSystem");
static_assert(sizeof(AtIoConfig) == 0x44, "AtIoConfig");
static_assert(sizeof(AtIoSystemFactory) == 8, "AtIoSystemFactory");
static_assert(offsetof(AtIoSystemBase, m_context) == 0x0C, "AtIoSystemBase::m_context");
static_assert(offsetof(AtIoSystemBase, m_kind) == 0x70, "AtIoSystemBase::m_kind");
static_assert(sizeof(AtIoSystem) == 0x74, "AtIoSystem");
#pragma pop_macro("offsetof")
#endif
