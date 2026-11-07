// Modifiers, as read from .ASM files: named objects that change how what is under them in a space graph is drawn, and the map that holds one file's. What each class is follows from the words of the keyword table at 0x002B1D40 and from what
// its reader does with them. The table has 58 words but only six classes have a reader here; the words for texture projectors, shadows, cube maps and the like belong to none of them, and a file using one gets a keyword report. Class names
// are the original's words. Original address clusters 0x0003E920-0x0003F58F, 0x0004F850-0x0005074F.
//
// Slots 2 to 5 belong to drawing. Only the group's, which pass on to its members, are recovered; the rest stop with a diagnostic.
#pragma once

#include "at/AtIoTokenizer.h"
#include "at/AtModifierState.h"
#include "at/AtNameTableOwner.h"
#include "at/AtQuaternion.h"
#include "at/AtScene.h"

// The kinds, which are also the slot of a modifier state.
enum AtModifierKind { kAtModifierLight, kAtModifierAmbient, kAtModifierPerspective, kAtModifierContextConstant, kAtModifierGroup, kAtModifierBlock };

// Words in the keyword table that this code tests for.
enum AtModifierWord {
    kAtWordModifierList = 57, kAtWordLightModifier = 58, kAtWordLight = 59, kAtWordDirectional = 60, kAtWordPoint = 61, kAtWordSpotLight = 62, kAtWordLightPosition = 63, kAtWordDirection = 64, kAtWordAmbientModifier = 65,
    kAtWordColor = 87, kAtWordGroupModifier = 102, kAtWordBlockModifier = 105, kAtWordBlock = 106, kAtWordPriority = 107, kAtWordModifierNameSpace = 108, kAtWordFieldOfViewY = 109, kAtWordPerspectiveModifier = 110,
    kAtWordContextConstantModifier = 111, kAtWordVertexProcessConstant = 112, kAtWordPixelProcessConstant = 113, kAtWordLightAnimation = 114
};

// Vftable 0x00276CE8.
class AtModifier {
public:
    enum Flags { kLoaded = 1, kDisabled = 2 };

    AtModifier();                              // 0x0004F890
    virtual ~AtModifier();                     // slot 0, 0x0004F8B0 deleting; 0x0004F850 complete
    virtual void Apply(AtModifierState* state, int data);  // slot 1, 0x0004F8E0 takes the slot of its kind, unless disabled
    virtual void Slot2(void* a, void* b, void* c, void* d, void* e);  // slot 2, 0x000E7850 empty, folded
    virtual void Slot3(void* a, void* b, void* c, void* d);           // slot 3, 0x000FCD40 empty, folded
    virtual void Slot4(void* a);               // slot 4, 0x000CBF30 empty, folded
    virtual void Slot5(void* a);               // slot 5, 0x000CBF30 empty, folded

    void Reset();                              // 0x0004F8D0 not the class word
    void ReadWord(AtIoTokenizer* tokenizer, int word);  // 0x0004F860 Priority, or a report

    static void SetHeap(void* heap);           // 0x0004F900 for the context constants' lists

    unsigned int m_flags;      // +0x04
    int m_kind;                // +0x08 -1 until set
    int m_priority;            // +0x0C

    static void* s_heap;       // 0x002C7E1C
};

// Vftable 0x00276D48. Words: Light, Directional, Point, SpotLight, Position, Direction, LightAnimation.
class AtLightModifier : public AtModifier {
public:
    enum Type { kDirectional, kPoint, kSpot };

    AtLightModifier();                         // 0x00050140
    ~AtLightModifier() override;               // slot 0, 0x0003EB00 deleting, for arrays too; complete folded with the base's, through 0x0003E990
    void Apply(AtModifierState* state, int data) override;  // slot 1, 0x00050530 joins the others of its kind, in order of priority
    void Slot2(void* a, void* b, void* c, void* d, void* e) override;  // slot 2, 0x000503A0 not recovered
    void Slot3(void* a, void* b, void* c, void* d) override;  // slot 3, 0x000504E0 not recovered
    void Slot4(void* a) override;              // slot 4, 0x00050510 not recovered
    void Slot5(void* a) override;              // slot 5, 0x00050560 not recovered

    void Read(AtSceneDevice* device, AtIoTokenizer* tokenizer);  // 0x000501B0 lights and animations are found in the device's tables

    void* m_light;             // +0x10
    int m_type;                // +0x14
    AtVector3 m_direction;     // +0x18 (1, 0, 0) to begin with
    AtVector3 m_position;      // +0x24
    int m_unknown30;           // +0x30
    // Five words at +0x34, of a class whose code is at 0x0005B710-0x0005BAB2 and held elsewhere: the animation a light follows. See BindAnimation in the source.
    unsigned int m_animationFlags;  // +0x34
    int m_animationPosition;   // +0x38
    int m_animationLength;     // +0x3C the animation's +4
    class AtLightAnimationListener* m_animationListener;  // +0x40 null in everything seen here
    void* m_animation;         // +0x44

private:
    void BindAnimation(void* animation);
};

// Slot 1 is called when a light's animation is bound, if there is one of these. Nothing here makes one.
class AtLightAnimationListener {
public:
    virtual void Slot0() = 0;
    virtual void OnBound(unsigned int* animation) = 0;  // slot 1
};

// Vftable 0x00276D78. Words: Color.
class AtAmbientModifier : public AtModifier {
public:
    AtAmbientModifier();                       // 0x000506B0
    ~AtAmbientModifier() override;             // slot 0, 0x0003EA40 deleting, for arrays too; complete folded with the base's, through 0x0003E990
    void Slot2(void* a, void* b, void* c, void* d, void* e) override;  // slot 2, 0x00050750 not recovered

    void Read(AtIoTokenizer* tokenizer);       // 0x00050700

    AtQuaternion m_color;      // +0x10 four reals, read as a quaternion is; (0, 0, 0, 1) to begin with
};

// Vftable 0x00276D60. Words: FieldOfViewY. Its constructor gives the kind, unlike the others', whose readers do.
class AtPerspectiveModifier : public AtModifier {
public:
    AtPerspectiveModifier();                   // 0x000505A0
    ~AtPerspectiveModifier() override;         // slot 0, 0x0003EAA0 deleting, for arrays too; complete folded with the base's, through 0x0003E990
    void Slot2(void* a, void* b, void* c, void* d, void* e) override;  // slot 2, 0x00050620 not recovered
    void Slot3(void* a, void* b, void* c, void* d) override;  // slot 3, 0x00050670 not recovered

    void Read(AtIoTokenizer* tokenizer);       // 0x000505D0

    float m_fieldOfViewY;      // +0x10 4/3 to begin with
    float m_saved;             // +0x14 4/3 to begin with
};

// Vftable 0x00276D00. Words: VertexProcessConstant, PixelProcessConstant, each a list of four reals and an integer.
class AtContextConstantModifier : public AtModifier {
public:
    struct Constant {
        float m_value[4];      // +0x00
        int m_index;           // +0x10
        int m_unused[3];       // +0x14 room only: left as the heap gave it
    };

    AtContextConstantModifier();               // 0x0004F910
    ~AtContextConstantModifier() override;     // slot 0, 0x0003E930 deleting, for arrays too; 0x0004F9B0 complete
    void Slot2(void* a, void* b, void* c, void* d, void* e) override;  // slot 2, 0x0004F940 not recovered

    void Read(AtIoTokenizer* tokenizer);       // 0x0004FA30

    unsigned int m_vertexCount;  // +0x10
    Constant* m_vertex;        // +0x14
    unsigned int m_pixelCount;  // +0x18
    Constant* m_pixel;         // +0x1C
};

// Vftable 0x00276D18. Words: Modifier, a list of names of others.
class AtGroupModifier : public AtModifier {
public:
    AtGroupModifier();                         // 0x0004FD50
    ~AtGroupModifier() override;               // slot 0, 0x0003E9A0 deleting, for arrays too; 0x0004FDB0 complete
    void Apply(AtModifierState* state, int data) override;  // slot 1, 0x0004FD70 each member, unless disabled
    void Slot2(void* a, void* b, void* c, void* d, void* e) override;  // slot 2, 0x0004FC60 each member
    void Slot3(void* a, void* b, void* c, void* d) override;  // slot 3, 0x0004FCB0 each member
    void Slot5(void* a) override;              // slot 5, 0x0004FD00 each member

    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);  // 0x0004FE20 reading again frees the first list, and resets the base
    AtModifier* MemberAt(unsigned int index) const;  // 0x0004FD30 null when it has none; otherwise no check

    static void SetHeap(void* heap);           // 0x0004FC50

    unsigned int m_count;      // +0x10
    unsigned int m_capacity;   // +0x14
    AtModifier** m_members;    // +0x18

    static void* s_heap;       // 0x002C7E20
};

// Vftable 0x00276D30. Words: Block, followed by the word for the class it blocks. Empties that kind's slot.
class AtBlockModifier : public AtModifier {
public:
    AtBlockModifier();                         // 0x0004FFA0
    ~AtBlockModifier() override;               // slot 0, 0x0003EB60 deleting, for arrays too; complete folded with the base's, through 0x0003E990
    void Apply(AtModifierState* state, int data) override;  // slot 1, 0x000500A0

    void Read(AtIoTokenizer* tokenizer);       // 0x0004FFC0

    int m_blocks;              // +0x10 -1 blocks nothing
};

// Vftable 0x00275CE0. A name map of the modifiers in one .ASM file.
class AtModifierDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 58 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B1D40

    AtModifierDatabase();                      // 0x0003EA00
    ~AtModifierDatabase() override;            // slot 0, 0x0002E760 deleting, for arrays too; 0x0003F530 complete
    void Clear() override;                     // slot 1, 0x0003F430

    void Load(AtNameTableOwner* owner, const char* name, int unused);  // 0x0003F390 name + ".ASM"
    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);      // 0x0003EBC0

    static void SetHeap(void* heap);           // 0x0003E920

    unsigned int m_lightCount;     // +0x14
    unsigned int m_ambientCount;   // +0x18
    unsigned int m_perspectiveCount;  // +0x1C
    unsigned int m_contextCount;   // +0x20
    unsigned int m_groupCount;     // +0x24
    unsigned int m_blockCount;     // +0x28
    AtLightModifier* m_lights;     // +0x2C
    AtAmbientModifier* m_ambients;  // +0x30
    AtPerspectiveModifier* m_perspectives;  // +0x34
    AtContextConstantModifier* m_contexts;  // +0x38
    AtGroupModifier* m_groups;     // +0x3C
    AtBlockModifier* m_blocks;     // +0x40

    static void* s_heap;           // 0x002C6410
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
static_assert(sizeof(AtModifier) == 0x10, "AtModifier");
static_assert(sizeof(AtLightModifier) == 0x48, "AtLightModifier");
static_assert(sizeof(AtAmbientModifier) == 0x20, "AtAmbientModifier");
static_assert(sizeof(AtPerspectiveModifier) == 0x18, "AtPerspectiveModifier");
static_assert(sizeof(AtContextConstantModifier) == 0x20, "AtContextConstantModifier");
static_assert(sizeof(AtContextConstantModifier::Constant) == 0x20, "AtContextConstantModifier::Constant");
static_assert(sizeof(AtGroupModifier) == 0x1C, "AtGroupModifier");
static_assert(sizeof(AtBlockModifier) == 0x14, "AtBlockModifier");
static_assert(sizeof(AtModifierDatabase) == 0x44, "AtModifierDatabase");
#endif
