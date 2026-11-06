// The game's list of levels, read from Levels.txt. All class and member names are inferred; no RTTI name survives for these.
// PARTIAL: this class also owns level content (members at +0x64, +0x1B4 and +0x3A4), which is not recovered. Its constructor and
// destructor at 0x0013E150 and 0x0013E930 are therefore not recorded, and neither is 0x0013E530.
//
// Meaning of the level kinds, from their users: a movie is played and skipped past (0x0009A820); only normal levels are listed for
// level select (0x000A0FC0). For hidden and non-selectable the meaning comes from their keywords in Levels.txt only.
#pragma once
#include "at/AtIoTokenizer.h"
#include "gm/GmSocket.h"

// vftable 0x00292F9C, one slot. A plug that holds an eight-byte name, not always terminated.
class GmLevelName : public GmPlug {
public:
    GmLevelName();                                       // 0x001950E0
    ~GmLevelName() override;                             // 0x00195100; slot 0 is the vector deleting form, 0x0013D8F0
    GmLevelName& operator=(const GmLevelName& other);    // 0x001950A0 copies the name only
    void SetName(const char* name);                      // 0x001950C0

    char m_name[8];        // +0x08
};

class GmLevelNameArray {
public:
    void Resize(unsigned int count);    // 0x0013DFE0 keeps the names that still fit; zero frees everything

    GmLevelName* m_items;  // +0x00
    unsigned int m_count;  // +0x04

    static void* s_heap;   // 0x003B3574
};

// vftable 0x0028DAB4.
class GmLevelManager {
public:
    // Values of m_kinds. The Type keyword and the other cinema keywords leave an entry's kind unset.
    enum Kind { kNormal = 0, kHidden = 1, kNonSelectable = 2, kMovie = 3 };

    GmLevelManager();                   // PARTIAL, not 0x0013E150: see above
    virtual ~GmLevelManager();          // slot 0, PARTIAL, not 0x0013E9F0: see above
    virtual void Load(const char* path);  // slot 1, 0x0013E1F0
    virtual void Unload();              // slot 2, 0x0013E670

    void Reset();                       // 0x0013D950
    void ReleaseLevelData();            // 0x0013E530 NOT recovered: stops with a diagnostic in GmLevelPending.cpp

    static void SetHeap(void* heap);    // 0x0013D8E0

    int m_currentIndex;        // +0x004
    char m_currentName[9];     // +0x008
    char m_pad011[3];
    int m_previousIndex;       // +0x014
    int m_levelCount;          // +0x018
    int m_loaded;              // +0x01C
    int m_unknown020;          // +0x020 Reset sets 1
    char m_startPoint[8];      // +0x024 Reset sets "Start"
    int m_unknown02C[12];      // +0x02C
    int m_unknown05C;          // +0x05C
    int m_unknown060;          // +0x060
    char m_unknown064[0x13C];  // +0x064 not recovered: level content
    GmLevelNameArray m_names;  // +0x1A0
    char** m_descriptions;     // +0x1A8 null where a level has no description
    int* m_kinds;              // +0x1AC unset for an entry with no kind keyword: the heap is not cleared
    int* m_loadItems;          // +0x1B0 unset for an entry with no NumLoadItems, which includes every movie in Levels.txt
    char m_unknown1B4[0x3F0];  // +0x1B4 not recovered: level content

    static void* s_heap;       // 0x003B356C
    static GmLevelManager* s_instance;  // 0x003B3570
};

void AtCopyName8(char* to, const char* from);   // 0x00013F70 two words, or an empty name if `from` is null

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(GmLevelName) == 0x10, "GmLevelName");
static_assert(offsetof(GmLevelManager, m_previousIndex) == 0x14, "GmLevelManager::m_previousIndex");
static_assert(offsetof(GmLevelManager, m_startPoint) == 0x24, "GmLevelManager::m_startPoint");
static_assert(offsetof(GmLevelManager, m_unknown05C) == 0x5C, "GmLevelManager::m_unknown05C");
static_assert(offsetof(GmLevelManager, m_names) == 0x1A0, "GmLevelManager::m_names");
static_assert(offsetof(GmLevelManager, m_loadItems) == 0x1B0, "GmLevelManager::m_loadItems");
#pragma pop_macro("offsetof")
#endif
