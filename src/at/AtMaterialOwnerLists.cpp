// More of AtMaterialOwner: the add methods of the families beside textures, materials and shaders, and the search for a material. See include/at/AtMaterial.h. They are one pattern on successive tables.
#include "at/AtMaterial.h"

void* AtMaterialOwner::FindMaterial(const char* name, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        return m_materialSpaces.Find(ns, name);
    }
    return m_materials.Find(name);
}

void AtMaterialOwner::AddLights(GmNameMap* map, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        m_lightSpaces.Add(ns, map);
        return;
    }
    m_lights.Add(map);
}

void AtMaterialOwner::AddMatAssignments(GmNameMap* map, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        m_matAssignmentSpaces.Add(ns, map);
        return;
    }
    m_matAssignments.Add(map);
}

void AtMaterialOwner::AddMatAnimations(GmNameMap* map, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        m_matAnimationSpaces.Add(ns, map);
        return;
    }
    m_matAnimations.Add(map);
}

void AtMaterialOwner::AddFonts(GmNameMap* map, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        m_fontSpaces.Add(ns, map);
        return;
    }
    m_fonts.Add(map);
}

void AtMaterialOwner::AddImages(GmNameMap* map, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        m_imageSpaces.Add(ns, map);
        return;
    }
    m_images.Add(map);
}
