// The registry names' Add and Release, and the name Add files a registry under. See include/gm/GmRegistryNames.h. Names inferred.
// Apart from GmRegistryNames.cpp so that a test which supplies its own Add and Release still links the rest of the class.
#include "gm/GmRegistryNames.h"

#include "at/AtDataRegistry.h"
#include "at/AtIoTokenizer.h"

extern "C" {
char* strcpy(char* to, const char* from);
char* strstr(const char* text, const char* part);    // 0x001269D0
char* strrchr(const char* text, int c);              // 0x00125F40
}

void GmRegistryNames::KeyFor(char* key, const AtDataRegistry* registry) {
    char name[0x100];
    strcpy(name, registry->m_fileName);
    // The first of the four found, in this order; the cut need not be at the end, nor the extension.
    char* extension = strstr(name, ".a");
    if (extension == 0) {
        extension = strstr(name, ".A");
    }
    if (extension == 0) {
        extension = strstr(name, ".b");
    }
    if (extension == 0) {
        extension = strstr(name, ".B");
    }
    if (extension != 0) {
        *extension = 0;
    }
    const char* separator = strrchr(name, '\\');
    if (separator != 0) {
        AtCopyName(separator + 1, key);
    } else {
        AtCopyName(name, key);
    }
}

void GmRegistryNames::Add(AtDataRegistry* registry) {
    char key[8];
    KeyFor(key, registry);
    Insert(key, registry);
}

void GmRegistryNames::Release(AtDataRegistry* registry) {
    char key[8];
    GetNameOf(registry, key);
    Remove(key);
}
