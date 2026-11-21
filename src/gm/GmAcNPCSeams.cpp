// The seams of GmAcNPC and FaAcNPC: each one original function that is not recovered, standing where the recovered code calls it, and stopping with a diagnostic. In a file of their own so that no
// recovered caller has one inlined: replacing a seam means deleting its body here. Each is mapped to its original address in config/functions.d/gmnpc.toml.
#include "gm/GmAcNPC.h"

void AtFatal(const char* message, int file, int line, int unused);

AtTempText* AtFormatTemp(AtTempText* out, const char*, ...) {
    AtFatal("Gm NPC: 0x00072330, formatting into a passing buffer, is not recovered", 0, 0, 0);
    out->m_text = 0;
    return out;
}

void GmEventManagerSend(GmEventManager*, const char*, const char*, GmGameEvent*, int) { AtFatal("Gm NPC: 0x001512B0, sending a game event, is not recovered", 0, 0, 0); }

