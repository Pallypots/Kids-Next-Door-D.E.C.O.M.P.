// Cell graphs, as read by the loader of ".ASC" text: rooms and the portals between them. Original address clusters 0x0003FC30-0x0003FF6C, 0x00052CC0-0x00053437, 0x00057040-0x000570D1 and 0x0005BAE0-0x0005C62E. The words are the
// original's, from its table at 0x002B2120; other names are inferred. What a leaf of a space graph finds by its Cell word is a cell of one of these, through AtNameTableOwner::FindCellTable. Looking through portals, 0x00052E00 and
// 0x000570E0, 0x0005BCE0 and 0x0005C6B0, is not recovered.
//
//   NameSpace "x"                       optional
//   CellGraph[n]                        the word is not looked at
//   {
//       CellGraph "name"
//       {
//           Mesh "mesh" "ns"            optional; without it no Portal list is read
//           Portal[n]                   read only straight after Mesh
//           {
//               Portal "name" { OppositeCell "cell"  OppositeCellDifferentGraph "graph" "ns" "cell"  Vertices array start count  Closed  ClosedByViewerDist d }
//           }
//           Cell[n] { Cell "name" { Portals first count } }
//           ExteriorPortals first count  optional
//       }
//   }
#pragma once

#include "at/AtMeshDatabase.h"
#include "at/AtNameTableOwner.h"

class AtCellGraph;
struct AtCell;

// One way out of a cell. 0x34 bytes.
struct AtCellPortal {
    enum Flags { kClosedBit1 = 2, kClosedByViewerDist = 4, kClosedBit3 = 8 };  // Closed sets both of the two named so. 1 clears the portal when read again; nothing here sets it.

    AtCellPortal();                                    // 0x0005C490 clears it

    // 0x0005C4D0. Its body. A list of vertices is required in practice: without one this reads through a null pointer, in ComputeNormal.
    void Read(AtCellGraph* graph, AtIoTokenizer* tokenizer);
    void ComputeCenter();                              // 0x0005BB70 the average of the vertices
    void ComputeNormal();                              // 0x0005BC70 from the first three
    // 0x0005BAE0. The names read for this portal to what they name, once all of a graph is read. An empty cell name leaves it alone; an empty graph name means this graph; each failure stops.
    void Resolve(AtNameTableOwner* owner, AtCellGraph* graph, const char* graphName, const char* ns, const char* cellName);

    AtCell* m_opposite;                // +0x00
    AtCellGraph* m_oppositeGraph;      // +0x04
    unsigned int m_flags;              // +0x08
    float m_center[3];                 // +0x0C
    float m_normal[3];                 // +0x18 not of unit length
    float m_closedDistanceSquared;     // +0x24
    const char* m_vertices;            // +0x28 in the mesh's vertex array, which is left unlocked after the read
    unsigned short m_vertexCount;      // +0x2C
    unsigned short m_stride;           // +0x2E
    unsigned short m_firstVertex;      // +0x30
    unsigned short m_vertexArray;      // +0x32
};

// A room. 0x18 bytes.
struct AtCell {
    AtCell();                                          // 0x00057040 m_unknown10 is left as it was
    void Read(AtCellGraph* graph, AtIoTokenizer* tokenizer);  // 0x00057060

    AtCellGraph* m_graph;              // +0x00
    AtCellPortal* m_portals;           // +0x04
    unsigned int m_portalCount;        // +0x08
    int m_unknown0C;                   // +0x0C
    int m_unknown10;                   // +0x10 the traversal's: not recovered
    int m_unknown14;                   // +0x14
};

// 0x48 bytes.
class AtCellGraph {
public:
    AtCellGraph();                     // 0x00052CD0
    ~AtCellGraph();                    // 0x00052E80
    void Clear();                      // 0x00052D30

    // 0x00052EE0. Its body. A file ending inside it reports without end, unless the report stops.
    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);
    // 0x00052D90. A vertex of the mesh by array and index. The array stays locked for the next call; a different array unlocks the last. Read unlocks at the end, without forgetting what it had.
    const char* LockedVertex(unsigned int vertexArray, unsigned int vertex);

    static void SetHeap(void* heap);   // 0x00052CC0

    AtMesh* m_mesh;                    // +0x00
    unsigned int m_portalCount;        // +0x04
    AtCellPortal* m_portals;           // +0x08
    unsigned int m_cellCount;          // +0x0C
    AtCell* m_cellArray;               // +0x10
    unsigned int m_exteriorPortalCount;  // +0x14
    AtCellPortal* m_exteriorPortals;   // +0x18
    GmNameMap m_portalNames;           // +0x1C
    GmNameMap m_cells;                 // +0x30 what a space's Cell word looks in
    int m_unknown44;                   // +0x44

    // While one graph's portals are being read. All instances share them.
    static char* s_graphNames;         // 0x002C7EA0 eight bytes each
    static char* s_spaceNames;         // 0x002C7EA4
    static char* s_cellNames;          // 0x002C7EA8
    static unsigned int s_portalIndex; // 0x002C7E28
    static unsigned int s_lockedArray; // 0x002C7EAC
    static AtVertexLock s_lock;        // 0x002C7E30
    static void* s_heap;               // 0x002C7EB0 for the portals and cells
};

// Vftable 0x00276148. What one .ASC file holds.
class AtCellGraphDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 12 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B2120

    AtCellGraphDatabase();                  // 0x0003FC40
    ~AtCellGraphDatabase() override;        // slot 0; 0x0002EAC0 vector deleting, 0x0003FEE0 complete
    void Clear() override;                  // slot 1, 0x0003FEA0

    void Load(AtNameTableOwner* owner, const char* name, int unused);  // 0x0003FE00 name + ".ASC"
    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);      // 0x0003FC60 clears first if read before. One list, and nothing after it is read.

    static void SetHeap(void* heap);        // 0x0003FC30

    unsigned int m_count;                   // +0x14
    AtCellGraph* m_graphs;                  // +0x18

    static void* s_heap;                    // 0x002C641C
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtCellPortal) == 0x34, "AtCellPortal");
static_assert(offsetof(AtCellPortal, m_flags) == 0x08, "AtCellPortal::m_flags");
static_assert(offsetof(AtCellPortal, m_closedDistanceSquared) == 0x24, "AtCellPortal::m_closedDistanceSquared");
static_assert(offsetof(AtCellPortal, m_vertexArray) == 0x32, "AtCellPortal::m_vertexArray");
static_assert(sizeof(AtCell) == 0x18, "AtCell");
static_assert(sizeof(AtCellGraph) == 0x48, "AtCellGraph");
static_assert(offsetof(AtCellGraph, m_portalNames) == 0x1C, "AtCellGraph::m_portalNames");
static_assert(offsetof(AtCellGraph, m_cells) == 0x30, "AtCellGraph::m_cells");
static_assert(sizeof(AtCellGraphDatabase) == 0x1C, "AtCellGraphDatabase");
#pragma pop_macro("offsetof")
#endif
