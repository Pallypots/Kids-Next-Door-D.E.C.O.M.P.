// A file system over the host's own files, for the recovered AtIo layer. This is port code, not recovered code: on the Xbox
// this role belongs to a device file system that has not been recovered. It reads only; archives are opened read-only.
#pragma once
#include <cstdio>
#include <string>

#include "at/AtIoJam.h"

class AtIoHostFile : public AtIoFile {
public:
    AtIoHostFile();
    ~AtIoHostFile() override;
    int Close() override;
    int Open(const char* path, const char* displayName, int flags) override;
    int Clear() override;
    int Seek(unsigned int offset) override;
    int Read(void* out, unsigned int size, unsigned int* done, AtIoFileRequest** request) override;
    int Write(const void* in, unsigned int size, unsigned int* done) override;
    int Flush() override;
    int Unknown8(void* request) override;
    int Release(AtIoFileRequest* request) override;

private:
    std::FILE* m_handle;
};

class AtIoHostFileSystem : public AtIoFileSystem {
public:
    explicit AtIoHostFileSystem(const std::string& root);
    ~AtIoHostFileSystem() override;
    int Open(const char* path, int flags, AtIoFile** out) override;
    int Close(AtIoFile* file) override;
    int IsDirectoryMissing(const char* path) override;
    int IsFileMissing(const char* path) override;
    int GetEntryType(const char* path, int* type) override;
    int Unknown7(void* a) override;
    int GetFileInfo(const char* path, AtIoFileStat* stat) override;
    void* List(const char* pattern) override;
    void FreeList(void* list) override;
    int Unknown11(void* a) override;
    int Unknown12() override;

private:
    std::string Full(const char* path) const;
    std::string m_hostRoot;
};
