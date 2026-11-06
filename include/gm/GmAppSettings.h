// App.ini: the first file the game reads. The retail disc's has only a DataFolder, but the reader knows many more keys, for a development build's benefit:
// a starting level, screen sizes, audio switches, and skips for the menus. All names here are inferred, from the keywords.
#pragma once
#include "at/AtIoMountManager.h"

// Supplied by the platform layer. On Xbox: 0x001B7B84, reading the console's language, and 0x001B7B12, its game region.
int AtPlatformLanguage();
int AtPlatformGameRegion();

// What the settings write into the App object, not otherwise recovered. Reached through 0x001414F0, which reads a pointer at 0x003B360C.
struct GmAppAudioOptions {
    char m_unknown[0x645C];
    int m_noMusic;             // +0x645C
    int m_noAmbientAudio;      // +0x6460
    int m_noAudio;             // +0x6464
    int m_displayBanks;        // +0x6468
    int m_displaySounds;       // +0x646C
    int m_displayMusic;        // +0x6470
    int m_audioMode;           // +0x6474
    int m_audioTest;           // +0x6478

    static GmAppAudioOptions* s_instance;  // 0x003B360C
};

// A small object with a virtual setter at slot 5. Two of them, for how many trace and error lines to keep. Reached through 0x002CB0A8.
class GmHistoryLog {
public:
    virtual void Unknown0() = 0;
    virtual void Unknown1() = 0;
    virtual void Unknown2() = 0;
    virtual void Unknown3() = 0;
    virtual void Unknown4() = 0;
    virtual void SetLength(int lines) = 0;   // slot 5
};

struct GmHistoryLogs {
    void SetTraceHistory(int lines);   // 0x0006C2D0
    void SetErrorHistory(int lines);   // 0x0006C2F0

    char m_unknown[0x18];
    GmHistoryLog* m_errors;    // +0x18
    GmHistoryLog* m_traces;    // +0x1C both setters test this one

    static GmHistoryLogs* s_instance;  // 0x002CB0A8
};

// Debug output channels, as bits, one name for each. In the original a global at 0x002B93A4, of which the low word is set whole by 0x0015DA00.
void GmSetDebugChannels(unsigned short channels);  // 0x0015DA00
extern unsigned short g_debugChannels;             // 0x002B93A4

// Plain switches read elsewhere. Globals in the original.
extern int g_usePlatformBinary;       // 0x002B9348 1 until DontUsePlatformBinary
extern int g_backgroundMusic;         // 0x002B9110 1 until BgMusicDisabled
extern int g_consoleMemStats;         // 0x002B93A0 1 until DisableConsoleMemStats
extern int g_skipProfileLoad;         // 0x003ADA5C
extern int g_enableAllUnlockables;    // 0x003ADA60
extern int g_noDialogs;               // 0x003B3908
extern int g_showFlashHeapInfo;       // 0x003B3544

// At +0x654C of App, with the command line reader at 0x00098410, not recovered, beside it.
class GmAppSettings {
public:
    GmAppSettings();                         // 0x000989D0
    void SetDefaultLanguage();               // 0x00098900
    static void SetChannel(const char* name, int on);  // 0x000981C0; the original has a this it never reads
    void Load();                             // 0x00098A50 quietly nothing when the device has no App.ini

    int m_ready;               // +0x00
    char m_level[8];           // +0x04
    char m_dataFolder[8];      // +0x0C
    int m_width;               // +0x14
    int m_height;              // +0x18
    int m_fullscreen;          // +0x1C
    int m_unknown20;           // +0x20
    int m_testAllLevels;       // +0x24
    int m_openList;            // +0x28
    int m_verticalSyncOff;     // +0x2C
    float m_controllerDeadZone;  // +0x30
    int m_activeControllers[4];  // +0x34 not bounded by the reader
    char m_character[8];       // +0x44 one per player by the reader's index, which writes on over what follows
    int m_skipIntro;           // +0x4C
    int m_skipMainMenu;        // +0x50
    int m_enableDebugMenu;     // +0x54
    int m_skipLegal;           // +0x58
    int m_memStats;            // +0x5C
    char m_language[8];        // +0x60
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(GmAppAudioOptions, m_audioTest) == 0x6478, "GmAppAudioOptions::m_audioTest");
static_assert(offsetof(GmHistoryLogs, m_traces) == 0x1C, "GmHistoryLogs::m_traces");
static_assert(offsetof(GmAppSettings, m_controllerDeadZone) == 0x30, "GmAppSettings::m_controllerDeadZone");
static_assert(offsetof(GmAppSettings, m_character) == 0x44, "GmAppSettings::m_character");
static_assert(offsetof(GmAppSettings, m_language) == 0x60, "GmAppSettings::m_language");
#pragma pop_macro("offsetof")
#endif
