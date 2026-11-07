// See include/at/AtMaterial.h. In a file of its own, so that a test of the material alone can answer a shader's name for itself.
#include "at/AtMaterial.h"

void* AtMaterialOwner::FindShader(const char* name, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        return m_shaderSpaces.Find(ns, name);
    }
    return m_shaders.Find(name);
}

void AtMaterialOwner::AddMaterials(GmNameMap* map, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        m_materialSpaces.Add(ns, map);
        return;
    }
    m_materials.Add(map);
}
