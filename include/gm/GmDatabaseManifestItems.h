// The base class of the level manifest's own words and items: what GmDatabaseManifest (include/gm/GmLevelManifest.h) makes for SoundDatabaseLoaders, GraphicsDatabasesLoaders,
// SceneDatabasesLoaders, CinemaDatabaseLoaders, BoundingGeometryDatabase, EventDatabases and InstanceRegistries, and the three small database classes that its scene, cinema and event items
// hold in place, of which only the life cycle is recovered here (the event database's classes, life cycle included, are at/aee's: include/at/AtEvent.h). All class and member names are inferred; the extensions are the original's, from each Configure.
// Original address clusters 0x0015C900-0x0015CB96 (the items' deleting destructors, compiler-made), 0x0015D000-0x0015D9D1, 0x001B3FF0-0x001B4B6D, 0x0002ED40-0x0002EECF, 0x0002F470-0x0002F7E1,
// 0x000C5780-0x000C5B27, 0x0013D9D0-0x0013DA1F, 0x00137AD0.
#pragma once
#include "at/AtAudioDatabase.h"
#include "at/AtBoundingGeometry.h"
#include "at/AtDataRegistry.h"
#include "at/AtDatabaseLoader.h"
#include "at/AtEvent.h"
#include "at/AtNameTableOwner.h"
#include "at/AtSceneDatabaseLoader.h"
#include "at/AtSpaceGraph.h"
#include "gm/GmApp.h"
#include "gm/GmLevelManifest.h"
#include "gm/GmRegistryNames.h"

// ---- What the items' contexts come from: start-up's objects, not recovered beyond what is named here ---------------------------------------------------------------------------------------

// The object at 0x003B3550, which makes and frees the sound databases an .AOD file is read into. Its class, and the Xbox database it makes (AtSoXboxDatabase, through 0x000E9E90), are another
// branch's; only these two slots are used here.
class GmAudioDatabaseMaker {
public:
    virtual void Slot0() = 0;
    virtual void Slot1() = 0;
    virtual void Slot2() = 0;
    virtual void Slot3() = 0;
    virtual void Slot4() = 0;
    virtual AtAudioDatabase* NewDatabase() = 0;                // slot 5
    virtual void DeleteDatabase(AtAudioDatabase* database) = 0; // slot 6

    static GmAudioDatabaseMaker* s_instance;  // 0x003B3550
};

// What CinemaDatabaseLoaders hands its items. Only its address is taken here.
struct GmCinemaOwner {
    int m_unknown00;
};

// The object at 0x003B358C: only the owner at +0x08 is known, what EventDatabases hands its items. It is an AtDataRegistryOwner: AtEventDatabase::Read calls 0x000992D0 on it.
struct GmEventHome {
    int m_unknown00;
    int m_unknown04;
    AtDataRegistryOwner m_owner;               // +0x08

    static GmEventHome* s_instance;            // 0x003B358C
};

// The name map at 0x002CD138, which is told of each registry InstanceRegistries loads, is GmRegistryNames (gm/regnames, include/gm/GmRegistryNames.h).

class AtSceneDatabaseLoader;

// The language's directory, what a SoundDatabaseLoaders path's %s becomes. 0x003B3BE4; set by start-up, not recovered.
extern const char* g_audioLanguage;

// ---- What the scene and cinema items hold in place. Only their life cycle and the scene's walks are recovered; their loads are not. -----------------------------------------------

// What a cinema database's five arrays hold: classes not recovered, each deleted through its slot 0.
class AtCinemaDatabaseEntry {
public:
    virtual ~AtCinemaDatabaseEntry();          // slot 0, the vector deleting destructor of a class not recovered
};

// .ACD. Vftable 0x00286474, 0x28 bytes.
class AtCinemaDatabase {
public:
    AtCinemaDatabase();                        // 0x000C5780
    virtual ~AtCinemaDatabase();               // slot 0; 0x000C5AD0 deleting, for arrays too; 0x000C5A00 complete: clears whether loaded or not
    virtual void Clear();                      // slot 1, 0x000C57C0

    void Load(GmCinemaOwner* owner, const char* name, int unused);  // 0x000C5B30 NOT recovered: stops with a diagnostic

    int m_unknown04;                           // +0x04
    AtCinemaDatabaseEntry* m_entries08;        // +0x08
    AtCinemaDatabaseEntry* m_entries0C;        // +0x0C
    AtCinemaDatabaseEntry* m_entries10;        // +0x10
    AtCinemaDatabaseEntry* m_entries14;        // +0x14
    AtCinemaDatabaseEntry* m_entries18;        // +0x18
    unsigned short m_count1C;                  // +0x1C
    unsigned short m_count1E;                  // +0x1E
    unsigned short m_count20;                  // +0x20
    unsigned short m_count22;                  // +0x22
    unsigned short m_count24;                  // +0x24
};

// The event database an EventDatabases item holds in place, AtEventDatabase, with its list and events: include/at/AtEvent.h (at/aee).

// ---- The items --------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// What InstanceRegistries hands its items: GmManifestItemRequest with a third context. A local of its own in 0x0015D000.
struct GmRegistryItemRequest {
    char m_unused[4];                          // +0x00 its first byte is cleared at the word, and nothing reads it
    char m_path[0x80];                         // +0x04
    void* m_context;                           // +0x84 the registry manager
    void* m_context2;                          // +0x88 the parent registry
    void* m_context3;                          // +0x8C the registry owner
};

// SoundDatabaseLoaders. Vftable 0x002950E0, 0x9C bytes. Runs during the parse, as soon as it is configured.
class GmSoundDatabaseItem : public GmManifestItem {
public:
    GmSoundDatabaseItem();                     // 0x001B4150
    virtual ~GmSoundDatabaseItem();            // slot 2; 0x0015C900 deleting, for arrays too; 0x001B41A0 complete
    void Unknown0() override;                  // slot 0, 0x001B4070 gives the database back, if it has an owner
    void Run() override;                       // slot 1, 0x001B40C0 once only
    // 0x001B3FF0 AOD. The path is a format: its %s becomes the language's directory, copied as an eight-byte name.
    void Configure(const GmManifestItemRequest* request);

    int m_ran;                                 // +0x8C
    AtAudioDatabase* m_database;               // +0x90
    GmAudioDatabaseMaker* m_maker;             // +0x94
    AtNameTableOwner* m_owner;                 // +0x98
};

// GraphicsDatabasesLoaders. Vftable 0x002950EC, 0x98 bytes.
class GmGraphicsDatabaseItem : public GmManifestItem {
public:
    GmGraphicsDatabaseItem();                  // 0x001B42C0
    virtual ~GmGraphicsDatabaseItem();         // slot 2; 0x0015C960 deleting, for arrays too; 0x001B4300 complete
    void Unknown0() override;                  // slot 0, 0x001B4270 gives the loader back
    void Run() override;                       // slot 1, 0x001B41B0 the loader made is not tested for null
    void Configure(const GmManifestItemRequest* request);  // 0x001B4220 AGD

    AtDatabaseLoader* m_loader;                // +0x8C
    AppGraphics* m_maker;                      // +0x90
    AtMaterialOwner* m_owner;                  // +0x94
};

// SceneDatabasesLoaders. Vftable 0x002950F8, 0x10C bytes.
class GmSceneDatabaseItem : public GmManifestItem {
public:
    GmSceneDatabaseItem();                     // 0x001B4430
    virtual ~GmSceneDatabaseItem();            // slot 2; 0x0015C9C0 deleting, for arrays too; 0x001B4480 complete
    void Unknown0() override;                  // slot 0, 0x001B43F0
    void Run() override;                       // slot 1, 0x001B4350
    void Configure(const GmManifestItemRequest* request);  // 0x001B43A0 ASD

    AtSceneDatabaseLoader m_database;          // +0x8C
    AtNameTableOwner* m_owner;                 // +0x104
    int m_inSpaceGraph;                        // +0x108 InSpaceGraph after the path
};

// CinemaDatabaseLoaders. Vftable 0x00295104, 0xB8 bytes. No real file has the word.
class GmCinemaDatabaseItem : public GmManifestItem {
public:
    GmCinemaDatabaseItem();                    // 0x001B45D0
    virtual ~GmCinemaDatabaseItem();           // slot 2; 0x0015CA20 deleting, for arrays too; 0x001B4620 complete
    void Unknown0() override;                  // slot 0, 0x001B45A0
    void Run() override;                       // slot 1, 0x001B4510
    void Configure(const GmManifestItemRequest* request);  // 0x001B4560 ACD

    AtCinemaDatabase m_database;               // +0x8C
    GmCinemaOwner* m_owner;                    // +0xB4
};

// BoundingGeometryDatabase. Vftable 0x00295110, 0xBC bytes. No real file has the word.
class GmBoundingGeometryItem : public GmManifestItem {
public:
    GmBoundingGeometryItem();                  // 0x001B4760
    virtual ~GmBoundingGeometryItem();         // slot 2; 0x0015CA80 deleting, for arrays too; 0x001B47B0 complete
    void Unknown0() override;                  // slot 0, 0x001B4730
    void Run() override;                       // slot 1, 0x001B46A0
    void Configure(const GmManifestItemRequest* request);  // 0x001B46F0 ASB

    AtBoundingGeometryDatabase m_database;     // +0x8C
    AtNameTableOwner* m_owner;                 // +0xB8
};

// EventDatabases. Vftable 0x0029511C, 0xB8 bytes.
class GmEventDatabaseItem : public GmManifestItem {
public:
    GmEventDatabaseItem();                     // 0x001B48F0 leaves the owner unset
    virtual ~GmEventDatabaseItem();            // slot 2; 0x0015CAE0 deleting, for arrays too; 0x001B4930 complete
    void Unknown0() override;                  // slot 0, 0x001B48C0 clears the path's first byte, and keeps the owner
    void Run() override;                       // slot 1, 0x001B4830
    void Configure(const GmManifestItemRequest* request);  // 0x001B4880 AEE

    AtDataRegistryOwner* m_owner;              // +0x8C
    AtEventDatabase m_database;                // +0x90
};

// InstanceRegistries. Vftable 0x00295128, 0x288 bytes.
class GmRegistryItem : public GmManifestItem {
public:
    GmRegistryItem();                          // 0x001B4AA0 leaves the manager unset
    virtual ~GmRegistryItem();                 // slot 2; 0x0015CB40 deleting, for arrays too; 0x001B4AF0 complete
    void Unknown0() override;                  // slot 0, 0x001B4A70 keeps the manager
    void Run() override;                       // slot 1, 0x001B49B0 loads, then hands the registry to the manager
    void Configure(const GmRegistryItemRequest* request);  // 0x001B4A10 ADR

    GmRegistryNames* m_manager;                // +0x8C
    AtDataRegistry m_registry;                 // +0x90
    AtDataRegistry* m_parent;                  // +0x280
    AtDataRegistryOwner* m_owner;              // +0x284
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtCinemaDatabase) == 0x28, "AtCinemaDatabase");
static_assert(sizeof(GmRegistryItemRequest) == 0x90, "GmRegistryItemRequest");
static_assert(sizeof(GmSoundDatabaseItem) == kSoundDatabaseItemSize, "GmSoundDatabaseItem");
static_assert(sizeof(GmGraphicsDatabaseItem) == kGraphicsDatabaseItemSize, "GmGraphicsDatabaseItem");
static_assert(offsetof(GmSceneDatabaseItem, m_owner) == 0x104, "GmSceneDatabaseItem::m_owner");
static_assert(offsetof(GmSceneDatabaseItem, m_inSpaceGraph) == 0x108, "GmSceneDatabaseItem::m_inSpaceGraph");
static_assert(sizeof(GmSceneDatabaseItem) == kSceneDatabaseItemSize, "GmSceneDatabaseItem");
static_assert(offsetof(GmCinemaDatabaseItem, m_owner) == 0xB4, "GmCinemaDatabaseItem::m_owner");
static_assert(sizeof(GmCinemaDatabaseItem) == kCinemaDatabaseItemSize, "GmCinemaDatabaseItem");
static_assert(offsetof(GmBoundingGeometryItem, m_owner) == 0xB8, "GmBoundingGeometryItem::m_owner");
static_assert(sizeof(GmBoundingGeometryItem) == kBoundingGeometryItemSize, "GmBoundingGeometryItem");
static_assert(offsetof(GmEventDatabaseItem, m_database) == 0x90, "GmEventDatabaseItem::m_database");
static_assert(sizeof(GmEventDatabaseItem) == kEventItemSize, "GmEventDatabaseItem");
static_assert(offsetof(GmRegistryItem, m_registry) == 0x90, "GmRegistryItem::m_registry");
static_assert(offsetof(GmRegistryItem, m_parent) == 0x280, "GmRegistryItem::m_parent");
static_assert(sizeof(GmRegistryItem) == kRegistryItemSize, "GmRegistryItem");
#pragma pop_macro("offsetof")
#endif
