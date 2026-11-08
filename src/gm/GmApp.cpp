// App: start-up, the main loop, and App's own small slots. See include/gm/GmApp.h. Address cluster 0x0008C620-0x0008EED0, with 0x00141500 (GmApp's start-up),
// 0x0009C280 (the atlas layer's) and the four heap setters elsewhere. Names are inferred unless the header says otherwise.
#include "gm/GmApp.h"

#include "at/AtDatabaseLoader.h"
#include "at/AtIoTokenizer.h"
#include "gm/GmAudioData.h"
#include "gm/GmLevelManager.h"
#include "gm/GmLevelManifest.h"
#include "gm/GmManifestLoader.h"

int AtFormat(char* out, const char* format, ...);  // Stands in for the library's sprintf: src/at/AtIoFormatHost.cpp

namespace {
struct InPlace {};
}
// Runs a constructor on storage that already exists, as the original does to App's settings in Init.
inline void* operator new(decltype(sizeof(0)), void* where, InPlace) noexcept { return where; }
inline void operator delete(void*, void*, InPlace) noexcept {}

namespace {
const double kBytesToMegabytes = 1.0 / 1048576.0;  // 0x0027A910
}

App* App::s_instance;
AtHeap* App::s_heap;
unsigned int App::s_heapMemory;
unsigned int App::s_memoryTotal;
unsigned int App::s_memoryOverhead;
FaAcPlayerGameStateData* App::s_gameState;
void* GmMoFramework::s_heap;

GmNameMap g_scriptNames;
void* g_heap00301B70;
void* g_heap003B3614;
void* g_heap00305914;
GmSaveTypes GmSaveTypes::s_instance;
int g_enabled3B3514;
int g_enabled3056A4, g_enabled3056C8, g_enabled3056EC, g_enabled305710, g_enabled305734, g_enabled305758, g_enabled30577C, g_enabled3057A0, g_enabled3057E8,
    g_enabled30580C, g_enabled305830;
char g_appShortName[8];
char g_appTitle[0x40];
int g_unknown305130;
int g_unknown304D80;
int g_unknown304D54;
int g_unknown3ADA50;
int g_inScripts;

// ---- the small pieces ----------------------------------------------------------------------------------------------

void GmScriptsInitNames() {
    g_scriptNames.Init(50);
}

void GmSetHeap00083490(void* heap) {
    g_heap00301B70 = heap;
}

void GmAcPlayerGameStateDataSetHeap(void* heap) {
    g_heap003B3614 = heap;
}

void GmSetHeap000980F0(void* heap) {
    g_heap00305914 = heap;
}

void GmMoFramework::SetHeap(void* heap) {
    s_heap = heap;
}

unsigned int GmGameSaveSize() {
    return 0xF8;
}

int GmLevelManager::FindLevel(const char* name) {
    for (int i = 0; i < m_levelCount; i++) {
        char entry[8];
        AtCopyName8(entry, m_names.m_items[i].m_name);
        // Eight characters at most: equal up to a terminator, or equal in all eight with the name ending there.
        unsigned int k = 0;
        for (;;) {
            if (name[k] != entry[k]) {
                break;
            }
            if (entry[k] == 0) {
                return i;
            }
            if (++k >= 8) {
                if (name[8] == 0) {
                    return i;
                }
                break;
            }
        }
    }
    return -1;
}

// ---- App's small slots ---------------------------------------------------------------------------------------------

void App::GetMemoryParams(AtMemoryParams* params) {
    params->m_unused = 0x1000;
    params->m_defaultHeapSize = 0x100000;
    params->m_regionSize = s_heapMemory + 0x2800;
}

void App::CreateHeaps() {
    char text[0x100];
    AtFormat(text, "***Mem For Heaps: %d (%fMb)\n", s_heapMemory, s_heapMemory * kBytesToMegabytes);
    for (int i = 0; i < 4; i++) {
        if (m_heapConfig[i].m_size != 0) {
            m_heaps[i] = AtHeapManager::s_instance->CreateHeap(m_heapConfig[i].m_size, 4);
            if (m_heapConfig[i].m_partner == -2) {
                AtHeapManager::s_instance->MakeEquivalent((AtHeap*)AtIoTokenFile::s_heap, m_heaps[i]);  // the default heap
            } else if (m_heapConfig[i].m_partner != -1) {
                AtHeapManager::s_instance->MakeEquivalent(m_heaps[m_heapConfig[i].m_partner], m_heaps[i]);
            }
        } else {
            m_heaps[i] = 0;
        }
    }
}

void App::DestroyHeaps() {
    for (int i = 0; i < 4; i++) {
        if (m_heapConfig[i].m_size != 0) {
            AtHeapManager::s_instance->DestroyHeap(m_heaps[i]);
        }
    }
}

void App::GetConfig07(AppLayerConfig07* config) {
    config->m_unknown00 = 0;
    config->m_unknown04 = 0;
    config->m_heap = 0;
    for (int i = 0; i < 9; i++) {
        config->m_unknown0C[i] = 0;
    }
    config->m_heap = m_heaps[0];
}

void App::GetAtlasConfig(AppAtlasConfig* config) {
    config->m_unknown00 = 0;
    config->m_heap = 0;
    for (int i = 0; i < 10; i++) {
        config->m_unknown08[i] = 0;
    }
    config->m_heap = m_heaps[0];
}

AtHeap* App::GetHeap(unsigned int index) {
    if (index < 4) {
        return m_heaps[index];
    }
    return 0;
}

unsigned int App::HeapsFree() {
    unsigned int free = 0;
    for (int i = 0; i < 4; i++) {
        if (m_heaps[i] != 0) {
            free += m_heaps[i]->m_size - m_heaps[i]->m_used;
        }
    }
    return free;
}

unsigned int App::HeapsUsed() {
    unsigned int used = 0;
    for (int i = 0; i < 4; i++) {
        if (m_heaps[i] != 0) {
            used += m_heaps[i]->m_used;
        }
    }
    return used;
}

GmAppAudioOptions* App::AudioOptionsView() {
    return (GmAppAudioOptions*)((char*)&m_noMusic - __builtin_offsetof(GmAppAudioOptions, m_noMusic));
}

// ---- construction --------------------------------------------------------------------------------------------------

App::App() {
    new (m_device.m_bytes, InPlace()) AppDevice();
    new (m_atlas.m_bytes, InPlace()) AppAtlasLayer();
    new (m_gameState.m_bytes, InPlace()) FaAcPlayerGameStateData();
    SetDefaults();
}

void App::SetDefaults() {
    s_instance = 0;
    m_initA = 0;
    m_initB = 0;
    m_quit = 0;
    for (int i = 0; i < 4; i++) {
        m_heaps[i] = 0;
    }
    m_heapConfig[0].m_size = 0xA00000;
    m_heapConfig[0].m_partner = -1;
    m_heapConfig[1].m_size = 0x600000;
    m_heapConfig[1].m_partner = -1;
    m_heapConfig[2].m_size = 0xF00000;
    m_heapConfig[2].m_partner = -1;
    m_heapConfig[3].m_size = 0x280000;
    m_heapConfig[3].m_partner = -1;
    m_unknown68EC = 0;
    m_historyLogs = 0;
    for (int i = 0; i < 8; i++) {
        m_unknown68F4[i] = 0;
    }
    m_mountManager = 0;
    for (int i = 0; i < 5; i++) {
        m_unknown6918[i] = 0;
    }
    m_unknown692C = 0;
    m_unknown6524 = 0;
    m_unknown6930 = 0;
    for (int i = 0; i < 4; i++) {
        m_frameworks[i] = 0;
    }
    s_heapMemory = 0x2280000;
    m_manifest = 0;
    m_unknown6548 = 0;
    m_previousFramework = 0;
}

// ---- start-up ------------------------------------------------------------------------------------------------------

void App::StartUp() {
    char text[0xFC];
    char report[0x1000];
    GmAppAudioOptions::s_instance = AudioOptionsView();
    AtSdkMemoryStatus status;
    AtSdkGlobalMemoryStatus(&status);
    unsigned int total = status.m_totalPhysical;
    const unsigned int available = status.m_availablePhysical;
    if (available <= 0x4000000) {
        total -= 0x4000000;  // ORIGINAL: with 64 MB or less left, 64 MB is taken off the total
    }
    s_memoryTotal = total;
    s_memoryOverhead = total - available;
    AtFormat(text, "***Mem Total: A:0 G:%d (%fMb)\n", total, total * kBytesToMegabytes);
    AtFormat(text, "***Mem Remaining: A:0 G:%d (%fMb)\n", available, available * kBytesToMegabytes);
    AtFormat(text, "***Mem XBE/Overhead is %d (%fMb)\n", s_memoryOverhead, s_memoryOverhead * kBytesToMegabytes);
    if (available < 0x4000000) {
        AtFormat(text, "***NOTE: You are running with /LIMITMEM and only allowed access to 64Mb of the total %dMb available\n", status.m_totalPhysical >> 20);  // as reported, not as reduced
    }
    AtFormat(text, "[MEM]Overhead\t%d\n", s_memoryOverhead);
    m_noMusic = 0;
    m_noAmbientAudio = 0;
    m_noAudio = 0;
    m_displayBanks = 0;
    m_displaySounds = 0;
    m_displayMusic = 0;
    m_audioMode = -1;
    m_audioTest = 0;
    StartUpAtlas();
    AtFormat(report, "[HEAP INFO] Start\n");
    AtHeapManager::s_instance->Report(report, 0x1000);
    AtFormat(report, "[HEAP INFO] End\n");
}

void App::StartUpAtlas() {
    AppAtlasConfig config;
    config.m_unknown00 = 0;
    config.m_heap = 0;
    for (int i = 0; i < 10; i++) {
        config.m_unknown08[i] = 0;
    }
    if (m_atlasRunning != 0) {
        Shutdown();
    }
    StartUpUserInterface();
    GetAtlasConfig(&config);
    m_atlas->Init(&config, m_userInterface.m_bytes, m_unknown004);
    AfterAtlas();
}

void App::LoadDirtyDiskScreen() {
    AtHeapManager::s_instance->m_override = s_instance->GetHeap(0);
    m_dirtyDiskLoader = s_instance->m_graphics->DatabaseLoader();
    m_dirtyDiskLoader->Load(s_instance->m_materialOwner, "DataXbox\\UI\\DirtyDsk\\loader", 0, 0, 1);
    AtHeapManager::s_instance->m_override = 0;
}

void App::CreateMountManager() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    void* block = operator new(0x48);
    m_mountManager = block != 0 ? AppMountManagerConstruct(block) : 0;
    AtPopHeap();
    if (m_mountManager == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    AtIoMountParams mount;  // ORIGINAL: +0x00, +0x08 and +0x0C are left as the stack had them
    mount.m_heap = m_heaps[0];
    mount.m_context = m_contextArea.Get();
    mount.m_owner = (AtIoMountOwner*)this;
    m_mountManager->Init(&mount);
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    block = operator new(0x24);
    m_historyLogs = block != 0 ? GmHistoryLogsConstruct(block) : 0;
    AtPopHeap();
    if (m_historyLogs == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    AppLogParams logs;  // ORIGINAL: +0x00 is left as the stack had it
    logs.m_heap = m_heaps[0];
    logs.m_app60E8 = (char*)s_instance + 0x60E8;
    logs.m_root = 0;
    logs.m_unknown14 = 1;
    logs.m_unknown18 = 1;
    logs.m_unknown1C = 1;
    logs.m_app = this;
    logs.m_root = m_mountManager->m_root;
    logs.m_unknown1C = 0x10;
    GmHistoryLogsInit(m_historyLogs, &logs);
    AtIoStream::SetHeap(AtIoTokenFile::s_heap);
}

void App::CreateFrameworks() {
    AtPushHeap(m_heaps[0]);
    void* block = operator new(0x110);
    m_frameworks[0] = block != 0 ? FaMoIntroFrameworkConstruct(block) : 0;
    AtPopHeap();
    if (m_frameworks[0] == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    AtPushHeap(m_heaps[0]);
    block = operator new(0x24C);
    m_frameworks[1] = block != 0 ? FaMoFlashMainMenuFrameworkConstruct(block) : 0;
    AtPopHeap();
    if (m_frameworks[1] == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    AtPushHeap(m_heaps[0]);
    block = operator new(0x394);
    m_frameworks[2] = block != 0 ? FaMoGameFrameworkConstruct(block) : 0;
    AtPopHeap();
    if (m_frameworks[2] == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    GmMoFramework::SetHeap(m_heaps[0]);
    GmMoFramework::SetHeap(m_heaps[0]);
    GmMoFramework::SetHeap(m_heaps[0]);
}

void App::SetFrameworkTable() {
    // Rows by the current framework's index, columns by its request. Index 0 is the intro, 1 the main menu, 2 the game, by the order they are made.
    int table[16];
    table[6] = 2;
    table[7] = 2;
    table[8] = 2;
    table[9] = 2;
    table[14] = 2;
    table[15] = 2;
    table[10] = 3;
    table[12] = 3;
    table[13] = 3;
    table[0] = 0;
    table[1] = 0;
    table[2] = 1;
    table[3] = 1;
    table[4] = 1;
    table[5] = 1;
    table[11] = 1;
    if (s_instance->m_settings.m_testAllLevels != 0) {
        // ORIGINAL: the same values again, so the switch changes nothing here.
        table[0] = 0;
        table[1] = 0;
        table[2] = 1;
        table[3] = 1;
    }
    for (int i = 0; i < 16; i++) {
        m_frameworkTable[i] = table[i];
    }
}

void App::LoadRegistries() {
    AtIoFileSystem* fs;
    AtIoMountManager::s_instance->m_context->PushPath("Design\\Registry\\");
    if (AtIoMountManager::s_instance->m_context->FindFile("Generic.ADR", &fs) == 0) {
        m_genericRegistry.Load(GmDataRoot::s_instance->Owner(), "Generic", 0, 0, 0);
        AtDataRegistryAttachUnder(&m_genericRegistry, &GmDataRoot::s_instance->m_registry, 0);
    }
    AtIoMountManager::s_instance->m_context->PopPath();
    s_instance->m_gameState->SetRegistry(&m_genericRegistry);
    AtIoMountManager::s_instance->m_context->PushPath("");
    if (AtIoMountManager::s_instance->m_context->FindFile("Library.ADD", &fs) == 0) {
        AtDataRegistryPrepare(&GmDataRoot::s_instance->m_registry, GmDataRoot::s_instance->Owner());
        AtDataRegistrySetFileName(&GmDataRoot::s_instance->m_registry, "Library");
        m_library.Load((AtDataOwner*)GmDataRoot::s_instance->Owner(), "Library", 0, &GmDataRoot::s_instance->m_registry);
        AtDataRegistry* root = &GmDataRoot::s_instance->m_registry;
        GmRegistryUser* user = GmRegistryUser::s_instance;
        user->m_registry = root;
        user->m_root = AtDataRegistryFindPath(root, &root->m_root, "", 0);
        GmRegistryNames::s_instance->Add(&GmDataRoot::s_instance->m_registry);
    }
    AtIoMountManager::s_instance->m_context->PopPath();
}

void App::Init(int a, int b) {
    char level[9];
    char shortName[8];
    char title[0x40];
    char report[0x1000];
    if (m_atlasRunning != 0) {
        Shutdown();
    }
    s_instance = this;
    m_initA = a;
    m_initB = b;
    App::StartUp();
    m_unknown6548 = 0;
    GmScriptsInitNames();
    s_heap = m_heaps[0];
    GmSetHeap00083490(m_heaps[0]);
    GmAcPlayerGameStateDataSetHeap(m_heaps[0]);
    new (&m_settings, InPlace()) GmAppSettings();
    CreateMountManager();
    SetDisplayMode();
    LoadDirtyDiskScreen();
    m_unknown65B4 = 1;
    GmSetHeap000980F0(s_heap);
    m_settings.Load();
    {
        char folder[8];
        AtCopyName8(folder, m_settings.m_dataFolder);
        m_mountManager->SetRegion(folder);
    }
    m_settings.ReadCommandLine();  // after App.ini, so the command line wins

    if (s_instance->m_settings.m_openList != 0) {
        AtIoMountManager::s_instance->OpenListFile("AppInit.txt");
    }
    AtIoMountManager::s_instance->Mount("AppInit", "");
    ReadMatrixDevice();
    CreateSubsystems();
    FaAcPlayerGameStateData* state = m_gameState.Get();
    state->Slot1();
    state->Slot3();
    s_gameState = state;
    m_unknown692C->Init();
    // The original calls an empty function (0x001CC9F0) on m_settings here.
    CreateFrameworks();
    SetFrameworkTable();
    LoadRegistries();
    GmAudioReadSavedVolumes(GmAudioData::s_instance);
    GmUnknown97EB0(g_unknown30567C + 4);
    if (s_instance->m_settings.m_openList != 0) {
        AtIoMountManager::s_instance->CloseListFile();
    }
    AtIoMountManager::s_instance->Unmount();

    // The starting level, by name, from App.ini or the command line.
    AtCopyName9(level, s_instance->m_settings.m_level);
    GmLevelManager* levels = GmLevelManager::s_instance;
    levels->m_previousIndex = levels->m_currentIndex;
    levels->m_currentIndex = levels->FindLevel(level);
    for (int i = 0;; i++) {
        levels->m_currentName[i] = level[i];
        if (level[i] == 0) {
            break;
        }
    }

    GmGetStringByName(shortName, 8, "SName");
    AtCopyName8(g_appShortName, shortName);
    GmGetStringByName(title, 0x40, "Title");
    for (int i = 0; i < 0x40; i++) {
        g_appTitle[i] = title[i];
    }

    GmScripts::s_instance->Init(m_heaps[0]);
    GmRegisterAppCallbacks();
    GmRegisterControlCallbacks();
    GmRegisterSaveDataCallbacks();
    GmScripts::s_instance->FindModule("Control")->SetEnabled(1);
    g_unknown305130 = 1;
    void* handle = AtIoSpaceManager::s_instance->m_device->Unknown2(4, 0);
    void* added = AtIoSpaceManagerAdd(AtIoSpaceManager::s_instance, handle, GmGameSaveSize(), 1);
    {
        char name[8] = {'G', 'a', 'm', 'e', 0, 0, 0, 0};
        GmSaveTypes& types = GmSaveTypes::s_instance;
        if ((unsigned int)types.m_count < 5) {
            types.m_current = types.m_count;
            types.m_types[types.m_count].m_handle = added;
            types.m_types[types.m_count].m_size = GmGameSaveSize();
            AtCopyName8(types.m_types[types.m_count].m_name, name);
            types.m_types[types.m_count].m_registered = 1;
            types.m_count++;
        }
        types.m_flags |= 0x10;
    }
    GmScripts::s_instance->FindModule("SaveData")->SetEnabled(1);

    m_framework = m_frameworks[0];
    m_framework->Enter(this, 0);
    if (m_settings.m_testAllLevels != 0) {
        m_framework->m_request = 2;
    }
    g_enabled305758 = 1;
    g_enabled3B3514 = 1;
    g_enabled3056EC = 1;
    g_enabled305710 = 1;
    g_enabled305734 = 1;
    g_enabled3056C8 = 1;
    g_enabled3056A4 = 1;
    g_enabled30580C = 1;
    g_enabled305830 = 1;
    g_enabled3057E8 = 1;
    g_enabled3057A0 = 1;
    g_enabled30577C = 1;

    // The persistent data: PERSIST.MNG from Persist.JAM, loaded in 30 millisecond slices until done.
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_manifest = new GmLevelManifest;
    AtPopHeap();
    if (m_manifest == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    GmManifestRequest request;
    char path[8];
    request.m_path = 0;
    request.m_heap = 0;
    if (s_instance->m_settings.m_openList != 0) {
        AtIoMountManager::s_instance->OpenListFile("Persist.txt");
    }
    AtIoMountManager::s_instance->Mount("Persist", "");
    for (int i = 0; i < 8; i++) {
        path[i] = "Persist"[i];
    }
    AtCopyName8(request.m_name, "Persist");
    AppSaveDataInit(m_saveData);
    request.m_path = path;
    m_manifest->Configure(&request);
    while (m_manifest->Update(30) == 0) {
    }
    AtIoMountManager::s_instance->Unmount();
    if (s_instance->m_settings.m_openList != 0) {
        AtIoMountManager::s_instance->CloseListFile();
    }
    AtFormat(report, "[HEAP INFO] Start\n");
    AtHeapManager::s_instance->Report(report, 0x1000);
    AtFormat(report, "[HEAP INFO] End\n");
}

// ---- the main loop -------------------------------------------------------------------------------------------------

void App::Run() {
    unsigned int last = AtPlatformMilliseconds() - 32;
    int elapsed = 0;  // ORIGINAL: summed and capped at 300 milliseconds every frame, and never read
    m_viewport->SetScreenSize(m_screenWidth, m_screenHeight);
    if (m_quit != 0) {
        return;
    }
    do {
        if ((m_device->m_flags & AppDevice::kQuit) != 0) {
            return;
        }
        if (m_device->RunFrame() == 0) {
            return;
        }
        const unsigned int now = AtPlatformMilliseconds();
        elapsed += now - last;
        if (elapsed > 300) {
            elapsed = 300;
        }
        last = now;
        g_unknown304D54 = 0;
        GmClock::s_instance->Tick(g_unknown304D80 == 0);
        m_unknown692C->Update(GmClock::s_instance->m_time);
        if (g_unknown3ADA50 != 0) {
            g_unknown3ADA50 = 0;
            AppUnknown90580();
        }
        g_inScripts = 1;
        while (GmRunScripts(GmClock::s_instance->m_frame) != 0) {
        }
        g_inScripts = 0;
        const int next = m_frameworkTable[m_framework->m_index * 4 + m_framework->m_request];
        if (next != m_framework->m_index) {
            m_previousFramework = m_framework->m_index;
            m_framework->Leave();
            m_framework = m_frameworks[next];
            m_framework->Enter(this, next);
        }
        m_framework->Slot3();
        m_framework->Slot2();
        App::Present(0);
        // The original calls an empty function (0x001CC9F0) on the framework here.
    } while (m_quit == 0);
}
