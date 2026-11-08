# Mutants of the .ASD scene lists, .ASC cell graphs and .ASI and .AST flat lists. Each text occurs exactly once in its file. See docs/verification.md.
#
# Dropped as equivalent, each with its reason:
#   * A portal's own fields that every reading of one sets again (centre, normal, vertices, count, stride, first vertex, array): Read always ends with the centre and normal, and a portal without a Vertices word reads through a
#     null pointer on both sides, whatever the constructor left.
#   * AtCellGraph::Read's own test for an earlier read: no loader reaches it, since a cell graph list destroys its graphs before reading new ones.
#   * AtImageDraw::Read's and AtTextDraw::Read's own test for an earlier read: the base's Read, which follows, makes the same test and the same virtual call.
#   * The constructors' zeroing of a list's array and count, and of an item's scales: each is set before it is read, and an object that fails before its read is not compared.
#   * The loader's destructor clearing whether loaded or not: on a loader never loaded, Clear finds only nulls and empty maps.
#   * The mode passed to the .AST, .ASI and .ASC loads: each takes it and does not use it, on either side.
#   * A cell's graph in its constructor, a portal list's own clearing of the first byte of a name space: each is set before it is read. A graph's Clear zeroing its mesh and +0x44: Clear is reached only from the destructor.
#   * Not closing a .ASC after reading it: the file's destructor, which runs next, closes it.
#   * The colour rectangle's constructor whitening: Reset, which the item's constructor calls next, whitens it again.
#   * The base item's Read testing for an earlier read: the derived Read has already cleared. The derived destructors' Reset: the base destructor resets again.
#   * A z sum first written through a double: the same rounding, written another way; a mistake of mine, kept out.
# Not equivalent, and not observed: the order of the three name arrays' allocation and of their freeing. They are the same size, and the harness logs a free by its size, not by which
# block it was. A gap in the harness, not closed.
L = "AtSceneDatabaseLoader.cpp"
C = "AtCellGraph.cpp"
D = "AtDrawItem.cpp"

M = []
add = M.append

# ---- the scene list -------------------------------------------------------------------------------------------------------------------------
add(("ReadCount: twice allowed", L, "    if (count > 0) {\n        tokenizer->Error(kListAlreadyGiven);", "    if (count > 1) {\n        tokenizer->Error(kListAlreadyGiven);"))
add(("ReadNames: not cleared", L, "        names[i] = 0;\n", ""))
add(("ReadNames: heap code", L, "    if (AtIoTokenFile::s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);", "    if (AtIoTokenFile::s_heap == 0) {\n        AtRaise(-3, 0, 0, 0);"))
add(("ReadNames: no memory code", L, "    if (names == 0) {\n        AtRaise(-4, 0, 0, 0);", "    if (names == 0) {\n        AtRaise(-5, 0, 0, 0);"))
add(("ReadNames: names off the token heap", L, "    AtPushHeap(AtIoTokenFile::s_heap);\n    for (unsigned int i = 0; i < count; i++) {\n        tokenizer->ExpectStringCopy", "    AtPushHeap(AtSceneDatabaseLoader::s_heap);\n    for (unsigned int i = 0; i < count; i++) {\n        tokenizer->ExpectStringCopy"))
add(("FreeList: names kept", L, "            if (names[i] != 0) {\n                operator delete(names[i]);\n            }\n", ""))
add(("FreeList: list kept", L, "        operator delete(names);\n", ""))
add(("FreeList: not forgotten", L, "        names = 0;\n", ""))
add(("MakeList: heap code", L, "    if (AtSceneDatabaseLoader::s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);", "    if (AtSceneDatabaseLoader::s_heap == 0) {\n        AtRaise(-3, 0, 0, 0);"))
add(("MakeList: wrong heap", L, "    AtPushHeap(AtSceneDatabaseLoader::s_heap);\n    T* list", "    AtPushHeap(AtIoTokenFile::s_heap);\n    T* list"))
add(("MakeList: no memory code", L, "    if (list == 0) {\n        AtRaise(-4, 0, 0, 0);", "    if (list == 0) {\n        AtRaise(-5, 0, 0, 0);"))
add(("DeleteList: kept", L, "    if (list != 0) {\n        AtDeleteArray(list);", "    if (list == 0) {\n        AtDeleteArray(list);"))
for make, count, other in [("m_imageDraws = MakeList<AtImageDrawDatabase>(", "m_imageDrawCount", "m_textDrawCount"), ("m_textDraws = MakeList<AtTextDrawDatabase>(", "m_textDrawCount", "m_imageDrawCount"),
                           ("m_strings = MakeList<AtTextTable>(", "m_stringCount", "m_nodeCount"), ("m_nodes = MakeList<AtScene>(", "m_nodeCount", "m_stringCount"),
                           ("m_modifiers = MakeList<AtModifierDatabase>(", "m_modifierCount", "m_emitterCount"), ("m_emitters = MakeList<AtEmitterDatabase>(", "m_emitterCount", "m_modifierCount"),
                           ("m_lensFlares = MakeList<AtLensFlareDatabase>(", "m_lensFlareCount", "m_cellGraphCount"), ("m_cellGraphs = MakeList<AtCellGraphDatabase>(", "m_cellGraphCount", "m_lensFlareCount"),
                           ("m_spaceGraphs = MakeList<AtSpaceGraph>(", "m_spaceGraphCount", "m_geometryCount"), ("m_geometry = MakeList<AtBoundingGeometryDatabase>(", "m_geometryCount", "m_spaceGraphCount"),
                           ("m_loaders = MakeList<AtSceneDatabaseLoader>(", "m_loaderCount", "m_nodeCount")]:
    add(("Make %s: count" % count, L, make + count + ");", make + other + ");"))
add(("MakeNodes: map not made", L, "    m_nodesByName.Init(m_nodeCount);\n", ""))
add(("MakeSpaceGraphs: map not made", L, "    m_spaceGraphsByName.Init(m_spaceGraphCount);\n", ""))
add(("MakeSpaceGraphs: not on the owner", L, "    m_owner->AddSpaceGraphs(&m_spaceGraphsByName);\n", ""))
add(("NodeDatabaseAt", L, "    return m_nodes + index;", "    return m_nodes + index + 1;"))
add(("SpaceGraphAt", L, "    return m_spaceGraphs + index;", "    return m_spaceGraphs + (index ^ 1);"))
add(("LoaderAt", L, "    return m_loaders + index;", "    return m_loaders;"))
for field in ["m_owner", "m_nodes", "m_modifiers", "m_emitters", "m_lensFlares", "m_cellGraphs", "m_spaceGraphs", "m_loaders", "m_geometry", "m_strings", "m_imageDraws", "m_textDraws", "m_loaderNames",
              "m_nodeCount", "m_modifierCount", "m_emitterCount", "m_lensFlareCount", "m_spaceGraphCount", "m_loaderCount", "m_geometryCount", "m_stringCount", "m_cellGraphCount", "m_imageDrawCount", "m_textDrawCount"]:
    add(("Zero: %s" % field, L, "    %s = 0;\n" % field, ""))
for lst in ["m_geometry", "m_loaders", "m_cellGraphs", "m_emitters", "m_lensFlares", "m_modifiers", "m_nodes", "m_strings", "m_imageDraws", "m_textDraws", "m_spaceGraphs"]:
    add(("Clear: %s kept" % lst, L, "    DeleteList(%s);\n" % lst, ""))
add(("Clear: graph map kept", L, "    m_spaceGraphsByName.Clear();\n", ""))
add(("Clear: node map kept", L, "    m_nodesByName.Clear();\n", ""))
add(("Clear: not zeroed", L, "    Zero();\n}\n\nvoid AtSceneDatabaseLoader::LoadDatabases", "}\n\nvoid AtSceneDatabaseLoader::LoadDatabases"))
add(("Clear: order", L, "    DeleteList(m_geometry);\n    DeleteList(m_loaders);\n", "    DeleteList(m_loaders);\n    DeleteList(m_geometry);\n"))
LOADS = [("strings", "        m_strings[i].Load((AtIoContext*)owner->m_loader, s_stringNames[i], mode);\n"), ("text", "        m_textDraws[i].Load(owner, s_textDrawNames[i], mode);\n"),
         ("images", "        m_imageDraws[i].Load(owner, s_imageDrawNames[i], mode);\n"), ("geometry", "        m_geometry[i].Load(owner, s_geometryNames[i], mode);\n"),
         ("modifiers", "        m_modifiers[i].Load(owner, s_modifierNames[i], mode);\n"), ("emitters", "        m_emitters[i].Load(owner, s_emitterNames[i], mode);\n"),
         ("lens flares", "        m_lensFlares[i].Load(owner, s_lensFlareNames[i], mode);\n"), ("nodes", "        m_nodes[i].Load(owner, s_nodeNames[i], mode);\n"),
         ("cell graphs", "        m_cellGraphs[i].Load(owner, s_cellGraphNames[i], mode);\n"), ("space graphs", "        m_spaceGraphs[i].Load(owner, s_spaceGraphNames[i], mode);\n"),
         ("nested", "        m_loaders[i].Load(owner, m_loaderNames[i], mode);\n")]
for name, text in LOADS:
    add(("load %s: dropped" % name, L, text, ""))
    if name not in ("text", "images", "cell graphs"):
        add(("load %s: mode" % name, L, text, text.replace(", mode);", ", 0);")))
add(("load strings: owner, not its context", L, "m_strings[i].Load((AtIoContext*)owner->m_loader,", "m_strings[i].Load((AtIoContext*)owner,"))
add(("load text: index", L, "m_textDraws[i].Load(owner, s_textDrawNames[i], mode);", "m_textDraws[0].Load(owner, s_textDrawNames[i], mode);"))
add(("load emitters: name", L, "m_emitters[i].Load(owner, s_emitterNames[i], mode);", "m_emitters[i].Load(owner, s_emitterNames[0], mode);"))
add(("node map: not inserted", L, "        m_nodesByName.Insert(leaf, m_nodes + i);\n", ""))
add(("node map: full name", L, "        AtLeafName(s_nodeNames[i], leaf);\n        m_nodesByName.Insert(leaf, m_nodes + i);", "        AtLeafName(s_nodeNames[i], leaf);\n        m_nodesByName.Insert(s_nodeNames[i], m_nodes + i);"))
add(("graph map: not inserted", L, "        m_spaceGraphsByName.Insert(leaf, m_spaceGraphs + i);\n", ""))
add(("graph map: value", L, "        m_spaceGraphsByName.Insert(leaf, m_spaceGraphs + i);", "        m_spaceGraphsByName.Insert(leaf, m_spaceGraphs);"))
add(("order: nodes before strings", L, "    for (unsigned int i = 0; i < m_stringCount; i++) {", "    for (unsigned int i = 0; i < m_nodeCount; i++) {\n        m_nodes[i].Load(owner, s_nodeNames[i], mode);\n    }\n    for (unsigned int i = 0; i < m_stringCount; i++) {"))
add(("free names: when not asked", L, "    if (freeNames != 0) {", "    if (freeNames == 0) {"))
for names, count in [("s_cellGraphNames", "m_cellGraphCount"), ("s_emitterNames", "m_emitterCount"), ("s_lensFlareNames", "m_lensFlareCount"), ("s_geometryNames", "m_geometryCount"),
                     ("s_spaceGraphNames", "m_spaceGraphCount"), ("s_modifierNames", "m_modifierCount"), ("s_nodeNames", "m_nodeCount"), ("s_stringNames", "m_stringCount"),
                     ("s_imageDrawNames", "m_imageDrawCount"), ("s_textDrawNames", "m_textDrawCount")]:
    add(("free %s: kept" % names, L, "        FreeList(%s, %s);\n" % (names, count), ""))
add(("free names: order", L, "        FreeList(s_cellGraphNames, m_cellGraphCount);\n        FreeList(s_emitterNames, m_emitterCount);\n", "        FreeList(s_emitterNames, m_emitterCount);\n        FreeList(s_cellGraphNames, m_cellGraphCount);\n"))
add(("free names: wrong count", L, "        FreeList(s_nodeNames, m_nodeCount);\n", "        FreeList(s_nodeNames, 1);\n"))
add(("Load: no clear", L, "    if (m_owner != 0) {\n        Clear();\n    }\n    const int status = file.Open(name, \"ASD\", 2, 0);", "    const int status = file.Open(name, \"ASD\", 2, 0);"))
add(("Load: flags", L, "file.Open(name, \"ASD\", 2, 0);", "file.Open(name, \"ASD\", 1, 0);"))
add(("Load: report", L, "        AtReportFileError(status, name, \"ASD\");", "        AtReportFileError(status, name, \"ASX\");"))
add(("Load: owner", L, "    m_owner = owner;\n", ""))
add(("Load: no directory", L, "    fs->m_path.PushFromFile(file.m_file.m_file->m_path);\n", ""))
add(("Load: keywords", L, "    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);", "    tokenizer->SetKeywords(s_keywords, kKeywordCount - 1, 0);"))
for count, make, names in [("m_nodeCount", "MakeNodes", "s_nodeNames"), ("m_modifierCount", "MakeModifiers", "s_modifierNames"), ("m_spaceGraphCount", "MakeSpaceGraphs", "s_spaceGraphNames"),
                           ("m_geometryCount", "MakeGeometry", "s_geometryNames"), ("m_loaderCount", "MakeLoaders", "m_loaderNames"), ("m_emitterCount", "MakeEmitters", "s_emitterNames"),
                           ("m_lensFlareCount", "MakeLensFlares", "s_lensFlareNames"), ("m_cellGraphCount", "MakeCellGraphs", "s_cellGraphNames"), ("m_stringCount", "MakeStrings", "s_stringNames"),
                           ("m_imageDrawCount", "MakeImageDraws", "s_imageDrawNames"), ("m_textDrawCount", "MakeTextDraws", "s_textDrawNames")]:
    add(("case %s: no count" % make, L, "            ReadCount(tokenizer, %s);\n" % count, ""))
    add(("case %s: not made" % make, L, "            %s();\n" % make, ""))
    add(("case %s: no names" % make, L, "            ReadNames(tokenizer, %s, %s);\n" % (count, names), ""))
add(("Load: unknown word silent", L, "        default:\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n", "        default:\n"))
add(("Load: no right curly", L, "        tokenizer->ExpectRightCurly();\n        token = tokenizer->Next();\n    }\n    file.Close();", "        token = tokenizer->Next();\n    }\n    file.Close();"))
add(("Load: not closed first", L, "    file.Close();\n    LoadDatabases", "    LoadDatabases"))
add(("Load: names kept", L, "    LoadDatabases(owner, mode, 1);", "    LoadDatabases(owner, mode, 0);"))
add(("Load: mode", L, "    LoadDatabases(owner, mode, 1);", "    LoadDatabases(owner, 0, 1);"))
add(("Load: directory kept", L, "    fs->m_path.Pop(0);\n", ""))
add(("Load: nested names kept", L, "    FreeList(m_loaderNames, m_loaderCount);\n}", "}"))
add(("keywords: node word", L, "{0x39, \"NodeDatabase\"}", "{0x3A, \"NodeDatabase\"}"))
add(("keywords: loader word", L, "{0x3D, \"DatabaseLoader\"}", "{0x3D, \"DatabaseLoaders\"}"))

# ---- cell graphs ------------------------------------------------------------------------------------------------------------------------------
add(("NewNames: heap code", C, "        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(AtIoTokenFile::s_heap);", "        AtRaise(-3, 0, 0, 0);\n    }\n    AtPushHeap(AtIoTokenFile::s_heap);"))
add(("NewNames: wrong heap", C, "    AtPushHeap(AtIoTokenFile::s_heap);", "    AtPushHeap(AtCellGraph::s_heap);"))
add(("NewNames: size", C, "operator new(count * 8);", "operator new(count * 9);"))
add(("NewNames: no memory code", C, "    if (names == 0) {\n        AtRaise(-4, 0, 0, 0);", "    if (names == 0) {\n        AtRaise(-5, 0, 0, 0);"))
add(("VertexArray: own when none", C, "    if (m_ownVertexArrays == 0) {", "    if (m_ownVertexArrays == 1) {"))
add(("VertexArray: bound", C, "    if (index >= m_ownVertexArrays) {", "    if (index > m_ownVertexArrays) {"))
add(("VertexArray: index", C, "    return VertexArrayAt(index);\n", "    return VertexArrayAt(0);\n"))
add(("VertexArray: database index", C, "        return m_database->VertexArrayAt(index);", "        return m_database->VertexArrayAt(index + 1);"))
add(("AddCellGraphs: test", C, "    if (ns == 0) {\n        m_cellTables.Add(map);", "    if (ns != 0) {\n        m_cellTables.Add(map);"))
add(("AddSpaceGraphs: not added", C, "    m_spaceGraphs.Add(map);\n", ""))
for field in ["m_opposite", "m_oppositeGraph", "m_flags", "m_closedDistanceSquared"]:
    add(("portal ctor: %s" % field, C, "    %s = 0;\n" % field, ""))
add(("portal: read again not reset", C, "    if (m_flags & 1) {\n        new (this, AtPlacement()) AtCellPortal;\n    }\n", ""))
add(("portal: no left curly", C, "    tokenizer->ExpectLeftCurly();\n    int token = tokenizer->Next();\n    while (token != AtIoTokenizer::kRightCurly) {\n        switch", "    int token = tokenizer->Next();\n    while (token != AtIoTokenizer::kRightCurly) {\n        switch"))
add(("portal: other graph's graph name", C, "            tokenizer->ExpectName(AtCellGraph::s_graphNames + AtCellGraph::s_portalIndex * 8);\n", "            tokenizer->ExpectName(AtCellGraph::s_spaceNames + AtCellGraph::s_portalIndex * 8);\n"))
add(("portal: other graph's space", C, "            tokenizer->ExpectName(AtCellGraph::s_spaceNames + AtCellGraph::s_portalIndex * 8);\n", "            tokenizer->ExpectName(AtCellGraph::s_spaceNames);\n"))
add(("portal: other graph's cell", C, "            tokenizer->ExpectName(AtCellGraph::s_cellNames + AtCellGraph::s_portalIndex * 8);\n            break;\n        case kOppositeCell:", "            tokenizer->ExpectName(AtCellGraph::s_graphNames + AtCellGraph::s_portalIndex * 8);\n            break;\n        case kOppositeCell:"))
add(("portal: opposite cell", C, "        case kOppositeCell:\n            tokenizer->ExpectName(AtCellGraph::s_cellNames", "        case kOppositeCell:\n            tokenizer->ExpectName(AtCellGraph::s_graphNames"))
add(("portal: lock order", C, "graph->LockedVertex(m_vertexArray, m_firstVertex);", "graph->LockedVertex(m_firstVertex, m_vertexArray);"))
add(("portal: vertices too many", C, "if (m_vertexCount >= 0x20 || m_vertexCount < 3) {", "if (m_vertexCount > 0x20 || m_vertexCount < 3) {"))
add(("portal: vertices too few", C, "if (m_vertexCount >= 0x20 || m_vertexCount < 3) {", "if (m_vertexCount >= 0x20 || m_vertexCount < 4) {"))
add(("portal: stride", C, "            m_stride = (unsigned short)AtCellGraph::s_lock.m_stride;\n", ""))
add(("portal: closed", C, "            m_flags |= kClosedBit1 | kClosedBit3;", "            m_flags |= kClosedBit1;"))
add(("portal: closed by distance flag", C, "            m_flags |= kClosedByViewerDist;\n", ""))
add(("portal: distance not squared", C, "            m_closedDistanceSquared = distance * distance;", "            m_closedDistanceSquared = distance;"))
add(("portal: unknown word silent", C, "        default:\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n            break;\n        }\n        token = tokenizer->Next();\n    }\n    ComputeCenter();", "        default:\n            break;\n        }\n        token = tokenizer->Next();\n    }\n    ComputeCenter();"))
add(("portal: no centre", C, "    ComputeCenter();\n    ComputeNormal();", "    ComputeNormal();"))
add(("portal: no normal", C, "    ComputeCenter();\n    ComputeNormal();", "    ComputeCenter();"))
add(("centre: y kept as float", C, "        double y = (double)sumY + v0[1];", "        float y = (float)((double)sumY + v0[1]);"))
add(("centre: x kept in a register", C, "        float x = (float)(sumX + v1[0]);", "        double x = sumX + v1[0];"))
add(("centre: z stored", C, "        const double z = (double)sumZ + v1[2];", "        const float z = (float)((double)sumZ + v1[2]);"))
add(("centre: x stored at the end of four", C, "        sumX = (double)x + v3[0];", "        sumX = (float)((double)x + v3[0]);"))
add(("centre: tail x stored", C, "        sumX += v[0];\n", "        sumX = (float)(sumX + v[0]);\n"))
add(("centre: scale kept", C, "    const float stored = (float)inverse;", "    const double stored = inverse;"))
add(("centre: x by the stored scale", C, "    m_center[0] = (float)(inverse * sumX);", "    m_center[0] = (float)((double)stored * sumX);"))
add(("centre: no unrolling", C, "    for (; i + 4 <= count; i += 4) {", "    for (; i + 4 < 0; i += 4) {"))
add(("centre: y of the last of four", C, "        sumY = (float)((double)sumY + v3[1]);", "        sumY = (float)((double)sumY + v3[0]);"))
add(("normal: bx kept", C, "    const float bx = (float)((double)v1[0] - v0[0]);", "    const double bx = (double)v1[0] - v0[0];"))
add(("normal: by kept", C, "    const float by = (float)((double)v1[1] - v0[1]);", "    const double by = (double)v1[1] - v0[1];"))
add(("normal: bz stored", C, "    const double bz = (double)v1[2] - v0[2];", "    const float bz = (float)((double)v1[2] - v0[2]);"))
add(("normal: ax stored", C, "    const double ax = (double)v2[0] - v0[0];", "    const float ax = (float)((double)v2[0] - v0[0]);"))
add(("normal: sign", C, "    m_normal[0] = (float)(bz * ay - by * az);", "    m_normal[0] = (float)(by * az - bz * ay);"))
add(("normal: z", C, "    m_normal[2] = (float)(by * ax - bx * ay);", "    m_normal[2] = (float)(by * ax - bx * az);"))
add(("resolve: empty cell name", C, "    if (cellName[0] == 0) {\n        return;\n    }\n", ""))
add(("resolve: graph name test", C, "    if (graphName[0] != 0) {", "    if (graphName[0] == 0) {"))
add(("resolve: empty name space", C, "        if (ns != 0 && ns[0] != 0) {\n            m_oppositeGraph", "        if (ns != 0) {\n            m_oppositeGraph"))
add(("resolve: graph message", C, "\"Atlas Scene: invalid cell graph reference in portal\"", "\"Atlas Scene: invalid cell graph reference\""))
add(("resolve: cell message", C, "\"Atlas Scene: invalid cell reference in portal\"", "\"Atlas Scene: invalid cell reference\""))
add(("resolve: this graph", C, "        m_oppositeGraph = graph;\n", ""))
add(("resolve: cell from this graph", C, "    m_opposite = (AtCell*)m_oppositeGraph->m_cells.Find(cellName);", "    m_opposite = (AtCell*)graph->m_cells.Find(cellName);"))
add(("cell ctor: 0C", C, "    m_unknown0C = 0;\n", ""))
add(("cell ctor: 14", C, "    m_unknown14 = 0;\n", ""))
add(("cell ctor: count", C, "    m_portals = 0;\n    m_portalCount = 0;\n    m_unknown0C", "    m_portals = 0;\n    m_unknown0C"))
add(("cell: graph", C, "    m_graph = graph;\n", ""))
add(("cell: word", C, "        if (token == kPortals) {", "        if (token == kPortals + 1) {"))
add(("cell: bound", C, "            if (m_portalCount + first > graph->m_portalCount) {", "            if (m_portalCount + first >= graph->m_portalCount) {"))
add(("cell: first", C, "            m_portals = graph->m_portals + first;", "            m_portals = graph->m_portals;"))
add(("cell: unknown word silent", C, "        } else {\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n        }\n", "        }\n"))
add(("graph ctor: mesh", C, "AtCellGraph::AtCellGraph() {\n    m_mesh = 0;\n", "AtCellGraph::AtCellGraph() {\n"))
add(("graph ctor: 44", C, "    m_unknown44 = 0;\n}\n\nAtCellGraph::~AtCellGraph", "}\n\nAtCellGraph::~AtCellGraph"))
add(("graph dtor: no clear", C, "AtCellGraph::~AtCellGraph() {\n    Clear();\n}", "AtCellGraph::~AtCellGraph() {\n}"))
add(("graph clear: cells map", C, "    m_cells.Clear();\n", ""))
add(("graph clear: cells kept", C, "    if (m_cellArray != 0) {\n        operator delete(m_cellArray);\n    }\n", ""))
add(("graph clear: portal map", C, "    m_portalNames.Clear();\n", ""))
add(("graph clear: portals kept", C, "    if (m_portals != 0) {\n        operator delete(m_portals);\n    }\n", ""))
add(("graph clear: order", C, "    m_cells.Clear();\n    if (m_cellArray != 0) {", "    if (m_cellArray != 0) {"))
add(("lock: kept when unlocked", C, "    if (vertexArray != s_lockedArray || s_lock.m_data == 0) {", "    if (vertexArray != s_lockedArray) {"))
add(("lock: no unlock", C, "        if (vertexArray != s_lockedArray && s_lock.m_data != 0) {\n            m_mesh->VertexArray(s_lockedArray)->Slot6();\n        }\n", ""))
add(("lock: unlock the new", C, "        if (vertexArray != s_lockedArray && s_lock.m_data != 0) {\n            m_mesh->VertexArray(s_lockedArray)->Slot6();", "        if (vertexArray != s_lockedArray && s_lock.m_data != 0) {\n            m_mesh->VertexArray(vertexArray)->Slot6();"))
add(("lock: not remembered", C, "        s_lockedArray = vertexArray;\n", ""))
add(("lock: argument", C, "->Lock(&s_lock, 0);", "->Lock(&s_lock, 1);"))
add(("lock: vertex", C, "    return (const char*)s_lock.m_data + s_lock.m_stride * vertex;", "    return (const char*)s_lock.m_data + s_lock.m_stride * (vertex + 1);"))
add(("graph read: no left curly", C, "    tokenizer->ExpectLeftCurly();\n    s_lock.m_data = 0;", "    s_lock.m_data = 0;"))
add(("graph read: lock remembered", C, "    s_lock.m_data = 0;\n", ""))
add(("graph read: mesh name space", C, "        if (ns[0] != 0) {\n            m_mesh = (AtMesh*)device->m_meshSpaces.Find(ns, name);", "        if (ns[0] == 0) {\n            m_mesh = (AtMesh*)device->m_meshSpaces.Find(ns, name);"))
add(("graph read: mesh report", C, "            tokenizer->Error(kNotFound);", "            tokenizer->Error(kOutOfRange);"))
add(("graph read: portal word", C, "        if (token == kPortal) {", "        if (token == kPortal + 1) {"))
add(("graph read: portal count silent", C, "            if (m_portalCount < 1) {\n                tokenizer->Error(kOutOfRange);\n            }\n", ""))
add(("graph read: portal heap code", C, "            if (s_heap == 0) {\n                AtRaise(-2, 0, 0, 0);", "            if (s_heap == 0) {\n                AtRaise(-3, 0, 0, 0);"))
add(("graph read: portal heap", C, "            AtPushHeap(s_heap);\n            {\n                const unsigned int count = m_portalCount;", "            AtPushHeap(AtIoTokenFile::s_heap);\n            {\n                const unsigned int count = m_portalCount;"))
add(("graph read: portals not made", C, "                        new (portals + i, AtPlacement()) AtCellPortal;\n", ""))
add(("graph read: portal no memory", C, "            if (m_portals == 0) {\n                AtRaise(-4, 0, 0, 0);", "            if (m_portals == 0) {\n                AtRaise(-5, 0, 0, 0);"))
add(("graph read: portal map", C, "            m_portalNames.Init(m_portalCount);\n", ""))
add(("graph read: graph name kept", C, "                s_graphNames[s_portalIndex * 8] = 0;\n", ""))
add(("graph read: cell name kept", C, "                s_cellNames[s_portalIndex * 8] = 0;\n", ""))
add(("graph read: portal word not expected", C, "                tokenizer->ExpectToken(kPortal);\n", ""))
add(("graph read: portal name", C, "                m_portalNames.Insert(name, m_portals + s_portalIndex);", "                m_portalNames.Insert(name, m_portals);"))
add(("graph read: portal not read", C, "                m_portals[s_portalIndex].Read(this, tokenizer);\n", ""))
add(("graph read: portals' right curly", C, "            tokenizer->ExpectRightCurly();\n            token = tokenizer->Next();\n        }\n    }", "            token = tokenizer->Next();\n        }\n    }"))
add(("graph read: cell word silent", C, "    if (token != kCellWord) {\n        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n    }\n", ""))
add(("graph read: cell count silent", C, "    if (m_cellCount < 1) {\n        tokenizer->Error(kOutOfRange);\n    }\n", ""))
add(("graph read: cell heap code", C, "    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    {", "    if (s_heap == 0) {\n        AtRaise(-3, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    {"))
add(("graph read: cells not made", C, "                new (cells + i, AtPlacement()) AtCell;\n", ""))
add(("graph read: cell no memory", C, "    if (m_cellArray == 0) {\n        AtRaise(-4, 0, 0, 0);", "    if (m_cellArray == 0) {\n        AtRaise(-5, 0, 0, 0);"))
add(("graph read: cell map", C, "    m_cells.Init(m_cellCount);\n", ""))
add(("graph read: cell word not expected", C, "        tokenizer->ExpectToken(kCellWord);\n", ""))
add(("graph read: cell name", C, "        m_cells.Insert(name, m_cellArray + i);", "        m_cells.Insert(name, m_cellArray);"))
add(("graph read: cell not read", C, "        m_cellArray[i].Read(this, tokenizer);\n", ""))
add(("graph read: exterior word", C, "    if (token == kExteriorPortals) {", "    if (token == kExteriorPortals + 1) {"))
add(("graph read: exterior bound", C, "        if (m_exteriorPortalCount + first > m_portalCount) {", "        if (m_exteriorPortalCount + first >= m_portalCount) {"))
add(("graph read: exterior first", C, "        m_exteriorPortals = m_portals + first;", "        m_exteriorPortals = m_portals;"))
add(("graph read: exterior next", C, "        m_exteriorPortals = m_portals + first;\n        token = tokenizer->Next();\n", "        m_exteriorPortals = m_portals + first;\n"))
add(("graph read: right curly report", C, "        tokenizer->Error(AtIoTokenizer::kExpectedRightCurly);", "        tokenizer->Error(AtIoTokenizer::kExpectedLeftCurly);"))
add(("graph read: not resolved", C, "    if (s_cellNames != 0) {", "    if (s_cellNames == 0) {"))
add(("graph read: resolve arguments", C, "s_graphNames + s_portalIndex * 8, s_spaceNames + s_portalIndex * 8,", "s_spaceNames + s_portalIndex * 8, s_graphNames + s_portalIndex * 8,"))
add(("graph read: cell names kept", C, "        s_cellNames = 0;\n", ""))
add(("graph read: left locked", C, "    if (s_lock.m_data != 0) {\n        m_mesh->VertexArray(s_lockedArray)->Slot6();\n    }\n}", "}"))
add(("db dtor: nothing", C, "AtCellGraphDatabase::~AtCellGraphDatabase() {\n    AtCellGraphDatabase::Clear();", "AtCellGraphDatabase::~AtCellGraphDatabase() {\n"))
add(("db clear: graphs kept", C, "    if (m_graphs != 0) {\n        AtDeleteArray(m_graphs);\n    }\n", ""))
add(("db clear: map kept", C, "    GmNameMap::Clear();\n", ""))
add(("db read: no clear", C, "    if (m_entries != 0) {\n        Clear();\n    }\n    tokenizer->SetKeywords", "    tokenizer->SetKeywords"))
add(("db read: keywords", C, "    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);", "    tokenizer->SetKeywords(s_keywords, kKeywordCount - 1, 0);"))
add(("db read: name space word", C, "    if (tokenizer->Next() == kNameSpace) {", "    if (tokenizer->Next() == kNameSpace + 1) {"))
add(("db read: name space next", C, "        tokenizer->Next();\n        space = ns;", "        space = ns;"))
add(("db read: name space lost", C, "        space = ns;\n", ""))
add(("db read: count silent", C, "    if (m_count < 1) {\n        tokenizer->Error(kOutOfRange);\n    }\n", ""))
add(("db read: map", C, "    Init(m_count);\n", ""))
add(("db read: not on the owner", C, "    owner->AddCellGraphs(this, space);\n", ""))
add(("db read: no memory code", C, "    if (m_graphs == 0) {\n        AtRaise(-4, 0, 0, 0);", "    if (m_graphs == 0) {\n        AtRaise(-5, 0, 0, 0);"))
add(("db read: word not expected", C, "        tokenizer->ExpectToken(kCellGraphWord);\n", ""))
add(("db read: name", C, "        Insert(name, m_graphs + i);", "        Insert(name, m_graphs);"))
add(("db read: graph not read", C, "        m_graphs[i].Read(owner, tokenizer);\n", ""))
add(("db read: no right curly", C, "    tokenizer->ExpectRightCurly();\n}\n\nvoid AtCellGraphDatabase::Load", "}\n\nvoid AtCellGraphDatabase::Load"))
add(("db load: report", C, "        AtReportFileError(status, name, \"ASC\");", "        AtReportFileError(status, name, \"ASX\");"))
add(("db load: flags", C, "file.Open(name, \"ASC\", 2, 0);", "file.Open(name, \"ASC\", 1, 0);"))
add(("keywords: Portal", C, "{0x3C, \"Portal\"}", "{0x3C, \"Portals\"}"))
add(("keywords: Cell", C, "{0x42, \"Cell\"}", "{0x42, \"Cells\"}"))

# ---- flat images and text ---------------------------------------------------------------------------------------------------------------------
add(("colours: read", D, "    m_colors[2].Read(tokenizer);\n", ""))
add(("colours: set", D, "    m_colors[1] = color;\n", ""))
add(("FindFont: space ignored", D, "        return m_fontSpaces.Find(ns, name);", "        return m_fonts.Find(name);"))
add(("FindFont: empty space", D, "    if (ns != 0 && ns[0] != 0) {\n        return m_fontSpaces", "    if (ns != 0) {\n        return m_fontSpaces"))
add(("FindImage: space ignored", D, "        return m_imageSpaces.Find(ns, name);", "        return m_images.Find(name);"))
add(("FindImage: plain", D, "    return m_images.Find(name);", "    return m_fonts.Find(name);"))
for field in ["m_kind", "m_flags", "m_unknown0C", "m_owner", "m_x", "m_y", "m_drawWidth", "m_drawHeight"]:
    add(("Reset: %s" % field, D, "    %s = 0;\n" % field, ""))
add(("Reset: clip", D, "    m_clip[2] = 0;\n", ""))
add(("Reset: colours", D, "    m_colors.SetAll(white);\n", ""))
add(("Reset: white", D, "    white.SetWhite();\n", ""))
add(("Reset: scale", D, "    m_scaleX = 1.0f;\n", ""))
add(("Reset: device scale", D, "    m_deviceScaleY = 1.0f;\n", ""))
add(("item: report", D, "    tokenizer->Error(kNoSuchWord);", "    tokenizer->Error(AtIoTokenizer::kExpectedKeyword);"))
add(("item read: owner", D, "    m_owner = owner;\n", ""))
add(("item read: device scale", D, "    owner->m_device->DrawScale(&m_deviceScaleX);\n", ""))
add(("item read: scale", D, "    m_scaleX = m_deviceScaleX;\n", ""))
add(("item read: scale y", D, "    m_scaleY = m_deviceScaleY;\n", "    m_scaleY = m_deviceScaleX;\n"))
add(("item read: no left curly", D, "    tokenizer->ExpectLeftCurly();\n", ""))
add(("item read: base words only", D, "        ReadWord(tokenizer, token);", "        AtDrawItem::ReadWord(tokenizer, token);"))
add(("image ctor", D, "AtImageDraw::AtImageDraw() {\n    m_image = 0;", "AtImageDraw::AtImageDraw() {"))
add(("image clear: image kept", D, "void AtImageDraw::Clear() {\n    if (m_owner != 0) {\n        Reset();\n        m_image = 0;", "void AtImageDraw::Clear() {\n    if (m_owner != 0) {\n        Reset();"))
add(("image clear: no reset", D, "void AtImageDraw::Clear() {\n    if (m_owner != 0) {\n        Reset();", "void AtImageDraw::Clear() {\n    if (m_owner != 0) {"))
add(("image read: kind", D, "    m_kind = kImage;\n", ""))
add(("image: name space order", D, "        m_image = m_owner->m_device->FindImage(name, ns);", "        m_image = m_owner->m_device->FindImage(ns, name);"))
add(("image: position", D, "        m_x = tokenizer->ExpectReal();\n        m_y = tokenizer->ExpectReal();", "        m_y = tokenizer->ExpectReal();\n        m_x = tokenizer->ExpectReal();"))
add(("image: draw area", D, "        m_drawWidth = tokenizer->ExpectReal();\n        m_drawHeight = tokenizer->ExpectReal();\n        break;\n    case kImageCenterH:", "        m_drawHeight = tokenizer->ExpectReal();\n        m_drawWidth = tokenizer->ExpectReal();\n        break;\n    case kImageCenterH:"))
add(("image: centre h", D, "    case kImageCenterH:\n        m_flags |= kCenterHorizontally;", "    case kImageCenterH:\n        m_flags |= kCenterVertically;"))
add(("image: centre v", D, "    case kImageCenterV:\n        m_flags |= kCenterVertically;", "    case kImageCenterV:\n        m_flags |= kCenterHorizontally;"))
add(("image: scale", D, "        m_scaleX = tokenizer->ExpectReal() * m_scaleX;\n        m_scaleY = tokenizer->ExpectReal() * m_scaleY;\n        break;\n    case kImageClip:", "        m_scaleX = tokenizer->ExpectReal() * m_deviceScaleX;\n        m_scaleY = tokenizer->ExpectReal() * m_scaleY;\n        break;\n    case kImageClip:"))
add(("image: clip", D, "    case kImageClip:\n        m_clip[0] = tokenizer->ExpectReal() * m_deviceScaleX;", "    case kImageClip:\n        m_clip[0] = tokenizer->ExpectReal() * m_deviceScaleY;"))
add(("image: colours flag", D, "    case kImageColors:\n        m_flags |= kHasColors;\n", "    case kImageColors:\n"))
add(("image: normalized", D, "    case kImageNormalized:\n        m_flags |= kNormalizedCoordinates;", "    case kImageNormalized:\n        m_flags |= kHasColors;"))
add(("image: unknown silent", D, "        AtDrawItem::ReadWord(tokenizer, token);\n        break;\n    }\n}\n\nvoid AtImageDraw::Draw", "        break;\n    }\n}\n\nvoid AtImageDraw::Draw"))
add(("image: word ids", D, "kImagePosition = 0x3B, kImageCenterH = 0x3C", "kImagePosition = 0x3C, kImageCenterH = 0x3B"))
add(("image: Image id", D, "kImageImage = 0x3F", "kImageImage = 0x44"))
add(("text ctor: kerning", D, "    m_kerning = kLargestReal;\n}\n\nAtTextDraw::~AtTextDraw", "    m_kerning = 0;\n}\n\nAtTextDraw::~AtTextDraw"))
add(("text ctor: string", D, "AtTextDraw::AtTextDraw() {\n    m_font = 0;\n    m_string = 0;", "AtTextDraw::AtTextDraw() {\n    m_font = 0;"))
add(("text clear: kerning", D, "        m_kerning = kLargestReal;\n    }\n}\n\nvoid AtTextDraw::Read", "    }\n}\n\nvoid AtTextDraw::Read"))
add(("text clear: font", D, "void AtTextDraw::Clear() {\n    if (m_owner != 0) {\n        Reset();\n        m_font = 0;", "void AtTextDraw::Clear() {\n    if (m_owner != 0) {\n        Reset();"))
add(("text read: kind", D, "    m_kind = kText;\n", ""))
add(("text: font", D, "        m_font = m_owner->m_device->FindFont(name, ns);", "        m_font = m_owner->m_device->FindImage(name, ns);"))
add(("text: string", D, "        m_string = ((AtIoContext*)m_owner->m_loader)->m_defaultGroup.Find(name);", "        m_string = ((AtIoContext*)m_owner->m_loader)->m_defaultGroup.Find(\"Hello\");"))
add(("text: position unscaled", D, "        m_x = tokenizer->ExpectReal() * m_deviceScaleX;\n        m_y = tokenizer->ExpectReal() * m_deviceScaleY;", "        m_x = tokenizer->ExpectReal();\n        m_y = tokenizer->ExpectReal() * m_deviceScaleY;"))
add(("text: wrap", D, "        m_wrapWidth = tokenizer->ExpectReal();", "        m_kerning = tokenizer->ExpectReal();"))
add(("text: kerning", D, "        m_kerning = tokenizer->ExpectReal();", "        m_wrapWidth = tokenizer->ExpectReal();"))
add(("text: centre h", D, "    case kTextCenterH:\n        m_flags |= kCenterHorizontally;", "    case kTextCenterH:\n        m_flags |= kCenterVertically;"))
add(("text: colours flag", D, "    case kTextColors:\n        m_flags |= kHasColors;\n", "    case kTextColors:\n"))
add(("text: clip", D, "    case kTextClip:\n        m_clip[0] = tokenizer->ExpectReal() * m_deviceScaleX;\n        m_clip[1]", "    case kTextClip:\n        m_clip[0] = tokenizer->ExpectReal();\n        m_clip[1]"))
add(("text: unknown silent", D, "        AtDrawItem::ReadWord(tokenizer, token);\n        break;\n    }\n}\n\nvoid AtTextDraw::Draw", "        break;\n    }\n}\n\nvoid AtTextDraw::Draw"))
add(("text: word ids", D, "kTextString = 0x3B, kTextPosition = 0x3C", "kTextString = 0x3C, kTextPosition = 0x3B"))
add(("text: Kerning id", D, "kTextKerning = 0x42", "kTextKerning = 0x47"))
for cls, other, ext, add_to in [("AtImageDrawDatabase", "AtImageDraw", "ASI", "AddToFirst"), ("AtTextDrawDatabase", "AtTextDraw", "AST", "AddToSecond")]:
    add(("%s dtor: nothing" % cls, D, "%s::~%s() {\n    %s::Clear();" % (cls, cls, cls), "%s::~%s() {\n" % (cls, cls)))
    add(("%s clear: draws kept" % cls, D, "void %s::Clear() {\n    if (m_draws != 0) {\n        AtDeleteArray(m_draws);\n    }\n" % cls, "void %s::Clear() {\n" % cls))
    add(("%s read: no clear" % cls, D, "void %s::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {\n    if (m_entries != 0) {\n        Clear();\n    }\n" % cls, "void %s::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {\n" % cls))
    add(("%s read: count" % cls, D, "            m_draws = AtNewArray<%s>(m_count);" % other, "            m_draws = AtNewArray<%s>(m_count + 1);" % other))
    add(("%s read: owner" % cls, D, "            owner->%s(this, space);\n" % add_to, ""))
    add(("%s load: report" % cls, D, "        AtReportFileError(status, name, \"%s\");" % ext, "        AtReportFileError(status, name, \"X%s\");" % ext[1:]))
add(("image list: name space next", D, "    int token = tokenizer->Next();\n    if (token == kNameSpace) {\n        tokenizer->ExpectName(ns);\n        token = tokenizer->Next();\n        space = ns;\n    }\n    while (token != AtIoTokenizer::kEnd) {\n        if (token != kItemWord) {\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n        } else {\n            m_count = (unsigned int)tokenizer->ExpectCount();\n            if (s_heap == 0) {\n                AtRaise(-2, 0, 0, 0);\n            }\n            AtPushHeap(s_heap);\n            m_draws = AtNewArray<AtImageDraw>",
    "    int token = tokenizer->Next();\n    if (token == kNameSpace) {\n        tokenizer->ExpectName(ns);\n        space = ns;\n    }\n    while (token != AtIoTokenizer::kEnd) {\n        if (token != kItemWord) {\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n        } else {\n            m_count = (unsigned int)tokenizer->ExpectCount();\n            if (s_heap == 0) {\n                AtRaise(-2, 0, 0, 0);\n            }\n            AtPushHeap(s_heap);\n            m_draws = AtNewArray<AtImageDraw>"))
add(("image list: unknown silent", D, "            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n        } else {\n            m_count = (unsigned int)tokenizer->ExpectCount();\n            if (s_heap == 0) {\n                AtRaise(-2, 0, 0, 0);\n            }\n            AtPushHeap(s_heap);\n            m_draws = AtNewArray<AtImageDraw>",
    "        } else {\n            m_count = (unsigned int)tokenizer->ExpectCount();\n            if (s_heap == 0) {\n                AtRaise(-2, 0, 0, 0);\n            }\n            AtPushHeap(s_heap);\n            m_draws = AtNewArray<AtImageDraw>"))
add(("image list: heap code", D, "                AtRaise(-2, 0, 0, 0);\n            }\n            AtPushHeap(s_heap);\n            m_draws = AtNewArray<AtImageDraw>", "                AtRaise(-3, 0, 0, 0);\n            }\n            AtPushHeap(s_heap);\n            m_draws = AtNewArray<AtImageDraw>"))
add(("image list: map after", D, "            Init(m_count);\n            owner->AddToFirst", "            owner->AddToFirst"))
add(("image list: item word", D, "                tokenizer->ExpectToken(kItemWord);\n                tokenizer->ExpectName(name);\n                Insert(name, m_draws + i);\n                m_draws[i].Read(owner, tokenizer);\n            }\n            tokenizer->ExpectRightCurly();\n        }\n        token = tokenizer->Next();\n    }\n}\n\nvoid AtImageDrawDatabase::Load",
    "                tokenizer->ExpectName(name);\n                Insert(name, m_draws + i);\n                m_draws[i].Read(owner, tokenizer);\n            }\n            tokenizer->ExpectRightCurly();\n        }\n        token = tokenizer->Next();\n    }\n}\n\nvoid AtImageDrawDatabase::Load"))
add(("image list: name", D, "                Insert(name, m_draws + i);\n                m_draws[i].Read(owner, tokenizer);\n            }\n            tokenizer->ExpectRightCurly();\n        }\n        token = tokenizer->Next();\n    }\n}\n\nvoid AtImageDrawDatabase::Load",
    "                Insert(name, m_draws);\n                m_draws[i].Read(owner, tokenizer);\n            }\n            tokenizer->ExpectRightCurly();\n        }\n        token = tokenizer->Next();\n    }\n}\n\nvoid AtImageDrawDatabase::Load"))
add(("image list: not read", D, "                m_draws[i].Read(owner, tokenizer);\n            }\n            tokenizer->ExpectRightCurly();\n        }\n        token = tokenizer->Next();\n    }\n}\n\nvoid AtImageDrawDatabase::Load",
    "            }\n            tokenizer->ExpectRightCurly();\n        }\n        token = tokenizer->Next();\n    }\n}\n\nvoid AtImageDrawDatabase::Load"))
add(("text list: no right curly", D, "            tokenizer->ExpectRightCurly();\n        }\n        token = tokenizer->Next();\n    }\n}\n\nvoid AtTextDrawDatabase::Load", "        }\n        token = tokenizer->Next();\n    }\n}\n\nvoid AtTextDrawDatabase::Load"))
add(("text list: no memory code", D, "                AtRaise(-4, 0, 0, 0);\n            }\n            Init(m_count);\n            owner->AddToSecond", "                AtRaise(-5, 0, 0, 0);\n            }\n            Init(m_count);\n            owner->AddToSecond"))
add(("text list: keywords", D, "    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);\n    char ns[8];\n    const char* space = 0;\n    int token = tokenizer->Next();\n    if (token == kNameSpace) {\n        tokenizer->ExpectName(ns);\n        token = tokenizer->Next();\n        space = ns;\n    }\n    while (token != AtIoTokenizer::kEnd) {\n        if (token != kItemWord) {\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n        } else {\n            m_count = (unsigned int)tokenizer->ExpectCount();\n            if (s_heap == 0) {\n                AtRaise(-2, 0, 0, 0);\n            }\n            AtPushHeap(s_heap);\n            m_draws = AtNewArray<AtTextDraw>",
    "    tokenizer->SetKeywords(s_keywords, kKeywordCount - 2, 0);\n    char ns[8];\n    const char* space = 0;\n    int token = tokenizer->Next();\n    if (token == kNameSpace) {\n        tokenizer->ExpectName(ns);\n        token = tokenizer->Next();\n        space = ns;\n    }\n    while (token != AtIoTokenizer::kEnd) {\n        if (token != kItemWord) {\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n        } else {\n            m_count = (unsigned int)tokenizer->ExpectCount();\n            if (s_heap == 0) {\n                AtRaise(-2, 0, 0, 0);\n            }\n            AtPushHeap(s_heap);\n            m_draws = AtNewArray<AtTextDraw>"))
add(("keywords: Image", D, "{0x3F, \"Image\"}", "{0x3F, \"Images\"}"))
add(("keywords: WrapWidth", D, "{0x41, \"WrapWidth\"}", "{0x42, \"WrapWidth\"}"))

SUITE = {"target": "oracle_asd", "subsystem": "at/asd", "src": "at", "var": "KND_AT_SRC", "cases": "tok", "mutants": M}
