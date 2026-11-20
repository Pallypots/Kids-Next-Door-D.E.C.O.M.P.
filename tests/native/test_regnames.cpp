// Host test of the registry names (include/gm/GmRegistryNames.h), on any pointer size. Not evidence of a byte match. The registries are zeroed storage with only a file name, which is
// all Add reads. No game data.
#include <cstdio>
#include <cstring>
#include <new>

#include "at/AtDataRegistry.h"
#include "gm/GmRegistryNames.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

int g_heap;

struct Registry {
    alignas(AtDataRegistry) unsigned char storage[sizeof(AtDataRegistry)];
    explicit Registry(const char* file) {
        std::memset(storage, 0, sizeof storage);
        get()->m_fileName = (char*)file;
    }
    AtDataRegistry* get() { return (AtDataRegistry*)storage; }
};

bool KeyIs(GmRegistryNames& names, const char* file, const char* expected, unsigned int length) {
    Registry r(file);
    char key[8];
    std::memset(key, 0x5A, sizeof key);
    names.KeyFor(key, r.get());
    if (std::memcmp(key, expected, length) != 0) return false;
    // Past the terminator the key is as it was; a key of eight has none.
    for (unsigned int i = length; i < 8; i++) {
        if ((unsigned char)key[i] != 0x5A) return false;
    }
    return true;
}

}  // namespace

int main() {
    GmNameMap::s_heap = &g_heap;

    GmRegistryNames* names = new GmRegistryNames;
    CHECK(GmRegistryNames::s_instance == names);
    CHECK(names->m_entries == nullptr && names->m_size == 0);
    CHECK(names->Init(20) == 0);
    const unsigned int size = names->m_size;
    CHECK(size > 20 && names->m_entries != nullptr);

    // The name a registry goes under.
    CHECK(KeyIs(*names, "Design\\Registry\\N_Dental.ADR", "N_Dental", 8));
    CHECK(KeyIs(*names, "D:\\DESIGN\\REGISTRY\\PAD.ADR", "PAD", 4));
    CHECK(KeyIs(*names, "design\\registry\\aimer.adr", "aimer", 6));
    CHECK(KeyIs(*names, "Generic", "Generic", 8));                       // no extension, no separator: seven and a terminator
    CHECK(KeyIs(*names, "Library.b", "Library", 8));
    CHECK(KeyIs(*names, "X\\Y.B", "Y", 2));
    CHECK(KeyIs(*names, "Abcdefghij.ADR", "Abcdefgh", 8));             // cut to eight, unterminated
    CHECK(KeyIs(*names, "Dir.adr\\Inner\\Leaf.ADR", "Dir", 4));         // the first ".a" anywhere: the leaf is what precedes it
    CHECK(KeyIs(*names, "One.Ab\\Two.a", "Two", 4));                    // ".a" is looked for before ".A", wherever each is
    CHECK(KeyIs(*names, "One.A\\Two.ADR", "One", 4));                   // ".A" first by position
    CHECK(KeyIs(*names, "P.B\\Q.b", "Q", 2));                           // ".b" is looked for before ".B"
    CHECK(KeyIs(*names, "Dir\\", "", 1));                               // nothing after the separator

    // Add, find, release.
    Registry a("Design\\Registry\\Aimer.ADR"), b("Design\\Registry\\N_Spank.ADR"), c("Design\\Registry\\Pad.ADR"), d("DESIGN\\LOCATION\\X\\INSTANCE.ADR");
    names->Add(a.get());
    names->Add(b.get());
    names->Add(c.get());
    names->Add(d.get());
    CHECK(names->Find("Aimer") == a.get());
    CHECK(names->Find("N_Spank") == b.get());
    CHECK(names->Find("Pad") == c.get());
    CHECK(names->Find("INSTANCE") == d.get());
    CHECK(names->Find("Instance") == nullptr);
    names->Release(b.get());
    CHECK(names->Find("N_Spank") == nullptr);
    CHECK(names->Find("Aimer") == a.get() && names->Find("Pad") == c.get() && names->Find("INSTANCE") == d.get());
    names->Add(b.get());
    CHECK(names->Find("N_Spank") == b.get());

    // Many: the map grows and keeps them.
    static char files[40][32];
    Registry* many[40];
    for (int i = 0; i < 40; i++) {
        std::snprintf(files[i], sizeof files[i], "Design\\Registry\\R%02d.ADR", i);
        many[i] = new Registry(files[i]);
        names->Add(many[i]->get());
    }
    CHECK(names->m_size > size);
    for (int i = 0; i < 40; i++) {
        char key[8];
        std::snprintf(key, sizeof key, "R%02d", i);
        CHECK(names->Find(key) == many[i]->get());
    }
    for (int i = 0; i < 40; i += 2) names->Release(many[i]->get());
    for (int i = 0; i < 40; i++) {
        char key[8];
        std::snprintf(key, sizeof key, "R%02d", i);
        CHECK(names->Find(key) == (i % 2 ? many[i]->get() : nullptr));
    }
    CHECK(names->Find("Aimer") == a.get() && names->Find("N_Spank") == b.get());

    // Init again clears through slot 1 first.
    CHECK(names->Init(4) == 0);
    CHECK(names->Find("Aimer") == nullptr);
    names->Add(a.get());
    CHECK(names->Find("Aimer") == a.get());

    // Clear through slot 1, then the destructor forgets s_instance, whoever it is.
    GmNameMap* base = names;
    base->Clear();
    CHECK(names->m_entries == nullptr && names->m_size == 0);
    names->Init(8);
    GmRegistryNames other;
    CHECK(GmRegistryNames::s_instance == &other);
    delete base;
    CHECK(GmRegistryNames::s_instance == nullptr);
    for (int i = 0; i < 40; i++) delete many[i];

    if (g_fail == 0) std::printf("test_regnames: all passed\n");
    return g_fail ? 1 : 0;
}
