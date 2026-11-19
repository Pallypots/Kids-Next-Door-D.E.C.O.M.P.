// The Xbox mesh database and its .XGG reader, with the base's Allocate that the reader calls, and the owner's list of mesh databases. See include/at/AtGrD3DDatabase.h. Names inferred.
#include "at/AtGrD3DDatabase.h"

#include "at/AtHeap.h"
#include "at/AtText.h"
#include "AtIoJamCommon.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

void* AtGrD3DMeshDatabase::s_heap;

// ---- The owner's list ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void AtGrD3DOwner::AddMeshDatabase(AtGrD3DMeshDatabase* database) {
    database->m_next = m_meshDatabases;
    m_meshDatabases = database;
}

void AtGrD3DOwner::RemoveMeshDatabase(AtGrD3DMeshDatabase* database) {
    AtGrD3DMeshDatabase** link = &m_meshDatabases;
    for (AtGrD3DMeshDatabase* at = *link; at != 0; at = *link) {
        if (at == database) {
            *link = database->m_next;
            return;
        }
        link = &at->m_next;
    }
}

// ---- The base's room for a file -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void AtMeshDatabase::Allocate(AtMaterialOwner* owner, unsigned int meshes, unsigned int vertexArrays, unsigned int indexArrays, const char* ns) {
    if (m_meshCount > 0) {
        Clear();
    }
    m_vertexArrayCount = vertexArrays;
    m_meshCount = meshes;
    m_indexArrayCount = indexArrays;
    Init(meshes);
    if (m_vertexArrayCount > 0) {
        MakeVertexArrays(owner);
    }
    if (m_indexArrayCount > 0) {
        MakeIndexArrays(owner);
    }
    MakeMeshes(owner);
    owner->AddMeshes(this, ns);
}

// ---- The database ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// m_streamCount is left as it was: neither the constructor nor Clear sets it.
AtGrD3DMeshDatabase::AtGrD3DMeshDatabase()
    : m_owner(0), m_meshes(0), m_vertexArrays(0), m_indexArrays(0), m_streams(0), m_kept(0), m_keptSize(0), m_contiguous(0), m_contiguousSize(0), m_next(0) {
}

AtGrD3DMeshDatabase::~AtGrD3DMeshDatabase() {
    Clear();
}

void AtGrD3DMeshDatabase::Clear() {
    if (m_owner != 0) {
        m_owner->RemoveMeshDatabase(this);
        m_owner = 0;
    }
    if (m_meshes != 0) {
        delete[] m_meshes;
        m_meshes = 0;
    }
    if (m_vertexArrays != 0) {
        delete[] m_vertexArrays;
    }
    if (m_indexArrays != 0) {
        delete[] m_indexArrays;
    }
    if (m_streams != 0) {
        delete[] m_streams;
    }
    if (m_kept != 0) {
        operator delete(m_kept);
    }
    if (m_contiguous != 0) {
        AtGrD3DFreeContiguous(m_contiguous);
    }
    AtMeshDatabase::Clear();
    m_meshes = 0;
    m_owner = 0;
    m_vertexArrays = 0;
    m_indexArrays = 0;
    m_streams = 0;
    m_kept = 0;
    m_keptSize = 0;
    m_contiguous = 0;
    m_contiguousSize = 0;
}

void AtGrD3DMeshDatabase::MakeMeshes(AtMaterialOwner* owner) {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_meshes = new AtGrD3DMesh[m_meshCount];
    AtPopHeap();
    if (m_meshes == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_owner = static_cast<AtGrD3DOwner*>(owner);
    m_owner->AddMeshDatabase(this);
}

void AtGrD3DMeshDatabase::MakeVertexArrays(AtMaterialOwner* owner) {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_vertexArrays = new AtGrD3DVertexArray[m_vertexArrayCount];
    AtPopHeap();
    if (m_vertexArrays == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (AtGrD3DVertexArray* array = m_vertexArrays; array < m_vertexArrays + m_vertexArrayCount; array++) {
        array->m_owner = static_cast<AtGrD3DOwner*>(owner);
    }
}

void AtGrD3DMeshDatabase::MakeIndexArrays(AtMaterialOwner* owner) {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_indexArrays = new AtGrD3DIndexArray[m_indexArrayCount];
    AtPopHeap();
    if (m_indexArrays == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (AtGrD3DIndexArray* array = m_indexArrays; array < m_indexArrays + m_indexArrayCount; array++) {
        array->m_owner = static_cast<AtGrD3DOwner*>(owner);
    }
}

void AtGrD3DMeshDatabase::MakeStreams() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_streams = new AtGrD3DVertexStream[m_streamCount];
    AtPopHeap();
    if (m_streams == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}

int AtGrD3DMeshDatabase::HasBinaryForm() {
    return 1;
}

void AtGrD3DMeshDatabase::LoadBinaryForm(AtMaterialOwner* owner, const char* name) {
    AtAudioBankFile file;
    int status = file.OpenWithExtension(name, "XGG", 2);
    if (status != 0) {
        AtReportFileError(status, name, "XGG");
    }
    char unread[0x10];
    status = file.Read(unread, sizeof unread, 0);
    if (status != 0) {
        AtReportFileError(status, name, "XGG");
    }
    AtXggHeader header;
    status = file.Read(&header, sizeof header, 0);
    if (status != 0) {
        AtReportFileError(status, name, "XGG");
    }
    file.m_file->Seek((file.m_file->m_position + 3) & ~3u);

    // The first block: the records, freed at the end.
    if (AtIoTokenFile::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtIoTokenFile::s_heap);
    char* scratch = (char*)operator new(header.m_scratchSize);
    AtPopHeap();
    if (scratch == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    status = AtReadBlock(&file, scratch, header.m_scratchSize);
    if (status != 0) {
        AtReportFileError(status, name, "XGG");
    }

    // The second, kept. The heap is not tested for null here, and it is asked for 32-byte alignment at least while the block is allocated.
    m_keptSize = header.m_keptSize;
    AtHeap* heap = (AtHeap*)s_heap;
    if (heap->m_alignShift < 5) {
        heap->m_allocShift = 5;
    } else {
        heap->m_allocShift = heap->m_alignShift;
    }
    AtPushHeap(s_heap);
    m_kept = (char*)operator new(m_keptSize);
    AtPopHeap();
    if (m_kept == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    heap = (AtHeap*)s_heap;
    heap->m_allocShift = heap->m_alignShift;
    status = AtReadBlock(&file, m_kept, m_keptSize);
    if (status != 0) {
        AtReportFileError(status, name, "XGG");
    }

    // The third, for the graphics processor, if there is one.
    m_contiguousSize = header.m_contiguousSize;
    if (m_contiguousSize > 0) {
        m_contiguous = AtGrD3DAllocateContiguous(m_contiguousSize, 4);
        status = AtReadBlock(&file, m_contiguous, m_contiguousSize);
        if (status != 0) {
            AtReportFileError(status, name, "XGG");
        }
    }
    file.Close();

    Allocate(owner, header.m_meshCount, 0, 0, header.m_nameSpace[0] != 0 ? header.m_nameSpace : 0);
    m_vertexArrayCount = header.m_vertexArrayCount;
    m_streamCount = header.m_streamCount;
    m_indexArrayCount = header.m_indexArrayCount;

    MakeVertexArrays(owner);
    char* records = scratch + header.m_vertexArrayOffset;
    char* kept = m_kept + header.m_vertexArrayDataOffset;
    char* contiguous = (char*)m_contiguous + header.m_vertexDataOffset;
    for (unsigned int i = 0; i < m_vertexArrayCount; i++) {
        m_vertexArrays[i].ReadBinary(&records, &kept, &contiguous);
    }

    MakeIndexArrays(owner);
    records = scratch + header.m_indexArrayOffset;
    kept = m_kept + header.m_indexDataOffset;
    for (unsigned int i = 0; i < m_indexArrayCount; i++) {
        m_indexArrays[i].ReadBinary(&records, &kept);
    }

    MakeStreams();
    records = scratch + header.m_streamOffset;
    kept = m_kept + header.m_streamDataOffset;
    for (unsigned int i = 0; i < m_streamCount; i++) {
        m_streams[i].ReadBinary(owner, &records, &kept);
    }

    char* triangles = m_kept + header.m_triangleOffset;
    char* parts = m_kept + header.m_partOffset;
    char* materials = scratch + header.m_materialRefOffset;
    records = scratch + header.m_meshOffset;
    kept = m_kept + header.m_meshKeptOffset;
    contiguous = (char*)m_contiguous + header.m_meshContiguousOffset;
    char meshName[12];  // The mesh copies its record's name here, as a string: a record leaves twelve bytes for it.
    for (unsigned int i = 0; i < m_meshCount; i++) {
        m_meshes[i].ReadBinary(owner, this, &triangles, &parts, &materials, &records, &kept, &contiguous, meshName);
        Insert(meshName, &m_meshes[i]);
    }
    operator delete(scratch);
}

// m_more is made from the mesh heap, a whole database inline, and given this one's owner, mesh count and the name of its group, as a string copied without bound.
void AtGrD3DMeshDatabase::Slot8() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_more = new AtGrD3DMeshDatabase;
    AtPopHeap();
    if (m_more == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    // ORIGINAL QUIRK. The group's name is eight bytes with no terminator when it is eight long, and strcpy goes on into the group's next fields up to a zero byte. The original's buffer is twelve bytes of its frame,
    // followed by its exception record, which a long copy overwrites. Here the buffer is long enough for the group's whole record; only the first eight bytes reach the name space.
    char ns[0x20];
    if (m_group == 0) {
        ns[0] = 0;
    } else {
        strcpy(ns, m_group->m_name);
    }
    m_more->Allocate(m_owner, m_meshCount, 0, 0, ns);
}

AtMesh* AtGrD3DMeshDatabase::MeshAt(unsigned int index) {
    return &m_meshes[index];
}

AtVertexArray* AtGrD3DMeshDatabase::VertexArrayAt(unsigned int index) {
    return &m_vertexArrays[index];
}

AtIndexArray* AtGrD3DMeshDatabase::IndexArrayAt(unsigned int index) {
    return &m_indexArrays[index];
}
