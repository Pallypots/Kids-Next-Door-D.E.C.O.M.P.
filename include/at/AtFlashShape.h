// The Flash user interface's shapes: .AFH files ("Atlas Flash Shape Definition File" by their own first line), each a rectangle and a mesh out of the graphics database the file names. Original address clusters 0x00012BD0-0x00012CEF,
// 0x00013050-0x0001337F, 0x00019560-0x000196D5. The keywords are the original's, from table 0x002710FC; other names are inferred.
#pragma once

#include "at/AtFlashTextFormat.h"

class AtDatabaseLoader;
struct AtMaterialOwner;

// What the owner's slot 18 returns. Only the two slots this file calls are known; nothing here says what else it is.
class AtFlashGraphicsFactory {
public:
    virtual void Slot0() {}
    virtual void Slot1() {}
    virtual void Slot2() {}
    virtual void Slot3() {}
    virtual void Slot4() {}
    virtual void Slot5() {}
    virtual void Slot6() {}
    virtual void Slot7() {}
    virtual void Slot8() {}
    virtual void Slot9() {}
    virtual void Slot10() {}
    virtual void Slot11() {}
    virtual void Slot12() {}
    virtual void Slot13() {}
    virtual void Slot14() {}
    virtual void Slot15() {}
    virtual void Slot16() {}
    virtual void Slot17() {}
    virtual void Slot18() {}
    virtual void Slot19() {}
    virtual void Slot20() {}
    virtual void Slot21() {}
    virtual void Slot22() {}
    virtual void Slot23() {}
    virtual void Slot24() {}
    virtual void Slot25() {}
    virtual void Slot26() {}
    virtual void Slot27() {}
    virtual void Slot28() {}
    virtual AtDatabaseLoader* MakeLoader() = 0;             // slot 29
    virtual void FreeLoader(AtDatabaseLoader* loader) = 0;  // slot 30
};

// What the level is handed, as far as the loaders go. Neither class is recovered. The application's +0x260 is a member with the material owner at +0x10.
struct AtFlashGraphics {
    char m_unknown00[0x10];
    AtMaterialOwner* m_owner;          // +0x10
};
struct AtFlashApplication {
    char m_unknown000[0x260];
    AtFlashGraphics* m_graphics;       // +0x260
};
struct AtFlashEnvironment {            // The class that reads .AFE, by its callers at 0x0002BBC8 to 0x0002CCFE.
    AtFlashApplication* m_application; // +0x000
    char m_unknown004[0x1F0];
    int m_platformSpecificFiles;       // +0x1F4 not 0 means .XFF over .AFF. The name guesses at the .AFE word of that name; the .AFE reader is not recovered.
};

// One Shape: 0x38 bytes. Vftable 0x00272210.
class AtFlashShape : public AtFlashResource {
public:
    AtFlashShape();                    // 0x00019600
    ~AtFlashShape();                   // 0x00019670 not virtual
    void Read(AtIoTokenizer* tokenizer) override;  // slot 0, 0x00019660 only jumps to ReadData
    void ReadData(AtIoTokenizer* tokenizer);       // 0x00019560 the mesh's name is read and freed

    float m_rectangle[4];              // +0x1C
    int m_mesh;                        // +0x2C -1 until read
    void* m_unknown30;                 // +0x30
    void* m_heap;                      // +0x34
};

// Vftable 0x00271158. The shapes of a level: Shapes\name.AFH, and the graphics database it names.
class AtFlashShapeTable : public AtFlashResourceTable {
public:
    struct TextureRef {
        unsigned char m_references;    // +0x00
        void* m_texture;               // +0x04
    };

    AtFlashShapeTable();               // 0x00012BD0
    void Allocate(unsigned int count) override;  // slot 0, 0x00013050
    ~AtFlashShapeTable() override;     // slot 1; 0x00013360 deleting, 0x00013140 complete
    void Release(unsigned int index) override;   // slot 2, 0x00012F70 not recovered: stops with a diagnostic. Only what draws a shape gets here.

    void ClearShapes();                // 0x00012C00 nothing unless loaded
    char* ReadFile(const char* name);  // 0x00012CF0 the file's items; returns the name of the graphics database, a new copy
    void Load(const char* name, int mode, AtFlashApplication* application);  // 0x00013190

    int m_loaded;                      // +0x28
    AtMaterialOwner* m_owner;          // +0x2C only set if the file has shapes
    AtDatabaseLoader* m_loader;        // +0x30
    TextureRef* m_textures;            // +0x34
    unsigned int m_textureCount;       // +0x38
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtFlashApplication, m_graphics) == 0x260, "AtFlashApplication::m_graphics");
static_assert(offsetof(AtFlashEnvironment, m_platformSpecificFiles) == 0x1F4, "AtFlashEnvironment::m_platformSpecificFiles");
static_assert(offsetof(AtFlashShape, m_rectangle) == 0x1C, "AtFlashShape::m_rectangle");
static_assert(offsetof(AtFlashShape, m_heap) == 0x34, "AtFlashShape::m_heap");
static_assert(sizeof(AtFlashShape) == 0x38, "AtFlashShape");
static_assert(sizeof(AtFlashShapeTable::TextureRef) == 8, "AtFlashShapeTable::TextureRef");
static_assert(offsetof(AtFlashShapeTable, m_textureCount) == 0x38, "AtFlashShapeTable::m_textureCount");
static_assert(sizeof(AtFlashShapeTable) == 0x3C, "AtFlashShapeTable");
#pragma pop_macro("offsetof")
#endif
