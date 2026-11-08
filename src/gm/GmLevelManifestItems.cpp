// The items a level's .MNG file makes: one class to each word of GmLevelManifest. See include/gm/GmLevelManifest.h. Original address cluster 0x001B2BB0-0x001B3B4F, less the sound notes'
// 0x001B3540-0x001B36CF; their deleting destructors are compiler-made, at 0x001421C0-0x0014263F.
// Each Run builds "%3s][%s" from the extension and path into a buffer and never uses it; some build it twice. The buffer is kept, as the original's work.
// Verification status per function: config/functions.d/manifest.toml.
#include "gm/GmLevelManifest.h"

extern "C" {
char* strcpy(char* to, const char* from);
}
int AtFormat(char* out, const char* format, ...);  // the C library's sprintf, 0x00125559: src/at/AtIoFormatHost.cpp

// ---- ANL --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmAnimationListItem::GmAnimationListItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    m_owner = 0;
    m_list = 0;
    AtFormat(m_extension, "");
}

GmAnimationListItem::~GmAnimationListItem() {
    Unknown0();
}

void GmAnimationListItem::Unknown0() {
    m_owner = 0;
    m_list = 0;
    AtFormat(m_extension, "");
}

void GmAnimationListItem::Run() {
    char unused[0x200];
    AtFormat(unused, "%3s][%s", m_extension, m_path);
    m_list = m_owner->LoadAnimations(m_path);
    if (m_list != 0) {
        m_list->Prepare();
    }
}

void GmAnimationListItem::Configure(const GmManifestItemRequest* request) {
    Unknown0();
    strcpy(m_path, request->m_path);
    AtFormat(m_extension, "ANL");
    m_owner = (GmBehaviorListOwner*)request->m_context;
}

// ---- SDL --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmSoundListItem::GmSoundListItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    m_owner = 0;
    m_list = 0;
    AtFormat(m_extension, "");
}

GmSoundListItem::~GmSoundListItem() {
    Unknown0();
}

void GmSoundListItem::Unknown0() {
    m_owner = 0;
    m_list = 0;
    AtFormat(m_extension, "");
}

void GmSoundListItem::Run() {
    char unused[0x200];
    AtFormat(unused, "%3s][%s", m_extension, m_path);
    m_list = m_owner->LoadSounds(m_path);
    if (m_list != 0) {
        m_list->Prepare();
    }
}

void GmSoundListItem::Configure(const GmManifestItemRequest* request) {
    Unknown0();
    strcpy(m_path, request->m_path);
    AtFormat(m_extension, "SDL");
    m_owner = (GmBehaviorListOwner*)request->m_context;
}

// ---- ASL --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmScriptItem::GmScriptItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    m_owner = 0;
    m_script = 0;
    AtFormat(m_extension, "");
}

GmScriptItem::~GmScriptItem() {
    Unknown0();
}

void GmScriptItem::Unknown0() {
    m_owner = 0;
    m_script = 0;
    AtFormat(m_extension, "");
}

void GmScriptItem::Run() {
    char unused[0x200];
    AtFormat(unused, "%3s][%s", m_extension, m_path);
    m_script = m_owner->Load(m_path);
}

void GmScriptItem::Configure(const GmManifestItemRequest* request) {
    Unknown0();
    strcpy(m_path, request->m_path);
    AtFormat(m_extension, "ASL");
    m_owner = (GmScriptOwner*)request->m_context;
}

// ---- AET --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmTriggerItem::GmTriggerItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    AtFormat(m_extension, "");
}

GmTriggerItem::~GmTriggerItem() {
    Unknown0();
}

void GmTriggerItem::Unknown0() {
    m_table.Unload();
    AtFormat(m_extension, "");
}

void GmTriggerItem::Run() {
    char unused[0x200];
    AtFormat(unused, "%3s][%s", m_extension, m_path);
    m_table.Load(m_path);
}

void GmTriggerItem::Configure(const GmManifestItemRequest* request) {
    Unknown0();
    strcpy(m_path, request->m_path);
    AtFormat(m_extension, "AET");
}

// ---- AES --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmSubscriberItem::GmSubscriberItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    AtFormat(m_extension, "");
}

GmSubscriberItem::~GmSubscriberItem() {
    Unknown0();
}

void GmSubscriberItem::Unknown0() {
    m_table.Unload();
    AtFormat(m_extension, "");
}

void GmSubscriberItem::Run() {
    char unused[0x200];
    AtFormat(unused, "%3s][%s", m_extension, m_path);
    m_table.Load(m_path);
}

void GmSubscriberItem::Configure(const GmManifestItemRequest* request) {
    Unknown0();
    strcpy(m_path, request->m_path);
    AtFormat(m_extension, "AES");
}

// ---- VFX --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmVfxItem::GmVfxItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    m_database = 0;
    m_manager = 0;
    AtFormat(m_extension, "");
}

GmVfxItem::~GmVfxItem() {
    Unknown0();
}

void GmVfxItem::Unknown0() {
    m_database = 0;
    m_manager = 0;
    AtFormat(m_extension, "");
}

void GmVfxItem::Run() {
    char unused[0x200];
    AtFormat(unused, "%3s][%s", m_extension, m_path);
    m_database = m_manager->Load(m_path);
}

void GmVfxItem::Configure(const GmManifestItemRequest* request) {
    Unknown0();
    strcpy(m_path, request->m_path);
    AtFormat(m_extension, "VFX");
    m_manager = (GmVfxManager*)request->m_context;
}

// ---- DAC --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmDynamicNodeItem::GmDynamicNodeItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    m_graph = 0;
    m_loader = 0;
    AtFormat(m_extension, "");
}

GmDynamicNodeItem::~GmDynamicNodeItem() {
    Unknown0();
}

void GmDynamicNodeItem::Unknown0() {
    m_graph = 0;
    m_loader = 0;
    AtFormat(m_extension, "");
}

void GmDynamicNodeItem::Run() {
    char unused[0x200];
    AtFormat(unused, "%3s][%s", m_extension, m_path);
    m_loader->Load(m_path, 0, &m_scene);
    AtFormat(unused, "%3s][%s", m_extension, m_path);
    m_scene.InsertInto(m_graph);
}

void GmDynamicNodeItem::Configure(const GmManifestItemRequest* request) {
    Unknown0();
    strcpy(m_path, request->m_path);
    AtFormat(m_extension, "DAC");
    m_loader = (GmDynamicNodeLoader*)request->m_context2;
    m_graph = (AtSpaceGraph*)request->m_context;
}

// ---- GOO --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmSoundNodeItem::GmSoundNodeItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    m_bank = 0;
    m_loaded = 0;
    AtFormat(m_extension, "");
}

GmSoundNodeItem::~GmSoundNodeItem() {
    Unknown0();
}

void GmSoundNodeItem::Unknown0() {
    if (m_bank != 0 && m_loaded != 0) {
        GmAudioFreeNodes(m_bank);
        GmAudioReleaseNodeBank(m_bank);
    }
    m_bank = 0;
    m_loaded = 0;
    AtFormat(m_extension, "");
}

// With no bank free, nothing is loaded, nothing is said, and the item still counts as loaded.
void GmSoundNodeItem::Run() {
    if (m_loaded != 0) {
        return;
    }
    char unused[0x200];
    AtFormat(unused, "%3s][%s", m_extension, m_path);
    m_bank = GmAudioAllocateNodeBank();
    if (m_bank != 0) {
        AtFormat(unused, "%3s][%s", m_extension, m_path);
        GmAudioLoadNodes(m_bank, m_path);
    }
    m_loaded = 1;
}

void GmSoundNodeItem::Configure(const GmManifestItemRequest* request) {
    Unknown0();
    strcpy(m_path, request->m_path);
    AtFormat(m_extension, "GOO");
}

// ---- GOM --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmMusicChainItem::GmMusicChainItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    m_chain = 0;
    AtFormat(m_extension, "");
}

GmMusicChainItem::~GmMusicChainItem() {
    Unknown0();
}

void GmMusicChainItem::Unknown0() {
    if (m_chain != 0) {
        GmMusicManager::s_instance->Stop(m_chain);
        m_chain->Clear();
        GmMusicManager::s_instance->Free(m_chain);
    }
    m_chain = 0;
    AtFormat(m_extension, "");
}

void GmMusicChainItem::Run() {
    char unused[0x200];
    m_chain = 0;
    AtFormat(unused, "%3s][%s", m_extension, m_path);
    m_chain = GmMusicManager::s_instance->Allocate();
    if (m_chain != 0) {
        AtFormat(unused, "%3s][%s", m_extension, m_path);
        m_chain->Load(m_path, 0);
    }
}

void GmMusicChainItem::Configure(const GmManifestItemRequest* request) {
    Unknown0();
    strcpy(m_path, request->m_path);
    AtFormat(m_extension, "GOM");
}

// ---- GOI --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmInstanceSoundItem::GmInstanceSoundItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    m_data = 0;
    m_loaded = 0;
    AtFormat(m_extension, "");
}

GmInstanceSoundItem::~GmInstanceSoundItem() {
    Unknown0();
}

void GmInstanceSoundItem::Unknown0() {
    if (m_data != 0 && m_loaded != 0) {
        GmAudioFreeInstanceSounds(m_data);
    }
    m_data = 0;
    m_loaded = 0;
    AtFormat(m_extension, "");
}

// Data already held is kept, and the item marked loaded.
void GmInstanceSoundItem::Run() {
    if (m_loaded != 0) {
        return;
    }
    if (m_data == 0) {
        char unused[0x200];
        AtFormat(unused, "%3s][%s", m_extension, m_path);
        m_data = GmAudioLoadInstanceSounds(m_path);
    }
    m_loaded = 1;
}

void GmInstanceSoundItem::Register() {
    if (m_data != 0) {
        GmAudioRegisterInstanceSounds(m_data);
    }
}

void GmInstanceSoundItem::Configure(const GmManifestItemRequest* request) {
    Unknown0();
    strcpy(m_path, request->m_path);
    AtFormat(m_extension, "GOI");
}

// ---- GOC --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GmCollisionSoundItem::GmCollisionSoundItem() {
    m_extension[0] = 0;
    m_path[0] = 0;
    m_next = 0;
    m_data = 0;
    m_loaded = 0;
    AtFormat(m_extension, "");
}

GmCollisionSoundItem::~GmCollisionSoundItem() {
    Unknown0();
}

void GmCollisionSoundItem::Unknown0() {
    if (m_data != 0 && m_loaded != 0) {
        GmAudioFreeCollisionSounds(m_data);
    }
    m_data = 0;
    m_loaded = 0;
    AtFormat(m_extension, "");
}

void GmCollisionSoundItem::Run() {
    if (m_loaded != 0) {
        return;
    }
    if (m_data == 0) {
        char unused[0x200];
        AtFormat(unused, "%3s][%s", m_extension, m_path);
        m_data = GmAudioLoadCollisionSounds(m_path);
    }
    m_loaded = 1;
}

void GmCollisionSoundItem::Configure(const GmManifestItemRequest* request) {
    Unknown0();
    strcpy(m_path, request->m_path);
    AtFormat(m_extension, "GOC");
}
