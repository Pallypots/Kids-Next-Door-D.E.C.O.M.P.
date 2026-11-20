// The graphics device's two abstract layers: AtDisplayDevice (include/at/AtDisplayDevice.h) and AtMaterialOwner's constructor, destructor and slots (include/at/AtMaterial.h). In a file of their
// own, apart from the concrete device, so that a harness that makes an owner of its own does not take the concrete device with it.
// Verification status per function: config/functions.d/gfxdev.toml.
#include "at/AtMaterial.h"

// ---- the display mode ----------------------------------------------------------------------------------------------------
AtDisplayMode::AtDisplayMode() {
    m_width = 0;
    m_height = 0;
    m_unknown1C = 0;
    m_unknown20 = 0;
}

void AtDisplayMode::Clear() {
    m_format.Clear();
    m_width = 0;
    m_height = 0;
    m_unknown1C = 0;
    m_unknown20 = 0;
}

// ---- the display device ---------------------------------------------------------------------------------------------------
AtDisplayDevice::AtDisplayDevice() {
    AtDisplayDevice::Slot1();  // Inline in the original: the same stores as Reset, after the mode's constructor.
}

AtDisplayDevice::~AtDisplayDevice() {
}

void AtDisplayDevice::Slot0() {
    this->~AtDisplayDevice();
    operator delete(this);
}

void AtDisplayDevice::Slot1() {
    m_unknown04 = 0;
    m_mode.Clear();
    // One word in the original.
    m_noTextures = 0;
    m_unknown31[0] = 0;
    m_unknown31[1] = 0;
    m_unknown31[2] = 0;
    m_unknown2C[0] = 0x80;
    m_unknown2C[1] = 0x80;
    m_unknown2C[2] = 0x80;
    m_unknown2C[3] = 0x80;
}

// ---- the owner of the name tables -----------------------------------------------------------------------------------------
AtMaterialOwner::AtMaterialOwner() {
    m_textureLimits = 0;
    m_unknown39C = 0;
    m_unknown3A0 = 0;
    m_unknown3A4 = 0;
}

AtMaterialOwner::~AtMaterialOwner() {
    AtMaterialOwner::Slot1();
}

void AtMaterialOwner::Slot0() {
    this->~AtMaterialOwner();
    operator delete(this);
}

namespace {

// The original clears each name space database through its vftable.
void ClearThroughTable(GmNameMap* map) {
    map->Clear();
}

}  // namespace

// The original's order, which is not the members' order: the plain groups first, from the last, with the coordinate systems after the mirrors; then the pairs from the images down, with the
// unknown table at +0x12C before the lights and the light animations between the lights and the meshes.
void AtMaterialOwner::Slot1() {
    m_pixelProcesses.Reset();
    m_vertexProcesses.Reset();
    m_animations.Reset();
    m_weights.Reset();
    m_coordSystems.Reset();
    m_mirrors.Reset();
    ClearThroughTable(&m_imageSpaces);
    m_images.Reset();
    ClearThroughTable(&m_fontSpaces);
    m_fonts.Reset();
    ClearThroughTable(&m_morphAnimationSpaces);
    m_morphAnimations.Reset();
    ClearThroughTable(&m_matAnimationSpaces);
    m_matAnimations.Reset();
    ClearThroughTable(&m_matAssignmentSpaces);
    m_matAssignments.Reset();
    ClearThroughTable(&m_unknown12CSpaces);
    m_unknown12C.Reset();
    ClearThroughTable(&m_lightSpaces);
    m_lights.Reset();
    ClearThroughTable(&m_lightAnimationSpaces);
    m_lightAnimations.Reset();
    ClearThroughTable(&m_meshSpaces);
    m_meshes.Reset();
    ClearThroughTable(&m_shaderSpaces);
    m_shaders.Reset();
    ClearThroughTable(&m_materialSpaces);
    m_materials.Reset();
    ClearThroughTable(&m_textureSpaces);
    m_textures.Reset();
    m_textureLimits = 0;
    m_unknown39C = 0;
    m_unknown3A0 = 0;
    m_unknown3A4 = 0;
    AtDisplayDevice::Slot1();
}

// The pure slots: a call stops, as the original's pure call does.
namespace {

void Pure(const char* message) {
    AtFatal(message, 0, 0, 0);
}

}  // namespace

void AtMaterialOwner::Slot5() { Pure("AtMaterialOwner: slot 5 is pure"); }
void AtMaterialOwner::Slot6() { Pure("AtMaterialOwner: slot 6 is pure"); }
void AtMaterialOwner::Slot7() { Pure("AtMaterialOwner: slot 7 is pure"); }
void AtMaterialOwner::Slot8() { Pure("AtMaterialOwner: slot 8 is pure"); }
void AtMaterialOwner::Slot9() { Pure("AtMaterialOwner: slot 9 is pure"); }
void AtMaterialOwner::Slot10() { Pure("AtMaterialOwner: slot 10 is pure"); }
void* AtMaterialOwner::RenderTarget0() { Pure("AtMaterialOwner: slot 13 is pure"); return 0; }
void* AtMaterialOwner::RenderTarget1() { Pure("AtMaterialOwner: slot 14 is pure"); return 0; }
AtGfxContext* AtMaterialOwner::Context() { Pure("AtMaterialOwner: slot 15 is pure"); return 0; }
AtGfxDrawLists* AtMaterialOwner::DrawLists() { Pure("AtMaterialOwner: slot 16 is pure"); return 0; }
AtMatrixDevice* AtMaterialOwner::MatrixDevice() { Pure("AtMaterialOwner: slot 17, the matrix device, is pure: only the concrete graphics device has one"); return 0; }
AtFlashGraphicsFactory* AtMaterialOwner::GraphicsFactory() { Pure("AtMaterialOwner: slot 18 is pure"); return 0; }
void* AtMaterialOwner::Slot19(void*) { Pure("AtMaterialOwner: slot 19 is pure"); return 0; }
void AtMaterialOwner::ReleaseObject(AtGfxObject*) { Pure("AtMaterialOwner: slot 20 is pure"); }
AtGfxBufferSet* AtMaterialOwner::BufferSet() { Pure("AtMaterialOwner: slot 21 is pure"); return 0; }
void AtMaterialOwner::Slot22() { Pure("AtMaterialOwner: slot 22 is pure"); }
unsigned int AtMaterialOwner::Width() { Pure("AtMaterialOwner: slot 23 is pure"); return 0; }
unsigned int AtMaterialOwner::Height() { Pure("AtMaterialOwner: slot 24 is pure"); return 0; }
void AtMaterialOwner::DrawScale(float*) { Pure("AtMaterialOwner: slot 25 is pure"); }
