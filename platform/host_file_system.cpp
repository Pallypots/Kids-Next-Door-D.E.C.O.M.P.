// See host_file_system.h. Port code, not recovered code.
#include "host_file_system.h"

#include <cstring>

AtIoHostFile::AtIoHostFile() : m_handle(nullptr) {}

AtIoHostFile::~AtIoHostFile() {
    Close();
}

int AtIoHostFile::Close() {
    if (m_handle) {
        std::fclose(m_handle);
        m_handle = nullptr;
    }
    return ResetFile();
}

int AtIoHostFile::Open(const char* path, const char* displayName, int flags) {
    Close();
    m_handle = std::fopen(path, "rb");
    if (!m_handle) {
        return kNotFound;
    }
    std::fseek(m_handle, 0, SEEK_END);
    m_size = m_sizeCopy = (unsigned int)std::ftell(m_handle);
    std::fseek(m_handle, 0, SEEK_SET);
    m_position = 0;
    m_flags = flags | kOpen;
    std::strncpy(m_path, displayName, sizeof m_path - 1);
    return kOk;
}

int AtIoHostFile::Clear() {
    return 0;
}

int AtIoHostFile::Seek(unsigned int offset) {
    if (!m_handle || offset > m_size) {
        return kIoError;
    }
    m_position = offset;
    return std::fseek(m_handle, (long)offset, SEEK_SET) == 0 ? kOk : kIoError;
}

// Always completes at once. The recovered callers also handle kPending, for a device that does not.
int AtIoHostFile::Read(void* out, unsigned int size, unsigned int* done, AtIoFileRequest** request) {
    if (request) {
        *request = nullptr;
    }
    if (!m_handle) {
        return kNotFound;
    }
    const size_t got = std::fread(out, 1, size, m_handle);
    m_position += (unsigned int)got;
    if (done) {
        *done = (unsigned int)got;
    }
    return got == size ? kOk : kIoError;
}

int AtIoHostFile::Write(const void*, unsigned int, unsigned int* done) {
    if (done) {
        *done = 0;
    }
    return kUnsupported;
}

int AtIoHostFile::Flush() { return kUnsupported; }
int AtIoHostFile::Unknown8(void*) { return kUnsupported; }
int AtIoHostFile::Release(AtIoFileRequest*) { return 0; }

AtIoHostFileSystem::AtIoHostFileSystem(const std::string& root) : m_hostRoot(root) {}
AtIoHostFileSystem::~AtIoHostFileSystem() {}

std::string AtIoHostFileSystem::Full(const char* path) const {
    std::string full = m_hostRoot + "/" + path;
    for (char& c : full) {
        if (c == '\\') c = '/';
    }
    return full;
}

int AtIoHostFileSystem::Open(const char* path, int flags, AtIoFile** out) {
    AtIoHostFile* file = new AtIoHostFile;
    const std::string full = Full(path);
    const int status = file->Open(full.c_str(), path, flags);
    if (status != AtIoFile::kOk) {
        delete file;
        *out = nullptr;
        return status;
    }
    *out = file;
    return 0;
}

int AtIoHostFileSystem::Close(AtIoFile* file) {
    delete file;
    return 0;
}

int AtIoHostFileSystem::IsDirectoryMissing(const char*) { return 1; }

int AtIoHostFileSystem::IsFileMissing(const char* path) {
    std::FILE* f = std::fopen(Full(path).c_str(), "rb");
    if (!f) {
        return 1;
    }
    std::fclose(f);
    return 0;
}

int AtIoHostFileSystem::GetEntryType(const char* path, int* type) {
    if (IsFileMissing(path)) {
        return 1;
    }
    *type = AtIoJamHeader::kEntryIsFile;
    return 0;
}

int AtIoHostFileSystem::Unknown7(void*) { return 0; }

int AtIoHostFileSystem::GetFileInfo(const char* path, AtIoFileStat* stat) {
    std::FILE* f = std::fopen(Full(path).c_str(), "rb");
    if (!f) {
        return AtIoFile::kNotFound;
    }
    std::fseek(f, 0, SEEK_END);
    stat->m_size = (unsigned int)std::ftell(f);
    std::fclose(f);
    stat->m_offset = 0;
    stat->m_time = 0;
    std::strncpy(stat->m_name, path, sizeof stat->m_name - 1);
    return 0;
}

void* AtIoHostFileSystem::List(const char*) { return nullptr; }
void AtIoHostFileSystem::FreeList(void*) {}
int AtIoHostFileSystem::Unknown11(void*) { return AtIoFile::kUnsupported; }
int AtIoHostFileSystem::Unknown12() { return 0; }
