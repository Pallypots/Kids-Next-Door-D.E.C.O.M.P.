// What starts and stops behaviour scripts by path: the level manifest's SmallCScripts items and the behaviour charts (0x0019BEF5) come through here.
// Subsystem at/smallvm; the class is gm/manifest's GmScriptOwner (include/gm/GmLevelManifest.h), whose seams these replace. In a file of its own so that a
// harness can still supply them. Original cluster 0x00070150-0x0007038F. Names are inferred.
#include "at/AtIoMountManager.h"
#include "at/AtSmallVm.h"
#include "gm/GmLevelManifest.h"

void AtCopyName8(char* to, const char* from);  // 0x00013F70 (gm/levels)

void GmScriptOwner::Release(GmScript* script) {
    if (script != 0) {
        char name[8];
        AtCopyName8(name, script->m_name);
        m_system->StopScript(name);
    }
}

int GmScriptOwner::CallPublic(GmScript* script, int index) {
    return m_system->CallPublic(script, index, 1);
}

GmScript* GmScriptOwner::Start(const char* directory, const char* file) {
    AtSmallCScriptDesc desc;
    AtCopyName8(desc.m_file, file);
    AtCopyName8(desc.m_name, file);
    desc.m_enabled = 0;
    desc.m_entry[0] = 0;
    desc.m_flag40 = 0;
    desc.m_shared = 0;
    AtIoMountManager::s_instance->m_context->PushPath(directory);
    const int status = m_system->StartScript(&desc);
    AtIoMountManager::s_instance->m_context->PopPath();
    if (status != 0) {
        return 0;
    }
    char name[8];
    AtCopyName8(name, file);
    return m_system->FindScript(name);
}

GmScript* GmScriptOwner::Load(const char* path) {
    char file[8];
    char directory[0x100];
    char* to = directory;  // ORIGINAL: copied unbounded
    const char* from = path;
    while ((*to++ = *from++) != 0) {
    }
    char* slash = 0;
    for (char* p = directory; *p != 0; p++) {
        if (*p == '\\') {
            slash = p;
        }
    }
    if (slash != 0) {
        AtCopyName(slash + 1, file);
        *slash = 0;
    } else {
        directory[0] = 0;
        AtCopyName(path, file);
    }
    return Start(directory, file);
}
