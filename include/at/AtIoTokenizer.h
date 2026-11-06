// Text files: a buffered stream over an open file, and a tokenizer that reads and writes the game's plain-text formats.
// All class and method names are inferred; none appears in the executable. Layouts: see the static_asserts below.
#pragma once
#include "at/AtIoFileSystem.h"
#include "at/AtIoJam.h"

// One open file and the file system that opened it.
class AtIoFileRef {
public:
    AtIoFileRef();                                                        // 0x00018810
    ~AtIoFileRef();                                                       // 0x00018820
    void Close();                                                         // 0x00018820 (same code: folded)
    int Read(void* out, unsigned int size, unsigned int* done);           // 0x00018850 waits for a pending read
    int Write(const void* in, unsigned int size);                         // 0x000188A0 waits for a pending write
    int Flush();                                                          // 0x000188E0
    int OpenOn(AtIoFileSystem* fs, const char* path, int flags);          // 0x000188F0
    int Open(const char* path, int flags);                                // 0x00018940 1 when no file system holds the file
    int OpenWithExtension(const char* path, const char* ext, int flags);  // 0x000189D0 ext is added if the leaf has no dot

    AtIoFileSystem* m_fs;      // +0x00
    AtIoFile* m_file;          // +0x04

    static AtIoContext* s_context;  // 0x002C6278
};

// vftable 0x00274A54. A buffer with a read cursor. Refill here shows the next 0x100 bytes of what is already in memory.
class AtIoStream {
public:
    AtIoStream();                                    // 0x000316B0
    virtual ~AtIoStream();                           // slot 0; 0x000317E0 deleting, 0x00031700 complete
    virtual int Refill();                            // slot 1, 0x00031680; 0 at the end of the data

    int SkipLine();                                  // 0x000315A0 past the next newline; 0 at the end of the data
    int SkipWhitespace();                            // 0x000315E0 to the next non-blank; 0 at the end of the data
    void Put(char c);                                // 0x00031630
    void Free();                                     // 0x000316D0
    void Allocate(unsigned int size);                // 0x00031740

    static void SetHeap(void* heap);                 // 0x00031590

    char* m_allocated;         // +0x04
    char* m_base;              // +0x08
    unsigned int m_size;       // +0x0C
    char* m_cur;               // +0x10
    char* m_limit;             // +0x14
    int m_line;                // +0x18 counts the newlines skipped; a newline inside a string is not counted

    static void* s_heap;       // 0x002C63CC
};

// vftable 0x00272AF4. A stream whose Refill also writes out the buffer, for a file open for writing.
class AtIoFileStream : public AtIoStream {
public:
    AtIoFileStream();                                // 0x000222A0
    ~AtIoFileStream() override;                      // slot 0; 0x000224A0 deleting, 0x000223D0 complete
    int Refill() override;                           // slot 1, 0x00022300

    void Detach();                                   // 0x000222C0 writes out what is pending, frees the buffer
    void Attach(AtIoFileRef* file, unsigned int bufferSize);  // 0x00022450

    AtIoFileRef* m_ref;        // +0x1C
    unsigned int m_filePosition;  // +0x20
};

struct AtIoKeyword {
    int m_id;
    const char* m_name;
};

// vftable 0x00271D58. Slots 5 to 41 are pure.
class AtIoTokenizer {
public:
    // The token Next() returns. Keyword ids are the caller's.
    enum Token {
        kEnd = 0, kError = 1, kString = 2, kReal = 3, kInt = 4, kLeftCurly = 5, kRightCurly = 6, kLeftBracket = 7, kRightBracket = 8,
        kComma = 9, kSemicolon = 10, kColon = 11, kExclamation = 12, kPercent = 13, kCaret = 14, kAmpersand = 15, kTimes = 16,
        kLeftParen = 17, kRightParen = 18, kMinus = 19, kPlus = 20, kDivide = 21, kEquals = 22, kLess = 23, kGreater = 24, kOr = 25,
        kQuestion = 26, kAt = 27, kTilde = 28
    };
    // The argument of Error(). Text: ErrorText().
    enum Code { kSyntaxError = 0, kExpectedString = 3, kExpectedReal = 4, kExpectedInt = 5, kExpectedLeftCurly = 6, kExpectedRightCurly = 7,
                kExpectedLeftBracket = 8, kExpectedRightBracket = 9, kExpectedKeyword = 0x1E, kInvalidKeyword = 0x21, kInvalidString = 0x22 };

    AtIoTokenizer();                                 // 0x00015D10
    virtual ~AtIoTokenizer();                        // slot 0; 0x00016030 deleting, 0x00015FF0 complete
    virtual void Reset();                            // slot 1, 0x00015D80 clears everything but the vftable
    virtual void SetKeywords(const AtIoKeyword* table, unsigned int count, AtIoTokenizer* inherit);  // slot 2, folded empty body
    virtual void Report(int code);                   // slot 3, 0x00015DB0 builds a message and discards it
    virtual void Error(int code);                    // slot 4, 0x00015ED0 reports through AtFatal
    virtual int Next() = 0;                          // slot 5
    virtual void PutInt(int value) = 0;              // slot 6
    virtual void PutReal(float value) = 0;           // slot 7
    virtual void PutString(const char* text) = 0;    // slot 8
    virtual void PutComment(const char* text) = 0;   // slot 9
    virtual void PutKeyword(int id) = 0;             // slot 10
    virtual void PutLeftBracket() = 0;               // slot 11
    virtual void PutRightBracket() = 0;              // slot 12
    virtual void PutLeftCurly() = 0;                 // slot 13
    virtual void PutRightCurly() = 0;                // slot 14
    virtual void PutComma() = 0;                     // slot 15
    virtual void PutSemicolon() = 0;                 // slot 16
    virtual void PutColon() = 0;                     // slot 17
    virtual void PutExclamation() = 0;               // slot 18
    virtual void PutPercent() = 0;                   // slot 19
    virtual void PutCaret() = 0;                     // slot 20
    virtual void PutAmpersand() = 0;                 // slot 21
    virtual void PutTimes() = 0;                     // slot 22
    virtual void PutLeftParen() = 0;                 // slot 23
    virtual void PutRightParen() = 0;                // slot 24
    virtual void PutMinus() = 0;                     // slot 25
    virtual void PutPlus() = 0;                      // slot 26
    virtual void PutDivide() = 0;                    // slot 27
    virtual void PutEquals() = 0;                    // slot 28
    virtual void PutLess() = 0;                      // slot 29
    virtual void PutGreater() = 0;                   // slot 30
    virtual void PutOr() = 0;                        // slot 31
    virtual void PutQuestion() = 0;                  // slot 32
    virtual void PutAt() = 0;                        // slot 33
    virtual void PutTilde() = 0;                     // slot 34
    virtual void PutNewline() = 0;                   // slot 35
    virtual void PutSpace() = 0;                     // slot 36
    virtual void PutSpaces(unsigned int count) = 0;  // slot 37
    virtual void PutTab() = 0;                       // slot 38
    virtual void PutTabs(unsigned int count) = 0;    // slot 39
    virtual void PutName(const char* name) = 0;      // slot 40, an eight-byte name, as a string
    virtual void PutRaw(const char* text) = 0;       // slot 41
    virtual const char* GetIdentifier();             // slot 42, 0x00015D00

    int ExpectInt();                                 // 0x00015880
    float ExpectReal();                              // 0x000158A0
    const char* ExpectString();                      // 0x000158C0
    const char* ExpectString(unsigned int maxLength);  // 0x000158E0
    void ExpectStringCopy(char** out);               // 0x00015920 a new copy on the current heap
    void ExpectName(char* out);                      // 0x00015990 at most eight bytes, not always terminated
    void ExpectToken(int token);                     // 0x000159D0
    void ExpectLeftBracket();                        // 0x000159F0
    void ExpectRightBracket();                       // 0x00015A10
    void ExpectLeftCurly();                          // 0x00015A30
    void ExpectRightCurly();                         // 0x00015A50
    int ExpectCount();                               // 0x00015A70 "[ n ] {"
    void CopyName(char* out);                        // 0x00015AD0 the current string; at most eight bytes
    const char* ErrorText(int code);                 // 0x00015B00
    void SetStream(AtIoStream* stream);              // 0x00015D50

    AtIoStream* m_stream;      // +0x004
    int m_pushedBack;          // +0x008 the next Next() returns m_type again; a format's reader sets it (0x0013E4A1)
    int m_type;                // +0x00C
    int m_int;                 // +0x010
    float m_real;              // +0x014
    char m_string[0x100];      // +0x018
    unsigned int m_stringLength;  // +0x118
    int m_escapes;             // +0x11C backslash escapes in strings; the game text reader turns them on while it reads
    const char* m_fileName;    // +0x120
    char m_extension[4];       // +0x124 ".RMB", with no terminator when full
};

// vftable 0x00272C78.
class AtIoTextTokenizer : public AtIoTokenizer {
public:
    enum { kHashSize = 0x14B };

    AtIoTextTokenizer();                             // 0x00022500; the hash table stays uninitialised until SetKeywords
    ~AtIoTextTokenizer() override;                   // slot 0; 0x00022DC0 deleting, 0x00022DE0 complete
    void SetKeywords(const AtIoKeyword* table, unsigned int count, AtIoTokenizer* inherit) override;  // slot 2, 0x00022530
    int Next() override;                             // slot 5, 0x00022DF0
    void PutInt(int value) override;                 // slot 6, 0x000227C0
    void PutReal(float value) override;              // slot 7, 0x00022810
    void PutString(const char* text) override;       // slot 8, 0x00022870
    void PutComment(const char* text) override;      // slot 9, 0x000228D0
    void PutKeyword(int id) override;                // slot 10, 0x00022930
    void PutLeftBracket() override;                  // slot 11, 0x000229E0
    void PutRightBracket() override;                 // slot 12, 0x00022A00
    void PutLeftCurly() override;                    // slot 13, 0x00022A20
    void PutRightCurly() override;                   // slot 14, 0x00022A40
    void PutComma() override;                        // slot 15, 0x00022A60
    void PutSemicolon() override;                    // slot 16, 0x00022A80
    void PutColon() override;                        // slot 17, 0x00022AA0
    void PutExclamation() override;                  // slot 18, 0x00022AC0
    void PutPercent() override;                      // slot 19, 0x00022AE0
    void PutCaret() override;                        // slot 20, 0x00022B00
    void PutAmpersand() override;                    // slot 21, 0x00022B20
    void PutTimes() override;                        // slot 22, 0x00022B40
    void PutLeftParen() override;                    // slot 23, 0x00022B60
    void PutRightParen() override;                   // slot 24, 0x00022B80
    void PutMinus() override;                        // slot 25, 0x00022BA0
    void PutPlus() override;                         // slot 26, 0x00022BC0
    void PutDivide() override;                       // slot 27, 0x00022BE0
    void PutEquals() override;                       // slot 28, 0x00022C00
    void PutLess() override;                         // slot 29, 0x00022C20
    void PutGreater() override;                      // slot 30, 0x00022C40
    void PutOr() override;                           // slot 31, 0x00022C60
    void PutQuestion() override;                     // slot 32, 0x00022C80
    void PutAt() override;                           // slot 33, 0x00022CA0
    void PutTilde() override;                        // slot 34, 0x00022CC0
    void PutNewline() override;                      // slot 35, 0x00022CE0
    void PutSpace() override;                        // slot 36, 0x00022D00
    void PutSpaces(unsigned int count) override;     // slot 37, 0x00022D10
    void PutTab() override;                          // slot 38, 0x00022D40
    void PutTabs(unsigned int count) override;       // slot 39, 0x00022D50
    void PutName(const char* name) override;         // slot 40, 0x00022D80
    void PutRaw(const char* text) override;          // slot 41, 0x000229B0
    const char* GetIdentifier() override;            // slot 42, 0x00022DB0

    void LookUpIdentifier();                         // 0x00022700
    static unsigned int HashOf(const char* upper);   // 0x000224C0 takes its argument in eax in the original

    char m_identifier[0x40];   // +0x128 upper case
    const AtIoKeyword* m_keywords;  // +0x168
    unsigned int m_keywordCount;    // +0x16C
    const AtIoKeyword* m_hash[kHashSize];  // +0x170 open addressing
};

// Opens a file and hands out a tokenizer for it. Not polymorphic: its constructor leaves the stream's vftable at offset 0, so the stream
// is its first member.
class AtIoTokenFile {
public:
    AtIoTokenFile();                                 // 0x00015460
    ~AtIoTokenFile();                                // 0x00015500
    void Close();                                    // 0x000154B0
    // 0 on success, otherwise the file system's status. A null tokenizer means a new AtIoTextTokenizer, owned by this object.
    int Open(const char* path, const char* ext, int flags, AtIoTokenizer* tokenizer);  // 0x00015560
    int Open(const char* path, int flags, AtIoTokenizer* tokenizer);                   // 0x00015700

    AtIoFileStream m_stream;   // +0x00
    AtIoFileRef m_file;        // +0x24
    AtIoTokenizer* m_tokenizer;  // +0x2C
    char* m_name;              // +0x30
    int m_ownsTokenizer;       // +0x34

    static void* s_heap;       // 0x002C60B0
};

void AtCopyName(const char* from, char* to);         // 0x00013ED0 at most eight bytes; stops after a terminator
void AtCopyName9(char* to, const char* from);        // 0x00013F90 always eight bytes then a terminator; null means empty
void AtFormatInt(int value, char* out);              // 0x000206F0
void AtFormatReal(float value, char* out);           // 0x00020710 "%f": no bound on the output

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtIoStream) == 0x1C, "AtIoStream");
static_assert(sizeof(AtIoFileStream) == 0x24, "AtIoFileStream");
static_assert(offsetof(AtIoTokenFile, m_ownsTokenizer) == 0x34, "AtIoTokenFile::m_ownsTokenizer");
static_assert(sizeof(AtIoTokenFile) == 0x38, "AtIoTokenFile");
static_assert(offsetof(AtIoTokenizer, m_stringLength) == 0x118, "AtIoTokenizer::m_stringLength");
static_assert(offsetof(AtIoTokenizer, m_extension) == 0x124, "AtIoTokenizer::m_extension");
static_assert(offsetof(AtIoTextTokenizer, m_keywords) == 0x168, "AtIoTextTokenizer::m_keywords");
static_assert(offsetof(AtIoTextTokenizer, m_hash) == 0x170, "AtIoTextTokenizer::m_hash");
static_assert(sizeof(AtIoTextTokenizer) == 0x69C, "AtIoTextTokenizer");
#pragma pop_macro("offsetof")
#endif
