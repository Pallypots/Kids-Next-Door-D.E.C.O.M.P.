// Lens flares, as read by the loader of ".ASL" text. Original address clusters 0x0003F8E0-0x0003FC24 and 0x00052190-0x00052969, with 0x00035800 and 0x0002E9A0. The words are the original's, from its table at
// 0x002B20A8; other names are inferred. No file on the disc is this format: the three files named .ASL there, in DESIGN\BEHAVIOR, are something else, compiled scripts, which 0x0003FAC0 reads as text and makes
// nothing of. Drawing them, 0x00052350-0x00052CB6, is not recovered.
#pragma once

#include "at/AtMatAssignment.h"
#include "at/AtNameTableOwner.h"
#include "at/AtQuaternion.h"

// Four reals, a colour, read as a quaternion is.
struct AtLensColor : public AtQuaternion {
    void SetWhite();    // 0x00035800 all 1. Also used by 0x00050FD8.
};

// One of a lens flare's squares. 0x10 bytes.
struct AtLensFlareSquare {
    unsigned int m_material;     // +0x00 in the flare's list; out of range is reported, and kept
    float m_amountTowardsLight;  // +0x04
    float m_halfSize;            // +0x08 half of SquareSize
    int m_rotate;                // +0x0C
};

// 0x60 bytes.
class AtLensFlare {
public:
    enum Flags { kDistanceColoring = 1, kDepthColoring = 2 };

    AtLensFlare();                // 0x000521A0
    ~AtLensFlare();               // 0x00052920 resets it, and its list's own destructor runs again on nothing
    void Reset();                 // 0x000522E0

    // 0x00052970. Its body, a left curly through its right curly. Colours are given as 0 to 1, and kept times 255. Materials are looked up on the owner's object at +4. Text that ends inside the body never leaves the loop, unless the report stops.
    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);
    void ReadSquare(AtLensFlareSquare* square, AtIoTokenizer* tokenizer);  // 0x00052230 "Flare { ... }"; m_rotate alone is set if its word is absent

    static void SetHeap(void* heap);  // 0x00052190

    AtNameTableOwner* m_owner;    // +0x00
    unsigned int m_flags;         // +0x04 both set from new read
    AtMatAssignment m_materials;  // +0x08
    AtLensColor m_centerColor;    // +0x10
    AtLensColor m_innerColor;     // +0x20
    AtLensColor m_outerColor;     // +0x30
    float m_innerStart;           // +0x40 1 from new
    float m_outerStart;           // +0x44 1 from new
    float m_innerEnd;             // +0x48 500 from new
    float m_outerEnd;             // +0x4C 1000 from new
    unsigned int m_squareCount;   // +0x50
    AtLensFlareSquare* m_squares; // +0x54
    float m_unknown58;            // +0x58 1 from new
    int m_unknown5C;              // +0x5C

    static void* s_heap;          // 0x002C7E24
};

// Vftable 0x002760BC. Concrete. 0x1C bytes.
class AtLensFlareDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 13 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B20A8

    AtLensFlareDatabase();                  // 0x0003F8F0
    ~AtLensFlareDatabase() override;        // slot 0; 0x0002E9A0 vector deleting, 0x0003FBA0 complete
    void Clear() override;                  // slot 1, 0x0003FB60

    void Load(AtNameTableOwner* owner, const char* name, int unused);  // 0x0003FAC0 name + ".ASL"
    // 0x0003F910. The word before the count is taken on trust: whichever it is, no report.
    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);

    static void SetHeap(void* heap);        // 0x0003F8E0

    unsigned int m_count;    // +0x14
    AtLensFlare* m_flares;   // +0x18

    static void* s_heap;     // 0x002C6418
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtLensFlareSquare) == 0x10, "AtLensFlareSquare");
static_assert(offsetof(AtLensFlare, m_centerColor) == 0x10, "AtLensFlare::m_centerColor");
static_assert(offsetof(AtLensFlare, m_innerStart) == 0x40, "AtLensFlare::m_innerStart");
static_assert(offsetof(AtLensFlare, m_squares) == 0x54, "AtLensFlare::m_squares");
static_assert(sizeof(AtLensFlare) == 0x60, "AtLensFlare");
static_assert(sizeof(AtLensFlareDatabase) == 0x1C, "AtLensFlareDatabase");
#pragma pop_macro("offsetof")
#endif
