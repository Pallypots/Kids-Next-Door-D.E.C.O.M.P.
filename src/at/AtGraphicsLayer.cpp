// The graphics layer's object short of the hardware: its constructors, Init, Shutdown and SetHeaps, the saved globals it keeps, the function tables they
// save, and the heap setters only it calls. Subsystem at/grlayer; see include/at/AtGraphicsLayer.h. Original clusters 0x000B2F50-0x000B3117,
// 0x000C7CA0-0x000C8142, 0x000C81B0-0x000C8572, 0x000C8600-0x000C8655, and single functions. Names are inferred.
#include "at/AtGraphicsLayer.h"

#include "at/AtDatabaseLoader.h"
#include "at/AtImageDatabase.h"
#include "at/AtMatAnimation.h"
#include "at/AtMatAssignment.h"
#include "at/AtMaterialDatabase.h"
#include "at/AtShader.h"
#include "at/AtShaderDatabase.h"
#include "at/AtTextureDatabase.h"
#include "gm/GmNameMap.h"

AtRenderTable g_atRender;

void* g_heap3B23DC;
void* g_heap2C7C38;
void* g_heap3B23D8;
void* g_heap2C7FF0;
void* g_heap2C635C;
void* g_heap2C63E8;
void* g_heap3B23D4;
void* g_heap3B1FC4;
void* g_heap2C6358;
void* g_heap2CB08C;
void* g_heap2CB090;
void* g_heap2C9C08;
void* g_heap3B13D4;
void* g_heap2C6378;
void* g_heap2C637C;
void* g_heap3B1F50;
void* g_heap3B23D0;
void* g_heap3B1FA0;
void* g_heap3B1F9C;
void* g_heap2C6384;
void* g_heap2C6388;
void* g_heap3B1FC0;
void* g_heap3B1FBC;
void* g_heap3B1FB8;
void* g_heap3B1FB4;
void* g_heap3B1FB0;
void* g_heap3B1FAC;
void* g_heap3B1FA8;
void* g_heap3B1FA4;
void* g_heap3B13CC;
void* g_heap3B1F98;
void* g_heap3B1F90;
void* g_heap3B1F70;
void* g_heap3B1F54;
void* g_heap3B13E0;
void* g_heap3B13DC;
void* g_heap3B13D8;
void* g_heap3B13D0;

// ---- the function tables ----------------------------------------------------------------------------------------------------

AtMatrixTable* AtMatrixTableGet() { return &g_atMatrix; }

void AtMatrixTableSet(const AtMatrixTable* table) {
    g_atMatrix = *table;  // ORIGINAL: 0x1A0 bytes; here the members known (include/at/AtCoordSys.h)
}

AtRenderTable* AtRenderTableGet() { return &g_atRender; }

void AtRenderTableSet(const AtRenderTable* table) {
    AtMatrixTableSet(&table->m_matrix);
    g_atRender = *table;
}

// ---- the saved globals ------------------------------------------------------------------------------------------------------

AtMathGlobals::AtMathGlobals() {
    m_matrix = 0;
}

void AtMathGlobals::Save() {
    AtIoGlobals::Save();
    m_matrix = AtMatrixTableGet();
}

void AtMathGlobals::Restore() {
    AtIoGlobals::Restore();
    AtMatrixTableSet(m_matrix);  // ORIGINAL: the live table onto itself
}

AtDataGlobals::AtDataGlobals() {
}

void AtDataGlobals::Save() {
    AtMathGlobals::Save();
}

void AtDataGlobals::Restore() {
    AtMathGlobals::Restore();
}

AtKinematicsGlobals::AtKinematicsGlobals() {
}

void AtKinematicsGlobals::Save() {
    AtDataGlobals::Save();
}

void AtKinematicsGlobals::Restore() {
    AtDataGlobals::Restore();
}

AtGraphicsGlobals::AtGraphicsGlobals() {
    m_render = 0;
}

void AtGraphicsGlobals::Save() {
    AtKinematicsGlobals::Save();
    m_render = AtRenderTableGet();
}

void AtGraphicsGlobals::Restore() {
    AtKinematicsGlobals::Restore();
    AtRenderTableSet(m_render);  // ORIGINAL: its matrix part over the matrix table, then the live table onto itself
}

// ---- the heap setters ---------------------------------------------------------------------------------------------------------

void AtSetHeap3B23DC(void* heap) { g_heap3B23DC = heap; }
void AtSetHeap2C7C38(void* heap) { g_heap2C7C38 = heap; }
void AtSetHeap3B23D8(void* heap) { g_heap3B23D8 = heap; }
void AtSetHeap2C7FF0(void* heap) { g_heap2C7FF0 = heap; }
void AtSetHeap2C635C(void* heap) { g_heap2C635C = heap; }
void AtSetHeap2C63E8(void* heap) { g_heap2C63E8 = heap; }
void AtSetHeap3B23D4(void* heap) { g_heap3B23D4 = heap; }
void AtSetHeap3B1FC4(void* heap) { g_heap3B1FC4 = heap; }
void AtSetHeap2C6358(void* heap) { g_heap2C6358 = heap; }
void AtSetHeap2CB08C(void* heap) { g_heap2CB08C = heap; }
void AtSetHeap2CB090(void* heap) { g_heap2CB090 = heap; }
void AtSetHeap2C9C08(void* heap) { g_heap2C9C08 = heap; }
void AtSetHeap3B13D4(void* heap) { g_heap3B13D4 = heap; }
void AtSetHeap2C6378(void* heap) { g_heap2C6378 = heap; }
void AtSetHeap2C637C(void* heap) { g_heap2C637C = heap; }
void AtSetHeap3B1F50(void* heap) { g_heap3B1F50 = heap; }
void AtSetHeap3B23D0(void* heap) { g_heap3B23D0 = heap; }
void AtSetHeap3B1FA0(void* heap) { g_heap3B1FA0 = heap; }
void AtSetHeap3B1F9C(void* heap) { g_heap3B1F9C = heap; }
void AtSetHeap2C6384(void* heap) { g_heap2C6384 = heap; }
void AtSetHeap2C6388(void* heap) { g_heap2C6388 = heap; }
void AtSetHeap3B1FC0(void* heap) { g_heap3B1FC0 = heap; }
void AtSetHeap3B1FBC(void* heap) { g_heap3B1FBC = heap; }
void AtSetHeap3B1FB8(void* heap) { g_heap3B1FB8 = heap; }
void AtSetHeap3B1FB4(void* heap) { g_heap3B1FB4 = heap; }
void AtSetHeap3B1FB0(void* heap) { g_heap3B1FB0 = heap; }
void AtSetHeap3B1FAC(void* heap) { g_heap3B1FAC = heap; }
void AtSetHeap3B1FA8(void* heap) { g_heap3B1FA8 = heap; }
void AtSetHeap3B1FA4(void* heap) { g_heap3B1FA4 = heap; }
void AtSetHeap3B13CC(void* heap) { g_heap3B13CC = heap; }
void AtSetHeap3B1F98(void* heap) { g_heap3B1F98 = heap; }
void AtSetHeap3B1F90(void* heap) { g_heap3B1F90 = heap; }
void AtSetHeap3B1F70(void* heap) { g_heap3B1F70 = heap; }
void AtSetHeap3B1F54(void* heap) { g_heap3B1F54 = heap; }
void AtSetHeap3B13E0(void* heap) { g_heap3B13E0 = heap; }
void AtSetHeap3B13DC(void* heap) { g_heap3B13DC = heap; }
void AtSetHeap3B13D8(void* heap) { g_heap3B13D8 = heap; }
void AtSetHeap3B13D0(void* heap) { g_heap3B13D0 = heap; }

// ---- the layer's base ------------------------------------------------------------------------------------------------------------

namespace {
// Each heap the config does not give falls back to its default heap.
inline void* Or(void* heap, const AtGraphicsConfig* config) {
    return heap != 0 ? heap : config->m_heap;
}
}

AtGraphicsSystemBase::AtGraphicsSystemBase() {
    m_unknown04 = 0;
    m_flags = 0;
    m_below = 0;
    m_factory = 0;
    m_unknown18 = 0;
    m_materialOwner = 0;
    m_unknown24 = 0;
    m_unknown20 = 0;
    m_unknown28 = 0;
    m_unknown2C = 0;
    m_kind = 0;
}

void AtGraphicsSystemBase::Init(AtGraphicsConfig* config, void* below, void*) {
    if ((m_flags & kRunning) != 0) {
        Shutdown();
    }
    m_below = below;
    m_unknown28 = config->m_unknown04;
    m_kind = config->m_unknown00;
}

void AtGraphicsSystemBase::Shutdown() {
    m_unknown04 = 0;
    m_flags = 0;
    m_below = 0;
    m_factory = 0;
    m_unknown18 = 0;
    m_materialOwner = 0;
    m_unknown24 = 0;
    m_unknown20 = 0;
    m_unknown28 = 0;
    m_unknown2C = 0;
    m_kind = 0;
}

void AtGraphicsSystemBase::SetHeaps(const AtGraphicsConfig* config) {
    AtSetHeap3B23DC(Or(config->m_heap30, config));
    AtSetHeap2C7C38(Or(config->m_displayModeHeap, config));
    AtDatabaseLoader::SetHeap(Or(config->m_loaderHeap, config));
    AtSetHeap3B1FCC(Or(config->m_heap34, config));
    AtSetHeap3B23D8(Or(config->m_heap34, config));
    AtSetHeap2C7FF0(Or(config->m_heap3C, config));
    AtSetHeap2C635C(Or(config->m_heap48, config));
    AtSetHeap2C63E8(Or(config->m_heap70, config));
    AtSetHeap3B23D4(Or(config->m_heap70, config));
    AtMaterialDatabase::SetHeap(Or(config->m_materialHeap, config));
    AtShader::SetHeap(Or(config->m_shaderHeap, config));
    AtMatAnimationDatabase::SetHeap(Or(config->m_matAnimationHeap, config));
    AtMatAssignment::SetHeap(Or(config->m_matAssignmentHeap, config));
    AtMatAssignmentDatabase::SetHeap(Or(config->m_matAssignmentDbHeap, config));
    AtSetHeap3B1FC4(Or(config->m_heap64, config));
    AtSetHeap2C6358(Or(config->m_heap78, config));
    AtSetHeap2CB08C(Or(config->m_heap80, config));
    AtSetHeap2CB090(Or(config->m_heap84, config));
    AtTextureDatabase::SetHeap(Or(config->m_textureHeap, config));
    AtSetHeap2C9C08(Or(config->m_heapB8, config));
    AtSetHeap3B13D4(Or(config->m_heapBC, config));
    AtSetHeap2C6378(Or(config->m_heap4C, config));
    AtSetHeap2C637C(Or(config->m_heap74, config));
    AtImageDatabase::SetHeap(Or(config->m_imageHeap, config));
    AtSetHeap3B1F50(Or(config->m_heap94, config));
    AtSetHeap3B23D0(Or(config->m_heap9C, config));
    AtShaderDatabase::SetHeap(Or(config->m_shaderDbHeap, config));
    AtFontDatabase::SetHeap(Or(config->m_fontHeap, config));
    AtSetHeap3B1FA0(Or(config->m_heap88, config));
    AtSetHeap3B1F9C(Or(config->m_heapC4, config));
    AtSetHeap2C6384(Or(config->m_heap8C, config));
    AtProgShaderComponent::SetHeap(Or(config->m_progShaderHeap, config));
    AtSetHeap2C6388(Or(config->m_heapC8, config));
    if (GmNameMap::s_heap == 0) {
        GmNameMap::SetHeap(Or(config->m_nameMapHeap, config));
    }
    if (GmNameSpaceDb::s_heap == 0) {
        GmNameSpaceDb::SetHeap(Or(config->m_nameSpaceHeap, config));
    }
}

// ---- the layer -----------------------------------------------------------------------------------------------------------------

AtDisplayMode::AtDisplayMode() {
    m_unknown00 = 0;
    m_unknown04 = 0;
    m_unknown1C = 0;
    m_unknown20 = 0;
}

AtGraphicsSystem::AtGraphicsSystem() {
    m_unknown34 = 0;
    m_unknown38 = 0;
    m_unknown3C = 0;
    m_unknown24 = 0;  // ORIGINAL: the base's constructor cleared it already
    m_unknown40 = 0;
    m_unknown44 = 0;
    m_unknown48 = 0;
    m_unknown8C = 0;
    m_unknown90 = 0;
    m_unknown94 = 0;
    m_unknown50 = 0;
}

void AtGraphicsSetHeaps(const AtGraphicsConfig* config) {
    AtSetHeap2C7C38(Or(config->m_displayModeHeap, config));
    AtDatabaseLoader::SetHeap(Or(config->m_loaderHeap, config));
    AtSetHeap3B1FCC(Or(config->m_heap34, config));
    AtSetHeap2C7FF0(Or(config->m_heap3C, config));
    AtMaterialDatabase::SetHeap(Or(config->m_materialHeap, config));
    AtSetHeap2C63E8(Or(config->m_heap70, config));
    AtShader::SetHeap(Or(config->m_shaderHeap, config));
    AtMatAnimationDatabase::SetHeap(Or(config->m_matAnimationHeap, config));
    AtMatAssignment::SetHeap(Or(config->m_matAssignmentHeap, config));
    AtMatAssignmentDatabase::SetHeap(Or(config->m_matAssignmentDbHeap, config));
    AtSetHeap3B1FC4(Or(config->m_heap64, config));
    AtSetHeap2C6358(Or(config->m_heap78, config));
    AtSetHeap2CB08C(Or(config->m_heap80, config));
    AtSetHeap2CB090(Or(config->m_heap84, config));
    if (GmNameMap::s_heap == 0) {
        GmNameMap::SetHeap(Or(config->m_nameMapHeap, config));
    }
    if (GmNameSpaceDb::s_heap == 0) {
        GmNameSpaceDb::SetHeap(Or(config->m_nameSpaceHeap, config));
    }
    AtSetHeap3B1FC0(Or(config->m_heap0C, config));
    AtSetHeap3B1FBC(Or(config->m_heap10, config));
    AtSetHeap3B1FB8(Or(config->m_heapB0, config));
    AtSetHeap3B1FB4(Or(config->m_heap1C, config));
    AtSetHeap3B1FB0(Or(config->m_heap20, config));
    AtSetHeap3B1FAC(Or(config->m_heap24, config));
    AtSetHeap3B1FA8(Or(config->m_heap28, config));
    AtSetHeap3B1FA4(Or(config->m_heap2C, config));
    AtSetHeap3B13CC(Or(config->m_heap30, config));
    AtFontDatabase::SetHeap(Or(config->m_fontHeap, config));
    AtImageDatabase::SetHeap(Or(config->m_imageHeap, config));
    AtSetHeap3B1FA0(Or(config->m_heap88, config));
    AtSetHeap3B1F9C(Or(config->m_heapC4, config));
    AtSetHeap2C6384(Or(config->m_heap8C, config));
    AtProgShaderComponent::SetHeap(Or(config->m_progShaderHeap, config));
    AtSetHeap2C6388(Or(config->m_heapC8, config));
    AtSetHeap3B1F98(Or(config->m_heap44, config));
    AtSetHeap2C6378(Or(config->m_heap4C, config));
    AtMatAssignment::SetHeap(Or(config->m_matAssignmentHeap, config));  // ORIGINAL: a second time
    AtSetHeap3B1F94(Or(config->m_heap68, config));
    AtSetHeap3B1F90(Or(config->m_materialHeap, config));
    AtSetHeap3B1F70(Or(config->m_heap60, config));
    AtSetHeap3B1F58(Or(config->m_heap6C, config));
    AtSetHeap2C637C(Or(config->m_heap74, config));
    AtSetHeap3B1F54(Or(config->m_heap7C, config));
    AtSetHeap3B1F50(Or(config->m_heap94, config));
    AtSetHeap3B13E0(Or(config->m_heap98, config));
    AtShader::SetHeap(Or(config->m_shaderHeap, config));  // ORIGINAL: a second time
    AtShaderDatabase::SetHeap(Or(config->m_shaderDbHeap, config));
    AtSetHeap3B13DC(Or(config->m_heapA8, config));
    AtSetHeap3B13D8(Or(config->m_heapAC, config));
    AtTextureDatabase::SetHeap(Or(config->m_textureHeap, config));
    AtSetHeap2C9C08(Or(config->m_heapB8, config));
    AtSetHeap3B13D4(Or(config->m_heapBC, config));
    AtImageDatabase::SetHeap(Or(config->m_imageHeap, config));  // ORIGINAL: a second time
    AtSetHeap3B13D0(Or(config->m_heapC0, config));
}

void AtGraphicsSystem::SetHeaps(const AtGraphicsConfig* config) {
    AtGraphicsSystemBase::SetHeaps(config);
    AtGraphicsSetHeaps(config);
}

void AtGraphicsSystem::Init(AtGraphicsConfig* config, void* below, void* memory) {
    AtGraphicsGlobals saved;
    if (config->m_unknown00 == 1) {
        config->m_unknown00 = 2;  // ORIGINAL: written back into App's config
    }
    AtGraphicsSystemBase::Init(config, below, memory);
    m_unknown34 = config->m_unknownDC;
    m_unknown38 = 0;
    if (config->m_unknown00 == 0) {
        return;
    }
    m_kind = 2;
    saved.Save();
    SetHeaps(config);
    m_factory = AtGraphicsStartFactory(&saved, config);
    m_unknown18 = m_factory->Slot0();
    m_materialOwner = m_factory->CreateDevice();
    m_unknown40 = config->m_unknownE8;
    m_unknown48 = config->m_unknownEC;
    saved.Restore();
    m_flags |= kRunning;
}

void AtGraphicsSystem::Shutdown() {
    if (m_materialOwner != 0) {
        m_materialOwner->Slot1();
        if (m_factory != 0) {
            m_factory->Destroy(m_materialOwner);
        }
    }
    if (m_kind != 0) {
        AtGraphicsStopFactory();
    }
    AtGraphicsSystemBase::Shutdown();
    m_format.Clear();
    m_unknown34 = 0;
    m_unknown38 = 0;
    m_unknown3C = 0;
    m_unknown24 = 0;
    m_unknown40 = 0;
    m_unknown44 = 0;
    m_unknown48 = 0;
    m_unknown8C = 0;
    m_unknown90 = 0;
    m_unknown94 = 0;
    m_unknown50 = 0;
}

int AtGraphicsSystem::Slot7() {
    return m_unknown38 != 0 ? m_unknown38 : m_unknown3C;
}
