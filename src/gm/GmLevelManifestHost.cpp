// What the level manifest reaches outside itself and is not recovered: what each item loads into, and two of the base's frees. Each stops with a diagnostic. The base's words, Finished and items
// are gm/dbmanifest's (src/gm/GmDatabaseManifest.cpp), whose own seams are in src/gm/GmDatabaseManifestHost.cpp.
// In a file of its own, so that a test can supply them instead. Not recovered code. Recovered code is in src/gm/GmLevelManifest.cpp and src/gm/GmLevelManifestItems.cpp.
#include "gm/GmLevelManifest.h"

void AtFatal(const char* message, int a, int b, int c);

namespace {
void StopHere(const char* what) {
    AtFatal(what, 0, 0, 0);
}
}

void GmAudioFreeDatabase(void*) { StopHere("Level manifest: 0x00139F90 is not recovered"); }

void GmBehaviorList::Prepare() { StopHere("Behaviour lists (0x00198C90) are not recovered"); }
GmBehaviorList* GmBehaviorListOwner::LoadAnimations(const char*) { StopHere("Behaviour lists (0x0019C5E0) are not recovered"); return 0; }
GmBehaviorList* GmBehaviorListOwner::LoadSounds(const char*) { StopHere("Behaviour lists (0x0019C100) are not recovered"); return 0; }
void GmBehaviorListOwner::Release(GmBehaviorList*) { StopHere("Behaviour lists (0x00198E70) are not recovered"); }
GmScript* GmScriptOwner::Load(const char*) { StopHere("Small C scripts (0x00070310) are not recovered"); return 0; }
void GmScriptOwner::Release(GmScript*) { StopHere("Small C scripts (0x00070150) are not recovered"); }

GmTriggerTable::GmTriggerTable() { StopHere("Trigger tables (0x001963B0) are not recovered"); }
GmTriggerTable::~GmTriggerTable() { StopHere("Trigger tables (0x00196480) are not recovered"); }
void GmTriggerTable::Slot0() { StopHere("Trigger tables are not recovered"); }
void GmTriggerTable::Unload() { StopHere("Trigger tables are not recovered"); }
void GmTriggerTable::Load(const char*) { StopHere("Trigger tables (0x00196500) are not recovered"); }
GmSubscriberTable::GmSubscriberTable() { StopHere("Subscriber tables (0x001968B0) are not recovered"); }
GmSubscriberTable::~GmSubscriberTable() { StopHere("Subscriber tables (0x00196980) are not recovered"); }
void GmSubscriberTable::Slot0() { StopHere("Subscriber tables are not recovered"); }
void GmSubscriberTable::Unload() { StopHere("Subscriber tables are not recovered"); }
void GmSubscriberTable::Load(const char*) { StopHere("Subscriber tables (0x00196A00) are not recovered"); }

GmVfxDatabase* GmVfxManager::Load(const char*) { StopHere("VFX databases (0x001616A0) are not recovered"); return 0; }
void GmVfxManager::Delete(GmVfxDatabase*) { StopHere("VFX databases are not recovered"); }

void GmDynamicNodeLoader::Load(const char*, int, AtScene*) { StopHere("Dynamic nodes (0x0014FF60) are not recovered"); }
void GmDynamicNodeLoader::Unload(AtScene*, int) { StopHere("Dynamic nodes (0x00150020) are not recovered"); }
void AtScene::InsertInto(AtSpaceGraph*) { StopHere("Atlas Scene: 0x0003E860 is not recovered"); }
void AtScene::RemoveFrom(AtSpaceGraph*) { StopHere("Atlas Scene: 0x0003E890 is not recovered"); }

void* GmAudioAllocateNodeBank() { StopHere("Sound nodes (0x001367E0) are not recovered"); return 0; }
void GmAudioLoadNodes(void*, const char*) { StopHere("Sound nodes (0x0013AA80) are not recovered"); }
void GmAudioFreeNodes(void*) { StopHere("Sound nodes (0x00138E50) are not recovered"); }
void GmAudioReleaseNodeBank(void*) { StopHere("Sound nodes (0x00136810) are not recovered"); }
void GmAudioRegisterNodes(void*) { StopHere("Sound nodes (0x00138A10) are not recovered"); }
void* GmAudioLoadInstanceSounds(const char*) { StopHere("Instance sounds (0x00139C40) are not recovered"); return 0; }
void GmAudioFreeInstanceSounds(void*) { StopHere("Instance sounds (0x00136B30) are not recovered"); }
void GmAudioRegisterInstanceSounds(void*) { StopHere("Instance sounds (0x00136A10) are not recovered"); }
void* GmAudioLoadCollisionSounds(const char*) { StopHere("Collision sounds (0x00139610) are not recovered"); return 0; }
void GmAudioFreeCollisionSounds(void*) { StopHere("Collision sounds (0x00139B50) are not recovered"); }

void GmMusicChain::Load(const char*, int) { StopHere("Music chains (0x0013D2A0) are not recovered"); }
void GmMusicChain::Clear() { StopHere("Music chains (0x0013CFF0) are not recovered"); }
GmMusicChain* GmMusicManager::Allocate() { StopHere("Music chains (0x0013D0C0) are not recovered"); return 0; }
void GmMusicManager::Stop(GmMusicChain*) { StopHere("Music chains (0x0013D3A0) are not recovered"); }
void GmMusicManager::Free(GmMusicChain*) { StopHere("Music chains (0x0013D360) are not recovered"); }
