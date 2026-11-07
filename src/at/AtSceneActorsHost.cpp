// The classes of node not recovered, and DataLink. A scene that holds any stops here with a diagnostic. In a file of its own, so that a test can supply them instead. Not recovered code. Recovered classes are in src/at/AtSceneActor.cpp.
#include "at/AtScene.h"

void AtFatal(const char* message, int a, int b, int c);

unsigned int AtSceneUnrecoveredActorsSize(int) {
    AtFatal("Atlas Scene: this class of node is not recovered", 0, 0, 0);
    return 0;
}

void AtSceneUnrecoveredActorsConstruct(int, void*, unsigned int) {
    AtFatal("Atlas Scene: this class of node is not recovered", 0, 0, 0);
}

void AtSceneUnrecoveredActorsDestroy(int, void*, unsigned int) {
    AtFatal("Atlas Scene: this class of node is not recovered", 0, 0, 0);
}

void* AtSceneFindDataLink(AtNameTableOwner*, const char*, const char*) {
    AtFatal("Atlas Scene: DataLink is not recovered", 0, 0, 0);
    return 0;
}
