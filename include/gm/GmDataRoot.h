// The object at 0x003B357C. Two works found it: App's start-up (its registry at +0x2C, which every loaded registry hangs under, and the owner reached through +0x04) and the
// level manifest (the two words at +0x24 and +0x28 that decide GmDynamicNodeLoader::Unload's flag, and the registry and owner InstanceRegistries hands its items). One struct; the
// manifest work called it GmDynamicNodeState and the start-up work GmDataRoot, folded on integrate-batch7. Its class is not recovered; names inferred.
#pragma once

#include "at/AtDataRegistry.h"

class App;

struct GmDataRoot {
    void* m_unknown00;
    App* m_app;                              // +0x04 the App: the registries' owner is its data management layer's (+0x394); Owner(), include/gm/GmApp.h
    char m_unknown08[0x1C];
    int m_unknown24;                         // +0x24 with +0x28, decides GmDynamicNodeLoader::Unload's flag (the level manifest's Free)
    int m_unknown28;                         // +0x28
    AtDataRegistry m_registry;               // +0x2C each loaded registry's parent: start-up makes it with Prepare and names it "Library"

    AtDataRegistryOwner* Owner();            // &m_app's owner at +0x394; defined in include/gm/GmApp.h, where App is complete

    static GmDataRoot* s_instance;           // 0x003B357C
};
