// Not recovered code. What stands between the texture and material databases and the graphics device: each stops with a diagnostic naming the original's address. In a file of its own, so that a test can supply
// these instead. A test that supplies any must supply all. See include/at/AtGrD3DTextures.h and include/at/AtGrD3DTextureLoad.h. The calls a texture makes when it loads (0x00208430 to 0x0020B220) are the Xbox
// Direct3D library's.
#include "at/AtGrD3DTextureLoad.h"

void AtGrD3DLoadMaterial(AtGrD3DMaterial*, AtMaterialOwner*) {
    AtFatal("Atlas Graphics: a material's slot 0, 0x0010B7F0, is not recovered: it builds the material's stage states", 0, 0, 0);
}

void AtGrD3DReleaseStage(void*, unsigned int stage) {
    AtFatal("Atlas Graphics: the device's stage release, 0x000FB630, is not recovered (the stage follows)", (int)stage, 0, 0);
}

void AtGrD3DBeforeRelease(void*) {
    AtFatal("Atlas Graphics: 0x00205E20, called before a texture is released, is not recovered", 0, 0, 0);
}

void AtGrD3DRelease(void*) {
    AtFatal("Atlas Graphics: a Direct3D resource's release, 0x00205D10, is not recovered", 0, 0, 0);
}

void* AtGrD3DGetBackBuffer(int) {
    AtFatal("Atlas Graphics: the device's front or back buffer, 0x00208430, is not recovered", 0, 0, 0);
    return 0;
}

void* AtGrD3DCreateTexture(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int) {
    AtFatal("Atlas Graphics: making a Direct3D texture, 0x0020B160, is not recovered", 0, 0, 0);
    return 0;
}

void* AtGrD3DSurfaceLevel(void*, unsigned int) {
    AtFatal("Atlas Graphics: a Direct3D texture's level, 0x0020B220, is not recovered", 0, 0, 0);
    return 0;
}

void AtGrD3DDescribe(void*, AtGrD3DSurfaceDesc*) {
    AtFatal("Atlas Graphics: describing a Direct3D surface, 0x0020AAB0, is not recovered", 0, 0, 0);
}

void AtGrD3DLockRect(void*, AtGrD3DLockedRect*, const void*, unsigned int) {
    AtFatal("Atlas Graphics: locking a Direct3D surface, 0x0020AAD0, is not recovered", 0, 0, 0);
}
