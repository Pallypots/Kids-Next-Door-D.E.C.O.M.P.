// What a level loads, read from a .MNG file: LOADER.MNG under each DESIGN\LOCATION\<level>\, PERSIST.MNG at start-up, UI.MNG and one to a playable character. Each word names a kind of
// database, followed by a counted list of paths; the parser makes one item for each path, and the time-sliced loader (GmManifestLoader) runs the items one at a time. All class and member names
// are inferred; the words are the original's, from the keyword tables at 0x002B9350 and 0x002B9200.
//
// Two classes, one built on the other, and only ever used together (0x0015CD10 is called only from 0x001428B3):
//   GmDatabaseManifest  0x0015CD10, vftable 0x0028F340. Seven words: SoundDatabaseLoaders, GraphicsDatabasesLoaders, SceneDatabasesLoaders, CinemaDatabaseLoaders, BoundingGeometryDatabase,
//                       EventDatabases, InstanceRegistries. Its word parser 0x0015D000, its Finished 0x0015D920 and its seven kinds of item are gm/dbmanifest's:
//                       include/gm/GmDatabaseManifestItems.h, src/gm/GmDatabaseManifest.cpp.
//   GmLevelManifest     0x001428B0, vftable 0x0028DFB8. Twelve more words, each with its kind of item, all recovered here except the sound notes (.GON), which are gm/gon's (include/gm/GmSoundNotes.h).
// Start-up makes a GmLevelManifest at App+0x6D48 (0x0008EA81) for PERSIST.MNG; GmLevelManager holds another at +0x64 (its constructor, 0x0013E176). Original address clusters
// 0x00142110-0x001431FF less 0x00142220-0x001425FF's deleting destructors (recorded with their classes), 0x0015C860-0x0015C8F4, 0x0015CBA0-0x0015CFFF, 0x0015D9E0-0x0015D9FB,
// 0x001B2BB0-0x001B3B4F less 0x001B3540-0x001B36CF.
#pragma once
#include "at/AtIoTokenizer.h"
#include "at/AtDataRegistry.h"
#include "at/AtScene.h"
#include "gm/GmManifestLoader.h"
#include "gm/GmSoundNotes.h"

// ---- Outside this subsystem: what the items load into. NOT recovered; every function below stops with a diagnostic in src/gm/GmLevelManifestHost.cpp. ---------------------------------------

class GmBehaviorList {
public:
    void Prepare();                          // 0x00198C90 called on each list as soon as it is loaded
};

// The owner of behaviour lists. Two instances, for animations and for sounds; one Release serves both, so one class is likely.
class GmBehaviorListOwner {
public:
    GmBehaviorList* LoadAnimations(const char* path);   // 0x0019C5E0
    GmBehaviorList* LoadSounds(const char* path);       // 0x0019C100
    void Release(GmBehaviorList* list);                 // 0x00198E70
};

class GmScript;
class GmScriptOwner {
public:
    GmScript* Load(const char* path);        // 0x00070310
    void Release(GmScript* script);          // 0x00070150
};

// The object at 0x003B35D4: only its three owners are known.
struct GmBehaviorSystem {
    char m_unknown00[0x10];
    GmBehaviorListOwner* m_animationLists;   // +0x10 BehaviorAnimationLists
    GmBehaviorListOwner* m_soundLists;       // +0x14 BehaviorSoundLists
    GmScriptOwner* m_scripts;                // +0x18 SmallCScripts

    static GmBehaviorSystem* s_instance;     // 0x003B35D4
};

// What an AET or AES item holds in place, 0x24 bytes with a table of its own. The two are different classes of the same shape.
class GmTriggerTable {
public:
    GmTriggerTable();                        // 0x001963B0
    ~GmTriggerTable();                       // 0x00196480
    virtual void Slot0();
    virtual void Unload();                   // slot 1
    void Load(const char* path);             // 0x00196500

    char m_unknown04[0x20];
};

class GmSubscriberTable {
public:
    GmSubscriberTable();                     // 0x001968B0
    ~GmSubscriberTable();                    // 0x00196980
    virtual void Slot0();
    virtual void Unload();                   // slot 1
    void Load(const char* path);             // 0x00196A00

    char m_unknown04[0x20];
};

class GmVfxDatabase;
class GmVfxManager {
public:
    GmVfxDatabase* Load(const char* path);   // 0x001616A0
    // 0x000EB9D0, folded with AtIoDeleteObject: deletes the database through its slot 0 if it is not null. A seam only because GmVfxDatabase is not recovered.
    void Delete(GmVfxDatabase* database);

    static GmVfxManager* s_instance;         // 0x003B3C28
};

// What loads a dynamic node's scene. 0x003B3984.
class GmDynamicNodeLoader {
public:
    void Load(const char* path, int unused, AtScene* scene);  // 0x0014FF60 the second argument is 0 from here
    void Unload(AtScene* scene, int flag);                     // 0x00150020

    static GmDynamicNodeLoader* s_instance;  // 0x003B3984
};

// 0x003B3604: only the space graph a dynamic node's scene enters is known.
struct GmSpaceGraphOwner {
    void SetGraph(AtSpaceGraph* graph);      // 0x00140E40 NOT recovered (gm/dbmanifest seam): sets m_graph and two other objects' copies of it

    char m_unknown00[0x28];
    AtSpaceGraph* m_graph;                   // +0x28

    static GmSpaceGraphOwner* s_instance;    // 0x003B3604
};

// 0x003B357C: two words that decide the flag GmDynamicNodeLoader::Unload is given, and what InstanceRegistries hands its items (gm/dbmanifest).
struct GmDataOwners;
struct GmDynamicNodeState {
    int m_unknown00;
    GmDataOwners* m_dataOwners;              // +0x04 its +0x394 is the registry owner (include/gm/GmDatabaseManifestItems.h)
    char m_unknown08[0x1C];
    int m_unknown24;                         // +0x24
    int m_unknown28;                         // +0x28
    AtDataRegistry m_registry;               // +0x2C each instance registry's parent

    static GmDynamicNodeState* s_instance;   // 0x003B357C
};

// The sound manager's work on the sound items: members of GmAudioData (0x003B354C) that neither this work nor gm/gon recovered or declared, so
// left as free functions, seams in src/gm/GmLevelManifestHost.cpp. Each takes the manager from its static, as the original does at each call. Bank and data types are not recovered.
void* GmAudioAllocateNodeBank();                     // 0x001367E0 the first free, or null; as AllocateNoteBank is for notes
void GmAudioLoadNodes(void* bank, const char* path); // 0x0013AA80 the .GOO file
void GmAudioFreeNodes(void* bank);                   // 0x00138E50
void GmAudioReleaseNodeBank(void* bank);             // 0x00136810
void GmAudioRegisterNodes(void* bank);               // 0x00138A10 once everything has run; given null for an item that got no bank
void* GmAudioLoadInstanceSounds(const char* path);   // 0x00139C40 the .GOI file
void GmAudioFreeInstanceSounds(void* data);          // 0x00136B30
void GmAudioRegisterInstanceSounds(void* data);      // 0x00136A10
void* GmAudioLoadCollisionSounds(const char* path);  // 0x00139610 the .GOC file
void GmAudioFreeCollisionSounds(void* data);         // 0x00139B50

class GmMusicChain {
public:
    void Load(const char* path, int unused); // 0x0013D2A0 the second argument is 0 from here
    void Clear();                            // 0x0013CFF0
};

// 0x003B3568.
class GmMusicManager {
public:
    GmMusicChain* Allocate();                // 0x0013D0C0 null when none is free
    void Stop(GmMusicChain* chain);          // 0x0013D3A0
    void Free(GmMusicChain* chain);          // 0x0013D360

    static GmMusicManager* s_instance;       // 0x003B3568
};

// ---- The items --------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// What the parser hands each item's Configure: one buffer on its stack, reused for every path of a word.
struct GmManifestItemRequest {
    char m_unused[4];                        // +0x00 its first byte is cleared at each word, and nothing reads it
    char m_path[0x80];                       // +0x04 as read; a longer path runs into what follows
    void* m_context;                         // +0x84 what the kind of item needs: an owner, or a manager
    void* m_context2;                        // +0x88 DynamicNodes only
};

// BehaviorAnimationLists. Vftable 0x00295040, 0x94 bytes.
class GmAnimationListItem : public GmManifestItem {
public:
    GmAnimationListItem();                   // 0x001B2C40
    virtual ~GmAnimationListItem();          // slot 2; 0x001421C0 deleting, for arrays too; 0x001B2C80 complete
    void Unknown0() override;                // slot 0, 0x001B2D40 forgets the list without giving it back. Folded with the next two kinds'
    void Run() override;                     // slot 1, 0x001B2BB0
    void Configure(const GmManifestItemRequest* request);  // 0x001B2C00 ANL

    GmBehaviorListOwner* m_owner;            // +0x8C
    GmBehaviorList* m_list;                  // +0x90
};

// BehaviorSoundLists. Vftable 0x0029504C, 0x94 bytes.
class GmSoundListItem : public GmManifestItem {
public:
    GmSoundListItem();                       // 0x001B2D60
    virtual ~GmSoundListItem();              // slot 2; 0x00142220 deleting, for arrays too; 0x001B2DA0 complete
    void Unknown0() override;                // slot 0, 0x001B2D40 (folded)
    void Run() override;                     // slot 1, 0x001B2CB0
    void Configure(const GmManifestItemRequest* request);  // 0x001B2D00 SDL

    GmBehaviorListOwner* m_owner;            // +0x8C
    GmBehaviorList* m_list;                  // +0x90
};

// SmallCScripts. Vftable 0x00295058, 0x94 bytes.
class GmScriptItem : public GmManifestItem {
public:
    GmScriptItem();                          // 0x001B2E50
    virtual ~GmScriptItem();                 // slot 2; 0x00142280 deleting, for arrays too; 0x001B2E90 complete
    void Unknown0() override;                // slot 0, 0x001B2D40 (folded)
    void Run() override;                     // slot 1, 0x001B2DD0
    void Configure(const GmManifestItemRequest* request);  // 0x001B2E10 ASL

    GmScriptOwner* m_owner;                  // +0x8C
    GmScript* m_script;                      // +0x90
};

// TriggerDatabases. Vftable 0x00295068, 0xB0 bytes.
class GmTriggerItem : public GmManifestItem {
public:
    GmTriggerItem();                         // 0x001B2F70
    virtual ~GmTriggerItem();                // slot 2; 0x001422E0 deleting, for arrays too; 0x001B2FB0 complete
    void Unknown0() override;                // slot 0, 0x001B2F40 unloads the table. Folded with the next kind's
    void Run() override;                     // slot 1, 0x001B2EC0
    void Configure(const GmManifestItemRequest* request);  // 0x001B2F00 AET

    GmTriggerTable m_table;                  // +0x8C
};

// SubscriberDatabases. Vftable 0x00295078, 0xB0 bytes.
class GmSubscriberItem : public GmManifestItem {
public:
    GmSubscriberItem();                      // 0x001B30A0
    virtual ~GmSubscriberItem();             // slot 2; 0x00142340 deleting, for arrays too; 0x001B30E0 complete
    void Unknown0() override;                // slot 0, 0x001B2F40 (folded)
    void Run() override;                     // slot 1, 0x001B3020
    void Configure(const GmManifestItemRequest* request);  // 0x001B3060 AES

    GmSubscriberTable m_table;               // +0x8C
};

// VFXDatabases. Vftable 0x00295088, 0x94 bytes.
class GmVfxItem : public GmManifestItem {
public:
    GmVfxItem();                             // 0x001B31F0
    virtual ~GmVfxItem();                    // slot 2; 0x001423A0 deleting, for arrays too; 0x001B3230 complete
    void Unknown0() override;                // slot 0, 0x001B31D0 forgets the database without deleting it
    void Run() override;                     // slot 1, 0x001B3150
    void Configure(const GmManifestItemRequest* request);  // 0x001B3190 VFX: the path keeps its own .VFX

    GmVfxManager* m_manager;                 // +0x8C
    GmVfxDatabase* m_database;               // +0x90
};

// DynamicNodes. Vftable 0x00295094, 0x198 bytes. No real file has the word.
class GmDynamicNodeItem : public GmManifestItem {
public:
    GmDynamicNodeItem();                     // 0x001B3340
    virtual ~GmDynamicNodeItem();            // slot 2; 0x00142400 deleting, for arrays too; 0x001B3390 complete
    void Unknown0() override;                // slot 0, 0x001B3320
    void Run() override;                     // slot 1, 0x001B3260
    void Configure(const GmManifestItemRequest* request);  // 0x001B32D0 DAC

    AtScene m_scene;                         // +0x8C
    char m_unknown184[0xC];                  // +0x184 nothing here touches it, nor any AtScene function read: AtScene is 0xF8 bytes
    AtSpaceGraph* m_graph;                   // +0x190
    GmDynamicNodeLoader* m_loader;           // +0x194
};

// SoundNodeDatabases. Vftable 0x002950A0, 0x94 bytes.
class GmSoundNodeItem : public GmManifestItem {
public:
    GmSoundNodeItem();                       // 0x001B34F0
    virtual ~GmSoundNodeItem();              // slot 2; 0x00142460 deleting, for arrays too; 0x001B3530 complete
    void Unknown0() override;                // slot 0, 0x001B3410 gives the bank back, if it was loaded
    void Run() override;                     // slot 1, 0x001B3470 once only
    void Configure(const GmManifestItemRequest* request);  // 0x001B33D0 GOO

    int m_loaded;                            // +0x8C
    void* m_bank;                            // +0x90
};

// MusicChainDatabases. Vftable 0x002950B8, 0x90 bytes.
class GmMusicChainItem : public GmManifestItem {
public:
    GmMusicChainItem();                      // 0x001B37E0
    virtual ~GmMusicChainItem();             // slot 2; 0x00142520 deleting, for arrays too; 0x001B3820 complete
    void Unknown0() override;                // slot 0, 0x001B3710
    void Run() override;                     // slot 1, 0x001B3770 a second run loses the first chain
    void Configure(const GmManifestItemRequest* request);  // 0x001B36D0 GOM

    GmMusicChain* m_chain;                   // +0x8C
};

// InstanceSoundNodeDatabases. Vftable 0x002950C4, four slots, 0x94 bytes. No real file has the word.
class GmInstanceSoundItem : public GmManifestItem {
public:
    GmInstanceSoundItem();                   // 0x001B3940
    virtual ~GmInstanceSoundItem();          // slot 2; 0x00142580 deleting, for arrays too; 0x001B3980 complete
    void Unknown0() override;                // slot 0, 0x001B3870
    void Run() override;                     // slot 1, 0x001B38C0 once only
    virtual void Register();                 // slot 3, 0x001B3920 from GmLevelManifest::Finished
    void Configure(const GmManifestItemRequest* request);  // 0x001B3830 GOI

    int m_loaded;                            // +0x8C
    void* m_data;                            // +0x90
};

// CollisionSoundNodeDatabases. Vftable 0x002950D4, 0x94 bytes. No real file has the word.
class GmCollisionSoundItem : public GmManifestItem {
public:
    GmCollisionSoundItem();                  // 0x001B3AC0
    virtual ~GmCollisionSoundItem();         // slot 2; 0x001425E0 deleting, for arrays too; 0x001B3B00 complete
    void Unknown0() override;                // slot 0, 0x001B3A10
    void Run() override;                     // slot 1, 0x001B3A60 once only
    void Configure(const GmManifestItemRequest* request);  // 0x001B39D0 GOC

    int m_loaded;                            // +0x8C
    void* m_data;                            // +0x90
};

// SoundNoteDatabases: GmSoundNoteItem, vftable 0x002950AC, 0x90 bytes, in include/gm/GmSoundNotes.h. Its Request is the first 0x84 bytes of GmManifestItemRequest.

// ---- The base's own items, as GmDatabaseManifest::Free sees them. Their sizes are those new[] is given in 0x0015D000; the classes are in include/gm/GmDatabaseManifestItems.h. ---------

enum GmDatabaseItemKind { kEventItems, kRegistryItems, kSoundDatabaseItems, kBoundingGeometryItems, kGraphicsDatabaseItems, kSceneDatabaseItems, kCinemaDatabaseItems };
enum {
    kEventItemSize = 0xB8, kRegistryItemSize = 0x288, kSoundDatabaseItemSize = 0x9C, kBoundingGeometryItemSize = 0xBC, kGraphicsDatabaseItemSize = 0x98, kSceneDatabaseItemSize = 0x10C,
    kCinemaDatabaseItemSize = 0xB8
};
void GmDatabaseItemsDelete(int kind, GmManifestItem* first);   // the original's delete[] through the first item's slot 2, with flags 3; by kind in src/gm/GmDatabaseManifest.cpp
void GmAudioFreeDatabase(void* database);                       // GmAudioData 0x00139F90, on a SoundDatabaseLoaders item's +0x90. NOT recovered: a seam
void GmSceneItemLeave(void* part, AtSpaceGraph* graph);         // AtSceneDatabase::RemoveFrom 0x0002EDD0 on a SceneDatabasesLoaders item's +0x8C, when its +0x108 is set

// ---- The loaders ------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Vftable 0x0028F340: Parse, AfterParse, OnToken, Finished, destructor, Clear, Free.
class GmDatabaseManifest : public GmManifestLoader {
public:
    // Keyword ids, the original's.
    enum Word {
        kEventDatabases = 0x39, kInstanceRegistries = 0x3A, kSoundDatabaseLoaders = 0x3B, kGraphicsDatabasesLoaders = 0x3C, kBoundingGeometryDatabase = 0x3D,
        kSceneDatabasesLoaders = 0x3E, kCinemaDatabaseLoaders = 0x3F, kInSpaceGraph = 0x40, kNotInSpaceGraph = 0x41, kInsertInSnoopie = 0x42
    };
    enum { kKeywordCount = 10 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B9350

    GmDatabaseManifest();                    // 0x0015CD10
    ~GmDatabaseManifest() override;          // slot 4; 0x0015D9E0 deleting, 0x0015CD30 complete
    void Parse(AtIoTokenizer* tokenizer) override;               // slot 0, 0x0015C8E0 installs the ten words
    void AfterParse() override;                                  // slot 1, 0x0015CBA0 queues the seven arrays
    void OnToken(int token, AtIoTokenizer* tokenizer) override;  // slot 2, 0x0015D000 the seven words; any other token goes by. src/gm/GmDatabaseManifest.cpp
    void Finished() override;                                    // slot 3, 0x0015D920 the scenes into the first space graph found. src/gm/GmDatabaseManifest.cpp
    virtual void Clear();                    // slot 5, 0x0015C8C0 frees, then resets
    virtual void Free();                     // slot 6, 0x0015CD90

    void ClearDatabaseArrays();              // 0x0015C860

    int m_unknown098;                        // +0x98 cleared, and not otherwise read here
    GmManifestItem* m_events;                // +0x9C EventDatabases
    GmManifestItem* m_registries;            // +0xA0 InstanceRegistries
    GmManifestItem* m_soundDatabases;        // +0xA4 SoundDatabaseLoaders: each runs as soon as it is configured, during the parse
    GmManifestItem* m_boundingGeometry;      // +0xA8 BoundingGeometryDatabase
    GmManifestItem* m_graphicsDatabases;     // +0xAC GraphicsDatabasesLoaders
    GmManifestItem* m_sceneDatabases;        // +0xB0 SceneDatabasesLoaders
    GmManifestItem* m_cinemaDatabases;       // +0xB4 CinemaDatabaseLoaders
    unsigned int m_soundDatabaseCount;       // +0xB8
    unsigned int m_graphicsDatabaseCount;    // +0xBC
    unsigned int m_sceneDatabaseCount;       // +0xC0
    unsigned int m_cinemaDatabaseCount;      // +0xC4
    unsigned int m_boundingGeometryCount;    // +0xC8
    unsigned int m_eventCount;               // +0xCC
    unsigned int m_registryCount;            // +0xD0
    int m_unknownD4[2];                      // +0xD4 not written by either class's clear

    static void* s_heap;                     // 0x003B3610 what both classes' item arrays come from
};

// Vftable 0x0028DFB8.
class GmLevelManifest : public GmDatabaseManifest {
public:
    enum Word {
        kBehaviorAnimationLists = 0x43, kBehaviorSoundLists = 0x44, kSmallCScripts = 0x45, kTriggerDatabases = 0x46, kSubscriberDatabases = 0x47, kVFXDatabases = 0x48,
        kDynamicNodes = 0x49, kVideos = 0x4A, kStreaming = 0x4B, kTexture = 0x4C, kNode = 0x4D, kSoundNodeDatabases = 0x4E, kSoundNoteDatabases = 0x4F, kMusicChainDatabases = 0x50,
        kCollisionSoundNodeDatabases = 0x51, kInstanceSoundNodeDatabases = 0x52
    };
    enum { kKeywordCount = 16 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B9200. Videos, Streaming, Texture and Node are words here, and go to the base, which lets them by

    GmLevelManifest();                       // 0x001428B0
    ~GmLevelManifest() override;             // slot 4; 0x00143DB0 deleting, 0x001428D0 complete
    void Parse(AtIoTokenizer* tokenizer) override;               // slot 0, 0x00142E00 the base's words, then these
    void AfterParse() override;                                  // slot 1, 0x00142640 the base's arrays, then these
    void OnToken(int token, AtIoTokenizer* tokenizer) override;  // slot 2, 0x00142E90 a word not its own goes to the base
    void Finished() override;                                    // slot 3, 0x00143CE0
    void Clear() override;                   // slot 5, 0x001421B0
    void Free() override;                    // slot 6, 0x00142930 then the base's

    void ClearLevelArrays();                 // 0x00142110

    GmAnimationListItem* m_animationLists;   // +0xDC
    GmSoundListItem* m_soundLists;           // +0xE0
    GmScriptItem* m_scripts;                 // +0xE4
    GmVfxItem* m_vfx;                        // +0xE8
    GmTriggerItem* m_triggers;               // +0xEC
    GmSubscriberItem* m_subscribers;         // +0xF0
    GmDynamicNodeItem* m_dynamicNodes;       // +0xF4
    GmSoundNoteItem* m_soundNotes;           // +0xF8
    GmMusicChainItem* m_musicChains;         // +0xFC
    GmSoundNodeItem* m_soundNodes;           // +0x100
    GmCollisionSoundItem* m_collisionSounds; // +0x104
    GmInstanceSoundItem* m_instanceSounds;   // +0x108
    unsigned int m_animationListCount;       // +0x10C
    unsigned int m_soundListCount;           // +0x110
    unsigned int m_scriptCount;              // +0x114
    unsigned int m_vfxCount;                 // +0x118
    unsigned int m_triggerCount;             // +0x11C
    unsigned int m_subscriberCount;          // +0x120
    unsigned int m_dynamicNodeCount;         // +0x124
    unsigned int m_soundNodeCount;           // +0x128
    unsigned int m_soundNoteCount;           // +0x12C
    unsigned int m_musicChainCount;          // +0x130
    unsigned int m_collisionSoundCount;      // +0x134
    unsigned int m_instanceSoundCount;       // +0x138
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(GmManifestItemRequest) == 0x8C, "GmManifestItemRequest");
static_assert(sizeof(GmAnimationListItem) == 0x94, "GmAnimationListItem");
static_assert(sizeof(GmSoundListItem) == 0x94, "GmSoundListItem");
static_assert(sizeof(GmScriptItem) == 0x94, "GmScriptItem");
static_assert(sizeof(GmTriggerTable) == 0x24, "GmTriggerTable");
static_assert(sizeof(GmTriggerItem) == 0xB0, "GmTriggerItem");
static_assert(sizeof(GmSubscriberItem) == 0xB0, "GmSubscriberItem");
static_assert(sizeof(GmVfxItem) == 0x94, "GmVfxItem");
static_assert(offsetof(GmDynamicNodeItem, m_scene) == 0x8C, "GmDynamicNodeItem::m_scene");
static_assert(offsetof(GmDynamicNodeItem, m_graph) == 0x190, "GmDynamicNodeItem::m_graph");
static_assert(sizeof(GmDynamicNodeItem) == 0x198, "GmDynamicNodeItem");
static_assert(sizeof(GmSoundNodeItem) == 0x94, "GmSoundNodeItem");
static_assert(sizeof(GmMusicChainItem) == 0x90, "GmMusicChainItem");
static_assert(sizeof(GmInstanceSoundItem) == 0x94, "GmInstanceSoundItem");
static_assert(sizeof(GmCollisionSoundItem) == 0x94, "GmCollisionSoundItem");
static_assert(offsetof(GmBehaviorSystem, m_scripts) == 0x18, "GmBehaviorSystem::m_scripts");
static_assert(offsetof(GmDynamicNodeState, m_unknown24) == 0x24, "GmDynamicNodeState::m_unknown24");
static_assert(offsetof(GmDynamicNodeState, m_registry) == 0x2C, "GmDynamicNodeState::m_registry");
static_assert(offsetof(GmDatabaseManifest, m_unknown098) == 0x98, "GmDatabaseManifest::m_unknown098");
static_assert(offsetof(GmDatabaseManifest, m_registryCount) == 0xD0, "GmDatabaseManifest::m_registryCount");
static_assert(offsetof(GmLevelManifest, m_animationLists) == 0xDC, "GmLevelManifest::m_animationLists");
static_assert(offsetof(GmLevelManifest, m_instanceSounds) == 0x108, "GmLevelManifest::m_instanceSounds");
static_assert(offsetof(GmLevelManifest, m_animationListCount) == 0x10C, "GmLevelManifest::m_animationListCount");
static_assert(sizeof(GmLevelManifest) == 0x13C, "GmLevelManifest");
#pragma pop_macro("offsetof")
#endif
