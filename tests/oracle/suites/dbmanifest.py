# Mutants of the level manifest's base words and items (gm/dbmanifest). Each text occurs exactly once in its file.
D = "GmDatabaseManifest.cpp"

# Dropped as equivalent, with the reason:
# * request.m_unused[0] = 0, request.m_path[0] = 0 and the zeroing of each context before a word sets it from a global: nothing reads the first, the rest are overwritten before any item
#   sees them.
# * The constructors' m_extension[0] = 0 and AtFormat(m_extension, ""): each alone leaves the same first byte.
# * The order of the zeroing stores in each constructor and Clear: nothing reads between them.
# * GmEventDatabaseItem's constructor clears m_path[0] twice, as the original does; dropping either changes nothing.
# * AtCopyName9 straight from the language instead of through AtCopyName8's eight bytes: both take the first eight bytes and terminate them; what follows a shorter
#   language's terminator is never read by the format. Not detectable, and the same text.
# * DeleteObjects' own test of a count of 0: the host compiler's delete[] of a class with a virtual destructor makes the same test itself (seen in the oracle's build:
#   it frees the block alone when the count is 0), so dropping the explicit one changes nothing here. It is kept as the original's behaviour, whatever the compiler.
# Not compared: GmEventDatabaseItem's and GmRegistryItem's constructors leave the owner or manager unset; both sides see the heap's fill byte, which the oracle sets alike.

MUTANTS = [
    # OnToken.
    ("sound: no run at once", D, "            As<GmSoundDatabaseItem>(m_soundDatabases)[i].Run();\n", ""),
    ("sound: path limit", D, "strcpy(request.m_path, tokenizer->ExpectString(0x80));", "strcpy(request.m_path, tokenizer->ExpectString(0x7F));"),
    ("sound: owner", D, "request.m_context2 = GmAudioData::s_soundOwner;", "request.m_context2 = 0;"),
    ("sound: maker", D, "request.m_context = GmAudioDatabaseMaker::s_instance;", "request.m_context = 0;"),
    ("sound: index", D, "As<GmSoundDatabaseItem>(m_soundDatabases)[i].Configure(&request);", "As<GmSoundDatabaseItem>(m_soundDatabases)[0].Configure(&request);"),
    ("graphics: maker", D, "request.m_context2 = GmHistoryLogs::s_instance->m_app->m_graphics;", "request.m_context2 = GmHistoryLogs::s_instance->m_app->m_graphics + 1;"),
    ("graphics: owner", D, "request.m_context = GmHistoryLogs::s_instance->m_app->m_materialOwner;", "request.m_context = 0;"),
    ("graphics: count", D, "for (unsigned int i = 0; i < m_graphicsDatabaseCount; i++) {", "for (unsigned int i = 0; i < m_graphicsDatabaseCount && i < 1; i++) {"),
    ("graphics: array", D, "m_graphicsDatabases = ReadItemArray<GmGraphicsDatabaseItem>(m_graphicsDatabaseCount);", "m_graphicsDatabases = ReadItemArray<GmGraphicsDatabaseItem>(m_graphicsDatabaseCount + 1);"),
    ("scene: owner", D, "request.m_context = &GmHistoryLogs::s_instance->m_app->m_sceneOwner;\n        // After", "request.m_context = &GmHistoryLogs::s_instance->m_app->m_cinemaOwner;\n        // After"),
    ("scene: flag word", D, "(tokenizer->Next() == kInSpaceGraph ? 1 : 0)", "(tokenizer->Next() == kNotInSpaceGraph ? 1 : 0)"),
    ("scene: snoopie kept", D, "            if (tokenizer->Next() != kInsertInSnoopie) {\n                tokenizer->m_pushedBack = 1;\n            }\n", "            tokenizer->Next();\n"),
    ("scene: snoopie read always back", D, "                tokenizer->m_pushedBack = 1;\n", "                tokenizer->m_pushedBack = 0;\n"),
    ("scene: path limit", D, "            strcpy(request.m_path, tokenizer->ExpectString(0x7F));\n            request.m_context2", "            strcpy(request.m_path, tokenizer->ExpectString(0x7E));\n            request.m_context2"),
    ("cinema: owner", D, "request.m_context = &GmHistoryLogs::s_instance->m_app->m_cinemaOwner;", "request.m_context = &GmHistoryLogs::s_instance->m_app->m_sceneOwner;"),
    ("cinema: count field", D, "m_cinemaDatabaseCount = tokenizer->ExpectCount();", "m_boundingGeometryCount = tokenizer->ExpectCount();"),
    ("bounding: owner", D, "request.m_context = &GmHistoryLogs::s_instance->m_app->m_sceneOwner;\n        for (unsigned int i = 0; i < m_boundingGeometryCount", "request.m_context = 0;\n        for (unsigned int i = 0; i < m_boundingGeometryCount"),
    ("bounding: array", D, "m_boundingGeometry = ReadItemArray<GmBoundingGeometryItem>(m_boundingGeometryCount);", "m_cinemaDatabases = ReadItemArray<GmBoundingGeometryItem>(m_boundingGeometryCount);"),
    ("event: owner", D, "request.m_context = &GmEventHome::s_instance->m_owner;", "request.m_context = GmEventHome::s_instance;"),
    ("event: count", D, "for (unsigned int i = 0; i < m_eventCount; i++) {", "for (unsigned int i = 1; i < m_eventCount; i++) {"),
    ("registry: parent", D, "registry.m_context2 = &GmDynamicNodeState::s_instance->m_registry;", "registry.m_context2 = 0;"),
    ("registry: owner", D, "registry.m_context3 = &GmDynamicNodeState::s_instance->m_dataOwners->m_registryOwner;", "registry.m_context3 = (void*)GmDynamicNodeState::s_instance->m_dataOwners;"),
    ("registry: names", D, "registry.m_context = GmRegistryNames::s_instance;", "registry.m_context = 0;"),
    ("registry: path limit", D, "strcpy(registry.m_path, tokenizer->ExpectString(0x7F));", "strcpy(registry.m_path, tokenizer->ExpectString());"),
    ("words: closing brace", D, "    }\n    tokenizer->ExpectRightCurly();\n}", "    }\n}"),
    ("words: default reads brace", D, "        return;\n    }\n    tokenizer->ExpectRightCurly();", "        break;\n    }\n    tokenizer->ExpectRightCurly();"),
    ("heap: no raise", D, "        AtRaise(-2, 0, 0, 0);\n", ""),
    ("heap: push", D, "    AtPushHeap(GmDatabaseManifest::s_heap);\n", ""),
    ("memory: no raise", D, "        AtRaise(-4, 0, 0, 0);\n", ""),
    ("new: count", D, "    *block = count;\n", "    *block = count + 1;\n"),
    # Finished.
    ("Finished: no break", D, "            GmSpaceGraphOwner::s_instance->SetGraph(graph);\n            break;\n", "            GmSpaceGraphOwner::s_instance->SetGraph(graph);\n"),
    ("Finished: no set", D, "            GmSpaceGraphOwner::s_instance->SetGraph(graph);\n", ""),
    ("Finished: flag", D, "if (item->m_inSpaceGraph != 0 && GmSpaceGraphOwner::s_instance->m_graph != 0) {", "if (GmSpaceGraphOwner::s_instance->m_graph != 0) {"),
    ("Finished: graph test", D, "if (item->m_inSpaceGraph != 0 && GmSpaceGraphOwner::s_instance->m_graph != 0) {", "if (item->m_inSpaceGraph != 0) {"),
    # Deleting the arrays.
    ("delete: registry as events", D, "        DeleteItems(As<GmRegistryItem>(first));\n", "        DeleteItems(As<GmEventDatabaseItem>(first));\n"),
    ("delete: last first", D, "        first[i - 1].~T();\n    }\n    operator delete(block);\n}\n\n// The same", "        first[*block - i].~T();\n    }\n    operator delete(block);\n}\n\n// The same"),
    ("scene leave: nothing", D, "    ((AtSceneDatabaseLoader*)part)->RemoveFrom(graph);\n", ""),
    # FindSpaceGraph and RegisterDatabase.
    ("find: own graph", D, "        return database->SpaceGraphAt(0);", "        return database->SpaceGraphAt(1);"),
    ("find: own first", D, "    if (database->m_spaceGraphCount > 0) {", "    if (database->m_spaceGraphCount > 1) {"),
    ("find: nested", D, "        AtSpaceGraph* graph = FindSpaceGraph(database->LoaderAt(i));", "        AtSpaceGraph* graph = FindSpaceGraph(database->LoaderAt(0));"),
    ("find: first found", D, "        if (graph != 0) {\n            return graph;\n        }\n    }\n    return 0;", "    }\n    return 0;"),
    ("register: first empty", D, "        if (m_databases[i] == 0) {", "        if (m_databases[i] == 0 && i > 0) {"),
    ("register: no stop", D, "            m_databases[i] = database;\n            return;\n", "            m_databases[i] = database;\n"),
    # AOD.
    ("AOD slot 0: owner test", D, "if (m_database != 0 && m_database->m_owner != 0) {", "if (m_database != 0) {"),
    ("AOD slot 0: no delete", D, "        m_maker->DeleteDatabase(m_database);\n", ""),
    ("AOD slot 0: ran kept", D, "    }\n    m_ran = 0;\n    m_maker = 0;\n", "    }\n    m_maker = 0;\n"),
    ("AOD slot 0: database kept", D, "    AtFormat(m_extension, \"\");\n    m_database = 0;\n}\n\nvoid GmSoundDatabaseItem::Run", "    AtFormat(m_extension, \"\");\n}\n\nvoid GmSoundDatabaseItem::Run"),
    ("AOD run: twice", D, "    if (m_ran != 0) {\n        return;\n    }\n", ""),
    ("AOD run: owner test", D, "    if (m_maker != 0 && m_owner != 0) {", "    if (m_maker != 0) {"),
    ("AOD run: null database", D, "        if (m_database != 0) {\n            char unused", "        if (m_database != 0 || m_maker != 0) {\n            char unused"),
    ("AOD run: load", D, "            m_database->Load(m_owner, m_path, 0);\n", ""),
    ("AOD run: register", D, "            GmAudioData::s_instance->RegisterDatabase(m_database);\n", ""),
    ("AOD run: ran", D, "    m_ran = 1;\n", "    m_ran = 2;\n"),
    ("AOD configure: no slot 0", D, "void GmSoundDatabaseItem::Configure(const GmManifestItemRequest* request) {\n    Unknown0();\n", "void GmSoundDatabaseItem::Configure(const GmManifestItemRequest* request) {\n"),
    ("AOD configure: not a format", D, "    AtFormat(m_path, request->m_path, language);\n", "    strcpy(m_path, request->m_path);\n"),
    ("AOD configure: extension", D, "AtFormat(m_extension, \"AOD\");", "AtFormat(m_extension, \"AOB\");"),
    ("AOD configure: owner", D, "    m_owner = (AtNameTableOwner*)request->m_context2;\n    m_maker", "    m_owner = (AtNameTableOwner*)request->m_context;\n    m_maker"),
    ("AOD constructor: database", D, "    AtFormat(m_extension, \"\");\n    m_database = 0;\n}\n\nGmSoundDatabaseItem::~", "    AtFormat(m_extension, \"\");\n}\n\nGmSoundDatabaseItem::~"),
    ("AOD constructor: ran", D, "    m_next = 0;\n    m_ran = 0;\n", "    m_next = 0;\n"),
    ("AOD destructor: nothing", D, "    GmSoundDatabaseItem::Unknown0();\n", ""),
    # AGD.
    ("AGD slot 0: no delete", D, "        m_maker->DeleteLoader(m_loader);\n", ""),
    ("AGD slot 0: loader kept", D, "    AtFormat(m_extension, \"\");\n    m_loader = 0;\n}\n\nvoid GmGraphicsDatabaseItem::Run", "    AtFormat(m_extension, \"\");\n}\n\nvoid GmGraphicsDatabaseItem::Run"),
    ("AGD run: mode", D, "const int mode = g_usePlatformBinary != 0 ? 2 : 0;", "const int mode = g_usePlatformBinary != 0 ? 1 : 0;"),
    ("AGD run: prefix", D, "m_loader->Load(m_owner, m_path, mode, 0, 1);", "m_loader->Load(m_owner, m_path, mode, 0, 0);"),
    ("AGD configure: extension", D, "AtFormat(m_extension, \"AGD\");", "AtFormat(m_extension, \"AGS\");"),
    ("AGD configure: maker", D, "    m_maker = (AppGraphics*)request->m_context2;\n", ""),
    ("AGD configure: owner", D, "    m_owner = (AtMaterialOwner*)request->m_context;\n", "    m_owner = 0;\n"),
    ("AGD destructor: nothing", D, "    GmGraphicsDatabaseItem::Unknown0();\n", ""),
    ("AGD constructor: loader", D, "    AtFormat(m_extension, \"\");\n    m_loader = 0;\n}\n\nGmGraphicsDatabaseItem::~", "    AtFormat(m_extension, \"\");\n}\n\nGmGraphicsDatabaseItem::~"),
    # ASD.
    ("ASD slot 0: no clear", D, "void GmSceneDatabaseItem::Unknown0() {\n    m_database.Clear();\n", "void GmSceneDatabaseItem::Unknown0() {\n"),
    ("ASD slot 0: flag kept", D, "    m_owner = 0;\n    m_inSpaceGraph = 0;\n    AtFormat(m_extension, \"\");\n}\n\nvoid GmSceneDatabaseItem::Run", "    m_owner = 0;\n    AtFormat(m_extension, \"\");\n}\n\nvoid GmSceneDatabaseItem::Run"),
    ("ASD run: no load", D, "    AtFormat(unused, \"%3s][%s\", m_extension, m_path);\n    m_database.Load(m_owner, m_path, 0);\n}\n\nvoid GmSceneDatabaseItem::Configure", "}\n\nvoid GmSceneDatabaseItem::Configure"),
    ("ASD configure: extension", D, "AtFormat(m_extension, \"ASD\");", "AtFormat(m_extension, \"ASN\");"),
    ("ASD configure: flag", D, "    m_inSpaceGraph = (int)(GmDbSize)request->m_context2;\n", ""),
    ("ASD configure: owner", D, "    m_owner = (AtNameTableOwner*)request->m_context;\n    m_inSpaceGraph", "    m_owner = 0;\n    m_inSpaceGraph"),
    ("ASD destructor: nothing", D, "    GmSceneDatabaseItem::Unknown0();\n", ""),
    ("ASD constructor: flag", D, "    m_owner = 0;\n    m_inSpaceGraph = 0;\n    AtFormat(m_extension, \"\");\n}\n\nGmSceneDatabaseItem::~", "    m_owner = 0;\n    AtFormat(m_extension, \"\");\n}\n\nGmSceneDatabaseItem::~"),
    # ACD.
    ("ACD slot 0: no clear", D, "void GmCinemaDatabaseItem::Unknown0() {\n    m_database.Clear();\n", "void GmCinemaDatabaseItem::Unknown0() {\n"),
    ("ACD slot 0: owner kept", D, "void GmCinemaDatabaseItem::Unknown0() {\n    m_database.Clear();\n    m_owner = 0;\n", "void GmCinemaDatabaseItem::Unknown0() {\n    m_database.Clear();\n"),
    ("ACD run: no load", D, "    AtFormat(unused, \"%3s][%s\", m_extension, m_path);\n    m_database.Load(m_owner, m_path, 0);\n}\n\nvoid GmCinemaDatabaseItem::Configure", "}\n\nvoid GmCinemaDatabaseItem::Configure"),
    ("ACD configure: extension", D, "AtFormat(m_extension, \"ACD\");", "AtFormat(m_extension, \"ACE\");"),
    ("ACD configure: owner", D, "    m_owner = (GmCinemaOwner*)request->m_context;\n", ""),
    ("ACD destructor: nothing", D, "    GmCinemaDatabaseItem::Unknown0();\n", ""),
    # ASB.
    ("ASB slot 0: no clear", D, "void GmBoundingGeometryItem::Unknown0() {\n    m_database.Clear();\n", "void GmBoundingGeometryItem::Unknown0() {\n"),
    ("ASB run: no load", D, "    AtFormat(unused, \"%3s][%s\", m_extension, m_path);\n    m_database.Load(m_owner, m_path, 0);\n}\n\nvoid GmBoundingGeometryItem::Configure", "}\n\nvoid GmBoundingGeometryItem::Configure"),
    ("ASB configure: extension", D, "AtFormat(m_extension, \"ASB\");", "AtFormat(m_extension, \"ASC\");"),
    ("ASB configure: owner", D, "    m_owner = (AtNameTableOwner*)request->m_context;\n}\n\n// ---- AEE", "    m_owner = (AtNameTableOwner*)request->m_context2;\n}\n\n// ---- AEE"),
    ("ASB destructor: nothing", D, "    GmBoundingGeometryItem::Unknown0();\n", ""),
    # AEE.
    ("AEE slot 0: no clear", D, "void GmEventDatabaseItem::Unknown0() {\n    m_database.Clear();\n", "void GmEventDatabaseItem::Unknown0() {\n"),
    ("AEE slot 0: path kept", D, "void GmEventDatabaseItem::Unknown0() {\n    m_database.Clear();\n    m_path[0] = 0;\n", "void GmEventDatabaseItem::Unknown0() {\n    m_database.Clear();\n"),
    ("AEE run: no load", D, "    AtFormat(unused, \"%3s][%s\", m_extension, m_path);\n    m_database.Load(m_owner, m_path, 0);\n}\n\nvoid GmEventDatabaseItem::Configure", "}\n\nvoid GmEventDatabaseItem::Configure"),
    ("AEE configure: extension", D, "AtFormat(m_extension, \"AEE\");", "AtFormat(m_extension, \"AES\");"),
    ("AEE configure: owner", D, "    m_owner = (AtDataRegistryOwner*)request->m_context;\n", ""),
    ("AEE configure: path", D, "    m_owner = (AtDataRegistryOwner*)request->m_context;\n    strcpy(m_path, request->m_path);\n", "    m_owner = (AtDataRegistryOwner*)request->m_context;\n"),
    ("AEE destructor: nothing", D, "    GmEventDatabaseItem::Unknown0();\n", ""),
    # ADR.
    ("ADR slot 0: no clear", D, "    m_registry.Clear();\n", ""),
    ("ADR slot 0: parent kept", D, "    m_registry.Clear();\n    m_parent = 0;\n", "    m_registry.Clear();\n"),
    ("ADR run: no load", D, "    m_registry.Load(m_owner, m_path, 0, 0, m_parent);\n", ""),
    ("ADR run: no parent", D, "m_registry.Load(m_owner, m_path, 0, 0, m_parent);", "m_registry.Load(m_owner, m_path, 0, 0, 0);"),
    ("ADR run: no add", D, "    m_manager->Add(&m_registry);\n", ""),
    ("ADR configure: extension", D, "AtFormat(m_extension, \"ADR\");", "AtFormat(m_extension, \"ADD\");"),
    ("ADR configure: manager", D, "    m_manager = (GmRegistryNames*)request->m_context;\n", ""),
    ("ADR configure: parent", D, "    m_parent = (AtDataRegistry*)request->m_context2;\n", ""),
    ("ADR configure: owner", D, "    m_owner = (AtDataRegistryOwner*)request->m_context3;\n", "    m_owner = (AtDataRegistryOwner*)request->m_context2;\n"),
    ("ADR destructor: nothing", D, "    GmRegistryItem::Unknown0();\n", ""),
    # The scene loader's walks.
    ("insert: nodes", D, "        m_nodes[i].InsertInto(graph);\n", ""),
    ("insert: nested", D, "        m_loaders[i].InsertInto(graph);\n", ""),
    ("insert: order", D,
     "    for (unsigned int i = 0; i < m_nodeCount; i++) {\n        m_nodes[i].InsertInto(graph);\n    }\n    for (unsigned int i = 0; i < m_loaderCount; i++) {\n        m_loaders[i].InsertInto(graph);\n    }\n",
     "    for (unsigned int i = 0; i < m_loaderCount; i++) {\n        m_loaders[i].InsertInto(graph);\n    }\n    for (unsigned int i = 0; i < m_nodeCount; i++) {\n        m_nodes[i].InsertInto(graph);\n    }\n"),
    ("remove: nodes", D, "        m_nodes[i].RemoveFrom(graph);\n", ""),
    ("remove: nested", D, "        m_loaders[i].RemoveFrom(graph);\n", ""),
    ("remove: order", D,
     "    for (unsigned int i = 0; i < m_loaderCount; i++) {\n        m_loaders[i].RemoveFrom(graph);\n    }\n    for (unsigned int i = 0; i < m_nodeCount; i++) {\n        m_nodes[i].RemoveFrom(graph);\n    }\n",
     "    for (unsigned int i = 0; i < m_nodeCount; i++) {\n        m_nodes[i].RemoveFrom(graph);\n    }\n    for (unsigned int i = 0; i < m_loaderCount; i++) {\n        m_loaders[i].RemoveFrom(graph);\n    }\n"),
    # AtCinemaDatabase.
    ("cinema clear: order", D, "    DeleteObjects(m_entries18);\n    DeleteObjects(m_entries08);\n", "    DeleteObjects(m_entries08);\n    DeleteObjects(m_entries18);\n"),
    ("cinema clear: 14 kept", D, "    DeleteObjects(m_entries14);\n", ""),
    ("cinema clear: 04", D, "    DeleteObjects(m_entries14);\n    m_unknown04 = 0;\n", "    DeleteObjects(m_entries14);\n"),
    ("cinema clear: count 22", D, "    m_count20 = 0;\n    m_count22 = 0;\n    m_count1C = 0;\n    m_count24 = 0;\n}\n\n// ----", "    m_count20 = 0;\n    m_count1C = 0;\n    m_count24 = 0;\n}\n\n// ----"),
    ("cinema clear: pointer 18", D, "    m_entries08 = 0;\n    m_entries18 = 0;\n    m_count1E = 0;\n    m_count20 = 0;\n    m_count22 = 0;\n    m_count1C = 0;\n    m_count24 = 0;\n}\n\n// ----",
     "    m_entries08 = 0;\n    m_count1E = 0;\n    m_count20 = 0;\n    m_count22 = 0;\n    m_count1C = 0;\n    m_count24 = 0;\n}\n\n// ----"),
    ("cinema destructor: nothing", D, "    AtCinemaDatabase::Clear();\n", ""),
    ("cinema constructor: 04", D, "AtCinemaDatabase::AtCinemaDatabase() {\n    m_unknown04 = 0;\n", "AtCinemaDatabase::AtCinemaDatabase() {\n"),
    ("cinema constructor: count 24", D, "    m_count1C = 0;\n    m_count24 = 0;\n}\n\nAtCinemaDatabase::~", "    m_count1C = 0;\n}\n\nAtCinemaDatabase::~"),
    # AtEventList and AtEventDatabase: their life cycle and its mutants are at/aee's now (tests/oracle/suites/aee.py).
]
SUITE = {"target": "oracle_dbmanifest", "subsystem": "gm/dbmanifest", "src": "gm", "var": "KND_GM_SRC", "cases": "tok", "mutants": MUTANTS}
