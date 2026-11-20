# Mutants of the registry names (include/gm/GmRegistryNames.h). Each text occurs exactly once in its source.
# Not written, being equivalent by construction:
# * Init's own "if (m_entries != 0) Clear();": the name map's Init (0x00024DA0) begins with the same test and the same call through slot 1, so removing it changes nothing.
# * The destructor's GmNameMap::Clear(): the name map's destructor (0x00025260), which runs next, makes the same call.
R = "GmRegistryNames.cpp"
A = "GmRegistryNamesAdd.cpp"

MUTANTS = [
    # The life cycle.
    ("constructor: not the instance", R, "    s_instance = this;\n", ""),
    ("destructor: instance kept", R, "    s_instance = 0;\n", ""),
    ("slot 1: does nothing", R, "void GmRegistryNames::Clear() {\n    GmNameMap::Clear();\n}", "void GmRegistryNames::Clear() {\n}"),
    ("init: count", R, "    GmNameMap::Init(count);\n", "    GmNameMap::Init(count + 1);\n"),
    ("init: not sized", R, "    GmNameMap::Init(count);\n", ""),
    ("init: returns", R, "    return 0;\n", "    return 1;\n"),
    # The name.
    ("key: no .a", A, 'char* extension = strstr(name, ".a");', 'char* extension = strstr(name, ".x");'),
    ("key: no .A", A, 'extension = strstr(name, ".A");', 'extension = strstr(name, ".X");'),
    ("key: no .b", A, 'extension = strstr(name, ".b");', 'extension = strstr(name, ".x");'),
    ("key: no .B", A, 'extension = strstr(name, ".B");', 'extension = strstr(name, ".X");'),
    ("key: .A before .a", A, 'char* extension = strstr(name, ".a");\n    if (extension == 0) {\n        extension = strstr(name, ".A");',
     'char* extension = strstr(name, ".A");\n    if (extension == 0) {\n        extension = strstr(name, ".a");'),
    ("key: .B before .b", A, 'extension = strstr(name, ".b");\n    }\n    if (extension == 0) {\n        extension = strstr(name, ".B");',
     'extension = strstr(name, ".B");\n    }\n    if (extension == 0) {\n        extension = strstr(name, ".b");'),
    ("key: the last .a", A, 'char* extension = strstr(name, ".a");', 'char* extension = strrchr(name, \'.\') != 0 && strrchr(name, \'.\')[1] == \'a\' ? strrchr(name, \'.\') : 0;'),
    ("key: not cut", A, "        *extension = 0;\n", ""),
    ("key: cut one later", A, "        *extension = 0;\n", "        extension[1] = 0;\n"),
    ("key: separator from the second byte", A, "const char* separator = strrchr(name, '\\\\');", "const char* separator = strrchr(name + 1, '\\\\');"),
    ("key: wrong separator", A, "const char* separator = strrchr(name, '\\\\');", "const char* separator = strrchr(name, '/');"),
    ("key: separator kept", A, "        AtCopyName(separator + 1, key);\n", "        AtCopyName(separator, key);\n"),
    ("key: no separator", A, "        AtCopyName(name, key);\n", "        AtCopyName(name + 1, key);\n"),
    ("key: eight terminated", A, "        AtCopyName(separator + 1, key);\n", "        AtCopyName(separator + 1, key);\n        key[7] = 0;\n"),
    ("key: file name", A, "    strcpy(name, registry->m_fileName);\n", "    strcpy(name, registry->m_fileName + 1);\n"),
    # Add and Release.
    ("add: key cleared first", A, "    char key[8];\n    KeyFor(key, registry);\n", "    char key[8] = {0};\n    KeyFor(key, registry);\n"),
    ("add: not inserted", A, "    Insert(key, registry);\n", ""),
    ("add: wrong value", A, "    Insert(key, registry);\n", "    Insert(key, this);\n"),
    ("release: not removed", A, "    Remove(key);\n", ""),
    ("release: by its file", A, "    GetNameOf(registry, key);\n", "    KeyFor(key, registry);\n"),
    ("release: empty name skipped", A, "    Remove(key);\n", "    if (key[0] != 0) {\n        Remove(key);\n    }\n"),
]
SUITE = {"target": "oracle_regnames", "subsystem": "gm/regnames", "src": "gm", "var": "KND_GM_SRC", "cases": "tok", "mutants": MUTANTS}
