// What an .ASD file lists, and the loading of it. An .ASD is to a scene what an .AGD is to a set of graphics: a list of the other files to read, by kind, and it holds what they make. It ties together, for one object or location, .ASN (the
// nodes), .ASM (modifiers), .ASG (a space graph), .ASB (bounding geometry), .ASE (emitters), .ASL (lens flares), .ASC (cell graphs), .AIS (strings), .ASI (flat images) and .AST (flat text), and further .ASD files. Original address cluster
// 0x0002E200-0x000301C5, with 0x0002F7F0 the loader. The words are the original's, from table 0x002B13A0; "DatabaseLoader" is the original's own word for this class's kind, the class name is inferred. It reads no format's own text: every word
// here is a list of leaf names, each opened in the directory of the .ASD.
//
//   NodeDatabase[n] { "name" ... }   and likewise ModifierDatabase, SpaceGraph, BoundingGeometryDatabase, DatabaseLoader, EmitterDatabase, LensFlareDatabase, CellGraphDatabase, StringDatabase, ImageDrawDatabase, TextDrawDatabase
//
// The 409 real files use only NodeDatabase, BoundingGeometryDatabase, EmitterDatabase, ModifierDatabase, SpaceGraph and CellGraphDatabase. Walking the nodes for two things the game calls after loading, 0x0002ED70 and 0x0002EDD0, is not recovered.
#pragma once

#include "at/AtBoundingGeometry.h"
#include "at/AtCellGraph.h"
#include "at/AtDrawItem.h"
#include "at/AtEmitter.h"
#include "at/AtLensFlare.h"
#include "at/AtModifier.h"
#include "at/AtNameTableOwner.h"
#include "at/AtScene.h"
#include "at/AtSpaceGraph.h"
#include "at/AtText.h"

// Vftable 0x002749C4. 0x78 bytes.
class AtSceneDatabaseLoader {
public:
    enum { kKeywordCount = 11 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B13A0

    AtSceneDatabaseLoader();                  // 0x0002EE30
    virtual ~AtSceneDatabaseLoader();         // slot 0; 0x0002F790 vector deleting, 0x0002F660 complete
    virtual void Clear();                     // slot 1, 0x0002F470

    // 0x0002F7F0. name + ".ASD": reads its lists, closes it, then loads everything listed in a fixed order, whatever order the file gave, with the file's directory current. mode goes on to each load. Clears first if loaded.
    void Load(AtNameTableOwner* owner, const char* name, int mode);
    // 0x0002EED0. In order: strings, flat text, flat images, bounding geometry, modifiers, emitters, lens flares, nodes, cell graphs, space graphs; the names freed, if asked; then nested loaders.
    void LoadDatabases(AtNameTableOwner* owner, int mode, int freeNames);

    AtScene* NodeDatabaseAt(unsigned int index);          // 0x0002ED20
    AtSpaceGraph* SpaceGraphAt(unsigned int index);       // 0x0002ED40
    AtSceneDatabaseLoader* LoaderAt(unsigned int index);  // 0x0002ED60
    // What the game calls right after loading a list: each scene's nodes put in a space graph, then the nested lists'; taking them out, the nested lists first. In src/at/AtSceneWalk.cpp.
    void InsertInto(AtSpaceGraph* graph);                 // 0x0002ED70
    void RemoveFrom(AtSpaceGraph* graph);                 // 0x0002EDD0

    static void SetHeap(void* heap);          // 0x0002E200

    AtNameTableOwner* m_owner;                // +0x04
    AtScene* m_nodes;                         // +0x08
    AtModifierDatabase* m_modifiers;          // +0x0C
    AtEmitterDatabase* m_emitters;            // +0x10
    AtLensFlareDatabase* m_lensFlares;        // +0x14
    AtSpaceGraph* m_spaceGraphs;              // +0x18
    AtBoundingGeometryDatabase* m_geometry;   // +0x1C
    AtCellGraphDatabase* m_cellGraphs;        // +0x20
    AtTextTable* m_strings;                   // +0x24
    AtImageDrawDatabase* m_imageDraws;        // +0x28
    AtTextDrawDatabase* m_textDraws;          // +0x2C
    AtSceneDatabaseLoader* m_loaders;         // +0x30
    GmNameMap m_nodesByName;                  // +0x34 by the leaf of each name
    GmNameMap m_spaceGraphsByName;            // +0x48 likewise; it goes on the owner's m_spaceGraphs when made
    unsigned short m_nodeCount;               // +0x5C
    unsigned short m_modifierCount;           // +0x5E
    unsigned short m_emitterCount;            // +0x60
    unsigned short m_lensFlareCount;          // +0x62
    unsigned short m_cellGraphCount;          // +0x64
    unsigned short m_spaceGraphCount;         // +0x66
    unsigned short m_geometryCount;           // +0x68
    unsigned short m_stringCount;             // +0x6A
    unsigned short m_imageDrawCount;          // +0x6C
    unsigned short m_textDrawCount;           // +0x6E
    unsigned short m_loaderCount;             // +0x70
    char** m_loaderNames;                     // +0x74 its own, where the others' are shared; so a nested loader can overwrite the others', which by then are freed

    // The lists of names, while a file's are being loaded. All instances share them.
    static char** s_nodeNames;                // 0x002C63A0
    static char** s_modifierNames;            // 0x002C63A4
    static char** s_emitterNames;             // 0x002C63A8
    static char** s_lensFlareNames;           // 0x002C63AC
    static char** s_cellGraphNames;           // 0x002C63B0
    static char** s_spaceGraphNames;          // 0x002C63B4
    static char** s_geometryNames;            // 0x002C63B8
    static char** s_stringNames;              // 0x002C6398
    static char** s_imageDrawNames;           // 0x002C63BC
    static char** s_textDrawNames;            // 0x002C63C0
    static void* s_heap;                      // 0x002C639C for the arrays this class makes. The names are on the token file's heap.

private:
    void MakeImageDraws();                    // 0x0002E210
    void MakeTextDraws();                     // 0x0002E330
    void MakeStrings();                       // 0x0002E450
    void MakeNodes();                         // 0x0002E570
    void MakeModifiers();                     // 0x0002E6A0
    void MakeEmitters();                      // 0x0002E7C0
    void MakeLensFlares();                    // 0x0002E8E0
    void MakeCellGraphs();                    // 0x0002EA00
    void MakeSpaceGraphs();                   // 0x0002EB20
    void MakeGeometry();                      // 0x0002EC00
    void MakeLoaders();                       // 0x0002F6D0
    void Zero();
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtSceneDatabaseLoader) == 0x78, "AtSceneDatabaseLoader");
static_assert(offsetof(AtSceneDatabaseLoader, m_loaders) == 0x30, "AtSceneDatabaseLoader::m_loaders");
static_assert(offsetof(AtSceneDatabaseLoader, m_nodesByName) == 0x34, "AtSceneDatabaseLoader::m_nodesByName");
static_assert(offsetof(AtSceneDatabaseLoader, m_spaceGraphsByName) == 0x48, "AtSceneDatabaseLoader::m_spaceGraphsByName");
static_assert(offsetof(AtSceneDatabaseLoader, m_nodeCount) == 0x5C, "AtSceneDatabaseLoader::m_nodeCount");
static_assert(offsetof(AtSceneDatabaseLoader, m_loaderCount) == 0x70, "AtSceneDatabaseLoader::m_loaderCount");
static_assert(offsetof(AtSceneDatabaseLoader, m_loaderNames) == 0x74, "AtSceneDatabaseLoader::m_loaderNames");
#pragma pop_macro("offsetof")
#endif
