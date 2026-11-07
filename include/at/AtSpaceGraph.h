// A space graph, as read from an .ASG file: a binary partition of space, whose leaves list the nodes of a scene that stand in them, and which carries modifiers down its branches. Every word here is the original's own, from the keyword
// table at 0x002B1800; the classes are named after them. What was AtSceneModifierSet when only its removal functions were known is this class: the node's "links" are its NodeLink, and their items its NodeIncidence.
//
//   SpaceGraph
//   {
//       BinarySpace[n] XBinarySpace[n] YBinarySpace[n] ZBinarySpace[n] LeafSpace[n] NodeLink[n] NodeIncidence[n]    any order, each optional; at least one leaf
//       Root LeafSpace 0
//       BinarySpace[n]
//       {
//           BinarySpace { Partition a b c d  PosChild LeafSpace 1  NegChild BinarySpace 2  BoundingBox ... Modifier "name" "ns" Draw "node" "ns" LineSegIntersect "node" "ns" }
//       }
//       LeafSpace[n]
//       {
//           LeafSpace { BoundingBox ...  Cell "table" "ns" "cell"  Modifier ... Draw ... LineSegIntersect ... }
//       }
//   }
//
// Original address clusters 0x00039AF0-0x0003AEFF, 0x00045710-0x00045E0F, 0x00046410-0x00046CBF, 0x00046F70-0x00047E00. Slots 6, 7, 10 and 11 of a space, and the drawing and moving functions of the graph, are not recovered. See docs/worklog/2026-11-07-asm.md.
#pragma once

#include "at/AtIoTokenizer.h"
#include "at/AtModifier.h"
#include "at/AtModifierState.h"
#include "at/AtNameTableOwner.h"
#include "at/AtScene.h"
#include "at/AtVector3.h"

class AtSpaceGraph;

// Words in the keyword table that this code tests for.
enum AtSpaceWord {
    kAtWordBinarySpace = 57, kAtWordXBinarySpace = 58, kAtWordYBinarySpace = 59, kAtWordZBinarySpace = 60, kAtWordLeafSpace = 61, kAtWordNodeLink = 62, kAtWordNodeIncidence = 63, kAtWordPotentiallyVisibleSet = 64,
    kAtWordPartition = 65, kAtWordDraw = 66, kAtWordCollide = 67, kAtWordSpaceModifier = 68, kAtWordSpaceGraph = 69, kAtWordRoot = 70, kAtWordBoundingBox = 71, kAtWordPosChild = 72, kAtWordNegChild = 73, kAtWordLineSegIntersect = 74,
    kAtWordCell = 75
};

// The list of a node's links, or of a leaf's items, that a link belongs to. What 0 and 1 are for is seen only in that the scene adds nodes to them.
enum AtSpaceLinkKind { kAtLinkKind0, kAtLinkKind1, kAtLinkDraw, kAtLinkLineSegIntersect, kAtLinkKinds };

// What 0x00046410 reads. Its callers are not recovered.
struct AtSpaceDesc {
    int m_unknown00[6];        // +0x00
    AtModifier* m_modifier;    // +0x18
};

// Vftable 0x002763C0. Slots 0, 1, 6 to 11 are pure.
class AtSpace {
public:
    // What a space read before the graph was whole: added to the graph once everything has been read.
    struct Pending {
        AtSpaceGraph* m_graph;
        unsigned int m_kind;
        AtSceneNode* m_node;
        Pending* m_next;
    };

    AtSpace();                                 // 0x00046520
    virtual void ReadWord(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer, int word) = 0;  // slot 0. 0x00046570 is the base's, for what all share
    virtual void Build(AtNameTableOwner* owner) = 0;  // slot 1 modifiers down the tree
    virtual ~AtSpace();                        // slot 2, 0x00046770 deleting; 0x00046740 complete
    virtual void InitFrom(const AtSpaceDesc* desc);   // slot 3, 0x00046410
    virtual void Read(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer);  // slot 4, 0x000463C0
    virtual void Clear();                      // slot 5, 0x00046540
    virtual void Slot6() = 0;                  // slot 6
    virtual void Slot7() = 0;                  // slot 7
    virtual void Attach(AtSpaceGraph* graph, unsigned int kind, AtSceneNode* node) = 0;  // slot 8 to the leaves under this
    virtual void Insert(AtSceneLink* link, float radius, AtSceneLinkItem* item) = 0;     // slot 9 a moving node into the leaves its sphere touches
    virtual void Slot10() = 0;                 // slot 10
    virtual void Slot11() = 0;                 // slot 11
    virtual void Slot12(void* a);              // slot 12, 0x000464C0 asks the modifier

    void ReadCommonWord(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer, int word);  // 0x00046570
    void AddPending(AtSpaceGraph* graph, unsigned int kind, AtSceneNode* node);          // 0x00046420 newest first
    void AttachPending();                      // 0x00046490
    void Enter(AtNameTableOwner* owner);       // 0x000466E0 a copy of the stack's top, and this one's modifier on it
    void Leave(AtNameTableOwner* owner);       // 0x00046720

    AtModifier* m_modifier;    // +0x04
    Pending* m_pending;        // +0x08
};

// Vftable 0x0027638C.
class AtLeafSpace : public AtSpace {
public:
    AtLeafSpace();                             // 0x00045750
    void ReadWord(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer, int word) override;  // slot 0, 0x00045E10
    void Build(AtNameTableOwner* owner) override;  // slot 1, 0x00045920 keeps a copy of what is in force here
    ~AtLeafSpace() override;                   // slot 2, 0x00039BA0 deleting, for arrays too; 0x00045D90 complete
    void Read(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer) override;  // slot 4, 0x00047830 the base's, through a jump
    void Clear() override;                     // slot 5, 0x000457C0
    void Slot6() override;                     // slot 6, 0x00046310 not recovered
    void Slot7() override;                     // slot 7, 0x00045960 not recovered
    void Attach(AtSpaceGraph* graph, unsigned int kind, AtSceneNode* node) override;  // slot 8, 0x000457E0 no link or no item left: nothing happens
    void Insert(AtSceneLink* link, float radius, AtSceneLinkItem* item) override;     // slot 9, 0x00045860
    void Slot10() override;                    // slot 10, 0x00045C30 not recovered
    void Slot11() override;                    // slot 11, 0x00045CA0 not recovered

    void* m_cell;              // +0x0C what a Cell word found
    unsigned int m_sortAxis;   // +0x10 the box's longest side, along which items of kinds 0 and 1 are kept in order
    AtSceneLinkItem* m_first[kAtLinkKinds];  // +0x14
    AtModifierState m_state;   // +0x24
};

// Vftable 0x002770A0. A space with two children. Its functions are at 0x000571F0-0x00057321, in a range held elsewhere. They are written in src/at/AtSpace.cpp so that a graph runs on a real build, are not recorded in config/ except as slots
// of the classes here, and are compared by the at/asm oracle through those classes.
class AtBinarySpaceBase : public AtSpace {
public:
    AtBinarySpaceBase();                       // 0x000572D0
    void ReadWord(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer, int word) override;  // slot 0, 0x000571F0 seam
    void Build(AtNameTableOwner* owner) override;  // slot 1, 0x00057290 seam
    ~AtBinarySpaceBase() override;             // slot 2, 0x000572F0 seam, deleting; 0x00039C00 complete
    void Clear() override;                     // slot 5, 0x00057310 seam
    void Attach(AtSpaceGraph* graph, unsigned int kind, AtSceneNode* node) override;  // slot 8, 0x00057250 seam
    virtual void Plane(float* out) const = 0;  // slot 13 the plane as a b c d

    AtSpace* m_posChild;       // +0x0C
    AtSpace* m_negChild;       // +0x10
};

// Vftable 0x0027649C. Partition: a plane.
class AtBinarySpace : public AtBinarySpaceBase {
public:
    AtBinarySpace();                           // 0x00047870
    void ReadWord(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer, int word) override;  // slot 0, 0x00047840
    ~AtBinarySpace() override;                 // slot 2, 0x0003A900 deleting, for arrays too
    void Read(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer) override;  // slot 4, 0x00047830 the base's, through a jump
    void Clear() override;                     // slot 5, 0x00047890
    void Slot6() override;                     // slot 6, 0x000479D0 not recovered
    void Slot7() override;                     // slot 7, 0x00047AC0 not recovered
    void Insert(AtSceneLink* link, float radius, AtSceneLinkItem* item) override;     // slot 9, 0x000478B0
    void Slot10() override;                    // slot 10, 0x00047BA0 not recovered
    void Slot11() override;                    // slot 11, 0x00047D30 not recovered
    void Plane(float* out) const override;     // slot 13, 0x00047DA0

    AtQuaternion m_plane;      // +0x14 four reals, read as a quaternion is
};

// What the three spaces split along one axis share; no vftable of its own is seen. Partition: one real.
class AtAxisBinarySpace : public AtBinarySpaceBase {
public:
    AtAxisBinarySpace();
    void ReadWord(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer, int word) override;  // slot 0, 0x00046ED0
    ~AtAxisBinarySpace() override;             // slot 2, 0x0003A960 deleting, for arrays too
    void Read(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer) override;  // slot 4, 0x000473A0 the base's, through a jump
    void Clear() override;                     // slot 5, 0x00046AD0

    float m_distance;          // +0x14
};

// Vftable 0x00276464.
class AtXBinarySpace : public AtAxisBinarySpace {
public:
    AtXBinarySpace();                          // 0x00047430
    ~AtXBinarySpace() override;
    void Slot6() override;                     // slot 6, 0x00047560 not recovered
    void Slot7() override;                     // slot 7, 0x00047620 not recovered
    void Insert(AtSceneLink* link, float radius, AtSceneLinkItem* item) override;     // slot 9, 0x00047450
    void Slot10() override;                    // slot 10, 0x000476D0 not recovered
    void Slot11() override;                    // slot 11, 0x000473B0 not recovered
    void Plane(float* out) const override;     // slot 13, 0x00047410
};

// Vftable 0x0027642C.
class AtYBinarySpace : public AtAxisBinarySpace {
public:
    AtYBinarySpace();                          // 0x00046FA0
    ~AtYBinarySpace() override;
    void Slot6() override;                     // slot 6, 0x000470D0 not recovered
    void Slot7() override;                     // slot 7, 0x00047190 not recovered
    void Insert(AtSceneLink* link, float radius, AtSceneLinkItem* item) override;     // slot 9, 0x00046FC0
    void Slot10() override;                    // slot 10, 0x00047240 not recovered
    void Slot11() override;                    // slot 11, 0x00046F10 not recovered
    void Plane(float* out) const override;     // slot 13, 0x00046F70
};

// Vftable 0x002763F4.
class AtZBinarySpace : public AtAxisBinarySpace {
public:
    AtZBinarySpace();                          // 0x00046AB0
    ~AtZBinarySpace() override;
    void Slot6() override;                     // slot 6, 0x00046C00 not recovered
    void Slot7() override;                     // slot 7, 0x00046CC0 not recovered
    void Insert(AtSceneLink* link, float radius, AtSceneLinkItem* item) override;     // slot 9, 0x00046AF0
    void Slot10() override;                    // slot 10, 0x00046D70 not recovered
    void Slot11() override;                    // slot 11, 0x00046A20 not recovered
    void Plane(float* out) const override;     // slot 13, 0x00046A80
};

// Something drawn before or after the graph's contents (0x0003AF10, not recovered): the two lists at +0x44 and +0x48 hold these. Only m_next is used here; the first word is a vftable, whose slot 4 the drawing code calls.
struct AtSpaceGraphHook {
    void* m_vftable;           // +0x00
    int m_unknown04[2];        // +0x04
    AtSpaceGraphHook* m_next;  // +0x0C
};

// What a leaf's Cell word finds cells in. Only this member is seen here.
struct AtSpaceCellTable {
    char m_unknown00[0x30];    // +0x00
    GmNameMap m_cells;         // +0x30
};

class AtSpaceGraph {
public:
    enum { kKeywordCount = 19 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B1800

    AtSpaceGraph();                            // 0x00039FD0 clears the two counts of items too
    ~AtSpaceGraph();                           // 0x0003A450 Clear, through a jump

    void Load(AtNameTableOwner* owner, const char* name, int unused);  // 0x0003ADC0 name + ".ASG"; clears first if loaded
    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);      // 0x0003AC90
    void Clear();                              // 0x0003A030 takes every node off, then frees all
    void Build(AtNameTableOwner* owner);       // 0x0003A410 works out each leaf's modifiers, on a stack made for it and freed after

    AtSpace* ReadSpaceReference(AtIoTokenizer* tokenizer);  // 0x00039C10 a class word and an index; reports and gives null past the end

    void AddAsKind0(AtSceneNode* node);        // 0x0003AEE0 once more from scratch: off the dirty list and links of that kind, then in
    void AddAsKind1(AtSceneNode* node);        // 0x0003AE80
    void AddAsKind2(AtSceneNode* node);        // 0x0003AE70 only in; if it has a link already, nothing
    void RemoveAsKind2(AtSceneNode* node);     // 0x0003A9C0
    void AddAsKind3(AtSceneNode* node);        // 0x0003AEB0
    void AddNode(AtSceneNode* node);           // 0x0003A9F0 of the current kind

    void MakeLink(AtSceneNode* node, AtSceneLink* link, AtSceneLinkItem* item);  // 0x0003A3A0 of the current kind
    AtSceneLink* TakeLink();                   // 0x00039E90 null when none is free
    void ReturnLink(AtSceneLink* link);        // 0x00039EB0
    AtSceneLinkItem* TakeItem();               // 0x00039ED0 null when none is free, but counted regardless

    void AddHookBefore(AtSpaceGraphHook* hook);     // 0x0003A1C0 at the end, unless there
    void RemoveHookBefore(AtSpaceGraphHook* hook);  // 0x0003A1F0
    void AddHookAfter(AtSpaceGraphHook* hook);      // 0x0003A240 at the end, unless there
    void RemoveHookAfter(AtSpaceGraphHook* hook);   // 0x0003A270

    void RemoveDirty(AtSceneNode* node);       // 0x0003A2C0
    void RemoveLink(AtSceneNode* node);        // 0x00039F00
    void RemoveAsKind0(AtSceneNode* node);     // 0x0003A370
    void RemoveAsKind1(AtSceneNode* node);     // 0x0003A310
    void RemoveAsKind3(AtSceneNode* node);     // 0x0003A340

    static void SetHeap(void* heap);           // 0x00039AF0

    unsigned int m_linkCount;  // +0x00 NodeLink. 0 becomes 1000
    AtSceneLink* m_links;      // +0x04
    AtSceneLink* m_freeLinks;  // +0x08
    unsigned int m_itemCount;  // +0x0C NodeIncidence. Not above the links: becomes three times them
    AtSceneLinkItem* m_items;  // +0x10
    AtSceneLinkItem* m_freeItems;  // +0x14
    unsigned int m_binaryCount;  // +0x18
    AtBinarySpace* m_binary;   // +0x1C
    unsigned int m_xCount;     // +0x20
    AtXBinarySpace* m_x;       // +0x24
    unsigned int m_yCount;     // +0x28
    AtYBinarySpace* m_y;       // +0x2C
    unsigned int m_zCount;     // +0x30
    AtZBinarySpace* m_z;       // +0x34
    unsigned int m_leafCount;  // +0x38
    AtLeafSpace* m_leaves;     // +0x3C
    AtSpace* m_root;           // +0x40
    AtSpaceGraphHook* m_hooksBefore;  // +0x44
    AtSpaceGraphHook* m_hooksAfter;   // +0x48
    unsigned int m_kind;       // +0x4C the kind that the functions without one in their name work on. A parameter by another name; Clear leaves it at 4
    AtSceneLink* m_dirty[kAtLinkKinds];  // +0x50

    static int s_itemsInUse;   // 0x002C63F4
    static int s_itemsPeak;    // 0x002C63F8
    static void* s_heap;       // 0x002C63FC an AtHeap

private:
    void ReadCounts(AtIoTokenizer* tokenizer);  // 0x00039B00 up to the word Root
    void ReadSpaces(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);  // 0x00039CD0
    void Allocate();                           // 0x0003A460
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtSpace) == 0xC, "AtSpace");
static_assert(sizeof(AtLeafSpace) == 0x2C, "AtLeafSpace");
static_assert(offsetof(AtLeafSpace, m_first) == 0x14, "AtLeafSpace::m_first");
static_assert(sizeof(AtBinarySpace) == 0x24, "AtBinarySpace");
static_assert(sizeof(AtXBinarySpace) == 0x18, "AtXBinarySpace");
static_assert(sizeof(AtYBinarySpace) == 0x18, "AtYBinarySpace");
static_assert(sizeof(AtZBinarySpace) == 0x18, "AtZBinarySpace");
static_assert(offsetof(AtSpaceGraph, m_root) == 0x40, "AtSpaceGraph::m_root");
static_assert(offsetof(AtSpaceGraph, m_kind) == 0x4C, "AtSpaceGraph::m_kind");
static_assert(offsetof(AtSpaceGraph, m_dirty) == 0x50, "AtSpaceGraph::m_dirty");
#pragma pop_macro("offsetof")
#endif
