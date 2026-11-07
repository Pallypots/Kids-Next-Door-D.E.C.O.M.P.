// What turns a physical input into an abstract one: .ANM files, "AbstractMapper" by the file's own word, and what is set up from them. Original address cluster 0x00072B70-0x00073BEF and 0x00078940-0x000793AF.
// The keyword names are the original's, from its tables at 0x002B2878 and 0x002B2C10; other names are inferred. The controllers these map onto, .ANA, are in AtInputController.h.
#pragma once

#include "at/AtInputController.h"
#include "at/AtIoTokenizer.h"
#include "gm/GmNameMap.h"

// Vftable 0x002785FC. A pool of links for one mapper, and the links now bound. What is not its own it passes along, to the queue of the controller a mapper is bound to.
class AtInputRouter : public AtInputReceiver {
public:
    AtInputRouter();                // 0x00078BD0
    ~AtInputRouter();               // 0x000792F0

    void Slot0(void* a) override;   // slot 0, 0x00078B60 passed on
    void Slot1(void* a) override;   // slot 1, 0x00078B70 passed on
    void Slot2(void* a) override;   // slot 2, 0x00078B90 passed on
    void Slot3(void* a, void* b) override;  // slot 3, 0x00078BB0 passed on
    void Unbind(AtInputController* controller, int id) override;  // slot 4, 0x000791B0 every link it holds for the pair; ids with bit 30 in the second list too; then passed on

    void AllocateButtonLinks(unsigned int count);  // 0x00078C40
    void AllocateAxisLinks(unsigned int count);    // 0x00078D00
    void UnbindAll();               // 0x00078DC0 the chains of the controllers first; every link goes back to the free list
    void Release();                 // 0x000792A0 keeps the free list's two pointers, which then point at what it freed
    void Allocate(void* device, unsigned int buttons, unsigned int axes);  // 0x00079350

    // A null controller means every controller of the device: the two it names, those it numbers, those connected. A controller without the id takes no link.
    int BindButton(AtInputController* controller, int physical, int state, int abstractId);  // 0x00079120 always 1
    int BindAxis(AtInputController* controller, int physical, int abstractId);  // 0x00079170 always 1
    int BindAll(int physical, int state, int abstractId);  // 0x00079040 always 1
    void BindButtonTo(AtInputController* controller, int physical, int state, int abstractId);  // 0x00078EF0 nothing if it is bound already. With no free link, crashes.
    void BindAxisTo(AtInputController* controller, int physical, int unused, int abstractId);  // 0x00078FA0 the same
    AtInputLink* FindButtonLink(AtInputController* controller, int physical, int state, int abstractId);  // 0x00078E50
    AtInputLink* FindAxisLink(AtInputController* controller, int physical, int abstractId);  // 0x00078EA0

    static void SetHeap(void* heap);  // 0x00078B50

    void (AtInputRouter::*m_bindFunction)(AtInputController*, int, int, int);  // +0x04 what BindAll does to each controller; left set
    void* m_device;                 // +0x08
    AtInputReceiver* m_passOnTo;    // +0x0C
    AtInputLink* m_buttonLinks;     // +0x10
    AtInputLink* m_axisLinks;       // +0x14
    AtInputLink* m_freeFirst;       // +0x18
    AtInputLink* m_freeLast;        // +0x1C
    AtInputLinkList m_boundButtons; // +0x20
    AtInputLinkList m_boundAxes;    // +0x30
    unsigned int m_linkCount;       // +0x40 not set by the constructor
    int m_unknown44;                // +0x44 not touched

    static void* s_heap;            // 0x00301B04
};

// One ButtonMap makes two of these: the state it names, and then the same pair with state 0.
struct AtInputButtonMap {
    int m_physical;                 // +0x00
    int m_abstract;                 // +0x04 bit 16 AxisNegative; 0x60000 if a POV state was named
    int m_state;                    // +0x08 1 by default, 2 AxisPositive, 3 AxisNegative, 0x1000 up to 0x1007 POVUp round to POVUpLeft
};

struct AtInputAxisMap {
    int m_physical;                 // +0x00
    int m_abstract;                 // +0x04 bit 16 when a POV axis word gave AxisY
};

struct AtInputOwner;

// Vftable 0x00277BC0, one slot.
class AtInputMapper {
public:
    struct Sizes {
        void* m_device;
        unsigned int m_buttonMaps;  // doubled on the way in
        unsigned int m_axisMaps;
    };

    AtInputMapper();                // 0x00072B90
    virtual ~AtInputMapper();       // slot 0; 0x00073790 deleting, which takes an array when asked; 0x000730B0 complete

    void Clear();                   // 0x00072BC0 nothing unless m_device is set, however much is held
    void ReadButtonMaps(AtIoTokenizer* tokenizer);  // 0x00072C10 a capacity of 0 reads nothing, not even the closing brace
    void ReadAxisMaps(AtIoTokenizer* tokenizer);    // 0x00072E80 the same
    void Allocate(const Sizes& sizes);  // 0x00073100 both lists cleared, none used
    void CopyFrom(const AtInputMapper& from);  // 0x00073220 takes the other's doubled capacity and doubles it again
    void Read(AtInputOwner* owner, AtIoTokenizer* tokenizer);  // 0x000732A0
    void Bind(AtInputController* controller, AtAbstractController* target);  // 0x00073010 from each map to the controller, or to all if null; out to the target's queue

    static void SetHeap(void* heap);  // 0x00072B70

    void* m_device;                 // +0x04 the owner's
    AtInputRouter m_router;         // +0x08
    AtInputButtonMap* m_buttonMaps; // +0x50
    AtInputAxisMap* m_axisMaps;     // +0x54
    unsigned int m_buttonMapCapacity;  // +0x58
    unsigned int m_axisMapCapacity;  // +0x5C
    unsigned int m_buttonMapsUsed;  // +0x60
    unsigned int m_axisMapsUsed;    // +0x64

    static void* s_heap;            // 0x002CF14C
};

// The holder of the input databases. Only these tables are known; its constructor and the rest are not recovered, and these members' own constructors stand in. It is the class of 0x0006CEE0.
struct AtInputOwner {
    void AddMappers(GmNameMap* map, const char* ns);  // 0x00073BC0 a null ns means the plain group; an empty one is a name space with an empty name
    void AddControllers(GmNameMap* map, const char* ns);  // 0x00073B90 the same

    int m_unknown00;                // +0x00
    void* m_device;                 // +0x04
    GmNameMapGroup m_controllers;   // +0x08
    GmNameMapGroup m_mappers;       // +0x20
    GmNameSpaceDb m_controllerSpaces;  // +0x38
    GmNameSpaceDb m_mapperSpaces;   // +0x5C
};

// Vftable 0x00277D4C. What holds a file's mappers by name.
class AtInputMapperDatabase : public GmNameMap {
public:
    AtInputMapperDatabase();        // 0x000737F0
    ~AtInputMapperDatabase() override;  // slot 0; 0x00073B70 deleting, 0x00073A50 complete
    void Clear() override;          // slot 1, 0x00073A00 nothing unless a file was read

    void Read(AtInputOwner* owner, AtIoTokenizer* tokenizer);  // 0x00073810
    void Load(AtInputOwner* owner, const char* name, int unused);  // 0x00073AD0 name + ".ANM"

    static void SetHeap(void* heap);  // 0x00073780

    enum { kKeywordCount = 23, kInputCodeCount = 235 };
    static const AtIoKeyword s_keywords[kKeywordCount];    // 0x002B2878
    static const AtIoKeyword s_inputCodes[kInputCodeCount];  // 0x002B2C10 what can be named on either side of a map; its count is the word at 0x002B3368. 0x00078691 and 0x000C3CC2 use it too.

    AtInputMapper* m_items;         // +0x14
    AtInputOwner* m_owner;          // +0x18
    unsigned int m_unknown1C;       // +0x1C only ever cleared

    static void* s_heap;            // 0x002CF154
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtInputRouter, m_boundButtons) == 0x20, "AtInputRouter::m_boundButtons");
static_assert(offsetof(AtInputRouter, m_linkCount) == 0x40, "AtInputRouter::m_linkCount");
static_assert(sizeof(AtInputRouter) == 0x48, "AtInputRouter");
static_assert(sizeof(AtInputButtonMap) == 12, "AtInputButtonMap");
static_assert(sizeof(AtInputAxisMap) == 8, "AtInputAxisMap");
static_assert(offsetof(AtInputMapper, m_router) == 0x08, "AtInputMapper::m_router");
static_assert(offsetof(AtInputMapper, m_buttonMaps) == 0x50, "AtInputMapper::m_buttonMaps");
static_assert(sizeof(AtInputMapper) == 0x68, "AtInputMapper");
static_assert(offsetof(AtInputOwner, m_mappers) == 0x20, "AtInputOwner::m_mappers");
static_assert(offsetof(AtInputOwner, m_mapperSpaces) == 0x5C, "AtInputOwner::m_mapperSpaces");
static_assert(sizeof(AtInputMapperDatabase) == 0x20, "AtInputMapperDatabase");
#pragma pop_macro("offsetof")
#endif
