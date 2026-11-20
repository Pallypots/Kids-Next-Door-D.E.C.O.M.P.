// Not recovered: what the action script machine calls out to (include/at/AtFpDisplay.h). Each stops with a diagnostic; replacing one with recovered code means deleting it here. Kept apart from the machine so
// that the machine's own file holds only recovered code.
#include "at/AtFpDisplay.h"

void AtFatal(const char* message, int file, int line, int unused);

namespace {
void NotRecovered(const char* what) {
    AtFatal(what, 0, 0, 0);
}
}  // namespace

void AtFpRfcArgs::Construct() { NotRecovered("AtFpRfcArgs::AtFpRfcArgs (0x00021620) is not recovered"); }
void AtFpRfcArgs::Destruct() { NotRecovered("AtFpRfcArgs::~AtFpRfcArgs (0x00021AD0) is not recovered"); }
void AtFpRfcArgs::Init() { NotRecovered("AtFpRfcArgs::Init (0x000216E0) is not recovered"); }
void AtFpRfcArgs::Add(const char*, AtFpActionScriptValue*) { NotRecovered("AtFpRfcArgs::Add (0x00021DD0) is not recovered"); }
void AtFpRfcArgs::Clear() { NotRecovered("AtFpRfcArgs::Clear (0x00021680) is not recovered"); }

void AtFpRfcTable::Call(const char*, AtFpRfcArgs*) { NotRecovered("AtFpRfcTable::Call (0x0002D9E0) is not recovered"); }

void AtFpDisplayMgrContextStack::SetTarget(void*) { NotRecovered("AtFpDisplayMgrContextStack::SetTarget (0x00016130) is not recovered"); }

int AtFpDisplayManager::GetCurrentFrame() {
    NotRecovered("AtFpDisplayManager::GetCurrentFrame (0x0002BF50) is not recovered");
    return 0;
}
void AtFpDisplayManager::GotoFrame(int) { NotRecovered("AtFpDisplayManager::GotoFrame (0x0002B970) is not recovered"); }
void AtFpDisplayManager::SetPlaying(int) { NotRecovered("AtFpDisplayManager::SetPlaying (0x0002BFC0) is not recovered"); }
void AtFpDisplayManager::StopSounds() { NotRecovered("AtFpDisplayManager::StopSounds (0x0002BF20) is not recovered"); }
void AtFpDisplayManager::GotoFrameByLabel(const char*) { NotRecovered("AtFpDisplayManager::GotoFrameByLabel (0x0002BF80) is not recovered"); }
void AtFpDisplayManager::GotoFrameByExpression(const char*) { NotRecovered("AtFpDisplayManager::GotoFrameByExpression (0x0002CB70) is not recovered"); }
void AtFpDisplayManager::SetTarget(const char*) { NotRecovered("AtFpDisplayManager::SetTarget (0x0002C3B0) is not recovered"); }
void AtFpDisplayManager::LoadMovie(const char*, const char*) { NotRecovered("AtFpDisplayManager::LoadMovie (0x0002CFD0) is not recovered"); }
AtFpRfcArgs* AtFpDisplayManager::ResolveTarget(const char*) {
    NotRecovered("AtFpDisplayManager::ResolveTarget (0x0002CC90) is not recovered");
    return 0;
}
int AtFpDisplayManager::GetVariable(const char*, AtFpActionScriptValue*) {
    NotRecovered("AtFpDisplayManager::GetVariable (0x0002C9C0) is not recovered");
    return 0;
}
int AtFpDisplayManager::SetVariable(const char*, AtFpActionScriptValue*) {
    NotRecovered("AtFpDisplayManager::SetVariable (0x0002CAA0) is not recovered");
    return 0;
}
void AtFpDisplayManager::GetProperty(const char*, int, AtFpActionScriptValue*) { NotRecovered("AtFpDisplayManager::GetProperty (0x0002C320) is not recovered"); }
void AtFpDisplayManager::SetProperty(const char*, int, AtFpActionScriptValue*) { NotRecovered("AtFpDisplayManager::SetProperty (0x0002C360) is not recovered"); }
int AtFpDisplayManager::ExecuteScripts(int) {
    NotRecovered("AtFpDisplayManager::ExecuteScripts(int) (0x0002C150) is not recovered");
    return 1;
}
int AtFpDisplayManager::ExecuteScripts(const char*) {
    NotRecovered("AtFpDisplayManager::ExecuteScripts(const char*) (0x0002C1C0) is not recovered");
    return 1;
}
