# Mutants of the Xbox database loader, the .XGG and .XKA readers and what they need. Each text occurs exactly once in its file. See docs/worklog/2026-11-07-loader.md.
# Dropped as equivalent, each with its reason:
#   * The constructor's store of m_next in the mesh database: the field is written by AddMeshDatabase before anything reads it, and nothing reads it of a database that is on no list.
#   * The return after unlinking in RemoveMeshDatabase: the loop goes on from the next database, which is not the one removed, to the end; the list is the same.
#   * Close() before the end of either binary reader where nothing comes between it and the file's destructor: the destructor closes it the same way. In the mesh reader it is mutated, items being read after it.
#   * The .XGG name space passed always, empty or not: AtMaterialOwner::AddMeshes treats an empty name space as none.
#   * A count pointer given to the read of a block: the readers never look at it, and the file writes only there.
#   * The .XKA aligned without rounding up: the table always ends at 0x88, already aligned, in every file whose table is whole; a short table is stack on both sides and not compared.
#   * Init's own clear of what was read: Reserve, next, clears through the same slot when the set is read, and the clear leaves it unread, so the slot runs once either way.
#   * The rate copied as a float rather than as a word: this compiler copies a float member with integer moves, so the code is the same; the mutant would differ only where the floating-point unit is used.
# Dropped as not observable here, a gap rather than an equivalence: the three mutants of the mesh heap's raised alignment during the kept block's allocation (at 5, of 16, and the heap's own kept). The
# harness's allocators ignore the heap, and the alignment is set back before anything else runs, so what it was during the one allocation cannot be seen. The store that sets it back is mutated and detected.
L = "AtGrD3DDatabaseLoader.cpp"
M = "AtGrD3DMeshDatabase.cpp"
A = "AtGrD3DAnimation.cpp"
K = "AtAnimationKeys.cpp"

MUTANTS = []

# ---- the loader ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
MUTANTS += [
    ("loader: no raise with no heap", L, "    if (AtDatabaseLoader::s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n", ""),
    ("loader: heap not pushed", L, "    AtPushHeap(AtDatabaseLoader::s_heap);\n    *array", "    *array"),
    ("loader: heap not popped", L, "    *array = new T[count];\n    AtPopHeap();\n", "    *array = new T[count];\n"),
    ("loader: no raise with no memory", L, "    if (*array == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("loader: dtor without unload", L, "AtGrD3DDatabaseLoader::~AtGrD3DDatabaseLoader() {\n    Unload();\n}", "AtGrD3DDatabaseLoader::~AtGrD3DDatabaseLoader() {\n}"),
    ("loader: base unload dropped", L, "    AtDatabaseLoader::Unload();\n", ""),
]
for field, other in [("textures", "meshes"), ("shaders", "textures"), ("materials", "textures"), ("meshes", "textures"), ("lights", "textures"), ("fonts", "textures"),
                     ("images", "textures"), ("vertexProcesses", "textures"), ("pixelProcesses", "textures"), ("loaders", "textures")]:
    MUTANTS.append(("loader: constructor leaves m_%s" % field, L, "m_%s(0)" % field, "m_%s(m_%s)" % (field, field)))
for word, count, other in [("Textures", "texture", "mesh"), ("Materials", "material", "texture"), ("Shaders", "shader", "material"), ("Meshes", "mesh", "texture"),
                           ("Lights", "light", "texture"), ("Fonts", "font", "image"), ("Images", "image", "font"), ("VertexProcesses", "vertexProcess", "pixelProcess"),
                           ("PixelProcesses", "pixelProcess", "vertexProcess"), ("Loaders", "loader", "mesh")]:
    field = word[0].lower() + word[1:]
    MUTANTS.append(("loader: Make%s by the wrong count" % word, L, "MakeArray(&m_%s, m_%sCount);" % (field, count), "MakeArray(&m_%s, m_%sCount);" % (field, other)))
for field in ["loaders", "pixelProcesses", "vertexProcesses", "images", "fonts", "lights", "meshes", "materials", "shaders", "textures"]:
    MUTANTS.append(("loader: unload keeps m_%s" % field, L, "        delete[] m_%s;\n" % field, ""))
    MUTANTS.append(("loader: at gives the first of m_%s" % field, L, "    return &m_%s[index];" % field, "    return &m_%s[0];" % field))
MUTANTS += [
    ("loader: unload leaves m_textures", L, "    AtDatabaseLoader::Unload();\n    m_textures = 0;\n", "    AtDatabaseLoader::Unload();\n"),
    ("loader: unload leaves m_loaders", L, "    m_pixelProcesses = 0;\n    m_loaders = 0;\n", "    m_pixelProcesses = 0;\n"),
    ("factory: sound, no raise with no heap", L, "    if (s_soundHeap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n", ""),
    ("factory: sound, the loaders' heap", L, "    AtPushHeap(s_soundHeap);", "    AtPushHeap(s_loaderHeap);"),
    ("factory: sound, no raise with no memory", L, "    if (database == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("factory: loader, no raise with no heap", L, "    if (s_loaderHeap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n", ""),
    ("factory: loader, no pop", L, "    AtGrD3DDatabaseLoader* loader = new AtGrD3DDatabaseLoader;\n    AtPopHeap();\n", "    AtGrD3DDatabaseLoader* loader = new AtGrD3DDatabaseLoader;\n"),
    ("factory: loader, no raise with no memory", L, "    if (loader == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
]

# ---- the mesh database --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
MUTANTS += [
    ("owner: add does not link", M, "    database->m_next = m_meshDatabases;\n", ""),
    ("owner: add does not set the head", M, "    m_meshDatabases = database;\n}", "}"),
    ("owner: remove cuts the rest", M, "            *link = database->m_next;", "            *link = 0;"),
    ("owner: remove does not advance", M, "        link = &at->m_next;\n", ""),
    ("allocate: no clear", M, "    if (m_meshCount > 0) {\n        Clear();\n    }\n    m_vertexArrayCount = vertexArrays;", "    m_vertexArrayCount = vertexArrays;"),
    ("allocate: vertex arrays", M, "    m_vertexArrayCount = vertexArrays;", "    m_vertexArrayCount = indexArrays;"),
    ("allocate: meshes", M, "    m_meshCount = meshes;", "    m_meshCount = vertexArrays;"),
    ("allocate: index arrays", M, "    m_indexArrayCount = indexArrays;", "    m_indexArrayCount = meshes;"),
    ("allocate: no map", M, "    Init(meshes);\n", ""),
    ("allocate: vertex arrays made always", M, "    if (m_vertexArrayCount > 0) {\n        MakeVertexArrays(owner);\n    }\n", "    MakeVertexArrays(owner);\n"),
    ("allocate: index arrays made always", M, "    if (m_indexArrayCount > 0) {\n        MakeIndexArrays(owner);\n    }\n", "    MakeIndexArrays(owner);\n"),
    ("allocate: no meshes made", M, "    MakeMeshes(owner);\n    owner->AddMeshes", "    owner->AddMeshes"),
    ("allocate: no name space", M, "    owner->AddMeshes(this, ns);", "    owner->AddMeshes(this, 0);"),
    ("allocate: not given to the owner", M, "    owner->AddMeshes(this, ns);\n", ""),
]
for field in ["owner", "meshes", "vertexArrays", "indexArrays", "streams", "kept", "keptSize", "contiguous", "contiguousSize"]:
    MUTANTS.append(("mesh: constructor leaves m_%s" % field, M, "m_%s(0)" % field, "m_%s(m_%s)" % (field, field)))
MUTANTS += [
    ("mesh: clear stays on the list", M, "        m_owner->RemoveMeshDatabase(this);\n", ""),
    ("mesh: clear keeps meshes", M, "        delete[] m_meshes;\n", ""),
    ("mesh: clear keeps vertex arrays", M, "        delete[] m_vertexArrays;\n", ""),
    ("mesh: clear keeps index arrays", M, "        delete[] m_indexArrays;\n", ""),
    ("mesh: clear keeps streams", M, "        delete[] m_streams;\n", ""),
    ("mesh: clear keeps the second block", M, "        operator delete(m_kept);\n", ""),
    ("mesh: clear keeps the third block", M, "        AtGrD3DFreeContiguous(m_contiguous);\n", ""),
    ("mesh: clear without the base's", M, "    AtMeshDatabase::Clear();\n", ""),
    ("mesh: clear leaves the contiguous size", M, "    m_contiguousSize = 0;\n}\n\nvoid AtGrD3DMeshDatabase::MakeMeshes", "}\n\nvoid AtGrD3DMeshDatabase::MakeMeshes"),
    ("mesh: clear leaves the kept size", M, "    m_kept = 0;\n    m_keptSize = 0;\n    m_contiguous = 0;\n    m_contiguousSize = 0;\n}", "    m_kept = 0;\n    m_contiguous = 0;\n    m_contiguousSize = 0;\n}"),
    ("mesh: meshes by the wrong count", M, "    m_meshes = new AtGrD3DMesh[m_meshCount];", "    m_meshes = new AtGrD3DMesh[m_indexArrayCount];"),
    ("mesh: meshes, no raise with no heap", M, "    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    m_meshes", "    AtPushHeap(s_heap);\n    m_meshes"),
    ("mesh: meshes, no raise with no memory", M, "    if (m_meshes == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("mesh: meshes, no owner kept", M, "    m_owner = static_cast<AtGrD3DOwner*>(owner);\n    m_owner->AddMeshDatabase(this);", "    static_cast<AtGrD3DOwner*>(owner)->AddMeshDatabase(this);"),
    ("mesh: meshes, not on the list", M, "    m_owner->AddMeshDatabase(this);\n", ""),
    ("mesh: vertex arrays by the wrong count", M, "    m_vertexArrays = new AtGrD3DVertexArray[m_vertexArrayCount];", "    m_vertexArrays = new AtGrD3DVertexArray[m_indexArrayCount];"),
    ("mesh: vertex arrays without their owner", M, "    for (AtGrD3DVertexArray* array = m_vertexArrays; array < m_vertexArrays + m_vertexArrayCount; array++) {\n        array->m_owner = static_cast<AtGrD3DOwner*>(owner);\n    }\n", ""),
    ("mesh: vertex arrays' owner one short", M, "array < m_vertexArrays + m_vertexArrayCount;", "array + 1 < m_vertexArrays + m_vertexArrayCount;"),
    ("mesh: index arrays by the wrong count", M, "    m_indexArrays = new AtGrD3DIndexArray[m_indexArrayCount];", "    m_indexArrays = new AtGrD3DIndexArray[m_vertexArrayCount];"),
    ("mesh: index arrays without their owner", M, "    for (AtGrD3DIndexArray* array = m_indexArrays; array < m_indexArrays + m_indexArrayCount; array++) {\n        array->m_owner = static_cast<AtGrD3DOwner*>(owner);\n    }\n", ""),
    ("mesh: index arrays, no raise with no heap", M, "    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    m_indexArrays", "    AtPushHeap(s_heap);\n    m_indexArrays"),
    ("mesh: streams by the wrong count", M, "    m_streams = new AtGrD3DVertexStream[m_streamCount];", "    m_streams = new AtGrD3DVertexStream[m_meshCount];"),
    ("mesh: streams, no pop", M, "    m_streams = new AtGrD3DVertexStream[m_streamCount];\n    AtPopHeap();\n", "    m_streams = new AtGrD3DVertexStream[m_streamCount];\n"),
    ("mesh: no binary form", M, "int AtGrD3DMeshDatabase::HasBinaryForm() {\n    return 1;", "int AtGrD3DMeshDatabase::HasBinaryForm() {\n    return 0;"),
    ("xgg: the wrong extension", M, "file.OpenWithExtension(name, \"XGG\", 2);", "file.OpenWithExtension(name, \"XGA\", 2);"),
    ("xgg: the wrong flags", M, "file.OpenWithExtension(name, \"XGG\", 2);", "file.OpenWithExtension(name, \"XGG\", 0);"),
    ("xgg: no report on open", M, "    int status = file.OpenWithExtension(name, \"XGG\", 2);\n    if (status != 0) {\n        AtReportFileError(status, name, \"XGG\");\n    }\n", "    int status = file.OpenWithExtension(name, \"XGG\", 2);\n"),
    ("xgg: the head not read", M, "    status = file.ReadAll(unread, sizeof unread, 0);\n", ""),
    ("xgg: the table read short", M, "    status = file.ReadAll(&header, sizeof header, 0);", "    status = file.ReadAll(&header, sizeof header - 4, 0);"),
    ("xgg: not aligned", M, "    file.m_file->Seek((file.m_file->m_position + 3) & ~3u);\n\n    // The first block", "    // The first block"),
    ("xgg: the first block on the mesh heap", M, "    AtPushHeap(AtIoTokenFile::s_heap);\n    char* scratch", "    AtPushHeap(s_heap);\n    char* scratch"),
    ("xgg: the first block, no raise with no heap", M, "    if (AtIoTokenFile::s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n", ""),
    ("xgg: the first block not read", M, "    status = AtReadBlock(&file, scratch, header.m_scratchSize);\n", ""),
    ("xgg: the second block's size", M, "    m_keptSize = header.m_keptSize;", "    m_keptSize = header.m_scratchSize;"),
    ("xgg: alignment not restored", M, "    heap = (AtHeap*)s_heap;\n    heap->m_allocShift = heap->m_alignShift;\n", ""),
    ("xgg: the second block, no raise with no memory", M, "    if (m_kept == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("xgg: the second block not read", M, "    status = AtReadBlock(&file, m_kept, m_keptSize);\n", ""),
    ("xgg: the third block's size", M, "    m_contiguousSize = header.m_contiguousSize;", "    m_contiguousSize = header.m_keptSize;"),
    ("xgg: a third block always", M, "    if (m_contiguousSize > 0) {\n        m_contiguous", "    {\n        m_contiguous"),
    ("xgg: the third block's alignment", M, "AtGrD3DAllocateContiguous(m_contiguousSize, 4);", "AtGrD3DAllocateContiguous(m_contiguousSize, 8);"),
    ("xgg: the third block not read", M, "        status = AtReadBlock(&file, m_contiguous, m_contiguousSize);\n", ""),
    ("xgg: no close before the items", M, "    file.Close();\n\n    Allocate(", "\n    Allocate("),
    ("xgg: the mesh count", M, "    Allocate(owner, header.m_meshCount, 0, 0,", "    Allocate(owner, header.m_vertexArrayCount, 0, 0,"),
    ("xgg: no name space", M, "header.m_nameSpace[0] != 0 ? header.m_nameSpace : 0", "0"),
    ("xgg: vertex array count", M, "    m_vertexArrayCount = header.m_vertexArrayCount;", "    m_vertexArrayCount = header.m_indexArrayCount;"),
    ("xgg: stream count", M, "    m_streamCount = header.m_streamCount;", "    m_streamCount = header.m_meshCount;"),
    ("xgg: index array count", M, "    m_indexArrayCount = header.m_indexArrayCount;", "    m_indexArrayCount = header.m_streamCount;"),
    ("xgg: vertex arrays not made", M, "    MakeVertexArrays(owner);\n    char* records", "    char* records"),
    ("xgg: vertex array records", M, "    char* records = scratch + header.m_vertexArrayOffset;", "    char* records = scratch + header.m_indexArrayOffset;"),
    ("xgg: vertex array data", M, "    char* kept = m_kept + header.m_vertexArrayDataOffset;", "    char* kept = m_kept + header.m_indexDataOffset;"),
    ("xgg: vertex data", M, "    char* contiguous = (char*)m_contiguous + header.m_vertexDataOffset;", "    char* contiguous = (char*)m_contiguous + header.m_meshContiguousOffset;"),
    ("xgg: vertex array cursors", M, "m_vertexArrays[i].ReadBinary(&records, &kept, &contiguous);", "m_vertexArrays[i].ReadBinary(&records, &contiguous, &kept);"),
    ("xgg: vertex arrays, the first again", M, "m_vertexArrays[i].ReadBinary(", "m_vertexArrays[0].ReadBinary("),
    ("xgg: index array records", M, "    records = scratch + header.m_indexArrayOffset;", "    records = scratch + header.m_streamOffset;"),
    ("xgg: index data", M, "    kept = m_kept + header.m_indexDataOffset;", "    kept = m_kept + header.m_streamDataOffset;"),
    ("xgg: index array cursors", M, "m_indexArrays[i].ReadBinary(&records, &kept);", "m_indexArrays[i].ReadBinary(&kept, &records);"),
    ("xgg: index arrays not made", M, "    MakeIndexArrays(owner);\n    records", "    records"),
    ("xgg: streams not made", M, "    MakeStreams();\n", ""),
    ("xgg: stream records", M, "    records = scratch + header.m_streamOffset;", "    records = scratch + header.m_meshOffset;"),
    ("xgg: stream data", M, "    kept = m_kept + header.m_streamDataOffset;", "    kept = m_kept + header.m_vertexArrayDataOffset;"),
    ("xgg: stream's owner", M, "m_streams[i].ReadBinary(owner, &records, &kept);", "m_streams[i].ReadBinary(0, &records, &kept);"),
    ("xgg: triangles", M, "    char* triangles = m_kept + header.m_triangleOffset;", "    char* triangles = m_kept + header.m_partOffset;"),
    ("xgg: parts", M, "    char* parts = m_kept + header.m_partOffset;", "    char* parts = m_kept + header.m_triangleOffset;"),
    ("xgg: material references", M, "    char* materials = scratch + header.m_materialRefOffset;", "    char* materials = scratch + header.m_meshOffset;"),
    ("xgg: mesh records", M, "    records = scratch + header.m_meshOffset;", "    records = scratch + header.m_materialRefOffset;"),
    ("xgg: mesh kept", M, "    kept = m_kept + header.m_meshKeptOffset;", "    kept = m_kept + header.m_triangleOffset;"),
    ("xgg: mesh contiguous", M, "    contiguous = (char*)m_contiguous + header.m_meshContiguousOffset;", "    contiguous = (char*)m_contiguous + header.m_vertexDataOffset;"),
    ("xgg: mesh's database", M, "m_meshes[i].ReadBinary(owner, this,", "m_meshes[i].ReadBinary(owner, 0,"),
    ("xgg: mesh cursors", M, "&triangles, &parts, &materials, &records, &kept, &contiguous, meshName);", "&parts, &triangles, &materials, &records, &kept, &contiguous, meshName);"),
    ("xgg: each name to the first mesh", M, "        Insert(meshName, &m_meshes[i]);", "        Insert(meshName, &m_meshes[0]);"),
    ("xgg: first block kept", M, "        Insert(meshName, &m_meshes[i]);\n    }\n    operator delete(scratch);\n", "        Insert(meshName, &m_meshes[i]);\n    }\n"),
    ("slot 8: no raise with no heap", M, "    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    m_more", "    AtPushHeap(s_heap);\n    m_more"),
    ("slot 8: no pop", M, "    m_more = new AtGrD3DMeshDatabase;\n    AtPopHeap();\n", "    m_more = new AtGrD3DMeshDatabase;\n"),
    ("slot 8: no group's name", M, "        strcpy(ns, m_group->m_name);", "        ns[0] = 0;"),
    ("slot 8: the mesh count", M, "    m_more->Allocate(m_owner, m_meshCount, 0, 0, ns);", "    m_more->Allocate(m_owner, 1, 0, 0, ns);"),
    ("slot 8: the owner", M, "    m_more->Allocate(m_owner, m_meshCount, 0, 0, ns);", "    m_more->Allocate(m_owner, m_meshCount, 0, 0, 0);"),
    ("mesh at: the first", M, "    return &m_meshes[index];", "    return &m_meshes[0];"),
    ("vertex array at: the first", M, "    return &m_vertexArrays[index];", "    return &m_vertexArrays[0];"),
    ("index array at: the first", M, "    return &m_indexArrays[index];", "    return &m_indexArrays[0];"),
]

# ---- the binary file, the keys and the animation database ---------------------------------------------------------------------------------------------------------------------------------------------------
MUTANTS += [
    ("file: the top whatever the flags", A, "    if (flags & 1) {\n        fs = s_context->Top();", "    if (flags & 2) {\n        fs = s_context->Top();"),
    ("file: no file found goes on", A, "    } else if (s_context->FindFile(path, &fs) != 0) {\n        return 1;\n    }", "    } else if (s_context->FindFile(path, &fs) != 0) {\n        return 2;\n    }"),
    ("file: an open file not closed", A, "        m_fs->Close(m_file);\n        m_fs = 0;\n        m_file = 0;\n    }\n    m_fs = fs;", "        m_fs = 0;\n        m_file = 0;\n    }\n    m_fs = fs;"),
    ("file: the flags not passed", A, "    return fs->Open(path, flags, &m_file);", "    return fs->Open(path, 0, &m_file);"),
    ("file: the leaf from the start always", A, "    if (leaf == 0) {\n        leaf = full;\n    }\n", "    leaf = full;\n"),
    ("file: extension added always", A, "    if (strrchr(leaf, '.') == 0) {", "    if (strrchr(leaf, '.') == 0 || 1) {"),
    ("file: no dot", A, "        strcat(full, \".\");\n", ""),
    ("file: no wait", A, "        status = request->Wait();\n        if (m_file != 0) {", "        if (m_file != 0) {"),
    ("file: no release", A, "        if (m_file != 0) {\n            m_file->Release(request);\n        }\n", ""),
    ("file: read without the count", A, "    return m_file->Read(out, size, done, request);", "    return m_file->Read(out, size, 0, request);"),
    ("file: release says 1 with no file", A, "    if (m_file == 0) {\n        return 0;\n    }\n    m_file->Release(request);", "    if (m_file == 0) {\n        return 1;\n    }\n    m_file->Release(request);"),
    ("file: release does not release", A, "    m_file->Release(request);\n    return 1;", "    return 1;"),
    ("file: close does not close", A, "    if (m_file != 0) {\n        m_fs->Close(m_file);\n    }\n    m_fs = 0;", "    m_fs = 0;"),
    ("block: no wait", A, "        status = request->Wait();\n        file->Release(request);", "        file->Release(request);"),
    ("block: no release", A, "        status = request->Wait();\n        file->Release(request);", "        status = request->Wait();"),
    ("keys: clear forgets nothing", A, "    if (m_flags & 0x400) {", "    if (m_flags & 0x800) {"),
    ("keys: clear without the base's", A, "    AtAnimationKeys::Clear();\n    m_frames = 0;", "    m_frames = 0;"),
    ("keys: clear leaves +40", A, "    m_unknown40 = 0;\n    m_unknown44 = 0;\n}\n\nvoid AtGrD3DAnimationKeys::Free", "    m_unknown44 = 0;\n}\n\nvoid AtGrD3DAnimationKeys::Free"),
    ("keys: free frees the rotations", A, "    operator delete(m_frames);", "    operator delete(m_rotationKeys);"),
    ("keys: slot 14", A, "void AtGrD3DAnimationKeys::Slot14() {\n    operator delete(m_rotationKeys);", "void AtGrD3DAnimationKeys::Slot14() {\n    operator delete(m_frames);"),
    ("keys: slot 15", A, "    operator delete(m_translationKeys);", "    operator delete(m_scaleKeys);"),
    ("keys: slot 16", A, "void AtGrD3DAnimationKeys::Slot16() {\n    operator delete(m_scaleKeys);", "void AtGrD3DAnimationKeys::Slot16() {\n    operator delete(m_noteKeys);"),
    ("keys: slot 17", A, "void AtGrD3DAnimationKeys::Slot17() {\n    operator delete(m_noteKeys);", "void AtGrD3DAnimationKeys::Slot17() {\n    operator delete(m_cameraKeys);"),
    ("keys: slot 18", A, "void AtGrD3DAnimationKeys::Slot18() {\n    operator delete(m_cameraKeys);", "void AtGrD3DAnimationKeys::Slot18() {\n    operator delete(m_frames);"),
    ("keys: read without init", A, "    Init(0, 0, 0, 0, 0, 0);\n", ""),
    ("keys: frame count", A, "    m_count = header->m_frameCount;", "    m_count = header->m_rotationKeyCount;"),
    ("keys: frames", A, "    m_frames = (unsigned short*)(data + header->m_frameOffset);", "    m_frames = (unsigned short*)(data + header->m_rotationKeyOffset);"),
    ("keys: rotation count", A, "    m_rotationKeyCount = header->m_rotationKeyCount;", "    m_rotationKeyCount = header->m_frameCount;"),
    ("keys: rotations", A, "    m_rotationKeys = data + header->m_rotationKeyOffset;", "    m_rotationKeys = data + header->m_frameOffset;"),
    ("keys: translation count", A, "    m_translationKeyCount = header->m_translationKeyCount;", "    m_translationKeyCount = header->m_scaleKeyCount;"),
    ("keys: translations", A, "    m_translationKeys = data + header->m_translationKeyOffset;", "    m_translationKeys = data + header->m_scaleKeyOffset;"),
    ("keys: scale count", A, "    m_scaleKeyCount = header->m_scaleKeyCount;", "    m_scaleKeyCount = header->m_translationKeyCount;"),
    ("keys: scales", A, "    m_scaleKeys = data + header->m_scaleKeyOffset;", "    m_scaleKeys = data + header->m_translationKeyOffset;"),
    ("keys: note count", A, "    m_noteKeyCount = header->m_noteKeyCount;", "    m_noteKeyCount = header->m_cameraKeyCount;"),
    ("keys: notes", A, "    m_noteKeys = data + header->m_noteKeyOffset;", "    m_noteKeys = data + header->m_cameraKeyOffset;"),
    ("keys: camera count", A, "    m_cameraKeyCount = header->m_cameraKeyCount;", "    m_cameraKeyCount = header->m_noteKeyCount;"),
    ("keys: cameras", A, "    m_cameraKeys = data + header->m_cameraKeyOffset;", "    m_cameraKeys = data + header->m_noteKeyOffset;"),
    ("keys: the file's flags", A, "    m_flags |= header->m_keyFlags | 0x400;", "    m_flags |= 0x400;"),
    ("keys: not marked as a file's", A, "    m_flags |= header->m_keyFlags | 0x400;", "    m_flags |= header->m_keyFlags;"),
    ("keys: +40", A, "    m_unknown40 = header->m_keyUnknown14;", "    m_unknown40 = header->m_keyUnknown18;"),
    ("keys: +44", A, "    m_unknown44 = header->m_keyUnknown18;", "    m_unknown44 = header->m_keyUnknown14;"),
    ("anim: clear leaves the components", A, "        operator delete(m_data);\n        m_rotation = 0;\n", "        operator delete(m_data);\n"),
    ("anim: clear leaves the camera components", A, "        m_note = 0;\n        m_camera = 0;\n    }\n    AtAnimationDatabase::Clear();", "        m_note = 0;\n    }\n    AtAnimationDatabase::Clear();"),
    ("anim: clear keeps the block", A, "        operator delete(m_data);\n", ""),
    ("anim: clear without the base's", A, "    AtAnimationDatabase::Clear();\n    m_data = 0;", "    m_data = 0;"),
    ("anim: clear leaves the block's pointer", A, "    AtAnimationDatabase::Clear();\n    m_data = 0;", "    AtAnimationDatabase::Clear();"),
    ("anim: no binary form", A, "int AtGrD3DAnimationDatabase::HasBinaryForm() {\n    return 1;", "int AtGrD3DAnimationDatabase::HasBinaryForm() {\n    return 0;"),
    ("xka: the wrong extension", A, "file.OpenWithExtension(name, \"XKA\", 2);", "file.OpenWithExtension(name, \"XKB\", 2);"),
    ("xka: no report on the table", A, "    status = file.ReadAll(&header, sizeof header, 0);\n    if (status != 0) {\n        AtReportFileError(status, name, \"XKA\");\n    }\n", "    status = file.ReadAll(&header, sizeof header, 0);\n"),
    ("xka: the table read short", A, "    status = file.ReadAll(&header, sizeof header, 0);", "    status = file.ReadAll(&header, sizeof header - 8, 0);"),
    ("xka: the first block on the database's heap", A, "    AtPushHeap(AtIoTokenFile::s_heap);", "    AtPushHeap(s_heap);"),
    ("xka: the first block's size", A, "    char* scratch = (char*)operator new(header.m_scratchSize);", "    char* scratch = (char*)operator new(header.m_keptSize);"),
    ("xka: the second block, no raise with no heap", A, "    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    m_data", "    AtPushHeap(s_heap);\n    m_data"),
    ("xka: the second block's size", A, "    m_data = (char*)operator new(header.m_keptSize);", "    m_data = (char*)operator new(header.m_scratchSize);"),
    ("xka: the second block not read", A, "    status = AtReadBlock(&file, m_data, header.m_keptSize);\n", ""),
    ("xka: no device kept", A, "    m_device = device;\n", ""),
    ("xka: the keys from the wrong block", A, "->ReadBinary(&header, m_data);", "->ReadBinary(&header, scratch);"),
    ("xka: rotation count", A, "    m_rotationCount = header.m_rotationCount;", "    m_rotationCount = header.m_translationCount;"),
    ("xka: translation count", A, "    m_translationCount = header.m_translationCount;", "    m_translationCount = header.m_rotationCount;"),
    ("xka: scale count", A, "    m_scaleCount = header.m_scaleCount;", "    m_scaleCount = header.m_noteCount;"),
    ("xka: note count", A, "    m_noteCount = header.m_noteCount;", "    m_noteCount = header.m_scaleCount;"),
    ("xka: camera count", A, "    m_cameraCount = header.m_cameraCount;", "    m_cameraCount = header.m_noteCount;"),
    ("xka: animation count", A, "    m_animationCount = header.m_animationCount;", "    m_animationCount = header.m_rotationCount;"),
    ("xka: rotations", A, "    m_rotation = (AtAnimationComponent*)(m_data + header.m_rotationOffset);", "    m_rotation = (AtAnimationComponent*)(m_data + header.m_translationOffset);"),
    ("xka: translations", A, "    m_translation = (AtAnimationComponent*)(m_data + header.m_translationOffset);", "    m_translation = (AtAnimationComponent*)(m_data + header.m_rotationOffset);"),
    ("xka: scales", A, "    m_scale = (AtAnimationComponent*)(m_data + header.m_scaleOffset);", "    m_scale = (AtAnimationComponent*)(m_data + header.m_noteOffset);"),
    ("xka: notes", A, "    m_note = (AtAnimationComponent*)(m_data + header.m_noteOffset);", "    m_note = (AtAnimationComponent*)(m_data + header.m_scaleOffset);"),
    ("xka: cameras", A, "    m_camera = (AtAnimationComponent*)(m_data + header.m_cameraOffset);", "    m_camera = (AtAnimationComponent*)(m_data + header.m_noteOffset);"),
    ("xka: animations, no raise with no memory", A, "    if (m_animations == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("xka: no map", A, "    Init(m_animationCount);\n", ""),
    ("xka: names", A, "    const char* names = scratch + header.m_namesOffset;", "    const char* names = scratch + header.m_recordsOffset;"),
    ("xka: records", A, "(const AtXkaAnimation*)(scratch + header.m_recordsOffset);", "(const AtXkaAnimation*)(scratch + header.m_namesOffset);"),
    ("xka: names four apart", A, "        Insert(names + i * 8, &m_animations[i]);", "        Insert(names + i * 4, &m_animations[i]);"),
    ("xka: every name to the first", A, "        Insert(names + i * 8, &m_animations[i]);", "        Insert(names + i * 8, &m_animations[0]);"),
    ("xka: the first record each time", A, "        m_animations[i].ReadBinary(this, &records[i]);", "        m_animations[i].ReadBinary(this, &records[0]);"),
    ("xka: first block kept", A, "    operator delete(scratch);\n", ""),
    ("device: keys, no raise with no heap", A, "    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    AtGrD3DAnimationKeys* keys", "    AtPushHeap(s_heap);\n    AtGrD3DAnimationKeys* keys"),
    ("device: keys, no raise with no memory", A, "    if (keys == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("device: keys not freed", A, "    delete static_cast<AtGrD3DAnimationKeys*>(keys);\n", ""),
    ("device: database, no pop", A, "    AtGrD3DAnimationDatabase* database = new AtGrD3DAnimationDatabase;\n    AtPopHeap();\n", "    AtGrD3DAnimationDatabase* database = new AtGrD3DAnimationDatabase;\n"),
    ("device: database, no raise with no memory", A, "    if (database == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("device: database not freed", A, "    delete database;\n", ""),
]
for field in ["frames", "rotationKeys", "translationKeys", "scaleKeys", "noteKeys", "cameraKeys", "unknown40", "unknown44"]:
    MUTANTS.append(("keys: constructor leaves m_%s" % field, A, "m_%s(0)" % field, "m_%s(m_%s)" % (field, field)))
MUTANTS.append(("anim: constructor leaves the block", A, "AtGrD3DAnimationDatabase::AtGrD3DAnimationDatabase() : m_data(0) {", "AtGrD3DAnimationDatabase::AtGrD3DAnimationDatabase() : m_data(m_data) {"))

# ---- the keys' base and an animation's record --------------------------------------------------------------------------------------------------------------------------------------------------------------
for field in ["rotationKeyCount", "translationKeyCount", "scaleKeyCount", "noteKeyCount", "cameraKeyCount"]:
    MUTANTS.append(("keys base: constructor leaves m_%s" % field, K, "m_%s(0)" % field, "m_%s(m_%s)" % (field, field)))
for n, field in [(14, "rotationKeyCount"), (15, "translationKeyCount"), (16, "scaleKeyCount"), (17, "noteKeyCount"), (18, "cameraKeyCount")]:
    MUTANTS.append(("keys base: clear does not free by m_%s" % field, K, "    if (m_%s > 0) {\n        Slot%d();\n    }\n" % (field, n), ""))
MUTANTS += [
    ("keys base: clear without the set's", K, "    AtGrKeySet::Clear();\n", ""),
    ("keys base: clear leaves a count", K, "    AtGrKeySet::Clear();\n    m_rotationKeyCount = 0;\n", "    AtGrKeySet::Clear();\n"),
    ("keys base: clear leaves the camera count", K, "    m_noteKeyCount = 0;\n    m_cameraKeyCount = 0;\n}\n\nvoid AtAnimationKeys::SetExternalName", "    m_noteKeyCount = 0;\n}\n\nvoid AtAnimationKeys::SetExternalName"),
    ("keys base: destructor does not clear", K, "AtAnimationKeys::~AtAnimationKeys() {\n    AtAnimationKeys::Clear();\n}", "AtAnimationKeys::~AtAnimationKeys() {\n}"),
    ("keys base: name without the set's", K, "    AtGrKeySet::SetExternalName(name);\n", ""),
    ("keys base: name's flags", K, "    m_flags |= 0x150;", "    m_flags |= 0x140;"),
    ("keys base: init reserves nothing", K, "    Reserve(frames);", "    Reserve(0);"),
    ("keys base: init rotations", K, "    m_rotationKeyCount = rotations;", "    m_rotationKeyCount = translations;"),
    ("keys base: init allocates rotations always", K, "    if (rotations != 0) {\n        Slot9(rotations);\n    }\n", "    Slot9(rotations);\n"),
    ("keys base: init translations", K, "        Slot10(translations);", "        Slot10(scales);"),
    ("keys base: init scales", K, "    m_scaleKeyCount = scales;", "    m_scaleKeyCount = notes;"),
    ("keys base: init notes", K, "    if (notes != 0) {\n        Slot12(notes);", "    if (cameras != 0) {\n        Slot12(notes);"),
    ("keys base: init cameras", K, "    m_cameraKeyCount = cameras;", "    m_cameraKeyCount = 0;"),
    ("keys base: init's flags", K, "    m_flags |= 0xA8;", "    m_flags |= 0x28;"),
    ("record: database", K, "    m_database = database;", "    m_database = 0;"),
    ("record: flags", K, "    m_flags = record->m_flags;", "    m_flags = record->m_length;"),
    ("record: velocity", K, "    CopyWord(&m_velocity[1], &record->m_velocity[1]);", "    CopyWord(&m_velocity[1], &record->m_velocity[2]);"),
    ("record: angular velocity", K, "    CopyWord(&m_angularVelocity[2], &record->m_angularVelocity[2]);", "    CopyWord(&m_angularVelocity[2], &record->m_angularVelocity[1]);"),
    ("record: rotation", K, "    m_rotation[1] = (unsigned short)record->m_rotation[1];", "    m_rotation[1] = (unsigned short)record->m_rotation[0];"),
    ("record: translation", K, "    m_translation[0] = (unsigned short)record->m_translation[0];", "    m_translation[0] = (unsigned short)record->m_translation[1];"),
    ("record: scale", K, "    m_scale[1] = (unsigned short)record->m_scale[1];", "    m_scale[1] = (unsigned short)record->m_note[1];"),
    ("record: note", K, "    m_note[0] = (unsigned short)record->m_note[0];", "    m_note[0] = (unsigned short)record->m_scale[0];"),
    ("record: camera", K, "    m_camera[1] = (unsigned short)record->m_camera[1];", "    m_camera[1] = (unsigned short)record->m_camera[0];"),
    ("record: length", K, "    m_length = (unsigned short)record->m_length;", "    m_length = (unsigned short)record->m_loopPeriod;"),
    ("record: loop", K, "    m_loopPeriod = (unsigned short)record->m_loopPeriod;", "    m_loopPeriod = (unsigned short)record->m_length;"),
    ("record: sphere", K, "    CopyWord(&m_boundingSphere.center.y, &record->m_boundingSphere[1]);", "    CopyWord(&m_boundingSphere.center.y, &record->m_boundingSphere[0]);"),
    ("record: radius", K, "    CopyWord(&m_boundingSphere.radius, &record->m_boundingSphere[3]);", "    CopyWord(&m_boundingSphere.radius, &record->m_boundingSphere[2]);"),
]

SUITE = {"target": "oracle_loader", "subsystem": "at/loader", "src": "at", "var": "KND_AT_SRC", "cases": "tok", "mutants": MUTANTS}
