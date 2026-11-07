// The classes of node not recovered. A scene that holds any stops here with a diagnostic. In a file of its own, so that a test can supply them instead. Not recovered code.
#include "at/AtScene.h"

void AtFatal(const char* message, int a, int b, int c);

unsigned int AtSceneActorsSize(int) {
    AtFatal("Atlas Scene: this class of node is not recovered", 0, 0, 0);
    return 0;
}

void AtSceneActorsConstruct(int, void*, unsigned int) {
    AtFatal("Atlas Scene: this class of node is not recovered", 0, 0, 0);
}

void AtSceneActorsDestroy(int, void*, unsigned int) {
    AtFatal("Atlas Scene: this class of node is not recovered", 0, 0, 0);
}

AtCoordSys* AtSceneActorSharedCoordSys(const AtSceneNode*) {
    AtFatal("Atlas Scene: this class of node is not recovered", 0, 0, 0);
    return 0;
}

GmNameMap* AtSceneActorJoints(const AtSceneNode*) {
    AtFatal("Atlas Scene: this class of node is not recovered", 0, 0, 0);
    return 0;
}
