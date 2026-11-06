// Not recovered: what finds a shader by name. It stops with a diagnostic, and it is not recorded and does not count. See include/at/AtMaterial.h.
#include "at/AtMaterial.h"

void AtFatal(const char* message, int file, int line, int unused);

void* AtFindShader(AtMaterialOwner*, const char*, const char*) {
    AtFatal("AtFindShader (0x000383A0) is not recovered", 0, 0, 0);
    return 0;
}
