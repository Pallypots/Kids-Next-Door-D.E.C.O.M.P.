// The seams of the player classes: each one original function that is not recovered, standing where the recovered code calls it, and stopping with a diagnostic. In a file of their own so that no
// recovered caller has one inlined: replacing a seam means deleting its body here. Each is mapped to its original address in config/functions.d/gmplayer.toml.
#include "gm/GmAcPlayer.h"

void AtFatal(const char* message, int file, int line, int unused);

float AppUnknown692C::AxisValue(int, int) {
    AtFatal("Gm player: 0x0006DFE0, the input's value of a controller's axis, is not recovered", 0, 0, 0);
    return 0.0f;
}

void GmAcSprite::Setup(void*) { AtFatal("Gm player: 0x0006E2E0, setting up a sprite, is not recovered", 0, 0, 0); }
void GmAcSprite::SetupNamed(void*, const char*, const char*) { AtFatal("Gm player: 0x0006E260, setting up a sprite with a named material, is not recovered", 0, 0, 0); }
