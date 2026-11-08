# Mutants of the .AGD lists and the .AGG, .AKA, .AGV and .AGP readers. Each text occurs exactly once in the source.
# Dropped as equivalent, each with its reason:
#   * Not calling file.Close() in a Load after which nothing happens: the file's destructor, which runs next, closes it. In AtDatabaseLoader::Load the Close does matter (children load after it) and is a mutant.
#   * The bodies of the four seams: the original's arrays are not compared there, only that the recovered side stops.
#   * AddMeshes' and the others' plain paths, kept: the group and space mutants below cover them.
#   * AtFreeNameList without its test for null: deleting null does nothing, in the original's delete and in this one.
#   * An index cut to a byte by a cast to a wider type first: the store to a byte cuts it anyway.
#   * The mode passed to the loads of vertex and pixel processes, lights, fonts and coordinate systems: each takes that argument and does not use it, on either side.
L = "AtDatabaseLoader.cpp"
M = "AtMeshDatabase.cpp"
A = "AtAnimationDatabase.cpp"


def _kind(word, count, make, names):
    block = "        case k%s:\n            ReadCount(tokenizer, %s);\n            %s();\n            ReadNames(tokenizer, %s, %s);\n            break;\n" % (word, count, make, count, names)
    return [
        ("kind %s: not made" % word, L, block, block.replace("            %s();\n" % make, "")),
        ("kind %s: no names" % word, L, block, block.replace("            ReadNames(tokenizer, %s, %s);\n" % (count, names), "")),
    ]


def _load(word, count, names, at):
    text = "    for (unsigned int i = 0; i < %s; i++) {\n        %s(i)->Load(owner, %s[i], mode);\n    }\n" % (count, at, names)
    return [
        ("load %s: dropped" % word, L, text, ""),
        ("load %s: mode" % word, L, text, text.replace(", mode);", ", 0);")),
    ]


def _free(names, count, other):
    text = "    FreeList(%s, %s);\n" % (names, count)
    return [
        ("free %s: dropped" % names, L, text, ""),
        ("free %s: wrong count" % names, L, text, "    FreeList(%s, %s);\n" % (names, other)),
    ]


_MAKE = "    m_coordSystemMap.Init(m_coordSystemCount);\n    m_owner->AddCoordSystems(&m_coordSystemMap);\n"
_ANIM = "    m_animationMap.Init(m_animationCount);\n    m_owner->AddAnimations(&m_animationMap);\n"

MUTANTS = [
    # ---- the table
    ("table: an id", L, '{0x3C, "MeshDatabase"}', '{0x3D, "MeshDatabase"}'),
    ("table: a word", L, '{0x47, "ImageDatabase"}', '{0x47, "ImageDatabas"}'),
    ("mesh table: an id", M, '{0x77, "DemandLoad"}', '{0x78, "DemandLoad"}'),
    ("vertex table: a word", M, '{0x58, "VertexProcess"}', '{0x58, "VertexProces"}'),
    ("pixel table: an id", M, '{0x39, "PixelProcess"}', '{0x3A, "PixelProcess"}'),
    ("animation table: a word", A, '{0x43, "Animation"}', '{0x43, "Animatio"}'),

    # ---- helpers
    ("leaf: no backslash", L, "    const char* leaf = FindLast((char*)path, '\\\\');\n", "    const char* leaf = 0;\n"),
    ("leaf: no slash", L, "        leaf = FindLast((char*)path, '/');\n", ""),
    ("leaf: no colon", L, "        leaf = FindLast((char*)path, ':');\n", ""),
    ("leaf: to the separator", L, "leaf = leaf != 0 ? leaf + 1 : path;", "leaf = leaf != 0 ? leaf : path;"),
    ("leaf: dot kept", L, "while (leaf[length] != 0 && leaf[length] != '.') {", "while (leaf[length] != 0) {"),
    ("leaf: always terminated", L, "            out[i] = leaf[i];\n        }\n        return;\n", "            out[i] = leaf[i];\n        }\n        out[8] = 0;\n        return;\n"),
    ("leaf: eight is short", L, "if (length >= 8) {", "if (length > 8) {"),
    ("leaf: not terminated", L, "    out[length] = 0;\n", ""),
    ("join: no slash, no separator", L, '        Append(out, prefix);\n        Append(out, "\\\\");\n        Append(out, name);\n', "        Append(out, prefix);\n        Append(out, name);\n"),
    ("join: not put back", L, "    *slash = '\\\\';\n", ""),
    ("join: order", L, '    Append(out, "\\\\");\n    Append(out, prefix);\n    Append(out, slash);\n', '    Append(out, slash);\n    Append(out, "\\\\");\n    Append(out, prefix);\n'),
    ("free list: list kept", L, "    operator delete(list);\n", ""),

    # ---- reset
    ("reset: unknown 28", L, "    m_unknown28 = 16;\n", "    m_unknown28 = 0;\n"),
    ("reset: unknown 2C", L, "    m_unknown2C = 16;\n", "    m_unknown2C = 0;\n"),
    ("reset: child", L, "    m_child = 0;\n    m_unknown2C", "    m_unknown2C"),
    ("reset: a count", L, "    m_textureCount = 0;\n", ""),
    ("reset: another count", L, "    m_loaderCount = 0;\n", ""),
    ("reset: a list", L, "    s_textureNames = 0;\n", ""),
    ("reset: last list", L, "    s_pixelProcessNames = 0;\n", ""),
    ("reset: loader names", L, "    m_loaderNames = 0;\n    m_child", "    m_child"),
    ("reset: animations", L, "    m_animations = 0;\n    m_lightAnimations", "    m_lightAnimations"),
    ("reset: owner", L, "void AtDatabaseLoader::Reset() {\n    m_owner = 0;\n", "void AtDatabaseLoader::Reset() {\n"),
    ("constructor: no reset", L, "AtDatabaseLoader::AtDatabaseLoader() {\n    Reset();\n", "AtDatabaseLoader::AtDatabaseLoader() {\n"),
    ("destructor: always unloads", L, "    if (m_owner != 0) {\n        AtDatabaseLoader::Unload();\n    }\n", "    AtDatabaseLoader::Unload();\n"),

    # ---- unload
    ("unload: child not unloaded", L, "        m_child->Unload();\n", ""),
    ("unload: child not deleted", L, "        delete m_child;\n", ""),
    ("unload: mat animations kept", L, "    if (m_matAnimations != 0) {\n        delete[] m_matAnimations;\n    }\n", ""),
    ("unload: mat lists kept", L, "    if (m_matAssignments != 0) {\n        delete[] m_matAssignments;\n    }\n", ""),
    ("unload: animations not freed", L, "            m_owner->m_animationDevice->FreeDatabase(m_animations[i]);\n", ""),
    ("unload: animation list kept", L, "        operator delete(m_animations);\n", ""),
    ("unload: coord systems kept", L, "    if (m_coordSystems != 0) {\n        delete[] m_coordSystems;\n    }\n", ""),
    ("unload: coord map kept", L, "    m_coordSystemMap.Clear();\n", ""),
    ("unload: animation map kept", L, "    m_animationMap.Clear();\n", ""),
    ("unload: no reset", L, "    m_mirrorMap.Clear();\n    Reset();\n", "    m_mirrorMap.Clear();\n"),

    # ---- what the loader makes
    ("coord systems: no heap test", L, "void AtDatabaseLoader::MakeCoordSystems() {\n    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n", "void AtDatabaseLoader::MakeCoordSystems() {\n"),
    ("coord systems: heap not pushed", L, "    AtPushHeap(s_heap);\n    m_coordSystems = new", "    m_coordSystems = new"),
    ("coord systems: map not sized", L, _MAKE, "    m_owner->AddCoordSystems(&m_coordSystemMap);\n"),
    ("coord systems: not in the owner", L, _MAKE, "    m_coordSystemMap.Init(m_coordSystemCount);\n"),
    ("animations: list's size", L, "operator new(m_animationCount * sizeof(AtAnimationDatabase*))", "operator new(m_animationCount)"),
    ("animations: not made", L, "        m_animations[i] = m_owner->m_animationDevice->NewDatabase();\n", "        m_animations[i] = 0;\n"),
    ("animations: map not sized", L, _ANIM, "    m_owner->AddAnimations(&m_animationMap);\n"),
    ("animations: not in the owner", L, _ANIM, "    m_animationMap.Init(m_animationCount);\n"),
    ("mat lists: no heap test", L, "void AtDatabaseLoader::MakeMatAssignments() {\n    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n", "void AtDatabaseLoader::MakeMatAssignments() {\n"),
    ("mat animations: no heap test", L, "void AtDatabaseLoader::MakeMatAnimations() {\n    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n", "void AtDatabaseLoader::MakeMatAnimations() {\n"),
    ("seam: goes on silently", L, '    AtFatal("AtDatabaseLoader: WeightsDatabase is not recovered (0x00019E70, .AKW)", 0, 0, 0);\n', ""),

    # ---- the file's lists
    ("count: second list not reported", L, "    if (count > 0) {\n        tokenizer->Error(kListAlreadyGiven);\n    }\n", ""),
    ("count: wrong problem", L, "enum Problem { kListAlreadyGiven = 0x27 };", "enum Problem { kListAlreadyGiven = 0x28 };"),
    ("names: no heap test", L, "    if (AtIoTokenFile::s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(AtIoTokenFile::s_heap);\n    names =", "    AtPushHeap(AtIoTokenFile::s_heap);\n    names ="),
    ("names: list not on the heap", L, "    AtPushHeap(AtIoTokenFile::s_heap);\n    names = (char**)operator new(count * sizeof(char*));\n    AtPopHeap();\n", "    names = (char**)operator new(count * sizeof(char*));\n"),
    ("names: size", L, "names = (char**)operator new(count * sizeof(char*));", "names = (char**)operator new(count);"),
    ("names: read off the heap", L, "    AtPushHeap(AtIoTokenFile::s_heap);\n    for (unsigned int i = 0; i < count; i++) {\n        tokenizer->ExpectStringCopy(&names[i]);\n    }\n    AtPopHeap();\n", "    for (unsigned int i = 0; i < count; i++) {\n        tokenizer->ExpectStringCopy(&names[i]);\n    }\n"),
    ("load: unload not first", L, "    if (m_owner != 0) {\n        Unload();\n    }\n    const int status", "    const int status"),
    ("load: wrong extension", L, 'file.Open(name, "AGD", 2, 0)', 'file.Open(name, "AGE", 2, 0)'),
    ("load: wrong flags", L, 'file.Open(name, "AGD", 2, 0)', 'file.Open(name, "AGD", 3, 0)'),
    ("load: no report", L, '    if (status != 0) {\n        AtReportFileError(status, name, "AGD");\n    }\n', ""),
    ("load: owner not kept", L, "    m_owner = owner;\n", "    m_owner = 0;\n"),
    ("load: directory always", L, "    if (inDirectory != 0) {\n        path = &file", "    {\n        path = &file"),
    ("load: directory not popped", L, "    if (inDirectory != 0) {\n        path->Pop(0);\n    }\n", ""),
    ("load: pop overwrites", L, "        path->Pop(0);\n", "        path->Pop(\"x\");\n"),
    ("load: keywords", L, "    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);\n", ""),
    ("load: no unexpected word", L, "        default:\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n            break;\n        }\n        tokenizer->ExpectRightCurly();\n", "        default:\n            break;\n        }\n        tokenizer->ExpectRightCurly();\n"),
    ("load: no right curly", L, "            break;\n        }\n        tokenizer->ExpectRightCurly();\n        token = tokenizer->Next();\n", "            break;\n        }\n        token = tokenizer->Next();\n"),
    ("load: children before the close", L, "    file.Close();\n    LoadDatabases(owner, mode, prefix, 1);\n", "    LoadDatabases(owner, mode, prefix, 1);\n    file.Close();\n"),
    ("load: names kept", L, "    LoadDatabases(owner, mode, prefix, 1);\n", "    LoadDatabases(owner, mode, prefix, 0);\n"),
    ("load: mode dropped", L, "    LoadDatabases(owner, mode, prefix, 1);\n", "    LoadDatabases(owner, 0, prefix, 1);\n"),
    ("load: prefix dropped", L, "    LoadDatabases(owner, mode, prefix, 1);\n", "    LoadDatabases(owner, mode, 0, 1);\n"),

    # ---- the order of loading, and each kind's loop
    ("order: textures after shaders", L, "    for (unsigned int i = 0; i < m_textureCount; i++) {\n        TextureAt(i)->Load(owner, s_textureNames[i], mode);\n    }\n    for (unsigned int i = 0; i < m_shaderCount; i++) {\n        ShaderAt(i)->Load(owner, s_shaderNames[i], mode);\n    }\n", "    for (unsigned int i = 0; i < m_shaderCount; i++) {\n        ShaderAt(i)->Load(owner, s_shaderNames[i], mode);\n    }\n    for (unsigned int i = 0; i < m_textureCount; i++) {\n        TextureAt(i)->Load(owner, s_textureNames[i], mode);\n    }\n"),
    ("coord: leaf not inserted", L, "        m_coordSystemMap.Insert(leaf, database);\n", ""),
    ("coord: whole name", L, "        AtLeafName(s_coordSystemNames[i], leaf);\n        m_coordSystemMap.Insert(leaf, database);\n", "        m_coordSystemMap.Insert(s_coordSystemNames[i], database);\n"),
    ("animation: leaf not inserted", L, "        m_animationMap.Insert(leaf, database);\n", ""),
    ("animation: mode", L, "database->Load(owner->m_animationDevice, s_animationNames[i], mode);", "database->Load(owner->m_animationDevice, s_animationNames[i], 0);"),
    ("mat list: dropped", L, "        m_matAssignments[i].Load(owner, s_matAssignmentNames[i], mode);\n", ""),
    ("mat animation: dropped", L, "        m_matAnimations[i].Load(owner, s_matAnimationNames[i], mode);\n", ""),
    ("image: prefix ignored", L, "        if (prefix != 0) {\n            if (AtIoTokenFile", "        if (false) {\n            if (AtIoTokenFile"),
    ("image: joined not freed", L, "            operator delete(joined);\n", ""),
    ("image: joined's size", L, "operator new(LengthOf(s_imageNames[i]) + prefixRoom + 1)", "operator new(LengthOf(s_imageNames[i]) + prefixRoom)"),
    ("image: room for a prefix", L, "prefix != 0 ? LengthOf(prefix) + 2 : 0;", "prefix != 0 ? LengthOf(prefix) + 1 : 0;"),
    ("image: joined off the heap", L, "            AtPushHeap(AtIoTokenFile::s_heap);\n            char* joined", "            char* joined"),
    ("image: joined no heap test", L, "            if (AtIoTokenFile::s_heap == 0) {\n                AtRaise(-2, 0, 0, 0);\n            }\n            AtPushHeap(AtIoTokenFile::s_heap);\n            char* joined", "            AtPushHeap(AtIoTokenFile::s_heap);\n            char* joined"),
    ("free: always", L, "    if (freeNames != 0) {\n        FreeNames();\n    }\n", "    FreeNames();\n"),
    ("free: never", L, "    if (freeNames != 0) {\n        FreeNames();\n    }\n", ""),
    ("free: after the children", L, "    if (freeNames != 0) {\n        FreeNames();\n    }\n    for (unsigned int i = 0; i < m_loaderCount; i++) {\n        LoaderAt(i)->Load(owner, m_loaderNames[i], mode, 0, 1);\n    }\n", "    for (unsigned int i = 0; i < m_loaderCount; i++) {\n        LoaderAt(i)->Load(owner, m_loaderNames[i], mode, 0, 1);\n    }\n    if (freeNames != 0) {\n        FreeNames();\n    }\n"),
    ("child: not in its directory", L, "LoaderAt(i)->Load(owner, m_loaderNames[i], mode, 0, 1);", "LoaderAt(i)->Load(owner, m_loaderNames[i], mode, 0, 0);"),
    ("child: prefix", L, "LoaderAt(i)->Load(owner, m_loaderNames[i], mode, 0, 1);", 'LoaderAt(i)->Load(owner, m_loaderNames[i], mode, "x", 1);'),
    ("child: mode", L, "LoaderAt(i)->Load(owner, m_loaderNames[i], mode, 0, 1);", "LoaderAt(i)->Load(owner, m_loaderNames[i], 0, 0, 1);"),
    ("child names: kept", L, "        AtFreeNameList(m_loaderNames, m_loaderCount);\n        m_loaderNames = 0;\n", ""),

    # ---- the owner
    ("meshes: empty space is a space", L, "void AtMaterialOwner::AddMeshes(GmNameMap* map, const char* ns) {\n    if (ns != 0 && ns[0] != 0) {", "void AtMaterialOwner::AddMeshes(GmNameMap* map, const char* ns) {\n    if (ns != 0) {"),
    ("meshes: wrong group", L, "    m_meshes.Add(map);\n", "    m_fonts.Add(map);\n"),
    ("meshes: wrong spaces", L, "    m_meshSpaces.Add(ns, map);\n", "    m_fontSpaces.Add(ns, map);\n"),
    ("light animations: wrong group", L, "    m_lightAnimations.Add(map);\n", "    m_fonts.Add(map);\n"),
    ("light animations: wrong spaces", L, "    m_lightAnimationSpaces.Add(ns, map);\n", "    m_fontSpaces.Add(ns, map);\n"),
    ("morph animations: wrong group", L, "    m_morphAnimations.Add(map);\n", "    m_fonts.Add(map);\n"),
    ("morph animations: wrong spaces", L, "    m_morphAnimationSpaces.Add(ns, map);\n", "    m_fontSpaces.Add(ns, map);\n"),
    ("coord: wrong group", L, "m_coordSystems.Add(map); }", "m_mirrors.Add(map); }"),
    ("mirrors: wrong group", L, "m_mirrors.Add(map); }", "m_weights.Add(map); }"),
    ("weights: wrong group", L, "m_weights.Add(map); }", "m_mirrors.Add(map); }"),
    ("animations: wrong group", L, "m_animations.Add(map); }", "m_mirrors.Add(map); }"),
    ("vertex: wrong group", L, "m_vertexProcesses.Add(map); }", "m_pixelProcesses.Add(map); }"),
    ("pixel: wrong group", L, "m_pixelProcesses.Add(map); }", "m_vertexProcesses.Add(map); }"),

    # ---- triangles
    ("normal: x", M, "out[0] = (float)(b2 * a1 - b1 * a2);", "out[0] = (float)(b2 * a1 + b1 * a2);"),
    ("normal: y", M, "out[1] = (float)(a2 * b0 - b2 * a0);", "out[1] = (float)(b2 * a0 - a2 * b0);"),
    ("normal: z", M, "out[2] = (float)(b1 * a0 - a1 * b0);", "out[2] = (float)(a1 * b0);"),
    ("normal: base vertex", M, "const double a0 = (double)v0[0] - v1[0];", "const double a0 = (double)v0[0] - v2[0];"),
    ("axis: no normal", M, "    if (length == 0.0) {\n        triangle->m_axis = 3;\n    } else if (fx > fy) {", "    if (fx > fy) {"),
    ("axis: x and y", M, "} else if (fx > fy) {", "} else if (fx >= fy) {"),
    ("axis: z over x", M, "triangle->m_axis = fz > fx ? 0 : 1;", "triangle->m_axis = fz >= fx ? 0 : 1;"),
    ("axis: z over y", M, "triangle->m_axis = fz > fy ? 0 : 2;", "triangle->m_axis = fz >= fy ? 0 : 2;"),
    ("axis: the values", M, "triangle->m_axis = fz > fx ? 0 : 1;", "triangle->m_axis = fz > fx ? 1 : 0;"),
    ("axis: not absolute", M, "volatile float z = normal[2] < 0 ? -normal[2] : normal[2];", "volatile float z = normal[2];"),

    # ---- index arrays
    ("indices: 8 bit", M, "        unsigned char* out = (unsigned char*)memory;\n", "        unsigned short* out = (unsigned short*)memory;\n"),
    ("indices: 32 bit", M, "        int* out = (int*)memory;\n", "        unsigned short* out = (unsigned short*)memory;\n"),
    ("indices: flags not given", M, "    Allocate(m_count, flags);\n", "    Allocate(m_count, 0);\n"),
    ("indices: lock's mode", M, "    Lock(&memory, 8);\n", "    Lock(&memory, 0);\n"),
    ("indices: no unlock", M, "    Unlock();\n", ""),
    ("index array: index", M, "    m_index = index;\n", ""),
    ("index array: no left curly", M, "    m_index = index;\n    tokenizer->ExpectLeftCurly();\n", "    m_index = index;\n"),
    ("index array: flags not cleared", M, "    m_flags = 0;\n    int token = tokenizer->Next();\n    while (token != kIndexWord) {", "    int token = tokenizer->Next();\n    while (token != kIndexWord) {"),
    ("flags: frequent", M, "m_flags = (unsigned short)((m_flags & 0xFFF9) | 1);", "m_flags = (unsigned short)((m_flags & 0xFFFD) | 1);"),
    ("flags: infrequent", M, "m_flags = (unsigned short)((m_flags & 0xFFFE) | 2);", "m_flags = (unsigned short)((m_flags & 0xFFFA) | 2);"),
    ("flags: fixed", M, "m_flags = (unsigned short)((m_flags & 0xFFFE) | 4);", "m_flags = (unsigned short)((m_flags & 0xFFFC) | 4);"),
    ("flags: fixed only", M, "m_flags = (unsigned short)((m_flags & 0xFFF8) | 8);", "m_flags = (unsigned short)((m_flags & 0xFFF9) | 8);"),
    ("flags: 16 bit", M, "m_flags &= 0xFFCF;", "m_flags &= 0xFFEF;"),
    ("flags: 8 bit", M, "m_flags |= kIndex8Bit;", "m_flags |= kIndex32Bit;"),
    ("flags: 32 bit", M, "m_flags |= kIndex32Bit;", "m_flags |= kIndex8Bit;"),
    ("flags: no unexpected word", M, "        default:\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n            break;\n        }\n        token = tokenizer->Next();\n    }\n    m_count =", "        default:\n            break;\n        }\n        token = tokenizer->Next();\n    }\n    m_count ="),
    ("index array: count", M, "    m_count = (unsigned int)tokenizer->ExpectCount();\n    if (tokenizer->Next() == kDemandLoadWord)", "    tokenizer->ExpectCount();\n    if (tokenizer->Next() == kDemandLoadWord)"),
    ("demand: flag", M, "m_loadFlags |= kDemandLoad;", "m_loadFlags |= 1;"),
    ("demand: name", M, "        tokenizer->ExpectName(m_name);\n", "        tokenizer->ExpectString();\n"),
    ("indices: not pushed back", M, "        tokenizer->m_pushedBack = 1;\n", ""),
    ("indices: one right curly", M, "        ReadIndices(tokenizer, m_flags);\n    }\n    tokenizer->ExpectRightCurly();\n    tokenizer->ExpectRightCurly();\n", "        ReadIndices(tokenizer, m_flags);\n    }\n    tokenizer->ExpectRightCurly();\n"),

    # ---- a mesh's finish
    ("mesh finish: own arrays", M, "        if (OwnVertexArrayAt(i) != 0) {\n            OwnVertexArrayAt(i)->Finish();\n        }\n", ""),
    ("mesh finish: null array", M, "        if (OwnVertexArrayAt(i) != 0) {\n            OwnVertexArrayAt(i)->Finish();\n        }\n", "        OwnVertexArrayAt(i);\n        if (OwnVertexArrayAt(i) != 0) {\n            OwnVertexArrayAt(i)->Finish();\n        }\n"),
    ("mesh finish: own lists", M, "        if (OwnIndexArrayAt(i) != 0) {\n            OwnIndexArrayAt(i)->Finish();\n        }\n", ""),
    ("mesh finish: bound", M, "if (which >= m_ownVertexArrays) {", "if (which > m_ownVertexArrays) {"),
    ("mesh finish: no report", M, '                AtFatal("Atlas Graphics: vertex array reference out of bounds", 0, 0, 0);\n', ""),
    ("mesh finish: always own", M, "        if (m_ownVertexArrays != 0) {\n", "        if (true) {\n"),
    ("mesh finish: database's", M, "            array = VertexArrayAt(which);\n", "            array = m_database->VertexArrayAt(which);\n"),
    ("mesh finish: flag", M, "if (array->m_flags & AtVertexArray::kPositionsInMemory) {", "if (array->m_flags != 0) {"),
    ("mesh finish: a position", M, "m_triangles[i].m_vertices[1] = array->PositionOf(m_triangles[i].m_vertices[1]);", "m_triangles[i].m_vertices[1] = array->PositionOf(m_triangles[i].m_vertices[0]);"),
    ("mesh finish: last position", M, "m_triangles[i].m_vertices[2] = array->PositionOf(m_triangles[i].m_vertices[2]);\n", ""),
    ("mesh finish: no axis", M, "            AtMathTriangleSetAxis(&m_triangles[i]);\n", ""),

    # ---- the mesh database
    ("mesh ctor: count", M, "AtMeshDatabase::AtMeshDatabase() {\n    m_meshCount = 0;\n", "AtMeshDatabase::AtMeshDatabase() {\n"),
    ("mesh ctor: 18", M, "AtMeshDatabase::AtMeshDatabase() {\n    m_meshCount = 0;\n    m_more = 0;\n", "AtMeshDatabase::AtMeshDatabase() {\n    m_meshCount = 0;\n"),
    ("mesh clear: base", M, "void AtMeshDatabase::Clear() {\n    GmNameMap::Clear();\n", "void AtMeshDatabase::Clear() {\n"),
    ("mesh clear: lists", M, "    m_vertexArrayCount = 0;\n    m_indexArrayCount = 0;\n}\n\nvoid AtMeshDatabase::Read", "    m_vertexArrayCount = 0;\n}\n\nvoid AtMeshDatabase::Read"),
    ("mesh read: clear", M, "    if (m_meshCount > 0) {\n        Clear();\n    }\n", ""),
    ("mesh read: keywords", M, "    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);\n    int token = tokenizer->Next();\n    const char* ns = 0;\n", "    int token = tokenizer->Next();\n    const char* ns = 0;\n"),
    ("mesh read: space not kept", M, "        ns = space;\n", ""),
    ("mesh read: second arrays", M, "            if (m_vertexArrayCount != 0) {\n                tokenizer->Error(kListAlreadyGiven);\n            }\n", ""),
    ("mesh read: second lists", M, "            if (m_indexArrayCount != 0) {\n                tokenizer->Error(kListAlreadyGiven);\n            }\n", ""),
    ("mesh read: arrays not made", M, "            MakeVertexArrays(owner);\n", ""),
    ("mesh read: lists not made", M, "            MakeIndexArrays(owner);\n", ""),
    ("mesh read: array word", M, "                tokenizer->ExpectToken(kVertexArrayWord);\n", ""),
    ("mesh read: list word", M, "                tokenizer->ExpectToken(kIndexArrayWord);\n", ""),
    ("mesh read: list's index", M, "IndexArrayAt(i)->Read(tokenizer, i);", "IndexArrayAt(i)->Read(tokenizer, 0);"),
    ("mesh read: array's index", M, "VertexArrayAt(i)->Read(owner, tokenizer, i);", "VertexArrayAt(i)->Read(owner, tokenizer, 0);"),
    ("mesh read: list's right curly", M, "        }\n        tokenizer->ExpectRightCurly();\n        token = tokenizer->Next();\n    }\n    if (token != kMeshWord) {", "        }\n        token = tokenizer->Next();\n    }\n    if (token != kMeshWord) {"),
    ("mesh read: unexpected word", M, "    if (token != kMeshWord) {\n        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n    }\n", ""),
    ("mesh read: no meshes", M, "    if (m_meshCount < 1) {\n        tokenizer->Error(kOutOfRange);\n    }\n", ""),
    ("mesh read: map not sized", M, "    Init(m_meshCount);\n", ""),
    ("mesh read: not in the owner", M, "    owner->AddMeshes(this, ns);\n", ""),
    ("mesh read: space not given", M, "    owner->AddMeshes(this, ns);\n", "    owner->AddMeshes(this, 0);\n"),
    ("mesh read: not made", M, "    MakeMeshes(owner);\n", ""),
    ("mesh read: mesh word", M, "        tokenizer->ExpectToken(kMeshWord);\n", ""),
    ("mesh read: not named", M, "        Insert(name, mesh);\n", ""),
    ("mesh read: mesh's space", M, "mesh->Read(owner, this, tokenizer, ns);", "mesh->Read(owner, this, tokenizer, 0);"),
    ("mesh read: right curly", M, "        mesh->Read(owner, this, tokenizer, ns);\n    }\n    tokenizer->ExpectRightCurly();\n", "        mesh->Read(owner, this, tokenizer, ns);\n    }\n"),
    ("mesh finish: arrays", M, "        VertexArrayAt(i)->Finish();\n", ""),
    ("mesh finish: lists", M, "        IndexArrayAt(i)->Finish();\n", ""),
    ("mesh finish: meshes", M, "        MeshAt(i)->Finish();\n", ""),
    ("mesh load: any mode", M, "    if (mode == kModeBinaryAllowed && HasBinaryForm() != 0) {\n        LoadBinaryForm(owner, name);\n", "    if (HasBinaryForm() != 0) {\n        LoadBinaryForm(owner, name);\n"),
    ("mesh load: not asked", M, "    if (mode == kModeBinaryAllowed && HasBinaryForm() != 0) {\n        LoadBinaryForm(owner, name);\n", "    if (mode == kModeBinaryAllowed) {\n        LoadBinaryForm(owner, name);\n"),
    ("mesh load: goes on", M, "        LoadBinaryForm(owner, name);\n        return;\n", "        LoadBinaryForm(owner, name);\n"),
    ("mesh load: extension", M, 'file.Open(name, "AGG", 2, 0)', 'file.Open(name, "AGH", 2, 0)'),
    ("mesh load: no report", M, '    if (status != 0) {\n        AtReportFileError(status, name, "AGG");\n    }\n', ""),
    ("mesh load: no finish", M, "    Read(owner, file.m_tokenizer);\n    Finish();\n", "    Read(owner, file.m_tokenizer);\n"),
    ("mesh load: pop", M, "    Finish();\n    path.Pop(0);\n", "    Finish();\n"),

    # ---- processes
    ("vertex ctor", M, "AtVertexProcessDatabase::AtVertexProcessDatabase() {\n    m_count = 0;\n", "AtVertexProcessDatabase::AtVertexProcessDatabase() {\n"),
    ("vertex clear: count", M, "void AtVertexProcessDatabase::Clear() {\n    m_count = 0;\n", "void AtVertexProcessDatabase::Clear() {\n"),
    ("vertex clear: map too", M, "void AtVertexProcessDatabase::Clear() {\n    m_count = 0;\n", "void AtVertexProcessDatabase::Clear() {\n    m_count = 0;\n    GmNameMap::Clear();\n"),
    ("vertex read: clear", M, "void AtVertexProcessDatabase::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) {\n    if (m_count > 0) {\n        Clear();\n    }\n", "void AtVertexProcessDatabase::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) {\n"),
    ("vertex read: first word", M, "    tokenizer->ExpectToken(kVertexProcessWord);\n    m_count", "    m_count"),
    ("vertex read: map", M, "    Init(m_count);\n    owner->AddVertexProcesses(this);\n", "    owner->AddVertexProcesses(this);\n"),
    ("vertex read: owner", M, "    owner->AddVertexProcesses(this);\n", ""),
    ("vertex read: made", M, "    owner->AddVertexProcesses(this);\n    Make(owner);\n", "    owner->AddVertexProcesses(this);\n"),
    ("vertex read: item word", M, "        tokenizer->ExpectToken(kVertexProcessWord);\n        char name[8];\n", "        char name[8];\n"),
    ("vertex read: not named", M, "        Insert(name, item);\n        item->ReadVertex(owner, tokenizer);\n", "        item->ReadVertex(owner, tokenizer);\n"),
    ("vertex read: not read", M, "        item->ReadVertex(owner, tokenizer);\n", ""),
    ("vertex read: not told", M, "    tokenizer->ExpectRightCurly();\n    for (unsigned int i = 0; i < m_count; i++) {\n        ItemAt(i)->OnLoaded(owner);\n    }\n}\n\nvoid AtVertexProcessDatabase::Load", "    tokenizer->ExpectRightCurly();\n}\n\nvoid AtVertexProcessDatabase::Load"),
    ("vertex load: extension", M, 'file.Open(name, "AGV", 2, 0)', 'file.Open(name, "AGW", 2, 0)'),
    ("vertex load: no report", M, '    if (status != 0) {\n        AtReportFileError(status, name, "AGV");\n    }\n', ""),
    ("pixel ctor", M, "AtPixelProcessDatabase::AtPixelProcessDatabase() {\n    m_count = 0;\n", "AtPixelProcessDatabase::AtPixelProcessDatabase() {\n"),
    ("pixel clear: count", M, "void AtPixelProcessDatabase::Clear() {\n    m_count = 0;\n", "void AtPixelProcessDatabase::Clear() {\n"),
    ("pixel clear: map", M, "void AtPixelProcessDatabase::Clear() {\n    m_count = 0;\n    GmNameMap::Clear();\n", "void AtPixelProcessDatabase::Clear() {\n    m_count = 0;\n"),
    ("pixel read: clear", M, "void AtPixelProcessDatabase::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) {\n    if (m_count > 0) {\n        Clear();\n    }\n", "void AtPixelProcessDatabase::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) {\n"),
    ("pixel read: first word", M, "    tokenizer->ExpectToken(kPixelProcessWord);\n    m_count", "    m_count"),
    ("pixel read: owner", M, "    owner->AddPixelProcesses(this);\n", ""),
    ("pixel read: not read", M, "        item->ReadPixel(owner, tokenizer);\n", ""),
    ("pixel read: not told", M, "    tokenizer->ExpectRightCurly();\n    for (unsigned int i = 0; i < m_count; i++) {\n        ItemAt(i)->OnLoaded(owner);\n    }\n}\n\nvoid AtPixelProcessDatabase::Load", "    tokenizer->ExpectRightCurly();\n}\n\nvoid AtPixelProcessDatabase::Load"),
    ("pixel load: extension", M, 'file.Open(name, "AGP", 2, 0)', 'file.Open(name, "AGQ", 2, 0)'),
    ("pixel load: no report", M, '    if (status != 0) {\n        AtReportFileError(status, name, "AGP");\n    }\n', ""),

    # ---- the keys
    ("keys: not cleared", A, "    if (m_flags & kRead) {\n        Clear();\n    }\n", ""),
    ("keys: no left curly", A, "        Clear();\n    }\n    tokenizer->ExpectLeftCurly();\n    int token", "        Clear();\n    }\n    int token"),
    ("keys: rotation", A, "            ReadRotationKeys(tokenizer);\n", "            ReadScaleKeys(tokenizer);\n"),
    ("keys: translation", A, "            ReadTranslationKeys(tokenizer);\n", "            ReadScaleKeys(tokenizer);\n"),
    ("keys: scale", A, "            ReadScaleKeys(tokenizer);\n", "            ReadNoteKeys(tokenizer);\n"),
    ("keys: note", A, "            ReadNoteKeys(tokenizer);\n", "            ReadCameraKeys(tokenizer);\n"),
    ("keys: camera", A, "            ReadCameraKeys(tokenizer);\n", "            ReadNoteKeys(tokenizer);\n"),
    ("keys: frame numbers", A, "        case kFrameNumber:\n            Read(tokenizer);\n", "        case kFrameNumber:\n"),

    # ---- one animation
    ("reset: rate", A, "    m_rate = 0;\n", ""),
    ("reset: radius", A, "    m_boundingSphere.radius = kLargestFloat;\n", "    m_boundingSphere.radius = 0;\n"),
    ("reset: flags", A, "    m_flags = 0;\n}\n\nvoid AtAnimation::Read", "}\n\nvoid AtAnimation::Read"),
    ("reset: length", A, "    m_length = 0;\n", ""),
    ("reset: loop", A, "    m_loopPeriod = 0;\n", ""),
    ("reset: camera", A, "    m_camera[1] = 0;\n", ""),
    ("reset: velocity", A, "    m_velocity[2] = 0;\n", ""),
    ("reset: angular", A, "    m_angularVelocity[2] = 0;\n", ""),
    ("reset: centre", A, "    m_boundingSphere.center.z = 0;\n", ""),
    ("animation: no reset", A, "    if (m_database != 0) {\n        Reset();\n    }\n", ""),
    ("animation: database", A, "    m_database = database;\n", ""),
    ("animation: rotation", A, "            ReadPair(tokenizer, m_rotation);\n", "            ReadPair(tokenizer, m_scale);\n"),
    ("animation: translation", A, "            ReadPair(tokenizer, m_translation);\n", "            ReadPair(tokenizer, m_scale);\n"),
    ("animation: scale", A, "            ReadPair(tokenizer, m_scale);\n", "            ReadPair(tokenizer, m_note);\n"),
    ("animation: note", A, "            ReadPair(tokenizer, m_note);\n", "            ReadPair(tokenizer, m_scale);\n"),
    ("animation: camera", A, "            ReadPair(tokenizer, m_camera);\n", "            ReadPair(tokenizer, m_scale);\n"),
    ("animation: pair", A, "    pair[1] = (unsigned short)tokenizer->ExpectInt();\n", "    pair[1] = 0;\n"),
    ("animation: rate", A, "            m_rate = tokenizer->ExpectReal();\n", "            tokenizer->ExpectReal();\n"),
    ("animation: linear velocity is its own", A, "        case kVelocityVector:\n        case kLinearVelocity:\n", "        case kLinearVelocity:\n            ReadThree(tokenizer, m_angularVelocity);\n            break;\n        case kVelocityVector:\n"),
    ("animation: third", A, "    three[2] = tokenizer->ExpectReal();\n", ""),
    ("animation: angular", A, "            ReadThree(tokenizer, m_angularVelocity);\n", "            ReadThree(tokenizer, m_velocity);\n"),
    ("animation: length", A, "            m_length = (unsigned short)length;\n", ""),
    ("animation: loop always", A, "            if (m_loopPeriod == 0) {\n                m_loopPeriod = (unsigned short)(length + 1);\n            }\n", "            m_loopPeriod = (unsigned short)(length + 1);\n"),
    ("animation: loop is the length", A, "m_loopPeriod = (unsigned short)(length + 1);", "m_loopPeriod = (unsigned short)length;"),
    ("animation: loop period", A, "            m_loopPeriod = (unsigned short)tokenizer->ExpectInt();\n", "            tokenizer->ExpectInt();\n"),
    ("animation: sphere", A, "            m_boundingSphere.center.Read(tokenizer);\n", ""),
    ("animation: radius", A, "            m_boundingSphere.radius = tokenizer->ExpectReal();\n", "            tokenizer->ExpectReal();\n"),
    ("animation: flag", A, "            m_flags |= kHasBoundingSphere;\n", ""),
    ("animation: no unexpected word", A, "            m_flags |= kHasBoundingSphere;\n            break;\n        default:\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n", "            m_flags |= kHasBoundingSphere;\n            break;\n        default:\n"),

    # ---- the animation database
    ("anim ctor: a field", A, "AtAnimationDatabase::AtAnimationDatabase() {\n    m_device = 0;\n", "AtAnimationDatabase::AtAnimationDatabase() {\n"),
    ("anim clear: base", A, "void AtAnimationDatabase::Clear() {\n    GmNameMap::Clear();\n", "void AtAnimationDatabase::Clear() {\n"),
    ("anim clear: keys not cleared", A, "        m_keys->Clear();\n", ""),
    ("anim clear: keys not freed", A, "        m_device->FreeKeys(m_keys);\n", ""),
    ("anim clear: rotation", A, "        operator delete(m_rotation);\n", ""),
    ("anim clear: camera", A, "        operator delete(m_camera);\n", ""),
    ("anim clear: animations", A, "        delete[] m_animations;\n", ""),
    ("anim dtor: no clear", A, "AtAnimationDatabase::~AtAnimationDatabase() {\n    AtAnimationDatabase::Clear();\n", "AtAnimationDatabase::~AtAnimationDatabase() {\n"),
    ("components: second list", A, "    if (*list != 0) {\n        tokenizer->Error(kListAlreadyGiven);\n    }\n", ""),
    ("components: count", A, "    *count = (unsigned int)tokenizer->ExpectCount();\n", "    tokenizer->ExpectCount();\n"),
    ("components: no heap", A, "    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    *list", "    AtPushHeap(s_heap);\n    *list"),
    ("components: size", A, "operator new(*count * sizeof(AtAnimationComponent))", "operator new(*count)"),
    ("components: field", A, "        entry.m_unknown04 = tokenizer->ExpectInt();\n", "        tokenizer->ExpectInt();\n"),
    ("components: zero", A, "        if (entry.m_unknown08 == 0) {\n            tokenizer->Error(kOutOfRange);\n        }\n", ""),
    ("components: last", A, "        entry.m_unknown0A = (unsigned short)tokenizer->ExpectInt();\n", "        tokenizer->ExpectInt();\n"),
    ("components: right curly", A, "        entry.m_unknown0A = (unsigned short)tokenizer->ExpectInt();\n    }\n    tokenizer->ExpectRightCurly();\n", "        entry.m_unknown0A = (unsigned short)tokenizer->ExpectInt();\n    }\n"),
    ("list: second", A, "    if (m_animations != 0) {\n        tokenizer->Error(kListAlreadyGiven);\n    }\n", ""),
    ("list: no heap", A, "    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    m_animations", "    AtPushHeap(s_heap);\n    m_animations"),
    ("list: map", A, "    Init(m_animationCount);\n", ""),
    ("list: word", A, "        tokenizer->ExpectToken(kAnimationWord);\n", ""),
    ("list: not named", A, "        Insert(name, &m_animations[i]);\n", ""),
    ("list: not read", A, "        m_animations[i].Read(this, tokenizer);\n", ""),
    ("list: right curly", A, "        m_animations[i].Read(this, tokenizer);\n    }\n    tokenizer->ExpectRightCurly();\n", "        m_animations[i].Read(this, tokenizer);\n    }\n"),
    ("read: clear", A, "    if (m_keys != 0) {\n        Clear();\n    }\n    tokenizer->SetKeywords", "    tokenizer->SetKeywords"),
    ("read: keywords", A, "    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);\n", ""),
    ("read: device", A, "    m_device = device;\n", ""),
    ("read: keyset", A, "            m_keys->ReadKeys(tokenizer);\n", ""),
    ("read: rotation list", A, "ReadComponents(tokenizer, &m_rotation, &m_rotationCount);", "ReadComponents(tokenizer, &m_scale, &m_scaleCount);"),
    ("read: translation list", A, "ReadComponents(tokenizer, &m_translation, &m_translationCount);", "ReadComponents(tokenizer, &m_scale, &m_scaleCount);"),
    ("read: scale list", A, "ReadComponents(tokenizer, &m_scale, &m_scaleCount);", "ReadComponents(tokenizer, &m_note, &m_noteCount);"),
    ("read: note list", A, "ReadComponents(tokenizer, &m_note, &m_noteCount);", "ReadComponents(tokenizer, &m_scale, &m_scaleCount);"),
    ("read: camera list", A, "ReadComponents(tokenizer, &m_camera, &m_cameraCount);", "ReadComponents(tokenizer, &m_scale, &m_scaleCount);"),
    ("read: animations", A, "            ReadAnimations(tokenizer);\n", ""),
    ("read: unexpected", A, "            ReadAnimations(tokenizer);\n            break;\n        default:\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n", "            ReadAnimations(tokenizer);\n            break;\n        default:\n"),
    ("read: keys not told", A, "    m_keys->Load();\n", ""),
    ("anim load: any mode", A, "    if (mode == kModeBinaryAllowed && HasBinaryForm() != 0) {\n", "    if (HasBinaryForm() != 0) {\n"),
    ("anim load: not asked", A, "    if (mode == kModeBinaryAllowed && HasBinaryForm() != 0) {\n", "    if (mode == kModeBinaryAllowed) {\n"),
    ("anim load: goes on", A, "        LoadBinaryForm(device, name);\n        return;\n", "        LoadBinaryForm(device, name);\n"),
    ("anim load: extension", A, 'file.Open(name, "AKA", 2, 0)', 'file.Open(name, "AKB", 2, 0)'),
    ("anim load: no report", A, '    if (status != 0) {\n        AtReportFileError(status, name, "AKA");\n    }\n', ""),
]

for _w, _c, _m, _n in [
    ("TextureDatabase", "m_textureCount", "MakeTextures", "s_textureNames"), ("ShaderDatabase", "m_shaderCount", "MakeShaders", "s_shaderNames"), ("MaterialDatabase", "m_materialCount", "MakeMaterials", "s_materialNames"),
    ("MeshDatabase", "m_meshCount", "MakeMeshes", "s_meshNames"), ("LightDatabase", "m_lightCount", "MakeLights", "s_lightNames"), ("CoordSysDatabase", "m_coordSystemCount", "MakeCoordSystems", "s_coordSystemNames"),
    ("AnimationDatabase", "m_animationCount", "MakeAnimations", "s_animationNames"), ("MatAssignmentDatabase", "m_matAssignmentCount", "MakeMatAssignments", "s_matAssignmentNames"), ("MatAnimationDatabase", "m_matAnimationCount", "MakeMatAnimations", "s_matAnimationNames"),
    ("FontDatabase", "m_fontCount", "MakeFonts", "s_fontNames"), ("ImageDatabase", "m_imageCount", "MakeImages", "s_imageNames"), ("VertexProcessDatabase", "m_vertexProcessCount", "MakeVertexProcesses", "s_vertexProcessNames"),
    ("PixelProcessDatabase", "m_pixelProcessCount", "MakePixelProcesses", "s_pixelProcessNames"), ("DatabaseLoader", "m_loaderCount", "MakeLoaders", "m_loaderNames"),
]:
    MUTANTS += _kind(_w, _c, _m, _n)

for _w, _c, _n, _a in [("vertex", "m_vertexProcessCount", "s_vertexProcessNames", "VertexProcessAt"), ("pixel", "m_pixelProcessCount", "s_pixelProcessNames", "PixelProcessAt"), ("mesh", "m_meshCount", "s_meshNames", "MeshAt"), ("light", "m_lightCount", "s_lightNames", "LightAt"), ("font", "m_fontCount", "s_fontNames", "FontAt"), ("material", "m_materialCount", "s_materialNames", "MaterialAt")]:
    MUTANTS += [_m for _m in _load(_w, _c, _n, _a) if _w in ("mesh", "material") or not _m[0].endswith(": mode")]

for _n, _c, _o in [("s_pixelProcessNames", "m_pixelProcessCount", "m_vertexProcessCount"), ("s_vertexProcessNames", "m_vertexProcessCount", "m_pixelProcessCount"), ("s_imageNames", "m_imageCount", "m_fontCount"), ("s_fontNames", "m_fontCount", "m_imageCount"),
                   ("s_matAnimationNames", "m_matAnimationCount", "m_fontCount"), ("s_morphAnimationNames", "m_morphAnimationCount", "m_fontCount"), ("s_lightAnimationNames", "m_lightAnimationCount", "m_fontCount"), ("s_matAssignmentNames", "m_matAssignmentCount", "m_fontCount"),
                   ("s_animationNames", "m_animationCount", "m_fontCount"), ("s_weightsNames", "m_weightsCount", "m_fontCount"), ("s_coordSystemNames", "m_coordSystemCount", "m_fontCount"), ("s_mirrorNames", "m_mirrorCount", "m_fontCount"),
                   ("s_lightNames", "m_lightCount", "m_fontCount"), ("s_meshNames", "m_meshCount", "m_fontCount"), ("s_materialNames", "m_materialCount", "m_fontCount"), ("s_shaderNames", "m_shaderCount", "m_fontCount"), ("s_textureNames", "m_textureCount", "m_fontCount")]:
    MUTANTS += _free(_n, _c, _o)

SUITE = {"target": "oracle_agd", "subsystem": "at/agd", "src": "at", "var": "KND_AT_SRC", "cases": "tok", "mutants": MUTANTS}
