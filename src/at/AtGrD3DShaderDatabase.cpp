// The Xbox shader database and its shaders. See include/at/AtGrD3DItems.h. Names inferred.
#include "at/AtGrD3DDatabase.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

// ---- Shaders ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrD3DShader::AtGrD3DShader() {
}

AtGrD3DShader::~AtGrD3DShader() {
}

// ---- The database -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrD3DShaderDatabase::AtGrD3DShaderDatabase() {
    m_shaders = 0;
}

AtGrD3DShaderDatabase::~AtGrD3DShaderDatabase() {
    AtGrD3DShaderDatabase::Clear();
}

void AtGrD3DShaderDatabase::Clear() {
    if (m_shaders != 0) {
        delete[] m_shaders;
    }
    AtShaderDatabase::Clear();
    m_shaders = 0;
}

void AtGrD3DShaderDatabase::OnReady() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_shaders = new AtGrD3DShader[m_count];
    AtPopHeap();
    if (m_shaders == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}

AtShader* AtGrD3DShaderDatabase::ShaderAt(unsigned int index) {
    return &m_shaders[index];
}

// ---- Lights -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void* AtLightDatabase::s_heap;

void AtLightDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

// The constructor, the destructor and Reset store the same values; Reset alone leaves the table.
AtLight::AtLight() {
    m_owner = 0;
    m_ambient.x = 1.0f;
    m_ambient.y = 1.0f;
    m_ambient.z = 1.0f;
    m_ambient.w = 1.0f;
    m_diffuse.x = 1.0f;
    m_diffuse.y = 1.0f;
    m_diffuse.z = 1.0f;
    m_diffuse.w = 1.0f;
    m_specular.x = 1.0f;
    m_specular.y = 1.0f;
    m_specular.z = 1.0f;
    m_specular.w = 1.0f;
    m_innerConeAngle = 1.0f;
    m_outerConeAngle = 1.0f;
    m_range = 1000.0f;
    m_attenuation[0] = 1.0f;
    m_attenuation[1] = 0.0f;
    m_attenuation[2] = 0.0f;
}

AtLight::~AtLight() {
    m_owner = 0;
    m_ambient.x = 1.0f;
    m_ambient.y = 1.0f;
    m_ambient.z = 1.0f;
    m_ambient.w = 1.0f;
    m_diffuse.x = 1.0f;
    m_diffuse.y = 1.0f;
    m_diffuse.z = 1.0f;
    m_diffuse.w = 1.0f;
    m_specular.x = 1.0f;
    m_specular.y = 1.0f;
    m_specular.z = 1.0f;
    m_specular.w = 1.0f;
    m_innerConeAngle = 1.0f;
    m_outerConeAngle = 1.0f;
    m_range = 1000.0f;
    m_attenuation[0] = 1.0f;
    m_attenuation[1] = 0.0f;
    m_attenuation[2] = 0.0f;
}

void AtLight::Reset() {
    m_owner = 0;
    m_ambient.x = 1.0f;
    m_ambient.y = 1.0f;
    m_ambient.z = 1.0f;
    m_ambient.w = 1.0f;
    m_diffuse.x = 1.0f;
    m_diffuse.y = 1.0f;
    m_diffuse.z = 1.0f;
    m_diffuse.w = 1.0f;
    m_specular.x = 1.0f;
    m_specular.y = 1.0f;
    m_specular.z = 1.0f;
    m_specular.w = 1.0f;
    m_innerConeAngle = 1.0f;
    m_outerConeAngle = 1.0f;
    m_range = 1000.0f;
    m_attenuation[0] = 1.0f;
    m_attenuation[1] = 0.0f;
    m_attenuation[2] = 0.0f;
}

AtGrD3DLight::AtGrD3DLight() {
}

AtGrD3DLight::~AtGrD3DLight() {
}

AtGrD3DLightDatabase::AtGrD3DLightDatabase() {
    m_lights = 0;
    m_owner = 0;
}

AtGrD3DLightDatabase::~AtGrD3DLightDatabase() {
    AtGrD3DLightDatabase::Clear();
}

void AtGrD3DLightDatabase::Clear() {
    if (m_lights != 0) {
        delete[] m_lights;
    }
    m_lights = 0;
    m_owner = 0;
    AtLightDatabase::Clear();
}

void AtGrD3DLightDatabase::MakeLights(AtMaterialOwner* owner) {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_lights = new AtGrD3DLight[m_count];
    AtPopHeap();
    if (m_lights == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_owner = owner;
}

AtLight* AtGrD3DLightDatabase::LightAt(unsigned int index) {
    return &m_lights[index];
}
