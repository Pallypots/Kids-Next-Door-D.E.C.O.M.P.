// See include/at/AtDataLibrary.h.
#include "at/AtDataLibrary.h"

#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

void* AtDataLibrary::s_heap;
char** AtDataLibrary::s_registryNames;
char** AtDataLibrary::s_databaseNames;

const AtIoKeyword AtDataLibrary::s_keywords[AtDataLibrary::kKeywordCount] = {
    {0x39, "DataRegistry"},
    {0x3A, "DatabaseManager"},
};

namespace {

enum Word { kDataRegistry = 0x39, kDatabaseManager = 0x3A };

// Codes passed to the tokenizer's Error; the word for this one is inferred from where it is used.
enum Problem { kListAlreadyGiven = 0x27 };

// 0xFF bytes, where a string may take 0x100 with its terminator: the longest string a file can give overruns this by one, as in the original.
enum { kTextBuffer = 0xFF, kLongestString = 0x100 };

unsigned int LengthOf(const char* text) {
    unsigned int length = 0;
    while (text[length] != 0) {
        length++;
    }
    return length;
}

void CopyText(char* to, const char* from) {
    unsigned int i = 0;
    while ((to[i] = from[i]) != 0) {
        i++;
    }
}

// "[ n ] { "a" "b" }" up to the last string: a list of names on the token file's heap, with each copy pushing and popping inside one long push.
void ReadNames(AtIoTokenizer* tokenizer, char* text, unsigned int count, char*** out) {
    if (AtIoTokenFile::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtIoTokenFile::s_heap);
    *out = (char**)operator new(count * sizeof(char*));
    AtPopHeap();
    if (*out == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    char** names = *out;
    for (unsigned int i = 0; i < count; i++) {
        names[i] = 0;
    }
    AtPushHeap(AtIoTokenFile::s_heap);
    for (unsigned int i = 0; i < count; i++) {
        CopyText(text, tokenizer->ExpectString(kLongestString));
        if (AtIoTokenFile::s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(AtIoTokenFile::s_heap);
        names[i] = (char*)operator new(LengthOf(text) + 1);
        AtPopHeap();
        if (names[i] == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        CopyText(names[i], text);
    }
    AtPopHeap();
}

void FreeNames(char** names, unsigned int count) {
    for (unsigned int i = 0; i < count; i++) {
        if (names[i] != 0) {
            operator delete(names[i]);
        }
    }
    operator delete(names);
}

}  // namespace

void AtDataRegistryOwner::AddDatabase(GmNameMap* map, const char* space) {
    if (space != 0 && space[0] != 0) {
        m_databaseSpaces.Add(space, map);
    } else {
        m_databases.Add(map);
    }
}

void AtDataLibrary::SetHeap(void* heap) {
    s_heap = heap;
}

AtDataLibrary::AtDataLibrary() {
    m_loaded = 0;
    m_registries = 0;
    m_databases = 0;
    m_registryCount = 0;
    m_databaseCount = 0;
    m_owner = 0;
    s_registryNames = 0;
    s_databaseNames = 0;
}

AtDataLibrary::~AtDataLibrary() {
    if (m_loaded != 0) {
        Clear();
    }
}

void AtDataLibrary::MakeRegistries() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_registries = (AtDataRegistry**)operator new(m_registryCount * sizeof(AtDataRegistry*));
    AtPopHeap();
    if (m_registries == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < m_registryCount; i++) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        m_registries[i] = new AtDataRegistry;
        AtPopHeap();
        if (m_registries[i] == 0) {
            AtRaise(-4, 0, 0, 0);
        }
    }
}

void AtDataLibrary::MakeDatabases() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_databases = (AtDatabaseManager**)operator new(m_databaseCount * sizeof(AtDatabaseManager*));
    AtPopHeap();
    if (m_databases == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < m_databaseCount; i++) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        m_databases[i] = new AtDatabaseManager;
        AtPopHeap();
        if (m_databases[i] == 0) {
            AtRaise(-4, 0, 0, 0);
        }
    }
}

void AtDataLibrary::Clear() {
    if (m_registries != 0) {
        for (unsigned int i = 0; i < m_registryCount; i++) {
            AtDataRegistry* registry = m_registries[i];
            if (registry != 0) {
                if (registry->m_loaded != 0) {
                    registry->Clear();
                }
                delete registry;
                m_registries[i] = 0;
            }
        }
        operator delete(m_registries);
    }
    if (m_databases != 0) {
        for (unsigned int i = 0; i < m_databaseCount; i++) {
            AtDatabaseManager* database = m_databases[i];
            if (database != 0) {
                if (database->m_loaded != 0) {
                    database->Clear();
                }
                delete database;
                m_databases[i] = 0;
            }
        }
        operator delete(m_databases);
    }
    m_loaded = 0;
    m_registries = 0;
    m_databases = 0;
    m_registryCount = 0;
    m_databaseCount = 0;
    m_owner = 0;
    s_registryNames = 0;
    s_databaseNames = 0;
}

void AtDataLibrary::LoadEntries(int unused, AtDataRegistry* parent) {
    for (unsigned int i = 0; i < m_registryCount; i++) {
        char space[8];
        space[0] = 0;
        m_registries[i]->Load(m_owner, s_registryNames[i], unused, space, parent);
    }
    for (unsigned int i = 0; i < m_databaseCount; i++) {
        char space[8];
        space[0] = 0;
        m_databases[i]->Load(m_owner, s_databaseNames[i], unused, space, 0, 1);
    }
    if (s_registryNames != 0) {
        FreeNames(s_registryNames, m_registryCount);
        s_registryNames = 0;
    }
    if (s_databaseNames != 0) {
        FreeNames(s_databaseNames, m_databaseCount);
        s_databaseNames = 0;
    }
}

void AtDataLibrary::Load(AtDataRegistryOwner* owner, const char* name, int unused, AtDataRegistry* parent) {
    AtIoTokenFile file;
    if (m_loaded != 0) {
        Clear();
    }
    const int status = file.Open(name, "ADD", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "ADD");
    }
    AtIoFileSystem* fileSystem = file.m_file.m_fs;
    fileSystem->m_path.PushFromFile(file.m_file.m_file->m_path);
    m_owner = owner;
    AtIoTokenizer* tokenizer = file.m_tokenizer;
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);

    if (AtIoTokenFile::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtIoTokenFile::s_heap);
    char* text = (char*)operator new(kTextBuffer);
    AtPopHeap();
    if (text == 0) {
        AtRaise(-4, 0, 0, 0);
    }

    for (int word = tokenizer->Next(); word != AtIoTokenizer::kEnd; word = tokenizer->Next()) {
        if (word == kDataRegistry) {
            if (m_registryCount > 0) {
                tokenizer->Error(kListAlreadyGiven);
            }
            m_registryCount = (unsigned short)tokenizer->ExpectCount();
            MakeRegistries();
            ReadNames(tokenizer, text, m_registryCount, &s_registryNames);
            tokenizer->ExpectRightCurly();
        } else if (word == kDatabaseManager) {
            if (m_databaseCount > 0) {
                tokenizer->Error(kListAlreadyGiven);
            }
            m_databaseCount = (unsigned short)tokenizer->ExpectCount();
            MakeDatabases();
            ReadNames(tokenizer, text, m_databaseCount, &s_databaseNames);
            tokenizer->ExpectRightCurly();
        } else {
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        }
    }

    file.Close();
    operator delete(text);
    LoadEntries(unused, parent);
    fileSystem->m_path.Pop(0);
    m_loaded = 1;
}
