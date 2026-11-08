// The game's main and its one App, 0x00011030 and 0x002BF310. In a file of its own: only a program links it. See include/gm/GmApp.h.
#include "gm/GmApp.h"

App g_app;

int AppMain(unsigned int argc, char** argv, char**) {
    GmAppSettings::s_argc = argc;
    GmAppSettings::s_argv = argv;
    g_app.Init(0, 0);
    g_app.Run();
    g_app.Shutdown();
    return 1;
}
