// The seams of FaCmCombat's update (gm/combat): each one original function of another class that is not recovered, standing where the recovered code calls it, and stopping with a diagnostic.
// In a file of their own so that no recovered caller has one inlined: replacing a seam means deleting its body here. Each is mapped to its original address in config/functions.d/combat.toml.
// What each stands for is in include/gm/FaCmCombat.h (and include/gm/GmAcPlayer.h for the input's button).
#include "gm/FaCmCombat.h"
#include "gm/GmAcPlayer.h"

void AtFatal(const char* message, int file, int line, int unused);

void GmDynamicNodeIterator::Find(const char*) { AtFatal("Gm combat: 0x00164760, finding the dynamic nodes' actors by name, is not recovered", 0, 0, 0); }
void GmDynamicNodeIterator::Next() { AtFatal("Gm combat: 0x001646C0, the next of the dynamic nodes' actors, is not recovered", 0, 0, 0); }

int GmAcCharacterCanSee(GmAcCharacter*, GmAcActor*, int, int, int, float*, int, int, int, int, int) {
    AtFatal("Gm combat: 0x00130E50, whether a character sees its target, is not recovered", 0, 0, 0);
    return 0;
}

void FaAcPlayerReadStick(GmAcActor*, float* out, int, int) {
    AtFatal("Gm combat: 0x00093300, reading a player's stick, is not recovered", 0, 0, 0);
    out[0] = 0.0f;
    out[1] = 0.0f;
}

float FaAcPlayerStickAngle(GmAcActor*, int, int, int) {
    AtFatal("Gm combat: 0x00093350, a player's stick's angle, is not recovered", 0, 0, 0);
    return 0.0f;
}

GmAcActor* GmAcFindAttached(GmAcActor*, const char*) {
    AtFatal("Gm combat: 0x00169730, an actor attached by name, is not recovered", 0, 0, 0);
    return 0;
}

AtSceneNode* GmAcNodeAimNode(GmAcNode*) {
    AtFatal("Gm combat: 0x0014C300, a node's aim node, is not recovered", 0, 0, 0);
    return 0;
}

GmAcActor* GmSpawnManager::Spawn(const char*) {
    AtFatal("Gm combat: 0x00153CA0, spawning an actor, is not recovered", 0, 0, 0);
    return 0;
}

GmButtonState AppUnknown692C::FindButton(int, const char*) {
    AtFatal("Gm combat: 0x0008F5D0, a controller's button by name, is not recovered", 0, 0, 0);
    GmButtonState state;
    state.m_state = 0;
    state.m_unknown04 = 0;
    return state;
}
