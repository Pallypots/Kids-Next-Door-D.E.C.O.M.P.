# Mutants of the AtAp layers' start-ups and App's configs for them (gm/atap, src/gm/GmAppLayers.cpp). Each text occurs exactly once in its file.
# The objects' side is the suite atap_at (at/atap); both run tests/oracle/oracle_atap.cpp.
G = "GmAppLayers.cpp"

# (name, flag test, its mutant, the call below, the config call, the Init call, its mutant, the after call)
LAYERS = [
    ("UserInterface", "if (m_userInterfaceRunning != 0) {", "if (m_userInterfaceRunning == 0) {", "StartUpScene();", "GetUserInterfaceConfig(&config);",
     "m_userInterface->Init(&config, m_scene.m_bytes);", "m_userInterface->Init(&config, m_video.m_bytes);", "AfterUserInterface();"),
    ("Scene", "if (m_sceneRunning != 0) {", "if (m_sceneRunning == 0) {", "StartUpVideo();", "GetSceneConfig(&config);",
     "m_scene->Init(&config, m_video.m_bytes, &m_memory);", "m_scene->Init(&config, m_sound.m_bytes, &m_memory);", "AfterScene();"),
    ("Video", "if (m_videoRunning != 0) {", "if (m_videoRunning == 0) {", "StartUpSound();", "GetVideoConfig(&config);",
     "m_video->Init(&config, m_sound.m_bytes, &m_memory);", "m_video->Init(&config, m_sound.m_bytes, &m_io);", "AfterVideo();"),
    ("Sound", "if ((m_soundFlags & 1) != 0) {", "if (m_soundFlags != 0) {", "StartUpController();", "GetSoundConfig(&config);",
     "m_sound->Init(&config, m_controller.m_bytes);", "m_sound->Init(&config, m_device.m_bytes);", "AfterSound();"),
    ("Controller", "if (m_controllerRunning != 0) {", "if (m_controllerRunning == 0) {", "StartUpGraphics();", "GetControllerConfig(&config);",
     "m_controller->Init(&config, m_device.m_bytes, &m_memory);", "m_controller->Init(&config, &m_kinematics, &m_memory);", "AfterController();"),
    ("Graphics", "if ((m_device->m_flags & AppDevice::kRunning) != 0) {", "if (m_device->m_flags != 0) {", "StartUpKinematics();", "GetGraphicsConfig(&config);",
     "m_device->Init(&config, &m_kinematics, &m_memory);", "m_device->Init(&config, &m_kinematics, &m_events);", "AfterGraphics();"),
    ("Kinematics", "if (m_kinematics.m_below != 0) {", "if (m_kinematics.m_below == 0) {", "StartUpScript();", "GetKinematicsConfig(&config);",
     "m_kinematics.Init(&config, &m_script, &m_memory);", "m_kinematics.Init(&config, &m_dataManagement, &m_memory);", "AfterKinematics();"),
    ("Script", "if (m_script.m_running != 0) {", "if (m_script.m_running == 0) {", "StartUpDataManagement();", "GetScriptConfig(&config);",
     "m_script.Init(&config, &m_dataManagement, &m_memory);", "m_script.Init(&config, &m_math, &m_memory);", "AfterScript();"),
    ("DataManagement", "if (m_dataManagement.m_below != 0) {", "if (m_dataManagement.m_below == 0) {", "StartUpMath();", "GetDataManagementConfig(&config);",
     "m_dataManagement.Init(&config, &m_math, &m_memory);", "m_dataManagement.Init(&config, &m_math, &m_math);", "AfterDataManagement();"),
    ("Math", "if (m_math.m_below != 0) {", "if (m_math.m_below == 0) {", "StartUpIo();", "GetMathConfig(&config);",
     "m_math.Init(&config, &m_io, &m_memory);", "m_math.Init(&config, &m_network, &m_memory);", "AfterMath();"),
    ("Io", "if (m_io.m_below != 0) {", "if (m_io.m_below == 0) {", "StartUpNetwork();", "GetIoConfig(&config);",
     "m_io.Init(&config, &m_network, &m_memory);", "m_io.Init(&config, &m_network, 0);", "AfterIo();"),
    ("Network", "if (m_network.m_below != 0) {", "if (m_network.m_below == 0) {", "StartUpEvent();", "GetNetworkConfig(&config);",
     "m_network.Init(&config, &m_events);", "m_network.Init(&config, &m_memory);", "AfterNetwork();"),
    ("Event", "if (m_events.m_below != 0) {", "if (m_events.m_below == 0) {", "StartUpBase();", "GetConfig07(&config);",
     "m_events.Init(&config, &m_memory);", "m_events.Init(&config, 0);", "AfterEvent();"),
]

CHAIN = []
for name, flag, flag_m, below, cfg, init, init_m, after in LAYERS:
    CHAIN += [
        ("%s: running" % name, G, flag, flag_m),
        ("%s: the layer below" % name, G, "    %s\n" % below, ""),
        ("%s: config" % name, G, "    %s\n" % cfg, ""),
        ("%s: Init's arguments" % name, G, init, init_m),
        ("%s: no Init" % name, G, "    %s\n" % init, ""),
        ("%s: after" % name, G, "    %s\n" % after, ""),
    ]
CHAIN += [
    ("Base: running", G, "if (m_memory.m_started != 0) {", "if (m_memory.m_started == 0) {"),
    ("Base: memory params", G, "    GetMemoryParams(&params);\n", ""),
    ("Base: startup", G, "    m_memory.Startup(&params);\n", ""),
    ("Base: heaps", G, "    CreateHeaps();\n", ""),
    ("Base: shutdown", G, "    if (m_memory.m_started != 0) {\n        Shutdown();\n    }\n", ""),
    ("configs not cleared", G, "    *config = T();\n", ""),
]

CONFIGS = [
    ("Io: kind", G, "config->m_kind = 1;", "config->m_kind = 2;"),
    ("Io: stack", G, "config->m_stackSize = 6;", "config->m_stackSize = 5;"),
    ("Io: buffer", G, "config->m_bufferBytes = 0x40000;", "config->m_bufferBytes = 0x40001;"),
    ("Io: spaces", G, "config->m_spaces = 0x10;", "config->m_spaces = 0x11;"),
    ("Io: requests", G, "config->m_requests = 0x10;", "config->m_requests = 0x11;"),
    ("Io: heap", G, "    config->m_heap = m_heaps[0];\n}\n\nvoid App::GetMathConfig", "    config->m_heap = m_heaps[1];\n}\n\nvoid App::GetMathConfig"),
    ("Io: clears", G, "void App::GetIoConfig(AtIoConfig* config) {\n", "void App::GetIoConfig(AtIoConfig* config) {\n    Clear(config);\n"),
    ("Math: +0", G, "void App::GetMathConfig(AppMathConfig* config) {\n    config->m_unknown00 = 0;", "void App::GetMathConfig(AppMathConfig* config) {\n    config->m_unknown00 = 1;"),
    ("Math: +0C", G, "    config->m_queryHeap = 0;\n    config->m_heap = m_heaps[0];\n}\n\nvoid App::GetDataManagementConfig", "    config->m_heap = m_heaps[0];\n}\n\nvoid App::GetDataManagementConfig"),
    ("Math: heap", G, "    config->m_heap = m_heaps[0];\n}\n\nvoid App::GetDataManagementConfig", "    config->m_heap = m_heaps[2];\n}\n\nvoid App::GetDataManagementConfig"),
    ("DataManagement: clear", G, "    Clear(config);\n    config->m_databaseSpaces = 0x80;", "    config->m_databaseSpaces = 0x80;"),
    ("DataManagement: +0", G, "config->m_databaseSpaces = 0x80;", "config->m_databaseSpaces = 0x81;"),
    ("DataManagement: +4", G, "config->m_registrySpaces = 0x80;", "config->m_registrySpaces = 0x81;"),
    ("DataManagement: heap", G, "    config->m_heap = m_heaps[0];\n}\n\nvoid App::GetScriptConfig", "    config->m_heap = m_heaps[3];\n}\n\nvoid App::GetScriptConfig"),
    ("Script: +0x10", G, "    config->m_systemHeap = 0;\n", ""),
    ("Script: +0", G, "void App::GetScriptConfig(AppScriptConfig* config) {\n    config->m_unknown00 = 0;", "void App::GetScriptConfig(AppScriptConfig* config) {\n    config->m_unknown00 = 1;"),
    ("Script: heap", G, "    config->m_heap = m_heaps[0];\n}\n\nvoid App::GetKinematicsConfig", "    config->m_heap = m_heaps[1];\n}\n\nvoid App::GetKinematicsConfig"),
    ("Kinematics: clear", G, "void App::GetKinematicsConfig(AppKinematicsConfig* config) {\n    Clear(config);\n", "void App::GetKinematicsConfig(AppKinematicsConfig* config) {\n"),
    ("Kinematics: heap", G, "    config->m_heap = m_heaps[0];\n}\n\nvoid App::GetGraphicsConfig", "    config->m_heap = m_heaps[1];\n}\n\nvoid App::GetGraphicsConfig"),
    ("Graphics: clear", G, "void App::GetGraphicsConfig(AppGraphicsConfig* config) {\n    Clear(config);\n", "void App::GetGraphicsConfig(AppGraphicsConfig* config) {\n"),
    ("Graphics: +0", G, "    config->m_unknown00 = 1;\n    config->m_heap = m_heaps[0];\n}\n\nvoid App::GetControllerConfig", "    config->m_heap = m_heaps[0];\n}\n\nvoid App::GetControllerConfig"),
    ("Graphics: heap", G, "    config->m_heap = m_heaps[0];\n}\n\nvoid App::GetControllerConfig", "    config->m_heap = 0;\n}\n\nvoid App::GetControllerConfig"),
    ("Controller: clear", G, "void App::GetControllerConfig(AppControllerConfig* config) {\n    Clear(config);\n", "void App::GetControllerConfig(AppControllerConfig* config) {\n"),
    ("Controller: +0", G, "    config->m_unknown00 = 1;\n    config->m_heap = m_heaps[0];\n}\n\nvoid App::GetSoundConfig", "    config->m_unknown00 = 2;\n    config->m_heap = m_heaps[0];\n}\n\nvoid App::GetSoundConfig"),
    ("Controller: heap", G, "    config->m_heap = m_heaps[0];\n}\n\nvoid App::GetSoundConfig", "    config->m_heap = m_heaps[1];\n}\n\nvoid App::GetSoundConfig"),
    ("Sound: clear", G, "void App::GetSoundConfig(AppSoundConfig* config) {\n    Clear(config);\n", "void App::GetSoundConfig(AppSoundConfig* config) {\n"),
    ("Sound: 100", G, "config->m_unknown2C = 100;", "config->m_unknown2C = 101;"),
    ("Sound: heap", G, "config->m_heap = m_heaps[1];", "config->m_heap = m_heaps[0];"),
    ("Scene: clear", G, "void App::GetSceneConfig(AppSceneConfig* config) {\n    Clear(config);\n", "void App::GetSceneConfig(AppSceneConfig* config) {\n"),
    ("Scene: heap", G, "void App::GetSceneConfig(AppSceneConfig* config) {\n    Clear(config);\n    config->m_heap = m_heaps[0];", "void App::GetSceneConfig(AppSceneConfig* config) {\n    Clear(config);\n    config->m_heap = m_heaps[3];"),
]

MUTANTS = CHAIN + CONFIGS

SUITE = {"target": "oracle_atap", "subsystem": "gm/atap", "src": "gm", "var": "KND_GM_SRC", "cases": "none", "mutants": MUTANTS}
