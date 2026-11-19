// GmDatabaseManifest (partial) and GmLevelManifest: the parser of a level's .MNG file and the arrays of items it makes. See include/gm/GmLevelManifest.h.
// Original address clusters 0x00142110-0x001421BF, 0x00142640-0x00143DCB, 0x0015C860-0x0015C8F4, 0x0015CBA0-0x0015CFFF, 0x0015D9E0-0x0015D9FB.
// Verification status per function: config/functions.d/manifest.toml.
#include "gm/GmLevelManifest.h"
#include "gm/GmDatabaseManifestItems.h"
#include "gm/GmAudioData.h"
#include "at/AtMemory.h"

extern "C" {
char* strcpy(char* to, const char* from);
}

// The original's arrays of items come from new[]: a count in the word ahead of the first, which delete[] goes by, last first. Written out, since the heap here may answer null and new[] may not.
// One word is 4 bytes on the Xbox.
typedef decltype(sizeof(0)) GmSize;
struct GmPlacement {};
inline void* operator new(GmSize, void* where, GmPlacement) { return where; }
inline void operator delete(void*, void*, GmPlacement) {}

namespace {

template <class T> T* NewItems(unsigned int count) {
    GmSize* block = (GmSize*)operator new(count * sizeof(T) + sizeof(GmSize));
    if (block == 0) {
        return 0;
    }
    *block = count;
    T* first = (T*)(block + 1);
    for (unsigned int i = 0; i < count; i++) {
        new (first + i, GmPlacement()) T;
    }
    return first;
}

// With a count of 0 the original frees the block itself; otherwise it calls the first item's deleting destructor for the array. Both come to this.
template <class T> void DeleteItems(T* first) {
    GmSize* block = (GmSize*)first - 1;
    for (GmSize i = *block; i > 0; i--) {
        first[i - 1].~T();
    }
    operator delete(block);
}

// What each word does around its new[].
void PushItemHeap() {
    if (GmDatabaseManifest::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(GmDatabaseManifest::s_heap);
}

void PopItemHeap(const void* items) {
    AtPopHeap();
    if (items == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}

template <class T> T* ReadItemArray(unsigned int count) {
    PushItemHeap();
    T* items = NewItems<T>(count);
    PopItemHeap(items);
    return items;
}

}  // namespace

void* GmDatabaseManifest::s_heap;
GmBehaviorSystem* GmBehaviorSystem::s_instance;
GmVfxManager* GmVfxManager::s_instance;
GmDynamicNodeLoader* GmDynamicNodeLoader::s_instance;
GmSpaceGraphOwner* GmSpaceGraphOwner::s_instance;
GmDynamicNodeState* GmDynamicNodeState::s_instance;
GmMusicManager* GmMusicManager::s_instance;

const AtIoKeyword GmDatabaseManifest::s_keywords[kKeywordCount] = {
    {kSoundDatabaseLoaders, "SoundDatabaseLoaders"},
    {kGraphicsDatabasesLoaders, "GraphicsDatabasesLoaders"},
    {kSceneDatabasesLoaders, "SceneDatabasesLoaders"},
    {kCinemaDatabaseLoaders, "CinemaDatabaseLoaders"},
    {kBoundingGeometryDatabase, "BoundingGeometryDatabase"},
    {kEventDatabases, "EventDatabases"},
    {kInstanceRegistries, "InstanceRegistries"},
    {kInSpaceGraph, "InSpaceGraph"},
    {kNotInSpaceGraph, "NotInSpaceGraph"},
    {kInsertInSnoopie, "InsertInSnoopie"},
};

const AtIoKeyword GmLevelManifest::s_keywords[kKeywordCount] = {
    {kBehaviorAnimationLists, "BehaviorAnimationLists"},
    {kBehaviorSoundLists, "BehaviorSoundLists"},
    {kSmallCScripts, "SmallCScripts"},
    {kTriggerDatabases, "TriggerDatabases"},
    {kSubscriberDatabases, "SubscriberDatabases"},
    {kVFXDatabases, "VFXDatabases"},
    {kDynamicNodes, "DynamicNodes"},
    {kVideos, "Videos"},
    {kStreaming, "Streaming"},
    {kTexture, "Texture"},
    {kNode, "Node"},
    {kSoundNodeDatabases, "SoundNodeDatabases"},
    {kSoundNoteDatabases, "SoundNoteDatabases"},
    {kMusicChainDatabases, "MusicChainDatabases"},
    {kCollisionSoundNodeDatabases, "CollisionSoundNodeDatabases"},
    {kInstanceSoundNodeDatabases, "InstanceSoundNodeDatabases"},
};

// ---- GmDatabaseManifest -----------------------------------------------------------------------------------------------------------------------------------------------------------------

void GmDatabaseManifest::ClearDatabaseArrays() {
    m_unknown098 = 0;
    m_events = 0;
    m_soundDatabases = 0;
    m_graphicsDatabases = 0;
    m_registries = 0;
    m_sceneDatabases = 0;
    m_cinemaDatabases = 0;
    m_boundingGeometry = 0;
    m_soundDatabaseCount = 0;
    m_graphicsDatabaseCount = 0;
    m_sceneDatabaseCount = 0;
    m_cinemaDatabaseCount = 0;
    m_boundingGeometryCount = 0;
    m_eventCount = 0;
    m_registryCount = 0;
}

GmDatabaseManifest::GmDatabaseManifest() {
    ClearDatabaseArrays();
}

GmDatabaseManifest::~GmDatabaseManifest() {
    Clear();
}

void GmDatabaseManifest::Clear() {
    Free();
    Reset();
    ClearDatabaseArrays();
}

void GmDatabaseManifest::Parse(AtIoTokenizer* tokenizer) {
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
}

void GmDatabaseManifest::AfterParse() {
    for (unsigned int i = 0; i < m_eventCount; i++) {
        Enqueue(&static_cast<GmEventDatabaseItem*>(m_events)[i], 0);
    }
    for (unsigned int i = 0; i < m_registryCount; i++) {
        Enqueue(&static_cast<GmRegistryItem*>(m_registries)[i], 0);
    }
    for (unsigned int i = 0; i < m_soundDatabaseCount; i++) {
        Enqueue(&static_cast<GmSoundDatabaseItem*>(m_soundDatabases)[i], 0);
    }
    for (unsigned int i = 0; i < m_graphicsDatabaseCount; i++) {
        Enqueue(&static_cast<GmGraphicsDatabaseItem*>(m_graphicsDatabases)[i], 0);
    }
    for (unsigned int i = 0; i < m_boundingGeometryCount; i++) {
        Enqueue(&static_cast<GmBoundingGeometryItem*>(m_boundingGeometry)[i], 0);
    }
    for (unsigned int i = 0; i < m_sceneDatabaseCount; i++) {
        Enqueue(&static_cast<GmSceneDatabaseItem*>(m_sceneDatabases)[i], 0);
    }
    for (unsigned int i = 0; i < m_cinemaDatabaseCount; i++) {
        Enqueue(&static_cast<GmCinemaDatabaseItem*>(m_cinemaDatabases)[i], 0);
    }
}

void GmDatabaseManifest::Free() {
    if (m_cinemaDatabases != 0) {
        GmDatabaseItemsDelete(kCinemaDatabaseItems, m_cinemaDatabases);
        m_cinemaDatabases = 0;
        m_cinemaDatabaseCount = 0;
    }
    if (m_sceneDatabases != 0) {
        for (unsigned int i = 0; i < m_sceneDatabaseCount; i++) {
            GmSceneDatabaseItem* item = &static_cast<GmSceneDatabaseItem*>(m_sceneDatabases)[i];
            if (item->m_inSpaceGraph != 0 && GmSpaceGraphOwner::s_instance->m_graph != 0) {
                GmSceneItemLeave(&item->m_database, GmSpaceGraphOwner::s_instance->m_graph);
            }
        }
        if (m_sceneDatabases != 0) {
            GmDatabaseItemsDelete(kSceneDatabaseItems, m_sceneDatabases);
        }
        m_sceneDatabases = 0;
        m_sceneDatabaseCount = 0;
    }
    if (m_boundingGeometry != 0) {
        GmDatabaseItemsDelete(kBoundingGeometryItems, m_boundingGeometry);
        m_boundingGeometry = 0;
        m_boundingGeometryCount = 0;
    }
    if (m_soundDatabases != 0) {
        for (unsigned int i = 0; i < m_soundDatabaseCount; i++) {
            GmSoundDatabaseItem* item = &static_cast<GmSoundDatabaseItem*>(m_soundDatabases)[i];
            GmAudioFreeDatabase(item->m_database);
            item->Unknown0();
        }
        if (m_soundDatabases != 0) {
            GmDatabaseItemsDelete(kSoundDatabaseItems, m_soundDatabases);
        }
        m_soundDatabases = 0;
        m_soundDatabaseCount = 0;
    }
    if (m_graphicsDatabases != 0) {
        GmDatabaseItemsDelete(kGraphicsDatabaseItems, m_graphicsDatabases);
        m_graphicsDatabases = 0;
        m_graphicsDatabaseCount = 0;
    }
    if (m_events != 0) {
        GmDatabaseItemsDelete(kEventItems, m_events);
        m_events = 0;
        m_eventCount = 0;
    }
    if (m_registries != 0) {
        for (unsigned int i = 0; i < m_registryCount; i++) {
            GmRegistryRelease(&static_cast<GmRegistryItem*>(m_registries)[i].m_registry);
        }
        if (m_registries != 0) {
            GmDatabaseItemsDelete(kRegistryItems, m_registries);
        }
        m_registries = 0;
        m_registryCount = 0;
    }
}

// ---- GmLevelManifest --------------------------------------------------------------------------------------------------------------------------------------------------------------------

void GmLevelManifest::ClearLevelArrays() {
    m_animationLists = 0;
    m_soundLists = 0;
    m_scripts = 0;
    m_vfx = 0;
    m_triggers = 0;
    m_subscribers = 0;
    m_dynamicNodes = 0;
    m_soundNodes = 0;
    m_musicChains = 0;
    m_soundNotes = 0;
    m_collisionSounds = 0;
    m_instanceSounds = 0;
    m_animationListCount = 0;
    m_soundListCount = 0;
    m_scriptCount = 0;
    m_vfxCount = 0;
    m_triggerCount = 0;
    m_subscriberCount = 0;
    m_dynamicNodeCount = 0;
    m_soundNodeCount = 0;
    m_soundNoteCount = 0;
    m_musicChainCount = 0;
    m_collisionSoundCount = 0;
    m_instanceSoundCount = 0;
}

GmLevelManifest::GmLevelManifest() {
    ClearLevelArrays();
}

GmLevelManifest::~GmLevelManifest() {
    Clear();
}

void GmLevelManifest::Clear() {
    GmDatabaseManifest::Clear();
    ClearLevelArrays();
}

void GmLevelManifest::Parse(AtIoTokenizer* tokenizer) {
    // The base installs its words on a tokenizer of its own, which this one then takes them from.
    AtIoTextTokenizer inherited;
    GmDatabaseManifest::Parse(&inherited);
    tokenizer->SetKeywords(s_keywords, kKeywordCount, &inherited);
}

void GmLevelManifest::AfterParse() {
    GmDatabaseManifest::AfterParse();
    for (unsigned int i = 0; i < m_animationListCount; i++) {
        Enqueue(&m_animationLists[i], 0);
    }
    for (unsigned int i = 0; i < m_soundListCount; i++) {
        Enqueue(&m_soundLists[i], 0);
    }
    for (unsigned int i = 0; i < m_scriptCount; i++) {
        Enqueue(&m_scripts[i], 0);
    }
    for (unsigned int i = 0; i < m_vfxCount; i++) {
        Enqueue(&m_vfx[i], 0);
    }
    for (unsigned int i = 0; i < m_triggerCount; i++) {
        Enqueue(&m_triggers[i], 0);
    }
    for (unsigned int i = 0; i < m_subscriberCount; i++) {
        Enqueue(&m_subscribers[i], 0);
    }
    for (unsigned int i = 0; i < m_dynamicNodeCount; i++) {
        Enqueue(&m_dynamicNodes[i], 0);
    }
    for (unsigned int i = 0; i < m_soundNodeCount; i++) {
        Enqueue(&m_soundNodes[i], 0);
    }
    for (unsigned int i = 0; i < m_soundNoteCount; i++) {
        Enqueue(&m_soundNotes[i], 0);
    }
    for (unsigned int i = 0; i < m_musicChainCount; i++) {
        Enqueue(&m_musicChains[i], 0);
    }
    for (unsigned int i = 0; i < m_collisionSoundCount; i++) {
        Enqueue(&m_collisionSounds[i], 0);
    }
    for (unsigned int i = 0; i < m_instanceSoundCount; i++) {
        Enqueue(&m_instanceSounds[i], 0);
    }
}

// Each word: "[ n ] {", then n paths, then "}". The paths of the first seven words are limited to 127 characters; the rest are not, and one longer than the request's buffer runs past it.
// Each word's array replaces the last without freeing it, so a word given twice leaks the first.
void GmLevelManifest::OnToken(int token, AtIoTokenizer* tokenizer) {
    GmManifestItemRequest request;
    switch (token) {
    case kBehaviorAnimationLists:
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        request.m_context = 0;
        m_animationListCount = tokenizer->ExpectCount();
        m_animationLists = ReadItemArray<GmAnimationListItem>(m_animationListCount);
        request.m_context = GmBehaviorSystem::s_instance->m_animationLists;
        for (unsigned int i = 0; i < m_animationListCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString(0x7F));
            m_animationLists[i].Configure(&request);
        }
        tokenizer->ExpectRightCurly();
        break;
    case kBehaviorSoundLists:
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        request.m_context = 0;
        m_soundListCount = tokenizer->ExpectCount();
        m_soundLists = ReadItemArray<GmSoundListItem>(m_soundListCount);
        request.m_context = GmBehaviorSystem::s_instance->m_soundLists;
        for (unsigned int i = 0; i < m_soundListCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString(0x7F));
            m_soundLists[i].Configure(&request);
        }
        tokenizer->ExpectRightCurly();
        break;
    case kSmallCScripts:
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        request.m_context = 0;
        m_scriptCount = tokenizer->ExpectCount();
        m_scripts = ReadItemArray<GmScriptItem>(m_scriptCount);
        request.m_context = GmBehaviorSystem::s_instance->m_scripts;
        for (unsigned int i = 0; i < m_scriptCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString(0x7F));
            m_scripts[i].Configure(&request);
        }
        tokenizer->ExpectRightCurly();
        break;
    case kTriggerDatabases:
        // Neither this word nor the next sets the context: their items do not read it.
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        m_triggerCount = tokenizer->ExpectCount();
        m_triggers = ReadItemArray<GmTriggerItem>(m_triggerCount);
        for (unsigned int i = 0; i < m_triggerCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString(0x7F));
            m_triggers[i].Configure(&request);
        }
        tokenizer->ExpectRightCurly();
        break;
    case kSubscriberDatabases:
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        m_subscriberCount = tokenizer->ExpectCount();
        m_subscribers = ReadItemArray<GmSubscriberItem>(m_subscriberCount);
        for (unsigned int i = 0; i < m_subscriberCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString(0x7F));
            m_subscribers[i].Configure(&request);
        }
        tokenizer->ExpectRightCurly();
        break;
    case kVFXDatabases:
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        request.m_context = 0;
        m_vfxCount = tokenizer->ExpectCount();
        m_vfx = ReadItemArray<GmVfxItem>(m_vfxCount);
        request.m_context = GmVfxManager::s_instance;
        for (unsigned int i = 0; i < m_vfxCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString(0x7F));
            m_vfx[i].Configure(&request);
        }
        tokenizer->ExpectRightCurly();
        break;
    case kDynamicNodes:
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        request.m_context2 = 0;
        request.m_context = 0;
        m_dynamicNodeCount = tokenizer->ExpectCount();
        m_dynamicNodes = ReadItemArray<GmDynamicNodeItem>(m_dynamicNodeCount);
        request.m_context2 = GmDynamicNodeLoader::s_instance;
        request.m_context = GmSpaceGraphOwner::s_instance->m_graph;
        for (unsigned int i = 0; i < m_dynamicNodeCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString(0x7F));
            m_dynamicNodes[i].Configure(&request);
        }
        // The only word whose closing brace is not read here: it reaches OnToken as a token of its own, and the base lets it by.
        break;
    case kSoundNodeDatabases:
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        request.m_context = 0;
        m_soundNodeCount = tokenizer->ExpectCount();
        m_soundNodes = ReadItemArray<GmSoundNodeItem>(m_soundNodeCount);
        for (unsigned int i = 0; i < m_soundNodeCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString());
            m_soundNodes[i].Configure(&request);
        }
        tokenizer->ExpectRightCurly();
        break;
    case kSoundNoteDatabases:
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        request.m_context = 0;
        m_soundNoteCount = tokenizer->ExpectCount();
        PushItemHeap();
        m_soundNotes = NewItems<GmSoundNoteItem>(m_soundNoteCount);
        PopItemHeap(m_soundNotes);
        for (unsigned int i = 0; i < m_soundNoteCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString());
            m_soundNotes[i].Configure((const GmSoundNoteItem::Request*)&request);
        }
        tokenizer->ExpectRightCurly();
        break;
    case kMusicChainDatabases:
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        request.m_context = 0;
        m_musicChainCount = tokenizer->ExpectCount();
        m_musicChains = ReadItemArray<GmMusicChainItem>(m_musicChainCount);
        for (unsigned int i = 0; i < m_musicChainCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString());
            m_musicChains[i].Configure(&request);
        }
        tokenizer->ExpectRightCurly();
        break;
    case kCollisionSoundNodeDatabases:
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        request.m_context = 0;
        m_collisionSoundCount = tokenizer->ExpectCount();
        m_collisionSounds = ReadItemArray<GmCollisionSoundItem>(m_collisionSoundCount);
        for (unsigned int i = 0; i < m_collisionSoundCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString());
            m_collisionSounds[i].Configure(&request);
        }
        tokenizer->ExpectRightCurly();
        break;
    case kInstanceSoundNodeDatabases:
        request.m_unused[0] = 0;
        request.m_path[0] = 0;
        request.m_context = 0;
        m_instanceSoundCount = tokenizer->ExpectCount();
        m_instanceSounds = ReadItemArray<GmInstanceSoundItem>(m_instanceSoundCount);
        for (unsigned int i = 0; i < m_instanceSoundCount; i++) {
            strcpy(request.m_path, tokenizer->ExpectString());
            m_instanceSounds[i].Configure(&request);
        }
        tokenizer->ExpectRightCurly();
        break;
    default:
        GmDatabaseManifest::OnToken(token, tokenizer);
        break;
    }
}

void GmLevelManifest::Finished() {
    GmDatabaseManifest::Finished();
    for (unsigned int i = 0; i < m_soundNodeCount; i++) {
        GmAudioRegisterNodes(m_soundNodes[i].m_bank);
    }
    for (unsigned int i = 0; i < m_soundNoteCount; i++) {
        GmAudioData::s_instance->RegisterNotes(m_soundNotes[i].m_bank);
    }
    for (unsigned int i = 0; i < m_instanceSoundCount; i++) {
        m_instanceSounds[i].Register();
    }
}

// The lists, scripts and effects are given back through the game's owners, not the ones the items were configured with.
void GmLevelManifest::Free() {
    if (m_animationLists != 0) {
        for (unsigned int i = 0; i < m_animationListCount; i++) {
            if (m_animationLists[i].m_list != 0) {
                GmBehaviorSystem::s_instance->m_animationLists->Release(m_animationLists[i].m_list);
            }
        }
        if (m_animationLists != 0) {
            DeleteItems(m_animationLists);
        }
        m_animationLists = 0;
        m_animationListCount = 0;
    }
    if (m_soundLists != 0) {
        for (unsigned int i = 0; i < m_soundListCount; i++) {
            if (m_soundLists[i].m_list != 0) {
                GmBehaviorSystem::s_instance->m_soundLists->Release(m_soundLists[i].m_list);
            }
        }
        if (m_soundLists != 0) {
            DeleteItems(m_soundLists);
        }
        m_soundLists = 0;
        m_soundListCount = 0;
    }
    if (m_scripts != 0) {
        for (unsigned int i = 0; i < m_scriptCount; i++) {
            if (m_scripts[i].m_script != 0) {
                GmBehaviorSystem::s_instance->m_scripts->Release(m_scripts[i].m_script);
            }
        }
        if (m_scripts != 0) {
            DeleteItems(m_scripts);
        }
        m_scripts = 0;
        m_scriptCount = 0;
    }
    if (m_triggers != 0) {
        DeleteItems(m_triggers);
        m_triggers = 0;
        m_triggerCount = 0;
    }
    if (m_subscribers != 0) {
        DeleteItems(m_subscribers);
        m_subscribers = 0;
        m_subscriberCount = 0;
    }
    if (m_vfx != 0) {
        for (unsigned int i = 0; i < m_vfxCount; i++) {
            if (m_vfx[i].m_database != 0) {
                GmVfxManager::s_instance->Delete(m_vfx[i].m_database);
            }
        }
        if (m_vfx != 0) {
            DeleteItems(m_vfx);
        }
        m_vfx = 0;
        m_vfxCount = 0;
    }
    if (m_dynamicNodes != 0) {
        for (unsigned int i = 0; i < m_dynamicNodeCount; i++) {
            m_dynamicNodes[i].m_scene.RemoveFrom(GmSpaceGraphOwner::s_instance->m_graph);
            const int flag = GmDynamicNodeState::s_instance->m_unknown24 != 0 || GmDynamicNodeState::s_instance->m_unknown28 != 0;
            GmDynamicNodeLoader::s_instance->Unload(&m_dynamicNodes[i].m_scene, flag);
        }
        if (m_dynamicNodes != 0) {
            DeleteItems(m_dynamicNodes);
        }
        m_dynamicNodes = 0;
        m_dynamicNodeCount = 0;
    }
    if (m_soundNodes != 0) {
        for (unsigned int i = 0; i < m_soundNodeCount; i++) {
            m_soundNodes[i].Unknown0();
        }
        if (m_soundNodes != 0) {
            DeleteItems(m_soundNodes);
        }
        m_soundNodes = 0;
        m_soundNodeCount = 0;
    }
    if (m_soundNotes != 0) {
        DeleteItems(m_soundNotes);
        m_soundNotes = 0;
        m_soundNoteCount = 0;
    }
    if (m_musicChains != 0) {
        DeleteItems(m_musicChains);
        m_musicChains = 0;
        m_musicChainCount = 0;
    }
    if (m_collisionSounds != 0) {
        for (unsigned int i = 0; i < m_collisionSoundCount; i++) {
            m_collisionSounds[i].Unknown0();
        }
        if (m_collisionSounds != 0) {
            DeleteItems(m_collisionSounds);
        }
        m_collisionSounds = 0;
        m_collisionSoundCount = 0;
    }
    if (m_instanceSounds != 0) {
        for (unsigned int i = 0; i < m_instanceSoundCount; i++) {
            m_instanceSounds[i].Unknown0();
        }
        if (m_instanceSounds != 0) {
            DeleteItems(m_instanceSounds);
        }
        m_instanceSounds = 0;
        m_instanceSoundCount = 0;
    }
    GmDatabaseManifest::Free();
}
