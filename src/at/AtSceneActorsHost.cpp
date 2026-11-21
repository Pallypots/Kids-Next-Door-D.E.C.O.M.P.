// DataLink, which is not recovered: a scene that has it stops here with a diagnostic. In a file of its own, so that a test can supply it instead. Not recovered code.
#include "at/AtScene.h"

void AtFatal(const char* message, int a, int b, int c);

void* AtSceneFindDataLink(AtNameTableOwner*, const char*, const char*) {
    AtFatal("Atlas Scene: DataLink is not recovered", 0, 0, 0);
    return 0;
}
