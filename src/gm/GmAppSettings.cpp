// GmAppSettings: the reader of App.ini. Original address clusters 0x000981C0-0x000983FE, 0x00098900-0x00098F91, and 0x0006C2D0, 0x0006C2F0, 0x0015DA00.
// Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "gm/GmAppSettings.h"
#include "at/AtText.h"
#include "gm/GmLevelManager.h"

extern "C" {
decltype(sizeof(0)) strlen(const char* s);
char* strncpy(char* to, const char* from, decltype(sizeof(0)) n);
int toupper(int c);
}

GmAppAudioOptions* GmAppAudioOptions::s_instance;
GmHistoryLogs* GmHistoryLogs::s_instance;
unsigned int GmAppSettings::s_argc;
char** GmAppSettings::s_argv;
unsigned int GmAppSettings::s_levelUppercaseCount;
unsigned short g_debugChannels = 3;
int g_usePlatformBinary = 1;
int g_backgroundMusic = 1;
int g_consoleMemStats = 1;
int g_skipProfileLoad;
int g_enableAllUnlockables;
int g_noDialogs;
int g_showFlashHeapInfo;

namespace {

enum SettingToken {
    kDataFolder = 57, kLevel = 58, kResolution = 59, kFullscreen = 60, kMaximized = 61, kNtsc = 62, kPal = 63, kPs2 = 64, kXb = 66, kXb720 = 67, kXb1080 = 68,
    kTestAllLevels = 69, kOpenList = 70, kAddDio = 71, kNoDio = 72, kDontUsePlatformBinary = 73, kBgMusicDisabled = 74, kControllerDeadZone = 75, kTraceHistory = 76,
    kErrorHistory = 77, kVerticalSyncOff = 78, kLanguage = 79, kPage = 80, kActivateController = 81, kSetCharacter = 82, kProfile = 83, kYellow = 84, kRed = 85,
    kSkipIntro = 86, kDisableConsoleMemStats = 87, kSkipMainMenu = 88, kEnableDebugMenu = 89, kSkipLegal = 90, kNoMusic = 91, kNoAmbientAudio = 92, kNoAudio = 93,
    kDisplayBanks = 94, kDisplaySounds = 95, kDisplayMusic = 96, kAudioMode = 97, kAudioTest = 98, kNoDialogs = 99, kShowFlashHeapInfo = 100, kSkipProfileLoadCreate = 101,
    kEnableAllUnlockables = 102
};

// 0x002B44B8. Two names share an id; both mean a 720 by 480 screen, and PS2Screen means the same as NTSCScreen.
const AtIoKeyword kKeywords[] = {
    {kDataFolder, "DataFolder"}, {kLevel, "Level"}, {kResolution, "Resolution"}, {kFullscreen, "Fullscreen"}, {kMaximized, "Maximized"}, {kNtsc, "NTSCScreen"}, {kPal, "PALScreen"},
    {kPs2, "PS2Screen"}, {kXb, "XBScreen"}, {kXb, "XB480pScreen"}, {kXb720, "XB720pScreen"}, {kXb1080, "XB1080iScreen"}, {kTestAllLevels, "TestAllLevels"}, {kOpenList, "OpenList"},
    {kControllerDeadZone, "ControllerDeadZone"}, {kTraceHistory, "TraceHistory"}, {kErrorHistory, "ErrorHistory"}, {kVerticalSyncOff, "VerticalSyncOff"}, {kAddDio, "add_dio"}, {kNoDio, "no_dio"},
    {kDontUsePlatformBinary, "DontUsePlatformBinary"}, {kBgMusicDisabled, "BgMusicDisabled"}, {kDisableConsoleMemStats, "DisableConsoleMemStats"}, {kLanguage, "Language"}, {kPage, "Page"},
    {kActivateController, "ActivateController"}, {kSetCharacter, "SetCharacter"}, {kProfile, "Profile"}, {kYellow, "Yellow"}, {kRed, "Red"}, {kSkipIntro, "SkipIntro"}, {kNoMusic, "NoMusic"},
    {kNoAmbientAudio, "NoAmbientAudio"}, {kNoAudio, "NoAudio"}, {kDisplayBanks, "DisplayBanks"}, {kDisplaySounds, "DisplaySounds"}, {kDisplayMusic, "DisplayMusic"}, {kAudioMode, "AudioMode"},
    {kAudioTest, "AudioTest"}, {kSkipMainMenu, "SkipMainMenu"}, {kEnableDebugMenu, "EnableDebugMenu"}, {kSkipLegal, "SkipLegal"}, {kNoDialogs, "NoDialogs"}, {kShowFlashHeapInfo, "ShowFlashHeapInfo"},
    {kSkipProfileLoadCreate, "SkipProfileLoadCreate"}, {kEnableAllUnlockables, "EnableAllUnlockables"},
};

struct Channel {
    const char* m_name;
    unsigned short m_bit;
};

const Channel kChannels[] = {
    {"framework", 0x0004}, {"ai", 0x0008}, {"audio", 0x0010}, {"io", 0x0020}, {"render", 0x0040}, {"sim", 0x0080}, {"ui", 0x0100}, {"controller", 0x0200}, {"tool", 0x0400},
    {"debugoutput", 0x0800}, {"event", 0x1000}, {"innerloop", 0x8000},
};

// The C library's _stricmp, here so as to need no header.
int EqualIgnoringCase(const char* a, const char* b) {
    for (;; a++, b++) {
        const int x = toupper((unsigned char)*a);
        if (x != toupper((unsigned char)*b)) {
            return 0;
        }
        if (x == 0) {
            return 1;
        }
    }
}

void SetSize(GmAppSettings* s, int width, int height) {
    s->m_width = width;
    s->m_height = height;
    s->m_fullscreen = 0;
}

}  // namespace

void GmSetDebugChannels(unsigned short channels) {
    g_debugChannels = channels;
}

void GmHistoryLogs::SetTraceHistory(int lines) {
    if (m_traces != 0) {
        m_traces->SetLength(lines);
    }
}

void GmHistoryLogs::SetErrorHistory(int lines) {
    if (m_traces != 0) {
        m_errors->SetLength(lines);
    }
}

void GmAppSettings::SetChannel(const char* name, int on) {
    if (name == 0) {
        return;
    }
    for (unsigned int i = 0; i < sizeof kChannels / sizeof kChannels[0]; i++) {
        if (EqualIgnoringCase(name, kChannels[i].m_name)) {
            if (on == 1) {
                g_debugChannels |= kChannels[i].m_bit;
            } else {
                g_debugChannels &= (unsigned short)~kChannels[i].m_bit;
            }
            return;
        }
    }
}

void GmAppSettings::SetDefaultLanguage() {
    char language[8];
    switch (AtPlatformLanguage()) {
    case 3: AtCopyName("GERMAN", language); break;
    case 4: AtCopyName("FRENCH", language); break;
    case 6: AtCopyName("ITALIAN", language); break;
    default: AtCopyName("ENGLISH", language); break;
    }
    if (AtPlatformGameRegion() == 1) {
        AtCopyName8(language, "ENGLISH\0");
    }
    AtCopyName8(m_language, language);
}

GmAppSettings::GmAppSettings() {
    GmSetDebugChannels(3);
    AtCopyName8(m_level, "ALMOND\0\0");
    AtCopyName8(m_dataFolder, "\0\0\0\0\0\0\0");
    SetDefaultLanguage();
    m_width = 640;
    m_height = 480;
    m_fullscreen = 0;
    m_unknown20 = 0;
    m_controllerDeadZone = 25.0f;
    AtCopyName8(m_character, "\0\0\0\0\0\0\0");
    m_skipIntro = 0;
    m_skipMainMenu = 0;
    m_enableDebugMenu = 0;
    m_skipLegal = 0;
    m_ready = 1;
}

void GmAppSettings::Load() {
    AtIoTokenFile file;
    if (AtIoMountManager::s_instance->m_device->IsFileMissing("App.ini") != 0) {
        return;
    }
    const int status = file.Open("App", "INI", 2, 0);
    if (status != 0) {
        AtReportFileErrorText(status, "App.ini");
    }
    AtIoTokenizer* t = file.m_tokenizer;
    t->SetKeywords(kKeywords, sizeof kKeywords / sizeof kKeywords[0], 0);
    t->ExpectLeftCurly();
    for (int token = t->Next(); token != AtIoTokenizer::kRightCurly; token = t->Next()) {
        switch (token) {
        case kDataFolder:
            t->ExpectName(m_dataFolder);
            break;
        case kLevel: {
            char text[0x80];
            strncpy(text, t->ExpectString(), 0x7F);
            text[0x7F] = 0;  // The original does not terminate it; what it then measures is longer than eight bytes either way.
            if (strlen(text) <= 8) {
                AtCopyName(text, m_level);
            }
            break;
        }
        case kResolution:
            m_width = t->ExpectInt();
            m_height = t->ExpectInt();
            break;
        case kFullscreen:
            m_fullscreen = 1;
            break;
        case kNtsc:
        case kPs2:
            SetSize(this, 640, 448);
            break;
        case kPal:
            SetSize(this, 512, 512);
            break;
        case kXb:
            SetSize(this, 720, 480);
            break;
        case kXb720:
            SetSize(this, 1280, 720);
            break;
        case kXb1080:
            SetSize(this, 1920, 1080);
            break;
        case kTestAllLevels:
            m_testAllLevels = 1;
            break;
        case kOpenList:
            m_openList = 1;
            break;
        case kControllerDeadZone: {
            const int value = t->ExpectInt();
            if (value >= 0 && value < 100) {
                m_controllerDeadZone = (float)value;
            }
            break;
        }
        case kTraceHistory: {
            const int value = t->ExpectInt();
            if (value >= 0 && value < 1000) {
                GmHistoryLogs::s_instance->SetTraceHistory(value);
            }
            break;
        }
        case kErrorHistory: {
            const int value = t->ExpectInt();
            if (value >= 0 && value < 1000) {
                GmHistoryLogs::s_instance->SetErrorHistory(value);
            }
            break;
        }
        case kVerticalSyncOff:
            m_verticalSyncOff = 1;
            break;
        case kAddDio:
            SetChannel(t->ExpectString(), 1);
            break;
        case kNoDio:
            SetChannel(t->ExpectString(), 0);
            break;
        case kDontUsePlatformBinary:
            g_usePlatformBinary = 0;
            break;
        case kBgMusicDisabled:
            g_backgroundMusic = 0;
            break;
        case kLanguage: {
            char name[8];
            t->ExpectName(name);
            AtCopyName8(m_language, name);
            break;
        }
        case kActivateController:
            m_activeControllers[t->ExpectInt()] = 1;
            break;
        case kSetCharacter: {
            const int index = t->ExpectInt();
            char name[8];
            t->ExpectName(name);
            AtCopyName8(m_character + index * 8, name);
            break;
        }
        case kSkipIntro:
            m_skipIntro = 1;
            break;
        case kDisableConsoleMemStats:
            m_memStats = 0;
            g_consoleMemStats = 0;
            break;
        case kSkipMainMenu:
            m_skipMainMenu = 1;
            g_skipProfileLoad = 1;
            break;
        case kSkipProfileLoadCreate:
            g_skipProfileLoad = 1;
            break;
        case kEnableDebugMenu:
            m_enableDebugMenu = 1;
            break;
        case kSkipLegal:
            m_skipLegal = 1;
            break;
        case kNoMusic:
            GmAppAudioOptions::s_instance->m_noMusic = 1;
            break;
        case kNoAmbientAudio:
            GmAppAudioOptions::s_instance->m_noAmbientAudio = 1;
            break;
        case kNoAudio:
            GmAppAudioOptions::s_instance->m_noAudio = 1;
            break;
        case kDisplayBanks:
            GmAppAudioOptions::s_instance->m_displayBanks = 1;
            break;
        case kDisplaySounds:
            GmAppAudioOptions::s_instance->m_displaySounds = 1;
            break;
        case kDisplayMusic:
            GmAppAudioOptions::s_instance->m_displayMusic = 1;
            break;
        case kAudioMode: {
            const unsigned int value = (unsigned int)t->ExpectInt();
            if (value < 3) {
                GmAppAudioOptions::s_instance->m_audioMode = (int)value;
            }
            break;
        }
        case kAudioTest:
            GmAppAudioOptions::s_instance->m_audioTest = 1;
            break;
        case kNoDialogs:
            g_noDialogs = 1;
            break;
        case kShowFlashHeapInfo:
            g_showFlashHeapInfo = 1;
            break;
        case kEnableAllUnlockables:
            g_enableAllUnlockables = 1;
            break;
        default:
            break;
        }
    }
    file.Close();
}

void GmAppSettings::ReadCommandLine() {
    for (unsigned int i = 1; i < s_argc; i++) {
        char* arg = s_argv[i];
        if (EqualIgnoringCase(arg, "level")) {
            arg = s_argv[++i];
            for (unsigned int k = 0; k < s_levelUppercaseCount; k++) {
                if (arg[k] >= 'a' && arg[k] <= 'z') {
                    arg[k] -= 0x20;
                }
            }
            // Only a name beginning with a capital L counts.
            if (strlen(arg) <= 8 && arg[0] == 'L') {
                AtCopyName(arg, m_level);
            }
        } else if (EqualIgnoringCase(arg, "fullscreen")) {
            m_fullscreen = 1;
        } else if (EqualIgnoringCase(arg, "testalllevels")) {
            m_testAllLevels = 1;
        } else if (EqualIgnoringCase(arg, "openlist")) {
            m_openList = 1;
        } else if (EqualIgnoringCase(arg, "maximized")) {
            // Recognised, and nothing happens.
        } else if (EqualIgnoringCase(arg, "controllerdeadzone")) {
            const int value = AtNarrowToInt(s_argv[++i]);
            if (value >= 0 && value < 100) {
                m_controllerDeadZone = (float)value;
            }
        } else if (EqualIgnoringCase(arg, "verticalsyncoff")) {
            m_verticalSyncOff = 1;
        } else if (EqualIgnoringCase(arg, "add_dio")) {
            SetChannel(s_argv[++i], 1);
        } else if (EqualIgnoringCase(arg, "no_dio")) {
            SetChannel(s_argv[++i], 0);
        } else if (EqualIgnoringCase(arg, "page")) {
            // Recognised, and nothing happens.
        } else if (EqualIgnoringCase(arg, "skipintro")) {
            m_skipIntro = 1;
        } else if (EqualIgnoringCase(arg, "disableconsolememstats")) {
            m_memStats = 0;
            g_consoleMemStats = 0;
        } else if (EqualIgnoringCase(arg, "nomusic")) {
            GmAppAudioOptions::s_instance->m_noMusic = 1;
        } else if (EqualIgnoringCase(arg, "noambientaudio")) {
            GmAppAudioOptions::s_instance->m_noAmbientAudio = 1;
        } else if (EqualIgnoringCase(arg, "noaudio")) {
            GmAppAudioOptions::s_instance->m_noAudio = 1;
        } else if (EqualIgnoringCase(arg, "displaybanks")) {
            GmAppAudioOptions::s_instance->m_displayBanks = 1;
        } else if (EqualIgnoringCase(arg, "displaysounds")) {
            GmAppAudioOptions::s_instance->m_displaySounds = 1;
        } else if (EqualIgnoringCase(arg, "displaymusic")) {
            GmAppAudioOptions::s_instance->m_displayMusic = 1;
        } else if (EqualIgnoringCase(arg, "audiomode")) {
            const int value = AtNarrowToInt(s_argv[++i]);
            if (value >= 0 && value < 3) {
                GmAppAudioOptions::s_instance->m_audioMode = value;
            }
        } else if (EqualIgnoringCase(arg, "audiotest")) {
            GmAppAudioOptions::s_instance->m_audioTest = 1;
        } else if (EqualIgnoringCase(arg, "skipmainmenu")) {
            m_skipMainMenu = 1;
        }
    }
}
