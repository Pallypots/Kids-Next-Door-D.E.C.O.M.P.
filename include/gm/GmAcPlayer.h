// The playable characters: GmAcPlayer and FaAcPlayer, what FaAcPlayer holds, and the controller frame every character may be given (GmAcCharController, declared in include/gm/GmAcCharacter.h,
// whose methods are recovered in src/gm/GmAcPlayer.cpp). Original address clusters 0x001318F0-0x00131AF0, 0x00131F20-0x001322A0, 0x00092BF0-0x00092C46, 0x00092E40-0x00092F6F, 0x00092F70-0x0009313A,
// 0x00093140-0x000931BB, 0x00093B50-0x00093D2A, 0x00093FA0-0x000940A0, 0x00094430-0x000944DC, 0x000948E0-0x00094A74, 0x00094D40, 0x00154520-0x00154775, 0x00152A80-0x00152AFC,
// 0x0006E1D0-0x0006E253, 0x0014BAC0, 0x0014F230, 0x00014030.
//
// Class names. GmAcPlayer and FaAcPlayer are RTTI names, from FaAcPlayer's hierarchy descriptor (0x0029D7F8: FaAcPlayer, GmAcPlayer, GmAcCharacter, GmAcObject, GmAcActor, GmAcNode, GmBaObject,
// AtEvSubscriber, and GmPlug at +0x32C). GmAcPlayer is matched by depth: FaAcPlayer's constructor 0x00093FA0 calls 0x00131F80, the only constructor that stores vftable 0x0028CB48, which calls
// GmAcCharacter's 0x00130F80. GmAcPlayer is abstract: its slot 65 is __purecall. Every other class, member and method name here is inferred, and says so by being descriptive or numbered; record
// words are the files' own.
//
// What is recovered: both classes' constructors and destructors, FaAcPlayer's reset, factory and record words, the slots that touch only their own members and the bases, the turn and movement frames,
// the controller frame's own members, and what FaAcPlayer holds as far as its constructors, destructors and clearing. Input reading (0x0006DFE0 and the input's slot 0), FaAcPlayer's slots 15, 20 and
// 21 (level continuation, combat and shooting, the update) and the target sprite's two set-up slots are not recovered and stop with a diagnostic.
#pragma once

#include "at/AtSegmentQuery.h"
#include "at/AtBoundingGeometry.h"
#include "at/AtVector3.h"
#include "at/AtCoordSys.h"
#include "at/AtDataRegistry.h"
#include "at/AtMaterial.h"
#include "gm/GmAcCharacter.h"
#include "gm/GmSocket.h"

class GmAcPlayer;
class GmNameMap;

// ---- what the players read that is someone else's ----------------------------------------------------------------------------
// 0x00014030. How long a name is, up to eight. A null name crashes. Among the name helpers (AtCopyName9 0x00013F90 and the rest), not recovered before; here because the player is its first user.
int AtNameLength(const char* name);

// What App+0x692C points to (AppUnknown692C in include/gm/GmApp.h on integrate-batch5, made in 0x0008CFB0): the game's input as abstract axes. Only what the players call is declared. A distinct name
// here so as not to clash with that declaration; the two are one class.
class GmPlayerInput {
public:
    virtual int FindAxis(const char* name) = 0;  // slot 0: an axis's id by its name ("GameAxisForward"). The class is not recovered; its vftable is not known.
    // 0x0006DFE0 not recovered: stops with a diagnostic. The value of one axis of one controller; a controller of -1 is 0. Reads a table at +0x108 by controller and calls an axis object's slot 10.
    float AxisValue(int controller, int axis);
};

// 0x000678A0, AtDataRegistry's search by a whole name: `under` (the root when null), then everything beneath it, then its later siblings and everything beneath them, depth first. `exact`
// compares case and all; otherwise case is ignored. A method of AtDataRegistry; a free function here so as not to change that header. src/gm/FaAcPlayerSetup.cpp.
AtDataLink* AtDataRegistryFindNamed(AtDataRegistry* registry, const char* name, AtDataLink* under, int exact);

// What App+0x65B8 is: the saved game's values, in a registry at +0x04, by a section's name and a value's. Only the lookups FaAcPlayer's slot 15 reaches are recovered, in
// src/gm/FaAcPlayerSetup.cpp; 0x00144B00 (booleans) and 0x00144AE0 (integers) are the same as FindReal with types 1 and 2. Names inferred.
struct GmSaveData {
    // 0x00144A50. The value named `name` in the section named `section`, if its type is `type`; null otherwise. The section is a node found anywhere in the registry by its name, exactly; within it
    // the value is searched with parents. An empty or missing section looks for `name` as a node itself, and failing that searches from the root.
    void* Find(const char* section, const char* name, unsigned int type);
    float* FindReal(const char* section, const char* name);  // 0x00144B20 type 3

    int m_unknown00;                    // +0x00
    AtDataRegistry m_registry;          // +0x04
};

// The object at 0x00305A48: game-wide state reached by the game's own classes. Only this reader is recovered; it does not read its own words. Names inferred.
struct GmSavedGame {
    float GetReal(const char* section, const char* name);  // 0x0009D980 App's saved value, or 0. 0x0009D920 and 0x0009D950 are its boolean reader and real writer
    static GmSavedGame* s_instance;     // 0x00305A48
};

// App (0x00304D58, App::s_instance on integrate-batch5): only its +0x65B8 and +0x692C. Nothing on this branch sets it.
struct GmPlayerApp {
    char m_unknown0000[0x65B8];
    GmSaveData m_saveData;              // +0x65B8
    char m_unknown67AC[0x180];
    GmPlayerInput* m_input;             // +0x692C

    static GmPlayerApp* s_instance;     // 0x00304D58
};

// 0x003B34E4: the world's up, at +0x2C, against which the players straighten a frame. Z is up: a controller frame starts forward (0, 1, 0), up (0, 0, 1), right (1, 0, 0).
struct GmWorldAxes {
    char m_unknown00[0x2C];
    AtVector3 m_up;                     // +0x2C

    static GmWorldAxes* s_instance;     // 0x003B34E4
};

// GmDynamicNodeLoader (0x003B3984): the player it holds, at +0x48, one slot, and a count. A member of GmDynamicNodeLoader written as a free function on a struct, as GmShadowCasters is, so as not to
// change include/gm/GmLevelManifest.h.
struct GmLoadedPlayers {
    char m_unknown00[0x48];
    GmAcPlayer* m_players[1];           // +0x48
    unsigned int m_count;               // +0x4C
};
void GmLoadedPlayersAdd(GmLoadedPlayers* loader, GmAcPlayer* player);  // 0x0014F230 into the first free slot, counted; nothing when there is none. The player's slot 10 is its only caller.

// What GmAcPlayer's slot 44 is asked about. Only these two words are read; what it is, is not known.
struct GmAcQuery {
    int m_unknown00;
    int m_kind;                         // +0x04 0 or 1 matter
    char m_unknown08[0x14];
    GmNameMap* m_names;                 // +0x1C where "BIGBOX" is looked up, kind 0
};

// GmAcPlayer's +0x304: only its +0x3C28 is read, by slot 44. Not set by anything recovered here.
struct GmAcPlayerUnknown304 {
    char m_unknown0000[0x3C28];
    int m_unknown3C28;                  // +0x3C28
};

// ---- GmAcPlayer: vftable 0x0028CB48, 67 slots, 0x32C bytes ----------------------------------------------------------------
class GmAcPlayer : public GmAcCharacter {
public:
    enum { kPlayerReset = 0x100 };      // in m_flags98: what lets SetController take a null owner as the character itself

    GmAcPlayer();                       // 0x00131F80
    ~GmAcPlayer() override;             // slot 0; 0x00132270 deleting (not for arrays); 0x00132220 complete
    int Slot2() override;               // slot 2, 0x00131910 BackwardsThresholdTurningValue, MinMoveStickMagnitude, then the character's, whose answer it returns
    void Shutdown() override;           // slot 3, 0x00132070 Reset, then the character's
    void Slot4() override;              // slot 4, 0x001318F0 the character's
    void Slot10() override;             // slot 10, 0x00131950 the character's, then held by the loader, and the scene node's +0x60 is 2
    void Slot11() override;             // slot 11, 0x00131900 m_link = 0; folded with a camera class's (0x00292E78)
    void Slot20(int a) override;        // slot 20, 0x00131980 takes the partner's frame, straightened up, as its own; then the character's
    int Slot27() override;              // slot 27, 0x001A2B40 returns 1, folded
    int Slot42() override;              // slot 42, 0x001A2B40 returns 1, folded
    int Slot44(GmAcQuery* query) override;  // slot 44, 0x00131AA0 "BIGBOX"
    // slot 60, 0x00131FE0. The frame to move in: the node's forward, up and right; 0 says the player is not moving. FaAcPlayer's answers from its stick.
    virtual int MoveFrame(AtVector3* forward, AtVector3* up, AtVector3* right);
    virtual int Slot61(float a, float b);  // slot 61, 0x00132000 m_unknown30C, a above zero, and |b| below BackwardsThresholdTurningValue (0 without one)
    virtual float Slot62();             // slot 62, 0x00094020 0, folded
    virtual float FollowMaxVerticalLook();  // slot 63, 0x00094020 0, folded; FaAcPlayer's reads the record
    virtual float FollowMinVerticalLook();  // slot 64, 0x00094020 0, folded; FaAcPlayer's reads the record
    virtual void ReadLook() = 0;        // slot 65, __purecall. The controller frame calls it before each change; FaAcPlayer's reads two axes
    virtual int Slot66();               // slot 66, 0x00132060 0xFFFF

    void Reset();                       // inlined in the constructor and the shutdown: m_flags98 gains kPlayerReset; every own word 0 but m_unknown30C, 1
    void SetPartner(GmAcActor* partner);  // 0x00131F20 a character partner is given this player's controller, and a character let go of loses it
    void ApplyPendingFrame();           // 0x00131A40 the controller's ApplyPending; the controller is not tested
    void SetControllerFrame(const AtVector3* forward, const AtVector3* up, const AtVector3* right, int now);  // 0x00131A50 nothing without a controller
    void ControllerFrame(AtVector3* forward, AtVector3* up, AtVector3* right);  // 0x00131A70 the controller is not tested
    int HasPendingFrame();              // 0x00131A80 0 without a controller

    GmAcPlayerUnknown304* m_unknown304; // +0x304
    int m_unknown308;                   // +0x308
    int m_unknown30C;                   // +0x30C 1 when reset
    float m_look[2];                    // +0x310 FaAcPlayer's two look axes (0x84, 0x85), each time ReadLook runs
    float m_unknown318[2];              // +0x318 the look axes again, when the controller has no frame pending
    float* m_backwardsThreshold;        // +0x320 the record's BackwardsThresholdTurningValue
    float* m_minMoveStick;              // +0x324 the record's MinMoveStickMagnitude
    GmAcActor* m_partner;               // +0x328
};

// ---- what FaAcPlayer holds -----------------------------------------------------------------------------------------------------
// +0x338, 0x2C bytes, no vftable. What GmAcCharacter's 0x00130BF0 is handed with the sight cone's words, as FaAcPlayer's slot 15 does; also made at 0x00130D3F and 0x00154F78. Its
// constructor and destructor count the living ones and keep one segment query for them all, of three, while any live.
struct GmAcSightQuery {
    GmAcSightQuery();                   // 0x00152A80
    ~GmAcSightQuery();                  // 0x00152AC0 also sets the three words again

    // 0x00152B00. Where from and which way, how far, and the half-angles in degrees, kept as cosines; the lengths also squared. src/gm/FaAcPlayerSetup.cpp.
    void Set(const AtVector3* position, const AtVector3* direction, float length, float yaw, float pitch, float minDistance);

    AtVector3 m_position;               // +0x00
    AtVector3 m_direction;              // +0x0C
    float m_unknown18;                  // +0x18 0 when made; the least distance, squared
    float m_unknown1C;                  // +0x1C -1; the length
    float m_unknown20;                  // +0x20 the length, squared
    float m_unknown24;                  // +0x24 1; the cosine of the yaw
    float m_unknown28;                  // +0x28 1; the cosine of the pitch

    static unsigned int s_count;        // 0x003B3998 how many live
    static AtSegmentQuery s_query;      // 0x003B39A0 allocated for three when the first is made with none, cleared when the last goes
};

// What a sprite is set up from: an object whose +0x10 holds the materials. FaAcPlayer's slot 15 hands its target the word at +0x08 of the object at +0x08 of 0x002CB0A8 (GmSystemScenes,
// include/gm/GmAppSettings.h). What class it is, is not known.
struct GmSpriteScene {
    char m_unknown00[0x10];
    AtMaterialOwner* m_materials;       // +0x10 its slot 21, and its materials
};

// +0x410: vftable 0x00277A3C (3 slots), 0xA4 bytes. A quad drawn with a material named in a scene's tables ("Target"): its vertex format is set to 0xA080, one texture coordinate. Also held by
// classes at 0x001A17D3, 0x001A61C7 and others. Its two set-up slots are in src/gm/FaAcPlayerSetup.cpp.
class GmAcSprite {
public:
    GmAcSprite();                       // 0x0006E1F0
    ~GmAcSprite();                      // 0x0006E230 not virtual: the vftable has no destructor. The same words as Clear
    virtual void Setup(GmSpriteScene* scene);  // slot 0, 0x0006E2E0 the scene, the materials' slot 21, the vertex format; no material
    // slot 1, 0x0006E260 the same, and the material by name, in the name space if one is given. Not an overload of Setup: MSVC orders overloads in reverse
    virtual void SetupNamed(GmSpriteScene* scene, const char* name, const char* ns);
    virtual void Slot2(int a);          // slot 2, 0x000CBF30 empty, folded
    void Clear();                       // 0x0006E1D0

    AtVertexFormat m_format;            // +0x04
    int m_unknown20;                    // +0x20 the scene device's slot 21, when set up
    char m_unknown24[0x70];             // +0x24
    GmSpriteScene* m_scene;             // +0x94
    int m_unknown98;                    // +0x98
    void* m_material;                   // +0x9C
    int m_unknownA0;                    // +0xA0
};

// ---- FaAcPlayer: vftables 0x0027B794 (67 slots) and 0x0027B78C (its GmPlug at +0x32C, 1 slot), 0x4D0 bytes ------------------------
class FaAcPlayer : public GmAcPlayer, public GmPlug {
public:
    enum { kFaPlayerReset = 0x40000 };  // in m_flags98

    FaAcPlayer();                       // 0x00093FA0
    ~FaAcPlayer() override;             // slot 0; 0x00094A20 deleting, for arrays too, and 0x00094D40 its GmPlug-part thunk; 0x00094430 complete
    int Slot2() override;               // slot 2, 0x00092F70 the camera and powuh words, the two game axes, then GmAcPlayer's, whose answer it returns
    void Shutdown() override;           // slot 3, 0x00094070 the combat object, the sprite, Reset, then GmAcPlayer's
    void Slot4() override;              // slot 4, 0x00092BF0 GmAcPlayer's
    void Slot10() override;             // slot 10, 0x00092C00 GmAcPlayer's
    // slot 15, 0x000940A0 the character's; the level's saved time when LevelContinuation; the head joint; the sight query; the combat module when Combat; the target sprite with "Target".
    // src/gm/FaAcPlayerSetup.cpp
    void Slot15() override;
    void Slot20(int a) override;        // slot 20, 0x00094A80 691 bytes: stops with a diagnostic
    void Slot21(int a) override;        // slot 21, 0x00093450 ShotType, Speed, ShotPitch, ShotBone: stops with a diagnostic
    int MoveFrame(AtVector3* forward, AtVector3* up, AtVector3* right) override;  // slot 60, 0x00093B50 from the two game axes, turned by m_unknown4C0 degrees
    int Slot61(float a, float b) override;  // slot 61, 0x00092C10 0
    float FollowMaxVerticalLook() override;  // slot 63, 0x00094050 the record's, or 60
    float FollowMinVerticalLook() override;  // slot 64, 0x00094030 the record's, or -60
    void ReadLook() override;           // slot 65, 0x00093140 axes 0x84 and 0x85 of this player's controller
    int Slot66() override;              // slot 66, 0x00092C40 m_unknown40C

    void Reset();                       // 0x00092E40 m_flags98 gains kFaPlayerReset; the words below, and the combat object at +0x2E4, without deleting it

    // 0x000948E0. Registered in the static list of create functions. One from the actor heap, or for more than one an array with a count ahead of it. -2 is raised for no heap, -4 for no memory.
    static FaAcPlayer* Create(int count);

    int m_unknown334;                   // +0x334
    GmAcSightQuery m_sightQuery;        // +0x338
    int m_unknown364[2];                // +0x364
    float* m_followMaxHorizontalLook;   // +0x36C the record's values, where they are: FollowMaxHorizontalLook
    float* m_followMinVerticalLook;     // +0x370 FollowMinVerticalLook
    float* m_followMaxVerticalLook;     // +0x374 FollowMaxVerticalLook
    float* m_fpMaxZoom;                 // +0x378 FPMaxZoom
    float* m_fpHorizontalDegreesPerSec; // +0x37C FPHorizontalDegreesPerSec
    float* m_fpVerticalDegreesPerSec;   // +0x380 FPVerticalDegreesPerSec
    float* m_fpMaxVerticalLook;         // +0x384 FPMaxVerticalLook
    int* m_jumpsAllowed;                // +0x388 JumpsAllowed
    const char* m_powuhUp;              // +0x38C PowuhUp: a record's name
    void* m_powuhUpDecay;               // +0x390 PowuhUpDecay, in that record
    void* m_powuhUpDeduct;              // +0x394 PowuhUpDeduct, in that record
    AtDataLink* m_powuhUpRecord;        // +0x398 the record PowuhUp names
    void* m_level2x4PickupType;         // +0x39C Level2x4PickupType
    void* m_level2x4PickupName;         // +0x3A0 Level2x4PickupName
    int m_unknown3A4;                   // +0x3A4
    int m_unknown3A8;                   // +0x3A8 1 when reset
    int m_unknown3AC;                   // +0x3AC
    int m_unknown3B0;                   // +0x3B0 1 when reset
    float* m_cosOfAngleToSwitchMouthSides;  // +0x3B4 CosOfAngleToSwitchMouthSides
    int m_unknown3B8[2];                // +0x3B8
    int m_unknown3C0[6];                // +0x3C0 not reset
    AtRotTransCoordSys* m_head;         // +0x3D8 the scene actor's joint HEADR, by slot 15 (m_unknown3D8 before)
    int m_unknown3DC[2];                // +0x3DC
    int m_unknown3E4[6];                // +0x3E4
    int m_unknown3FC;                   // +0x3FC -1 when reset
    int m_unknown400[2];                // +0x400
    int m_unknown408;                   // +0x408 1 when reset
    int m_unknown40C;                   // +0x40C 2 when reset; slot 66's answer
    GmAcSprite m_target;                // +0x410 set up by slot 15 with "Target"
    int m_unknown4B4;                   // +0x4B4 not reset
    int m_gameAxisForward;              // +0x4B8 the input's id for "GameAxisForward"; -1 when reset
    int m_gameAxisTurning;              // +0x4BC "GameAxisTurning"
    float m_unknown4C0;                 // +0x4C0 degrees MoveFrame turns the frame by
    int m_unknown4C4[3];                // +0x4C4
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(GmPlayerApp, m_input) == 0x692C, "GmPlayerApp::m_input");
static_assert(offsetof(GmWorldAxes, m_up) == 0x2C, "GmWorldAxes::m_up");
static_assert(offsetof(GmLoadedPlayers, m_count) == 0x4C, "GmLoadedPlayers::m_count");
static_assert(offsetof(GmAcQuery, m_names) == 0x1C, "GmAcQuery::m_names");
static_assert(offsetof(GmAcPlayerUnknown304, m_unknown3C28) == 0x3C28, "GmAcPlayerUnknown304::m_unknown3C28");
static_assert(offsetof(GmAcPlayer, m_unknown304) == 0x304, "GmAcPlayer::m_unknown304");
static_assert(offsetof(GmAcPlayer, m_look) == 0x310, "GmAcPlayer::m_look");
static_assert(offsetof(GmAcPlayer, m_partner) == 0x328, "GmAcPlayer::m_partner");
static_assert(sizeof(GmAcPlayer) == 0x32C, "GmAcPlayer");
static_assert(sizeof(GmAcSightQuery) == 0x2C, "GmAcSightQuery");
static_assert(offsetof(GmAcSightQuery, m_unknown18) == 0x18, "GmAcSightQuery::m_unknown18");
static_assert(offsetof(GmPlayerApp, m_saveData) == 0x65B8, "GmPlayerApp::m_saveData");
static_assert(sizeof(GmSaveData) == 0x1F4, "GmSaveData");
static_assert(offsetof(GmSpriteScene, m_materials) == 0x10, "GmSpriteScene::m_materials");
static_assert(offsetof(GmAcSprite, m_unknown20) == 0x20, "GmAcSprite::m_unknown20");
static_assert(offsetof(GmAcSprite, m_scene) == 0x94, "GmAcSprite::m_scene");
static_assert(sizeof(GmAcSprite) == 0xA4, "GmAcSprite");
static_assert(offsetof(FaAcPlayer, m_unknown334) == 0x334, "FaAcPlayer::m_unknown334");
static_assert(offsetof(FaAcPlayer, m_sightQuery) == 0x338, "FaAcPlayer::m_sightQuery");
static_assert(offsetof(FaAcPlayer, m_followMaxHorizontalLook) == 0x36C, "FaAcPlayer::m_followMaxHorizontalLook");
static_assert(offsetof(FaAcPlayer, m_powuhUpRecord) == 0x398, "FaAcPlayer::m_powuhUpRecord");
static_assert(offsetof(FaAcPlayer, m_cosOfAngleToSwitchMouthSides) == 0x3B4, "FaAcPlayer::m_cosOfAngleToSwitchMouthSides");
static_assert(offsetof(FaAcPlayer, m_head) == 0x3D8, "FaAcPlayer::m_head");
static_assert(offsetof(FaAcPlayer, m_unknown3FC) == 0x3FC, "FaAcPlayer::m_unknown3FC");
static_assert(offsetof(FaAcPlayer, m_unknown40C) == 0x40C, "FaAcPlayer::m_unknown40C");
static_assert(offsetof(FaAcPlayer, m_target) == 0x410, "FaAcPlayer::m_target");
static_assert(offsetof(FaAcPlayer, m_gameAxisForward) == 0x4B8, "FaAcPlayer::m_gameAxisForward");
static_assert(offsetof(FaAcPlayer, m_unknown4C0) == 0x4C0, "FaAcPlayer::m_unknown4C0");
static_assert(sizeof(FaAcPlayer) == 0x4D0, "FaAcPlayer");
#pragma pop_macro("offsetof")
#endif
