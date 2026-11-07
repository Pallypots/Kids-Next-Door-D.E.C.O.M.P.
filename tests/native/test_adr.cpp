// Host test of the design data registry through the real text tokenizer, on any pointer size. Not evidence of a byte match.
#include <cstdio>
#include <cstring>

#include "at/AtDataRegistry.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

int g_heap;

void Read(AtDataRegistry& registry, AtDataRegistryOwner& owner, const char* text, char* space, AtDataRegistry* parent) {
    Memory stream;
    AtIoTextTokenizer tokenizer;
    stream.m_base = stream.m_cur = (char*)text;
    stream.m_limit = stream.m_base + std::strlen(text);
    stream.m_size = (unsigned int)std::strlen(text);
    tokenizer.SetStream(&stream);
    CHECK(registry.Read(&owner, &tokenizer, space, parent) == 0);
    stream.m_base = stream.m_cur = stream.m_limit = nullptr;
    tokenizer.SetStream(nullptr);
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
        "NameSpace \"Base\"\n"
        "DataComponentTypes[3]\n{\n"
        "  DataComponent { Type Empty NumComponents 1 }\n"
        "  DataComponent { Type Int NumComponents 1 }\n"
        "  DataComponent { Type RealArray NumComponents 1 }\n"
        "}\n"
        "DataRegistry[4]\n{\n"
        "  DataLink { Name \"Actor\" Type Empty Data }\n"
        "  DataLink { Name \"Flags\" Type Int Parent \"Actor\\\" Floor 0 Ceiling 7 Data 99 WillAcceptParentData }\n"
        "  DataLink { Name \"Range\" Type RealArray NumElements 3 Parent \"Actor\\\" Floor -1.0 Ceiling 1.0 Data -5.0 0.5 5.0 }\n"
        // More than declared: another block comes on the chain, of the same count.
        "  DataLink { Name \"More\" Type Int Parent \"Actor\\\" Data 12 }\n"
        "}\n";
    char space[16];
    AtDataRegistry base;
    Read(base, owner, base_text, space, nullptr);
    CHECK(std::strcmp(space, "Base") == 0);
    CHECK(base.m_loaded == 1);
    CHECK(base.m_componentTypes == 3);
    CHECK(base.m_pools[AtDataRegistry::kInt].m_count == 1);
    CHECK(base.m_pools[AtDataRegistry::kInt].m_next != nullptr);
    CHECK(base.m_group != nullptr && std::strcmp(base.m_group->m_name, "Base") == 0);

    AtDataLink* actor = base.FindChild("Actor\\", nullptr);
    CHECK(actor != nullptr && std::strcmp(actor->m_name, "Actor") == 0);
    AtDataLink* flags = base.FindChild("Flags", actor);
    CHECK(flags != nullptr);
    AtDataInt* value = static_cast<AtDataInt*>(flags->m_value);
    CHECK(value->m_value == 7);  // The ceiling.
    CHECK(value->m_descriptor->m_flags & AtDataDescriptor::kAcceptsParentData);
    CHECK(flags->Registry() == &base);
    CHECK(base.m_root.Registry() == &base);
    AtDataRealArray* range = static_cast<AtDataRealArray*>(base.FindChild("Range", actor)->m_value);
    CHECK(range->m_descriptor->m_count == 3);
    CHECK(range->m_data[0] == -1.0f && range->m_data[1] == 0.5f && range->m_data[2] == 1.0f);

    char child_text[] =
        "DataComponentTypes[4]\n{\n"
        "  DataComponent { Type Boolean NumComponents 1 }\n"
        "  DataComponent { Type ShortName NumComponents 1 }\n"
        "  DataComponent { Type String NumComponents 2 }\n"
        "  DataComponent { Type StringArray NumComponents 1 }\n"
        "}\n"
        "DataRegistry[6]\n{\n"
        "  DataLink { Name \"Alive\" Type Boolean Parent \"Actor\\\" Data 4 }\n"
        "  DataLink { Name \"Short\" Type ShortName Parent \"Actor\\\" Data \"Name\" }\n"
        "  DataLink { Name \"Text\" Type String Parent \"Nowhere\\\" Data \"Plain\" }\n"
        "  DataLink { Name \"Wide\" Type String Data \"Ignored\" \"unicode\" }\n"
        "  DataLink { Name \"List\" Type StringArray NumElements 2 Ceiling 8 Data \"One\" \"Two\" }\n"
        "  DataLink { Name \"Under\" Type Boolean Parent \"text\" Data 0 }\n"
        "}\n";
    AtDataRegistry child;
    Read(child, owner, child_text, nullptr, &base);
    CHECK(child.m_parent == &base);
    CHECK(child.m_root.m_parent == &base.m_root);
    CHECK(child.m_group == &owner.m_plain);

    CHECK(child.FindChild("Alive", nullptr) == nullptr);  // It hangs in the parent's tree, not under this registry's own root.
    AtDataLink* alive = base.FindChild("Alive", actor);
    CHECK(alive != nullptr && static_cast<AtDataBoolean*>(alive->m_value)->m_value == 1);
    CHECK(alive->m_parent == actor);
    CHECK(std::strcmp(static_cast<AtDataShortName*>(base.FindChild("Short", actor)->m_value)->m_value, "Name") == 0);

    AtDataLink* text = child.m_pools[AtDataRegistry::kString].FindByName("TEXT", 0);
    CHECK(text != nullptr && text->m_parent == &child.m_root);
    CHECK(child.m_pools[AtDataRegistry::kString].FindByName("TEXT", 1) == nullptr);
    CHECK(child.m_pools[AtDataRegistry::kBoolean].m_next != nullptr);
    AtDataLink* under = child.m_pools[AtDataRegistry::kBoolean].m_next->FindByName("Under", 1);
    CHECK(under != nullptr && under->m_parent == text);
    AtDataString* plain = static_cast<AtDataString*>(text->m_value);
    CHECK(plain->m_unicode == 0 && plain->m_narrow.m_end == 5 && std::strcmp(plain->m_narrow.m_data, "Plain") == 0);
    // The tokenizer holds one string, so the word Unicode is what the value then says.
    AtDataString* wide = static_cast<AtDataString*>(child.m_pools[AtDataRegistry::kString].FindByName("Wide", 1)->m_value);
    CHECK(wide->m_unicode == 1 && wide->m_wide.m_end == 7 && wide->m_wide.m_data[0] == 'u');
    AtDataStringArray* list = static_cast<AtDataStringArray*>(child.m_pools[AtDataRegistry::kStringArray].FindByName("List", 1)->m_value);
    CHECK(list->m_descriptor->m_count == 2);
    CHECK(list->m_texts[1].m_capacity == 8 && list->m_texts[1].m_end == 3 && list->m_chars[1][2] == 'o');

    // Every value class, for its size and the walk along a block.
    char all_text[] =
        "DataComponentTypes[12]\n{\n"
        "  DataComponent { Type Empty NumComponents 2 }\n  DataComponent { Type Boolean NumComponents 2 }\n  DataComponent { Type Int NumComponents 2 }\n"
        "  DataComponent { Type Real NumComponents 2 }\n  DataComponent { Type ShortName NumComponents 2 }\n  DataComponent { Type String NumComponents 2 }\n"
        "  DataComponent { Type BooleanArray NumComponents 2 }\n  DataComponent { Type ByteArray NumComponents 2 }\n  DataComponent { Type IntArray NumComponents 2 }\n"
        "  DataComponent { Type RealArray NumComponents 2 }\n  DataComponent { Type ShortNameArray NumComponents 2 }\n  DataComponent { Type StringArray NumComponents 2 }\n"
        "}\n"
        "DataRegistry[2]\n{\n"
        "  DataLink { Name \"Bytes\" Type ByteArray NumElements 2 Data 300 7 }\n"
        "  DataLink { Name \"Names\" Type ShortNameArray NumElements 2 Data \"A\" \"EightChr\" }\n"
        "}\n";
    AtDataRegistry all;
    Read(all, owner, all_text, nullptr, nullptr);
    for (int t = 0; t < 12; t++) {
        AtDataComponent* first = all.m_pools[t].ElementAt(0);
        AtDataComponent* second = all.m_pools[t].ElementAt(1);
        CHECK(first != nullptr && second != nullptr && first != second);
        CHECK(all.m_pools[t].ElementAt(2) == nullptr);
        second->Reset();
    }
    CHECK(static_cast<AtDataByteArray*>(all.FindChild("Bytes", nullptr)->m_value)->m_data[0] == 44);
    CHECK(std::strncmp(static_cast<AtDataShortNameArray*>(all.FindChild("Names", nullptr)->m_value)->m_data[1], "EightChr", 8) == 0);

    AtDataIntArray ints;
    AtDataDescriptor bounds;
    bounds.SetType(AtDataRegistry::kIntArray, 2);
    ints.SetDescriptor(&bounds);
    CHECK(ints.Resize(2) == 0);
    ints.m_data[0] = 5;
    ints.m_data[1] = 6;
    CHECK(ints.Resize(3) == 0);
    CHECK(bounds.m_count == 3 && ints.m_data[1] == 6);
    bounds.SetRealBounds(0.0f, 1.0f);
    CHECK(bounds.SetRealBounds(0.0f, 1.0f) == -1);
    bounds.SetFormat("%d");
    CHECK(std::strcmp(bounds.m_format, "%d") == 0);

    all.Clear();
    CHECK(all.m_loaded == 0 && all.m_pools[2].m_values == nullptr);
    child.Clear();
    CHECK(child.m_owner == nullptr && child.m_group == nullptr);
    base.Clear();
    CHECK(owner.m_spaces.m_free != nullptr);
    CHECK(owner.m_spaces.Find("Base") == nullptr);

    if (g_fail) {
        std::printf("%d failed\n", g_fail);
        return 1;
    }
    std::printf("test_adr passed\n");
    return 0;
}
