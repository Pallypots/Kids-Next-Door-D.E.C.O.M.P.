// Coordinate systems: a tree of transforms, each with a local and a world matrix, and the table of them that a scene or a skeleton (.AKC) owns. Original address clusters 0x00026820-0x00026E49, 0x000271F0-0x0002732F,
// 0x00032270-0x000329FF and 0x00032A80-0x00032DEA. All names are inferred, from the words of the .ASN format that fill them (RotTransCoordSys, ScaleCoordSys) and from what each does.
//
// Matrices are not read here. The original reaches its matrix functions through a table of pointers in memory, set up by the graphics layer at 0x000EC670, which is not recovered; so a matrix is opaque, and what each pointer does is
// inferred from its arguments only. They stop with a diagnostic until something sets them. The exception is Read, whose function is recovered and is the table's from the start.
#pragma once

#include "at/AtQuaternion.h"
#include "gm/GmNameMap.h"

class AtIoTokenizer;
struct AtIoKeyword;
struct AtMatrix;

// The table at 0x002C60C8-0x002C625C, those members of it this code uses. All cdecl.
struct AtMatrixTable {
    void (*Copy)(AtMatrix* to, const AtMatrix* from);                                  // 0x002C60C8
    void (*Identity)(AtMatrix* m);                                                     // 0x002C60CC
    void (*SetTranslation)(AtMatrix* m, const AtVector3* position);                    // 0x002C60D4
    void (*SetRotation)(AtMatrix* m, const AtQuaternion* rotation);                    // 0x002C60E0
    void (*SetLookAt)(AtMatrix* m, const AtVector3* direction, const AtVector3* up);   // 0x002C60F4
    void (*SetScale)(AtMatrix* m, const AtVector3* scale);                             // 0x002C6120
    void (*SetInverse)(AtMatrix* to, const AtMatrix* from);                            // 0x002C61D4
    void (*Multiply)(AtMatrix* m, const AtMatrix* by);                                 // 0x002C6228
    void (*SetProduct)(AtMatrix* to, const AtMatrix* a, const AtMatrix* b);            // 0x002C6234
    void (*TransformPoint)(const AtMatrix* m, const AtVector3* from, AtVector3* to);   // 0x002C6250
    void (*SetUniformScale)(AtMatrix* m, float scale);                                 // 0x002C611C
    void (*Read)(AtMatrix* m, AtIoTokenizer* tokenizer);                               // 0x002C6264
};
extern AtMatrixTable g_atMatrix;

void AtMatrixRead(AtMatrix* m, AtIoTokenizer* tokenizer);  // 0x00117080 sixteen reals, in the order of the sixteen floats a matrix is. In the table's slot Read.

// A block of matrices the device keeps. Only the slots used here are known.
class AtMatrixPool {
public:
    virtual void Slot0() = 0;
    virtual void Reserve(unsigned int count) = 0;   // slot 1
    virtual void Clear() = 0;                       // slot 2
    virtual unsigned int Count() = 0;               // slot 3 what was reserved
    virtual AtMatrix* At(unsigned int index) = 0;   // slot 4
};

// What hands out matrices; in the graphics layer, not recovered.
class AtMatrixDevice {
public:
    virtual AtMatrix* NewMatrix() = 0;              // slot 0
    virtual AtMatrixPool* NewPool() = 0;            // slot 1
    virtual void Slot2() = 0;
    virtual void Slot3() = 0;
    virtual void FreeMatrix(AtMatrix* matrix) = 0;  // slot 4
    virtual void FreePool(AtMatrixPool* pool) = 0;  // slot 5
};

// Vftable 0x00274AE8.
class AtCoordSysDatabase;

class AtCoordSys {
public:
    enum Dirty { kWorld = 0x01, kAllLocal = 0x2B, kScaled = 0x10, kInverse = 0x20 };

    AtCoordSys();                              // 0x00032460
    virtual float ScaleFactor();               // slot 0, 0x00032360 always 1
    virtual void Invalidate();                 // slot 1, 0x00032330 clears kScaled here and below
    virtual ~AtCoordSys();                     // slot 2, 0x000329D0 deleting; 0x00032490 complete
    virtual void Reset();                      // slot 3, 0x00032270
    virtual void Attach(AtCoordSys* child);    // slot 4, 0x00032370 leaves any parent it has; nothing if this is its parent
    virtual void Detach(AtCoordSys* child);    // slot 5, 0x000323B0 nothing if not found
    virtual int UpdateInverse();               // slot 6, 0x00032610 kInverse if it was done, otherwise 0
    virtual void Visit(void* with);            // slot 7, 0x000323F0 asks every child to do the same, and nothing else happens

    void MarkDirty(unsigned int mask);         // 0x000322D0 stops at any that has one of the bits already
    void SetMatrices(AtMatrix* local, AtMatrix* world);  // 0x000324A0 resets first, if it had any
    int UpdateWorld();                         // 0x000325A0 parents first; kWorld if it was done, otherwise 0
    void Transform(const AtVector3* from, AtVector3* to);  // 0x00032780 local to world
    void CopyLocal(const AtCoordSys* from);    // 0x00032910
    void SetRotation(const AtQuaternion* rotation);  // 0x000272E0
    void SetPosition(const AtVector3* position);     // 0x00027310
    void SetLookAt(const AtVector3* direction, const AtVector3* up);  // 0x00049960
    // 0x000324E0 for a file's word that either kind takes: Parent, or LocalDrawingTransform. Anything else is reported. The parent must have come earlier in the file; if it has not, the report is followed by
    // a call through null, as it is not in the game, where a report does not return.
    void ReadWord(AtIoTokenizer* tokenizer, AtCoordSysDatabase* database, int token);

    AtCoordSys* m_parent;       // +0x04
    AtCoordSys* m_nextSibling;  // +0x08
    AtCoordSys* m_firstChild;   // +0x0C
    AtMatrix* m_drawingLocal;   // +0x10 the word LocalDrawingTransform's matrix; 0 if the file gave none. Multiplied by the world matrix at 0x00032940, and the result kept in m_drawingWorld.
    AtMatrix* m_local;          // +0x14
    AtMatrix* m_world;          // +0x18
    AtMatrix* m_drawingWorld;   // +0x1C
    int m_unknown20;            // +0x20 -1 when reset; not used here
    unsigned int m_dirty;       // +0x24
};

// Vftable 0x00273100. Adds nothing but being able to go in an array.
class AtRotTransCoordSys : public AtCoordSys {
public:
    AtRotTransCoordSys();                      // 0x00026830
    ~AtRotTransCoordSys() override;            // slot 2, 0x00026B50 deleting, for arrays too; 0x00026850 complete

    void Read(AtIoTokenizer* tokenizer, AtCoordSysDatabase* database);  // 0x00032D40 its name, named at once in the database, then a left curly through its right curly
};

// Vftable 0x00274B08. A transform whose scale reaches what is attached to it.
class AtScaleCoordSys : public AtCoordSys {
public:
    AtScaleCoordSys();                         // 0x00032AE0
    void Invalidate() override;                // slot 1, 0x001CC9F0 empty, folded with others
    ~AtScaleCoordSys() override;               // slot 2, 0x00026BB0 deleting, for arrays too
    void Reset() override;                     // slot 3, 0x00032A80 scale of one, and the base's
    void Attach(AtCoordSys* child) override;   // slot 4, 0x00032AA0 and marks kScaled below
    void Detach(AtCoordSys* child) override;   // slot 5, 0x00032AC0 and the child invalidates
    int UpdateInverse() override;              // slot 6, 0x00032B70

    void SetScale(const AtVector3* scale);     // 0x00032B30
    void Read(AtIoTokenizer* tokenizer, AtCoordSysDatabase* database);  // 0x00032C70 as the rotation's, with its own words

    AtVector3 m_scale;          // +0x28
};

// Vftable 0x00273120. The arrays of a file's coordinate systems, and the matrices for them. Filled by Load from an .AKC file: see the worklog for the format. A scene fills its own from an .ASN file.
class AtCoordSysDatabase : public GmNameMap {
public:
    AtCoordSysDatabase();                      // 0x000268F0
    ~AtCoordSysDatabase() override;            // slot 0, 0x00019CD0 deleting; 0x00027280 complete
    void Clear() override;                     // slot 1, 0x000271F0

    // 0x00026920. Room in the name map for rotTrans + scaled, and matrices for those and for `extra` more, which are taken by what calls this.
    void Allocate(AtMatrixDevice* device, unsigned int rotTrans, unsigned int scaled, unsigned int extra);
    void Load(AtMatrixDevice* device, const char* name, int unused);  // 0x00026DB0 name + ".AKC"
    // 0x00026C10. Errors go to the tokenizer, and reading goes on. An entry past the count given is reported, and then read over what lies after the array.
    void Read(AtMatrixDevice* device, AtIoTokenizer* tokenizer);
    AtMatrix* NextMatrix();                    // 0x000268C0 the next of the pool's, or null when there are no more

    static void SetHeap(void* heap);           // 0x00026820

    AtMatrixDevice* m_device;     // +0x14
    AtMatrixPool* m_pool;         // +0x18
    unsigned int m_rotTransCount; // +0x1C
    unsigned int m_scaledCount;   // +0x20
    AtRotTransCoordSys* m_rotTrans;  // +0x24
    AtScaleCoordSys* m_scaled;    // +0x28
    AtCoordSys* m_root;           // +0x2C the last entry read with no parent, by Read; a scene's never sets it
    unsigned int m_extra;         // +0x30
    unsigned int m_nextMatrix;    // +0x34

    enum { kKeywordCount = 9 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B05F8
    static void* s_heap;          // 0x002C6344
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtCoordSys, m_local) == 0x14, "AtCoordSys::m_local");
static_assert(offsetof(AtCoordSys, m_dirty) == 0x24, "AtCoordSys::m_dirty");
static_assert(sizeof(AtRotTransCoordSys) == 0x28, "AtRotTransCoordSys");
static_assert(sizeof(AtScaleCoordSys) == 0x34, "AtScaleCoordSys");
static_assert(offsetof(AtCoordSysDatabase, m_scaled) == 0x28, "AtCoordSysDatabase::m_scaled");
static_assert(sizeof(AtCoordSysDatabase) == 0x38, "AtCoordSysDatabase");
#pragma pop_macro("offsetof")
#endif
