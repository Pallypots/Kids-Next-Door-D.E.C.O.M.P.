// The class with vftable 0x00274A78, of which only the part the loaders use is recovered. Its constructor is not recovered either; these members' own constructors stand in. Names are inferred.
// Sound lists know it by its first two tables, scenes by the rest. These are one class: the add methods are consecutive and the same code on successive tables (0x00031A30, 0x00031A60, 0x00031A90), and
// each caller's offsets fit one layout. A different class from AtMaterialOwner, whose tables sit elsewhere.
#pragma once

#include "gm/GmNameMap.h"

class AtAudioBankLoader;  // include/at/AtAudioBank.h
class AtMatrixDevice;     // include/at/AtCoordSys.h
class AtSceneDevice;      // include/at/AtScene.h
class AtSceneNode;        // include/at/AtScene.h

struct AtNameTableOwner {
    // The image draw lists (0x0003B0C0, 0x0003B490) use these two as well, on an owner of their own, so they are not only for sound.
    void AddToFirst(GmNameMap* map, const char* ns);   // 0x00031A30 a null ns means the plain group; an empty one is a name space with an empty name. Holds sound banks. src/at/AtAudioBank.cpp
    void AddToSecond(GmNameMap* map, const char* ns);  // 0x00031A60 the same. Holds sound segments. src/at/AtAudioBank.cpp
    void AddScene(GmNameMap* scene, const char* ns);   // 0x00031A90 the same. Also called from 0x00025360, on what is not established. src/at/AtSceneNode.cpp
    // The original has this inline in 0x00031C50. Here an empty ns does mean the plain group, unlike the add methods.
    void* FindInFirst(const char* name, const char* ns) { return ns != 0 && ns[0] != 0 ? m_firstSpaces.Find(ns, name) : m_first.Find(name); }
    AtSceneNode* FindNode(const char* name, const char* ns);       // 0x00045680 ns may be empty or null
    void* FindBoundingGeometry(const char* name, const char* ns);  // 0x000499B0 ns may be empty or null

    int m_unknown00;                   // +0x00
    AtSceneDevice* m_device;           // +0x04
    AtMatrixDevice* m_unknown08;       // +0x08 where emitters take their matrices from. 0x0011ED90, 0x0011EE30.
    AtAudioBankLoader* m_loader;       // +0x0C
    int m_unknown10;                   // +0x10
    int m_unknown14;                   // +0x14
    GmNameMapGroup m_first;            // +0x18
    GmNameSpaceDb m_firstSpaces;       // +0x30
    GmNameMapGroup m_second;           // +0x54
    GmNameSpaceDb m_secondSpaces;      // +0x6C
    GmNameMapGroup m_nodes;            // +0x90
    GmNameSpaceDb m_nodeSpaces;        // +0xA8
    GmNameMapGroup m_modifiers;        // +0xCC
    GmNameSpaceDb m_modifierSpaces;    // +0xE4
    char m_unknown108[0x78];
    GmNameMapGroup m_geometry;         // +0x180
    GmNameSpaceDb m_geometrySpaces;    // +0x198
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtNameTableOwner, m_device) == 0x04, "AtNameTableOwner::m_device");
static_assert(offsetof(AtNameTableOwner, m_loader) == 0x0C, "AtNameTableOwner::m_loader");
static_assert(offsetof(AtNameTableOwner, m_first) == 0x18, "AtNameTableOwner::m_first");
static_assert(offsetof(AtNameTableOwner, m_firstSpaces) == 0x30, "AtNameTableOwner::m_firstSpaces");
static_assert(offsetof(AtNameTableOwner, m_second) == 0x54, "AtNameTableOwner::m_second");
static_assert(offsetof(AtNameTableOwner, m_secondSpaces) == 0x6C, "AtNameTableOwner::m_secondSpaces");
static_assert(offsetof(AtNameTableOwner, m_nodes) == 0x90, "AtNameTableOwner::m_nodes");
static_assert(offsetof(AtNameTableOwner, m_nodeSpaces) == 0xA8, "AtNameTableOwner::m_nodeSpaces");
static_assert(offsetof(AtNameTableOwner, m_modifiers) == 0xCC, "AtNameTableOwner::m_modifiers");
static_assert(offsetof(AtNameTableOwner, m_modifierSpaces) == 0xE4, "AtNameTableOwner::m_modifierSpaces");
static_assert(offsetof(AtNameTableOwner, m_geometry) == 0x180, "AtNameTableOwner::m_geometry");
static_assert(offsetof(AtNameTableOwner, m_geometrySpaces) == 0x198, "AtNameTableOwner::m_geometrySpaces");
#pragma pop_macro("offsetof")
#endif
