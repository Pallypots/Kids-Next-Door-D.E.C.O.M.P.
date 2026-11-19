// The game layer's actor base and what it is built on: AtEvSubscriber, GmBaObject, GmAcNode and GmAcActor, with the animation callback every actor holds. Original address clusters 0x00147CE0-0x001481A0,
// 0x00148EF0-0x00148F5F, 0x0014A630-0x0014A6EF, 0x0014B150-0x0014BCF0, 0x0014CE40-0x0014CEA5, 0x0014DC60-0x0014DD01, 0x0014E250-0x0014E2BB, 0x0014E4B0-0x0014E5F2, 0x0014E730-0x0014E781, 0x00159910-0x00159944,
// 0x00159B20-0x00159CDB, 0x001598B0-0x00159909, 0x001590A0-0x001590EC, 0x00160000-0x001602DA, 0x00091820-0x0009197F, 0x000994B0-0x0009951E, 0x000B1590-0x000B15DC, 0x000DF5B0.
//
// Class names. GmAcAnimationCallback, AtKiAnimationCallBack, AtEvSubscriber, GmBaObject, GmAcNode and GmAcActor are the original's RTTI names. Only the first has a vftable with RTTI; the rest are named by the
// hierarchy descriptors of the classes built on them (FaAcScalable, FaAcSpawnWave, FaAcLightSourceRegion: GmAcActor, GmAcNode, GmBaObject, AtEvSubscriber), matched to constructors by depth: FaAcScalable's
// constructor 0x000A8950 calls 0x0014B5D0 directly, which calls 0x0014CE40, which calls 0x0014E550, whose destructor ends in 0x000DF5B0. The game library has no RTTI of its own (vftables 0x0028E4C8,
// 0x0028E638 and 0x0028E850 have no locator), so a class with no Fa descendant would have no name. Every other class, member and method name here is inferred, and says so by being descriptive.
//
// What is recovered: the four constructors and destructors, the members' own constructors and destructors, the slots that only touch the object's own members, the actor's shutdown, the factories,
// and the animation callback. Slots that reach the scene, physics, the AI, effects or a data file stop with a diagnostic (each body names its original address), and so do the seams below. A seam is
// one original function, named here at its address and mapped to it in config/functions.d/gmactor.toml; replacing it means deleting its body here, not writing a second class.
#pragma once

#include "at/AtScene.h"
#include "at/AtSceneActor.h"
#include "gm/GmLinkedList.h"
#include "gm/GmNameMap.h"

class AtCoordSys;
class AtDataComponent;
class AtVector3;
class GmAcActor;
class GmAcNode;
class GmAudioData;
class GmVfxManager;
struct AtMatAssignment;
struct GmSoundNote;

// ---- what an object of unknown class is, where only its virtual destructor is used -----------------------------------------
// The original deletes these through slot 0 with 1. Nothing else of them is known.
class GmAcOwned {
public:
    virtual ~GmAcOwned() = 0;
};

// ---- AtEvSubscriber: vftable 0x00285828, no RTTI locator. Only its destructor is known --------------------------------------
// 0x000DF5B0 stores the vftable and nothing else; GmBaObject's destructor ends in it, as do those of at least three other classes. Its constructor's store is dead in GmBaObject's and does not appear.
// How many slots it has is not known, so its vftable is not recorded.
class AtEvSubscriber {
public:
    AtEvSubscriber() {}
    virtual ~AtEvSubscriber();          // 0x000DF5B0 complete
};

// ---- GmBaObject: vftable 0x0028E850, 7 slots, 0x18 bytes ------------------------------------------------------------------
class GmBaObject : public AtEvSubscriber {
public:
    // What m_descriptor points at: only these two words are read here.
    struct Descriptor {
        int m_unknown00[2];
        const char* m_name;             // +0x08 handed to AtCopyName by slot 9, and returned by slot 6
        Descriptor* m_alias;            // +0x0C used instead by GmAcActor's slot 6 when that actor's +0x1FC is 0
    };

    GmBaObject();                       // 0x0014E550
    ~GmBaObject() override;             // slot 0; 0x0014E730 deleting, for arrays too; 0x0014E5D0 complete
    virtual int Initialize();           // slot 1, 0x0014E530 shuts down first if initialized; returns 0
    virtual int Slot2();                // slot 2, 0x000AB720 returns 0, folded
    virtual void Shutdown();            // slot 3, 0x0014E570 clears the five words
    virtual void Slot4();               // slot 4, 0x001CC9F0 empty, folded
    virtual int IsInitialized();        // slot 5, 0x00079DA0 m_initialized, folded
    virtual const char* Name();         // slot 6, 0x000955F0 the descriptor's name, or null

    // Not an original function: new[] of these. Some caller of the original makes arrays of them, since its slot 0 (0x0014E730) handles arrays; the compiler gives a class that handling only where new[]
    // of it is compiled, so this is here for that.
    static GmBaObject* NewArray(unsigned int count);

    // 0x0014E590. The first element of the value named `name` under this object's record, in this object's own registry only; null without either. The second argument is not read. The registry's
    // search is told to look in the record's parents too. See include/gm/GmAcCharacter.h: m_unknown10 is the registry and m_descriptor the record, an AtDataLink.
    void* FindValue(const char* name, int unused);

    int m_unknown04;                    // +0x04
    int m_unknown08;                    // +0x08
    int m_initialized;                  // +0x0C
    void* m_unknown10;                  // +0x10 the first argument of slot 9
    Descriptor* m_descriptor;           // +0x14 the last argument of slot 9
};

// ---- what GmAcNode's +0xA8 points at, and its owner -----------------------------------------------------------------------
class GmAcNodeLink;

// The owner holds two sockets that may hold the link, at +0x14 and +0x2C, 0x18 bytes apart: the size of a GmLinkedListOf<T>, which is what they likely are. GmAcNode's shutdown takes the link out of both,
// the second first, through each one's slot 2. Kept as bytes so that the call stays virtual, as in the original: what they are is not established.
struct GmAcNodeLinkOwner {
    GmSocket* First() { return (GmSocket*)m_first; }
    GmSocket* Second() { return (GmSocket*)m_second; }

    char m_unknown00[0x14];
    char m_first[0x18];                 // +0x14
    char m_second[0x18];                // +0x2C
};

// Passed where a GmPlug is expected, deleted through slot 0. Inferred from GmAcNode's shutdown alone.
class GmAcNodeLink : public GmPlug {
public:
    int m_unknown08;                    // +0x08
    int m_unknown0C;                    // +0x0C
    GmAcNodeLinkOwner* m_owner;         // +0x10
    int m_unknown14;                    // +0x14
    int m_unknown18;                    // +0x18 set to 1 when the node lets go
};

// ---- the animation controller at GmAcNode's +0xAC: vftable 0x0028EF50 (3 slots) on an abstract base 0x0028EF44 (3 slots) ------------
// 0x34 bytes. It passes two events to its owner node's slots 40 and 41. Names inferred.
class GmAcAnimSlot {
public:
    ~GmAcAnimSlot();                    // 0x001B3B80 not recovered: stops with a diagnostic
    char m_unknown00[0x20];
};

// The controller's array of slots at +0x28: a pointer from new[] and a count. Only emptying it is known here; the destructor of the controller has it inlined, as the holder's own destructor.
struct GmAcAnimSlots {
    GmAcAnimSlots() { m_data = 0; m_count = 0; }
    ~GmAcAnimSlots() { Free(); }
    void Resize(unsigned int count);    // 0x00159760 not recovered: stops with a diagnostic. Its first branch, for 0, is Free
    void Free();                        // what 0x00159760 does for 0; inlined in the original wherever it appears

    GmAcAnimSlot* m_data;               // +0x00 a count ahead of the first
    unsigned int m_count;               // +0x04
};

class GmAcAnimControllerBase {
public:
    virtual ~GmAcAnimControllerBase() {}  // slot 0; 0x001590D0 deleting. Its body only stores the vftable
    virtual void Slot1(int a) = 0;        // slot 1, pure
    virtual void Slot2(int a) = 0;        // slot 2, pure
};

class GmAcAnimController : public GmAcAnimControllerBase {
public:
    GmAcAnimController();               // 0x001598B0
    ~GmAcAnimController() override;     // slot 0; 0x00159CC0 deleting; 0x00159B20 complete
    void Slot1(int a) override;         // slot 1, 0x001590A0 the owner's slot 40
    void Slot2(int a) override;         // slot 2, 0x001590B0 the owner's slot 41
    void Clear();                       // 0x00159910 nothing at all unless it has an owner
    void Detach();                      // 0x00159300 not recovered: stops with a diagnostic

    int m_unknown04;                    // +0x04
    AtSceneAnimBlend m_blend;           // +0x08 0x18 bytes
    char m_unknown20;                   // +0x20
    char m_unknown21;                   // +0x21
    GmAcNode* m_owner;                  // +0x24
    GmAcAnimSlots m_slots;              // +0x28
    int m_unknown30;                    // +0x30
};

// ---- GmAcNode: vftable 0x0028E638, 45 slots, 0x14C bytes ------------------------------------------------------------------
// Slot names are SlotN where what a slot does is not established; those that stop with a diagnostic say so beside them.
class GmAcNode : public GmBaObject {
public:
    enum Flags98 { kNodeReset = 0x1, kActorReset = 0x4, kUnknown200 = 0x200 };
    enum FlagsA4 { kFlagA4_2 = 0x2, kFlagA4_8 = 0x8, kFlagA4_10 = 0x10, kFlagA4_100 = 0x100 };

    GmAcNode();                         // 0x0014CE40
    ~GmAcNode() override;               // slot 0; 0x0014E4B0 deleting; 0x0014E250 complete
    int Slot2() override;               // slot 2, 0x0014D900 reads the record: update distances and levels, collision groups, material assignments. src/gm/GmAcRecords.cpp
    void Shutdown() override;           // slot 3, 0x0014DC60
    void Slot4() override;              // slot 4, 0x0014D2E0 reads nCollisionGroups: stops with a diagnostic
    virtual int Slot7(int unused);      // slot 7, 0x0014BAA0 Initialize, the argument unused
    virtual void Slot8(int a, const char* name);  // slot 8, 0x0014CEB0 the scene: stops with a diagnostic
    // slot 9, 0x0014BC20. Keeps the first and last arguments, and hands the third and the descriptor's name to slot 8. The second is not read.
    virtual void Setup(void* a, int unused, int c, Descriptor* descriptor);
    virtual void Slot10();              // slot 10, 0x0014BBC0 the scene: stops with a diagnostic
    virtual void Slot11();              // slot 11, 0x0014E2C0 stops with a diagnostic
    virtual void Slot12();              // slot 12, 0x0014D0F0 the scene node: stops with a diagnostic
    virtual int Slot13();               // slot 13, 0x00091820 bit 0x100 of m_flagsA4
    virtual void Slot14(int a);         // slot 14, 0x000CBF30 empty, folded
    virtual void Slot15();              // slot 15, 0x0014DB70 stops with a diagnostic
    virtual void Slot16();              // slot 16, 0x001CC9F0 empty, folded
    virtual void Slot17(int a);         // slot 17, 0x0014D070 stops with a diagnostic
    virtual void Slot18(int a);         // slot 18, 0x000CBF30 empty, folded
    virtual void Slot19();              // slot 19, 0x0014D120 stops with a diagnostic
    virtual void Slot20(int a);         // slot 20, 0x0014E4D0 the scene node: stops with a diagnostic
    virtual void Slot21(int a);         // slot 21, 0x000CBF30 empty, folded
    virtual void Slot22(int a, int b, int c, int d, int e);  // slot 22, 0x0014BD90 stops with a diagnostic
    virtual void Slot23(int a, int b, int c, int d, int e);  // slot 23, 0x0014BE10 stops with a diagnostic
    virtual void Slot24(int a, int b, int c, int d, int e);  // slot 24, 0x0014BF20 stops with a diagnostic
    virtual void Slot25(int a, int b, int c, int d, int e);  // slot 25, 0x0014BFB0 stops with a diagnostic
    virtual void SetFlag10(int on);     // slot 26, 0x00147FB0 sets or clears bit 0x10 of m_flagsA4
    virtual int Slot27();               // slot 27, 0x0014C170 the scene node: stops with a diagnostic
    virtual void Slot28();              // slot 28, 0x0014C040 the scene: stops with a diagnostic
    virtual void Slot29();              // slot 29, 0x0014C080 the scene: stops with a diagnostic
    virtual void Slot30();              // slot 30, 0x0014C6B0 stops with a diagnostic
    virtual void Slot31();              // slot 31, 0x0014C6F0 stops with a diagnostic
    virtual void Slot32();              // slot 32, 0x0014C0C0 stops with a diagnostic
    virtual void Slot33();              // slot 33, 0x0014C120 stops with a diagnostic
    virtual void Slot34();              // slot 34, 0x0014C730 stops with a diagnostic
    virtual void Slot35();              // slot 35, 0x0014C7B0 stops with a diagnostic
    virtual void Slot36(int a, int b);  // slot 36, 0x000447F0 empty, folded
    virtual void Slot37(int a);         // slot 37, 0x000CBF30 empty, folded
    virtual void Slot38(int a);         // slot 38, 0x000CBF30 empty, folded
    virtual void Slot39();              // slot 39, 0x0014C8A0 stops with a diagnostic
    virtual void Slot40(int a);         // slot 40, 0x000CBF30 empty, folded. The animation controller's slot 1 calls it
    virtual void Slot41(int a);         // slot 41, 0x000CBF30 empty, folded. The animation controller's slot 2 calls it
    virtual int Slot42();               // slot 42, 0x000AB720 returns 0, folded
    virtual void Slot43(int a, int b, int c);  // slot 43, 0x0014D600 stops with a diagnostic
    virtual int Slot44(int a);          // slot 44, 0x0014D800 stops with a diagnostic

    void Reset();                       // 0x0014BC60 all but m_flags98, which gains kNodeReset, and the controller
    void SetUnknown140(int value);      // 0x0014BA90
    GmAcAnimController* Animation();    // 0x0014BAB0
    int SetupAndConfigure(void* a, Descriptor* descriptor);   // 0x0014BB00 keeps both, then slot 2, whose answer it returns
    // 0x0014CAA0. FindValue, then the same name under the game's default record (0x003B3524), searched without parents. An empty name finds nothing. The second argument is not read.
    void* FindValueOrDefault(const char* name, int unused);
    // 0x0014CB00. The value object named `name` under this object's record, searched with parents, not its first element; null for an empty name or none found. Neither the registry nor the record
    // is tested. The second argument is not read. src/gm/GmAcNPC.cpp
    AtDataComponent* FindValueObject(const char* name, int unused);
    void ApplyMatAssignment(int which);  // 0x0014D590 the scene: not recovered, stops with a diagnostic. 1, 2, 3 pick +0x12C, +0x130, +0x134
    int IsVisible();                    // 0x0014C1B0 the scene and the camera: not recovered, stops with a diagnostic

    int m_unknown18[3];                 // +0x18
    int m_unknown24[3];                 // +0x24 not touched here
    int m_unknown30[9];                 // +0x30
    int m_unknown54[3];                 // +0x54
    int m_unknown60[3];                 // +0x60 not touched here
    int m_unknown6C[6];                 // +0x6C
    int m_unknown84[3];                 // +0x84
    char m_noteBank[8];                 // +0x90 the bank the animation callback looks in first, when the actor has no audio component. Only the first byte is cleared
    unsigned int m_flags98;             // +0x98
    AtSceneNode* m_sceneNode;           // +0x9C
    AtSceneNode* m_unknownA0;           // +0xA0
    unsigned int m_flagsA4;             // +0xA4
    GmAcNodeLink* m_link;               // +0xA8
    GmAcAnimController m_animation;     // +0xAC
    void* m_unknownE0[7];               // +0xE0 the record's rUpdateDistance, rCriticalUpdateDistanceSquared, rHighUpdateDistanceSquared, rMediumUpdateDistanceSquared, nMinUpdateLevel, nMaxUpdateLevel, nCulledUpdateLevel
    float m_unknownFC[3];               // +0xFC 254 when reset
    float m_unknown108[5];              // +0x108 255 when reset
    void* m_unknown11C[7];              // +0x11C words; the last three, +0x12C, +0x130 and +0x134, are the Normal, Blend and ZOnly material assignments slot 2 finds
    AtMatAssignment* m_matAssignment;   // +0x138
    GmAcOwned* m_unknown13C;            // +0x13C
    int m_unknown140;                   // +0x140 1 when reset
    int m_unknown144;                   // +0x144 cleared by the constructor and destructor only
    int m_unknown148;                   // +0x148 likewise
};

// ---- the two sets of four vectors in an actor: vftable 0x0028E30C, 1 slot, 0x38 bytes -------------------------------------
// The constructor (inlined in the actor's) sets only the vftable and m_active; the destructor clears every word.
class GmAcVectorSet {
public:
    GmAcVectorSet() { m_active = 0; }
    virtual ~GmAcVectorSet();           // slot 0; 0x0014B580 deleting; 0x0014B550 complete
    void Clear();                       // every word but the vftable; inlined in the original
    int m_active;                       // +0x04
    float m_vectors[4][3];              // +0x08
};

// ---- what the actor's list at +0x21C holds ------------------------------------------------------------------------------
class GmAcLinkedItem : public GmPlug {
public:
    struct ReleaseArgs {
        int m_one;                      // always 1 from the actor's shutdown
        int m_flag;                     // the argument of GmAcActor::ReleaseItems
    };
    void Release(const ReleaseArgs* args);  // 0x001B1420 not recovered: stops with a diagnostic

    char m_unknown08[0x1C];
    GmAcActor* m_owner;                 // +0x24 cleared when the actor lets go of it
};

// The list's own class: GmLinkedListOf<T> by the pattern of the one with RTTI (GmLinkedListOf<FaUiBase*>); this one has none, and its element type is inferred. Vftables 0x0028E2F4 (GmLinkedList part, 5 slots)
// and 0x0028E308 (GmPlug part, 1 slot). Its constructor and destructor are out of line in the original.
template <>
class GmLinkedListOf<GmAcLinkedItem*> : public GmLinkedList, public GmPlug {
public:
    GmLinkedListOf();                   // 0x00147EE0
    ~GmLinkedListOf() override;         // slot 0; 0x00148170 deleting, 0x00148190 its GmPlug-part thunk; 0x00148110 complete
};

// The list's iterator: vftable 0x0028E33C. Nothing of its own; the actor's shutdown makes one on the stack.
template <class T>
class GmLinkedListIteratorOf : public GmLinkedListIterator {
public:
    explicit GmLinkedListIteratorOf(GmLinkedList* list) : GmLinkedListIterator(list) {}
};

// ---- the actor's audio component at +0x20C, and what it holds at +0x68 ---------------------------------------------------
class GmAcAudio {
public:
    void Slot38(int a);                 // 0x00194BF0 not recovered: stops with a diagnostic
};

class GmAcComponent {
public:
    virtual ~GmAcComponent() = 0;       // slot 0, through which the actor deletes it
    virtual void Slot1() = 0;
    virtual void Slot2() = 0;
    virtual void Slot3() = 0;           // slot 3: the actor's shutdown calls it before deleting
    void SendEvent(int a, const char* name);  // 0x0015EC10 not recovered: stops with a diagnostic

    char m_unknown04[0x64];
    GmAcAudio* m_audio;                 // +0x68 the animation callback plays nothing without it
};

// An actor's motions at +0x214. Each is told slot 3 and deleted when the actor shuts down.
class GmAcMotion {
public:
    virtual ~GmAcMotion() = 0;
    virtual void Slot1() = 0;
    virtual void Slot2() = 0;
    virtual void Slot3() = 0;
};

// +0x218: only its slot 15 is used, by the actor's shutdown.
class GmAcUnknown218 {
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
};

// The table at an actor's +0x164: 0x18-byte records of three names. Not recovered.
class GmAcTriggerTable {
public:
    void FindTrigger(const char* trigger, char* first, char* second);  // 0x0017E7C0 not recovered: stops with a diagnostic
    GmSoundNote* FindNote(const char* trigger);                         // 0x0017E1A0 not recovered: stops with a diagnostic
};

// 0x003B3990: what a note's GameEvent goes to.
class GmEventManager {
public:
    void Post(const char* event, const char* argument, GmAcActor* actor);  // 0x001511E0 not recovered: stops with a diagnostic
    static GmEventManager* s_instance;  // 0x003B3990
};

// One of the actor's records at +0x234, 0x44 bytes: three of them unless +0x238 says otherwise. Only the words ClearRecords clears are named. A waiting list of effects, by the record's words that size it.
struct GmAcActorRecord {
    GmAcActorRecord();                  // 0x001480C0 every word 0 but +0x20, 1. src/gm/GmAcRecords.cpp
    int m_unknown00[2];                 // +0x00
    int m_unknown08[3];                 // +0x08
    int m_unknown14[3];                 // +0x14
    float m_unknown20;                  // +0x20 1 when constructed
    int m_unknown24;                    // +0x24
    int m_unknown28;                    // +0x28
    int m_unknown2C[2];                 // +0x2C
    int m_unknown34[4];                 // +0x34
};

// ---- the animation callback ----------------------------------------------------------------------------------------------
// AtKiAnimationCallBack: vftable 0x002845C0, 7 slots, no RTTI locator; the RTTI name is from GmAcAnimationCallback's hierarchy. 8 bytes.
class AtKiAnimationCallBack {
public:
    AtKiAnimationCallBack();            // 0x000B1590
    virtual ~AtKiAnimationCallBack();   // slot 0; 0x000B15C0 deleting; 0x000B15A0 complete
    virtual void Slot1(int a);          // slot 1, 0x000CBF30 empty, folded
    virtual void Slot2(int a);          // slot 2, 0x000CBF30 empty, folded
    virtual void OnLoop(int a);         // slot 3, 0x000CBF30 empty, folded
    virtual void OnTrigger(const char* trigger, int b, int c);  // slot 4, 0x001ACD70 empty
    virtual int Slot5(int a);           // slot 5, 0x000B15B0 returns 0, folded
    virtual void Slot6(int a);          // slot 6, 0x000CBF30 empty, folded

    int m_unknown04;                    // +0x04 1 from the constructor
};

// GmAcAnimationCallback (RTTI name): vftable 0x0027C1D8, 7 slots, 0x14 bytes. An actor's member at +0x168.
class GmAcAnimationCallback : public AtKiAnimationCallBack {
public:
    GmAcAnimationCallback() { Reset(); }  // inlined in the actor's constructor
    ~GmAcAnimationCallback() override;  // slot 0; 0x000994B0 deleting; 0x000994D0 complete
    void OnLoop(int unused) override;   // slot 3, 0x00160010 the actor's slot 49 with "ANIMLOOP", 0, 1
    // slot 4, 0x00160030. An animation's trigger: an effect, then a sound note, then the actor's slot 49; or, with no actor, a note placed at a scene node. Only the first argument is read.
    void OnTrigger(const char* trigger, int unused1, int unused2) override;
    void Reset();                       // 0x00160000

    GmAcActor* m_actor;                 // +0x08
    AtSceneNode* m_node;                // +0x0C with no actor: where a note is placed
    AtCoordSys* m_place;                // +0x10 with no actor: what is placed there
};

// ---- GmAcActor: vftable 0x0028E4C8, 60 slots, 0x2D8 bytes -----------------------------------------------------------------
class GmAcActor : public GmAcNode {
public:
    GmAcActor();                        // 0x0014B5D0
    ~GmAcActor() override;              // slot 0; 0x0014B8E0 deleting, for arrays too; 0x0014B680 complete
    int Slot2() override;               // slot 2, 0x0014ACF0 reads the record (TYPE, Flags, Mass ...), then the node's. src/gm/GmAcRecords.cpp
    void Shutdown() override;           // slot 3, 0x0014B150
    void Slot4() override;              // slot 4, 0x0014AC20 reads Flags: stops with a diagnostic
    const char* Name() override;        // slot 6, 0x00091960 the descriptor's alias's name, unless m_unknown1FC
    void Slot10() override;             // slot 10, 0x001494C0 stops with a diagnostic
    void Slot15() override;             // slot 15, 0x0014AE90 reads Stats, BChart, BScript ...: stops with a diagnostic
    void Slot16() override;             // slot 16, 0x0009DA40 a jump to the node's, which is empty; folded with others
    void Slot17(int a) override;        // slot 17, 0x00149080 stops with a diagnostic
    void Slot20(int a) override;        // slot 20, 0x0014B340 stops with a diagnostic
    void SetFlag10(int on) override;    // slot 26, 0x0014B510 the node's, then the scene
    int Slot27() override;              // slot 27, 0x000918A0 bit 2 of m_flagsA4, or HasUnknown254
    void Slot28() override;             // slot 28, 0x00148570 the node's, then m_attached's
    void Slot29() override;             // slot 29, 0x00148590 likewise
    void Slot38(int a) override;        // slot 38, 0x00148CA0 to the audio component's +0x68, if both are there
    void Slot40(int a) override;        // slot 40, 0x001484B0 ANIMOVER: stops with a diagnostic
    void Slot41(int a) override;        // slot 41, 0x00147CF0 slot 40
    int Slot44(int a) override;         // slot 44, 0x000FBA70 returns 1, folded
    virtual void StartCollision(int a, int unused);  // slot 45, 0x00147D00 "STRTCOL" to the audio component, if there is one
    virtual void EndCollision(int a);   // slot 46, 0x00147D40 "ENDCOL" likewise
    virtual void Slot47(int a, int b, int c);  // slot 47, 0x001494D0 the update, 2819 bytes: stops with a diagnostic
    virtual void ForgetComponent();     // slot 48, 0x00147CE0 m_component = 0, without deleting it
    virtual void Slot49(const char* event, int b, int c);  // slot 49, 0x00148A00 RSETGAME, INIT, EXIT: stops with a diagnostic
    virtual int Unknown278();           // slot 50, 0x00091830 *m_unknown278, or 0
    virtual int Unknown274();           // slot 51, 0x00091840 *m_unknown274, or 0
    virtual float Unknown27C();         // slot 52, 0x00091850 *m_unknown27C as unsigned, or the float at 0x002725F0 (0)
    virtual unsigned int Unknown17C();  // slot 53, 0x00091880
    virtual void Slot54(int* out);      // slot 54, 0x00091890 two zeroes
    virtual int Slot55();               // slot 55, 0x000AB720 returns 0, folded
    virtual int Slot56();               // slot 56, 0x000918F0
    virtual int Slot57();               // slot 57, 0x00091920
    virtual void SetUnknown284(GmAcActor* actor);  // slot 58, 0x00091940 null for this actor itself
    virtual int Slot59();               // slot 59, 0x000AB720 returns 0, folded

    int HasUnknown254();                // 0x000918C0
    void ReleaseItems(int flag);        // 0x0014A630
    void ClearRecords();                // 0x00148EF0 the words of each record that it clears; the array stays
    void AllocateRecords();             // 0x0014A7D0 once, when ConserveVfx says so: VfxWaitingListSize records, or 3; none for a size of 0. src/gm/GmAcRecords.cpp
    void ResetActor();                  // 0x001481A0 every word of the actor's own but the members with classes, and m_flags98 gains kActorReset
    void SpawnEffect(const char* name, int b, const char* c, int d, int e, int f);  // 0x0014A260 not recovered: stops with a diagnostic
    void GetNoteBank(char* out);        // 0x000A1E10 not recovered: stops with a diagnostic
    void SceneSetFlag10(int on);        // 0x0014A8D0 the scene: not recovered, stops with a diagnostic

    // 0x0014B7A0 and 0x0014B950, the same code twice: registered as the "Rotator" and the "Node" of package "GmAcNode" (0x00201620, 0x00202360). One from the heap below, or for more than one an array
    // with a count ahead of it. -2 is raised for no heap, -4 for no memory.
    static GmAcActor* CreateRotator(int count);
    static GmAcActor* CreateNode(int count);
    static void SetHeap(void* heap);    // 0x0014B940

    int m_unknown14C[5];                // +0x14C the last, +0x15C, is the record's Flags
    int m_unknown160;                   // +0x160
    GmAcTriggerTable* m_triggers;       // +0x164
    GmAcAnimationCallback m_callback;   // +0x168
    unsigned int m_unknown17C;          // +0x17C
    int m_unknown180;                   // +0x180
    GmAcVectorSet m_vectors184;         // +0x184
    GmAcVectorSet m_vectors1BC;         // +0x1BC
    AtSceneNode* m_sceneActor;          // +0x1F4 cleared from the space graph, cleared and deleted at shutdown
    GmAcNode* m_attached;               // +0x1F8 its slots 28 and 29 follow this actor's; handed to 0x0014FB40 at shutdown
    int m_unknown1FC;                   // +0x1FC
    int m_unknown200;                   // +0x200
    int m_unknown204;                   // +0x204 the record's RecyclableWhileActive, when it has one
    int m_unknown208;                   // +0x208
    GmAcComponent* m_component;         // +0x20C
    unsigned int m_motionCount;         // +0x210
    GmAcMotion** m_motions;             // +0x214
    GmAcUnknown218* m_unknown218;       // +0x218
    GmLinkedListOf<GmAcLinkedItem*> m_items;  // +0x21C
    GmAcActorRecord* m_records;         // +0x234
    unsigned int* m_recordCount;        // +0x238 null means 3; the record's VfxWaitingListSize
    int* m_unknown23C;                  // +0x23C the record's ConserveVfx
    void* m_unknown240[3];              // +0x240 the record's TYPE, Mass and NearRange
    float m_unknown24C;                 // +0x24C -FLT_MAX when reset
    void* m_unknown250;                 // +0x250 the record's AllowEventsWhenDisabled
    int* m_unknown254;                  // +0x254 the record's UpdateActorWhenDrawDisabled
    void* m_unknown258[3];              // +0x258 the record's UpdateChartWhenDrawDisabled, RicochetShotFlags, UVAnimationDefaultVelocity
    int m_unknown264[2];                // +0x264
    int m_unknown26C;                   // +0x26C
    GmAcOwned* m_unknown270;            // +0x270
    int* m_unknown274;                  // +0x274 the record's NumEmitters
    int* m_unknown278;                  // +0x278 the record's Num2DEmitters
    unsigned int* m_unknown27C;         // +0x27C the record's MaxDistanceOverride
    int m_unknown280;                   // +0x280 1 when reset
    GmAcActor* m_unknown284;            // +0x284
    int m_unknown288[2];                // +0x288
    float m_unknown290[3];              // +0x290 1 when reset
    int m_unknown29C;                   // +0x29C
    int m_unknown2A0[2];                // +0x2A0
    void* m_unknown2A8[5];              // +0x2A8 words; +0x2AC, +0x2B0, +0x2B4 are the record's SendOnScreenEvents, SendOOEEvents, WorldCollisionHeight
    int m_unknown2BC[3];                // +0x2BC
    int m_unknown2C8[3];                // +0x2C8
    float m_unknown2D4;                 // +0x2D4 1 when reset

    static void* s_heap;                // 0x003B3930
    static GmNameMap s_namedMaps;       // 0x002CD124 a name map the shutdown looks "Motion" up in, and ignores what it finds
};

// ---- seams in classes declared elsewhere: each one original function, not recovered, stopping with a diagnostic ------------
void GmDynamicNodeRelease(GmAcNode* node, int flag);                       // 0x0014FB40, a method of GmDynamicNodeLoader (0x003B3984)
int GmVfxManagerHas(GmVfxManager* manager, const char* name);              // 0x001616C0, a method of GmVfxManager (0x003B3C28)
void GmAudioDataPlayNote(GmAudioData* audio, GmAcActor* actor, GmSoundNote* note);  // 0x001386D0, a method of GmAudioData (0x003B354C)
void AtCoordSysWorldPosition(AtCoordSys* coordSys, AtVector3* out);        // 0x000332B0, a method of AtCoordSys
void AtCoordSysPlace(AtCoordSys* coordSys, const AtVector3* forward, int b, const AtVector3* position, int d);  // 0x00032EA0, a method of AtCoordSys

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(GmBaObject) == 0x18, "GmBaObject");
static_assert(offsetof(GmBaObject, m_descriptor) == 0x14, "GmBaObject::m_descriptor");
static_assert(offsetof(GmAcNodeLinkOwner, m_second) == 0x2C, "GmAcNodeLinkOwner::m_second");
static_assert(offsetof(GmAcNodeLink, m_unknown18) == 0x18, "GmAcNodeLink::m_unknown18");
static_assert(offsetof(GmAcAnimController, m_blend) == 0x08, "GmAcAnimController::m_blend");
static_assert(offsetof(GmAcAnimController, m_owner) == 0x24, "GmAcAnimController::m_owner");
static_assert(sizeof(GmAcAnimController) == 0x34, "GmAcAnimController");
static_assert(offsetof(GmAcNode, m_noteBank) == 0x90, "GmAcNode::m_noteBank");
static_assert(offsetof(GmAcNode, m_sceneNode) == 0x9C, "GmAcNode::m_sceneNode");
static_assert(offsetof(GmAcNode, m_animation) == 0xAC, "GmAcNode::m_animation");
static_assert(offsetof(GmAcNode, m_unknownFC) == 0xFC, "GmAcNode::m_unknownFC");
static_assert(offsetof(GmAcNode, m_matAssignment) == 0x138, "GmAcNode::m_matAssignment");
static_assert(sizeof(GmAcNode) == 0x14C, "GmAcNode");
static_assert(sizeof(GmAcVectorSet) == 0x38, "GmAcVectorSet");
static_assert(offsetof(GmAcLinkedItem, m_owner) == 0x24, "GmAcLinkedItem::m_owner");
static_assert(sizeof(GmLinkedListOf<GmAcLinkedItem*>) == 0x18, "GmLinkedListOf<GmAcLinkedItem*>");
static_assert(offsetof(GmAcComponent, m_audio) == 0x68, "GmAcComponent::m_audio");
static_assert(sizeof(GmAcActorRecord) == 0x44, "GmAcActorRecord");
static_assert(sizeof(GmAcAnimationCallback) == 0x14, "GmAcAnimationCallback");
static_assert(offsetof(GmAcActor, m_triggers) == 0x164, "GmAcActor::m_triggers");
static_assert(offsetof(GmAcActor, m_callback) == 0x168, "GmAcActor::m_callback");
static_assert(offsetof(GmAcActor, m_vectors184) == 0x184, "GmAcActor::m_vectors184");
static_assert(offsetof(GmAcActor, m_vectors1BC) == 0x1BC, "GmAcActor::m_vectors1BC");
static_assert(offsetof(GmAcActor, m_sceneActor) == 0x1F4, "GmAcActor::m_sceneActor");
static_assert(offsetof(GmAcActor, m_component) == 0x20C, "GmAcActor::m_component");
static_assert(offsetof(GmAcActor, m_items) == 0x21C, "GmAcActor::m_items");
static_assert(offsetof(GmAcActor, m_records) == 0x234, "GmAcActor::m_records");
static_assert(offsetof(GmAcActor, m_unknown24C) == 0x24C, "GmAcActor::m_unknown24C");
static_assert(offsetof(GmAcActor, m_unknown270) == 0x270, "GmAcActor::m_unknown270");
static_assert(offsetof(GmAcActor, m_unknown284) == 0x284, "GmAcActor::m_unknown284");
static_assert(offsetof(GmAcActor, m_unknown290) == 0x290, "GmAcActor::m_unknown290");
static_assert(offsetof(GmAcActor, m_unknown2D4) == 0x2D4, "GmAcActor::m_unknown2D4");
static_assert(sizeof(GmAcActor) == 0x2D8, "GmAcActor");
#pragma pop_macro("offsetof")
#endif
