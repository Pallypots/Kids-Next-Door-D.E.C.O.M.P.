// The graphics device's draw lists (AtGraphicsDeviceXbox +0x3E14, the device's slot 16), at/gfxdev. The same object chooses the formats textures and render targets are made in: at/texload's
// AtGrD3DFormatChooser, folded here (its slots 9 and 12 are the draw lists' vftable 0x00288EC0's, 0x000EDB50 and 0x000EDC90). Names inferred, by role.
#pragma once

#include "at/AtPixelFormat.h"

// ---- the draw lists, +0x3E14 ----------------------------------------------------------------------------------------------
// Vftable 0x00288E78, twelve slots: slot 0 (0x000ED5F0, deleting) not recovered, the rest pure (here they stop with a diagnostic). No constructor of its own survives: it is inlined into the derived one, whose vftable overwrites it.
class AtGfxDrawListsBase {
public:
    ~AtGfxDrawListsBase() {}           // 0x001197C0 this vftable put back
    virtual void Slot0();
    virtual void Slot1();
    virtual void Clear();              // slot 2
    virtual void Slot3();
    virtual void Slot4();
    virtual void Slot5();
    virtual void Slot6();
    virtual void Slot7();
    virtual void Slot8();
    // Slot 9: turns the format a render target asks for into one the device makes, in place (the derived class's is 0x000EDB50, not recovered).
    virtual void ChooseRenderTarget(const AtPixelFormat* asked, AtPixelFormat* chosen);
    virtual void Slot10();
    virtual void Slot11();
};

// Vftable 0x00288EC0, 27 slots, 0x2C bytes: three arrays, each a count, a second word and a block.
class AtGfxDrawLists : public AtGfxDrawListsBase {
public:
    AtGfxDrawLists();                  // 0x000ED6A0 all zero
    ~AtGfxDrawLists();                 // 0x000EE210 Clear, then the base's
    void Slot0() override;
    void Slot1() override;
    // Slot 12, the derived class's own: likewise for a texture (0x000EDC90, not recovered). at/texload's harnesses override 9 and 12.
    virtual void ChooseTexture(const AtPixelFormat* asked, AtPixelFormat* chosen);
    void Clear() override;             // slot 2, 0x000ED6D0 the blocks at +0x24, +0x18 and +0x0C freed in that order, then every word cleared

    int m_unknown04;                   // +0x04
    int m_unknown08;                   // +0x08
    void* m_block0C;                   // +0x0C
    int m_unknown10;                   // +0x10
    int m_unknown14;                   // +0x14
    void* m_block18;                   // +0x18
    int m_unknown1C;                   // +0x1C
    int m_unknown20;                   // +0x20
    void* m_block24;                   // +0x24
    int m_unknown28;                   // +0x28
};

// at/texload's name for the same class.
typedef AtGfxDrawLists AtGrD3DFormatChooser;
