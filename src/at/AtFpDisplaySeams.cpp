// Not recovered: what the display manager and the nodes call that loads levels, plays sounds, draws, or runs a level's frames (include/at/AtFpDisplay.h). Each stops with a diagnostic; replacing one with recovered code means
// deleting it here. Kept apart so that src/at/AtFpDisplay.cpp holds only recovered code.
#include "at/AtFpDisplay.h"

void AtFatal(const char* message, int file, int line, int unused);

namespace {
void NotRecovered(const char* what) {
    AtFatal(what, 0, 0, 0);
}
}  // namespace

// What LoadMovie calls.
void AtFpDisplayManager::UnloadLevel(int) { NotRecovered("AtFpDisplayManager::UnloadLevel (0x0002C570) is not recovered"); }
AtFlashLevel* AtFpDisplayManager::FindLoadedLevel(const char*) {
    NotRecovered("AtFpDisplayManager::FindLoadedLevel (0x0002C5D0) is not recovered");
    return 0;
}
AtFlashLevel* AtFpDisplayManager::NewLevel(const char*, int) {
    NotRecovered("AtFpDisplayManager::NewLevel (0x0002CCB0) is not recovered");
    return 0;
}
void AtFpDisplayManager::AttachLevel(AtFlashLevel*, int) { NotRecovered("AtFpDisplayManager::AttachLevel (0x0002C430) is not recovered"); }

// A level's sounds.
void AtFlashSoundTable::StopAll() { NotRecovered("AtFlashSoundTable::StopAll (0x00012AA0) is not recovered"); }
void AtFlashSoundTable::Play(int) { NotRecovered("AtFlashSoundTable::Play (0x00012790) is not recovered"); }

// A node's matrix from a placement.
void AtFpCharacterNode::SetMatrix(const float*) { NotRecovered("AtFpCharacterNode::SetMatrix (0x00020EA0) is not recovered"); }

// The nodes' virtual slots.
AtFpCharacterNode::~AtFpCharacterNode() { NotRecovered("AtFpCharacterNode::~AtFpCharacterNode (0x00021080) is not recovered"); }
void AtFpCharacterNode::Init(int, int, AtFpCharacterNode*, AtFlashScriptTable*, int, int) { NotRecovered("AtFpCharacterNode::Init (0x00020D80) is not recovered"); }
void AtFpCharacterNode::Release() { NotRecovered("AtFpCharacterNode::Release (0x00020FD0) is not recovered"); }
void AtFpCharacterNode::Slot3() { NotRecovered("AtFpCharacterNode::Slot3 (0x00020D40) is not recovered"); }
void AtFpCharacterNode::Replace(const int*, int) { NotRecovered("AtFpCharacterNode::Replace (0x00020D50) is not recovered"); }

AtFpBaseBranchCharacterNode::~AtFpBaseBranchCharacterNode() { NotRecovered("AtFpBaseBranchCharacterNode::~AtFpBaseBranchCharacterNode (0x00021540) is not recovered"); }
void AtFpBaseBranchCharacterNode::Init(int, int, AtFpCharacterNode*, AtFlashScriptTable*, int, int) { NotRecovered("AtFpBaseBranchCharacterNode::Init (0x00030590) is not recovered"); }
void AtFpBaseBranchCharacterNode::Release() { NotRecovered("AtFpBaseBranchCharacterNode::Release (0x000210D0) is not recovered"); }
void AtFpBaseBranchCharacterNode::Slot3() { NotRecovered("AtFpBaseBranchCharacterNode::Slot3 (0x00021470) is not recovered"); }

AtFpSpriteCharacterNode::~AtFpSpriteCharacterNode() { NotRecovered("AtFpSpriteCharacterNode::~AtFpSpriteCharacterNode (0x000152F0) is not recovered"); }
void AtFpSpriteCharacterNode::Init(int, int, AtFpCharacterNode*, AtFlashScriptTable*, int, int) { NotRecovered("AtFpSpriteCharacterNode::Init (0x000143B0) is not recovered"); }
void AtFpSpriteCharacterNode::Release() { NotRecovered("AtFpSpriteCharacterNode::Release (0x00014370) is not recovered"); }
void AtFpSpriteCharacterNode::Update() { NotRecovered("AtFpSpriteCharacterNode::Update (0x000153C0) is not recovered"); }
void AtFpSpriteCharacterNode::Slot5() { NotRecovered("AtFpSpriteCharacterNode::Slot5 (0x00014420) is not recovered"); }
void AtFpSpriteCharacterNode::Acquire() { NotRecovered("AtFpSpriteCharacterNode::Acquire (0x00014DB0) is not recovered"); }
void AtFpSpriteCharacterNode::Unacquire() { NotRecovered("AtFpSpriteCharacterNode::Unacquire (0x00014DE0) is not recovered"); }
void AtFpSpriteCharacterNode::Slot8() { NotRecovered("AtFpSpriteCharacterNode::Slot8 (0x000CBF30) is not recovered"); }
void AtFpSpriteCharacterNode::Replace(const int*, int) { NotRecovered("AtFpSpriteCharacterNode::Replace (0x00014C40) is not recovered"); }

AtFpShapeCharacterNode::~AtFpShapeCharacterNode() { NotRecovered("AtFpShapeCharacterNode::~AtFpShapeCharacterNode (0x00022280) is not recovered"); }
void AtFpShapeCharacterNode::Init(int, int, AtFpCharacterNode*, AtFlashScriptTable*, int, int) { NotRecovered("AtFpShapeCharacterNode::Init (0x00022060) is not recovered"); }
void AtFpShapeCharacterNode::Release() { NotRecovered("AtFpShapeCharacterNode::Release (0x00022000) is not recovered"); }
void AtFpShapeCharacterNode::Update() { NotRecovered("AtFpShapeCharacterNode::Update (0x001CC9F0) is not recovered"); }
void AtFpShapeCharacterNode::Slot5() { NotRecovered("AtFpShapeCharacterNode::Slot5 (0x000220D0) is not recovered"); }
void AtFpShapeCharacterNode::Acquire() { NotRecovered("AtFpShapeCharacterNode::Acquire (0x000221D0) is not recovered"); }
void AtFpShapeCharacterNode::Unacquire() { NotRecovered("AtFpShapeCharacterNode::Unacquire (0x00022200) is not recovered"); }
void AtFpShapeCharacterNode::Slot8() { NotRecovered("AtFpShapeCharacterNode::Slot8 (0x000CBF30) is not recovered"); }
void AtFpShapeCharacterNode::Replace(const int*, int) { NotRecovered("AtFpShapeCharacterNode::Replace (0x00021FB0) is not recovered"); }

AtFpTextCharacterNode::~AtFpTextCharacterNode() { NotRecovered("AtFpTextCharacterNode::~AtFpTextCharacterNode (0x00030FD0) is not recovered"); }
void AtFpTextCharacterNode::Init(int, int, AtFpCharacterNode*, AtFlashScriptTable*, int, int) { NotRecovered("AtFpTextCharacterNode::Init (0x00030590) is not recovered"); }
void AtFpTextCharacterNode::Release() { NotRecovered("AtFpTextCharacterNode::Release (0x000305D0) is not recovered"); }
void AtFpTextCharacterNode::Update() { NotRecovered("AtFpTextCharacterNode::Update (0x001CC9F0) is not recovered"); }
void AtFpTextCharacterNode::Slot5() { NotRecovered("AtFpTextCharacterNode::Slot5 (0x000307A0) is not recovered"); }
void AtFpTextCharacterNode::Acquire() { NotRecovered("AtFpTextCharacterNode::Acquire (0x00030F60) is not recovered"); }
void AtFpTextCharacterNode::Unacquire() { NotRecovered("AtFpTextCharacterNode::Unacquire (0x000306A0) is not recovered"); }
void AtFpTextCharacterNode::Slot8() { NotRecovered("AtFpTextCharacterNode::Slot8 (0x00030600) is not recovered"); }
void AtFpTextCharacterNode::Replace(const int*, int) { NotRecovered("AtFpTextCharacterNode::Replace (0x00021FB0) is not recovered"); }
