// What the game's fatal error screen draws with: rectangles, colours in reals, an image's strips, and the camera. Original address clusters 0x000412D0-0x00041436,
// 0x00042280-0x000423E6, 0x00035840-0x000359A1, 0x0005F600-0x0005F719, 0x000582F0-0x00058611, 0x00037EB0-0x00037EFD and 0x00013F00. All names are inferred.
// Bodies in src/at/AtGr2D.cpp; AtImage::Draw is declared with the image (include/at/AtImage.h), and the colour's and the colour rectangle's members with them (include/at/AtMaterial.h).
//
// The renderer and the camera's own class belong to the graphics layer, which is not recovered: AtGr2DRenderer is a seam whose slots stop with a diagnostic, and AtCamera is known
// only by the one member recovered here and the fields it touches.
//
// Arithmetic: see at/AtVector3.h. `double` for what the original keeps in an x87 register, `float` for what it stores. The game's main thread runs the x87 at 53 bits.
#pragma once

#include "at/AtCoordSys.h"
#include "at/AtEmitter.h"
#include "at/AtImage.h"
#include "at/AtMaterial.h"

class AtCamera;

// Four floats: left, top, right, bottom.
struct AtRect {
    // 0x0005F600. this becomes rect clipped to clip. If nothing is left, 0, and uv is untouched. Otherwise each side that was cut moves the same side of uv in proportion, and 1. A side
    // is cut when clip's is strictly inside. A rectangle with a NaN is not empty.
    int Clip(const AtRect& clip, const AtRect& rect, AtRect* uv);

    float left;    // +0x00
    float top;     // +0x04
    float right;   // +0x08
    float bottom;  // +0x0C
};

// A colour in four reals, 0 to 1.
struct AtColorF {
    // The same body as AtFpPlaceObject2Tag::SetColorScale, 0x00023CB0, whose this is in truth its four floats (include/at/AtFlashSprite.h): each byte times 1/255. Not recorded twice.
    void SetBytes(const AtColor& color);
    void Lerp(const AtColorF& from, const AtColorF& to, float t);        // 0x00035840 from + (to - from) * t; see the source for where it rounds
    void LerpBytes(const AtColor& from, const AtColor& to, float t);     // 0x000358D0 both made reals first

    float r, g, b, a;
};

// What an image's m_data points to in the graphics layer's images, as Draw reads it: one record for each strip, m_unknown1E of them. 0x18 bytes. Strips whose x0 is the first strip's
// begin a row.
struct AtImageStrip {
    int m_unknown00;   // +0x00
    int m_unknown04;   // +0x04
    int x0;            // +0x08
    int y0;            // +0x0C
    int x1;            // +0x10
    int y1;            // +0x14
};

// What App+0x5F9C points to: the 2D renderer of the graphics layer, known by the two slots the fatal error screen calls, and the fields AtImage::Draw reads. Not recovered.
class AtGr2DRenderer {
public:
    enum Flags { kQuarterPixelY = 0x100, kQuarterPixelX = 0x200 };  // a strip is moved back by a quarter of a pixel, not a half, on that axis

    virtual void Slot0();
    virtual void Slot1();
    virtual void Slot2();
    virtual void Begin(AtCamera* camera, int a, int b);  // slot 3: the screen passes (camera, 7, 0)
    virtual void Slot4();
    virtual void Slot5();
    virtual void Slot6();
    virtual void Slot7();
    virtual void Slot8();
    virtual void Slot9();
    virtual void Slot10();
    virtual void Slot11();
    virtual void Slot12();
    virtual void Slot13();
    virtual void Slot14();
    virtual void Slot15();
    virtual void Slot16();
    virtual void Slot17();
    virtual void Slot18();
    virtual void Slot19();
    virtual void Slot20();
    virtual void Slot21();
    virtual void Slot22();
    virtual void Slot23();
    virtual void Slot24();
    virtual void Slot25();
    virtual void Slot26();
    virtual void Slot27();
    virtual void Slot28();
    virtual void Slot29();
    virtual void Slot30();
    virtual void Slot31();
    virtual void Slot32();
    virtual void Slot33();
    virtual void Slot34();
    virtual void Slot35();
    virtual void Slot36();
    virtual void Slot37();
    virtual void Slot38();
    virtual void Slot39();
    virtual void Slot40();
    virtual void Slot41();
    virtual void Slot42();
    virtual void Slot43();
    virtual void Slot44();
    virtual void Slot45();
    virtual void Slot46();
    virtual void Slot47();
    virtual void Slot48();
    virtual void Slot49();
    virtual void Slot50();
    virtual void Slot51();
    virtual void Slot52();
    virtual void Slot53();
    virtual void Slot54();
    virtual void Slot55();
    virtual void Slot56();
    virtual void Slot57();
    virtual void Slot58();
    virtual void Slot59();
    virtual void Slot60();
    virtual void Slot61();
    virtual void End();                                  // slot 62

    unsigned char m_unknown04[0x18];  // +0x04
    unsigned int m_flags;             // +0x1C Flags
    unsigned char m_unknown20[0x10];  // +0x20
    int m_viewport[4];                // +0x30 left, top, right, bottom: Draw's clip when it is given none
    unsigned char m_unknown40[0xF4];  // +0x40
    AtMaterial* m_material;           // +0x134 when set, every image draws with it
};

// What slot 27 of App's graphics gives: a coordinate system with an owner and two more matrices. Its class (near 0x00037CA0, which reports "Safe Zone" errors) is not recovered.
class AtCamera : public AtCoordSys {
public:
    // 0x00037EB0. Resets first if it has its matrices, keeps owner, then takes four matrices from the owner's matrix device, in order: +0x2C, +0x30, then the world and the local.
    void Attach(AtMaterialOwner* owner);

    AtMaterialOwner* m_owner;   // +0x28
    AtMatrix* m_matrix2C;       // +0x2C
    AtMatrix* m_matrix30;       // +0x30
};

// 0x00013F00. At most eight characters, a null name as empty: 0 if alike, else the first difference of signed characters.
int AtCompareName8(const char* a, const char* b);

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtRect) == 0x10, "AtRect");
static_assert(sizeof(AtColorF) == 0x10, "AtColorF");
static_assert(sizeof(AtImageStrip) == 0x18, "AtImageStrip");
static_assert(offsetof(AtGr2DRenderer, m_flags) == 0x1C, "AtGr2DRenderer::m_flags");
static_assert(offsetof(AtGr2DRenderer, m_viewport) == 0x30, "AtGr2DRenderer::m_viewport");
static_assert(offsetof(AtGr2DRenderer, m_material) == 0x134, "AtGr2DRenderer::m_material");
static_assert(offsetof(AtCamera, m_owner) == 0x28, "AtCamera::m_owner");
static_assert(offsetof(AtCamera, m_matrix30) == 0x30, "AtCamera::m_matrix30");
#pragma pop_macro("offsetof")
#endif
