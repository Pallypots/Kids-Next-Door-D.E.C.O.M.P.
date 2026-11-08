// Not recovered code. What stands between the recovered databases and Direct3D: each stops with a diagnostic naming the original's address. In a file of its own, so that a test can supply these instead.
// See include/at/AtGrD3DDatabase.h.
#include "at/AtGrD3DDatabase.h"

void AtFatal(const char* message, int a, int b, int c);

unsigned int AtGrD3DUnrecovered(unsigned int va, void*, unsigned int, unsigned int) {
    AtFatal("Atlas Graphics: this function of the Xbox graphics layer is not recovered (its address is the number that follows)", (int)va, 0, 0);
    return 0;
}

void* AtGrD3DAllocateContiguous(unsigned int size, unsigned int) {
    AtFatal("Atlas Graphics: memory for the graphics processor, 0x00205EA0, is not recovered (the size follows)", (int)size, 0, 0);
    return 0;
}

void AtGrD3DFreeContiguous(void*) {
    AtFatal("Atlas Graphics: memory for the graphics processor, 0x00205EE0, is not recovered", 0, 0, 0);
}

void AtGrD3DReadVertexArray(AtGrD3DVertexArray*, char**, char**, char**) {
    AtFatal("Atlas Graphics: a vertex array's binary reader, 0x000EC8C0, is not recovered: it makes a vertex buffer", 0, 0, 0);
}

void AtGrD3DReadIndexArray(AtGrD3DIndexArray*, char**, char**) {
    AtFatal("Atlas Graphics: an index array's binary reader, 0x000F7EE0, is not recovered", 0, 0, 0);
}

void AtGrD3DReadVertexStream(AtGrD3DVertexStream*, AtMaterialOwner*, char**, char**) {
    AtFatal("Atlas Graphics: a vertex stream's binary reader, 0x000EE750, is not recovered", 0, 0, 0);
}

void AtGrD3DReadMesh(AtGrD3DMesh*, AtMaterialOwner*, AtGrD3DMeshDatabase*, char**, char**, char**, char**, char**, char**, char*) {
    AtFatal("Atlas Graphics: a mesh's binary reader, 0x00122550, is not recovered", 0, 0, 0);
}
