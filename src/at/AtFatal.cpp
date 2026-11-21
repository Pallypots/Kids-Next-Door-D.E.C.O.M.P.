// The game's AtFatal, 0x00011070: see include/at/AtFatal.h for where it meets the port. Alone in its file, so that a program that supplies its own AtFatal (platform/fatal.cpp's
// stand-in, a test's, an oracle's double) never pulls this one in. Verification status: config/functions.d/fatal.toml.
#include "at/AtFatal.h"

#include "at/AtControllerManager.h"
#include "gm/GmApp.h"

int AtFormat(char* out, const char* format, ...);  // the C library's sprintf, 0x00125559: src/at/AtIoFormatHost.cpp

int g_atFatalScreenShown;
int g_atFatalExiting;

namespace {

const int kStopRumbleMilliseconds = 10000;  // the controllers are moved on ten seconds, so the motors hear that the effects have ended

// AtFatal's frame: the text first, then what goes to the dashboard. ORIGINAL: the text has 0x100 bytes, and a message over about 0xE0 characters runs on into the launch data,
// which has gone by then.
struct Frame {
    char text[0x100];
    AtLaunchData launch;
};

}  // namespace

void AtFatal(const char* message, int file, int line, int) {
    if (AtControllerManager::s_ready != 0) {
        AtControllerManager* manager = AtControllerManager::s_instance;
        for (unsigned int i = 0; i < manager->m_playerCount; i++) {
            manager->m_players[i].StopEffects();
        }
        if (AppUnknown692C::Instance() != 0) {
            AppUnknown692C::Instance()->Update(kStopRumbleMilliseconds);
        }
    }
    if (g_atFatalScreenShown == 0) {
        g_atFatalScreenShown = 1;
        g_app.DrawFatalScreen();  // never returns once the dirty-disc screen is loaded
    }
    Frame frame;
    for (unsigned int i = 0; i < sizeof frame.launch.m_words / sizeof frame.launch.m_words[0]; i++) {
        frame.launch.m_words[i] = 0;
    }
    frame.launch.m_words[0] = 1;
    frame.launch.m_words[2] = 5;
    AtPlatformLaunchImage(0, &frame.launch);  // on the Xbox it does not return
    const char* fileName = (const char*)(decltype(sizeof(0)))(unsigned int)file;  // what was passed as a number, back to the pointer it stands for
    AtFormat(frame.text, "%s File: %s, Line:%d", message, fileName, line);  // ORIGINAL: made and never used
    AtLibraryPrint("%s, file \"%s\" line %d\n", message, fileName, line);
    if (g_atFatalExiting == 0) {
        g_atFatalExiting = 1;
    }
    AtLibraryExit(1);
}
