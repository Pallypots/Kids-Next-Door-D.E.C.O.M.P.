// STAND-INS for at/asd's AtSceneDatabaseLoader (src/at/AtSceneDatabaseLoader.cpp on the branch integrate-batch5, not on master): its constructor, destructor, Clear, SpaceGraphAt and
// LoaderAt, written to the original's code so that a scene item can be made, configured and freed; its Load is a seam in src/gm/GmDatabaseManifestHost.cpp. Recorded nowhere: that branch records them.
// When it is merged, delete this file and the class's declaration in include/gm/GmDatabaseManifestItems.h. The oracle runs the original's code for these on the other side.
#include "gm/GmDatabaseManifestItems.h"

typedef decltype(sizeof(0)) GmAsdSize;

namespace {

// The original's delete[] of an array of a class with a virtual destructor: the block alone when the count is 0, otherwise through the first element's vector deleting destructor.
template <class T> void DeleteList(T* first) {
    if (first != 0) {
        GmAsdSize* block = (GmAsdSize*)first - 1;
        if (*block != 0) {
            delete[] first;
        } else {
            operator delete(block);
        }
    }
}

}  // namespace

AtSpaceGraph* AtSceneDatabaseLoader::SpaceGraphAt(unsigned int index) {
    return m_spaceGraphs + index;
}

AtSceneDatabaseLoader* AtSceneDatabaseLoader::LoaderAt(unsigned int index) {
    return m_loaders + index;
}

AtSceneDatabaseLoader::AtSceneDatabaseLoader() {
    m_owner = 0;
    m_nodes = 0;
    m_modifiers = 0;
    m_emitters = 0;
    m_lensFlares = 0;
    m_cellGraphs = 0;
    m_spaceGraphs = 0;
    m_loaders = 0;
    m_geometry = 0;
    m_strings = 0;
    m_imageDraws = 0;
    m_textDraws = 0;
    m_loaderNames = 0;
    m_nodeCount = 0;
    m_modifierCount = 0;
    m_emitterCount = 0;
    m_lensFlareCount = 0;
    m_spaceGraphCount = 0;
    m_loaderCount = 0;
    m_geometryCount = 0;
    m_stringCount = 0;
    m_cellGraphCount = 0;
    m_imageDrawCount = 0;
    m_textDrawCount = 0;
}

AtSceneDatabaseLoader::~AtSceneDatabaseLoader() {
    if (m_owner != 0) {
        AtSceneDatabaseLoader::Clear();
    }
}

void AtSceneDatabaseLoader::Clear() {
    DeleteList(m_geometry);
    DeleteList(m_loaders);
    DeleteList(m_cellGraphs);
    DeleteList(m_emitters);
    DeleteList(m_lensFlares);
    DeleteList(m_modifiers);
    DeleteList(m_nodes);
    DeleteList(m_strings);
    DeleteList(m_imageDraws);
    DeleteList(m_textDraws);
    // The space graphs have no virtual destructor: each is destroyed, last first, and the block freed, a count of 0 included.
    if (m_spaceGraphs != 0) {
        GmAsdSize* block = (GmAsdSize*)m_spaceGraphs - 1;
        for (GmAsdSize i = *block; i > 0; i--) {
            m_spaceGraphs[i - 1].~AtSpaceGraph();
        }
        operator delete(block);
    }
    m_spaceGraphsByName.Clear();
    m_nodesByName.Clear();
    m_owner = 0;
    m_nodes = 0;
    m_modifiers = 0;
    m_emitters = 0;
    m_lensFlares = 0;
    m_cellGraphs = 0;
    m_spaceGraphs = 0;
    m_loaders = 0;
    m_geometry = 0;
    m_strings = 0;
    m_imageDraws = 0;
    m_textDraws = 0;
    m_loaderNames = 0;
    m_nodeCount = 0;
    m_modifierCount = 0;
    m_emitterCount = 0;
    m_lensFlareCount = 0;
    m_spaceGraphCount = 0;
    m_loaderCount = 0;
    m_geometryCount = 0;
    m_stringCount = 0;
    m_cellGraphCount = 0;
    m_imageDrawCount = 0;
    m_textDrawCount = 0;
}
