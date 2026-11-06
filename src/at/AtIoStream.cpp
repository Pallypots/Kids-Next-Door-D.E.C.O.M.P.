// AtIoFileRef, AtIoStream, AtIoFileStream. Original address clusters 0x00018810-0x00018A87, 0x00031590-0x000317DD and
// 0x000222A0-0x000224BB. Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "at/AtIoTokenizer.h"
#include "AtIoJamCommon.h"

AtIoContext* AtIoFileRef::s_context;
void* AtIoStream::s_heap;

AtIoFileRef::AtIoFileRef() {
    m_fs = 0;
    m_file = 0;
}

AtIoFileRef::~AtIoFileRef() {
    Close();
}

void AtIoFileRef::Close() {
    if (m_file != 0 && m_fs != 0) {
        m_fs->Close(m_file);
    }
    m_fs = 0;
    m_file = 0;
}

int AtIoFileRef::Read(void* out, unsigned int size, unsigned int* done) {
    AtIoFileRequest* request;
    int status = m_file->Read(out, size, done, &request);
    if (status == AtIoFile::kPending) {
        status = request->Wait();
        m_file->Release(request);
    }
    return status;
}

int AtIoFileRef::Write(const void* in, unsigned int size) {
    // The third argument doubles as where a pending write leaves its request.
    union {
        unsigned int done;
        AtIoFileRequest* request;
    } result;
    int status = m_file->Write(in, size, &result.done);
    if (status == AtIoFile::kPending) {
        status = result.request->Wait();
        m_file->Release(result.request);
    }
    return status;
}

int AtIoFileRef::Flush() {
    return m_file->Flush();
}

int AtIoFileRef::OpenOn(AtIoFileSystem* fs, const char* path, int flags) {
    Close();
    m_fs = fs;
    return fs->Open(path, flags, &m_file);
}

int AtIoFileRef::Open(const char* path, int flags) {
    AtIoFileSystem* fs;
    if (flags & AtIoFile::kFlagsIgnored) {
        fs = s_context->Top();
    } else if (s_context->FindFile(path, &fs) != 0) {
        return 1;
    }
    return OpenOn(fs, path, flags);
}

int AtIoFileRef::OpenWithExtension(const char* path, const char* ext, int flags) {
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

void AtIoStream::SetHeap(void* heap) {
    s_heap = heap;
}

AtIoStream::AtIoStream() {
    m_allocated = 0;
    m_base = 0;
    m_size = 0;
    m_cur = 0;
    m_limit = 0;
    m_line = 0;
}

AtIoStream::~AtIoStream() {
    Free();
}

int AtIoStream::Refill() {
    char* end = m_base + m_size;
    m_cur = m_limit;
    if (m_limit >= end) {
        return 0;
    }
    m_limit += 0x100;
    if (m_limit > end) {
        m_limit = end;
    }
    return 1;
}

int AtIoStream::SkipLine() {
    for (;;) {
        while (m_cur < m_limit) {
            char c = *m_cur++;
            if (c == '\n') {
                m_line++;
                return 1;
            }
        }
        if (!Refill()) {
            return 0;
        }
    }
}

int AtIoStream::SkipWhitespace() {
    for (;;) {
        while (m_cur < m_limit) {
            char c = *m_cur;
            if (c == '\n') {
                m_line++;
            } else if (c != ' ' && c != '\t' && c != '\r') {
                return 1;
            }
            m_cur++;
        }
        if (!Refill()) {
            return 0;
        }
    }
}

void AtIoStream::Put(char c) {
    if (m_cur >= m_base + m_size) {
        if (!Refill()) {
            AtFatal("Unable to write buffer to media", 0, 0, 0);
        }
        m_cur = m_base;
    }
    *m_cur = c;
    m_cur++;
}

void AtIoStream::Free() {
    if (m_allocated != 0) {
        operator delete(m_allocated);
    }
    m_allocated = 0;
    m_base = 0;
    m_size = 0;
    m_cur = 0;
    m_limit = 0;
    m_line = 0;
}

void AtIoStream::Allocate(unsigned int size) {
    Free();
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_allocated = (char*)operator new(size);
    AtPopHeap();
    if (m_allocated == 0) {
        AtRaise(-4, 0, 0, 0);
        if (m_allocated == 0) {
            AtRaise(-4, 0, 0, 0);
        }
    }
    m_size = size;
    m_base = m_allocated;
    m_cur = m_allocated;
    m_limit = m_allocated;
    m_line = 1;
}

AtIoFileStream::AtIoFileStream() {
    m_ref = 0;
    m_filePosition = 0;
}

AtIoFileStream::~AtIoFileStream() {
    Detach();
}

int AtIoFileStream::Refill() {
    unsigned int done = 0;
    AtIoFile* file = m_ref->m_file;
    if (file != 0 && (file->m_flags & AtIoFile::kFlagsWrite)) {
        if (file->m_position != m_filePosition) {
            file->Seek(m_filePosition);
        }
        m_ref->Write(m_base, (unsigned int)(m_cur - m_base));
        m_filePosition += (unsigned int)(m_cur - m_base);
    }
    file = m_ref->m_file;
    if (file != 0 && (file->m_flags & 2)) {
        if (m_ref->Read(m_base, m_size, &done) != 0) {
            return 0;
        }
        m_filePosition = m_ref->m_file->m_position;
    }
    file = m_ref->m_file;
    if (file != 0 && (file->m_flags & AtIoFile::kFlagsWrite)) {
        done = m_size;
    }
    m_cur = m_base;
    m_limit = m_base + done;
    return m_size == done || done != 0;
}

void AtIoFileStream::Detach() {
    if (m_ref != 0 && m_ref->m_file != 0 && (m_ref->m_file->m_flags & AtIoFile::kFlagsWrite)) {
        Refill();
    }
    m_ref = 0;
    m_filePosition = 0;
    Free();
}

void AtIoFileStream::Attach(AtIoFileRef* file, unsigned int bufferSize) {
    if (m_ref != 0) {
        Detach();
    }
    m_ref = file;
    Allocate(bufferSize);
}
