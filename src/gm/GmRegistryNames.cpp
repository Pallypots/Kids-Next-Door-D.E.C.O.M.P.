// The registry names' life cycle. See include/gm/GmRegistryNames.h. Names inferred.
// Add, Release and KeyFor are in GmRegistryNamesAdd.cpp, so that a test which supplies its own Add and Release still links the rest of the class.
#include "gm/GmRegistryNames.h"

GmRegistryNames* GmRegistryNames::s_instance;

GmRegistryNames::GmRegistryNames() {
    s_instance = this;
}

GmRegistryNames::~GmRegistryNames() {
    s_instance = 0;
    GmNameMap::Clear();
}

void GmRegistryNames::Clear() {
    GmNameMap::Clear();
}

int GmRegistryNames::Init(unsigned int count) {
    if (m_entries != 0) {
        Clear();
    }
    GmNameMap::Init(count);
    return 0;
}
