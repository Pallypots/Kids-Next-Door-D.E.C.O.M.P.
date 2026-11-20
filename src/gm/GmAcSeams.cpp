// The seams of the actor base: each one original function that is not recovered, standing where the recovered code calls it, and stopping with a diagnostic. In a file of their own so that no
// recovered caller has one inlined: replacing a seam means deleting its body here. Each is mapped to its original address in config/functions.d/gmactor.toml. See include/gm/GmAcActor.h.
#include "gm/GmAcActor.h"

void AtFatal(const char* message, int file, int line, int unused);

GmAcAnimSlot::~GmAcAnimSlot() {
    AtFatal("Gm actor: 0x001B3B80, an animation slot's destructor, is not recovered", 0, 0, 0);
}

void GmAcAnimSlots::Resize(unsigned int) {
    AtFatal("Gm actor: 0x00159760, resizing an animation controller's slots, is not recovered", 0, 0, 0);
}

void GmAcAnimController::Detach() {
    AtFatal("Gm actor: 0x00159300, detaching an animation controller, is not recovered", 0, 0, 0);
}

void GmAcLinkedItem::Release(const ReleaseArgs*) {
    AtFatal("Gm actor: 0x001B1420, releasing an item of an actor's list, is not recovered", 0, 0, 0);
}
void GmAcActor::SpawnEffect(const char*, int, const char*, int, int, int) { AtFatal("Gm actor: 0x0014A260, an actor's effect, is not recovered", 0, 0, 0); }
void GmAcActor::SceneSetFlag10(int) { AtFatal("Gm actor: 0x0014A8D0, an actor's scene flag, is not recovered", 0, 0, 0); }
void GmAcAudio::Slot38(int) { AtFatal("Gm actor: 0x00194BF0, the audio component, is not recovered", 0, 0, 0); }
void GmAcComponent::SendEvent(int, const char*) { AtFatal("Gm actor: 0x0015EC10, the component's events, is not recovered", 0, 0, 0); }
void GmAcTriggerTable::FindTrigger(const char*, char*, char*) { AtFatal("Gm actor: 0x0017E7C0, the trigger table, is not recovered", 0, 0, 0); }
GmSoundNote* GmAcTriggerTable::FindNote(const char*) { AtFatal("Gm actor: 0x0017E1A0, the trigger table's notes, is not recovered", 0, 0, 0); return 0; }
void GmEventManager::Post(const char*, const char*, GmAcActor*) { AtFatal("Gm actor: 0x001511E0, posting a game event, is not recovered", 0, 0, 0); }
void GmDynamicNodeRelease(GmAcNode*, int) { AtFatal("Gm actor: 0x0014FB40, releasing an attached node, is not recovered", 0, 0, 0); }
int GmVfxManagerHas(GmVfxManager*, const char*) { AtFatal("Gm actor: 0x001616C0, finding an effect, is not recovered", 0, 0, 0); return 0; }
void GmAudioDataPlayNote(GmAudioData*, GmAcActor*, GmSoundNote*) { AtFatal("Gm actor: 0x001386D0, playing a sound note, is not recovered", 0, 0, 0); }
void AtCoordSysWorldPosition(AtCoordSys*, AtVector3*) { AtFatal("Gm actor: 0x000332B0, a coordinate system's position, is not recovered", 0, 0, 0); }
void AtCoordSysPlace(AtCoordSys*, const AtVector3*, int, const AtVector3*, int) { AtFatal("Gm actor: 0x00032EA0, placing a coordinate system, is not recovered", 0, 0, 0); }
