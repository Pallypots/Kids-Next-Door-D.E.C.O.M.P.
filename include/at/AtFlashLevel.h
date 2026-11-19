// A level of the Flash user interface: an .AFD file ("Loader { Level Sprite Shape Text Script }") names what it is made of, each in a file of its own, .AFL ("Atlas Flash Level Definition File" by its own first line) among them. The keyword
// names are the original's, from tables 0x00270E14 and 0x00270E4C; other names are inferred.
// The readers are what is recovered. The class starts at 0x00011440, and its constructor and destructor need the player at +0x0C, which is not; see docs/worklog/2026-11-07-afd.md.
#pragma once

#include "at/AtFlashScript.h"
#include "at/AtFlashShape.h"
#include "at/AtFlashSprite.h"
#include "at/AtFlashText.h"

class AtFlashLevel {
public:
    // 0x00011770, name + ".AFD". What each word names is loaded at once, in the file's order. Sound is read and nothing is done with it. The second argument goes to Load, which does not read it.
    void ReadDefinition(const char* name, int unused);

    // 0x000114A0, name + ".AFL". Sets defaults, reads, and hands out a heap. The name is copied first, to eight bytes. A HeapSize in the file is read and thrown away, so the file cannot give the level
    // a heap of its own: the branch that would make one is never taken. The second argument is not read.
    void Load(const char* name, int unused);

    void Reset();                      // 0x000113B0 the destructor's end, and the constructor's

    char m_unknown000[8];
    int m_unknown008;                  // +0x008
    char m_player[0xC4 - 0x0C];        // +0x00C vftable 0x00271994, constructor 0x00014300; not recovered
    AtFlashScriptTable m_scripts;      // +0x0C4
    AtFlashShapeTable m_shapes;        // +0x154
    char m_sounds[0x4C];               // +0x190 vftable 0x002710E0, constructor 0x000128E0; not recovered. The file's word Sound does not reach it.
    void* m_heap1DC;                   // +0x1DC the sounds' heap
    AtFlashSpriteTable m_sprites;      // +0x1E0
    AtFlashTextTable m_texts;          // +0x248
    int m_unknown31C;                  // +0x31C
    AtFlashEnvironment* m_environment; // +0x320
    int m_unknown324;                  // +0x324
    int m_read;                        // +0x328 set by the word Level
    unsigned int m_heapSize;           // +0x32C always 0 after Load
    int m_unknown330;                  // +0x330 20000; nothing here reads it
    int m_maxVariableValueLength;      // +0x334 1000 by default
    int m_maxVariables;                // +0x338 10000 by default
    int m_fontSourceLevel;             // +0x33C -1 by default and for the word Self, -2 for All
    int m_unknown340;                  // +0x340
    char m_name[8];                    // +0x344
    void* m_heapInUse;                 // +0x34C
    void* m_block;                     // +0x350 what was taken for the heap of its own, from the heap it fell back on
    void* m_heap;                      // +0x354 kept if already set; otherwise AtHeapStack::s_contentHeap
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtFlashLevel, m_scripts) == 0xC4, "AtFlashLevel::m_scripts");
static_assert(offsetof(AtFlashLevel, m_shapes) == 0x154, "AtFlashLevel::m_shapes");
static_assert(offsetof(AtFlashLevel, m_heap1DC) == 0x1DC, "AtFlashLevel::m_heap1DC");
static_assert(offsetof(AtFlashLevel, m_sprites) == 0x1E0, "AtFlashLevel::m_sprites");
static_assert(offsetof(AtFlashLevel, m_texts) == 0x248, "AtFlashLevel::m_texts");
static_assert(offsetof(AtFlashLevel, m_environment) == 0x320, "AtFlashLevel::m_environment");
static_assert(offsetof(AtFlashLevel, m_heapSize) == 0x32C, "AtFlashLevel::m_heapSize");
static_assert(offsetof(AtFlashLevel, m_heap) == 0x354, "AtFlashLevel::m_heap");
#pragma pop_macro("offsetof")
#endif
