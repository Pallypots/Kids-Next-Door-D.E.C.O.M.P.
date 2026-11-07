// Bounding geometry, the .ASB format: named boxes, spheres and groups in a tree, tested against each other and against a line segment, for collision. What the format is comes from the code at 0x00040AA0-0x00040F90 and 0x00053A40, from the keyword
// table at 0x002B21A0 and from the text of the 239 distinct real files; the original's own message says "bounding geometry". The class names are the file's words; everything else is inferred. Original address clusters 0x00040A60-0x000410DF,
// 0x000539F0-0x000551BF, 0x0005CC80-0x0005ED62, with what they draw with at 0x00058CB0-0x000594FF and 0x000454C0-0x00045586.
//
//     NameSpace "x"                      optional
//     BoxBoundingGeometry[n]             any order, each optional, and counts only
//     SphereBoundingGeometry[n]
//     GroupBoundingGeometry[n]
//     BoundingGeometry[count]            the list proper
//     {
//         BoxBoundingGeometry "name"     or Sphere..., Group...
//         {
//             Parent "name"              must be earlier in the file
//             RotTransCoordSys 1         a joint of the node's skeleton, from 1; 0 or none is the node's own coordinate system
//             Box cx cy cz hx hy hz      half sizes, taken as absolute values
//             BoxMinMax x y z x y z      or corners
//             Sphere cx cy cz r          for a sphere
//         }
//     }
//
// The words Cone and Cylinder are in the table, and nothing reads them: they are reported.
//
// Arithmetic as in at/AtVector3.h: `double` for what stays in an x87 register, `float` for what the original stored. Several functions return their result in st0, unrounded, so here they return double.
#pragma once

#include "at/AtCoordSys.h"
#include "at/AtScene.h"

struct AtNameTableOwner;
class AtBoundingGeometry;
class AtSphereBoundingGeometry;
class AtBoxBoundingGeometry;

// The shape of a box: its centre and half sizes. All the arithmetic on boxes is here. Rotations are three column vectors, as the context below holds them.
struct AtBoundingBox {
    AtBoundingBox();                               // 0x000320A0 clears it; the same code as AtSceneAnimBlend's constructor, folded
    void Read(AtIoTokenizer* tokenizer);           // 0x0005CDB0 centre, then three absolute values
    int Overlaps(const AtBoundingBox* other, const AtVector3* rotation, const AtVector3* position) const;  // 0x0005CE20 separating axes. other is rotated and moved into this box's frame
    int OverlapsSphere(const AtSceneSphere* sphere) const;  // 0x0005D570 the sphere already in this box's frame, and this box taken as at the origin: its centre is not read
    int OverlapsSegment(const AtVector3* direction, const AtVector3* middle, float halfLength) const;  // 0x0005D6E0
    // 0x0005D880. Cuts the segment to the box, each axis in turn, and says which faces it crossed: bits 0-5 where it enters, 6-11 where it leaves. 0 if it misses. A t is written only for a face crossed.
    unsigned int Clip(const AtVector3* start, const AtVector3* end, AtVector3* clippedStart, AtVector3* clippedEnd, float* tStart, float* tEnd) const;
    // 0x0005DD50. The axis of least overlap: its depth, negative when they overlap, and its normal and the two extents along it.
    double Penetration(const AtBoundingBox* other, const AtVector3* rotation, const AtVector3* position, AtVector3* normal, float* extent, float* otherExtent) const;
    double PenetrationSphere(const AtSceneSphere* sphere, AtVector3* normal, float* extent, float* otherExtent) const;  // 0x0005EAB0 as OverlapsSphere. Writes only zero to the extents

    AtVector3 m_center;  // +0x00
    AtVector3 m_half;    // +0x0C
};

// What a segment test collects its answers in: the class with vftable 0x00276F28. What fills it is at 0x000552B0 and in a class derived from it, vftable 0x002770F8, neither recovered here.
class AtSegmentHitList {
public:
    AtSegmentHitList();                            // 0x000555A0
    virtual void Allocate(unsigned int capacity);  // slot 0, 0x000553A0 the keys are zeroed, the hits are not
    virtual void SetStorage(unsigned int capacity, void* keys, void* hits);  // slot 1, 0x00055360 not owned: clearing does not free them
    virtual ~AtSegmentHitList();                   // slot 2, 0x00057440 deleting, for arrays too; 0x000555F0 complete
    virtual void Clear();                          // slot 3, 0x000555B0
    virtual void CountOne();                       // slot 4, 0x00055340 up to the capacity

    void Reset();                                  // 0x00055490 all but m_current
    static void SetHeap(void* heap);               // 0x000552A0

    enum Flags { kNotOwned = 2 };
    enum { kKeySize = 8, kHitSize = 0x2C };

    unsigned int m_flags;              // +0x04
    AtVector3 m_vectors0[2];           // +0x08
    AtVector3* m_first0;               // +0x20 at m_vectors0[0]
    AtVector3* m_second0;              // +0x24 at m_vectors0[1]
    AtVector3 m_vectors1[2];           // +0x28
    AtVector3 m_vectors2[2];           // +0x40
    AtVector3* m_first2;               // +0x58 at m_vectors2[0]
    AtVector3* m_second2;              // +0x5C at m_vectors2[1]
    int m_unknown60[6];                // +0x60
    unsigned int m_capacity;           // +0x78
    unsigned int m_current;            // +0x7C the place the last key went in. Reset leaves it
    void* m_keys;                      // +0x80 eight bytes each
    void* m_hits;                      // +0x84 0x2C bytes each
    unsigned int m_count;              // +0x88
    int m_unknown8C;                   // +0x8C

    static void* s_heap;               // 0x002C7F58
};

// The derived class's array, the only part of it this code touches. What a hit is otherwise is not established.
struct AtSegmentHit {
    int m_unknown00;                   // +0x00
    AtBoundingGeometry* m_geometry;    // +0x04
    unsigned int m_faces;              // +0x08 a box's, from AtBoundingBox::Clip
    int m_unknown0C;                   // +0x0C
};
class AtSegmentQuery : public AtSegmentHitList {
public:
    AtSegmentHit* m_records;           // +0x90
};

// Other subsystems' functions, which stop with a diagnostic in src/at/AtBoundingGeometryHost.cpp, so that a test can supply them. In the original: AtSegmentQueryInsert 0x000552B0 keeps keys in order and returns 0 when a key does not
// make the list; AtSegmentQueryFinish 0x00057B50; AtCoordSysTransformDirection 0x000327B0, UpdateWorld and then the rotation part of the world matrix; AtSphereSegment 0x000434C0, a method of the sphere.
int AtSegmentQueryInsert(AtSegmentQuery* query, float key);
void AtSegmentQueryFinish(AtSegmentQuery* query, int kind);
void AtCoordSysTransformDirection(AtCoordSys* coordSys, const AtVector3* from, AtVector3* to);
int AtSphereSegment(const AtSceneSphere* sphere, const AtVector3* start, const AtVector3* end, float* length, const AtVector3* direction, AtVector3* point, float* key, int result);  // the point it enters and the fraction along; 0 if it misses, and result if start is inside

// Three more of the table of matrix functions of at/AtCoordSys.h, kept apart so as not to disturb that aggregate while others add to it. They belong in AtMatrixTable. Each stops with a diagnostic until set.
struct AtGeometryMatrixTable {
    void (*GetPosition)(const AtMatrix* m, AtVector3* position);                                        // 0x002C61F8
    void (*GetAxes)(const AtMatrix* m, AtVector3* x, AtVector3* y, AtVector3* z);                       // 0x002C61FC
    void (*TransformDirection)(const AtMatrix* m, const AtVector3* from, AtVector3* to);                // 0x002C625C
};
extern AtGeometryMatrixTable g_atGeometryMatrix;

// How vertices are laid out, as the two drawing functions below use it. Other callers of these exist in the graphics layer, which is not recovered. Names are inferred; the flags are not understood beyond what these functions do to them.
struct AtVertexAttributes {
    AtVertexAttributes();                          // 0x00055750
    void Set(unsigned int flags, unsigned int count);  // 0x00055770 up to four

    unsigned int m_flags;              // +0x00
    unsigned int m_count;              // +0x04
    unsigned char m_mask0;             // +0x08
    unsigned char m_mask1;             // +0x09
    unsigned char m_mask2;             // +0x0A
    unsigned char m_mask3;             // +0x0B
};
struct AtVertexFormat {
    AtVertexFormat();                              // 0x000454C0
    void Set(unsigned int flags, unsigned int count, unsigned int texCoords);  // 0x000454F0 up to eight

    AtVertexAttributes m_attributes;   // +0x00
    unsigned int m_texCoordCount;      // +0x0C
    unsigned char m_texCoordMasks[8];  // +0x10
    unsigned char m_mask4;             // +0x18
    unsigned char m_mask5;             // +0x19
};

// What a geometry buffer hands back when it is locked. Only the two arrays used here are known.
struct AtGeometryLocked {
    void* m_positions;                 // +0x00
    int m_positionStride;              // +0x04
    int m_unknown08[10];               // +0x08
    void* m_texCoords;                 // +0x30
    int m_unknown34[7];                // +0x34
    int m_texCoordStride;              // +0x50
    int m_unknown54[7];                // +0x54
};

class AtGeometryDrawDevice;

// A buffer the drawing layer lends for one draw. In the graphics layer, not recovered; only these slots are known.
class AtGeometryBuffer {
public:
    virtual void Slot0() = 0; virtual void Slot1() = 0; virtual void Slot2() = 0; virtual void Slot3() = 0; virtual void Slot4() = 0;
    virtual void Lock(const AtVertexFormat* format, unsigned int vertices, AtGeometryLocked* locked) = 0;  // slot 5
    virtual void Unlock(unsigned int vertices) = 0;                                                        // slot 6
    virtual void Slot7() = 0; virtual void Slot8() = 0;
    virtual void LockIndices(unsigned int indices, unsigned short** out) = 0;                              // slot 9
    virtual void UnlockIndices(unsigned int indices) = 0;                                                  // slot 10
    virtual void Draw(AtGeometryDrawDevice* device, void* material, unsigned int count, void** instances) = 0;  // slot 11 instances points at the caller's own last argument
};

class AtGeometryBufferSource {
public:
    virtual void Slot0() = 0; virtual void Slot1() = 0; virtual void Slot2() = 0; virtual void Slot3() = 0; virtual void Slot4() = 0; virtual void Slot5() = 0;
    virtual void Slot6() = 0; virtual void Slot7() = 0; virtual void Slot8() = 0; virtual void Slot9() = 0; virtual void Slot10() = 0; virtual void Slot11() = 0;
    virtual void Slot12() = 0; virtual void Slot13() = 0; virtual void Slot14() = 0; virtual void Slot15() = 0; virtual void Slot16() = 0; virtual void Slot17() = 0;
    virtual void Slot18() = 0; virtual void Slot19() = 0; virtual void Slot20() = 0;
    virtual AtGeometryBuffer* Buffer() = 0;        // slot 21
};

// Whatever draws. This may be AtSceneDevice; nothing here establishes it.
class AtGeometryDrawDevice {
public:
    virtual void Slot0() = 0; virtual void Slot1() = 0; virtual void Slot2() = 0; virtual void Slot3() = 0; virtual void Slot4() = 0; virtual void Slot5() = 0;
    virtual void Slot6() = 0; virtual void Slot7() = 0; virtual void Slot8() = 0; virtual void Slot9() = 0; virtual void Slot10() = 0; virtual void Slot11() = 0;
    virtual void Slot12() = 0; virtual void Slot13() = 0; virtual void Slot14() = 0; virtual void Slot15() = 0; virtual void Slot16() = 0; virtual void Slot17() = 0;
    virtual void Slot18() = 0; virtual void Slot19() = 0; virtual void Slot20() = 0; virtual void Slot21() = 0; virtual void Slot22() = 0; virtual void Slot23() = 0;
    virtual void Slot24() = 0; virtual void Slot25() = 0; virtual void Slot26() = 0; virtual void Slot27() = 0; virtual void Slot28() = 0; virtual void Slot29() = 0;
    virtual void Slot30() = 0; virtual void Slot31() = 0; virtual void Slot32() = 0; virtual void Slot33() = 0; virtual void Slot34() = 0; virtual void Slot35() = 0;
    virtual void Slot36() = 0; virtual void Slot37() = 0; virtual void Slot38() = 0; virtual void Slot39() = 0; virtual void Slot40() = 0; virtual void Slot41() = 0;
    virtual void Slot42() = 0; virtual void Slot43() = 0; virtual void Slot44() = 0; virtual void Slot45() = 0; virtual void Slot46() = 0; virtual void Slot47() = 0;
    virtual void Slot48() = 0; virtual void Slot49() = 0; virtual void Slot50() = 0; virtual void Slot51() = 0; virtual void Slot52() = 0; virtual void Slot53() = 0;
    virtual void Slot54() = 0; virtual void Slot55() = 0; virtual void Slot56() = 0; virtual void Slot57() = 0; virtual void Slot58() = 0; virtual void Slot59() = 0;
    virtual void Slot60() = 0; virtual void Slot61() = 0; virtual void Slot62() = 0; virtual void Slot63() = 0; virtual void Slot64() = 0; virtual void Slot65() = 0;
    virtual void Slot66() = 0; virtual void Slot67() = 0;
    virtual AtMatrix* DrawingMatrix() = 0;         // slot 68
    virtual int DrawingGeneration() = 0;           // slot 69
    virtual void SetWorld(int stage, AtMatrix* world) = 0;  // slot 70

    int m_unknown04[3];                // +0x04
    AtGeometryBufferSource* m_source;  // +0x10

    void DrawBox(const AtBoundingBox* box, void* material, void* instance);  // 0x00058CB0
    void DrawSphere(const AtSceneSphere* sphere, void* material, unsigned int stacks, unsigned int slices, void* instance);  // 0x00059170
};

// One overlap found, and where they go. The caller of the overlap test owns the list.
struct AtBoundingContact {
    AtBoundingGeometry* m_first;       // +0x00
    AtBoundingGeometry* m_second;      // +0x04
    AtSceneNode* m_firstNode;          // +0x08
    AtSceneNode* m_secondNode;         // +0x0C
};
struct AtBoundingContactList {
    enum Flags { kLeavesOnly = 1 };
    void Add(AtBoundingGeometry* first, AtBoundingGeometry* second, AtSceneNode* firstNode, AtSceneNode* secondNode);  // 0x0005CC80 drops it when full, or when kLeavesOnly and either has children

    unsigned int m_capacity;           // +0x00
    unsigned int m_count;              // +0x04
    unsigned int m_flags;              // +0x08
    AtBoundingContact* m_items;        // +0x0C
};

// Vftable 0x00276D98. What the virtual functions take is the pair of geometries and, in a static context, set up by the functions at 0x00053B70-0x00054286, which are not recovered: they are how a running game calls this.
class AtBoundingGeometry {
public:
    enum Kind { kBox = 0, kSphere = 1, kGroup = 4 };

    AtBoundingGeometry();                          // 0x00053A10
    virtual void ReadWord(AtIoTokenizer* tokenizer, int token) = 0;  // slot 0 a word inside the curly brackets
    virtual int Overlaps(AtBoundingGeometry* other) = 0;  // slot 1
    virtual int TestSegment() = 0;                 // slot 2 on the context's segment
    virtual void CollectSegment() = 0;             // slot 3 the same, and what it hits goes in the context's query
    // Slot 4. How far apart, along the line of least overlap. normal comes back in world space; the extents are the two shapes' thicknesses along it.
    virtual double Penetration(AtSceneNode* node, AtBoundingGeometry* other, AtSceneNode* otherNode, AtVector3* normal, float* extent, float* otherExtent) = 0;
    // Slot 5. The same along a direction given in world space.
    virtual double PenetrationAlong(AtSceneNode* node, AtBoundingGeometry* other, AtSceneNode* otherNode, const AtVector3* direction, float* extent, float* otherExtent) = 0;
    virtual void Draw(void* material, AtGeometryDrawDevice* device, AtSceneNode* node, AtCoordSys* coordSys);  // slot 6, 0x000FCD40 nothing here
    virtual void DrawTree(void* material, AtGeometryDrawDevice* device, AtSceneNode* node, AtCoordSys* coordSys);  // slot 7, 0x00053960 this one and everything under it

    void Read(GmNameMap* database, AtIoTokenizer* tokenizer);  // 0x00053A40 a left curly through its right curly
    void SetDrawTransform(AtGeometryDrawDevice* device, AtSceneNode* node, AtCoordSys* coordSys);  // 0x00053AF0 coordSys may be null: the joint or the node's own
    void RelateTo(const AtBoundingGeometry* other);  // 0x00053DC0 fills s_relativePosition and s_relativeAxes: other in this one's frame
    static void SwapNodes();                       // 0x000539F0 only the nodes: the context's two coordinate systems stay as they are

    int m_kind;                        // +0x04
    int m_jointIndex;                  // +0x08 RotTransCoordSys; from 1
    int m_unknown0C;                   // +0x0C
    AtBoundingGeometry* m_parent;      // +0x10
    AtBoundingGeometry* m_nextSibling; // +0x14
    AtBoundingGeometry* m_firstChild;  // +0x18
    GmNameMap* m_database;             // +0x1C

    // The context.
    static AtSceneNode* s_otherNode;           // 0x002C7ED8
    static AtSceneNode* s_node;                // 0x002C7EDC
    static AtCoordSys* s_otherCoordSys;        // 0x002C7EE4
    static AtSegmentQuery* s_query;            // 0x002C7EE8
    static AtBoundingContactList* s_contacts;  // 0x002C7EEC
    static float s_segmentHalfLength;          // 0x002C7EF0
    static AtCoordSys* s_coordSys;             // 0x002C7EF4
    static AtVector3 s_segmentDirection;       // 0x002C7EF8
    static AtVector3 s_segmentStart;           // 0x002C7F04
    static AtVector3 s_relativePosition;       // 0x002C7F10
    static AtVector3 s_relativeAxes[3];        // 0x002C7F1C
    static AtVector3 s_segmentMiddle;          // 0x002C7F40
    static AtVector3 s_segmentEnd;             // 0x002C7F4C
};

// Vftable 0x00276E38. Only holds others together.
class AtGroupBoundingGeometry : public AtBoundingGeometry {
public:
    AtGroupBoundingGeometry();                     // 0x00054290
    void ReadWord(AtIoTokenizer* tokenizer, int token) override;  // slot 0, 0x000542B0 it has no words of its own
    int Overlaps(AtBoundingGeometry* other) override;  // slot 1, 0x000FBA70 always
    int TestSegment() override;                    // slot 2, 0x001A2B40 always
    void CollectSegment() override;                // slot 3, 0x001CC9F0 empty, folded
    double Penetration(AtSceneNode* node, AtBoundingGeometry* other, AtSceneNode* otherNode, AtVector3* normal, float* extent, float* otherExtent) override;  // slot 4, 0x000542C0 zero
    double PenetrationAlong(AtSceneNode* node, AtBoundingGeometry* other, AtSceneNode* otherNode, const AtVector3* direction, float* extent, float* otherExtent) override;  // slot 5, 0x000542C0 zero
};

// Vftable 0x00276E58.
class AtSphereBoundingGeometry : public AtBoundingGeometry {
public:
    AtSphereBoundingGeometry();                    // 0x000542D0
    void ReadWord(AtIoTokenizer* tokenizer, int token) override;  // slot 0, 0x00054340
    int Overlaps(AtBoundingGeometry* other) override;  // slot 1, 0x00054390
    int TestSegment() override;                    // slot 2, 0x00054950
    void CollectSegment() override;                // slot 3, 0x00054990
    double Penetration(AtSceneNode* node, AtBoundingGeometry* other, AtSceneNode* otherNode, AtVector3* normal, float* extent, float* otherExtent) override;  // slot 4, 0x00054530
    double PenetrationAlong(AtSceneNode* node, AtBoundingGeometry* other, AtSceneNode* otherNode, const AtVector3* direction, float* extent, float* otherExtent) override;  // slot 5, 0x00054750
    void Draw(void* material, AtGeometryDrawDevice* device, AtSceneNode* node, AtCoordSys* coordSys) override;  // slot 6, 0x00054300

    AtSceneSphere m_sphere;            // +0x20
};

// Vftable 0x00276F08.
class AtBoxBoundingGeometry : public AtBoundingGeometry {
public:
    AtBoxBoundingGeometry();                       // 0x00054AA0
    void ReadWord(AtIoTokenizer* tokenizer, int token) override;  // slot 0, 0x00054B30
    int Overlaps(AtBoundingGeometry* other) override;  // slot 1, 0x00054C00
    int TestSegment() override;                    // slot 2, 0x00054AD0
    void CollectSegment() override;                // slot 3, 0x000551C0
    double Penetration(AtSceneNode* node, AtBoundingGeometry* other, AtSceneNode* otherNode, AtVector3* normal, float* extent, float* otherExtent) override;  // slot 4, 0x00054D70
    double PenetrationAlong(AtSceneNode* node, AtBoundingGeometry* other, AtSceneNode* otherNode, const AtVector3* direction, float* extent, float* otherExtent) override;  // slot 5, 0x00054F90
    void Draw(void* material, AtGeometryDrawDevice* device, AtSceneNode* node, AtCoordSys* coordSys) override;  // slot 6, 0x00054AF0

    AtBoundingBox m_box;               // +0x20
};

// Vftable 0x002761F8. A name map of the geometries of one file, which owns them.
class AtBoundingGeometryDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 14 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B21A0

    AtBoundingGeometryDatabase();                  // 0x00040A70
    ~AtBoundingGeometryDatabase() override;        // slot 0, 0x0002ECC0 deleting; 0x00041090 complete
    void Clear() override;                         // slot 1, 0x00041030 the arrays go without destructors, their classes having none

    void Load(AtNameTableOwner* owner, const char* name, int unused);  // 0x00040F90 name + ".ASB"
    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);      // 0x00040AA0

    static void SetHeap(void* heap);               // 0x00040A60

    unsigned int m_boxCount;           // +0x14
    unsigned int m_sphereCount;        // +0x18
    unsigned int m_groupCount;         // +0x1C
    AtBoxBoundingGeometry* m_boxes;    // +0x20
    AtSphereBoundingGeometry* m_spheres;  // +0x24
    AtGroupBoundingGeometry* m_groups; // +0x28

    static void* s_heap;               // 0x002C6428
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtBoundingBox) == 0x18, "AtBoundingBox");
static_assert(offsetof(AtSegmentHitList, m_first0) == 0x20, "AtSegmentHitList::m_first0");
static_assert(offsetof(AtSegmentHitList, m_vectors2) == 0x40, "AtSegmentHitList::m_vectors2");
static_assert(offsetof(AtSegmentHitList, m_capacity) == 0x78, "AtSegmentHitList::m_capacity");
static_assert(offsetof(AtSegmentHitList, m_count) == 0x88, "AtSegmentHitList::m_count");
static_assert(sizeof(AtSegmentHitList) == 0x90, "AtSegmentHitList");
static_assert(offsetof(AtSegmentQuery, m_records) == 0x90, "AtSegmentQuery::m_records");
static_assert(sizeof(AtSegmentHit) == 0x10, "AtSegmentHit");
static_assert(sizeof(AtVertexFormat) == 0x1C, "AtVertexFormat");
static_assert(offsetof(AtGeometryLocked, m_texCoords) == 0x30, "AtGeometryLocked::m_texCoords");
static_assert(offsetof(AtGeometryLocked, m_texCoordStride) == 0x50, "AtGeometryLocked::m_texCoordStride");
static_assert(sizeof(AtGeometryLocked) == 0x70, "AtGeometryLocked");
static_assert(offsetof(AtGeometryDrawDevice, m_source) == 0x10, "AtGeometryDrawDevice::m_source");
static_assert(sizeof(AtBoundingContact) == 0x10, "AtBoundingContact");
static_assert(offsetof(AtBoundingGeometry, m_database) == 0x1C, "AtBoundingGeometry::m_database");
static_assert(sizeof(AtBoundingGeometry) == 0x20, "AtBoundingGeometry");
static_assert(sizeof(AtGroupBoundingGeometry) == 0x20, "AtGroupBoundingGeometry");
static_assert(sizeof(AtSphereBoundingGeometry) == 0x30, "AtSphereBoundingGeometry");
static_assert(sizeof(AtBoxBoundingGeometry) == 0x38, "AtBoxBoundingGeometry");
static_assert(offsetof(AtBoundingGeometryDatabase, m_groups) == 0x28, "AtBoundingGeometryDatabase::m_groups");
static_assert(sizeof(AtBoundingGeometryDatabase) == 0x2C, "AtBoundingGeometryDatabase");
#pragma pop_macro("offsetof")
#endif
