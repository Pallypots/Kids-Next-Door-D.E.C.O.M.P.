// The AtAp layers' start-ups below the atlas layer, and the slots by which App fills in their configs. Subsystem gm/atap; see include/gm/GmApp.h and
// include/at/AtApLayer.h. Original clusters 0x0009C3C0-0x0009D533 and 0x0008C650-0x0008C7EE. Names are inferred.
//
// The chain runs from the top: each start-up first runs the one below it, so the layers come up from the base: memory and App's heaps, events, the network
// (always off), I/O, then math, data management, script, kinematics, graphics, controller, sound, video, scene and the user interface.
#include "gm/GmApp.h"

namespace {
// Start-up clears each config on the stack before App fills it in.
template <class T> void Clear(T* config) {
    *config = T();
}
}

// ---- the start-ups, from the top ---------------------------------------------------------------------------------------

void App::StartUpUserInterface() {
    AppUserInterfaceConfig config;
    Clear(&config);
    if (m_userInterfaceRunning != 0) {
        Shutdown();
    }
    StartUpScene();
    GetUserInterfaceConfig(&config);
    m_userInterface->Init(&config, m_scene.m_bytes);
    AfterUserInterface();
}

void App::StartUpScene() {
    AppSceneConfig config;
    Clear(&config);
    if (m_sceneRunning != 0) {
        Shutdown();
    }
    StartUpVideo();
    GetSceneConfig(&config);
    m_scene->Init(&config, m_video.m_bytes, &m_memory);
    AfterScene();
}

void App::StartUpVideo() {
    AppVideoConfig config;
    Clear(&config);
    if (m_videoRunning != 0) {
        Shutdown();
    }
    StartUpSound();
    GetVideoConfig(&config);
    m_video->Init(&config, m_sound.m_bytes, &m_memory);
    AfterVideo();
}

void App::StartUpSound() {
    AppSoundConfig config;
    Clear(&config);
    if ((m_soundFlags & 1) != 0) {
        Shutdown();
    }
    StartUpController();
    GetSoundConfig(&config);
    m_sound->Init(&config, m_controller.m_bytes);
    AfterSound();
}

void App::StartUpController() {
    AppControllerConfig config;
    Clear(&config);
    if (m_controllerRunning != 0) {
        Shutdown();
    }
    StartUpGraphics();
    GetControllerConfig(&config);
    m_controller->Init(&config, m_device.m_bytes, &m_memory);
    AfterController();
}

void App::StartUpGraphics() {
    AppGraphicsConfig config;
    Clear(&config);  // ORIGINAL: cleared twice, 0xD4 bytes and then all 0xF0
    if ((m_device->m_flags & AppDevice::kRunning) != 0) {
        Shutdown();
    }
    StartUpKinematics();
    GetGraphicsConfig(&config);
    m_device->Init(&config, &m_kinematics, &m_memory);
    AfterGraphics();
}

void App::StartUpKinematics() {
    AppKinematicsConfig config;
    Clear(&config);
    if (m_kinematics.m_below != 0) {
        Shutdown();
    }
    StartUpScript();
    GetKinematicsConfig(&config);
    m_kinematics.Init(&config, &m_script, &m_memory);
    AfterKinematics();
}

void App::StartUpScript() {
    AppScriptConfig config;
    Clear(&config);
    if (m_script.m_running != 0) {
        Shutdown();
    }
    StartUpDataManagement();
    GetScriptConfig(&config);
    m_script.Init(&config, &m_dataManagement, &m_memory);
    AfterScript();
}

void App::StartUpDataManagement() {
    AppDataManagementConfig config;
    Clear(&config);
    if (m_dataManagement.m_below != 0) {
        Shutdown();
    }
    StartUpMath();
    GetDataManagementConfig(&config);
    m_dataManagement.Init(&config, &m_math, &m_memory);
    AfterDataManagement();
}

void App::StartUpMath() {
    AppMathConfig config;
    Clear(&config);
    if (m_math.m_below != 0) {
        Shutdown();
    }
    StartUpIo();
    GetMathConfig(&config);
    m_math.Init(&config, &m_io, &m_memory);
    AfterMath();
}

void App::StartUpIo() {
    AtIoConfig config;
    Clear(&config);
    if (m_io.m_below != 0) {
        Shutdown();
    }
    StartUpNetwork();
    GetIoConfig(&config);
    m_io.Init(&config, &m_network, &m_memory);
    AfterIo();
}

void App::StartUpNetwork() {
    AtNetworkConfig config;
    Clear(&config);
    if (m_network.m_below != 0) {
        Shutdown();
    }
    StartUpEvent();
    GetNetworkConfig(&config);
    m_network.Init(&config, &m_events);
    AfterNetwork();
}

void App::StartUpEvent() {
    AppLayerConfig07 config;
    Clear(&config);
    if (m_events.m_below != 0) {
        Shutdown();
    }
    StartUpBase();
    GetConfig07(&config);
    m_events.Init(&config, &m_memory);
    AfterEvent();
}

void App::StartUpBase() {
    AtMemoryParams params;
    Clear(&params);
    if (m_memory.m_started != 0) {
        Shutdown();
    }
    GetMemoryParams(&params);
    m_memory.Startup(&params);
    CreateHeaps();
}

// ---- App's configs for them ---------------------------------------------------------------------------------------------

// ORIGINAL: the only one of these that does not clear the config first; start-up has.
void App::GetIoConfig(AtIoConfig* config) {
    config->m_kind = 1;
    config->m_stackSize = 6;
    config->m_bufferBytes = 0x40000;
    config->m_spaces = 0x10;
    config->m_requests = 0x10;
    config->m_heap = m_heaps[0];
}

void App::GetMathConfig(AppMathConfig* config) {
    config->m_unknown00 = 0;
    config->m_heap = 0;
    config->m_segmentHeap = 0;
    config->m_queryHeap = 0;
    config->m_heap = m_heaps[0];
}

void App::GetDataManagementConfig(AppDataManagementConfig* config) {
    Clear(config);
    config->m_databaseSpaces = 0x80;
    config->m_registrySpaces = 0x80;
    config->m_heap = m_heaps[0];
}

void App::GetScriptConfig(AppScriptConfig* config) {
    config->m_unknown00 = 0;
    config->m_heap = 0;
    config->m_hostHeap = 0;
    config->m_factoryHeap = 0;
    config->m_systemHeap = 0;
    config->m_heap = m_heaps[0];
}

void App::GetKinematicsConfig(AppKinematicsConfig* config) {
    Clear(config);
    config->m_heap = m_heaps[0];
}

void App::GetGraphicsConfig(AppGraphicsConfig* config) {
    Clear(config);
    config->m_unknown00 = 1;
    config->m_heap = m_heaps[0];
}

void App::GetControllerConfig(AppControllerConfig* config) {
    Clear(config);
    config->m_unknown00 = 1;
    config->m_heap = m_heaps[0];
}

void App::GetSoundConfig(AppSoundConfig* config) {
    Clear(config);
    config->m_unknown2C = 100;
    config->m_heap = m_heaps[1];
}

void App::GetSceneConfig(AppSceneConfig* config) {
    Clear(config);
    config->m_heap = m_heaps[0];
}
