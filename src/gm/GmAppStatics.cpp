// Two statics read by several subsystems, in a file of their own so that a program of one of them (the settings, the sound manager, the level manifest) does not pull in the others, nor App.
#include "gm/GmApp.h"
#include "gm/GmAppSettings.h"

GmHistoryLogs* GmHistoryLogs::s_instance;  // 0x002CB0A8
App* App::s_gmApp;                        // 0x003B360C
int g_usePlatformBinary = 1;              // 0x002B9348 the settings clear it (DontUsePlatformBinary); the level manifest's graphics items read it
