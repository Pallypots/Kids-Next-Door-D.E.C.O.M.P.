// What App's start-up and main loop reach and is not recovered. Each stops with a diagnostic; none is recorded or counted. In a file of its own, so that a test can
// supply them instead. Recovered code is in src/gm/GmApp.cpp and src/gm/GmMain.cpp.
#include "gm/GmApp.h"

namespace {
void StopHere(const char* what) {
    AtFatal(what, 0, 0, 0);
}
}

// The SDK's memory query is in src/gm/GmAppSdk.cpp, apart, so that a port's answer (platform/host_sdk.cpp) can replace it.

// App's slots and members that are not recovered.
void App::DeletingDestructor(unsigned int) { StopHere("App: the deleting destructor (0x0008F4D0) is not recovered"); }
void App::Shutdown() { StopHere("App: Shutdown (0x0008ED60) is not recovered"); }
void App::Present(int) { StopHere("App: slot 39 (0x0009CA90), graphics, is not recovered"); }
void App::GetUserInterfaceConfig(AppUserInterfaceConfig*) { StopHere("App start-up: the user interface layer's config (slot 56, 0x0008DB30) is not recovered"); }
void App::Slot65() { StopHere("App: slot 65 (0x0010F4B0) is not recovered"); }
void App::Slot66() { StopHere("App: slot 66 (0x0008E1F0) is not recovered"); }
void App::SetDisplayMode() { StopHere("App start-up: the display mode (0x0008DBA0), graphics, is not recovered"); }
void App::ReadMatrixDevice() { StopHere("App start-up: the matrix device (0x001416A0), graphics, is not recovered"); }
void App::CreateSubsystems() { StopHere("App start-up: the subsystems (0x0008CFB0) are not recovered"); }

// The parts of App with classes of their own.
void AppDevice::Slot0() { StopHere("App device: slot 0 is not recovered"); }
void AppDevice::Init(const void*, void*, void*) { StopHere("App start-up: the graphics layer's Init (0x000B3000) is not recovered"); }
void AppDevice::Slot2() { StopHere("App device: slot 2 is not recovered"); }
void AppDevice::Slot3() { StopHere("App device: slot 3 is not recovered"); }
void AppDevice::Slot4() { StopHere("App device: slot 4 is not recovered"); }
void AppDevice::Slot5() { StopHere("App device: slot 5 is not recovered"); }
int AppDevice::RunFrame() { StopHere("App device: a frame (slot 6) is not recovered"); return 0; }

void AppGraphics::Slot0() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot1() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot2() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot3() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot4() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot5() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot6() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot7() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot8() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot9() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot10() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot11() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot12() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot13() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot14() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot15() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot16() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot17() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot18() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot19() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot20() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot21() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot22() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot23() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot24() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot25() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot26() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot27() { StopHere("App graphics: not recovered"); }
void AppGraphics::Slot28() { StopHere("App graphics: not recovered"); }
AtDatabaseLoader* AppGraphics::DatabaseLoader() { StopHere("App graphics: the database loader (slot 29) is not recovered"); return 0; }

// The AtAp layers' objects above the I/O layer.
void AppMathLayer::Slot0() { StopHere("App math layer: slot 0 is not recovered"); }
void AppMathLayer::Init(const void*, void*, void*) { StopHere("App start-up: the math layer's Init (0x000B38D0) is not recovered"); }
void AppDataManagementLayer::Slot0() { StopHere("App data management layer: slot 0 is not recovered"); }
void AppDataManagementLayer::Init(const void*, void*, void*) { StopHere("App start-up: the data management layer's Init (0x000B3790) is not recovered"); }
void AppScriptLayer::Slot0() { StopHere("App script layer: slot 0 is not recovered"); }
void AppScriptLayer::Init(const void*, void*, void*) { StopHere("App start-up: the script layer's Init (0x0009EE80) is not recovered"); }
void AppKinematicsLayer::Slot0() { StopHere("App kinematics layer: slot 0 is not recovered"); }
void AppKinematicsLayer::Init(const void*, void*, void*) { StopHere("App start-up: the kinematics layer's Init (0x000B34D0) is not recovered"); }
void AppControllerLayer::Slot0() { StopHere("App controller layer: slot 0 is not recovered"); }
void AppControllerLayer::Init(const void*, void*, void*) { StopHere("App start-up: the controller layer's Init (0x000B2E00) is not recovered"); }
void AppSoundLayer::Slot0() { StopHere("App sound layer: slot 0 is not recovered"); }
void AppSoundLayer::Init(const void*, void*) { StopHere("App start-up: the sound layer's Init (0x000B2BE0) is not recovered"); }
void AppVideoLayer::Slot0() { StopHere("App video layer: slot 0 is not recovered"); }
void AppVideoLayer::Init(const void*, void*, void*) { StopHere("App start-up: the video layer's Init (0x000B2AA0) is not recovered"); }
void AppSceneLayer::Slot0() { StopHere("App scene layer: slot 0 is not recovered"); }
void AppSceneLayer::Init(const void*, void*, void*) { StopHere("App start-up: the scene layer's Init (0x000B2970) is not recovered"); }
void AppUserInterfaceLayer::Slot0() { StopHere("App user interface layer: slot 0 is not recovered"); }
void AppUserInterfaceLayer::Init(const void*, void*) { StopHere("App start-up: the user interface layer's Init (0x000B28B0) is not recovered"); }

void AppAtlasLayer::Slot0() { StopHere("App atlas layer: slot 0 is not recovered"); }
void AppAtlasLayer::Init(const AppAtlasConfig*, void*, void*) { StopHere("App atlas layer: Init (slot 1) is not recovered"); }
void AppViewport::SetScreenSize(int, int) { StopHere("App viewport: 0x0009EA00 is not recovered"); }

void FaAcPlayerGameStateData::Slot0() { StopHere("FaAcPlayerGameStateData: slot 0 is not recovered"); }
void FaAcPlayerGameStateData::Slot1() { StopHere("FaAcPlayerGameStateData: slot 1 (0x0009DA40) is not recovered"); }
void FaAcPlayerGameStateData::Slot2() { StopHere("FaAcPlayerGameStateData: slot 2 is not recovered"); }
void FaAcPlayerGameStateData::Slot3() { StopHere("FaAcPlayerGameStateData: slot 3 (0x0009DA20) is not recovered"); }
void FaAcPlayerGameStateData::Slot4() { StopHere("FaAcPlayerGameStateData: slot 4 is not recovered"); }
void FaAcPlayerGameStateData::SetRegistry(AtDataRegistry*) { StopHere("FaAcPlayerGameStateData: slot 5 (0x0019D010) is not recovered"); }

void GmMoFramework::Enter(App*, int) { StopHere("Frameworks: Enter (slot 1) is not recovered"); }
void GmMoFramework::Slot2() { StopHere("Frameworks: slot 2 is not recovered"); }
void GmMoFramework::Slot3() { StopHere("Frameworks: slot 3 is not recovered"); }
void GmMoFramework::Slot4() { StopHere("Frameworks: slot 4 is not recovered"); }
void GmMoFramework::Leave() { StopHere("Frameworks: Leave (slot 5) is not recovered"); }
void GmMoFramework::Slot6() { StopHere("Frameworks: slot 6 is not recovered"); }
GmMoFramework* FaMoIntroFrameworkConstruct(void*) { StopHere("FaMoIntroFramework (0x0009B6B0) is not recovered"); return 0; }
GmMoFramework* FaMoFlashMainMenuFrameworkConstruct(void*) { StopHere("FaMoFlashMainMenuFramework (0x0009B1E0) is not recovered"); return 0; }
GmMoFramework* FaMoGameFrameworkConstruct(void*) { StopHere("FaMoGameFramework (0x00099520) is not recovered"); return 0; }

AtIoMountManager* AppMountManagerConstruct(void*) { StopHere("App start-up: the derived mount manager (0x00140350) is not recovered"); return 0; }
GmHistoryLogs* GmHistoryLogsConstruct(void*) { StopHere("History logs: the constructor (0x0006C2A0) is not recovered"); return 0; }
void GmHistoryLogsInit(GmHistoryLogs*, const AppLogParams*) { StopHere("History logs: Init (0x0006C3E0) is not recovered"); }

// The script system.
GmScripts* GmScripts::s_instance;
void GmScriptModule::Slot0() { StopHere("Script modules: not recovered"); }
void GmScriptModule::Slot1() { StopHere("Script modules: not recovered"); }
void GmScriptModule::Slot2() { StopHere("Script modules: not recovered"); }
void GmScriptModule::Slot3() { StopHere("Script modules: not recovered"); }
void GmScriptModule::Slot4() { StopHere("Script modules: not recovered"); }
void GmScriptModule::Slot5() { StopHere("Script modules: not recovered"); }
void GmScriptModule::SetEnabled(int) { StopHere("Script modules: slot 6 is not recovered"); }
void GmScripts::Init(AtHeap*) { StopHere("Scripts: Init (0x0012EEA0) is not recovered"); }
GmScriptModule* GmScripts::FindModule(const char*) { StopHere("Scripts: FindModule (0x0012EFC0) is not recovered"); return 0; }
void GmRegisterAppCallbacks() { StopHere("Scripts: the application callbacks (0x0008E4E0) are not recovered"); }
void GmRegisterControlCallbacks() { StopHere("Scripts: the Control callbacks (0x0008C450) are not recovered"); }
void GmRegisterSaveDataCallbacks() { StopHere("Scripts: the SaveData callbacks (0x0008CB30) are not recovered"); }
int GmRunScripts(int) { StopHere("Scripts: a step (0x00091540) is not recovered"); return 0; }
void GmGetStringByName(char*, unsigned int, const char*) { StopHere("Game text: 0x00090950 is not recovered"); }

GmClock* GmClock::s_instance;
void GmClock::Tick(int) { StopHere("Clock: 0x00071A60 is not recovered"); }
void AppUnknown692C::Init() { StopHere("App+0x692C: 0x00090210 is not recovered"); }
void AppUnknown692C::Update(int) { StopHere("App+0x692C: 0x0006DEA0 is not recovered"); }

// The neighbours of the registry members start-up calls (those are at/datalookup's, src/at/AtDataLookup.cpp).
GmDataRoot* GmDataRoot::s_instance;
GmRegistryUser* GmRegistryUser::s_instance;
GmRegistryNames* GmRegistryNames::s_instance;
void GmRegistryNames::Add(AtDataRegistry*) { StopHere("Registry names: 0x00071F20 is not recovered"); }

// Save data, sound, and the rest.
char* g_unknown30567C;
void AppSaveDataInit(void*) { StopHere("App save data: 0x0009D9C0 is not recovered"); }
void GmAudioReadSavedVolumes(GmAudioData*) { StopHere("Sound: the saved volumes (0x00139F10) are not recovered"); }
void GmUnknown97EB0(void*) { StopHere("0x00097EB0 is not recovered"); }
void* AtIoSpaceManagerAdd(AtIoSpaceManager*, void*, unsigned int, int) { StopHere("Space manager: 0x0007D070 is not recovered"); return 0; }
void AppUnknown90580() { StopHere("0x00090580 is not recovered"); }
