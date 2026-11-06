// AtIoJam* — JAM archive directory reading, recovered types (partial).
//
// Reference build: knd-xbox-usa-5454008a-v1. Format: docs/formats.md.
// Evidence classes:
//   ORIGINAL  AtIoJamInfoHeader, AtIoFastAlignedJamHeader, AtIoFastJamHeader, AtIoJamFileSystem: the game's own names,
//             from error strings and from name strings stored after the vftables.
//   OBSERVED  offsets, slot order and control flow: instructions cited below.
//   INFERRED  AtIoJam1Header, AtIoJamNameTable, AtIoJamHeader, AtIoJamDirectoryHeader, AtIoFastRawJamHeader, AtIoFile, AtIoFileRequest,
//             AtIoJamFile, AtIoFastAlignedJamFile, AtIoFastJamFile, and every method/member name.
//
// Field reads go through ReadU16/ReadU32, byte by byte, as the original does; they work on any host byte order.
// Paths are copied into fixed buffers with no length check. This is original behaviour and is preserved here: a port must
// harden it as a separate, tested change (docs/portability.md).
#pragma once
#include "at/AtIoFileSystem.h"

void AtFatal(const char* message, int a, int b, int c);  // 0x00011070: not recovered; platform/ supplies a diagnostic.
void AtRaise(int code, int a, int b, int c);             // 0x00011170: -2 heap not set, -4 out of memory. Not recovered.
void AtPushHeap(void* heap);                             // 0x00011240: operator new allocates from the top heap.
void AtPopHeap();                                        // 0x00011260

class AtIoJamDirectoryHeader;
class AtIoJamFileSystem;

// Asynchronous read in progress. Only the slot used here is known.
class AtIoFileRequest {
public:
    virtual void Unknown0() = 0;
    virtual void Unknown1() = 0;
    virtual int Wait() = 0;  // slot 2, returns the read's status
};

// Open file. vftable 0x00273004: ten pure slots. Used both for the archive on disc and for files inside it.
class AtIoFile {
public:
    // Names older than the table of meanings at 0x000140D0, AtIoStatusText, which is what they really say: 2 is "error opening the specified file", 3 "general error",
    // 8 "file pointer is out of bounds", 9 "access violation".
    enum Status { kOk = 0, kNotFound = 2, kIoError = 3, kPending = 7, kEndOfFile = 8, kUnsupported = 9 };
    enum Flags { kFlagsIgnored = 0x21, kFlagsWrite = 0x0C, kOpen = 0x10000, kReachedEnd = 0x20000 };

    AtIoFile();                                                                    // 0x00025B80
    virtual ~AtIoFile() = 0;                                                       // slot 0; 0x00025BB0
    virtual int Close() = 0;                                                       // slot 1
    virtual int Open(const char* path, const char* displayName, int flags) = 0;    // slot 2
    virtual int Clear() = 0;                                                       // slot 3, always 0 for an archive file
    virtual int Seek(unsigned int offset) = 0;                                     // slot 4, 0 on success
    virtual int Read(void* out, unsigned int size, unsigned int* done, AtIoFileRequest** request) = 0;  // slot 5
    virtual int Write(const void* in, unsigned int size, unsigned int* done) = 0;  // slot 6
    virtual int Flush() = 0;                                                       // slot 7
    virtual int Unknown8(void* request) = 0;                                       // slot 8; argument count inferred
    virtual int Release(AtIoFileRequest* request) = 0;                             // slot 9

    int ResetFile();                                                               // 0x00025BC0

    unsigned int m_position;                 // +0x04
    unsigned int m_size;                     // +0x08
    unsigned int m_sizeCopy;                 // +0x0C
    int m_flags;                             // +0x10
    char m_path[0x80];                       // +0x14
    int m_unknown94;                         // +0x94
    int m_unknown98;                         // +0x98
};

// A file inside an archive. The methods here are shared by both kinds: their vftables hold the same addresses.
class AtIoJamFile : public AtIoFile {
public:
    AtIoJamFile();
    virtual int Close();                                                           // 0x00082860
    virtual int Clear();                                                           // 0x00082810
    virtual int Read(void* out, unsigned int size, unsigned int* done, AtIoFileRequest** request);  // 0x00082FB0
    virtual int Write(const void* in, unsigned int size, unsigned int* done);      // 0x00083140, always kUnsupported
    virtual int Flush();                                                           // 0x00083160, always kUnsupported
    virtual int Unknown8(void* request);                                           // 0x000829E0, forwards to the archive file
    virtual int Release(AtIoFileRequest* request);                                 // 0x00082A00, forwards to the archive file

    void Attach(AtIoJamDirectoryHeader* owner);                                    // 0x00082830

    AtIoJamDirectoryHeader* m_owner;         // +0x9C null when free
    unsigned int m_base;                     // +0xA0 offset of the data in the archive
    unsigned int m_secondSize;               // +0xA4 JAM2 only: the record's second size field
};

// vftable 0x00279F1C.
class AtIoFastAlignedJamFile : public AtIoJamFile {
public:
    AtIoFastAlignedJamFile();                                                      // 0x000827E0
    virtual ~AtIoFastAlignedJamFile();                                             // 0x00082A20
    virtual int Open(const char* path, const char* displayName, int flags);        // 0x000828A0
    virtual int Seek(unsigned int offset);                                         // 0x00082970
};

// vftable 0x00279F48.
class AtIoFastJamFile : public AtIoJamFile {
public:
    AtIoFastJamFile();                                                             // 0x00082DA0
    virtual ~AtIoFastJamFile();                                                    // 0x00083090
    virtual int Open(const char* path, const char* displayName, int flags);        // 0x00082DD0
    virtual int Seek(unsigned int offset);                                         // 0x00082F40
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
    void Clear();                           // 0x00082C50 (same code as the destructor: folded)
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
    virtual void vInit(AtIoJamFileSystem* fs) = 0;                   // slot 1
    virtual AtIoJamFile* OpenFile() = 0;                             // slot 2
    virtual int CloseFile(AtIoFile* file) = 0;                       // slot 3
    virtual void Unknown4() = 0;                                     // slot 4
    virtual void Unknown5() = 0;                                     // slot 5
    virtual int IsDirectoryMissing(const char* path) = 0;            // slot 6
    virtual int IsFileMissing(const char* path) = 0;                 // slot 7
    virtual int GetEntryType(const char* path, int* type) = 0;       // slot 8
    virtual void List(void* a, void* b, void* c) = 0;                // slot 9
};

// What the FSTA and JAM2 classes hold in common, at the same offsets. Whether this class existed in the original is
// INFERRED: the name table's destructor runs in each derived destructor, so these members are not in AtIoJamHeader.
// OpenFile and CloseFile are shared by both vftables.
class AtIoJamDirectoryHeader : public AtIoJamHeader {
public:
    AtIoJamDirectoryHeader() {}
    virtual AtIoJamFile* OpenFile();                                 // 0x000806A0
    virtual int CloseFile(AtIoFile* file);                           // 0x00080D90
    virtual void Unknown4();                                         // 0x001CC9F0 empty (folded)
    virtual void Unknown5();                                         // 0x001CC9F0 empty (folded)

    AtIoJamNameTable m_table;                // +0x04
    AtIoJamFileSystem* m_fs;                 // +0x1C
    AtIoJamFile* m_files;                    // +0x20 array of m_fs->m_maxOpen, 0xA8 bytes each
    unsigned char* m_buffer;                 // +0x24 the whole header as loaded
    const unsigned char* m_root;             // +0x28
    unsigned int m_rootFiles;                // +0x2C
    unsigned int m_rootDirs;                 // +0x30
};

// FSTA. vftable 0x00279A84. Directory words: subdirectory count in bits 12-20, file count in bits 21-31.
class AtIoFastAlignedJamHeader : public AtIoJamDirectoryHeader {
public:
    AtIoFastAlignedJamHeader();                                      // 0x0007F650
    virtual ~AtIoFastAlignedJamHeader();                             // 0x0007F6B0
    virtual void vInit(AtIoJamFileSystem* fs);                       // 0x0007F750
    virtual int IsDirectoryMissing(const char* path);                // 0x0007F9A0
    virtual int IsFileMissing(const char* path);                     // 0x0007F9C0
    virtual int GetEntryType(const char* path, int* type);           // 0x0007F1F0
    virtual void List(void* a, void* b, void* c);                    // 0x0007F9E0 (not recovered)
    virtual void Reset();                                            // slot 10, 0x0007F120

    void Shutdown();                                                 // 0x0007F1A0
    // Returns the directory's table, with its subdirectory and file counts. Null if the last path component is absent.
    const unsigned char* FindDirectory(const char* path, unsigned int* dirs, unsigned int* files);  // 0x0007F220
    // Returns the data offset and writes the size; -1 if absent.
    int FindFile(const char* path, unsigned int* size);              // 0x0007F4B0
    int FindFileInfo(const char* path, unsigned int* offset, unsigned int* size);  // 0x0007FDE0
    static void SetHeap(void* heap);                                 // 0x0007F110

    int m_swappedFields;                     // +0x34 set for FSTR: the two counts swap places

    static void* s_heap;                     // 0x00301B5C
};

// FSTR. vftable 0x002793D8.
class AtIoFastRawJamHeader : public AtIoFastAlignedJamHeader {
public:
    AtIoFastRawJamHeader();                  // 0x0007D190
    virtual ~AtIoFastRawJamHeader();         // 0x0007D700
    virtual void Reset();                    // 0x0007D170
};

// JAM2. vftable 0x00279B88. Same directory layout as FSTA; file entries are 8 bytes and address a record.
class AtIoFastJamHeader : public AtIoJamDirectoryHeader {
public:
    AtIoFastJamHeader();                                             // 0x00080390
    virtual ~AtIoFastJamHeader();                                    // 0x000803F0
    virtual void vInit(AtIoJamFileSystem* fs);                       // 0x00080450
    virtual int IsDirectoryMissing(const char* path);                // 0x000806F0
    virtual int IsFileMissing(const char* path);                     // 0x00080C70
    virtual int GetEntryType(const char* path, int* type);           // 0x0007F1F0 (folded with the FSTA method)
    virtual void List(void* a, void* b, void* c);                    // 0x00080710 (not recovered)

    void Shutdown();                                                 // 0x00080040
    const unsigned char* FindDirectory(const char* path, unsigned int* dirs, unsigned int* files);  // 0x000800A0
    // Splits a path, resolves the name and extension indexes, and finds the directory.
    const unsigned char* Locate(const char* path, unsigned int* dirs, unsigned int* files, int* name, int* ext);  // 0x00080280
    int FindRecord(const char* path);                                // 0x00080AE0, record offset or -1
    int GetFileInfo(const char* path, unsigned int* offset, unsigned int* size);  // 0x00080BA0, reads the record
    int GetRecordOffset(const char* path, unsigned int* offset);     // 0x00080C90
    static void SetHeap(void* heap);                                 // 0x0007FFD0

    static void* s_heap;                     // 0x00301B60
};

// JAM1. No archive on this disc uses it, and it is NOT recovered: src/at/AtIoJamPending.cpp stops with a diagnostic. The name is
// inferred. vftable 0x00279C3C, constructor 0x00081020.
class AtIoJam1Header : public AtIoJamHeader {
public:
    AtIoJam1Header();
    virtual ~AtIoJam1Header();
    virtual void vInit(AtIoJamFileSystem* fs);
    virtual AtIoJamFile* OpenFile();
    virtual int CloseFile(AtIoFile* file);
    virtual void Unknown4();
    virtual void Unknown5();
    virtual int IsDirectoryMissing(const char* path);
    virtual int IsFileMissing(const char* path);
    virtual int GetEntryType(const char* path, int* type);
    virtual void List(void* a, void* b, void* c);
    int GetFileInfo(const char* path, unsigned int* offset, unsigned int* size);  // 0x00081000

    char m_unknown04[0x3C - 4];
};

// A mounted archive. vftable 0x00279404. Allocated at 0x0007DB50 (0x818 bytes).
class AtIoJamFileSystem : public AtIoFileSystem {
public:
    AtIoJamFileSystem();                                                 // 0x0007D1E0
    virtual ~AtIoJamFileSystem();                                        // 0x0007D710
    virtual int Open(const char* path, int flags, AtIoFile** out);       // slot 2, 0x0007D330
    virtual int Close(AtIoFile* file);                                   // slot 3, 0x0007D110
    virtual int IsDirectoryMissing(const char* path);                    // slot 4, 0x0007D440
    virtual int IsFileMissing(const char* path);                         // slot 5, 0x0007D470
    virtual int GetEntryType(const char* path, int* type);               // slot 6, 0x0007D4A0
    virtual int Unknown7(void* a);                                       // slot 7, 0x000B15B0 (folded)
    virtual int GetFileInfo(const char* path, AtIoFileStat* stat);       // slot 8, 0x0007D5A0
    virtual void* List(const char* pattern);                             // slot 9, 0x0007D4D0 (not recovered)
    virtual void FreeList(void* list);                                   // slot 10, 0x00103640 (not recovered)
    virtual int Unknown11(void* a);                                      // slot 11, 0x0007D160
    virtual int Unknown12();                                             // slot 12, 0x000AB720 (folded)
    virtual void SetRoot(const char* root);                              // slot 13, 0x000CBF30 empty (folded)

    void vInit(unsigned int maxOpen, const char* path, int flags);       // 0x0007D770
    void Shutdown();                                                     // 0x0007D270
    static AtIoJamFileSystem* Create(const char* path, unsigned int maxOpen, int flags);  // 0x0007DB10
    static void SetHeaderHeap(void* heap);                               // 0x0007D100
    static void SetHeap(void* heap);                                     // 0x0007DAD0

    AtIoFile* File() const { return m_overrideFile ? m_overrideFile : m_file; }

    AtIoJamInfoHeader m_info;      // +0x6BC
    AtIoJamHeader* m_header;       // +0x6E0
    AtIoFile* m_file;              // +0x6E4 the archive, opened through m_device
    AtIoFileSystem* m_device;      // +0x6E8 the file system that holds the archive
    char m_archiveName[0x10C];     // +0x6EC
    unsigned int m_archiveTime;    // +0x7F8 meaning inferred
    unsigned int m_archiveSize;    // +0x7FC
    void* m_unknown800;            // +0x800 heap block, freed by Shutdown
    unsigned int m_unknown804;     // +0x804
    AtIoFile* m_overrideFile;      // +0x808 used instead of m_file when set
    unsigned int m_unknown80C;     // +0x80C
    unsigned int m_unknown810;     // +0x810
    unsigned int m_unknown814;     // +0x814

    static void* s_headerHeap;     // 0x00301B50
    static void* s_heap;           // 0x00301B54
};

// Original 32-bit layout. See the note in gm/GmLinkedList.h.
#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtIoFile, m_path) == 0x14, "AtIoFile::m_path");
static_assert(offsetof(AtIoJamFile, m_owner) == 0x9C, "AtIoJamFile::m_owner");
static_assert(sizeof(AtIoFastAlignedJamFile) == 0xA8, "array stride at 0x0007F920");
static_assert(sizeof(AtIoFastJamFile) == 0xA8, "array stride at 0x00080620");
static_assert(offsetof(AtIoJamFileSystem, m_maxOpen) == 0x6AC, "AtIoJamFileSystem::m_maxOpen");
static_assert(offsetof(AtIoJamFileSystem, m_archiveName) == 0x6EC, "AtIoJamFileSystem::m_archiveName");
static_assert(offsetof(AtIoJamFileSystem, m_archiveTime) == 0x7F8, "AtIoJamFileSystem::m_archiveTime");
static_assert(sizeof(AtIoJamFileSystem) == 0x818, "allocation size at 0x0007DB4B");
static_assert(sizeof(AtIoJam1Header) == 0x3C, "allocation size at 0x0007D91E");
static_assert(offsetof(AtIoJamFileSystem, m_info) == 0x6BC, "AtIoJamFileSystem::m_info");
static_assert(offsetof(AtIoJamFileSystem, m_header) == 0x6E0, "AtIoJamFileSystem::m_header");
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
