// Other subsystems' functions that bounding geometry calls. Each stops with a diagnostic. In a file of its own, so that a test can supply them instead. Not recovered code.
#include "at/AtBoundingGeometry.h"

void AtFatal(const char* message, int a, int b, int c);

int AtSegmentQueryInsert(AtSegmentQuery*, float) {
    AtFatal("Atlas Scene: the segment query's insert, 0x000552B0, is not recovered here", 0, 0, 0);
    return 0;
}

void AtSegmentQueryFinish(AtSegmentQuery*, int) {
    AtFatal("Atlas Scene: the segment query's finish, 0x00057B50, is not recovered here", 0, 0, 0);
}

void AtCoordSysTransformDirection(AtCoordSys*, const AtVector3*, AtVector3*) {
    AtFatal("Atlas Scene: the coordinate system's direction transform, 0x000327B0, is not recovered here", 0, 0, 0);
}

int AtSphereSegment(const AtSceneSphere*, const AtVector3*, const AtVector3*, float*, const AtVector3*, AtVector3*, float*, int) {
    AtFatal("Atlas Scene: the sphere's segment test, 0x000434C0, is not recovered here", 0, 0, 0);
    return 0;
}
