// AtIoJam* — JAM archive directory reading, recovered types (partial).
//
// Reference build: knd-xbox-usa-5454008a-v1. Format: docs/formats.md.
// Evidence classes:
//   ORIGINAL  AtIoJamInfoHeader, AtIoFastAlignedJamHeader, AtIoFastJamHeader, AtIoJamFileSystem: the game's own names,
//             from error strings and from name strings stored after the vftables.
//   OBSERVED  offsets, slot order and control flow: instructions cited below.
//   INFERRED  AtIoJamNameTable, AtIoJamHeader, AtIoFastRawJamHeader, AtIoFile, AtIoFileRequest, and every method/member name.
//
// Field reads go through ReadU16/ReadU32, byte by byte, as the original does; they work on any host byte order.
// Paths are copied into fixed buffers with no length check. This is original behaviour and is preserved here: a port must
// harden it as a separate, tested change (docs/portability.md).
#pragma once

void AtFatal(const char* message, int a, int b, int c);  // 0x00011070: not recovered; platform/ supplies a diagnostic.

// Asynchronous read in progress. Only the slot used here is known.
class AtIoFileRequest {
public:
    virtual void Unknown0() = 0;
    virtual void Unknown1() = 0;
    virtual int Wait() = 0;  // slot 2, returns the read's status
};

// Open file. Only slots used by recovered code are known; the rest keep their positions.
class AtIoFile {
public:
    enum { kPending = 7 };  // Read status: finished later through the request
    virtual void Unknown0() = 0;
    virtual void Unknown1() = 0;
    virtual void Unknown2() = 0;
    virtual void Unknown3() = 0;
    virtual int Seek(unsigned int offset) = 0;                                               // slot 4, 0 on success
    virtual int Read(void* out, unsigned int size, int flags, AtIoFileRequest** request) = 0;  // slot 5
    virtual void Unknown6() = 0;
    virtual void Unknown7() = 0;
    virtual void Unknown8() = 0;
    virtual void Release(AtIoFileRequest* request) = 0;                                      // slot 9
};

// Only the two fields recovered code reads. The class is far larger; see 0x0007D770.
class AtIoJamFileSystem {
public:
    AtIoFile* File() const { return m_overrideFile ? m_overrideFile : m_file; }

    char m_unknown000[0x6E4];
    AtIoFile* m_file;          // +0x6E4
    char m_unknown6E8[0x808 - 0x6E8];
    AtIoFile* m_overrideFile;  // +0x808
};

// The fixed 0x20-byte header at the start of every archive.
class AtIoJamInfoHeader {
public:
    enum Token { kNone = 0, kJam1 = 1, kJam2 = 2, kFsta = 3, kFstr = 4 };

    AtIoJamInfoHeader();                    // 0x0007FE40
    void Clear();                           // 0x0007FE60
    void Init(const char* raw);             // 0x0007FE80 (not a vftable slot; the error text calls it vInit)

    int m_token;                            // +0x00
    unsigned int m_unknown04;               // +0x04
    unsigned int m_headerSize;              // +0x08
    unsigned int m_nameCount;               // +0x0C
    unsigned int m_extensionCount;          // +0x10
    char m_label[16];                       // +0x14

    static const char* s_tokens[4];         // 0x002B35FC
};

// Sorted name and extension tables inside the loaded header.
class AtIoJamNameTable {
public:
    AtIoJamNameTable();                     // 0x00082B50
    ~AtIoJamNameTable();                    // 0x00082C50
    void Init(const char* names, unsigned int nameCount, unsigned int extensionCount);  // 0x00082C70
    int GetFullName(unsigned int name, unsigned int extension, char* out);              // 0x00082B70
    int FindName(const char* name);         // 0x00082BF0, -1 if absent
    int FindExtension(const char* ext);     // 0x00082C20, 0 for null or empty, -1 if absent
    int Search(const char* key, const char* table, unsigned int width, int count);      // 0x00082AA0
    int Matches(const char* fullName, const char* namePattern, const char* extPattern); // 0x00082CB0

    const char* m_names;                    // +0x00
    const char* m_extensions;               // +0x04
    int m_unknown08;                        // +0x08 -1
    int m_unknown0C;                        // +0x0C -1
    unsigned int m_nameCount;               // +0x10
    unsigned int m_extensionCount;          // +0x14
};

// Abstract base. vftable 0x00279A5C: ten pure slots.
class AtIoJamHeader {
public:
    enum { kEntryIsFile = 2 };

    AtIoJamHeader() {}
    virtual ~AtIoJamHeader() = 0;                                    // slot 0; 0x00082D90
    virtual void vInit(AtIoJamFileSystem* fs) = 0;                   // slot 1 (not recovered)
    virtual void* OpenFile() = 0;                                    // slot 2 (not recovered)
    virtual void CloseFile(void* file) = 0;                          // slot 3 (not recovered)
    virtual void Unknown4() = 0;                                     // slot 4
    virtual void Unknown5() = 0;                                     // slot 5
    virtual int IsDirectoryMissing(const char* path) = 0;            // slot 6
    virtual int IsFileMissing(const char* path) = 0;                 // slot 7
    virtual int GetEntryType(const char* path, int* type) = 0;       // slot 8
    virtual void List(void* a, void* b, void* c) = 0;                // slot 9 (not recovered)
};

// FSTA. vftable 0x00279A84. Directory words: subdirectory count in bits 12-20, file count in bits 21-31.
class AtIoFastAlignedJamHeader : public AtIoJamHeader {
public:
    AtIoFastAlignedJamHeader();                                      // 0x0007F650
    virtual ~AtIoFastAlignedJamHeader();                             // 0x0007F6B0 (not recovered)
    virtual void vInit(AtIoJamFileSystem* fs);                       // 0x0007F750 (not recovered)
    virtual void* OpenFile();                                        // 0x000806A0 (not recovered)
    virtual void CloseFile(void* file);                              // 0x00080D90 (not recovered)
    virtual void Unknown4();                                         // 0x001CC9F0 empty (folded)
    virtual void Unknown5();                                         // 0x001CC9F0 empty (folded)
    virtual int IsDirectoryMissing(const char* path);                // 0x0007F9A0
    virtual int IsFileMissing(const char* path);                     // 0x0007F9C0
    virtual int GetEntryType(const char* path, int* type);           // 0x0007F1F0
    virtual void List(void* a, void* b, void* c);                    // 0x0007F9E0 (not recovered)
    virtual void Reset();                                            // slot 10, 0x0007F120

    // Returns the directory's table, with its subdirectory and file counts. Null if the last path component is absent.
    const unsigned char* FindDirectory(const char* path, unsigned int* dirs, unsigned int* files);  // 0x0007F220
    // Returns the data offset and writes the size; -1 if absent.
    int FindFile(const char* path, unsigned int* size);              // 0x0007F4B0

    AtIoJamNameTable m_table;                // +0x04
    AtIoJamFileSystem* m_fs;                 // +0x1C
    void* m_files;                           // +0x20 open-file objects, 0xA8 bytes each
    const unsigned char* m_buffer;           // +0x24 the whole header as loaded
    const unsigned char* m_root;             // +0x28
    unsigned int m_rootFiles;                // +0x2C
    unsigned int m_rootDirs;                 // +0x30
    int m_swappedFields;                     // +0x34 set for FSTR: the two counts swap places
};

// FSTR. vftable 0x002793D8.
class AtIoFastRawJamHeader : public AtIoFastAlignedJamHeader {
public:
    AtIoFastRawJamHeader();                  // 0x0007D190
    virtual void Reset();                    // 0x0007D170
};

// JAM2. vftable 0x00279B88. Same directory layout as FSTA; file entries are 8 bytes and address a record.
class AtIoFastJamHeader : public AtIoJamHeader {
public:
    AtIoFastJamHeader();                                             // 0x00080390
    virtual ~AtIoFastJamHeader();                                    // 0x000803F0 (not recovered)
    virtual void vInit(AtIoJamFileSystem* fs);                       // 0x00080450 (not recovered)
    virtual void* OpenFile();                                        // 0x000806A0 (not recovered, shared)
    virtual void CloseFile(void* file);                              // 0x00080D90 (not recovered, shared)
    virtual void Unknown4();                                         // 0x001CC9F0 empty (folded)
    virtual void Unknown5();                                         // 0x001CC9F0 empty (folded)
    virtual int IsDirectoryMissing(const char* path);                // 0x000806F0
    virtual int IsFileMissing(const char* path);                     // 0x00080C70
    virtual int GetEntryType(const char* path, int* type);           // 0x0007F1F0 (folded with the FSTA method)
    virtual void List(void* a, void* b, void* c);                    // 0x00080710 (not recovered)

    const unsigned char* FindDirectory(const char* path, unsigned int* dirs, unsigned int* files);  // 0x000800A0
    // Splits a path, resolves the name and extension indexes, and finds the directory.
    const unsigned char* Locate(const char* path, unsigned int* dirs, unsigned int* files, int* name, int* ext);  // 0x00080280
    int FindRecord(const char* path);                                // 0x00080AE0, record offset or -1
    int GetFileInfo(const char* path, unsigned int* offset, unsigned int* size);  // 0x00080BA0, reads the record
    int GetRecordOffset(const char* path, unsigned int* offset);     // 0x00080C90

    AtIoJamNameTable m_table;                // +0x04
    AtIoJamFileSystem* m_fs;                 // +0x1C
    void* m_files;                           // +0x20
    const unsigned char* m_buffer;           // +0x24
    const unsigned char* m_root;             // +0x28
    unsigned int m_rootFiles;                // +0x2C
    unsigned int m_rootDirs;                 // +0x30
};

// Original 32-bit layout. See the note in gm/GmLinkedList.h.
#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtIoJamFileSystem, m_file) == 0x6E4, "AtIoJamFileSystem::m_file");
static_assert(offsetof(AtIoJamFileSystem, m_overrideFile) == 0x808, "AtIoJamFileSystem::m_overrideFile");
static_assert(offsetof(AtIoJamInfoHeader, m_extensionCount) == 0x10, "AtIoJamInfoHeader::m_extensionCount");
static_assert(offsetof(AtIoJamInfoHeader, m_label) == 0x14, "AtIoJamInfoHeader::m_label");
static_assert(sizeof(AtIoJamNameTable) == 0x18, "AtIoJamNameTable");
static_assert(offsetof(AtIoFastAlignedJamHeader, m_table) == 0x04, "FSTA m_table");
static_assert(offsetof(AtIoFastAlignedJamHeader, m_root) == 0x28, "FSTA m_root");
static_assert(sizeof(AtIoFastAlignedJamHeader) == 0x38, "allocation size at 0x0007D9B5");
static_assert(sizeof(AtIoFastRawJamHeader) == 0x38, "allocation size at 0x0007D9FC");
static_assert(sizeof(AtIoFastJamHeader) == 0x34, "allocation size at 0x0007D968");
#pragma pop_macro("offsetof")
#endif
