// Not recovered code. What stands between the texture and material databases and the graphics device: each stops with a diagnostic naming the original's address. In a file of its own, so that a test can supply
// these instead. See include/at/AtGrD3DTextures.h.
#include "at/AtGrD3DTextures.h"

void AtGrD3DLoadTexture2D(AtGrD3DTexture2D*, AtMaterialOwner*) {
    AtFatal("Atlas Graphics: a 2D texture's slot 12, 0x000FCAD0, is not recovered: it reads the image file and makes the Direct3D texture", 0, 0, 0);
}

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
