// A scene list's nodes into a space graph and out again: AtSceneDatabaseLoader::InsertInto and RemoveFrom. See include/at/AtSceneDatabaseLoader.h.
#include "at/AtSceneDatabaseLoader.h"

#include "at/AtScene.h"

void AtSceneDatabaseLoader::InsertInto(AtSpaceGraph* graph) {
    for (unsigned int i = 0; i < m_nodeCount; i++) {
        m_nodes[i].InsertInto(graph);
    }
    for (unsigned int i = 0; i < m_loaderCount; i++) {
        m_loaders[i].InsertInto(graph);
    }
}

void AtSceneDatabaseLoader::RemoveFrom(AtSpaceGraph* graph) {
    for (unsigned int i = 0; i < m_loaderCount; i++) {
        m_loaders[i].RemoveFrom(graph);
    }
    for (unsigned int i = 0; i < m_nodeCount; i++) {
        m_nodes[i].RemoveFrom(graph);
    }
}
