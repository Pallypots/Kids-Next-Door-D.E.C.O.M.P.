// Not recovered code. What stands between the font and image databases' items and the graphics device: each stops with a diagnostic naming the original's address. In a file of its own, so that a test can
// supply these instead. See include/at/AtGrD3DItems.h.
#include "at/AtGrD3DItems.h"

void AtGrD3DLoadFont2D(AtGrD3DFont2D*, AtMaterialOwner*) {
    AtFatal("Atlas Graphics: a 2D font's slot 9, 0x00123850, is not recovered: what the font does with the device once read", 0, 0, 0);
}

void AtGrD3DLoadImage(AtGrD3DImage*, AtMaterialOwner*) {
    AtFatal("Atlas Graphics: an image's slot 10, 0x00124CD0, is not recovered: it loads the image's pixels and makes its texture", 0, 0, 0);
}
