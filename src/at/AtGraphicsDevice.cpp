// The concrete graphics device and its members. See include/at/AtGraphicsDevice.h.
// Verification status per function: config/functions.d/gfxdev.toml.
#include "at/AtGraphicsDevice.h"

namespace {

void NotRecovered(const char* message) {
    AtFatal(message, 0, 0, 0);
}

}  // namespace

// ---- the context ----------------------------------------------------------------------------------------------------------
int AtGfxContextBase::s_shared[3];

AtGfxContextBase::Part30::Part30() {
    m_unknown10 = 0;
    m_unknown14 = 1.0f;
}

AtGfxContextBase::Part48::Part48() {
    for (int i = 0; i < 0x18; i++) {
        m_unknown00[i] = 0.0f;
    }
    for (int i = 0; i < 0x1B; i++) {
        m_unknown60[i] = 0.0f;
    }
    m_unknownD4 = 0;
    m_unknownD8 = 0;
    m_unknownE4 = 0;
    m_unknownE8 = 0;
    m_unknownCC = 0;
    m_unknownDC = -1.0f;
    m_unknownE0 = 1.0f;
    m_unknownD0 = 5;
}

AtGfxContextBase::AtGfxContextBase() {
    ResetBase();
}

AtGfxContextBase::~AtGfxContextBase() {
    ResetBase();
}

void AtGfxContextBase::Slot0() { NotRecovered("AtGfxContextBase: slot 0, 0x00059550, is not recovered"); }
void AtGfxContextBase::Slot1() { NotRecovered("AtGfxContextBase: slot 1 is pure"); }

void AtGfxContextBase::Reset() {
    ResetBase();
}

void AtGfxContextBase::ResetBase() {
    m_unknown10 = 0;
    m_unknown14 = 0;
    m_unknown18 = 0;
    m_unknown1C = 0;
    m_unknown24 = 1.0f;
    m_unknown28 = 0;
    m_unknown2C = 0;
    m_color20[0] = 0;
    m_color20[1] = 0;
    m_color20[2] = 0;
    m_color20[3] = 0xFF;
    m_unknown2940 = 0;
    m_unknown2944 = 0;
    m_unknown134 = 0;
    m_shared = s_shared;
    m_unknown2948 = 0;
    s_shared[0] = 0;
    s_shared[1] = 0;
    s_shared[2] = 0;
}

AtGfxContext::AtGfxContext() {
    ResetLocal();
}

AtGfxContext::~AtGfxContext() {
    if (m_unknown10 != 0) {
        m_device->UnlinkContext(this);
    }
    AtGfxContextBase::Reset();
    ResetLocal();
}

void AtGfxContext::Slot0() { NotRecovered("AtGfxContext: slot 0, 0x0010A240, is not recovered"); }
void AtGfxContext::Slot1() { NotRecovered("AtGfxContext: slot 1, 0x00109A20, is not recovered"); }
void AtGfxContext::Reset() { NotRecovered("AtGfxContext: slot 2, 0x00108000, is not recovered"); }

void AtGfxContext::ResetLocal() {
    m_device = 0;
    m_unknown2954 = 0;
    m_unknown2958 = 0;
    m_next = 0;
    m_unknown295C = 0;
    m_unknown2960 = 0;
    m_unknown2A50 = 0;
    m_unknown2A54 = 0;
    m_unknown2964 = 0;
    m_unknown2980 = 0;
    for (int i = 0; i < 0x15; i++) {
        m_unknown2984[i] = 0;
    }
    for (int i = 0; i < 0x15; i++) {
        m_unknown29D8[i] = 0;
    }
    m_unknown2A2C = 0;
    for (int i = 0; i < 7; i++) {
        m_unknown2A30[i] = 0;
    }
    m_unknown2A58 = 0;
    m_unknown2A5C = 0;
    m_unknown2A64 = 0;
    m_unknown2A68 = 0;
    m_unknown2A6C = 0;
    m_unknown2A78 = 0;
    m_unknown2A60 = 2;
    m_unknown2A70 = 1.0f;
    m_unknown2A74 = 1.0f;
}

// ---- the state cache ------------------------------------------------------------------------------------------------------
AtGfxStateCache::AtGfxStateCache() {
    Reset();
}

AtGfxStateCache::~AtGfxStateCache() {
    if (m_blockA58 != 0) {
        operator delete(m_blockA58);
    }
    Reset();
}

void AtGfxStateCache::Reset() {
    m_unknownA98 = 0;
    m_unknown000[0] = 0;
    m_unknown000[1] = 0;
    m_unknown000[2] = 0;
    m_unknown9FC = 0;
    m_unknown9F8 = 0;
    m_unknownA00 = 0;
    m_unknownA10 = 0;
    m_unknownA54 = 0;
    m_unknownA5C = 0;
    for (int i = 0; i < 0x11; i++) {
        m_unknown9B4[i] = 0;
    }
    m_unknownA9C = 0;
    m_unknownA60 = 0;
    m_unknownAA0 = 0;
    m_unknownA94 = 0;
    m_blockA58 = 0;
    ResetState();
    m_unknown9B0 = 0;
}

void AtGfxStateCache::ResetState() {
    for (int i = 0; i < 0x160; i++) {
        m_unknown00C[i] = 0;
    }
    m_cachedA04 = 0;
    m_cachedA08 = 0;
    m_cachedA0C = 0;
    for (int i = 0; i < 8; i++) {
        m_unknownA14[i] = 0;
    }
    for (int i = 0; i < 0x100; i++) {
        m_unknown5AC[i] = 0;
    }
    for (int i = 0; i < 8; i++) {
        m_unknown58C[i] = 0;
    }
    m_unknownA34[0] = 0;
    m_unknownA34[1] = 0;
    m_unknownA3C[0] = 0;
    m_unknownA3C[1] = 0;
    m_unknownA4C[0] = 0;
    m_unknownA4C[1] = 0;
    m_unknownA44[0] = 0;
    m_unknownA44[1] = 0;
    for (int i = 0; i < 0xC; i++) {
        m_unknownA64[i] = 0;
    }
    m_unknown9AC = 0;
}

// ---- the draw lists -------------------------------------------------------------------------------------------------------
void AtGfxDrawListsBase::Slot0() { NotRecovered("AtGfxDrawListsBase: slot 0, 0x000ED5F0, is not recovered"); }
void AtGfxDrawListsBase::Slot1() { NotRecovered("AtGfxDrawListsBase: slot 1 is pure"); }
void AtGfxDrawListsBase::Clear() { NotRecovered("AtGfxDrawListsBase: slot 2 is pure"); }
#define PURE_SLOT(n) void AtGfxDrawListsBase::Slot##n() { NotRecovered("AtGfxDrawListsBase: slot " #n " is pure"); }
PURE_SLOT(3) PURE_SLOT(4) PURE_SLOT(5) PURE_SLOT(6) PURE_SLOT(7) PURE_SLOT(8) PURE_SLOT(10) PURE_SLOT(11)
#undef PURE_SLOT
void AtGfxDrawListsBase::ChooseRenderTarget(const AtPixelFormat*, AtPixelFormat*) { NotRecovered("AtGfxDrawLists: slot 9, 0x000EDB50, is not recovered"); }
void AtGfxDrawLists::ChooseTexture(const AtPixelFormat*, AtPixelFormat*) { NotRecovered("AtGfxDrawLists: slot 12, 0x000EDC90, is not recovered"); }

AtGfxDrawLists::AtGfxDrawLists() {
    m_unknown04 = 0;
    m_unknown08 = 0;
    m_block0C = 0;
    m_unknown10 = 0;
    m_unknown14 = 0;
    m_block18 = 0;
    m_unknown1C = 0;
    m_unknown20 = 0;
    m_block24 = 0;
    m_unknown28 = 0;
}

AtGfxDrawLists::~AtGfxDrawLists() {
    AtGfxDrawLists::Clear();
}

void AtGfxDrawLists::Slot0() { NotRecovered("AtGfxDrawLists: slot 0, 0x000EE530, is not recovered"); }
void AtGfxDrawLists::Slot1() { NotRecovered("AtGfxDrawLists: slot 1, 0x000EE260, is not recovered"); }

void AtGfxDrawLists::Clear() {
    if (m_block24 != 0) {
        operator delete(m_block24);
    }
    if (m_block18 != 0) {
        operator delete(m_block18);
    }
    if (m_block0C != 0) {
        operator delete(m_block0C);
    }
    m_unknown04 = 0;
    m_unknown08 = 0;
    m_block0C = 0;
    m_unknown10 = 0;
    m_unknown14 = 0;
    m_block18 = 0;
    m_unknown1C = 0;
    m_unknown20 = 0;
    m_block24 = 0;
    m_unknown28 = 0;
}

// ---- the concrete index array and the buffer set --------------------------------------------------------------------------
AtGrD3DIndexArray::AtGrD3DIndexArray() {
    m_resource = 0;
    m_memory = 0;
    m_owner = 0;
}

// Its slots and destructor are loader's (src/at/AtGrD3DUnrecovered.cpp; the reader src/at/AtGrD3DSeam.cpp).

AtGfxBufferSet::AtGfxBufferSet() {
    m_unknown04 = 0;
    m_unknown08 = 0;
    m_unknown0C = 0;
    m_unknown18 = 0;
    m_unknown1C = 0;
    m_unknown20 = 0;
    m_unknown10 = 0;
    m_unknown14 = 0;
    m_unknown2C = 0;
    m_unknown78 = 0;
    m_unknown7C = 0;
    m_unknown80 = 0;
    m_unknown84 = 0;
    m_unknown88 = 0;
}

void AtGfxBufferSet::Slot0() { NotRecovered("AtGfxBufferSet: slot 0, 0x000FA1F0, is not recovered"); }

// ---- the concrete mesh database: loader's, src/at/AtGrD3DMeshDatabase.cpp ----------------------------------------------

// ---- the lists of blocks --------------------------------------------------------------------------------------------------
AtGfxBlockList::AtGfxBlockList() {
    m_block00 = 0;
    m_unknown04 = 0;
    m_block08 = 0;
    m_unknown0C = 0;
    m_block10 = 0;
}

void AtGfxBlockList::Clear() {
    if (m_block10 != 0) {
        operator delete(m_block10);
    }
    if (m_block08 != 0) {
        operator delete(m_block08);
    }
    if (m_block00 != 0) {
        operator delete(m_block00);
    }
    m_block00 = 0;
    m_unknown04 = 0;
    m_block08 = 0;
    m_unknown0C = 0;
    m_block10 = 0;
}

AtGfxIndexedList::AtGfxIndexedList() {
    m_block14 = 0;
    m_unknown18 = 0;
    m_block1C = 0;
    m_unknown20 = 0;
}

AtGfxIndexedList::~AtGfxIndexedList() {
    Clear();
}

void AtGfxIndexedList::Clear() {
    AtGfxBlockList::Clear();
    if (m_block1C != 0) {
        operator delete(m_block1C);
    }
    if (m_block14 != 0) {
        operator delete(m_block14);
    }
    m_block14 = 0;
    m_unknown18 = 0;
    m_block1C = 0;
    m_unknown20 = 0;
}

AtGfxBlockPair::AtGfxBlockPair() {
    m_block00 = 0;
    m_unknown04 = 0;
    m_block08 = 0;
    m_unknown0C = 0;
}

void AtGfxBlockPair::Clear() {
    if (m_block08 != 0) {
        operator delete(m_block08);
    }
    if (m_block00 != 0) {
        operator delete(m_block00);
    }
    m_block00 = 0;
    m_unknown04 = 0;
    m_block08 = 0;
    m_unknown0C = 0;
}

// ---- the resources --------------------------------------------------------------------------------------------------------
AtGfxResource::AtGfxResource() {
    m_unknown04[0] = 0;
    m_unknown04[1] = 0;
    m_unknown04[2] = 0;
    m_unknown04[3] = 0;
    m_unknown14 = 0;
    m_unknown18 = 0;
    m_unknown24 = 0;
    m_unknown26 = 0;
    m_unknown1C = 0;
    m_unknown20 = 0;
    m_unknown28 = 0;
    m_unknown2A = 0;
    m_unknown2C = 0;
    m_unknown2E = 1;
    m_unknown30 = 0;  // The original stores it twice.
}

void AtGfxResource::Slot0() { NotRecovered("AtGfxResource: slot 0 is not recovered"); }

AtGfxResourceBlock::AtGfxResourceBlock() {
    m_unknown34 = 0;
    m_unknown38 = 0;
    m_unknown3C = 0;
}

AtGfxResourceXbox::AtGfxResourceXbox() {
    m_unknown40 = 0;
    m_unknown44 = 0;
    m_resource = 0;
}

int AtGfxLessAt10(const void* a, const void* b) {
    return ((const float*)a)[4] < ((const float*)b)[4];
}

// ---- the device -----------------------------------------------------------------------------------------------------------
AtGraphicsDeviceXbox::AtGraphicsDeviceXbox() {
    m_creator = 0;
    Init();
}

AtGraphicsDeviceXbox::~AtGraphicsDeviceXbox() {
    AtGraphicsDeviceXbox::Slot1();
}

void AtGraphicsDeviceXbox::Slot0() {
    this->~AtGraphicsDeviceXbox();
    operator delete(this);
}

void AtGraphicsDeviceXbox::Init() {
    m_unknown3AC = 0;
    m_unknown332C = 0;
    m_unknown3330 = 0;
    m_contexts = 0;
    m_unknown3338 = 0;
    m_unknown333C = 0;
    m_meshDatabases = 0;
    m_unknown3344 = 0;
    m_unknown3348 = 0;
    m_unknown334C = 0;
    m_unknown3350 = 0;
    for (int i = 0; i < 10; i++) {
        m_unknown3F7C[i] = 0;
    }
    m_unknown3FA4 = m_table3FA8;
    for (int i = 0; i < 0x100; i++) {
        m_table3FA8[i] = 0;
    }
    for (int i = 0; i < 0x100; i++) {
        m_table43A8[i] = 0;
    }
    for (int i = 0; i < 0x100; i++) {
        m_table47B4[i] = 0;
    }
    for (int i = 0; i < 0x100; i++) {
        m_table4BC0[i] = 0;
    }
    for (int i = 0; i < 0x100; i++) {
        m_table4FCC[i] = 0;
    }
    for (int i = 0; i < 0x100; i++) {
        m_table53D8[i] = 0;
    }
    for (int i = 0; i < 0x100; i++) {
        m_table57E4[i] = 0;
    }
    for (int i = 0; i < 0x100; i++) {
        m_table5BE4[i] = 0;
    }
    m_unknown5FE4[0] = 0;
    m_unknown5FE4[1] = 0;
    m_unknown5FE4[2] = 0;
    m_unknown5FE4[3] = 0;
    m_unknown5FF4 = 0;
    m_unknown6100 = 0;
    for (int i = 0; i < 0x10; i++) {
        m_unknown6104[i] = 0;
    }
    m_unknown6144 = 0;
    m_unknown6148 = 0;
    for (int i = 0; i < 0x3400; i++) {
        m_unknown614C[i] = 0;
    }
    m_sortedA.m_less = AtGfxLessAt10;
    m_sortedB.m_less = AtGfxLessAt10;
    for (int i = 0; i < 5; i++) {
        m_unknown30240[i] = 0;
    }
    m_unknown3E10 = 0x901;
    for (int i = 0; i < 0x11; i++) {
        m_unknown30254[i] = 0;
    }
    m_unknown3DF8 = 0;
    m_unknown3E00 = 1;
    m_unknown3E04 = 0;
    m_unknown3E08 = 0;
    m_unknown3E0C = 0;
    m_unknown30598 = 2;
    for (int i = 0; i < 4; i++) {
        m_unknown305A0[i] = 0;
    }
}

void AtGraphicsDeviceXbox::UnlinkContext(AtGfxContext* context) {
    AtGfxContext** link = &m_contexts;
    for (AtGfxContext* c = *link; c != 0; c = *link) {
        if (c == context) {
            *link = context->m_next;
            return;
        }
        link = &c->m_next;
    }
}

void AtGraphicsDeviceXbox::Slot1() { NotRecovered("Atlas Graphics: the device's slot 1, Release, 0x000F2750, is not recovered"); }
void AtGraphicsDeviceXbox::Slot2() { NotRecovered("Atlas Graphics: the device's slot 2, 0x000F18B0, is not recovered"); }
void AtGraphicsDeviceXbox::Slot3() { NotRecovered("Atlas Graphics: the device's slot 3, 0x000EF350, is not recovered"); }
void AtGraphicsDeviceXbox::Slot4() { NotRecovered("Atlas Graphics: the device's slot 4, 0x000F18C0, is not recovered"); }
void AtGraphicsDeviceXbox::Slot5() { NotRecovered("Atlas Graphics: the device's slot 5, 0x000F44E0, is not recovered"); }
void AtGraphicsDeviceXbox::Slot6() { NotRecovered("Atlas Graphics: the device's slot 6, 0x000F2270, is not recovered"); }
void AtGraphicsDeviceXbox::Slot7() { NotRecovered("Atlas Graphics: the device's slot 7, 0x000EFEC0, is not recovered"); }
void AtGraphicsDeviceXbox::Slot8() { NotRecovered("Atlas Graphics: the device's slot 8, 0x000F0010, is not recovered"); }
void AtGraphicsDeviceXbox::Slot9() { NotRecovered("Atlas Graphics: the device's slot 9, 0x000F0310, is not recovered"); }
void AtGraphicsDeviceXbox::Slot10() { NotRecovered("Atlas Graphics: the device's slot 10, 0x000F0350, is not recovered"); }
void AtGraphicsDeviceXbox::Slot11() { NotRecovered("Atlas Graphics: the device's slot 11, 0x000F14C0, is not recovered"); }
void AtGraphicsDeviceXbox::Slot12() { NotRecovered("Atlas Graphics: the device's slot 12, 0x000F1560, is not recovered"); }
void* AtGraphicsDeviceXbox::Slot19(void*) { NotRecovered("Atlas Graphics: the device's slot 19, 0x000EF300, is not recovered"); return 0; }
void AtGraphicsDeviceXbox::Slot22() { NotRecovered("Atlas Graphics: the device's slot 22, 0x000EF340, is not recovered"); }

void* AtGraphicsDeviceXbox::RenderTarget0() {
    return &m_target0;
}

void* AtGraphicsDeviceXbox::RenderTarget1() {
    return &m_target1;
}

AtGfxContext* AtGraphicsDeviceXbox::Context() {
    return &m_context;
}

AtGfxDrawLists* AtGraphicsDeviceXbox::DrawLists() {
    return &m_drawLists;
}

AtMatrixDevice* AtGraphicsDeviceXbox::MatrixDevice() {
    return &m_matrixDevice;
}

AtFlashGraphicsFactory* AtGraphicsDeviceXbox::GraphicsFactory() {
    return static_cast<AtFlashGraphicsFactory*>(m_creator);  // the graphics factory, as the Flash code knows it (include/at/AtMaterial.h)
}

void AtGraphicsDeviceXbox::ReleaseObject(AtGfxObject* object) {
    object->Release();
}

AtGfxBufferSet* AtGraphicsDeviceXbox::BufferSet() {
    return &m_bufferSet;
}

unsigned int AtGraphicsDeviceXbox::Width() {
    return m_mode.m_width;
}

unsigned int AtGraphicsDeviceXbox::Height() {
    return m_mode.m_height;
}

void AtGraphicsDeviceXbox::DrawScale(float* out) {
    out[0] = 1.0f;
    out[1] = 1.0f;
}
