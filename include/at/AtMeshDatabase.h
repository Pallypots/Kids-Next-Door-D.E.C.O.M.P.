// Meshes, as read from .AGG text, and the vertex and pixel processes of .AGV and .AGP files. Original address clusters 0x0002ADD0-0x0002B0D7, 0x0002B150, 0x00037220-0x000374E0, 0x00043B70-0x00043C9D, 0x00044820-0x000448DD,
// 0x0002B430-0x0002B8AF. The words are the original's, from tables 0x002B0AA0, 0x002B0E90 and 0x002B0F58; other names are inferred. Almost all of what a mesh does is in slots the graphics layer fills, which are not
// recovered: this is the part that reads the file and hands on.
//
// No file on the disc has any of these three extensions. The disc's meshes are .XGG, whose reader is not found here: a concrete database's slot 7 is the way to it. See docs/worklog/2026-11-06-agd.md.
#pragma once

#include "at/AtMaterial.h"

class AtMeshDatabase;

// One triangle: three vertices, given first as indices. 0x10 bytes.
struct AtMathTriangle {
    float* m_vertices[3];          // +0x00 as read, an index; AtMesh::Finish makes it the vertex's position
    unsigned char m_unknown0C[2];  // +0x0C
    unsigned char m_array;         // +0x0E which vertex array
    unsigned char m_axis;          // +0x0F 0: Z of the normal is the largest, 1: X, 2: Y, 3: there is no normal
};
void AtMathTriangleNormal(const AtMathTriangle* triangle, float* out);  // 0x00043B70 (v0 - v1) x (v2 - v1)
void AtMathTriangleSetAxis(AtMathTriangle* triangle);                   // 0x00043BE0

// What a vertex array's lock hands back. The graphics layer's lock may write more of it; only these two are read.
struct AtVertexLock {
    void* m_data;             // +0x00
    unsigned int m_stride;    // +0x04
    char m_unknown08[0x48];   // +0x08 the room up to the next thing known at this address
};

// A vertex array, as the mesh's file sees it. Concrete in the graphics layer.
class AtVertexArray {
public:
    enum Flags { kPositionsInMemory = 0x20 };

    virtual float* PositionOf(float* index) = 0;   // slot 0: an index as read, to the vertex's position
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
    virtual void Finish() = 0;                     // slot 16: called once the file has been read
    virtual void Slot17() = 0;
    virtual void Slot18() = 0;
    virtual void Slot19() = 0;
    virtual void Slot20() = 0;
    virtual void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer, unsigned int index) = 0;  // slot 21
    // Slot 22. Concrete in the graphics layer, which is not recovered; slot 6 is what undoes it: see AtCellGraph::LockedVertex. In src/at/AtCellGraph.cpp.
    virtual void Lock(struct AtVertexLock* out, int unknown);

    int m_unknown04;          // +0x04
    unsigned char m_flags;    // +0x08
};

// An index array. Its base's reader is recovered; where the indices live is the graphics layer's.
class AtIndexArray {
public:
    enum Flags { kFrequentModify = 1, kInfrequentModify = 2, kFixedSysMem = 4, kFixedSysMemOnly = 8, kIndex8Bit = 0x10, kIndex32Bit = 0x20 };
    enum LoadFlags { kDemandLoad = 0x20 };

    virtual void Allocate(unsigned int count, unsigned int flags) = 0;  // slot 0
    virtual void Slot1() = 0;
    virtual void Slot2() = 0;
    virtual void Lock(void** out, int mode) = 0;   // slot 3 the one caller here passes 8
    virtual void Unlock() = 0;                     // slot 4
    virtual void Finish() = 0;                     // slot 5: called once the file has been read

    // 0x000373C0. The token after "IndexArray": "{", any of the flags up to the word Index, its count, the indices or DemandLoad and a name, and two right curlies.
    void Read(AtIoTokenizer* tokenizer, unsigned int index);
    void ReadIndices(AtIoTokenizer* tokenizer, unsigned int flags);  // 0x00044820 each cut to the size the flags say

    unsigned int m_count;      // +0x04
    unsigned short m_flags;    // +0x08
    unsigned char m_loadFlags; // +0x0A
    unsigned char m_unknown0B; // +0x0B
    char m_name[8];            // +0x0C
    unsigned int m_index;      // +0x14
};

// One mesh. The base of a class in the graphics layer.
class AtMesh {
public:
    virtual void Slot0() = 0;
    virtual void Slot1() = 0;
    virtual AtVertexArray* OwnVertexArrayAt(unsigned int index) = 0;  // slot 2, may be null
    virtual AtIndexArray* OwnIndexArrayAt(unsigned int index) = 0;    // slot 3, may be null
    virtual void Slot4() = 0;
    virtual void Slot5() = 0;
    virtual void Slot6() = 0;
    virtual void Slot7() = 0;
    virtual void Slot8() = 0;
    virtual void Slot9() = 0;
    virtual void Slot10() = 0;
    virtual void Slot11() = 0;
    virtual void Read(AtMaterialOwner* owner, AtMeshDatabase* database, AtIoTokenizer* tokenizer, const char* ns) = 0;  // slot 12
    virtual AtVertexArray* VertexArrayAt(unsigned int index) = 0;     // slot 13

    // 0x00037220. Its own arrays finish, then each triangle whose vertex array keeps its positions in memory gets them and its axis. Reports an index past its own arrays, and goes on.
    void Finish();
    // 0x00036AD0. By index: its own if it has any, stopping if out of range; otherwise the database's. In src/at/AtCellGraph.cpp.
    AtVertexArray* VertexArray(unsigned int index);

    unsigned int m_ownVertexArrays;    // +0x04
    unsigned int m_ownIndexArrays;     // +0x08
    int m_unknown0C;                   // +0x0C
    unsigned int m_triangleCount;      // +0x10
    AtMathTriangle* m_triangles;       // +0x14
    AtMeshDatabase* m_database;        // +0x18
};

// Vftable 0x00273908. Slots 2, 4, 5 and 8 to 11 are a concrete database's. The base has no binary form: a concrete database that has one overrides slots 6 and 7.
class AtMeshDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 66 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B0AA0

    AtMeshDatabase();                        // 0x0002ADD0
    void Clear() override;                   // slot 1, 0x0002B150. Slot 0, 0x0002ADF0, deletes through GmNameMap's complete destructor: this class adds nothing to it, so the compiler's does here.

    virtual void MakeMeshes(AtMaterialOwner* owner) = 0;        // slot 2
    virtual void Slot3(int unknown) {}                          // slot 3, 0x000CBF30: does nothing
    virtual void MakeVertexArrays(AtMaterialOwner* owner) = 0;  // slot 4
    virtual void MakeIndexArrays(AtMaterialOwner* owner) = 0;   // slot 5
    virtual int HasBinaryForm() { return 0; }                   // slot 6, 0x000AB720
    virtual void LoadBinaryForm(AtMaterialOwner* owner, const char* name) {}  // slot 7, 0x000447F0: does nothing
    virtual void Slot8() = 0;
    virtual AtMesh* MeshAt(unsigned int index) = 0;             // slot 9
    virtual AtVertexArray* VertexArrayAt(unsigned int index) = 0;  // slot 10
    virtual AtIndexArray* IndexArrayAt(unsigned int index) = 0;    // slot 11

    // 0x0002AFE0. mode 2 and a binary form: that form, and no text. Otherwise name + ".AGG", with its directory current while it is read and finished.
    void Load(AtMaterialOwner* owner, const char* name, int mode);
    // 0x0002AE10. An optional NameSpace first, VertexArray and IndexArray lists in any order, then one Mesh list, which ends the reading. A word that is none of these is reported and taken as Mesh.
    void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer);
    void Finish();                           // 0x0002AD60 vertex arrays, index arrays, meshes
    // 0x0002B0E0. Makes room as a file with these counts would, with no file: clears first if it has meshes, then the name map, vertex and index arrays if any, meshes, and the owner's table under ns. Recovered in
    // src/at/AtGrD3DMeshDatabase.cpp, with the binary reader that calls it.
    void Allocate(AtMaterialOwner* owner, unsigned int meshes, unsigned int vertexArrays, unsigned int indexArrays, const char* ns);

    unsigned int m_meshCount;                // +0x14
    union {
        int m_unknown18;                     // +0x18
        AtMeshDatabase* m_more;              // +0x18 what a concrete database's slot 8 makes. Clear forgets it, and nothing frees it
    };
    unsigned int m_vertexArrayCount;         // +0x1C
    unsigned int m_indexArrayCount;          // +0x20
};

// A vertex process, or a pixel process, as its database sees it. In the graphics layer.
class AtProcessItem {
public:
    virtual void OnLoaded(AtMaterialOwner* owner) = 0;  // slot 0
    virtual void Slot1() = 0;
    virtual void ReadPixel(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) = 0;   // slot 2 on a pixel process
    virtual void Slot3() = 0;
    virtual void Slot4() = 0;
    virtual void ReadVertex(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) = 0;  // slot 5 on a vertex process
};

// Vftable 0x00274508. Its Clear leaves the map as it is; the pixel processes' does not.
class AtVertexProcessDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 127 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B0F58

    AtVertexProcessDatabase();               // 0x0002B690
    ~AtVertexProcessDatabase() override;     // slot 0; 0x0002B890 deleting, 0x0002B870 complete
    void Clear() override;                   // slot 1, 0x0002B860

    virtual void Make(AtMaterialOwner* owner) = 0;          // slot 2
    virtual AtProcessItem* ItemAt(unsigned int index) = 0;  // slot 3

    void Load(AtMaterialOwner* owner, const char* name, int unused);   // 0x0002B7A0 name + ".AGV", with its directory current
    void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer);       // 0x0002B6B0

    unsigned int m_count;                    // +0x14
};

// Vftable 0x00273E7C.
class AtPixelProcessDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 23 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B0E90

    AtPixelProcessDatabase();                // 0x0002B430
    ~AtPixelProcessDatabase() override;      // slot 0; 0x0002B660 deleting, 0x0002B610 complete
    void Clear() override;                   // slot 1, 0x0002B600

    virtual void Make(AtMaterialOwner* owner) = 0;          // slot 2
    virtual AtProcessItem* ItemAt(unsigned int index) = 0;  // slot 3

    void Load(AtMaterialOwner* owner, const char* name, int unused);   // 0x0002B540 name + ".AGP", with its directory current
    void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer);       // 0x0002B450

    unsigned int m_count;                    // +0x14
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtMathTriangle) == 0x10, "AtMathTriangle");
static_assert(offsetof(AtVertexArray, m_flags) == 0x08, "AtVertexArray::m_flags");
static_assert(offsetof(AtIndexArray, m_name) == 0x0C, "AtIndexArray::m_name");
static_assert(sizeof(AtIndexArray) == 0x18, "AtIndexArray");
static_assert(offsetof(AtMesh, m_database) == 0x18, "AtMesh::m_database");
static_assert(sizeof(AtMeshDatabase) == 0x24, "AtMeshDatabase");
static_assert(offsetof(AtVertexProcessDatabase, m_count) == 0x14, "AtVertexProcessDatabase::m_count");
static_assert(offsetof(AtPixelProcessDatabase, m_count) == 0x14, "AtPixelProcessDatabase::m_count");
#pragma pop_macro("offsetof")
#endif
