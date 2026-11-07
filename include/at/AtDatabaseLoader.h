// What an .AGD file lists, and the loading of it. An .AGD is a list of the other files to read, by kind: TextureDatabase[n] { "name" ... }, and so on. The words are the original's, from table 0x002B0190, and "DatabaseLoader"
// is the original's own word for this class's kind. Original address cluster 0x000196E0-0x0001BF8B. What a kind's list holds is made in slots a concrete class fills, at vftable 0x0028A7D0 in the graphics layer, which is
// not recovered.
//
// Four kinds are behind a seam that stops with a diagnostic: CoordSysMirrorDatabase, WeightsDatabase, MorphAnimationDatabase and LightAnimationDatabase. No .AGD on the disc uses them. See docs/worklog/2026-11-06-agd.md.
#pragma once

#include "at/AtAnimationDatabase.h"
#include "at/AtCoordSys.h"
#include "at/AtImageDatabase.h"
#include "at/AtLight.h"
#include "at/AtMatAnimation.h"
#include "at/AtMaterialDatabase.h"
#include "at/AtMeshDatabase.h"
#include "at/AtShaderDatabase.h"
#include "at/AtTextureDatabase.h"

// Vftable 0x00272370. Slots 0 to 10 and 13 to 22 are the concrete class's.
class AtDatabaseLoader {
public:
    enum { kKeywordCount = 18 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B0190

    AtDatabaseLoader();                       // 0x0001A440

    // Each is called with the count known and before the names are read.
    virtual void MakeTextures() = 0;          // slot 0
    virtual void MakeMaterials() = 0;         // slot 1
    virtual void MakeShaders() = 0;           // slot 2
    virtual void MakeMeshes() = 0;            // slot 3
    virtual void MakeLights() = 0;            // slot 4
    virtual void Slot5() = 0;
    virtual void MakeFonts() = 0;             // slot 6
    virtual void MakeImages() = 0;            // slot 7
    virtual void MakeVertexProcesses() = 0;   // slot 8
    virtual void MakePixelProcesses() = 0;    // slot 9
    virtual void MakeLoaders() = 0;           // slot 10
    virtual ~AtDatabaseLoader();              // slot 11; 0x0001BF70 deleting, 0x0001AD40 complete
    virtual void Unload();                    // slot 12, 0x0001AB10
    virtual AtTextureDatabase* TextureAt(unsigned int index) = 0;          // slot 13
    virtual AtMaterialDatabase* MaterialAt(unsigned int index) = 0;        // slot 14
    virtual AtShaderDatabase* ShaderAt(unsigned int index) = 0;            // slot 15
    virtual AtMeshDatabase* MeshAt(unsigned int index) = 0;                // slot 16
    virtual AtLightDatabase* LightAt(unsigned int index) = 0;              // slot 17
    virtual AtFontDatabase* FontAt(unsigned int index) = 0;                // slot 18
    virtual AtImageDatabase* ImageAt(unsigned int index) = 0;              // slot 19
    virtual AtVertexProcessDatabase* VertexProcessAt(unsigned int index) = 0;  // slot 20
    virtual AtPixelProcessDatabase* PixelProcessAt(unsigned int index) = 0;    // slot 21
    virtual AtDatabaseLoader* LoaderAt(unsigned int index) = 0;            // slot 22

    // 0x0001ADC0. name + ".AGD": reads its lists, closes it, then loads everything listed in a fixed order, whatever order the file gave. mode goes on to each load. prefix is for images alone. With inDirectory, the file's own
    // directory is current for what it lists.
    void Load(AtMaterialOwner* owner, const char* name, int mode, const char* prefix, int inDirectory);

    // 0x0001A4B0. In order: vertex and pixel processes, textures, shaders, materials, meshes, lights, coordinate systems, animations, material lists, fonts, images, material animations; the names freed, if asked; then nested loaders.
    void LoadDatabases(AtMaterialOwner* owner, int mode, const char* prefix, int freeNames);
    void FreeNames();                         // 0x000198C0

    static void SetHeap(void* heap);          // 0x000196E0

    // The lists of names, while a file's are being loaded. All instances share them. Nested loaders overwrite them, which is safe only because a loader's own are freed before its nested ones are loaded. A constructor empties them, so
    // a list before DatabaseLoader in a file is lost if MakeLoaders constructs its loaders then, and Load crashes on it. No file on the disc uses DatabaseLoader.
    static char** s_textureNames;             // 0x002C62BC
    static char** s_shaderNames;              // 0x002C62A4
    static char** s_materialNames;            // 0x002C629C
    static char** s_meshNames;                // 0x002C6298
    static char** s_lightNames;               // 0x002C62C4
    static char** s_coordSystemNames;         // 0x002C62AC
    static char** s_mirrorNames;              // 0x002C62D4
    static char** s_weightsNames;             // 0x002C62B8
    static char** s_animationNames;           // 0x002C62C0
    static char** s_matAssignmentNames;       // 0x002C62C8
    static char** s_matAnimationNames;        // 0x002C62D0
    static char** s_morphAnimationNames;      // 0x002C62B0
    static char** s_lightAnimationNames;      // 0x002C62B4
    static char** s_fontNames;                // 0x002C62CC
    static char** s_imageNames;               // 0x002C62A8
    static char** s_vertexProcessNames;       // 0x002C62D8
    static char** s_pixelProcessNames;        // 0x002C62A0
    static void* s_heap;                      // 0x002C62DC for the arrays this class makes itself. The names are on the token file's heap.

    AtMaterialOwner* m_owner;                 // +0x04 set while something is loaded
    AtCoordSysDatabase* m_coordSystems;       // +0x08
    void* m_mirrors;                          // +0x0C not recovered: stays null
    void* m_weights;                          // +0x10 not recovered: stays null
    AtAnimationDatabase** m_animations;       // +0x14 the device's
    AtMatAssignmentDatabase* m_matAssignments;  // +0x18
    AtMatAnimationDatabase* m_matAnimations;  // +0x1C
    void* m_morphAnimations;                  // +0x20 not recovered: stays null
    void* m_lightAnimations;                  // +0x24 not recovered: stays null
    int m_unknown28;                          // +0x28 16; nothing here reads it
    int m_unknown2C;                          // +0x2C 16; nothing here reads it
    GmNameMap m_coordSystemMap;               // +0x30 by the leaf of each file's name, to its database
    GmNameMap m_mirrorMap;                    // +0x44
    GmNameMap m_weightsMap;                   // +0x58
    GmNameMap m_animationMap;                 // +0x6C
    unsigned short m_textureCount;            // +0x80
    unsigned short m_shaderCount;             // +0x82
    unsigned short m_materialCount;           // +0x84
    unsigned short m_meshCount;               // +0x86
    unsigned short m_lightCount;              // +0x88
    unsigned short m_coordSystemCount;        // +0x8A
    unsigned short m_mirrorCount;             // +0x8C
    unsigned short m_weightsCount;            // +0x8E
    unsigned short m_animationCount;          // +0x90
    unsigned short m_matAssignmentCount;      // +0x92
    unsigned short m_matAnimationCount;       // +0x94
    unsigned short m_morphAnimationCount;     // +0x96
    unsigned short m_lightAnimationCount;     // +0x98
    unsigned short m_unknown9A;               // +0x9A nothing here touches it
    unsigned short m_fontCount;               // +0x9C
    unsigned short m_imageCount;              // +0x9E
    unsigned short m_vertexProcessCount;      // +0xA0
    unsigned short m_pixelProcessCount;       // +0xA2
    unsigned short m_loaderCount;             // +0xA4
    char** m_loaderNames;                     // +0xA8 a member, unlike the others: it is live while nested loaders use the statics
    AtDatabaseLoader* m_child;                // +0xAC nothing here sets it. Unload unloads and deletes it

private:
    void Reset();                             // 0x00019AE0 also the statics
    void MakeCoordSystems();                  // 0x00019BF0
    void MakeAnimations();                    // 0x0001ACA0
    void MakeMatAssignments();                // 0x00019FB0
    void MakeMatAnimations();                 // 0x0001A0D0
    void MakeMirrors();                       // 0x00019D30 not recovered: stops
    void MakeWeights();                       // 0x00019E70 not recovered: stops
    void MakeMorphAnimations();               // 0x0001A1F0 not recovered: stops
    void MakeLightAnimations();               // 0x0001A310 not recovered: stops
};

void AtFreeNameList(char** list, unsigned int count);     // 0x000196F0 the original takes a this it does not use. Null entries are passed over. Also called from 0x000BB1C2.
void AtLeafName(const char* path, char* out);             // 0x00019730 after the last \, else /, else :; up to the first dot; at most eight bytes, terminated only if shorter
void AtJoinPrefix(char* out, char* name, const char* prefix);  // 0x000197D0 the original takes a this it does not use. "a\b" and "p" give "a\p\b"; "b" gives "p\b". name is changed and put back

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtDatabaseLoader, m_owner) == 0x04, "AtDatabaseLoader::m_owner");
static_assert(offsetof(AtDatabaseLoader, m_lightAnimations) == 0x24, "AtDatabaseLoader::m_lightAnimations");
static_assert(offsetof(AtDatabaseLoader, m_coordSystemMap) == 0x30, "AtDatabaseLoader::m_coordSystemMap");
static_assert(offsetof(AtDatabaseLoader, m_animationMap) == 0x6C, "AtDatabaseLoader::m_animationMap");
static_assert(offsetof(AtDatabaseLoader, m_textureCount) == 0x80, "AtDatabaseLoader::m_textureCount");
static_assert(offsetof(AtDatabaseLoader, m_fontCount) == 0x9C, "AtDatabaseLoader::m_fontCount");
static_assert(offsetof(AtDatabaseLoader, m_loaderCount) == 0xA4, "AtDatabaseLoader::m_loaderCount");
static_assert(offsetof(AtDatabaseLoader, m_loaderNames) == 0xA8, "AtDatabaseLoader::m_loaderNames");
static_assert(sizeof(AtDatabaseLoader) == 0xB0, "AtDatabaseLoader");
#pragma pop_macro("offsetof")
#endif
