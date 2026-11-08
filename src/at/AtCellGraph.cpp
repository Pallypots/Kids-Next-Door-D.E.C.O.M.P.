// See include/at/AtCellGraph.h.
#include "at/AtCellGraph.h"

#include "at/AtScene.h"
#include "AtSceneArray.h"
#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int file, int line, int unused);

char* AtCellGraph::s_graphNames;
char* AtCellGraph::s_spaceNames;
char* AtCellGraph::s_cellNames;
unsigned int AtCellGraph::s_portalIndex;
unsigned int AtCellGraph::s_lockedArray;
AtVertexLock AtCellGraph::s_lock;
void* AtCellGraph::s_heap;
void* AtCellGraphDatabase::s_heap;

// 0x002B2120.
const AtIoKeyword AtCellGraphDatabase::s_keywords[AtCellGraphDatabase::kKeywordCount] = {
    {0x39, "NameSpace"}, {0x3A, "CellGraph"}, {0x3B, "Mesh"}, {0x3C, "Portal"}, {0x3D, "OppositeCell"}, {0x3E, "OppositeCellDifferentGraph"}, {0x3F, "Vertices"}, {0x40, "Closed"},
    {0x41, "ClosedByViewerDist"}, {0x42, "Cell"}, {0x43, "Portals"}, {0x44, "ExteriorPortals"},
};

namespace {

enum Word {
    kNameSpace = 0x39, kCellGraphWord = 0x3A, kMesh = 0x3B, kPortal = 0x3C, kOppositeCell = 0x3D, kOppositeCellDifferentGraph = 0x3E, kVertices = 0x3F, kClosed = 0x40, kClosedByViewerDistWord = 0x41,
    kCellWord = 0x42, kPortals = 0x43, kExteriorPortals = 0x44
};

// Codes passed to the tokenizer's Error; the words for them are inferred from where they are used.
enum Problem { kOutOfRange = 0x23, kNotFound = 0x25 };

char* NewNames(unsigned int count) {
    if (AtIoTokenFile::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtIoTokenFile::s_heap);
    char* names = (char*)operator new(count * 8);
    AtPopHeap();
    if (names == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return names;
}

}  // namespace

// ---- what a portal asks of a mesh -------------------------------------------------------------------------------

AtVertexArray* AtMesh::VertexArray(unsigned int index) {
    if (m_ownVertexArrays == 0) {
        return m_database->VertexArrayAt(index);
    }
    if (index >= m_ownVertexArrays) {
        AtFatal("Atlas Graphics: vertex array reference out of bounds", 0, 0, 0);
    }
    return VertexArrayAt(index);
}

void AtVertexArray::Lock(AtVertexLock*, int) {
    AtFatal("not recovered: a concrete vertex array's lock, in the graphics layer", 0, 0, 0);
}

void AtNameTableOwner::AddCellGraphs(GmNameMap* map, const char* ns) {
    if (ns == 0) {
        m_cellTables.Add(map);
    } else {
        m_cellTableSpaces.Add(ns, map);
    }
}

void AtNameTableOwner::AddSpaceGraphs(GmNameMap* map) {
    m_spaceGraphs.Add(map);
}

// ---- a portal ---------------------------------------------------------------------------------------------------

AtCellPortal::AtCellPortal() {
    m_opposite = 0;
    m_oppositeGraph = 0;
    m_flags = 0;
    m_center[0] = 0;
    m_center[1] = 0;
    m_center[2] = 0;
    m_normal[0] = 0;
    m_normal[1] = 0;
    m_normal[2] = 0;
    m_closedDistanceSquared = 0;
    m_vertices = 0;
    m_vertexCount = 0;
    m_stride = 0;
    m_firstVertex = 0;
    m_vertexArray = 0;
}

void AtCellPortal::Read(AtCellGraph* graph, AtIoTokenizer* tokenizer) {
    if (m_flags & 1) {
        new (this, AtPlacement()) AtCellPortal;
    }
    tokenizer->ExpectLeftCurly();
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kRightCurly) {
        switch (token) {
        case kOppositeCellDifferentGraph:
            tokenizer->ExpectName(AtCellGraph::s_graphNames + AtCellGraph::s_portalIndex * 8);
            tokenizer->ExpectName(AtCellGraph::s_spaceNames + AtCellGraph::s_portalIndex * 8);
            tokenizer->ExpectName(AtCellGraph::s_cellNames + AtCellGraph::s_portalIndex * 8);
            break;
        case kOppositeCell:
            tokenizer->ExpectName(AtCellGraph::s_cellNames + AtCellGraph::s_portalIndex * 8);
            break;
        case kVertices:
            m_vertexArray = (unsigned short)tokenizer->ExpectInt();
            m_firstVertex = (unsigned short)tokenizer->ExpectInt();
            m_vertices = graph->LockedVertex(m_vertexArray, m_firstVertex);
            m_vertexCount = (unsigned short)tokenizer->ExpectInt();
            if (m_vertexCount >= 0x20 || m_vertexCount < 3) {
                tokenizer->Error(kOutOfRange);
            }
            m_stride = (unsigned short)AtCellGraph::s_lock.m_stride;
            break;
        case kClosed:
            m_flags |= kClosedBit1 | kClosedBit3;
            break;
        case kClosedByViewerDistWord: {
            m_flags |= kClosedByViewerDist;
            const float distance = tokenizer->ExpectReal();
            m_closedDistanceSquared = distance * distance;
            break;
        }
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        token = tokenizer->Next();
    }
    ComputeCenter();
    ComputeNormal();
}

// The source will have been three floats summed in a plain loop. The compiler's unrolling by four keeps some running sums in x87 registers across some of the additions, so which sums are cut to float, and when, shows in the low bits of
// the result. That is kept here: a double is what stayed in a register, a float what was stored.
void AtCellPortal::ComputeCenter() {
    double sumX = 0.0;
    float sumY = 0;
    float sumZ = 0;
    const char* at = m_vertices;
    const int count = m_vertexCount;
    int i = 0;
    for (; i + 4 <= count; i += 4) {
        const float* v0 = (const float*)at;
        const float* v1 = (const float*)(at + m_stride);
        const float* v2 = (const float*)(at + m_stride * 2);
        const float* v3 = (const float*)(at + m_stride * 3);
        sumX += v0[0];
        double y = (double)sumY + v0[1];
        sumZ = (float)((double)sumZ + v0[2]);
        float x = (float)(sumX + v1[0]);
        y += v1[1];
        const double z = (double)sumZ + v1[2];
        x = (float)((double)x + v2[0]);
        sumY = (float)(y + v2[1]);
        sumZ = (float)(z + v2[2]);
        sumX = (double)x + v3[0];
        sumY = (float)((double)sumY + v3[1]);
        sumZ = (float)((double)sumZ + v3[2]);
        at += m_stride * 4;
    }
    for (; i < count; i++) {
        const float* v = (const float*)at;
        sumX += v[0];
        sumY = (float)((double)sumY + v[1]);
        sumZ = (float)((double)sumZ + v[2]);
        at += m_stride;
    }
    const double inverse = 1.0 / (double)count;
    const float stored = (float)inverse;
    m_center[0] = (float)(inverse * sumX);
    m_center[1] = (float)((double)stored * sumY);
    m_center[2] = (float)((double)stored * sumZ);
}

// (v2 - v0) x (v1 - v0). Likewise a double is what stayed in a register.
void AtCellPortal::ComputeNormal() {
    const float* v0 = (const float*)m_vertices;
    const float* v1 = (const float*)(m_vertices + m_stride);
    const float* v2 = (const float*)(m_vertices + m_stride * 2);
    const double ax = (double)v2[0] - v0[0];
    const double ay = (double)v2[1] - v0[1];
    const double az = (double)v2[2] - v0[2];
    const float bx = (float)((double)v1[0] - v0[0]);
    const float by = (float)((double)v1[1] - v0[1]);
    const double bz = (double)v1[2] - v0[2];
    m_normal[0] = (float)(bz * ay - by * az);
    m_normal[1] = (float)(bx * az - bz * ax);
    m_normal[2] = (float)(by * ax - bx * ay);
}

void AtCellPortal::Resolve(AtNameTableOwner* owner, AtCellGraph* graph, const char* graphName, const char* ns, const char* cellName) {
    if (cellName[0] == 0) {
        return;
    }
    if (graphName[0] != 0) {
        if (ns != 0 && ns[0] != 0) {
            m_oppositeGraph = (AtCellGraph*)owner->m_cellTableSpaces.Find(ns, graphName);
        } else {
            m_oppositeGraph = (AtCellGraph*)owner->m_cellTables.Find(graphName);
        }
        if (m_oppositeGraph == 0) {
            AtFatal("Atlas Scene: invalid cell graph reference in portal", 0, 0, 0);
        }
    } else {
        m_oppositeGraph = graph;
    }
    m_opposite = (AtCell*)m_oppositeGraph->m_cells.Find(cellName);
    if (m_opposite == 0) {
        AtFatal("Atlas Scene: invalid cell reference in portal", 0, 0, 0);
    }
}

// ---- a cell -----------------------------------------------------------------------------------------------------

AtCell::AtCell() {
    m_graph = 0;
    m_portals = 0;
    m_portalCount = 0;
    m_unknown0C = 0;
    m_unknown14 = 0;
}

void AtCell::Read(AtCellGraph* graph, AtIoTokenizer* tokenizer) {
    m_graph = graph;
    tokenizer->ExpectLeftCurly();
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kRightCurly) {
        if (token == kPortals) {
            const unsigned int first = (unsigned int)tokenizer->ExpectInt();
            m_portalCount = (unsigned int)tokenizer->ExpectInt();
            if (m_portalCount + first > graph->m_portalCount) {
                tokenizer->Error(kOutOfRange);
            }
            m_portals = graph->m_portals + first;
        } else {
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        }
        token = tokenizer->Next();
    }
}

// ---- a graph ----------------------------------------------------------------------------------------------------

void AtCellGraph::SetHeap(void* heap) {
    s_heap = heap;
}

AtCellGraph::AtCellGraph() {
    m_mesh = 0;
    m_portalCount = 0;
    m_portals = 0;
    m_cellCount = 0;
    m_cellArray = 0;
    m_exteriorPortalCount = 0;
    m_exteriorPortals = 0;
    m_unknown44 = 0;
}

AtCellGraph::~AtCellGraph() {
    Clear();
}

void AtCellGraph::Clear() {
    m_cells.Clear();
    if (m_cellArray != 0) {
        operator delete(m_cellArray);
    }
    m_portalNames.Clear();
    if (m_portals != 0) {
        operator delete(m_portals);
    }
    m_mesh = 0;
    m_portalCount = 0;
    m_portals = 0;
    m_cellCount = 0;
    m_cellArray = 0;
    m_exteriorPortalCount = 0;
    m_exteriorPortals = 0;
    m_unknown44 = 0;
}

const char* AtCellGraph::LockedVertex(unsigned int vertexArray, unsigned int vertex) {
    if (vertexArray != s_lockedArray || s_lock.m_data == 0) {
        if (vertexArray != s_lockedArray && s_lock.m_data != 0) {
            m_mesh->VertexArray(s_lockedArray)->Slot6();
        }
        s_lockedArray = vertexArray;
        m_mesh->VertexArray(vertexArray)->Lock(&s_lock, 0);
    }
    return (const char*)s_lock.m_data + s_lock.m_stride * vertex;
}

void AtCellGraph::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_cellArray != 0) {
        Clear();
    }
    tokenizer->ExpectLeftCurly();
    s_lock.m_data = 0;
    int token = tokenizer->Next();
    if (token == kMesh) {
        char name[8];
        char ns[8];
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        AtSceneDevice* device = owner->m_device;
        if (ns[0] != 0) {
            m_mesh = (AtMesh*)device->m_meshSpaces.Find(ns, name);
        } else {
            m_mesh = (AtMesh*)device->m_meshes.Find(name);
        }
        if (m_mesh == 0) {
            tokenizer->Error(kNotFound);
        }
        token = tokenizer->Next();
        if (token == kPortal) {
            m_portalCount = (unsigned int)tokenizer->ExpectCount();
            if (m_portalCount < 1) {
                tokenizer->Error(kOutOfRange);
            }
            if (s_heap == 0) {
                AtRaise(-2, 0, 0, 0);
            }
            AtPushHeap(s_heap);
            {
                const unsigned int count = m_portalCount;
                AtCellPortal* portals = (AtCellPortal*)operator new(count * sizeof(AtCellPortal));
                if (portals != 0) {
                    for (unsigned int i = 0; i < count; i++) {
                        new (portals + i, AtPlacement()) AtCellPortal;
                    }
                }
                m_portals = portals;
            }
            AtPopHeap();
            if (m_portals == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            m_portalNames.Init(m_portalCount);
            s_graphNames = NewNames(m_portalCount);
            s_spaceNames = NewNames(m_portalCount);
            s_cellNames = NewNames(m_portalCount);
            for (s_portalIndex = 0; s_portalIndex < m_portalCount; s_portalIndex++) {
                s_graphNames[s_portalIndex * 8] = 0;
                s_spaceNames[s_portalIndex * 8] = 0;
                s_cellNames[s_portalIndex * 8] = 0;
                tokenizer->ExpectToken(kPortal);
                tokenizer->ExpectName(name);
                m_portalNames.Insert(name, m_portals + s_portalIndex);
                m_portals[s_portalIndex].Read(this, tokenizer);
            }
            tokenizer->ExpectRightCurly();
            token = tokenizer->Next();
        }
    }
    if (token != kCellWord) {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }
    m_cellCount = (unsigned int)tokenizer->ExpectCount();
    if (m_cellCount < 1) {
        tokenizer->Error(kOutOfRange);
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    {
        const unsigned int count = m_cellCount;
        AtCell* cells = (AtCell*)operator new(count * sizeof(AtCell));
        if (cells != 0) {
            for (unsigned int i = 0; i < count; i++) {
                new (cells + i, AtPlacement()) AtCell;
            }
        }
        m_cellArray = cells;
    }
    AtPopHeap();
    if (m_cellArray == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_cells.Init(m_cellCount);
    for (unsigned int i = 0; i < m_cellCount; i++) {
        char name[8];
        tokenizer->ExpectToken(kCellWord);
        tokenizer->ExpectName(name);
        m_cells.Insert(name, m_cellArray + i);
        m_cellArray[i].Read(this, tokenizer);
    }
    tokenizer->ExpectRightCurly();
    token = tokenizer->Next();
    if (token == kExteriorPortals) {
        const unsigned int first = (unsigned int)tokenizer->ExpectInt();
        m_exteriorPortalCount = (unsigned int)tokenizer->ExpectInt();
        if (m_exteriorPortalCount + first > m_portalCount) {
            tokenizer->Error(kOutOfRange);
        }
        m_exteriorPortals = m_portals + first;
        token = tokenizer->Next();
    }
    if (token != AtIoTokenizer::kRightCurly) {
        tokenizer->Error(AtIoTokenizer::kExpectedRightCurly);
    }
    if (s_cellNames != 0) {
        for (s_portalIndex = 0; s_portalIndex < m_portalCount; s_portalIndex++) {
            m_portals[s_portalIndex].Resolve(owner, this, s_graphNames + s_portalIndex * 8, s_spaceNames + s_portalIndex * 8, s_cellNames + s_portalIndex * 8);
        }
        operator delete(s_cellNames);
        operator delete(s_graphNames);
        operator delete(s_spaceNames);
        s_cellNames = 0;
        s_graphNames = 0;
        s_spaceNames = 0;
    }
    if (s_lock.m_data != 0) {
        m_mesh->VertexArray(s_lockedArray)->Slot6();
    }
}

// ---- a file -----------------------------------------------------------------------------------------------------

void AtCellGraphDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtCellGraphDatabase::AtCellGraphDatabase() {
    m_count = 0;
    m_graphs = 0;
}

AtCellGraphDatabase::~AtCellGraphDatabase() {
    AtCellGraphDatabase::Clear();
}

void AtCellGraphDatabase::Clear() {
    if (m_graphs != 0) {
        AtDeleteArray(m_graphs);
    }
    GmNameMap::Clear();
    m_count = 0;
    m_graphs = 0;
}

void AtCellGraphDatabase::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_entries != 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    char ns[8];
    const char* space = 0;
    if (tokenizer->Next() == kNameSpace) {
        tokenizer->ExpectName(ns);
        tokenizer->Next();
        space = ns;
    }
    m_count = (unsigned int)tokenizer->ExpectCount();
    if (m_count < 1) {
        tokenizer->Error(kOutOfRange);
    }
    Init(m_count);
    owner->AddCellGraphs(this, space);
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_graphs = AtNewArray<AtCellGraph>(m_count);
    AtPopHeap();
    if (m_graphs == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < m_count; i++) {
        char name[8];
        tokenizer->ExpectToken(kCellGraphWord);
        tokenizer->ExpectName(name);
        Insert(name, m_graphs + i);
        m_graphs[i].Read(owner, tokenizer);
    }
    tokenizer->ExpectRightCurly();
}

void AtCellGraphDatabase::Load(AtNameTableOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "ASC", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "ASC");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}
