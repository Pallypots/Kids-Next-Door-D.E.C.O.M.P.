// The seams of GmAcNPC and FaAcNPC: each one original function that is not recovered, standing where the recovered code calls it, and stopping with a diagnostic. In a file of their own so that no
// recovered caller has one inlined: replacing a seam means deleting its body here. Each is mapped to its original address in config/functions.d/gmnpc.toml.
#include "gm/GmAcNPC.h"

void AtFatal(const char* message, int file, int line, int unused);

AtTempText* AtFormatTemp(AtTempText* out, const char*, ...) {
    AtFatal("Gm NPC: 0x00072330, formatting into a passing buffer, is not recovered", 0, 0, 0);
    out->m_text = 0;
    return out;
}

GmGameEvent::GmGameEvent() { AtFatal("Gm NPC: 0x00152590, a game event, is not recovered", 0, 0, 0); }
void GmGameEvent::Set(GmAcActor*, int, int, int) { AtFatal("Gm NPC: 0x00152780, setting a game event, is not recovered", 0, 0, 0); }
void GmEventManagerSend(GmEventManager*, const char*, const char*, GmGameEvent*, int) { AtFatal("Gm NPC: 0x001512B0, sending a game event, is not recovered", 0, 0, 0); }

int GmAcCharacterCanSee(GmAcCharacter*, GmAcActor*, int, int, int, int, int, int, int, int, int) {
    AtFatal("Gm NPC: 0x00130E50, whether a character sees its target, is not recovered", 0, 0, 0);
    return 0;
}

FaCmCombat::FaCmCombat() { AtFatal("Gm NPC: 0x000A3360, an NPC's combat object, is not recovered", 0, 0, 0); }
FaCmCombat::~FaCmCombat() { AtFatal("Gm NPC: 0x000A46D0, deleting an NPC's combat object, is not recovered", 0, 0, 0); }
void FaCmCombat::Attach(GmAcActor*) { AtFatal("Gm NPC: 0x000A33B0, attaching an NPC's combat object, is not recovered", 0, 0, 0); }
void FaCmCombat::Slot2() { AtFatal("Gm NPC: 0x000A1F10, a combat object's slot 2, is not recovered", 0, 0, 0); }
void FaCmCombat::Stop() { AtFatal("Gm NPC: 0x000A3430, stopping a combat object, is not recovered", 0, 0, 0); }
void FaCmCombat::Slot5() { AtFatal("Gm NPC: 0x000A2310, a combat object's slot 5, is not recovered", 0, 0, 0); }
void FaCmCombat::Update(int) { AtFatal("Gm NPC: 0x000A4BB0, updating a combat object, is not recovered", 0, 0, 0); }
int FaCmCombat::Slot7() { AtFatal("Gm NPC: 0x000A3380, a combat object's slot 7, is not recovered", 0, 0, 0); return 0; }
