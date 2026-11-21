// What the level manifest reaches outside itself and is not recovered: what each item loads into, and two of the base's frees. Each stops with a diagnostic. The base's words, Finished and items
// are gm/dbmanifest's (src/gm/GmDatabaseManifest.cpp), whose own seams are in src/gm/GmDatabaseManifestHost.cpp.
// In a file of its own, so that a test can supply them instead. Not recovered code. Recovered code is in src/gm/GmLevelManifest.cpp and src/gm/GmLevelManifestItems.cpp.
#include "gm/GmLevelManifest.h"
#include "gm/GmAudioData.h"

void AtFatal(const char* message, int a, int b, int c);

namespace {
void StopHere(const char* what) {
    AtFatal(what, 0, 0, 0);
}
}

// Recovered in src/gm/GmSoundPlay.cpp: this forwards to the sound manager, as the original calls its member.
void GmAudioFreeDatabase(void* database) { GmAudioData::s_instance->RemoveDatabase((AtAudioDatabase*)database); }

void GmBehaviorList::Prepare() { StopHere("Behaviour lists (0x00198C90) are not recovered"); }
GmBehaviorList* GmBehaviorListOwner::LoadAnimations(const char*) { StopHere("Behaviour lists (0x0019C5E0) are not recovered"); return 0; }
GmBehaviorList* GmBehaviorListOwner::LoadSounds(const char*) { StopHere("Behaviour lists (0x0019C100) are not recovered"); return 0; }
void GmBehaviorListOwner::Release(GmBehaviorList*) { StopHere("Behaviour lists (0x00198E70) are not recovered"); }

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

// Recovered in src/gm/GmSoundNodes.cpp: these forward to the sound manager, as the original calls its members.
void* GmAudioAllocateNodeBank() { return GmAudioData::s_instance->AllocateNodeBank(); }
void GmAudioLoadNodes(void* bank, const char* path) { GmAudioData::s_instance->LoadNodes((GmSoundNodeBank*)bank, path); }
void GmAudioFreeNodes(void* bank) { GmAudioData::s_instance->FreeNodes((GmSoundNodeBank*)bank); }
void GmAudioReleaseNodeBank(void* bank) { GmAudioData::s_instance->ReleaseNodeBank((GmSoundNodeBank*)bank); }
void GmAudioRegisterNodes(void* bank) { GmAudioData::s_instance->RegisterNodes((GmSoundNodeBank*)bank); }
void* GmAudioLoadInstanceSounds(const char* path) { return GmAudioData::s_instance->LoadInstanceSounds(path); }
void GmAudioFreeInstanceSounds(void* data) { GmAudioData::s_instance->FreeInstanceSounds((GmInstanceSoundBank*)data); }
void GmAudioRegisterInstanceSounds(void* data) { GmAudioData::s_instance->RegisterInstanceSounds((GmInstanceSoundBank*)data); }
void* GmAudioLoadCollisionSounds(const char* path) { return GmAudioData::s_instance->LoadCollisionSounds(path); }
void GmAudioFreeCollisionSounds(void* data) { GmAudioData::s_instance->FreeCollisionSounds((GmCollisionSoundBank*)data); }

void GmMusicChain::Load(const char*, int) { StopHere("Music chains (0x0013D2A0) are not recovered"); }
void GmMusicChain::Clear() { StopHere("Music chains (0x0013CFF0) are not recovered"); }
GmMusicChain* GmMusicManager::Allocate() { StopHere("Music chains (0x0013D0C0) are not recovered"); return 0; }
void GmMusicManager::Stop(GmMusicChain*) { StopHere("Music chains (0x0013D3A0) are not recovered"); }
void GmMusicManager::Free(GmMusicChain*) { StopHere("Music chains (0x0013D360) are not recovered"); }
