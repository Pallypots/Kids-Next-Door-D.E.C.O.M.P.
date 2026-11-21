// The matrix device and its pool: what AtCoordSysDatabase, the scene and its nodes take their matrices from. Original address clusters 0x000F5FC0-0x000F61BB and 0x000F7BD0-0x000F7EC9, with a few
// functions elsewhere that the compiler folded with others of the same bytes. All names are inferred, from what each does and what its callers do with it.
//
// The graphics device (vftable 0x00289208, constructor 0x000F1FA0) embeds one AtMatrixDeviceXbox at +0x3E40, and its slot 17, 0x000EF2E0, returns that member's address: that is what 0x0001A68C, the
// owner's slot 0x44, hands to AtCoordSysDatabase::Load. A second, free-standing one is made by 0x000EBDF0, slot 23 of vftable 0x00288B88, not recovered. No other class derives from the base: its
// constructor 0x0010F800 has those two callers only.
//
// Only the matrix part is recovered. Slots 2 and 3 make the graphics layer's vertex array (AtGrD3DVertexArray: vftable 0x00288CF8, 0x64 bytes, constructor 0x000EC890) and index array
// (AtGrD3DIndexArray: vftable 0x00289358, 0x24 bytes, constructor 0x000F8010), giving each the device's m_unknown04 as its m_owner (+0x44, +0x18); they stay behind NewVertexArray and
// NewIndexArray, which stop with a diagnostic. Slots 6 and 7 free them through their deleting destructors, the bases' Destroy (include/at/AtMeshDatabase.h).
//
// Memory: every matrix is 64 bytes, made the identity by AtMatrixObject's constructor. The pool asks its heap for 64-byte alignment while it allocates (AtHeap::m_allocShift 6), which the game's heap
// honours, and the original's SSE matrix functions need 16 (movaps). A host build's operator new does not see that field, so there the alignment is the host allocator's: enough for the recovered matrix
// functions, which need none.
#pragma once

#include "at/AtCoordSys.h"
#include "at/AtMatrix.h"
#include "at/AtMeshDatabase.h"

class AtHeap;

// The original's matrix class: AtMatrix's sixteen floats with a constructor. AtMatrix itself stays a plain layout for the code that only reads and writes it.
struct AtMatrixObject : AtMatrix {
    AtMatrixObject();  // 0x000EC270 the identity, word by word
};

// Vftable 0x002892E4, seven slots. 0xC bytes. One block of count matrices from the class's heap.
class AtMatrixPoolXbox : public AtMatrixPool {
public:
    AtMatrixPoolXbox();                              // 0x000F6180
    ~AtMatrixPoolXbox();                             // 0x000F6000 frees the block, then the base's

    void Slot0() override;                           // slot 0, 0x000F61A0 the deleting destructor; the flag its one caller passes, 1, is taken as given
    // Slot 1, 0x000F6070. Frees any block first. Then count matrices, each the identity, from s_heap, with the heap's allocation shift raised to at least 6 meanwhile and set back to the heap's
    // alignment shift afterwards. count is kept before the allocation, even if it fails. The size is count * 64 in 32 bits, and constructing counts down from count - 1 as a signed number: so a count
    // of 0x80000001 or more constructs nothing, and the size wraps (0x80000000 constructs 2^31 into nothing, and faults). s_heap is not checked for null, as every other heap of its kind is.
    void Reserve(unsigned int count) override;
    void Clear() override;                           // slot 2, 0x001A81E0 frees the block, count 0
    unsigned int Count() override;                   // slot 3, 0x00154540 folded with every other getter of +8
    AtMatrix* At(unsigned int index) override;       // slot 4, 0x000F5FF0 not checked against the count
    virtual int IsReserved();                        // slot 5, 0x000F5FE0 1 if there is a block
    virtual AtMatrix* Matrix(unsigned int index);    // slot 6, 0x000F5FF0 the same function as At

    static void SetHeap(AtHeap* heap);               // 0x000F5FC0

    AtMatrixObject* m_matrices;  // +0x04
    unsigned int m_count;        // +0x08

    static AtHeap* s_heap;       // 0x003B1F58
};

// Vftable 0x00288B48, sixteen slots. 8 bytes: the base's vftable and m_unknown04. The base's own six are its first; slots 6-15 are declared here, which a caller through the base would need moved there.
// The constructor is inlined into its two makers: the base's, then this vftable.
class AtMatrixDeviceXbox : public AtMatrixDevice {
public:
    AtMatrixDeviceXbox();                            // inlined in the original, at 0x000F2131 and 0x000EBE47; out of line here, so that the vftable has a home
    AtMatrix* NewMatrix() override;                 // slot 0, 0x000F7D40 DoNewMatrix, through the vftable
    AtMatrixPool* NewPool() override;                // slot 1, 0x000F7D50 DoNewPool, through the vftable
    void Slot2() override;                           // slot 2, 0x000F7D60 NewVertexArray, through the vftable; the original passes on what it returns
    void Slot3() override;                           // slot 3, 0x000F7D70 NewIndexArray, likewise
    void FreeMatrix(AtMatrix* matrix) override;      // slot 4, 0x000F7D80 operator delete
    void FreePool(AtMatrixPool* pool) override;      // slot 5, 0x000EB9D0 folded: its slot 0 with 1, if not null
    void FreeVertexArray(AtVertexArray* array) override;  // slot 6, 0x000EBB30 folded: its slot 2, Destroy, with 1, if not null
    void FreeIndexArray(AtIndexArray* array) override;   // slot 7, 0x000F7D20 its slot 1, Destroy, with 1, if not null
    // Slot 8, 0x000F7BE0. One identity matrix from s_heap. A null heap is reported (AtRaise -2) and pushed all the same; no memory is reported (-4) and null returned.
    AtMatrix* DoNewMatrix() override;
    virtual AtMatrixPool* DoNewPool();               // slot 9, 0x000F7C80 an empty pool from s_heap, reported as DoNewMatrix's
    virtual AtVertexArray* NewVertexArray();         // slot 10, 0x000F7D90 not recovered: stops with a diagnostic. The original gives the array m_unknown04 at +0x44
    virtual AtIndexArray* NewIndexArray();           // slot 11, 0x000F7E30 not recovered: stops with a diagnostic. The original gives the array m_unknown04 at +0x18
    // Slots 12-15 hold the same functions as 4-7: the compiler folded identical bodies, so these are separate members with the same code. Which callers use which is not known.
    virtual void ReleaseMatrix(AtMatrix* matrix);    // slot 12, 0x000F7D80
    virtual void ReleasePool(AtMatrixPool* pool);    // slot 13, 0x000EB9D0
    virtual void ReleaseVertexArray(AtVertexArray* array);  // slot 14, 0x000EBB30
    virtual void ReleaseIndexArray(AtIndexArray* array);   // slot 15, 0x000F7D20

    static void SetHeap(void* heap);                 // 0x000F7BD0

    static void* s_heap;                             // 0x003B1F94 an AtHeap; set by the startup 0x000C7CA0, not recovered
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
static_assert(sizeof(AtMatrixObject) == 0x40, "AtMatrixObject");
static_assert(sizeof(AtMatrixPoolXbox) == 0xC, "AtMatrixPoolXbox");
static_assert(sizeof(AtMatrixDeviceXbox) == 8, "AtMatrixDeviceXbox");
#endif
