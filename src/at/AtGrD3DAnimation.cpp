// The Xbox animations: .XKA files, their keys, and the device that makes both; with the binary file they are read through. See include/at/AtGrD3DDatabase.h. Names inferred.
#include "at/AtGrD3DDatabase.h"

#include "at/AtText.h"
#include "AtIoJamCommon.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

AtIoContext* AtIoBinaryFile::s_context;
void* AtGrD3DAnimationDevice::s_heap;

// ---- The binary file ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

int AtIoBinaryFile::Open(const char* path, int flags) {
    AtIoFileSystem* fs;
    if (flags & 1) {
        fs = s_context->Top();
    } else if (s_context->FindFile(path, &fs) != 0) {
        return 1;
    }
    if (m_file != 0) {
        m_fs->Close(m_file);
        m_fs = 0;
        m_file = 0;
    }
    m_fs = fs;
    return fs->Open(path, flags, &m_file);
}

int AtIoBinaryFile::OpenWithExtension(const char* path, const char* ext, int flags) {
    char full[0x80];
    strcpy(full, path);
    char* leaf = strrchr(full, '\\');
    if (leaf == 0) {
        leaf = full;
    }
    if (strrchr(leaf, '.') == 0) {
        strcat(full, ".");
        strcat(full, ext);
    }
    return Open(full, flags);
}

int AtIoBinaryFile::ReadAll(void* out, unsigned int size, unsigned int* done) {
    AtIoFileRequest* request;
    int status = m_file->Read(out, size, done, &request);
    if (status == AtIoFile::kPending) {
        status = request->Wait();
        if (m_file != 0) {
            m_file->Release(request);
        }
    }
    return status;
}

int AtIoBinaryFile::Read(void* out, unsigned int size, unsigned int* done, AtIoFileRequest** request) {
    return m_file->Read(out, size, done, request);
}

int AtIoBinaryFile::Release(AtIoFileRequest* request) {
    if (m_file == 0) {
        return 0;
    }
    m_file->Release(request);
    return 1;
}

void AtIoBinaryFile::Close() {
    if (m_file != 0) {
        m_fs->Close(m_file);
    }
    m_fs = 0;
    m_file = 0;
}

// Not a function of the original: each reader writes this out at each block.
int AtReadBlock(AtIoBinaryFile* file, void* out, unsigned int size) {
    AtIoFileRequest* request;
    int status = file->Read(out, size, 0, &request);
    if (status == AtIoFile::kPending) {
        status = request->Wait();
        file->Release(request);
    }
    return status;
}

// ---- The keys -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrD3DAnimationKeys::AtGrD3DAnimationKeys()
    : m_frames(0), m_rotationKeys(0), m_translationKeys(0), m_scaleKeys(0), m_noteKeys(0), m_cameraKeys(0), m_unknown40(0), m_unknown44(0) {
}

AtGrD3DAnimationKeys::~AtGrD3DAnimationKeys() {
    Clear();
}

void AtGrD3DAnimationKeys::Clear() {
    // Lists a file gave are in the database's block: forgotten first, so that the frees below free nothing.
    if (m_flags & 0x400) {
        m_frames = 0;
        m_rotationKeys = 0;
        m_translationKeys = 0;
        m_scaleKeys = 0;
        m_noteKeys = 0;
        m_cameraKeys = 0;
    }
    AtAnimationKeys::Clear();
    m_frames = 0;
    m_rotationKeys = 0;
    m_translationKeys = 0;
    m_scaleKeys = 0;
    m_noteKeys = 0;
    m_cameraKeys = 0;
    m_unknown40 = 0;
    m_unknown44 = 0;
}

void AtGrD3DAnimationKeys::Free() {
    operator delete(m_frames);
}

void AtGrD3DAnimationKeys::Slot14() {
    operator delete(m_rotationKeys);
}

void AtGrD3DAnimationKeys::Slot15() {
    operator delete(m_translationKeys);
}

void AtGrD3DAnimationKeys::Slot16() {
    operator delete(m_scaleKeys);
}

void AtGrD3DAnimationKeys::Slot17() {
    operator delete(m_noteKeys);
}

void AtGrD3DAnimationKeys::Slot18() {
    operator delete(m_cameraKeys);
}

void AtGrD3DAnimationKeys::ReadBinary(const AtXkaHeader* header, char* data) {
    Init(0, 0, 0, 0, 0, 0);
    m_count = header->m_frameCount;
    m_frames = (unsigned short*)(data + header->m_frameOffset);
    m_rotationKeyCount = header->m_rotationKeyCount;
    m_rotationKeys = data + header->m_rotationKeyOffset;
    m_translationKeyCount = header->m_translationKeyCount;
    m_translationKeys = data + header->m_translationKeyOffset;
    m_scaleKeyCount = header->m_scaleKeyCount;
    m_scaleKeys = data + header->m_scaleKeyOffset;
    m_noteKeyCount = header->m_noteKeyCount;
    m_noteKeys = data + header->m_noteKeyOffset;
    m_cameraKeyCount = header->m_cameraKeyCount;
    m_cameraKeys = data + header->m_cameraKeyOffset;
    m_flags |= header->m_keyFlags | 0x400;
    m_unknown40 = header->m_keyUnknown14;
    m_unknown44 = header->m_keyUnknown18;
}

// ---- The database ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrD3DAnimationDatabase::AtGrD3DAnimationDatabase() : m_data(0) {
}

AtGrD3DAnimationDatabase::~AtGrD3DAnimationDatabase() {
    Clear();
}

void AtGrD3DAnimationDatabase::Clear() {
    // The component lists are in the block: forgotten, so that the base's Clear does not free them.
    if (m_data != 0) {
        operator delete(m_data);
        m_rotation = 0;
        m_translation = 0;
        m_scale = 0;
        m_note = 0;
        m_camera = 0;
    }
    AtAnimationDatabase::Clear();
    m_data = 0;
}

int AtGrD3DAnimationDatabase::HasBinaryForm() {
    return 1;
}

void AtGrD3DAnimationDatabase::LoadBinaryForm(AtAnimationDevice* device, const char* name) {
    AtIoBinaryFile file;
    int status = file.OpenWithExtension(name, "XKA", 2);
    if (status != 0) {
        AtReportFileError(status, name, "XKA");
    }
    char unread[0x10];
    status = file.ReadAll(unread, sizeof unread, 0);
    if (status != 0) {
        AtReportFileError(status, name, "XKA");
    }
    AtXkaHeader header;
    status = file.ReadAll(&header, sizeof header, 0);
    if (status != 0) {
        AtReportFileError(status, name, "XKA");
    }
    file.m_file->Seek((file.m_file->m_position + 3) & ~3u);

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
        AtReportFileError(status, name, "XKA");
    }

    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_data = (char*)operator new(header.m_keptSize);
    AtPopHeap();
    if (m_data == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    status = AtReadBlock(&file, m_data, header.m_keptSize);
    if (status != 0) {
        AtReportFileError(status, name, "XKA");
    }

    m_device = device;
    m_keys = device->NewKeys();
    static_cast<AtGrD3DAnimationKeys*>(m_keys)->ReadBinary(&header, m_data);
    m_rotationCount = header.m_rotationCount;
    m_translationCount = header.m_translationCount;
    m_scaleCount = header.m_scaleCount;
    m_noteCount = header.m_noteCount;
    m_cameraCount = header.m_cameraCount;
    m_animationCount = header.m_animationCount;
    m_rotation = (AtAnimationComponent*)(m_data + header.m_rotationOffset);
    m_translation = (AtAnimationComponent*)(m_data + header.m_translationOffset);
    m_scale = (AtAnimationComponent*)(m_data + header.m_scaleOffset);
    m_note = (AtAnimationComponent*)(m_data + header.m_noteOffset);
    m_camera = (AtAnimationComponent*)(m_data + header.m_cameraOffset);

    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_animations = new AtAnimation[m_animationCount];
    AtPopHeap();
    if (m_animations == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    Init(m_animationCount);
    const char* names = scratch + header.m_namesOffset;
    const AtXkaAnimation* records = (const AtXkaAnimation*)(scratch + header.m_recordsOffset);
    for (unsigned int i = 0; i < m_animationCount; i++) {
        Insert(names + i * 8, &m_animations[i]);
        m_animations[i].ReadBinary(this, &records[i]);
    }
    operator delete(scratch);
    file.Close();
}

// ---- The device -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrD3DAnimationDevice::AtGrD3DAnimationDevice() {
}

AtGrD3DAnimationDevice::~AtGrD3DAnimationDevice() {
}

AtAnimationKeys* AtGrD3DAnimationDevice::NewKeys() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    AtGrD3DAnimationKeys* keys = new AtGrD3DAnimationKeys;
    AtPopHeap();
    if (keys == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return keys;
}

void AtGrD3DAnimationDevice::FreeKeys(AtAnimationKeys* keys) {
    delete static_cast<AtGrD3DAnimationKeys*>(keys);
}

AtAnimationDatabase* AtGrD3DAnimationDevice::NewDatabase() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    AtGrD3DAnimationDatabase* database = new AtGrD3DAnimationDatabase;
    AtPopHeap();
    if (database == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return database;
}

void AtGrD3DAnimationDevice::FreeDatabase(AtAnimationDatabase* database) {
    delete database;
}
