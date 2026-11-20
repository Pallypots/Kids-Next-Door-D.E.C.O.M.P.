// GmDatabaseManifest's word parser and Finished, its seven kinds of item, and the life cycle of the three databases its scene, cinema and event items hold in place.
// See include/gm/GmDatabaseManifestItems.h. The rest of GmDatabaseManifest is in src/gm/GmLevelManifest.cpp. Verification status per function: config/functions.d/dbmanifest.toml.
// Each Run builds "%3s][%s" from the extension and path into a buffer and never uses it, as the level's items do; the registry's builds it after loading.
#include "gm/GmDatabaseManifestItems.h"
#include "gm/GmAppSettings.h"
#include "gm/GmAudioData.h"
#include "gm/GmLevelManager.h"
#include "at/AtMemory.h"

extern "C" {
char* strcpy(char* to, const char* from);
}
int AtFormat(char* out, const char* format, ...);  // the C library's sprintf, 0x00125559: src/at/AtIoFormatHost.cpp
void AtCopyName8(char* to, const char* from);     // 0x00013F70, src/gm/GmLevelManager.cpp

// The original's arrays of items come from new[]: a count in the word ahead of the first, which delete[] goes by, last first. Written out, since the heap here may answer null and new[] may not.
typedef decltype(sizeof(0)) GmDbSize;
struct GmDbPlacement {};
inline void* operator new(GmDbSize, void* where, GmDbPlacement) { return where; }
inline void operator delete(void*, void*, GmDbPlacement) {}

namespace {

template <class T> T* NewItems(unsigned int count) {
    GmDbSize* block = (GmDbSize*)operator new(count * sizeof(T) + sizeof(GmDbSize));
    if (block == 0) {
        return 0;
    }
    *block = count;
    T* first = (T*)(block + 1);
    for (unsigned int i = 0; i < count; i++) {
        new (first + i, GmDbPlacement()) T;
    }
    return first;
}

// The original's delete[] of an array of a class with a virtual destructor: with a count of 0 the block is freed; otherwise the first element's vector deleting destructor (slot 0 or 2)
// destroys them all, last first, and frees the block. The classes are known here, so it is done by type.
template <class T> void DeleteItems(T* first) {
    GmDbSize* block = (GmDbSize*)first - 1;
    for (GmDbSize i = *block; i > 0; i--) {
        first[i - 1].~T();
    }
    operator delete(block);
}

// The same for arrays of classes that are not recovered: through their own vector deleting destructor.
template <class T> void DeleteObjects(T* first) {
    if (first != 0) {
        GmDbSize* block = (GmDbSize*)first - 1;
        if (*block != 0) {
            delete[] first;
        } else {
            operator delete(block);
        }
    }
}

// What each word does around its new[].
template <class T> T* ReadItemArray(unsigned int count) {
    if (GmDatabaseManifest::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(GmDatabaseManifest::s_heap);
    T* items = NewItems<T>(count);
    AtPopHeap();
    if (items == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return items;
}

template <class T> T* As(GmManifestItem* first) {
    return static_cast<T*>(first);
}

}  // namespace

GmAudioDatabaseMaker* GmAudioDatabaseMaker::s_instance;
GmEventHome* GmEventHome::s_instance;
const char* g_audioLanguage;

// ---- GmDatabaseManifest -----------------------------------------------------------------------------------------------------------------------------------------------------------------

// Each word: "[ n ] {", then n paths, then "}". SoundDatabaseLoaders' paths are limited to 128 characters, the others' to 127; a path of 128 runs its terminator into the request's
// context. Each word's array replaces the last without freeing it.
void GmDatabaseManifest::OnToken(int token, AtIoTokenizer* tokenizer) {
    GmManifestItemRequest request;
    GmRegistryItemRequest registry;
    switch (token) {
    case kSoundDatabaseLoaders:
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        request.m_context = 0;
        request.m_context2 = 0;
        m_soundDatabaseCount = tokenizer->ExpectCount();
        m_soundDatabases = ReadItemArray<GmSoundDatabaseItem>(m_soundDatabaseCount);
        request.m_context2 = GmAudioData::s_soundOwner;
        request.m_context = GmAudioDatabaseMaker::s_instance;
        // Each runs at once: the sound databases are loaded while the file is still being read.
        for (unsigned int i = 0; i < m_soundDatabaseCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString(0x80));
            As<GmSoundDatabaseItem>(m_soundDatabases)[i].Configure(&request);
            As<GmSoundDatabaseItem>(m_soundDatabases)[i].Run();
        }
        break;
    case kGraphicsDatabasesLoaders:
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        request.m_context2 = 0;
        request.m_context = 0;
        m_graphicsDatabaseCount = tokenizer->ExpectCount();
        m_graphicsDatabases = ReadItemArray<GmGraphicsDatabaseItem>(m_graphicsDatabaseCount);
        request.m_context2 = GmHistoryLogs::s_instance->m_app->m_graphics;
        request.m_context = GmHistoryLogs::s_instance->m_app->m_materialOwner;
        for (unsigned int i = 0; i < m_graphicsDatabaseCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString(0x7F));
            As<GmGraphicsDatabaseItem>(m_graphicsDatabases)[i].Configure(&request);
        }
        break;
    case kSceneDatabasesLoaders:
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        request.m_context = 0;
        request.m_context2 = 0;
        m_sceneDatabaseCount = tokenizer->ExpectCount();
        m_sceneDatabases = ReadItemArray<GmSceneDatabaseItem>(m_sceneDatabaseCount);
        request.m_context = &GmHistoryLogs::s_instance->m_app->m_sceneOwner;
        // After each path, InSpaceGraph or NotInSpaceGraph, which is read whatever it is, then InsertInSnoopie, which is read only if it is there, and means nothing here.
        for (unsigned int i = 0; i < m_sceneDatabaseCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString(0x7F));
            request.m_context2 = (void*)(GmDbSize)(tokenizer->Next() == kInSpaceGraph ? 1 : 0);
            if (tokenizer->Next() != kInsertInSnoopie) {
                tokenizer->m_pushedBack = 1;
            }
            As<GmSceneDatabaseItem>(m_sceneDatabases)[i].Configure(&request);
        }
        break;
    case kCinemaDatabaseLoaders:
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        request.m_context = 0;
        m_cinemaDatabaseCount = tokenizer->ExpectCount();
        m_cinemaDatabases = ReadItemArray<GmCinemaDatabaseItem>(m_cinemaDatabaseCount);
        request.m_context = &GmHistoryLogs::s_instance->m_app->m_cinemaOwner;
        for (unsigned int i = 0; i < m_cinemaDatabaseCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString(0x7F));
            As<GmCinemaDatabaseItem>(m_cinemaDatabases)[i].Configure(&request);
        }
        break;
    case kBoundingGeometryDatabase:
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        request.m_context = 0;
        m_boundingGeometryCount = tokenizer->ExpectCount();
        m_boundingGeometry = ReadItemArray<GmBoundingGeometryItem>(m_boundingGeometryCount);
        request.m_context = &GmHistoryLogs::s_instance->m_app->m_sceneOwner;
        for (unsigned int i = 0; i < m_boundingGeometryCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString(0x7F));
            As<GmBoundingGeometryItem>(m_boundingGeometry)[i].Configure(&request);
        }
        break;
    case kEventDatabases:
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        request.m_context = 0;
        m_eventCount = tokenizer->ExpectCount();
        request.m_context = &GmEventHome::s_instance->m_owner;
        m_events = ReadItemArray<GmEventDatabaseItem>(m_eventCount);
        for (unsigned int i = 0; i < m_eventCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString(0x7F));
            As<GmEventDatabaseItem>(m_events)[i].Configure(&request);
        }
        break;
    case kInstanceRegistries:
        registry.m_unused[0] = 0;
        registry.m_path[0] = 0;
        registry.m_context = 0;
        registry.m_context2 = 0;
        registry.m_context3 = 0;
        m_registryCount = tokenizer->ExpectCount();
        m_registries = ReadItemArray<GmRegistryItem>(m_registryCount);
        registry.m_context2 = &GmDynamicNodeState::s_instance->m_registry;
        registry.m_context3 = &GmDynamicNodeState::s_instance->m_dataOwners->m_registryOwner;
        registry.m_context = GmRegistryNames::s_instance;
        for (unsigned int i = 0; i < m_registryCount; i++) {
            strcpy(registry.m_path, tokenizer->ExpectString(0x7F));
            As<GmRegistryItem>(m_registries)[i].Configure(&registry);
        }
        break;
    default:
        // Anything else goes by: the level's words that nothing handles, and DynamicNodes' closing brace.
        return;
    }
    tokenizer->ExpectRightCurly();
}

// Once everything has run: the first space graph any scene database holds becomes the current one, and the scenes marked InSpaceGraph go into the current graph, whichever it is.
void GmDatabaseManifest::Finished() {
    for (unsigned int i = 0; i < m_sceneDatabaseCount; i++) {
        AtSpaceGraph* graph = GmLevelManager::s_instance->FindSpaceGraph(&As<GmSceneDatabaseItem>(m_sceneDatabases)[i].m_database);
        if (graph != 0) {
            GmSpaceGraphOwner::s_instance->SetGraph(graph);
            break;
        }
    }
    for (unsigned int i = 0; i < m_sceneDatabaseCount; i++) {
        GmSceneDatabaseItem* item = &As<GmSceneDatabaseItem>(m_sceneDatabases)[i];
        if (item->m_inSpaceGraph != 0 && GmSpaceGraphOwner::s_instance->m_graph != 0) {
            item->m_database.InsertInto(GmSpaceGraphOwner::s_instance->m_graph);
        }
    }
}

// What GmDatabaseManifest::Free has inline for each array: the original's delete[], through the first item's slot 2.
void GmDatabaseItemsDelete(int kind, GmManifestItem* first) {
    switch (kind) {
    case kEventItems:
        DeleteItems(As<GmEventDatabaseItem>(first));
        break;
    case kRegistryItems:
        DeleteItems(As<GmRegistryItem>(first));
        break;
    case kSoundDatabaseItems:
        DeleteItems(As<GmSoundDatabaseItem>(first));
        break;
    case kBoundingGeometryItems:
        DeleteItems(As<GmBoundingGeometryItem>(first));
        break;
    case kGraphicsDatabaseItems:
        DeleteItems(As<GmGraphicsDatabaseItem>(first));
        break;
    case kSceneDatabaseItems:
        DeleteItems(As<GmSceneDatabaseItem>(first));
        break;
    default:
        DeleteItems(As<GmCinemaDatabaseItem>(first));
        break;
    }
}

void GmSceneItemLeave(void* part, AtSpaceGraph* graph) {
    ((AtSceneDatabaseLoader*)part)->RemoveFrom(graph);
}

AtSpaceGraph* GmLevelManager::FindSpaceGraph(AtSceneDatabaseLoader* database) {
    if (database->m_spaceGraphCount > 0) {
        return database->SpaceGraphAt(0);
    }
    const int nested = database->m_loaderCount;
    for (int i = 0; i < nested; i++) {
        AtSpaceGraph* graph = FindSpaceGraph(database->LoaderAt(i));
        if (graph != 0) {
            return graph;
        }
    }
    return 0;
}

void GmAudioData::RegisterDatabase(AtAudioDatabase* database) {
    for (unsigned int i = 0; i < kDatabases; i++) {
        if (m_databases[i] == 0) {
            m_databases[i] = database;
            return;
        }
    }
}

// ---- AOD --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmSoundDatabaseItem::GmSoundDatabaseItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    m_ran = 0;
    m_maker = 0;
    m_owner = 0;
    AtFormat(m_extension, "");
    m_database = 0;
}

GmSoundDatabaseItem::~GmSoundDatabaseItem() {
    GmSoundDatabaseItem::Unknown0();
}

void GmSoundDatabaseItem::Unknown0() {
    if (m_database != 0 && m_database->m_owner != 0) {
        m_maker->DeleteDatabase(m_database);
    }
    m_ran = 0;
    m_maker = 0;
    m_owner = 0;
    AtFormat(m_extension, "");
    m_database = 0;
}

void GmSoundDatabaseItem::Run() {
    if (m_ran != 0) {
        return;
    }
    if (m_maker != 0 && m_owner != 0) {
        m_database = m_maker->NewDatabase();
        if (m_database != 0) {
            char unused[0x200];
            AtFormat(unused, "%3s][%s", m_extension, m_path);
            m_database->Load(m_owner, m_path, 0);
            GmAudioData::s_instance->RegisterDatabase(m_database);
        }
    }
    m_ran = 1;
}

void GmSoundDatabaseItem::Configure(const GmManifestItemRequest* request) {
    Unknown0();
    char name[0x10];
    char language[0x10];
    AtCopyName8(name, g_audioLanguage);
    AtCopyName9(language, name);
    AtFormat(m_path, request->m_path, language);
    AtFormat(m_extension, "AOD");
    m_owner = (AtNameTableOwner*)request->m_context2;
    m_maker = (GmAudioDatabaseMaker*)request->m_context;
}

// ---- AGD --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmGraphicsDatabaseItem::GmGraphicsDatabaseItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    m_maker = 0;
    m_owner = 0;
    AtFormat(m_extension, "");
    m_loader = 0;
}

GmGraphicsDatabaseItem::~GmGraphicsDatabaseItem() {
    GmGraphicsDatabaseItem::Unknown0();
}

void GmGraphicsDatabaseItem::Unknown0() {
    if (m_loader != 0) {
        m_maker->DeleteLoader(m_loader);
    }
    m_maker = 0;
    m_owner = 0;
    AtFormat(m_extension, "");
    m_loader = 0;
}

void GmGraphicsDatabaseItem::Run() {
    const int mode = g_usePlatformBinary != 0 ? 2 : 0;
    m_loader = m_maker->NewLoader();
    char unused[0x200];
    AtFormat(unused, "%3s][%s", m_extension, m_path);
    m_loader->Load(m_owner, m_path, mode, 0, 1);
}

void GmGraphicsDatabaseItem::Configure(const GmManifestItemRequest* request) {
    Unknown0();
    strcpy(m_path, request->m_path);
    AtFormat(m_extension, "AGD");
    m_owner = (AtMaterialOwner*)request->m_context;
    m_maker = (AppGraphics*)request->m_context2;
}

// ---- ASD --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmSceneDatabaseItem::GmSceneDatabaseItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    m_owner = 0;
    m_inSpaceGraph = 0;
    AtFormat(m_extension, "");
}

GmSceneDatabaseItem::~GmSceneDatabaseItem() {
    GmSceneDatabaseItem::Unknown0();
}

void GmSceneDatabaseItem::Unknown0() {
    m_database.Clear();
    m_owner = 0;
    m_inSpaceGraph = 0;
    AtFormat(m_extension, "");
}

void GmSceneDatabaseItem::Run() {
    char unused[0x200];
    AtFormat(unused, "%3s][%s", m_extension, m_path);
    m_database.Load(m_owner, m_path, 0);
}

void GmSceneDatabaseItem::Configure(const GmManifestItemRequest* request) {
    Unknown0();
    strcpy(m_path, request->m_path);
    AtFormat(m_extension, "ASD");
    m_owner = (AtNameTableOwner*)request->m_context;
    m_inSpaceGraph = (int)(GmDbSize)request->m_context2;
}

// ---- ACD --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmCinemaDatabaseItem::GmCinemaDatabaseItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    m_owner = 0;
    AtFormat(m_extension, "");
}

GmCinemaDatabaseItem::~GmCinemaDatabaseItem() {
    GmCinemaDatabaseItem::Unknown0();
}

void GmCinemaDatabaseItem::Unknown0() {
    m_database.Clear();
    m_owner = 0;
    AtFormat(m_extension, "");
}

void GmCinemaDatabaseItem::Run() {
    char unused[0x200];
    AtFormat(unused, "%3s][%s", m_extension, m_path);
    m_database.Load(m_owner, m_path, 0);
}

void GmCinemaDatabaseItem::Configure(const GmManifestItemRequest* request) {
    Unknown0();
    strcpy(m_path, request->m_path);
    AtFormat(m_extension, "ACD");
    m_owner = (GmCinemaOwner*)request->m_context;
}

// ---- ASB --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmBoundingGeometryItem::GmBoundingGeometryItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    m_owner = 0;
    AtFormat(m_extension, "");
}

GmBoundingGeometryItem::~GmBoundingGeometryItem() {
    GmBoundingGeometryItem::Unknown0();
}

void GmBoundingGeometryItem::Unknown0() {
    m_database.Clear();
    m_owner = 0;
    AtFormat(m_extension, "");
}

void GmBoundingGeometryItem::Run() {
    char unused[0x200];
    AtFormat(unused, "%3s][%s", m_extension, m_path);
    m_database.Load(m_owner, m_path, 0);
}

void GmBoundingGeometryItem::Configure(const GmManifestItemRequest* request) {
    Unknown0();
    strcpy(m_path, request->m_path);
    AtFormat(m_extension, "ASB");
    m_owner = (AtNameTableOwner*)request->m_context;
}

// ---- AEE --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmEventDatabaseItem::GmEventDatabaseItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    m_path[0] = 0;
    AtFormat(m_extension, "");
}

GmEventDatabaseItem::~GmEventDatabaseItem() {
    GmEventDatabaseItem::Unknown0();
}

void GmEventDatabaseItem::Unknown0() {
    m_database.Clear();
    m_path[0] = 0;
    AtFormat(m_extension, "");
}

void GmEventDatabaseItem::Run() {
    char unused[0x200];
    AtFormat(unused, "%3s][%s", m_extension, m_path);
    m_database.Load(m_owner, m_path, 0);
}

void GmEventDatabaseItem::Configure(const GmManifestItemRequest* request) {
    Unknown0();
    AtFormat(m_extension, "AEE");
    m_owner = (AtDataRegistryOwner*)request->m_context;
    strcpy(m_path, request->m_path);
}

// ---- ADR --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmRegistryItem::GmRegistryItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    m_parent = 0;
    m_owner = 0;
    AtFormat(m_extension, "");
}

GmRegistryItem::~GmRegistryItem() {
    GmRegistryItem::Unknown0();
}

void GmRegistryItem::Unknown0() {
    m_registry.Clear();
    m_parent = 0;
    m_owner = 0;
    AtFormat(m_extension, "");
}

void GmRegistryItem::Run() {
    m_registry.Load(m_owner, m_path, 0, 0, m_parent);
    char unused[0x200];
    AtFormat(unused, "%3s][%s", m_extension, m_path);
    m_manager->Add(&m_registry);
}

void GmRegistryItem::Configure(const GmRegistryItemRequest* request) {
    Unknown0();
    strcpy(m_path, request->m_path);
    AtFormat(m_extension, "ADR");
    m_manager = (GmRegistryNames*)request->m_context;
    m_parent = (AtDataRegistry*)request->m_context2;
    m_owner = (AtDataRegistryOwner*)request->m_context3;
}

// ---- AtSceneDatabaseLoader: the two walks; the rest of the class is at/asd's (stand-ins in src/gm/GmDatabaseManifestAsd.cpp) ----------------------------------------------------------------

void AtSceneDatabaseLoader::InsertInto(AtSpaceGraph* graph) {
    for (unsigned int i = 0; i < m_nodeCount; i++) {
        m_nodes[i].InsertInto(graph);
    }
    for (unsigned int i = 0; i < m_loaderCount; i++) {
        m_loaders[i].InsertInto(graph);
    }
}

void AtSceneDatabaseLoader::RemoveFrom(AtSpaceGraph* graph) {
    for (unsigned int i = 0; i < m_loaderCount; i++) {
        m_loaders[i].RemoveFrom(graph);
    }
    for (unsigned int i = 0; i < m_nodeCount; i++) {
        m_nodes[i].RemoveFrom(graph);
    }
}

// ---- AtCinemaDatabase -------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtCinemaDatabase::AtCinemaDatabase() {
    m_unknown04 = 0;
    m_entries0C = 0;
    m_entries10 = 0;
    m_entries14 = 0;
    m_entries08 = 0;
    m_entries18 = 0;
    m_count1E = 0;
    m_count20 = 0;
    m_count22 = 0;
    m_count1C = 0;
    m_count24 = 0;
}

AtCinemaDatabase::~AtCinemaDatabase() {
    AtCinemaDatabase::Clear();
}

void AtCinemaDatabase::Clear() {
    DeleteObjects(m_entries18);
    DeleteObjects(m_entries08);
    DeleteObjects(m_entries0C);
    DeleteObjects(m_entries10);
    DeleteObjects(m_entries14);
    m_unknown04 = 0;
    m_entries0C = 0;
    m_entries10 = 0;
    m_entries14 = 0;
    m_entries08 = 0;
    m_entries18 = 0;
    m_count1E = 0;
    m_count20 = 0;
    m_count22 = 0;
    m_count1C = 0;
    m_count24 = 0;
}

// ---- AtEventList and AtEventDatabase: their life cycle is at/aee's, src/at/AtEvent.cpp. --------------------------------------------------------------------------------------
