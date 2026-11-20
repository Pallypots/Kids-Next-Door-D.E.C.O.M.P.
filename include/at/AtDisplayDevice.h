// The root of the graphics device: a display mode and a few words. Original address cluster 0x000321A0-0x00032268, with the display mode's 0x00043480-0x000434B9. Names are inferred.
//
// The one class built on it is AtMaterialOwner (include/at/AtMaterial.h): the constructor 0x00032200 has the one caller 0x000261AE, in AtMaterialOwner's constructor. The concrete graphics device is
// AtGraphicsDeviceXbox, include/at/AtGraphicsDevice.h.
#pragma once

#include "at/AtPixelFormat.h"

// A display mode: 0x24 bytes. 0x0008DBA0 fills one with 640, 480, a 32-bit RGBA format, 0 and a flag (through 0x00043420, not recovered) and hands it to the device's slot 4, which opens the display.
struct AtDisplayMode {
    AtDisplayMode();                   // 0x00043480 all zero
    void Clear();                      // 0x000434A0 all zero
    // 0x00043420 (at/grfactory, src/at/AtGraphicsFactory.cpp): the five fields in order; the adapter's modes give the refresh rate and flags as the last two.
    void Set(unsigned int width, unsigned int height, const AtPixelFormat* format, int unknown1C, int unknown20);

    unsigned int m_width;              // +0x00
    unsigned int m_height;             // +0x04
    AtPixelFormat m_format;            // +0x08
    int m_unknown1C;                   // +0x1C
    int m_unknown20;                   // +0x20
};

// Vftable 0x00274AD4, five slots. 0x34 bytes.
//
// The slots keep the names Slot0 to Slot4 because the harnesses' devices, built on AtMaterialOwner, override them by those names. Slot 0 is the original's deleting destructor, and here deletes, as the
// original's does with the flag 1; the flag is not taken. Slots 2 to 4 take one argument in the original and do nothing with it (0x000CBF30); here they take none.
class AtDisplayDevice {
public:
    AtDisplayDevice();                 // 0x00032200 the mode constructed, then what Reset does
    ~AtDisplayDevice();                // 0x000321A0 puts back this vftable, nothing else
    virtual void Slot0();              // slot 0, 0x000321B0 the deleting destructor
    virtual void Slot1();              // slot 1, 0x00032240 Reset: the words and the mode cleared, the four bytes at +0x2C back to 0x80
    virtual void Slot2() {}            // slot 2, 0x000CBF30
    virtual void Slot3() {}            // slot 3, 0x000CBF30
    virtual void Slot4() {}            // slot 4, 0x000CBF30

    int m_unknown04;                   // +0x04
    AtDisplayMode m_mode;              // +0x08 slots 23 and 24 of the graphics device hand on its width and height
    unsigned char m_unknown2C[4];      // +0x2C each 0x80 from Reset
    signed char m_noTextures;          // +0x30 negative: a material loaded for the device keeps no shader or textures. Reset clears the four bytes from here as one word.
    unsigned char m_unknown31[3];      // +0x31
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtDisplayMode) == 0x24, "AtDisplayMode");
static_assert(offsetof(AtDisplayMode, m_unknown20) == 0x20, "AtDisplayMode::m_unknown20");
static_assert(offsetof(AtDisplayDevice, m_mode) == 0x08, "AtDisplayDevice::m_mode");
static_assert(offsetof(AtDisplayDevice, m_unknown2C) == 0x2C, "AtDisplayDevice::m_unknown2C");
static_assert(offsetof(AtDisplayDevice, m_noTextures) == 0x30, "AtDisplayDevice::m_noTextures");
static_assert(sizeof(AtDisplayDevice) == 0x34, "AtDisplayDevice");
#pragma pop_macro("offsetof")
#endif
