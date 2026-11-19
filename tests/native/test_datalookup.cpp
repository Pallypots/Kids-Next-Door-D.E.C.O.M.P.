// Host test of reading a loaded design data registry: lookups, the copy form, start-up's calls and the value classes' slots, on any pointer size.
// Not evidence of a byte match.
#include <cstdio>
#include <cstring>
#include <string>

#include "at/AtDataRegistry.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

int g_heap;

void Read(AtDataRegistry& registry, AtDataRegistryOwner& owner, const char* text, AtDataRegistry* parent) {
    Memory stream;
    AtIoTextTokenizer tokenizer;
    stream.m_base = stream.m_cur = (char*)text;
    stream.m_limit = stream.m_base + std::strlen(text);
    stream.m_size = (unsigned int)std::strlen(text);
    tokenizer.SetStream(&stream);
    CHECK(registry.Read(&owner, &tokenizer, nullptr, parent) == 0);
    stream.m_base = stream.m_cur = stream.m_limit = nullptr;
    tokenizer.SetStream(nullptr);
}

const AtIoKeyword kWords[] = {{0x54, "WillAcceptParentData"}};

// What a value writes, as text.
std::string Written(AtDataComponent* value) {
    static char buffer[0x400];
    std::memset(buffer, 0, sizeof buffer);
    Memory stream;
    stream.m_base = stream.m_cur = buffer;
    stream.m_size = sizeof buffer - 1;
    stream.m_limit = buffer + stream.m_size;
    AtIoTextTokenizer tokenizer;
    tokenizer.SetKeywords(kWords, 1, nullptr);
    tokenizer.SetStream(&stream);
    value->Write(&tokenizer);
    const std::string text(buffer, stream.m_cur - buffer);
    stream.m_base = stream.m_cur = stream.m_limit = nullptr;
    tokenizer.SetStream(nullptr);
    return text;
}

}  // namespace

int main() {
    AtDataRegistry::SetHeap(&g_heap);
    AtDataComponent::SetHeap(&g_heap);
    AtDataDescriptor::SetHeap(&g_heap);
    AtDataLink::SetHeap(&g_heap);
    AtDataRegistryNameSpaces::SetHeap(&g_heap);
    AtIoTokenFile::s_heap = &g_heap;
    AtIoStream::s_heap = &g_heap;
    GmNameMap::s_heap = &g_heap;

    static AtDataRegistryOwner owner;
    owner.m_spaces.Init(4);

    char base_text[] =
        "DataComponentTypes[5]\n{\n"
        "  DataComponent { Type Empty NumComponents 1 }\n"
        "  DataComponent { Type Int NumComponents 2 }\n"
        "  DataComponent { Type RealArray NumComponents 1 }\n"
        "  DataComponent { Type String NumComponents 1 }\n"
        "  DataComponent { Type ShortName NumComponents 1 }\n"
        "}\n"
        "DataRegistry[6]\n{\n"
        "  DataLink { Name \"Actor\" Type Empty Data }\n"
        "  DataLink { Name \"Flags\" Type Int Parent \"Actor\\\" Floor 0 Ceiling 7 Data 99 WillAcceptParentData }\n"
        "  DataLink { Name \"Range\" Type RealArray NumElements 3 Parent \"Actor\\\" Data -5.0 0.5 5.0 }\n"
        "  DataLink { Name \"More\" Type Int Parent \"Actor\\\" Data 12 }\n"
        "  DataLink { Name \"Title\" Type String Data \"Hello\" }\n"
        "  DataLink { Name \"Short\" Type ShortName Data \"Nm\" }\n"
        "}\n";
    AtDataRegistry base;
    Read(base, owner, base_text, nullptr);
    AtDataLink* actor = base.FindChild("Actor", nullptr);
    AtDataLink* flags = base.FindChild("Flags", actor);
    AtDataLink* range = base.FindChild("Range", actor);
    AtDataLink* more = base.FindChild("More", actor);
    CHECK(actor && flags && range && more);

    // By path.
    CHECK(base.FindPath(nullptr, "Actor\\Flags", 0) == flags);
    CHECK(base.FindPath(nullptr, "Actor\\Flags\\", 0) == flags);
    CHECK(base.FindPath(nullptr, "Actor", 0) == actor);
    CHECK(base.FindPath(nullptr, "Flags", 0) == nullptr);
    CHECK(base.FindPath(nullptr, "actor", 0) == nullptr);
    CHECK(base.FindPath(actor, "", 0) == actor);
    CHECK(base.FindPath(actor, nullptr, 1) == actor);
    CHECK(base.FindPath(nullptr, "Actor\\Nope", 0) == nullptr);
    // Climbing: More is not among Range's children but among its parent's.
    CHECK(base.FindPath(range, "More", 0) == nullptr);
    CHECK(base.FindPath(range, "More", 1) == more);
    CHECK(base.FindPath(range, "Actor\\Range", 1) == range);
    // A part found nowhere leaves nothing, and the next part is looked for from the root.
    CHECK(base.FindPath(range, "Zip\\Actor", 1) == actor);
    CHECK(base.FindPath(range, "Zip", 1) == nullptr);
    CHECK(base.FindFromRoot("Actor\\More", 0) == more);
    CHECK(base.ValueAtPath("Actor\\More", 0) == more->m_value);
    CHECK(base.ValueAtPath("Actor\\Gone", 1) == nullptr);
    CHECK(base.ValueAt(actor, "Flags", 0) == flags->m_value);

    // By name.
    CHECK(base.FindLink("flags", nullptr, 0) == flags);
    CHECK(base.FindLink("flags", nullptr, 1) == nullptr);
    CHECK(base.FindLink("Flags", nullptr, 1) == flags);
    CHECK(base.FindLink("Title", actor, 0) == base.FindInPools("Title", 1));  // Below Actor, then through Actor's siblings: Title hangs beside it.
    CHECK(base.FindLink("Flags", more, 0) == nullptr);  // More's siblings come after it, and Flags is before.
    CHECK(base.FindInPools("MORE", 0) == more);
    CHECK(base.FindInPools("MORE", 1) == nullptr);
    CHECK(base.ValueNamed("more") == more->m_value);
    CHECK(base.ValueNamed("Nothing") == nullptr);

    // A link's path.
    char path[0x100] = "";
    flags->AppendPath(path, 0);
    CHECK(std::strcmp(path, "Actor\\Flags\\") == 0);
    path[0] = 0;
    base.m_root.AppendPath(path, 1);
    CHECK(path[0] == 0);

    // The slots.
    AtDataInt* flag = static_cast<AtDataInt*>(flags->m_value);
    CHECK(flag->ElementAt(9) == &flag->m_value);
    CHECK(Written(flag) == "7 WillAcceptParentData");
    const int three = 3;
    flag->SetElement(&three, 5, 5);
    CHECK(flag->m_value == 3);
    CHECK(flag->Equals(more->m_value) == 0);
    AtDataRealArray* reals = static_cast<AtDataRealArray*>(range->m_value);
    CHECK(reals->ElementAt(2) == &reals->m_data[2]);
    CHECK(Written(reals) == "-5.000000 0.500000 5.000000 ");
    reals->RemoveAt(0);
    CHECK(reals->m_descriptor->m_count == 2 && reals->m_data[0] == 0.5f && reals->m_data[1] == 5.0f);
    reals->RemoveAt(7);
    CHECK(reals->m_descriptor->m_count == 1 && reals->m_data[0] == 0.5f);
    const float two = 2.0f;
    reals->SetElement(&two, 0, 1);
    CHECK(reals->m_data[0] == 2.0f);
    AtDataString* title = static_cast<AtDataString*>(base.FindInPools("Title", 1)->m_value);
    CHECK(Written(title) == "\"Hello\"");
    CHECK(title->ElementAt(0) == nullptr);
    AtDataShortName* shortName = static_cast<AtDataShortName*>(base.FindInPools("Short", 1)->m_value);
    CHECK(Written(shortName) == "\"Nm\"");
    CHECK(shortName->Equals(shortName) == 1);
    AtDataReal one;
    AtDataReal other;
    one.m_value = 0.0f;
    other.m_value = -0.0f;
    CHECK(one.Equals(&other) == 1);

    // The copy form: every link under the copy's root, the values copied, the parent found by path.
    AtDataRegistry copy;
    CHECK(copy.CopyFrom(&base) == 0);
    CHECK(copy.m_loaded == 1 && copy.m_group == &owner.m_plain && copy.m_parent == nullptr);
    AtDataLink* copiedFlags = copy.FindPath(nullptr, "Flags", 0);
    CHECK(copiedFlags != nullptr && copiedFlags != flags);
    CHECK(copiedFlags->m_parent == copy.FindPath(nullptr, "Actor", 0));
    CHECK(static_cast<AtDataInt*>(copiedFlags->m_value)->m_value == 3);
    CHECK(copiedFlags->m_value->m_registry == &copy);
    AtDataString* copiedTitle = static_cast<AtDataString*>(copy.FindInPools("Title", 1)->m_value);
    CHECK(copiedTitle->m_unicode == 1 && copiedTitle->m_wide.m_end == 5 && copiedTitle->m_wide.m_data[4] == 'o');
    AtDataRealArray* copiedReals = static_cast<AtDataRealArray*>(copy.FindInPools("Range", 1)->m_value);
    CHECK(copiedReals->m_descriptor->m_count == 1 && copiedReals->m_data[0] == 2.0f && copiedReals->m_data != reals->m_data);

    // Start-up: an empty registry everything hangs under, a loaded one attached beneath it, and its file name.
    AtDataRegistry top;
    CHECK(top.Prepare(&owner, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, nullptr) == 0);
    CHECK(top.m_loaded == 1 && top.m_group == &owner.m_plain && top.m_pools[AtDataRegistry::kInt].m_count == 0);
    CHECK(base.AttachUnder(&top, nullptr) == 1);
    CHECK(base.m_root.m_parent == &top.m_root);
    CHECK(top.FindPath(nullptr, "Actor\\More", 0) == more);  // Through base's root, which has no value.
    CHECK(copy.AttachUnder(&base, "Nowhere") == 0);
    CHECK(copy.m_root.m_parent == nullptr);
    top.SetFileName("Library");
    CHECK(std::strcmp(top.m_fileName, "Library") == 0);
    char* block = top.m_fileName;
    top.SetFileName("Lib");
    CHECK(top.m_fileName == block && std::strcmp(top.m_fileName, "Lib") == 0);
    top.SetFileName("A longer name");
    CHECK(std::strcmp(top.m_fileName, "A longer name") == 0);
    AtDataRegistry counted;
    CHECK(counted.Prepare(&owner, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, &top) == 0);
    CHECK(counted.m_pools[AtDataRegistry::kBoolean].m_count == 2 && counted.m_pools[AtDataRegistry::kBoolean].m_values != nullptr);
    CHECK(counted.m_pools[AtDataRegistry::kStringArray].m_links != nullptr && counted.m_parent == &top);

    // The string in characters of the caller's.
    AtDataString held;
    AtDataDescriptor descriptor;
    char characters[16];
    CHECK(held.Bind(&descriptor, characters, sizeof characters) == 0);
    CHECK(held.m_narrow.m_data == characters && held.m_chars == nullptr);
    held.Forget();
    CHECK(held.m_narrow.m_capacity == 0 && held.m_descriptor == &descriptor);

    counted.Clear();
    copy.Clear();
    base.Clear();
    top.Clear();

    if (g_fail) {
        std::printf("%d failed\n", g_fail);
        return 1;
    }
    std::printf("test_datalookup passed\n");
    return 0;
}
