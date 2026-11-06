// Not recovered. Fails with a diagnostic; never recorded, never counted.
#include "gm/GmNameMap.h"

void GmNameMapGroup::RemoveFromParent(GmNameMapGroup*) {
    AtFatal("GmNameMapGroup::RemoveFromParent (0x00018010) is not recovered", 0, 0, 0);
}
