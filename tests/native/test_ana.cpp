// Host test of the input mappers through the real text tokenizer, on any pointer size. Not evidence of a byte match.
#include <cstdio>
#include <cstring>

#include "at/AtInputMapper.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

int g_heap;
int g_device;

}  // namespace

int main() {
    AtInputMapper::SetHeap(&g_heap);
    AtInputMapperDatabase::SetHeap(&g_heap);
    AtInputLinkList::SetHeap(&g_heap);
    AtInputRouter::SetHeap(&g_heap);
    GmNameMap::s_heap = &g_heap;
    GmNameSpaceDb::s_heap = &g_heap;

    char text[] =
        "NameSpace \"Pad\"\n"
        "AbstractMapper[2]\n"
        "{\n"
        "  AbstractMapper \"First\"\n"
        "  {\n"
        "    ButtonMap[2]\n"
        "    {\n"
        "      ButtonMap { PhysicalID JoystickButton4 AbstractID JoystickButton0 }\n"
        "      ButtonMap { PhysicalID JoystickButton2 AbstractID AxisU AbstractAxisState AxisNegative PhysicalButtonState POVLeft }\n"
        "    }\n"
        "    AxisMap[1]\n"
        "    {\n"
        "      AxisMap { PhysicalID AxisX AbstractID JoystickButton2 PhysicalPOVAxis AxisY }\n"
        "    }\n"
        "  }\n"
        "  AbstractMapper \"Second\"\n"
        "  {\n"
        "    AbstractMapper \"First\" \"Pad\"\n"
        "  }\n"
        "}\n";

    Memory stream;
    stream.m_base = text;
    stream.m_cur = text;
    stream.m_limit = text + sizeof(text) - 1;
    stream.m_size = sizeof(text) - 1;
    AtIoTextTokenizer tokenizer;
    tokenizer.SetStream(&stream);

    {
        AtInputOwner* owner = new AtInputOwner;
        owner->m_device = &g_device;
        owner->m_mapperSpaces.Init(4);
        AtInputMapperDatabase db;
        db.Read(owner, &tokenizer);

        AtInputMapper* first = (AtInputMapper*)db.Find("First");
        AtInputMapper* second = (AtInputMapper*)db.Find("Second");
        CHECK(first == &db.m_items[0]);
        CHECK(second == &db.m_items[1]);
        CHECK(owner->m_mapperSpaces.Find("Pad", "First") == first);

        CHECK(first->m_device == &g_device);
        CHECK(first->m_buttonMapCapacity == 4);
        CHECK(first->m_buttonMapsUsed == 4);
        CHECK(first->m_axisMapCapacity == 1);
        CHECK(first->m_axisMapsUsed == 1);
        CHECK(first->m_buttonMaps[0].m_physical == 0x30000004);
        CHECK(first->m_buttonMaps[0].m_abstract == 0x30000000);
        CHECK(first->m_buttonMaps[0].m_state == 1);
        CHECK(first->m_buttonMaps[1].m_physical == 0x30000004);
        CHECK(first->m_buttonMaps[1].m_state == 0);
        CHECK(first->m_buttonMaps[2].m_physical == 0x30000002);
        CHECK(first->m_buttonMaps[2].m_abstract == (0x40000003 | 0x10000));
        CHECK(first->m_buttonMaps[2].m_state == 0x1006);
        CHECK(first->m_axisMaps[0].m_physical == 0x40000000);
        CHECK(first->m_axisMaps[0].m_abstract == (0x30000002 | 0x10000));
        CHECK(first->m_router.m_device == &g_device);
        CHECK(first->m_router.m_linkCount == 5);
        CHECK(first->m_router.m_boundButtons.m_capacity == 4);
        CHECK(first->m_router.m_boundAxes.m_capacity == 1);

        // The copy's capacity is doubled again; what it holds is the same.
        CHECK(second->m_buttonMapCapacity == 8);
        CHECK(second->m_buttonMapsUsed == 4);
        CHECK(second->m_buttonMaps[2].m_state == 0x1006);
        CHECK(second->m_axisMapCapacity == 1);
        CHECK(second->m_axisMaps[0].m_abstract == first->m_axisMaps[0].m_abstract);

        // Five links on the free list, last made first. Unbinding one sends it back.
        AtInputRouter& router = first->m_router;
        CHECK(router.m_freeFirst == &router.m_axisLinks[0]);
        CHECK(router.m_freeLast == &router.m_buttonLinks[0]);

        db.Clear();
        CHECK(db.m_items == nullptr);
        CHECK(db.m_owner == nullptr);
        GmNameSpaceDb::Remove(&db);
        delete owner;
    }
    stream.m_base = nullptr;
    stream.m_cur = nullptr;
    stream.m_limit = nullptr;
    tokenizer.SetStream(nullptr);

    if (g_fail == 0) std::printf("test_ana: passed\n");
    return g_fail != 0;
}
