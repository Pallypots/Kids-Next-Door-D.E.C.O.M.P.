// What sits on top of the allocator: the stack of heaps new allocates from, the original's global new and delete, and the object that creates the manager and its first heap.
// All names are inferred.
//
// In the original 0x000111F0 and 0x00011210 are the global operator new and delete. Here they have names of their own, since recovered code also shares a process with the host's
// library, which cannot use a heap stack that nothing has set up. So recovered code still reaches the host's allocator through `new`, until a port decides otherwise.
#pragma once
#include "at/AtHeap.h"

void* AtHeapNew(unsigned int size);     // 0x000111F0 from the heap on top of the stack, which must not be empty
void AtHeapDelete(void* data);          // 0x00011210 null is fine; what no heap owns goes to the C library
void AtPushHeap(void* heap);            // 0x00011240 past 16 only the depth grows
void AtPopHeap();                       // 0x00011260 unbalanced, it writes outside the table, as the original does
AtHeap* AtCurrentHeap();                // 0x00011280 null when empty
void AtSetContentHeap(AtHeap* heap);    // 0x000112A0

struct AtHeapStack {
    static int s_top;                   // 0x002AF048 starts at -1
    static AtHeap* s_heaps[16];         // 0x002C6068
    static AtHeap* s_contentHeap;       // 0x002C60A8 what the .AFL reader at 0x000114A0 falls back on; set from 0x0002D0EC. The name is a guess.
};

// Supplied by the platform layer.
void* AtPlatformAllocateRegion(unsigned int size);  // On Xbox: VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE), 0x000853E7. Null on failure.
void AtPlatformFreeRegion(void* region);            // On Xbox: VirtualFree(region, 0, MEM_RELEASE), 0x00085415.
void AtPlatformInitDevices();                       // On Xbox: 0x0026D478 with (0, 0), in the XPP section. By its shape XInitDevices; a hypothesis, there are no symbols.
void AtLibraryFree(void* data);                     // 0x001256A3, the C library's free.

struct AtMemoryParams {
    unsigned int m_regionSize;          // +0x00
    int m_unused;                       // +0x04 passed on, and not read
    unsigned int m_defaultHeapSize;     // +0x08 0 means 0x10000
};

// Vftable 0x00286694.
class AtMemorySystem {
public:
    AtMemorySystem();                   // 0x000C9AF0
    virtual void Initialize(const AtMemoryParams* params) = 0;  // slot 0
    virtual ~AtMemorySystem();          // slot 1; 0x000C9BE0 deleting, 0x000C9B10 complete
    virtual void Startup(const AtMemoryParams* params);         // slot 2, 0x000C9B90 makes this the instance
    virtual void Shutdown();            // slot 3, 0x000C9AC0

    int m_started;                      // +0x04
    AtHeapManager m_manager;            // +0x08
    AtHeap* m_defaultHeap;              // +0x25C not set by the constructor
};

// Vftable 0x00284840.
class AtRegionMemorySystem : public AtMemorySystem {
public:
    AtRegionMemorySystem();             // 0x000B3D80
    virtual void Initialize(const AtMemoryParams* params);      // slot 0, 0x000B3DB0
    virtual ~AtRegionMemorySystem();    // slot 1; 0x000B3ED0 deleting, 0x000B3E60 complete
    virtual void Shutdown();            // slot 3, 0x000B3E30 keeps the pointer it freed, so a second call frees it again

    void* m_region;                     // +0x260

    // Written by the constructor; nothing in the code reads them.
    static int s_unknown3ADE40;         // 0x003ADE40
    static short s_unknown3ADE44;       // 0x003ADE44
};

// The globals of the memory system together. A class of the original, at 0x001038C0 and 0x001038E0: what is called through the thunks at 0x000C93B0 and 0x000C93C0 is not established.
struct AtMemoryGlobals {
    AtHeapManager* m_manager;           // +0x00
    AtHeap* m_defaultHeap;              // +0x04
    void* m_fatal;                      // +0x08

    void Save();                        // 0x001038C0
    void Restore();                     // 0x001038E0 pushes the default heap if the stack is empty

    static void* s_fatal;               // 0x002B48E4 starts as the address of the fatal routine; nothing in the code calls through it
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtMemorySystem, m_manager) == 8, "AtMemorySystem::m_manager");
static_assert(offsetof(AtMemorySystem, m_defaultHeap) == 0x25C, "AtMemorySystem::m_defaultHeap");
static_assert(offsetof(AtRegionMemorySystem, m_region) == 0x260, "AtRegionMemorySystem::m_region");
#pragma pop_macro("offsetof")
#endif
