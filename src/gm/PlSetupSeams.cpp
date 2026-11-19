// The seams of the player's set-up and the combat modules: each one original function that is not recovered, standing where the recovered code calls it, and stopping with a diagnostic. In a
// file of their own so that no recovered caller has one inlined: replacing a seam means deleting its body here. Each is mapped to its original address in config/functions.d/plsetup.toml.
#include "gm/FaCmCombat.h"

void AtFatal(const char* message, int file, int line, int unused);

void FaCmCombat::Update(int) { AtFatal("Gm combat: FaCmCombat slot 6, 0x000A4BB0, the update, is not recovered", 0, 0, 0); }
