// A time-sliced loader of a text file: each call to Update spends a budget of milliseconds on opening the file, parsing it, then running a queue of items, one at a time.
// The file's extension is MNG. Start-up waits on one 30 milliseconds at a time. All names are inferred.
// The one concrete class known, with its seven kinds of item, begins at 0x0015CD10 and is not recovered; on the disc its files are lists of what a level loads.
#pragma once
#include "at/AtIoTokenizer.h"

// Supplied by the platform layer. On Xbox: 0x00020640, from the performance counter.
unsigned int AtPlatformMilliseconds();

// The memory manager at 0x002C60AC, not recovered. Only one field is known: what the loader installs around each item.
struct AtMemoryManager {
    char m_unknown000[0x24C];
    int m_unknown24C;

    static AtMemoryManager* s_instance;  // 0x002C60AC
};

// What runs on the queue. Only these parts are known; a real one at 0x00142220 is 0x94 bytes.
class GmManifestItem {
public:
    virtual void Unknown0() = 0;
    virtual void Run() = 0;              // slot 1

    char m_unknown04[0x84];
    GmManifestItem* m_next;              // +0x88
};

struct GmManifestRequest {
    char m_name[8];
    const char* m_path;                  // +0x08 without its extension
    int m_tag;                           // +0x0C
};

class GmManifestLoader {
public:
    enum State { kOpening = 0, kParsing = 1, kRunning = 2, kDone = 3, kFailed = 4 };

    GmManifestLoader();                  // 0x00141F20
    virtual void Parse(AtIoTokenizer* tokenizer) = 0;                 // slot 0: first, with the file open
    virtual void AfterParse() = 0;                                    // slot 1: after every token, with the file closed; queues the items
    virtual void OnToken(int token, AtIoTokenizer* tokenizer) = 0;    // slot 2: for each token Parse left
    virtual void Finished() = 0;                                      // slot 3: when the last item has run
    virtual ~GmManifestLoader();         // slot 4; 0x00142020 deleting, 0x00141F50 complete

    void Configure(const GmManifestRequest* request);  // 0x00141E10 no bound on the path
    void Reset();                        // 0x00141E50 leaves the file alone
    void ReadTokens(AtIoTokenizer* tokenizer);         // 0x00141E70
    void Open();                         // 0x00141EA0
    void RunNext();                      // 0x00141F80
    // 0x00141FF0. The first item's link is not cleared, so a stale one is followed. The second argument is not read.
    void Enqueue(GmManifestItem* item, int unused);
    // 0x00142040. 1 when done or failed, 0 when the budget ran out. A budget of 0 does nothing.
    int Update(unsigned int budgetMilliseconds);

    GmManifestItem* m_head;              // +0x04
    GmManifestItem* m_tail;              // +0x08
    char m_name[8];                      // +0x0C
    int m_done;                          // +0x14
    int m_state;                         // +0x18
    AtIoTokenFile m_file;                // +0x1C
    int m_tag;                           // +0x54
    char m_path[0x44];                   // +0x58 the size is inferred: the concrete class begins its fields after it
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtMemoryManager, m_unknown24C) == 0x24C, "AtMemoryManager::m_unknown24C");
static_assert(offsetof(GmManifestItem, m_next) == 0x88, "GmManifestItem::m_next");
static_assert(offsetof(GmManifestRequest, m_tag) == 0xC, "GmManifestRequest::m_tag");
static_assert(offsetof(GmManifestLoader, m_state) == 0x18, "GmManifestLoader::m_state");
static_assert(offsetof(GmManifestLoader, m_file) == 0x1C, "GmManifestLoader::m_file");
static_assert(offsetof(GmManifestLoader, m_tag) == 0x54, "GmManifestLoader::m_tag");
static_assert(sizeof(GmManifestLoader) == 0x9C, "GmManifestLoader");
#pragma pop_macro("offsetof")
#endif
