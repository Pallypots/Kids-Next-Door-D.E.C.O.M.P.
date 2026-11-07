// Controllers: what a game's input arrives on, read from .ANA files, "AbstractController" by the file's own word. Original address clusters 0x00072700-0x00072AAF, 0x000733B0-0x000737EF, 0x00077100-0x000783FF, 0x00078AD0, 0x000795B0-0x00079E0F,
// 0x0007DBC0-0x0007DF69, 0x0007E100. The keyword names are the original's, from its table at 0x002B2828; other names are inferred.
#pragma once

#include "at/AtIoTokenizer.h"
#include "at/AtText.h"
#include "gm/GmNameMap.h"

class AtInputController;
class AtAbstractController;
struct AtInputOwner;

// What input is passed along to: the five slots a router and a queue have in common. No vftable of its own is known; it is what lets one chain to the other.
class AtInputReceiver {
public:
    virtual void Slot0(void* a) = 0;       // slot 0 a button's event
    virtual void Slot1(void* a) = 0;       // slot 1 another of a button's
    virtual void Slot2(void* a) = 0;       // slot 2 an axis's
    virtual void Slot3(void* a, void* b) = 0;  // slot 3
    virtual void Unbind(AtInputController* controller, int id) = 0;  // slot 4 the controller is losing that input
};

// One binding from a physical input to a controller, 0x1C bytes, in a chain of the controller's while bound and on its router's free list otherwise.
struct AtInputLink {
    AtInputLink* m_next;            // +0x00
    AtInputLink* m_prev;            // +0x04
    int m_id;                       // +0x08 the physical id, as the controller knows it
    int m_state;                    // +0x0C a button map's state; not set for an axis
    int m_abstract;                 // +0x10 the id it comes out as
    AtInputController* m_controller;  // +0x14
    AtInputReceiver* m_owner;       // +0x18 the router that holds it
};

// Vftable 0x002785F8. Room for a fixed number of pointers, in order. A router's hold links; a controller's hold receivers, cast.
class AtInputLinkList {
public:
    AtInputLinkList();              // 0x000789F0
    virtual ~AtInputLinkList();     // slot 0; 0x00078B00 deleting, 0x00078A10 complete

    void Clear();                   // 0x00078950
    void Allocate(unsigned int capacity);  // 0x00078A40 what it had is freed; cleared; s_heap
    int Remove(AtInputLink* link);  // 0x00078980 1 if it was there. Reads the first entry whatever the count, so with no room at all it crashes.
    int Add(AtInputLink* link);     // 0x00078AD0 0 when full

    static void SetHeap(void* heap);  // 0x00078940

    AtInputLink** m_items;          // +0x04
    unsigned int m_capacity;        // +0x08
    unsigned int m_count;           // +0x0C

    static void* s_heap;            // 0x00301B00
};

// Vftable 0x002795FC, 0x40 bytes. What an axis's raw value goes through.
class AtInputAxisHandler {
public:
    // What Setup copies from, nine words. Its dead zone and saturation are percentages.
    struct Setting {
        int m_mode;                 // +0x00 1 is a straight copy
        int m_raw;                  // +0x04 nonzero: nothing is filtered at all
        int m_id;                   // +0x08 the input's
        int m_index;                // +0x0C the abstract index
        float m_minimum;            // +0x10
        float m_maximum;            // +0x14
        float m_center;             // +0x18
        float m_deadZone;           // +0x1C
        float m_saturation;         // +0x20
    };

    AtInputAxisHandler();           // 0x0007DC00
    virtual ~AtInputAxisHandler();  // slot 0; 0x00079440 deleting, which takes an array when asked; 0x0007DF20 complete
    virtual void Reset();           // slot 1, 0x0007DD10 nothing unless it was set up
    virtual void SetRaw(int raw);   // slot 2, 0x0007DF00
    virtual void SetMode(int mode);  // slot 3, 0x000DF720 one store, folded with others
    virtual float SetMinimum(float value);  // slot 4, 0x0007DF10 each of these gives back what it was given
    virtual float SetMaximum(float value);  // slot 5, 0x0007DBC0
    virtual float SetCenter(float value);   // slot 6, 0x0007DBD0
    virtual float SetDeadZone(float value);  // slot 7, 0x0007DBE0
    virtual float SetSaturation(float value);  // slot 8, 0x0007DBF0

    void Setup(Setting* setting);   // 0x0007DC50 a dead zone above the saturation makes the saturation 95, here and in the caller's copy
    float Filter(float value) const;  // 0x0007DD60
    void Apply(float value);        // 0x0007DEC0

    int m_mode;                     // +0x04
    int m_raw;                      // +0x08
    int m_id;                       // +0x0C
    int m_index;                    // +0x10
    float m_minimum;                // +0x14
    float m_maximum;                // +0x18
    float m_center;                 // +0x1C
    float m_deadZone;               // +0x20
    float m_saturation;             // +0x24
    float m_value;                  // +0x28
    float m_previousValue;          // +0x2C
    float m_filtered;               // +0x30
    float m_previousFiltered;       // +0x34
    unsigned int m_flags;           // +0x38 bit 0 negate, bit 1 set up
    int m_isSet;                    // +0x3C
};

// Vftable 0x00277BA8, 0x28 bytes. The events a controller has had and nobody has read, in a pool. Passes everything on.
class AtInputEventQueue : public AtInputReceiver {
public:
    struct Node {
        Node* m_next;               // +0x00
        Node* m_prev;               // +0x04
        int* m_event;               // +0x08 seven words; the second says whose
    };

    AtInputEventQueue();            // 0x00072730
    virtual ~AtInputEventQueue();   // slot 5; 0x000729D0 deleting, 0x00072760 complete. Leaves the lists' pointers.

    void Slot0(void* event) override;  // slot 0, 0x00072940 kept if mask bit 0
    void Slot1(void* event) override;  // slot 1, 0x00072970 kept if mask bit 0
    void Slot2(void* event) override;  // slot 2, 0x000729A0 kept if mask bit 1
    void Slot3(void* a, void* b) override;  // slot 3, 0x000726F0 passed on
    void Unbind(AtInputController* controller, int id) override;  // slot 4, 0x00072710 passed on

    void Allocate(unsigned int count, unsigned int mask);  // 0x000727A0 keeps the lists' pointers, so a second, after a controller's Release, writes one word into what it freed for each list's end
    int Push(const void* event);    // 0x000728D0 0 if there is no room
    void Flush(unsigned int mask);  // 0x00072A30 what the mask asks for, from the oldest; a bit 1 event if its second word is 1, else bit 0

    static void SetHeap(void* heap);  // 0x000726E0

    AtInputReceiver* m_next;        // +0x04
    Node* m_first;                  // +0x08
    Node* m_last;                   // +0x0C
    Node* m_freeFirst;              // +0x10
    Node* m_freeLast;               // +0x14
    int* m_events;                  // +0x18
    Node* m_nodes;                  // +0x1C
    unsigned int m_mask;            // +0x20
    int m_count;                    // +0x24

    static void* s_heap;            // 0x002CF148
};

// What .ANA gives, and what a caller may fill in. The first five words are all the reader sets.
struct AtInputControllerSetting {
    int m_unknown00;                // +0x00
    int m_unknown04;                // +0x04
    unsigned int m_eventHandlers;   // +0x08
    float m_deadZone;               // +0x0C
    float m_saturation;             // +0x10
    unsigned int m_buttons;         // +0x14 only Init reads these three
    unsigned int m_axes;            // +0x18
    unsigned int m_povs;            // +0x1C
};

// Vftable 0x00278580, 0xD4 bytes. The inputs of something, and for each a chain of the links bound to it. Other classes derive from it through a virtual base: see AtInputPhysicalController.
class AtInputController {
public:
    struct Input {
        int m_id;
        int m_unknown04;
    };
    struct Chain {
        AtInputLink* m_first;
        AtInputLink* m_last;
    };

    AtInputController();            // 0x000773B0

    virtual int Slot0() = 0;        // slot 0
    virtual int CopyStates() = 0;   // slot 1
    virtual int Update() = 0;       // slot 2
    virtual int AllocateInputs();   // slot 3, 0x000773E0 always 0; a failed request raises, and then what follows crashes
    virtual void Acquire();         // slot 4, 0x000795B0
    virtual void Unacquire();       // slot 5, 0x000795C0
    virtual ~AtInputController();   // slot 6; 0x00078380 deleting, 0x00077EF0 complete
    virtual int Release();          // slot 7, 0x00077B90 tells everyone of each input; nothing unless started; always 0
    virtual int Tick(int milliseconds);  // slot 8, 0x000783A0 not recovered: stops with a diagnostic
    virtual void InputName(int id, AtText* out);  // slot 9, 0x000772E0 not recovered: stops with a diagnostic
    virtual unsigned int ButtonCount() = 0;  // slot 10
    virtual unsigned int AxisCount() = 0;    // slot 11
    virtual unsigned int PovCount() = 0;     // slot 12
    virtual AtInputAxisHandler* AxisHandler(unsigned int index) = 0;  // slot 13
    virtual int SetUnknown1C(int value);  // slot 14, 0x000E9920 folded
    virtual void Slot15(int a, int b, int c);  // slot 15, 0x001ACD70 empty, folded
    virtual void Slot16(int a);     // slot 16, 0x000CBF30 empty, folded

    int UnbindButton(AtInputLink* link);  // 0x00077870 1 if the link's id is among the first inputs
    int UnbindAxis(AtInputLink* link);    // 0x000779A0 the same, among the second
    int BindButton(AtInputLink* link);    // 0x000777F0 onto the front of the chain of the link's id; 0 if it has no such input
    int BindAxis(AtInputLink* link);      // 0x00077920 the same, among the second
    void ResetFields();             // 0x00077110
    int Start(const AtInputControllerSetting& setting);  // 0x000771D0
    void TellOfLoss(int id, Chain* chain);  // 0x00077B40 each receiver, then each link's router. A router unbinds the link, which is what ends the loop.
    void SetValue(unsigned int index, float value);  // 0x00077AA0 an axis through its handler, a POV kept with the one before; past both, nothing

    static void SetHeap(void* heap);  // 0x00077100

    unsigned int m_buttons;         // +0x04
    unsigned int m_axes;            // +0x08
    unsigned int m_povs;            // +0x0C
    int m_unknown10;                // +0x10
    int m_unknown14;                // +0x14 1 here, and an abstract controller makes it 5
    int m_unknown18;                // +0x18 not touched
    int m_unknown1C;                // +0x1C
    int m_unknown20;                // +0x20
    AtTextTable* m_names;           // +0x24
    AtTextUnicode m_text;           // +0x28
    AtInputLinkList m_receivers;    // +0x3C
    Input* m_inputs;                // +0x4C the block the next three point into
    Input* m_buttonInputs;          // +0x50 every input: buttons, then axes, then POVs
    Input* m_axisInputs;            // +0x54 axes, then POVs
    Input* m_povInputs;             // +0x58
    int m_unknown5C[8];             // +0x5C
    int* m_states;                  // +0x7C
    int* m_previousStates;          // +0x80
    int* m_unknown84;               // +0x84
    int m_unknown88[4];             // +0x88
    Chain* m_buttonChains;          // +0x98 one for each input
    Chain* m_axisChains;            // +0x9C one for each axis and POV
    int* m_povDirections;           // +0xA0 8 for none
    float* m_povValues;             // +0xA4
    float* m_previousPovValues;     // +0xA8
    unsigned int m_buttonInputCount;  // +0xAC every input
    unsigned int m_axisInputCount;  // +0xB0 axes and POVs
    int m_unknownB4;                // +0xB4
    int m_unknownB8;                // +0xB8
    int m_unknownBC;                // +0xBC
    int m_started;                  // +0xC0
    int m_acquired;                 // +0xC4
    int m_unknownC8;                // +0xC8
    int m_unknownCC;                // +0xCC
    int m_unknownD0;                // +0xD0 1

    static void* s_heap;            // 0x00301AF0
};

// Vftable 0x002786C0, 0x120 bytes. A controller nothing physical stands behind: what mappers map onto, and what the game reads.
class AtAbstractController : public AtInputController {
public:
    AtAbstractController();         // 0x00079650
    ~AtAbstractController() override;  // slot 6; 0x00073570 deleting, which takes an array when asked; 0x00079DB0 complete

    int Slot0() override;           // slot 0, 0x000AB720 returns 0, folded
    int CopyStates() override;      // slot 1, 0x00079AE0 5 unless acquired
    int Update() override;          // slot 2, 0x00079B40 not recovered: stops with a diagnostic
    int AllocateInputs() override;  // slot 3, 0x00079990 what the base's returns if not 0. Handlers, then the ids, 0x30000000, 0x40000000 and 0x50000000 up. Every POV starts at two pi.
    int Release() override;         // slot 7, 0x00079940
    void InputName(int id, AtText* out) override;  // slot 9, 0x000795D0 not recovered: stops with a diagnostic
    unsigned int ButtonCount() override;  // slot 10, 0x00154530 folded
    unsigned int AxisCount() override;    // slot 11, 0x00154540 folded
    unsigned int PovCount() override;     // slot 12, 0x00079DA0
    AtInputAxisHandler* AxisHandler(unsigned int index) override;  // slot 13, 0x00079D80 null past the end
    int SetUnknown1C(int value) override;  // slot 14, 0x000B15B0 returns 0, folded
    virtual void Read(void* unused, AtIoTokenizer* tokenizer);  // slot 17, 0x00079830
    virtual int Init(const AtInputControllerSetting& setting);  // slot 18, 0x000797B0 releases first; what that returns if not 0

    int StartFrom(const AtInputControllerSetting& setting);  // 0x000796B0
    void ClearHandlers();           // 0x000793B0 also writes the eight POV angles, each time

    AtInputAxisHandler* m_handlers;  // +0xD4
    AtInputEventQueue m_queue;      // +0xD8
    int m_unknown100[4];            // +0x100
    int m_unknown110[4];            // +0x110

    enum { kPovDirections = 8 };
    static float s_povAngles[kPovDirections];  // 0x00301B08 up, then round by quarters of pi
};

// A controller something physical stands behind: its controller is a virtual base, with this class's vbptr at +4. Only the shape is known here, which is what binding depends on; no such class is recovered.
class AtInputPhysicalController : public virtual AtInputController {
public:
    virtual void PhysicalSlot0() = 0;
};

// What owns the physical controllers. Only the slots binding calls are known; no such class is recovered. Used only by AtInputRouter's binding.
class AtInputDevice {
public:
    virtual void Slot0() = 0;
    virtual void Slot1() = 0;
    virtual void Slot2() = 0;
    virtual void Slot3() = 0;
    virtual void Slot4() = 0;
    virtual void Slot5() = 0;
    virtual void Slot6() = 0;
    virtual void Slot7() = 0;
    virtual void Slot8() = 0;
    virtual void Slot9() = 0;
    virtual void Slot10() = 0;
    virtual void Slot11() = 0;
    virtual void Slot12() = 0;
    virtual void Slot13() = 0;
    virtual void Slot14() = 0;
    virtual void Slot15() = 0;
    virtual void Slot16() = 0;
    virtual void Slot17() = 0;
    virtual unsigned int PhysicalCount() = 0;  // slot 18
    virtual unsigned int ConnectedCount() = 0;  // slot 19
    virtual AtInputPhysicalController* First() = 0;  // slot 20
    virtual AtInputPhysicalController* Second() = 0;  // slot 21
    virtual AtInputPhysicalController* Numbered(unsigned int index) = 0;  // slot 22
    virtual AtInputPhysicalController* Physical(unsigned int index) = 0;  // slot 23

    AtInputPhysicalController* Connected(unsigned int index);  // 0x0007E100 the index-th that is started and acquired; else null

    int m_unknown04[5];             // +0x04
    unsigned int m_numberedCount;   // +0x18
};

// Vftable 0x00277C18. What holds a file's controllers by name.
class AtInputControllerDatabase : public GmNameMap {
public:
    AtInputControllerDatabase();    // 0x000735D0
    ~AtInputControllerDatabase() override;  // slot 0; 0x00073760 deleting, 0x000736E0 complete
    void Clear() override;          // slot 1, 0x00073690 nothing unless a file was read

    void Read(AtInputOwner* owner, AtIoTokenizer* tokenizer);  // 0x000733C0
    void Load(AtInputOwner* owner, const char* name, int unused);  // 0x000735F0 name + ".ANA"

    static void SetHeap(void* heap);  // 0x000733B0

    enum { kKeywordCount = 8 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B2828

    AtAbstractController* m_items;  // +0x14
    void* m_unknown18;              // +0x18 given to each controller's Read, which ignores it. Nothing sets it.
    unsigned int m_unknown1C;       // +0x1C only ever cleared

    static void* s_heap;            // 0x002CF150
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtInputLink) == 0x1C, "AtInputLink");
static_assert(sizeof(AtInputLinkList) == 0x10, "AtInputLinkList");
static_assert(sizeof(AtInputAxisHandler) == 0x40, "AtInputAxisHandler");
static_assert(sizeof(AtInputAxisHandler::Setting) == 0x24, "AtInputAxisHandler::Setting");
static_assert(sizeof(AtInputEventQueue) == 0x28, "AtInputEventQueue");
static_assert(offsetof(AtInputController, m_text) == 0x28, "AtInputController::m_text");
static_assert(offsetof(AtInputController, m_receivers) == 0x3C, "AtInputController::m_receivers");
static_assert(offsetof(AtInputController, m_buttonInputs) == 0x50, "AtInputController::m_buttonInputs");
static_assert(offsetof(AtInputController, m_states) == 0x7C, "AtInputController::m_states");
static_assert(offsetof(AtInputController, m_buttonChains) == 0x98, "AtInputController::m_buttonChains");
static_assert(offsetof(AtInputController, m_axisInputCount) == 0xB0, "AtInputController::m_axisInputCount");
static_assert(offsetof(AtInputController, m_started) == 0xC0, "AtInputController::m_started");
static_assert(sizeof(AtInputController) == 0xD4, "AtInputController");
static_assert(offsetof(AtAbstractController, m_queue) == 0xD8, "AtAbstractController::m_queue");
static_assert(sizeof(AtAbstractController) == 0x120, "AtAbstractController");
static_assert(sizeof(AtInputControllerSetting) == 0x20, "AtInputControllerSetting");
static_assert(offsetof(AtInputDevice, m_numberedCount) == 0x18, "AtInputDevice::m_numberedCount");
static_assert(sizeof(AtInputControllerDatabase) == 0x20, "AtInputControllerDatabase");
#pragma pop_macro("offsetof")
#endif
