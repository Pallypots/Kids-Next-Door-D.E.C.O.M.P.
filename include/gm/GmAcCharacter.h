// The next two classes of the game's actors towards the playable characters and the enemies: GmAcObject and GmAcCharacter, and what they hold. Original address clusters 0x0015B6C0-0x0015BA76,
// 0x00130920-0x00130F7E, 0x00130F80-0x0013127B. The record reader they and the actor base share is in src/gm/GmAcRecords.cpp, declared in include/gm/GmAcActor.h.
//
// Class names. GmAcObject and GmAcCharacter are RTTI names from the hierarchy descriptors of FaAcPlayer and FaAcNPC (the game library's own vftables have no locators), matched by depth: FaAcNPC's
// constructor 0x000928F0 calls GmAcNPC's 0x0014EA60, which calls 0x00130F80, the only place vftable 0x0028C980 is stored, which inlines 0x00130AB0, which stores 0x0028C890 after GmAcActor's
// constructor. Every other class, member and method name here is inferred, and says so by being descriptive; record words are the files' own.
//
// The design record. GmBaObject's m_unknown10 is the AtDataRegistry an object's record is in and m_descriptor is its AtDataLink: the reader hands both to the registry's search, the name at +0x08 and the
// "alias" at +0x0C are the link's m_name and m_parent. include/gm/GmAcActor.h keeps the older names; nothing here renames them.
#pragma once

#include "at/AtDataRegistry.h"
#include "gm/GmAcActor.h"

class AtCoordSys;
class GmAcCharacter;
class GmAcObject;

// ---- seams in classes declared elsewhere -----------------------------------------------------------------------------------
// 0x000679E0, AtDataRegistry's search by path from a link: each backslash part among the children, and, when `parents` is not 0, again from each parent of `under` in turn while nothing is found. Not
// recovered (the .ADR work recovered loading, not the lookups): stops with a diagnostic. A method of AtDataRegistry; a free function here so as not to change that header.
AtDataLink* AtDataRegistryFind(AtDataRegistry* registry, AtDataLink* under, const char* path, int parents);

// 0x003B3524: what the reader falls back on. Only the default record is known: its registry and link.
struct GmDesignDefaults {
    char m_unknown00[0x40];
    AtDataRegistry* m_registry;         // +0x40
    AtDataLink* m_link;                 // +0x44

    static GmDesignDefaults* s_instance;  // 0x003B3524
};

// The object at GmDynamicNodeState::s_instance (0x003B357C)'s +0x04, of which only the scene device is known; its +0x5F98. Read only by the node's slot 2, for material assignments.
struct GmSceneDeviceHolder {
    char m_unknown00[0x5F98];
    AtSceneDevice* m_device;            // +0x5F98
};

// ---- what GmAcObject holds -------------------------------------------------------------------------------------------------
// vftable 0x0028ED20, 0x1DC bytes: made when a record says AllowInteraction. Only what GmAcObject calls is declared; its constructor and slots 0 to 3 are not recovered and stop with a diagnostic.
class GmAcInteraction {
public:
    GmAcInteraction();                  // 0x001553C0 not recovered
    virtual ~GmAcInteraction();         // slot 0; 0x00155B50 deleting, not recovered
    virtual void Attach(GmAcObject* object);  // slot 1, 0x001645D0 not recovered; given the object that made it
    virtual void Slot2();               // slot 2, 0x00154780 not recovered
    virtual void Slot3();               // slot 3, 0x00154BC0 not recovered
    virtual void Slot4();               // slot 4, 0x001CC9F0 empty, folded
    virtual void Update(int a);         // slot 5, 0x000CBF30 empty, folded; GmAcObject's slot 20 calls it

    char m_unknown04[0x1D8];
};

// An actor's +0x270 (GmAcActor::m_unknown270) is an object whose +0x04 is a set of named stats. Only the search is known.
struct GmAcStat {
    void* m_unknown00;
    char m_name[8];                     // +0x04
    char m_unknown0C[0x10];
    float m_value;                      // +0x1C
};

struct GmAcStatSet {
    GmAcStat* Find(const char* name, int unused);  // 0x0014EFB0 the first with the name, compared as eight-byte names; the second argument is not read

    void* m_unknown00;
    unsigned int m_count;               // +0x04
    GmAcStat** m_stats;                 // +0x08 null entries are passed over
};

struct GmAcStatsHolder {
    void* m_vftable;
    GmAcStatSet* m_stats;               // +0x04
};

// ---- GmAcObject: vftable 0x0028C890, 60 slots, 0x2DC bytes -----------------------------------------------------------------
class GmAcObject : public GmAcActor {
public:
    enum { kObjectReset = 0x400 };      // in m_flags98

    GmAcObject();                       // 0x00130AB0, an out-of-line copy; inlined in GmAcCharacter's and 0x001915C0
    ~GmAcObject() override;             // slot 0; 0x0015BA20 deleting, for arrays too; 0x00130B00 complete
    int Slot2() override;               // slot 2, 0x0015B710 the actor's, then AllowInteraction; returns the actor's answer
    void Shutdown() override;           // slot 3, 0x0015B870 only when initialized: the interaction, then the actor's
    void Slot10() override;             // slot 10, 0x0015B6C0 shuts down if initialized, then the actor's slot 10
    void Slot20(int a) override;        // slot 20, 0x0015B6E0 the actor's, then the interaction's Update when slot 27 says so
    int Slot55() override;              // slot 55, 0x0015B830 0 when the stat sHealth is there and not above zero; 1 otherwise

    void Reset();                       // 0x0015B810 m_flags98 gains kObjectReset; no interaction

    // 0x0015B8C0. Registered five times in the static list of create functions. One from the heap below, or for more than one an array with a count ahead of it. -2 is raised for no heap, -4
    // for no memory, as the actor's factories do.
    static GmAcObject* Create(int count);

    GmAcInteraction* m_interaction;     // +0x2D8
};

// ---- what GmAcCharacter holds ----------------------------------------------------------------------------------------------
// +0x2E0: made by name, from the "Shadow" name map in GmAcActor::s_namedMaps, by the record's ShadowType. What each slot does is not known; these are the ones the character calls.
class GmAcShadow {
public:
    struct Setup {
        GmAcCharacter* m_owner;
    };
    // An entry of a name map of create functions: what the factory made is handed this back at +0x04.
    struct Registration {
        GmAcShadow* (*m_create)(int count);  // called with -1
    };

    virtual ~GmAcShadow() = 0;          // slot 0, through which the character deletes it
    virtual void Slot1() = 0;
    virtual void Start() = 0;           // slot 2, after Configure
    virtual void Stop() = 0;            // slot 3, before it is deleted
    virtual void Slot4() = 0;
    virtual void Slot5() = 0;
    virtual void Slot6() = 0;
    virtual void Configure(Setup* setup) = 0;  // slot 7
    virtual void Slot8() = 0;           // from the character's slot 15
    virtual void Slot9() = 0;
    virtual void Slot10() = 0;          // from the character's slot 33
    virtual void Slot11() = 0;          // from the character's slot 32
    virtual void Update(int a) = 0;     // slot 12, from the character's slot 20 when visible
    virtual void Slot13(int a) = 0;     // slot 13, from the character's slot 21 when visible

    Registration* m_registration;       // +0x04
};

// +0x2E4: nothing here makes it. Slot names say who calls them.
class GmAcSightCone {
public:
    virtual ~GmAcSightCone() = 0;       // slot 0, through which the character deletes it
    virtual void Slot1() = 0;
    virtual void Slot2() = 0;
    virtual void Slot3() = 0;
    virtual void Stop() = 0;            // slot 4, before it is deleted
    virtual void Slot5() = 0;           // from the character's slot 4
    virtual void Update(int a) = 0;     // slot 6, from the character's slot 20 when slot 27 says so
    virtual int Slot7() = 0;            // slot 7, answers the character's slot 55
};

// What the two above are is not known, so neither is what their destructors do: these bodies are only for host test classes built on them.
inline GmAcShadow::~GmAcShadow() {}
inline GmAcSightCone::~GmAcSightCone() {}

// +0x2DC: given by the player and NPC classes (0x00131F43, 0x00131F67) and a caller at 0x0009A545. Its +0x08 is told whose it is; its +0x04 is what the character's slot 59 answers. 0x0007DF00 and
// 0x00154530, the original's two calls, are one-store and one-load bodies folded with other classes' (AtInputAxisHandler, AtAbstractController); they are written in place here.
struct GmAcCharController {
    void* m_vftable;
    int m_unknown04;                    // +0x04
    GmAcActor* m_owner;                 // +0x08
};

// ---- GmAcCharacter: vftable 0x0028C980, 60 slots, 0x304 bytes ---------------------------------------------------------------
// 0x304: GmAcNPC's first own word is +0x304 and the character's constructor writes up to +0x300.
class GmAcCharacter : public GmAcObject {
public:
    enum { kCharacterReset = 0x2000 };  // in m_flags98

    GmAcCharacter();                    // 0x00130F80
    ~GmAcCharacter() override;          // slot 0; 0x00131260 deleting; 0x00131210 complete
    int Slot2() override;               // slot 2, 0x00131020 the sight cone's words and the shadow, then GmAcObject's, whose answer it returns
    void Shutdown() override;           // slot 3, 0x00131150 GmAcObject's first, then the sight cone and the shadow
    void Slot4() override;              // slot 4, 0x00130930 the sight cone's slot 5, then the actor's slot 4
    void Slot10() override;             // slot 10, 0x00130920 GmAcObject's
    void Slot15() override;             // slot 15, 0x00130950 the shadow's slot 8, then the actor's slot 15
    void Slot20(int a) override;        // slot 20, 0x00130970
    void Slot21(int a) override;        // slot 21, 0x001309D0 the shadow's slot 13 when visible; visibility is asked first
    void Slot32() override;             // slot 32, 0x00130A10 the shadow's slot 11, then the node's
    void Slot33() override;             // slot 33, 0x00130A30 the shadow's slot 10, then the node's
    void Slot49(const char* event, int b, int c) override;  // slot 49, 0x00130A00 the actor's
    int Slot55() override;              // slot 55, 0x00130A50 the sight cone's slot 7, or GmAcObject's without one
    int Slot59() override;              // slot 59, 0x00130A90 the controller's +0x04, or 0

    void Reset();                       // inlined in the constructor and the shutdown: m_flags98 gains kCharacterReset; every own word 0
    void SetController(GmAcCharController* controller, GmAcActor* owner);  // 0x00130F50 a null owner is this character when m_flags98 has 0x100
    void PostEvent();                   // 0x00130A70 slot 49 with m_event, 0, 1

    GmAcCharController* m_controller;   // +0x2DC
    GmAcShadow* m_shadow;               // +0x2E0
    GmAcSightCone* m_sightCone;         // +0x2E4
    float* m_sightConeLength;           // +0x2E8 the record's values, where they are: SightConeLength
    float* m_sightConeYaw;              // +0x2EC SightConeYaw
    float* m_sightConePitch;            // +0x2F0 SightConePitch
    float* m_sightConeMinDistance;      // +0x2F4 SightConeMinDistance
    AtCoordSys* m_sightConeBone;        // +0x2F8 the joint SightConeAttachToBone names, in the scene actor at +0xA0
    char m_event[8];                    // +0x2FC
};

// ---- GmDynamicNodeLoader's list of characters with shadows ------------------------------------------------------------------
// Its +0xF0, 256 slots. These two are in the character's address range, the only callers its slots 2 and 3. Declared here as free functions so as not to change include/gm/GmLevelManifest.h; they
// stand for members of GmDynamicNodeLoader (0x003B3984).
struct GmShadowCasters {
    char m_unknown00[0xF0];
    GmAcCharacter* m_casters[256];      // +0xF0 packed from the front
};
void GmShadowCastersAdd(GmShadowCasters* list, GmAcCharacter* character);     // 0x00130B50 into the first slot free or already its; nothing when full
void GmShadowCastersRemove(GmShadowCasters* list, GmAcCharacter* character);  // 0x00130B90 those after it move down one, and the last is cleared

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(GmDesignDefaults, m_link) == 0x44, "GmDesignDefaults::m_link");
static_assert(offsetof(GmSceneDeviceHolder, m_device) == 0x5F98, "GmSceneDeviceHolder::m_device");
static_assert(sizeof(GmAcInteraction) == 0x1DC, "GmAcInteraction");
static_assert(offsetof(GmAcStat, m_value) == 0x1C, "GmAcStat::m_value");
static_assert(offsetof(GmAcStatSet, m_stats) == 0x08, "GmAcStatSet::m_stats");
static_assert(offsetof(GmAcObject, m_interaction) == 0x2D8, "GmAcObject::m_interaction");
static_assert(sizeof(GmAcObject) == 0x2DC, "GmAcObject");
static_assert(offsetof(GmAcShadow, m_registration) == 0x04, "GmAcShadow::m_registration");
static_assert(offsetof(GmAcCharController, m_owner) == 0x08, "GmAcCharController::m_owner");
static_assert(offsetof(GmAcCharacter, m_controller) == 0x2DC, "GmAcCharacter::m_controller");
static_assert(offsetof(GmAcCharacter, m_sightConeLength) == 0x2E8, "GmAcCharacter::m_sightConeLength");
static_assert(offsetof(GmAcCharacter, m_sightConeBone) == 0x2F8, "GmAcCharacter::m_sightConeBone");
static_assert(offsetof(GmAcCharacter, m_event) == 0x2FC, "GmAcCharacter::m_event");
static_assert(sizeof(GmAcCharacter) == 0x304, "GmAcCharacter");
static_assert(offsetof(GmShadowCasters, m_casters) == 0xF0, "GmShadowCasters::m_casters");
#pragma pop_macro("offsetof")
#endif
