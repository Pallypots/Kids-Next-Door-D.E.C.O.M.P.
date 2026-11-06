// AtIoMountManager, AtIoSpaceManager. Original address clusters 0x00076640-0x00076C0B, 0x0007CEE0-0x0007CEFD, 0x0007CFC0-0x0007D012, 0x0007D140-0x0007D15A, 0x000EB9D0.
// Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "at/AtIoMountManager.h"
#include "AtIoJamCommon.h"

AtIoMountManager* AtIoMountManager::s_instance;
void* AtIoMountManager::s_heap;
void* AtIoSpaceManager::s_heap;
AtIoSpaceManager* AtIoSpaceManager::s_instance;

void AtIoNotifyMounted(AtIoJamFileSystem* fs) {
    fs->m_header->Unknown4();
}

void AtIoNotifyUnmounted(AtIoJamFileSystem* fs) {
    fs->m_header->Unknown5();
}

void AtIoDeleteObject(AtIoFileSystem* fs) {
    if (fs != 0) {
        delete fs;
    }
}

void AtIoSpaceManager::SetHeap(void* heap) {
    s_heap = heap;
}

AtIoSpaceManager::AtIoSpaceManager() {
    s_instance = this;
    m_unknown04 = 0;
    m_device = 0;
    m_unknown08 = 0;
    m_unknown0C = 0;
}

AtIoSpaceManager::~AtIoSpaceManager() {
    m_unknown04 = 0;
    m_device = 0;
    m_unknown08 = 0;
    m_unknown0C = 0;
    s_instance = 0;
}

void AtIoSpaceManager::Clear() {
    m_unknown04 = 0;
    m_device = 0;
    m_unknown08 = 0;
    m_unknown0C = 0;
}

int AtIoSpaceManager::Init() {
    m_device = *(AtIoContextDevice**)AtIoMountManager::s_instance->m_context;
    return m_device->Unknown1();
}

AtIoMountManager::AtIoMountManager() {
    s_instance = this;
    m_device = 0;
    m_root = 0;
    m_unknown28 = 0;
    m_factory = 0;
    s_heap = 0;
    m_mounted = 0;
    m_side = 0;
    m_region[0] = 0;
    m_space = 0;
}

AtIoMountManager::~AtIoMountManager() {
    s_instance = 0;
    AtIoMountManager::Shutdown();
}

void AtIoMountManager::Init(const AtIoMountParams* params) {
    if (s_heap != 0) {
        Shutdown();
    }
    m_owner = params->m_owner;
    m_context = params->m_context;
    m_factory = params->m_owner->m_factory;
    s_heap = params->m_heap;
    AtIoSpaceManager::SetHeap(s_heap);
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_space = new AtIoSpaceManager;
    AtPopHeap();
    if (m_space == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_space->Init();
    CreateFileSystems();
}

void AtIoMountManager::Shutdown() {
    if (m_space != 0) {
        m_space->Clear();
        delete m_space;
        m_space = 0;
    }
    if (m_factory != 0) {
        if (m_device != 0) {
            m_factory->Destroy(m_device);
        }
        if (m_root != 0) {
            m_factory->Destroy(m_root);
        }
        if (m_unknown28 != 0) {
            m_factory->Destroy(m_unknown28);
        }
    }
    m_device = 0;
    m_root = 0;
    m_unknown28 = 0;
    m_factory = 0;
    s_heap = 0;
    m_mounted = 0;
    m_side = 0;
    m_region[0] = 0;
    m_space = 0;
}

void AtIoMountManager::CreateFileSystems() {
    m_device = m_factory->Create(AtIoFileSystemFactory::kDevice, 0, 1);
    m_context->Push(m_device);
    m_root = m_factory->Create(AtIoFileSystemFactory::kRoot, 0, 1);
}

void AtIoMountManager::SetRegion(const char* name) {
    if (name == 0) {
        m_region[0] = 0;
    } else {
        for (int i = 0; i < 8; i++) {
            m_region[i] = name[i];
        }
    }
    char root[9];
    AtCopyName9(root, m_region);
    m_device->SetRoot(root);
}

void AtIoMountManager::WriteList(const char* text) {
    m_listFile.Write(text, (unsigned int)strlen(text));
}

void AtIoMountManager::Mount(const char* name, const char* directory) {
    char prefix[0x20];
    strcpy(prefix, directory);
    if (strlen(prefix) != 0) {
        strcat(prefix, "\\");
    }
    // Original behaviour preserved: no bound on either buffer.
    char path[0x40];
    strcpy(path, "JamFiles\\XBox\\");
    strcat(path, prefix);
    strcat(path, name);
    strcat(path, ".JAM");
    if (m_device->IsFileMissing(path) == 0) {
        m_mounted = AtIoJamFileSystem::Create(path, 4, 2);
        AtIoNotifyMounted(m_mounted);
        m_context->Push(m_mounted);
    }
}

void AtIoMountManager::MountPath(const char* path) {
    const unsigned int length = (unsigned int)strlen(path);
    if ((int)(length + 4) >= 0x100) {
        return;
    }
    char full[0x100];
    strcpy(full, path);
    strcpy(full + length, ".JAM");
    if (m_device->IsFileMissing(full) == 0) {
        m_mounted = AtIoJamFileSystem::Create(full, 4, 2);
        AtIoNotifyMounted(m_mounted);
        m_context->Push(m_mounted);
    }
}

void AtIoMountManager::Unmount() {
    if (m_mounted != 0) {
        m_context->Pop();
        AtIoNotifyUnmounted(m_mounted);
        AtIoDeleteObject(m_mounted);
        m_mounted = 0;
    }
}

void AtIoMountManager::DropSideArchive() {
    if (m_side != 0) {
        AtIoNotifyUnmounted(m_side);
        if (m_context->Top() == m_side) {
            m_context->Pop();
        }
        AtIoDeleteObject(m_side);
        m_side = 0;
    }
}

void AtIoMountManager::SwapSideArchive(const char* path) {
    DropSideArchive();
    const unsigned int length = (unsigned int)strlen(path);
    if ((int)(length + 4) >= 0x100) {
        return;
    }
    char full[0x100];
    strcpy(full, path);
    strcpy(full + length, ".JAM");
    if (m_device->IsFileMissing(full) == 0) {
        m_side = AtIoJamFileSystem::Create(full, 4, 2);
        AtIoNotifyMounted(m_side);
    }
}

void AtIoMountManager::OpenListFile(const char* name) {
    if (m_listFile.OpenOn(m_root, name, 0x24) == 0) {
        m_device->m_unknown6B4 = &m_listFile;
    }
    m_root->m_path.Pop(0);
}

void AtIoMountManager::CloseListFile() {
    m_device->m_unknown6B4 = 0;
    m_listFile.Close();
}
