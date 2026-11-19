// The application object and start-up: the game's main, App's init and its main loop, and App's own small virtual slots.
//
// RTTI names the class App, vftable 0x0027AE44 with 69 slots, deriving through GmApp (vftable 0x0028DD98) and sixteen AtAp*Application classes, from
// AtApAtlasApplication down to AtApBaseApplication. One static instance, at 0x002BF310. The bases are not recovered: their members are laid out here flat, at
// their offsets in App, with names inferred. Which of App, GmApp or an AtAp layer defines each method is not established; they are all members of App here.
// Every name other than App, GmApp, the AtAp* names, FaAcPlayerGameStateData, GmAcPlayerGameStateData and the Fa/GmMo*Framework names (all RTTI) is inferred.
//
// The AtAp layer chain beneath GmApp's start-up is recovered (at/atap: src/gm/GmAppLayers.cpp, include/at/AtApLayer.h), with the base, event, network and
// I/O layers' objects. What is not recovered is behind a seam that stops with a diagnostic (src/gm/GmAppHost.cpp): the layer objects from math up, the
// display, graphics, input and sound objects, the frameworks, the script system, the save-data system and the registry members that belong to the .ADR
// work. The SDK's memory query is the platform's (platform/host_sdk.cpp) or a stop (src/gm/GmAppSdk.cpp). See docs/worklog/2026-11-08-atap.md for where a
// real build stops.
#pragma once
#include "at/AtApLayer.h"
#include "at/AtDataLibrary.h"
#include "at/AtDataRegistry.h"
#include "at/AtHeap.h"
#include "at/AtIoMountManager.h"
#include "at/AtMemory.h"
#include "gm/GmAppSettings.h"
#include "gm/GmSocket.h"

class App;
class AtDatabaseLoader;
class GmAudioData;
class GmLevelManifest;
struct AtMaterialOwner;
struct GmHistoryLogs;

// ---- what the start-up reaches and is not recovered ------------------------------------------------------------------

// What the SDK's GlobalMemoryStatus fills in (0x00085451, XAPI). Only the two fields start-up reads are named. A port answers it from the host
// (platform/host_sdk.cpp); src/gm/GmAppSdk.cpp, which stops, is linked where nothing else answers it.
struct AtSdkMemoryStatus {
    unsigned int m_length;          // +0x00
    unsigned int m_memoryLoad;      // +0x04
    unsigned int m_totalPhysical;   // +0x08
    unsigned int m_availablePhysical;  // +0x0C
    unsigned int m_totalPageFile;   // +0x10
    unsigned int m_availablePageFile;  // +0x14
    unsigned int m_totalVirtual;    // +0x18
    unsigned int m_availableVirtual;   // +0x1C
};
void AtSdkGlobalMemoryStatus(AtSdkMemoryStatus* status);  // 0x00085451 SDK; the platform's, or a stop

// The object at +0x5F7C of App, by its slots: the graphics layer's (at/atap). Slot 1 is its Init, from the graphics layer's start-up; slot 4 sets a display
// mode (from 0x0008DBA0), slot 6 runs one frame and returns 0 to end the loop. Its class and the meaning of its flags are not known beyond what start-up
// and the loop test. vftable 0x0028478C, then App's 0x0028DC48 over it (constructor 0x000B2F80).
class AppDevice {
public:
    enum Flags { kRunning = 1, kNoPresent = 4, kQuit = 8 };  // names from use: start-up shuts down first on 1, 0x0009CA90 skips a call on 4, the loop ends on 8
    virtual void Slot0();
    virtual void Init(const void* config, void* below, void* memory);  // slot 1, 0x000B3000 not recovered
    virtual void Slot2();
    virtual void Slot3();
    virtual void Slot4();
    virtual void Slot5();
    virtual int RunFrame();          // slot 6

    int m_unknown04;                 // +0x04
    int m_unknown08;                 // +0x08
    unsigned int m_flags;            // +0x0C, App+0x5F88
};

// What App+0x5F90 points to. Slot 29 gives the database loader start-up loads the dirty-disc screen with; slot 21 is read by 0x0009D9C0.
class AppGraphics {
public:
    virtual void Slot0();
    virtual void Slot1();
    virtual void Slot2();
    virtual void Slot3();
    virtual void Slot4();
    virtual void Slot5();
    virtual void Slot6();
    virtual void Slot7();
    virtual void Slot8();
    virtual void Slot9();
    virtual void Slot10();
    virtual void Slot11();
    virtual void Slot12();
    virtual void Slot13();
    virtual void Slot14();
    virtual void Slot15();
    virtual void Slot16();
    virtual void Slot17();
    virtual void Slot18();
    virtual void Slot19();
    virtual void Slot20();
    virtual void Slot21();
    virtual void Slot22();
    virtual void Slot23();
    virtual void Slot24();
    virtual void Slot25();
    virtual void Slot26();
    virtual void Slot27();
    virtual void Slot28();
    virtual AtDatabaseLoader* DatabaseLoader();  // slot 29
};

// The layer objects that GmApp's start-up initialises, one for each AtAp layer. Only the first two are reached here: the atlas layer's at +0x6450, which is a
// vftable alone (its flag follows at +0x6454), and the user interface layer's at +0x6358 (flag at +0x644C), initialised by 0x0009C3C0.
struct AppAtlasConfig {
    int m_unknown00;
    AtHeap* m_heap;                  // +0x04 App's first heap
    int m_unknown08[10];
};
class AppAtlasLayer {
public:
    virtual void Slot0();
    virtual void Init(const AppAtlasConfig* config, void* userInterface, void* app04);  // slot 1
};

// What slot 7 fills in: the event layer's config (at/atap). Only the heap is set.
typedef AtEventConfig AppLayerConfig07;

// The objects of the AtAp layers above the I/O layer, where they are not recovered (at/atap). Each layer's start-up calls slot 1, the object's Init, with the
// layer's config, the object of the layer below and, for most, App's memory system. Each Init stops with a diagnostic (src/gm/GmAppHost.cpp). Slot 0 is a
// deleting destructor in each. The vftables are App's own, written over the layer's by App's constructor where it has one.
class AppMathLayer {         // +0x0384, vftable 0x002847FC, constructor 0x000B38B0
public:
    virtual void Slot0();
    virtual void Init(const void* config, void* below, void* memory);  // slot 1, 0x000B38D0
};
class AppDataManagementLayer {  // +0x0390, vftable 0x002847EC, constructor 0x000B3770
public:
    virtual void Slot0();
    virtual void Init(const void* config, void* below, void* memory);  // slot 1, 0x000B3790
};
class AppScriptLayer {       // +0x0414, vftable 0x0027D03C, constructor 0x0009EE60
public:
    virtual void Slot0();
    virtual void Init(const void* config, void* below, void* memory);  // slot 1, 0x0009EE80
};
class AppKinematicsLayer {   // +0x5F70, vftables 0x002847DC and 0x0028DC6C, constructor 0x000B34B0
public:
    virtual void Slot0();
    virtual void Init(const void* config, void* below, void* memory);  // slot 1, 0x000B34D0
};
class AppControllerLayer {   // +0x6020, vftables 0x0028477C and 0x0028DC38, constructor 0x000B2DE0
public:
    virtual void Slot0();
    virtual void Init(const void* config, void* below, void* memory);  // slot 1, 0x000B2E00
};
class AppSoundLayer {        // +0x60B0, vftables 0x00284768 and 0x0028DC24, constructor 0x000B2C80
public:
    virtual void Slot0();
    virtual void Init(const void* config, void* below);                // slot 1, 0x000B2BE0
};
class AppVideoLayer {        // +0x60CC, vftables 0x00284758 and 0x0028DC14, constructor 0x000B2A80
public:
    virtual void Slot0();
    virtual void Init(const void* config, void* below, void* memory);  // slot 1, 0x000B2AA0
};
class AppSceneLayer {        // +0x60DC, vftables 0x00284744 and 0x0028DC00, constructor 0x000B29C0
public:
    virtual void Slot0();
    virtual void Init(const void* config, void* below, void* memory);  // slot 1, 0x000B2970
};
class AppUserInterfaceLayer {  // +0x6358, vftable 0x00284724, constructor 0x000B2830
public:
    virtual void Slot0();
    virtual void Init(const void* config, void* below);                // slot 1, 0x000B28B0
};

// What App's config slots fill in for those layers, as far as App sets them. Sizes are those start-up clears.
struct AppMathConfig {               // slot 19
    int m_unknown00;
    AtHeap* m_heap;                  // +0x04
    int m_unknown08[2];
};
struct AppDataManagementConfig {     // slot 23
    int m_unknown00;                 // 0x80
    int m_unknown04;                 // 0x80
    AtHeap* m_heap;                  // +0x08
    int m_unknown0C[10];
};
struct AppScriptConfig {             // slot 27
    int m_unknown00;
    AtHeap* m_heap;                  // +0x04
    int m_unknown08[3];
};
struct AppKinematicsConfig {         // slot 31
    int m_unknown00;
    AtHeap* m_heap;                  // +0x04
    int m_unknown08[11];
};
struct AppGraphicsConfig {           // slot 35
    int m_unknown00;                 // 1
    int m_unknown04;
    AtHeap* m_heap;                  // +0x08
    int m_unknown0C[57];
};
struct AppControllerConfig {         // slot 40
    int m_unknown00;                 // 1
    AtHeap* m_heap;                  // +0x04
    int m_unknown08[10];
};
struct AppSoundConfig {              // slot 44
    AtHeap* m_heap;                  // +0x00 App's second heap
    int m_unknown04[10];
    int m_unknown2C;                 // 100
};
struct AppVideoConfig {              // slot 48, which App leaves empty
    int m_unknown00[5];
};
struct AppSceneConfig {              // slot 52
    int m_unknown00[7];
    AtHeap* m_heap;                  // +0x1C
    int m_unknown20[27];
};
struct AppUserInterfaceConfig {      // slot 56, not recovered
    int m_unknown00[26];
};

// What App+0x6458 points to. 0x0009EA00 divides the screen size by two reals the object holds, and stores the results as floats.
class AppViewport {
public:
    void SetScreenSize(int width, int height);  // 0x0009EA00 not recovered
};

// RTTI FaAcPlayerGameStateData, deriving GmAcPlayerGameStateData; vftable 0x0027CE08, constructor 0x0009DA50. At App+0x65B8, 0x214 bytes.
// Start-up calls slots 1 and 3, and hands slot 5 the registry read from Generic.ADR. Not recovered.
class FaAcPlayerGameStateData {
public:
    virtual void Slot0();
    virtual void Slot1();                        // 0x0009DA40
    virtual void Slot2();
    virtual void Slot3();                        // 0x0009DA20
    virtual void Slot4();
    virtual void SetRegistry(AtDataRegistry* registry);  // slot 5, 0x0019D010
};

// RTTI GmMoFramework, deriving GmPlug; vftable 0x0028DBD8, constructor 0x00140F70. App holds three, made by 0x0008DC70: FaMoIntroFramework, FaMoFlashMainMenuFramework
// and FaMoGameFramework. The main loop runs the current one each frame and moves between them by App's table. Not recovered.
class GmMoFramework : public GmPlug {
public:
    virtual void Enter(App* app, int index);     // slot 1
    virtual void Slot2();                        // second each frame
    virtual void Slot3();                        // first each frame
    virtual void Slot4();
    virtual void Leave();                        // slot 5
    virtual void Slot6();                        // 0x001CC9F0, empty in all three

    int m_index;                     // +0x08 its place among App's frameworks
    int m_request;                   // +0x0C a column of App's table: what it asks to move to

    static void SetHeap(void* heap); // 0x00140C20 recovered; sets s_heap
    static void* s_heap;             // 0x003B3600
};
GmMoFramework* FaMoIntroFrameworkConstruct(void* block);         // 0x0009B6B0, 0x110 bytes
GmMoFramework* FaMoFlashMainMenuFrameworkConstruct(void* block); // 0x0009B1E0, 0x24C bytes
GmMoFramework* FaMoGameFrameworkConstruct(void* block);          // 0x00099520, 0x394 bytes

// The derived mount manager made at App+0x6914 (vftable 0x0028DBA0, 0x48 bytes, constructor 0x00140350, which also sets 0x003B35CC). It overrides slot 4,
// SetRegion, with 0x001402E0. Not recovered beyond its base.
AtIoMountManager* AppMountManagerConstruct(void* block);         // 0x00140350

// The history logs at App+0x68F0 (0x24 bytes): their constructor and their Init are not recovered.
struct AppLogParams {
    int m_unknown00;                 // +0x00 not set by start-up
    App* m_app;                      // +0x04
    AtHeap* m_heap;                  // +0x08
    void* m_app60E8;                 // +0x0C App+0x60E8
    AtIoFileSystem* m_root;          // +0x10 the mount manager's m_root
    int m_unknown14;                 // +0x14 1
    int m_unknown18;                 // +0x18 1
    int m_unknown1C;                 // +0x1C 0x10
};
GmHistoryLogs* GmHistoryLogsConstruct(void* block);               // 0x0006C2A0 sets GmHistoryLogs::s_instance
void GmHistoryLogsInit(GmHistoryLogs* logs, const AppLogParams* params);  // 0x0006C3E0

// The script system, at 0x003B3418. Start-up initialises it and finds two modules by name. Not recovered.
class GmScriptModule {
public:
    virtual void Slot0();
    virtual void Slot1();
    virtual void Slot2();
    virtual void Slot3();
    virtual void Slot4();
    virtual void Slot5();
    virtual void SetEnabled(int on); // slot 6
};
class GmScripts {
public:
    void Init(AtHeap* heap);                     // 0x0012EEA0
    GmScriptModule* FindModule(const char* name);  // 0x0012EFC0 the first whose eight-character name matches; null if none
    static GmScripts* s_instance;    // 0x003B3418
};
void GmRegisterAppCallbacks();       // 0x0008E4E0 thirteen script callbacks: names, dialogs, input, strings, PAL
void GmRegisterControlCallbacks();   // 0x0008C450 five "Control" callbacks
void GmRegisterSaveDataCallbacks();  // 0x0008CB30 makes the "SaveData" module and its 31 callbacks
int GmRunScripts(int frame);         // 0x00091540 one step; non-zero while there is more

// The game's text: 0x00090950 copies the string of a name into out, at most size. Not recovered.
void GmGetStringByName(char* out, unsigned int size, const char* name);

// The object at 0x002CD11C: a clock, by its use. Tick is not recovered.
class GmClock {
public:
    void Tick(int running);          // 0x00071A60
    int m_unknown00[6];
    int m_time;                      // +0x18 passed to App+0x692C's update
    int m_frame;                     // +0x1C passed to the script step
    static GmClock* s_instance;      // 0x002CD11C
};

// What App+0x692C points to; made in 0x0008CFB0. Not recovered.
class AppUnknown692C {
public:
    void Init();                     // 0x00090210
    void Update(int time);           // 0x0006DEA0
};

// The registry at +0x2C of the object at 0x003B357C, and the members of AtDataRegistry the start-up calls that are not recovered (they belong to the .ADR work).
struct GmDataRoot {
    void* m_unknown00;
    char* m_ownerBase;               // +0x04 the registries' owner lies at +0x394 of what this points to
    char m_unknown08[0x24];
    AtDataRegistry m_registry;       // +0x2C
    AtDataRegistryOwner* Owner() { return (AtDataRegistryOwner*)(m_ownerBase + 0x394); }
    static GmDataRoot* s_instance;   // 0x003B357C
};
// AtDataRegistry's AttachUnder, Prepare, SetFileName and FindPath, which start-up calls, are declared with the class (include/at/AtDataRegistry.h) and recovered by at/datalookup.
// The object at 0x003B3524 keeps the library's registry and its root node; the name map at 0x002CD138 is told of the registry. Neither is recovered.
struct GmRegistryUser {
    char m_unknown00[0x40];
    AtDataRegistry* m_registry;      // +0x40
    AtDataLink* m_root;              // +0x44
    static GmRegistryUser* s_instance;  // 0x003B3524
};
struct GmRegistryNames {
    void Add(AtDataRegistry* registry);  // 0x00071F20
    static GmRegistryNames* s_instance;  // 0x002CD138
};

// The save-data part at App+0x67CC, 0x120 bytes (constructor 0x0009D5D0); 0x0009D9C0 initialises it. Not recovered.
void AppSaveDataInit(void* part);    // 0x0009D9C0
void GmAudioReadSavedVolumes(GmAudioData* audio);  // 0x00139F10 SaveData\Persist\Sound\Volumes and Mode
void GmUnknown97EB0(void* object);   // 0x00097EB0 on +4 of the object at 0x0030567C
extern char* g_unknown30567C;        // 0x0030567C
void* AtIoSpaceManagerAdd(AtIoSpaceManager* manager, void* handle, unsigned int size, int flag);  // 0x0007D070 a member, not recovered; start-up passes flag 1
void AppUnknown90580();              // 0x00090580

// ---- recovered small pieces ----------------------------------------------------------------------------------------

void GmScriptsInitNames();           // 0x000703B0 the small-C scripts' name map, 50 entries
void GmSetHeap00083490(void* heap);  // 0x00083490 stores the heap for the code at 0x00083740-0x000837F0; its class is not identified
void GmAcPlayerGameStateDataSetHeap(void* heap);  // 0x00143DD0 its users are at 0x001440C0-0x001441F3, beside GmAcPlayerGameStateData; inferred
void GmSetHeap000980F0(void* heap);  // 0x000980F0 nothing reads what it stores
unsigned int GmGameSaveSize();       // 0x0009D540 0xF8, the size the "Game" save type is registered with

extern GmNameMap g_scriptNames;      // 0x002CD0F8
extern void* g_heap00301B70;         // 0x00301B70
extern void* g_heap003B3614;         // 0x003B3614
extern void* g_heap00305914;         // 0x00305914

// Save types, from 0x0008E921: five at most. Who reads them is at 0x000903D0 and beyond, not recovered.
struct GmSaveType {
    void* m_handle;                  // +0x00 from the space manager
    int m_registered;                // +0x04
    unsigned int m_size;             // +0x08
    char m_name[8];                  // +0x0C
};
struct GmSaveTypes {
    int m_count;                     // 0x00305140
    int m_current;                   // 0x00305144
    GmSaveType m_types[5];           // 0x00305148
    int m_unknown6C;                 // 0x003051AC
    unsigned int m_flags;            // 0x003051B0 start-up sets 0x10
    static GmSaveTypes s_instance;
};

// Start-up sets twelve words to 1, eleven of them the second word of objects 0x24 bytes apart from 0x003056A0 (the one at 0x003057C0 is skipped), on a list
// whose head is 0x00305680, built by static constructors. What they are is not known.
extern int g_enabled3B3514;
extern int g_enabled3056A4, g_enabled3056C8, g_enabled3056EC, g_enabled305710, g_enabled305734, g_enabled305758, g_enabled30577C, g_enabled3057A0,
    g_enabled3057E8, g_enabled30580C, g_enabled305830;

extern char g_appShortName[8];       // 0x003050E8 the "SName" string; 0x000903B0 returns it
extern char g_appTitle[0x40];        // 0x003050F0 the "Title" string; 0x000903C0 returns it
extern int g_unknown305130;          // 0x00305130 start-up sets 1
extern int g_unknown304D80;          // 0x00304D80 the clock runs while this is 0
extern int g_unknown304D54;          // 0x00304D54 cleared each frame
extern int g_unknown3ADA50;          // 0x003ADA50 one-shot: 0x00090580 runs on the frame after it is set
extern int g_inScripts;              // 0x003B361C 1 while the frame's scripts run

// A piece of App at a fixed place whose class is not recovered: only reached through T's virtual slots, never by its own constructor here.
template <class T, unsigned int N> struct AppPart {
    alignas(void*) unsigned char m_bytes[N >= sizeof(T) ? N : sizeof(T)];
    T* Get() { return reinterpret_cast<T*>(m_bytes); }
    T* operator->() { return Get(); }
};

// ---- App ------------------------------------------------------------------------------------------------------------

class App {
public:
    struct HeapConfig {
        unsigned int m_size;         // 0 means no heap
        int m_partner;               // -2 the default heap's group, -1 none, otherwise another of the four
    };

    // PARTIAL, not 0x0008E5B0. The original constructs the bases (0x00141DA0, GmApp, and down the AtAp chain), settings by 0x00098160, the game state (0x0009DA50),
    // the save-data part (0x0009D5D0), the two registries and the library, then calls SetDefaults. Here the bases are not constructed: what they hold is zero,
    // settings are constructed by GmAppSettings(), and the unrecovered parts get their seam classes' vftables.
    App();

    virtual void GetMemoryParams(AtMemoryParams* params);   // slot 0, 0x0008C620 the region the heaps take, plus 0x2800
    virtual void CreateHeaps();                  // slot 1, 0x0008C860
    virtual void DestroyHeaps();                 // slot 2, 0x0008C920
    virtual void Slot03() {}                     // slot 3, 0x001CC9F0 empty
    virtual void DeletingDestructor(unsigned int flags);  // slot 4, 0x0008F4D0 not recovered
    virtual void StartUp();                      // slot 5, 0x00141500; GmApp's, by its address
    virtual void Shutdown();                     // slot 6, 0x0008ED60 not recovered
    // From slot 7 on, four slots to each AtAp layer above the base, from the bottom (five to the graphics layer): its config, filled in before its start-up
    // initialises the object, then what start-up calls after that, then two more that start-up does not call (at/atap).
    virtual void GetConfig07(AppLayerConfig07* config);  // slot 7, 0x0008C680 the event layer's config
    virtual void AfterEvent() {}                 // slot 8, 0x001CC9F0 empty, as are 9-10
    virtual void Slot09() {}
    virtual void Slot10() {}
    virtual void GetNetworkConfig(AtNetworkConfig* config) {}  // slot 11, 0x000CBF30 empty (folded): the config stays clear
    virtual void AfterNetwork() {}               // slot 12, empty, as are 13-14
    virtual void Slot13() {}
    virtual void Slot14() {}
    virtual void GetIoConfig(AtIoConfig* config);  // slot 15, 0x0008C650
    virtual void AfterIo() {}                    // slot 16
    virtual void Slot17() {}
    virtual void Slot18() {}
    virtual void GetMathConfig(AppMathConfig* config);  // slot 19, 0x0008C6B0
    virtual void AfterMath() {}                  // slot 20
    virtual void Slot21() {}
    virtual void Slot22() {}
    virtual void GetDataManagementConfig(AppDataManagementConfig* config);  // slot 23, 0x0008C6E0
    virtual void AfterDataManagement() {}        // slot 24
    virtual void Slot25() {}
    virtual void Slot26() {}
    virtual void GetScriptConfig(AppScriptConfig* config);  // slot 27, 0x0008C800
    virtual void AfterScript() {}                // slot 28
    virtual void Slot29() {}
    virtual void Slot30() {}
    virtual void GetKinematicsConfig(AppKinematicsConfig* config);  // slot 31, 0x0008C710
    virtual void AfterKinematics() {}            // slot 32
    virtual void Slot33() {}
    virtual void Slot34() {}
    virtual void GetGraphicsConfig(AppGraphicsConfig* config);  // slot 35, 0x0008C740
    virtual void AfterGraphics() {}              // slot 36
    virtual void Slot37() {}
    virtual void Slot38() {}
    virtual void Present(int unused);            // slot 39, 0x0009CA90 not recovered: unless the device's flag 4, the material owner's slot 9
    virtual void GetControllerConfig(AppControllerConfig* config);  // slot 40, 0x0008C770
    virtual void AfterController() {}            // slot 41
    virtual void Slot42() {}
    virtual void Slot43() {}
    virtual void GetSoundConfig(AppSoundConfig* config);  // slot 44, 0x0008C7A0
    virtual void AfterSound() {}                 // slot 45
    virtual void Slot46() {}
    virtual void Slot47() {}
    virtual void GetVideoConfig(AppVideoConfig* config) {}  // slot 48, 0x000CBF30 empty (folded)
    virtual void AfterVideo() {}                 // slot 49
    virtual void Slot50() {}
    virtual void Slot51() {}
    virtual void GetSceneConfig(AppSceneConfig* config);  // slot 52, 0x0008C7D0
    virtual void AfterScene() {}                 // slot 53
    virtual void Slot54() {}
    virtual void Slot55() {}
    virtual void GetUserInterfaceConfig(AppUserInterfaceConfig* config);  // slot 56, 0x0008DB30 not recovered: needs the material owner's slot 15
    virtual void AfterUserInterface() {}         // slot 57
    virtual void Slot58() {}
    virtual void Slot59() {}
    virtual void GetAtlasConfig(AppAtlasConfig* config);  // slot 60, 0x0008C830
    virtual void AfterAtlas() {}                 // slot 61, 0x001CC9F0 empty
    virtual void Slot62() {}
    virtual void Slot63() {}
    virtual AtHeap* GetHeap(unsigned int index); // slot 64, 0x0008C960 null past the fourth
    virtual void Slot65();                       // 0x0010F4B0 not recovered
    virtual void Slot66();                       // 0x0008E1F0 not recovered
    virtual unsigned int HeapsFree();            // slot 67, 0x0008C9C0 what the four heaps have not handed out
    virtual unsigned int HeapsUsed();            // slot 68, 0x0008C980

    void Init(int a, int b);                     // 0x0008E660 a and b are stored only; main passes 0 and 0
    void Run();                                  // 0x0008EBE0 the main loop
    void SetDefaults();                          // 0x0008DA10 the constructor's last step
    void StartUpAtlas();                         // 0x0009C280 the AtApAtlasApplication layer's start-up, inferred from the layer order
    // The AtAp layers' start-ups, from the top (at/atap, src/gm/GmAppLayers.cpp). Each shuts App down first if its layer is running, runs the layer
    // below's start-up, asks App for its config, initialises its object and calls App's slot for after; so the base layer is brought up first. Which
    // AtAp class each belongs to is inferred: see include/at/AtApLayer.h.
    void StartUpUserInterface();                 // 0x0009C3C0
    void StartUpScene();                         // 0x0009C510
    void StartUpVideo();                         // 0x0009C680
    void StartUpSound();                         // 0x0009C7D0
    void StartUpController();                    // 0x0009C940
    void StartUpGraphics();                      // 0x0009CAB0
    void StartUpKinematics();                    // 0x0009CC10
    void StartUpScript();                        // 0x0009CD50
    void StartUpDataManagement();                // 0x0009CEB0
    void StartUpMath();                          // 0x0009CFF0
    void StartUpIo();                            // 0x0009D140
    void StartUpNetwork();                       // 0x0009D2A0
    void StartUpEvent();                         // 0x0009D3D0
    void StartUpBase();                          // 0x0009D4F0 the memory system, then App's heaps
    void SetDisplayMode();                       // 0x0008DBA0 not recovered: graphics
    void LoadDirtyDiskScreen();                  // 0x0008E240
    void CreateMountManager();                   // 0x0008CE00 also the history logs, and the stream heap
    void ReadMatrixDevice();                     // 0x001416A0 not recovered: graphics
    void CreateSubsystems();                     // 0x0008CFB0 not recovered: 634 functions beneath it
    void CreateFrameworks();                     // 0x0008DC70
    void SetFrameworkTable();                    // 0x0008DDF0
    void LoadRegistries();                       // 0x0008DEA0 Generic.ADR, then Library.ADD

    // The AtAp layers' objects, from the bottom (at/atap). Each layer's running flag is a word of its object.
    AtRegionMemorySystem m_memory;               // +0x0004 the base layer's: App's has vftable 0x0028DCA0, differing only in the destructor
    AtEventSystem m_events;                      // +0x0268
    AtNetworkSystem m_network;                   // +0x02F0
    AtIoSystem m_io;                             // +0x0310 its factory at +0x0318 (App is the mount manager's AtIoMountOwner), its context at +0x031C
    AppPart<AppMathLayer, 4> m_math;             // +0x0384
    int m_mathRunning;                           // +0x0388
    int m_unknown038C;                           // +0x038C
    AppPart<AppDataManagementLayer, 0x80> m_dataManagement;  // +0x0390
    int m_dataManagementRunning;                 // +0x0410
    AppPart<AppScriptLayer, 4> m_script;         // +0x0414
    int m_scriptRunning;                         // +0x0418
    char m_unknown041C[0x5B54];                  // +0x041C the rest of the script layer's object (0x000B42B0 writes its +0x5B1C); +0x0420 is handed on
    AppPart<AppKinematicsLayer, 8> m_kinematics; // +0x5F70
    int m_kinematicsRunning;                     // +0x5F78
    AppPart<AppDevice, 0x14> m_device;           // +0x5F7C the graphics layer's
    AppGraphics* m_graphics;                     // +0x5F90
    int m_unknown5F94;                           // +0x5F94
    AtMaterialOwner* m_materialOwner;            // +0x5F98
    char m_unknown5F9C[0x84];                    // +0x5F9C the rest of the graphics layer's object
    AppPart<AppControllerLayer, 0x84> m_controller;  // +0x6020
    int m_controllerRunning;                     // +0x60A4
    char m_unknown60A8[8];                       // +0x60A8
    AppPart<AppSoundLayer, 4> m_sound;           // +0x60B0
    unsigned int m_soundFlags;                   // +0x60B4 bit 0: running
    char m_unknown60B8[0x14];                    // +0x60B8 +0x60C4 is handed on
    AppPart<AppVideoLayer, 4> m_video;           // +0x60CC
    int m_videoRunning;                          // +0x60D0
    char m_unknown60D4[8];                       // +0x60D4
    AppPart<AppSceneLayer, 4> m_scene;           // +0x60DC
    int m_sceneRunning;                          // +0x60E0
    char m_unknown60E4[0x274];                   // +0x60E4 +0x60E8 is handed on
    AppPart<AppUserInterfaceLayer, 0xF4> m_userInterface;  // +0x6358
    int m_userInterfaceRunning;                  // +0x644C
    AppPart<AppAtlasLayer, 4> m_atlas;           // +0x6450
    int m_atlasRunning;                          // +0x6454 a second Init shuts down first
    AppViewport* m_viewport;                     // +0x6458
    int m_noMusic;                               // +0x645C the eight audio words the settings write, through s_gmApp
    int m_noAmbientAudio;                        // +0x6460
    int m_noAudio;                               // +0x6464
    int m_displayBanks;                          // +0x6468
    int m_displaySounds;                         // +0x646C
    int m_displayMusic;                          // +0x6470
    int m_audioMode;                             // +0x6474
    int m_audioTest;                             // +0x6478
    int m_unknown647C;                           // +0x647C
    void* m_matrix;                              // +0x6480 set by 0x001416A0
    void* m_matrix6484;                          // +0x6484 set by 0x001416A0
    int m_initA;                                 // +0x6488
    int m_initB;                                 // +0x648C
    AtDatabaseLoader* m_dirtyDiskLoader;         // +0x6490
    AtHeap* m_heaps[4];                          // +0x6494
    HeapConfig m_heapConfig[4];                  // +0x64A4
    GmMoFramework* m_frameworks[4];              // +0x64C4 the fourth is never made, though the table can name it
    GmMoFramework* m_framework;                  // +0x64D4 the current one
    int m_frameworkTable[16];                    // +0x64D8 by the current framework's index, then its request
    int m_quit;                                  // +0x6518
    int m_screenWidth;                           // +0x651C set by 0x0008DBA0
    int m_screenHeight;                          // +0x6520
    int m_unknown6524;                           // +0x6524
    int m_unknown6528[8];                        // +0x6528
    int m_unknown6548;                           // +0x6548
    GmAppSettings m_settings;                    // +0x654C
    int m_unknown65B4;                           // +0x65B4 Init sets 1
    AppPart<FaAcPlayerGameStateData, 0x214> m_gameState;  // +0x65B8
    unsigned char m_saveData[0x120];             // +0x67CC
    int m_unknown68EC;                           // +0x68EC
    GmHistoryLogs* m_historyLogs;                // +0x68F0
    int m_unknown68F4[8];                        // +0x68F4
    AtIoMountManager* m_mountManager;            // +0x6914
    int m_unknown6918[5];                        // +0x6918
    AppUnknown692C* m_unknown692C;               // +0x692C
    int m_unknown6930;                           // +0x6930
    int m_unknown6934[7];                        // +0x6934
    AtDataRegistry m_genericRegistry;            // +0x6950 Design\Registry\Generic.ADR
    AtDataLibrary m_library;                     // +0x6B40 Library.ADD
    AtDataRegistry m_registry6B54;               // +0x6B54
    int m_previousFramework;                     // +0x6D44
    GmLevelManifest* m_manifest;                 // +0x6D48 PERSIST.MNG

    static App* s_instance;                      // 0x00304D58 set by Init
    static App* s_gmApp;                         // 0x003B360C set by GmApp's start-up (0x00141500), read by 0x001414F0; the settings write its audio words
    static App* GetGmApp();                      // 0x001414F0 s_gmApp. src/gm/GmSoundPlay.cpp (gm/gmsound)
    static AtHeap* s_heap;                       // 0x00304D6C the first heap, copied by Init
    static unsigned int s_heapMemory;            // 0x00304D68 what the four heaps take: 0x2280000
    static unsigned int s_memoryTotal;           // 0x003B3BE0
    static unsigned int s_memoryOverhead;        // 0x003B3BDC
    static FaAcPlayerGameStateData* s_gameState; // 0x003B3530
};

// The game's main, 0x00011030. The C library calls it as main with (0, 0, 0); the third argument is not read. Returns 1. In src/gm/GmMain.cpp, which only a
// program links, since a library cannot hold a main.
int AppMain(unsigned int argc, char** argv, char** envp);
extern App g_app;                                // 0x002BF310

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtSdkMemoryStatus) == 0x20, "AtSdkMemoryStatus");
static_assert(sizeof(AppAtlasConfig) == 0x30, "AppAtlasConfig");
static_assert(offsetof(AppDevice, m_flags) == 0xC, "AppDevice::m_flags");
static_assert(offsetof(GmMoFramework, m_request) == 0xC, "GmMoFramework::m_request");
static_assert(sizeof(AppLogParams) == 0x20, "AppLogParams");
static_assert(offsetof(GmClock, m_frame) == 0x1C, "GmClock::m_frame");
static_assert(offsetof(GmDataRoot, m_registry) == 0x2C, "GmDataRoot::m_registry");
static_assert(offsetof(GmRegistryUser, m_root) == 0x44, "GmRegistryUser::m_root");
static_assert(sizeof(GmSaveType) == 0x14, "GmSaveType");
static_assert(offsetof(GmSaveTypes, m_flags) == 0x70, "GmSaveTypes::m_flags");
static_assert(offsetof(App, m_memory) == 0x004, "App::m_memory");
static_assert(offsetof(App, m_events) == 0x268, "App::m_events");
static_assert(offsetof(App, m_network) == 0x2F0, "App::m_network");
static_assert(offsetof(App, m_io) == 0x310, "App::m_io");
static_assert(offsetof(App, m_io.m_factory) == 0x318, "App::m_io.m_factory");
static_assert(offsetof(App, m_io.m_context) == 0x31C, "App::m_io.m_context");
static_assert(offsetof(App, m_mathRunning) == 0x388, "App::m_mathRunning");
static_assert(offsetof(App, m_dataManagement) == 0x390, "App::m_dataManagement");
static_assert(offsetof(App, m_dataManagementRunning) == 0x410, "App::m_dataManagementRunning");
static_assert(offsetof(App, m_scriptRunning) == 0x418, "App::m_scriptRunning");
static_assert(offsetof(App, m_kinematics) == 0x5F70, "App::m_kinematics");
static_assert(offsetof(App, m_kinematicsRunning) == 0x5F78, "App::m_kinematicsRunning");
static_assert(offsetof(App, m_controller) == 0x6020, "App::m_controller");
static_assert(offsetof(App, m_controllerRunning) == 0x60A4, "App::m_controllerRunning");
static_assert(offsetof(App, m_soundFlags) == 0x60B4, "App::m_soundFlags");
static_assert(offsetof(App, m_videoRunning) == 0x60D0, "App::m_videoRunning");
static_assert(offsetof(App, m_scene) == 0x60DC, "App::m_scene");
static_assert(offsetof(App, m_sceneRunning) == 0x60E0, "App::m_sceneRunning");
static_assert(offsetof(App, m_userInterfaceRunning) == 0x644C, "App::m_userInterfaceRunning");
static_assert(sizeof(AppDataManagementConfig) == 0x34, "AppDataManagementConfig");
static_assert(sizeof(AppKinematicsConfig) == 0x34, "AppKinematicsConfig");
static_assert(sizeof(AppGraphicsConfig) == 0xF0, "AppGraphicsConfig");
static_assert(sizeof(AppControllerConfig) == 0x30, "AppControllerConfig");
static_assert(sizeof(AppSoundConfig) == 0x30, "AppSoundConfig");
static_assert(sizeof(AppSceneConfig) == 0x8C, "AppSceneConfig");
static_assert(sizeof(AppUserInterfaceConfig) == 0x68, "AppUserInterfaceConfig");
static_assert(offsetof(App, m_device) == 0x5F7C, "App::m_device");
static_assert(offsetof(App, m_graphics) == 0x5F90, "App::m_graphics");
static_assert(offsetof(App, m_materialOwner) == 0x5F98, "App::m_materialOwner");
static_assert(offsetof(App, m_userInterface) == 0x6358, "App::m_userInterface");
static_assert(offsetof(App, m_atlas) == 0x6450, "App::m_atlas");
static_assert(offsetof(App, m_noMusic) == 0x645C, "App::m_noMusic");
static_assert(offsetof(App, m_audioTest) == 0x6478, "App::m_audioTest");
static_assert(offsetof(App, m_initA) == 0x6488, "App::m_initA");
static_assert(offsetof(App, m_heaps) == 0x6494, "App::m_heaps");
static_assert(offsetof(App, m_heapConfig) == 0x64A4, "App::m_heapConfig");
static_assert(offsetof(App, m_frameworks) == 0x64C4, "App::m_frameworks");
static_assert(offsetof(App, m_frameworkTable) == 0x64D8, "App::m_frameworkTable");
static_assert(offsetof(App, m_quit) == 0x6518, "App::m_quit");
static_assert(offsetof(App, m_unknown6548) == 0x6548, "App::m_unknown6548");
static_assert(offsetof(App, m_settings) == 0x654C, "App::m_settings");
static_assert(offsetof(App, m_gameState) == 0x65B8, "App::m_gameState");
static_assert(offsetof(App, m_saveData) == 0x67CC, "App::m_saveData");
static_assert(offsetof(App, m_historyLogs) == 0x68F0, "App::m_historyLogs");
static_assert(offsetof(App, m_mountManager) == 0x6914, "App::m_mountManager");
static_assert(offsetof(App, m_unknown692C) == 0x692C, "App::m_unknown692C");
static_assert(offsetof(App, m_genericRegistry) == 0x6950, "App::m_genericRegistry");
static_assert(offsetof(App, m_library) == 0x6B40, "App::m_library");
static_assert(offsetof(App, m_registry6B54) == 0x6B54, "App::m_registry6B54");
static_assert(offsetof(App, m_manifest) == 0x6D48, "App::m_manifest");
static_assert(sizeof(App) == 0x6D4C, "App");
#pragma pop_macro("offsetof")
#endif
