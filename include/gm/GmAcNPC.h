// The enemies and the game's other AI characters: GmAcNPC and FaAcNPC, and what they alone need. Original address clusters 0x0014E950-0x0014ED26 (GmAcNPC), 0x00091F50-0x00092C3D (FaAcNPC and
// the helpers its object file holds), 0x0014CB00-0x0014CB42 (a reader of GmAcNode's), 0x001524C0-0x00152514 (the health range test).
//
// Class names. GmAcNPC, FaAcNPC, FaCmCombat and GmCmCombat are RTTI names: FaAcNPC's hierarchy descriptor (vftable 0x0027B41C) lists FaAcNPC, GmAcNPC, GmAcCharacter, GmAcObject, GmAcActor,
// GmAcNode, GmBaObject, AtEvSubscriber, and FaAcNPC's constructor 0x000928F0 calls 0x0014EA60, which calls GmAcCharacter's 0x00130F80 and stores vftable 0x0028E9A8 (no locator: the game library
// has no RTTI of its own). FaCmCombat's vftable 0x00282EF4 has a locator naming FaCmCombat and GmCmCombat. Every other class, member and method name here is inferred, and says so by being
// descriptive; record words and condition names are the files' and the executable's own.
//
// Finding about the base: GmAcCharacter's +0x2E4, called m_sightCone in include/gm/GmAcCharacter.h, is the character's combat object. FaAcNPC's slot 15 makes a FaCmCombat there (0xE4 bytes,
// constructor 0x000A3360, vftable 0x00282EF4) when the record says Combat; FaAcNPC's reset clears it; its slot 60 reads FaCmCombat's words through it. GmAcCharacter's uses (slots 4 to 7, delete
// through slot 0) are consistent with that. Nothing here renames it: the combat object is reached by a cast, CombatOf below.
#pragma once

#include "at/AtDataRegistry.h"
#include "gm/GmAcCharacter.h"
#include "gm/GmAiActionManager.h"

class FaAcNPC;
class GmAcNPC;

// ---- seams in classes declared elsewhere ---------------------------------------------------------------------------------------
// 0x00072330: formats into the next of seven 0x400-byte buffers (0x002CD540, index at 0x002CF140) and points `out` at it; with a null format, `out` is null. A text routine of the engine whose
// owner is not established; not recovered, stops with a diagnostic. Its result's destructor, 0x0012FA00, only clears the pointer, and is written in place.
struct AtTempText {
    ~AtTempText() { m_text = 0; }
    char* m_text;
};
AtTempText* AtFormatTemp(AtTempText* out, const char* format, ...);

// The event FaAcNPC's health levels post: vftable 0x0028EBCC, 0x28 bytes, built on the stack and never destroyed (the original calls no destructor). Not recovered: its constructor and the one
// method used stop with a diagnostic.
struct GmGameEvent {
    GmGameEvent();                                      // 0x00152590
    void Set(GmAcActor* actor, int b, int c, int d);    // 0x00152780 FaAcNPC passes itself, 0, 0, -1

    char m_unknown00[0x28];
};

// 0x001512B0, a method of GmEventManager (0x003B3990): the event by name in a namespace, with the event object. A free function so as not to change include/gm/GmAcActor.h. Not recovered.
void GmEventManagerSend(GmEventManager* manager, const char* event, const char* nameSpace, GmGameEvent* object, int d);

// 0x00130E50, a method of GmAcCharacter: whether it sees a target, 10 arguments (ret 0x28), 0 for a null target; reaches the scene node of both and 0x00130D20. A free function so as not to
// change include/gm/GmAcCharacter.h. Not recovered.
int GmAcCharacterCanSee(GmAcCharacter* character, GmAcActor* target, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10);

// ---- what GmAcNPC makes: the AI action manager, include/gm/GmAiActionManager.h ------------------------------------------------

// ---- what FaAcNPC makes: the combat object ------------------------------------------------------------------------------------------
// FaCmCombat (RTTI), 0xE4 bytes, on GmCmCombat (RTTI; vftable 0x0028F5C4, constructor 0x001645F0, which sets +0x04 to 0 and +0x08 to 1). Its constructor resets every word (0x000A2210). Only the
// words FaAcNPC reads are named. Not recovered: its constructor and slots stop with a diagnostic, but slot 3, an empty body folded with others, which is written.
class FaCmCombat {
public:
    FaCmCombat();                       // 0x000A3360
    virtual ~FaCmCombat();              // slot 0; 0x000A46D0 deleting, not recovered
    virtual void Attach(GmAcActor* owner);  // slot 1, 0x000A33B0 not recovered; given the NPC that made it
    virtual void Slot2();               // slot 2, 0x000A1F10 not recovered
    virtual void Start();               // slot 3, 0x001CC9F0 empty, folded; called right after Attach
    virtual void Stop();                // slot 4, 0x000A3430 not recovered; GmAcCharacter's shutdown calls it
    virtual void Slot5();               // slot 5, 0x000A2310 not recovered
    virtual void Update(int a);         // slot 6, 0x000A4BB0 not recovered
    virtual int Slot7();                // slot 7, 0x000A3380 not recovered

    char m_unknown04[0x7C];
    GmAcStat* m_health;                 // +0x80 the owner's health stat: its +0x0C and +0x10 are the bounds, +0x1C the value
    char m_unknown84[0x10];
    int m_threat;                       // +0x94 "cThreat" when not 0
    int m_attackRange;                  // +0x98 -1 when out of every attack range, else which; "cAtkRg<n>"
    char m_unknown9C[0x48];
};

// The combat object at a character's +0x2E4. See the finding above.
inline FaCmCombat* CombatOf(GmAcCharacter* character) { return (FaCmCombat*)character->m_sightCone; }

// 0x00013E00, an engine text routine with no owner yet: a bounded comparison, a null text being the empty text; the difference of the first two characters that differ, else 0. Recovered here,
// in src/gm/FaAcNPC.cpp, for its one caller in this work; it belongs with the text routines (AtNamesEqual 0x00013FC0 is beside it) and should move there.
int AtCompareN(const char* a, const char* b, unsigned int count);

// ---- helpers in FaAcNPC's object file, called by many classes ----------------------------------------------------------------------
// 0x000920D0. The actor's slot 53 word as an actor, when it is one (m_flags98 has kActorReset); else null. Called on players and other classes too, so it stands for an inline member of GmAcActor
// emitted out of line here; a free function so as not to change include/gm/GmAcActor.h. Quirk kept: slot 53 is asked three times.
GmAcActor* GmAcTargetOf(GmAcActor* actor);

// 0x00092C20. 1 when the actor's float at +0x154 is not zero (a NaN is not zero). Called on the player by "cTgtAir" and from 0x000AD377. A member of GmAcActor by its use; free here.
int GmAcFloat154NotZero(GmAcActor* actor);

// 0x00092110, a method of GmDynamicNodeLoader (0x003B3984): the n-th of its characters with a controller (+0x2DC), counting from 0, or null. Free, on this struct, so as not to change
// include/gm/GmLevelManifest.h. Quirk kept: the count is the word at +0x4C, which is also where the loop reads the second character, so the list can only work with one; the original reads it so.
struct GmControlledCharacters {
    char m_unknown00[0x48];
    GmAcCharacter* m_first;             // +0x48 the characters, from here
    unsigned int m_count;               // +0x4C
};
GmAcCharacter* GmNthControlledCharacter(GmControlledCharacters* list, unsigned int n);

// 0x001524C0. Whether a stat's value is in a health range: the range value's descriptor's real bounds are fractions of the stat's span. 1 when min + span * floor < value <= min + span * ceiling;
// 0 for a null stat or range, and when a comparison is unordered.
int GmStatInRange(GmAcStat* stat, AtDataComponent* range);

// ---- GmAcNPC: vftable 0x0028E9A8, 61 slots, 0x308 bytes -------------------------------------------------------------------------------
class GmAcNPC : public GmAcCharacter {
public:
    enum { kNPCReset = 0x4000 };        // in m_flags98

    GmAcNPC();                          // 0x0014EA60
    ~GmAcNPC() override;                // slot 0; 0x0014ECD0 deleting, for arrays too; 0x0014EAE0 complete
    int Slot2() override;               // slot 2, 0x0014E960 GmAcCharacter's
    void Shutdown() override;           // slot 3, 0x0014EA90 the action manager, then GmAcCharacter's; whether or not initialized
    void Slot4() override;              // slot 4, 0x001318F0 GmAcCharacter's; one body with GmAcPlayer's slot 4, folded
    void Slot10() override;             // slot 10, 0x0014E950 GmAcCharacter's
    void Slot15() override;             // slot 15, 0x0014E970 the action manager when the record says CreateAiActionManager, then GmAcCharacter's
    void Slot20(int a) override;        // slot 20, 0x0014EA40 GmAcCharacter's
    void Slot49(const char* event, int b, int c) override;  // slot 49, 0x0014EA50 GmAcCharacter's
    virtual int TestCondition(const char* name);  // slot 60, 0x000B15B0 returns 0, folded. FaAcNPC answers the AI's condition names ("cDead" ...)

    void Reset();                       // inlined in the constructor, the shutdown and the factory: m_flags98 gains kNPCReset; no action manager

    // 0x0014EB70. Registered in the static list of create functions (0x002015CE). One from the heap below, or for more than one an array with a count ahead of it. -2 is raised for no heap,
    // -4 for no memory.
    static GmAcNPC* Create(int count);

    GmAiActionManager* m_aiActions;     // +0x304
};

// ---- FaAcNPC: vftable 0x0027B41C, 61 slots, 0x320 bytes --------------------------------------------------------------------------------
class FaAcNPC : public GmAcNPC {
public:
    enum { kFaNPCReset = 0x20000 };     // in m_flags98

    FaAcNPC();                          // 0x000928F0
    ~FaAcNPC() override;                // slot 0; 0x00092B90 deleting, for arrays too; 0x000929C0 complete
    int Slot2() override;               // slot 2, 0x00091F70 GmAcNPC's, then the health ranges and levels; GmAcNPC's answer
    void Shutdown() override;           // slot 3, 0x00092940 GmAcNPC's, then the reset
    void Slot4() override;              // slot 4, 0x00091F50 the last health back to FLT_MAX, then GmAcNPC's
    void Slot10() override;             // slot 10, 0x00091F60 GmAcNPC's
    void Slot15() override;             // slot 15, 0x00091FE0 GmAcNPC's, then the combat object when the record says Combat
    void Slot20(int a) override;        // slot 20, 0x00092990 the health levels when slot 27 says so, then GmAcNPC's
    void Slot49(const char* event, int b, int c) override;  // slot 49, 0x000920C0 GmAcNPC's
    int TestCondition(const char* name) override;  // slot 60, 0x00092160 the AI's condition names

    void Reset();                       // inlined in the constructor, the shutdown and the destructor
    // 0x00092650. When health has fallen since last time, the highest AI health level whose threshold it has crossed: its action to slot 49, its event posted. The value is kept as the last.
    void UpdateHealthLevels();

    // 0x00092A50. Registered three times in the static list of create functions. As GmAcNPC's.
    static FaAcNPC* Create(int count);

    AtDataComponent* m_lowHealthRange;  // +0x308 the record's LowHealthRange value, its descriptor's real bounds; kept when absent
    AtDataComponent* m_medHealthRange;  // +0x30C MedHealthRange
    AtDataComponent* m_highHealthRange; // +0x310 HighHealthRange
    int* m_numAIHealthLevels;           // +0x314 NumAIHealthLevels, where it is
    float m_lastHealth;                 // +0x318 FLT_MAX when reset
    int m_unknown31C;                   // +0x31C only cleared
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(GmGameEvent) == 0x28, "GmGameEvent");
static_assert(offsetof(FaCmCombat, m_health) == 0x80, "FaCmCombat::m_health");
static_assert(offsetof(FaCmCombat, m_threat) == 0x94, "FaCmCombat::m_threat");
static_assert(offsetof(FaCmCombat, m_attackRange) == 0x98, "FaCmCombat::m_attackRange");
static_assert(sizeof(FaCmCombat) == 0xE4, "FaCmCombat");
static_assert(offsetof(GmControlledCharacters, m_count) == 0x4C, "GmControlledCharacters::m_count");
static_assert(offsetof(GmAcNPC, m_aiActions) == 0x304, "GmAcNPC::m_aiActions");
static_assert(sizeof(GmAcNPC) == 0x308, "GmAcNPC");
static_assert(offsetof(FaAcNPC, m_lowHealthRange) == 0x308, "FaAcNPC::m_lowHealthRange");
static_assert(offsetof(FaAcNPC, m_lastHealth) == 0x318, "FaAcNPC::m_lastHealth");
static_assert(sizeof(FaAcNPC) == 0x320, "FaAcNPC");
#pragma pop_macro("offsetof")
#endif
