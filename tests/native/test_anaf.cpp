// Host test of the controllers and of binding through the real text tokenizer, on any pointer size. Not evidence of a byte match.
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

class Phys : public AtInputPhysicalController {
public:
    Input inputs[2];
    Input axisInputs[2];
    Chain chains[2];
    Chain axisChains[2];
    Phys() {
        inputs[0].m_id = 0x30000000;
        inputs[1].m_id = 0x30000001;
        axisInputs[0].m_id = 0x40000000;
        axisInputs[1].m_id = 0x40000001;
        for (int i = 0; i < 2; i++) {
            chains[i].m_first = chains[i].m_last = nullptr;
            axisChains[i].m_first = axisChains[i].m_last = nullptr;
        }
        m_buttonInputs = inputs;
        m_axisInputs = axisInputs;
        m_buttonChains = chains;
        m_axisChains = axisChains;
        m_buttonInputCount = 2;
        m_axisInputCount = 2;
    }
    int Slot0() override { return 0; }
    int CopyStates() override { return 0; }
    int Update() override { return 0; }
    unsigned int ButtonCount() override { return 2; }
    unsigned int AxisCount() override { return 2; }
    unsigned int PovCount() override { return 0; }
    AtInputAxisHandler* AxisHandler(unsigned int) override { return nullptr; }
    void PhysicalSlot0() override {}
};

class Dev : public AtInputDevice {
public:
    Phys* one;
    explicit Dev(Phys* p) : one(p) { m_numberedCount = 1; }
    void Slot0() override {}
    void Slot1() override {}
    void Slot2() override {}
    void Slot3() override {}
    void Slot4() override {}
    void Slot5() override {}
    void Slot6() override {}
    void Slot7() override {}
    void Slot8() override {}
    void Slot9() override {}
    void Slot10() override {}
    void Slot11() override {}
    void Slot12() override {}
    void Slot13() override {}
    int Update(int) override { return 0; }
    void Slot15() override {}
    void Slot16() override {}
    void Slot17() override {}
    unsigned int PhysicalCount() override { return 1; }
    unsigned int ConnectedCount() override { return 0; }
    AtInputPhysicalController* First() override { return one; }
    AtInputPhysicalController* Second() override { return one; }
    AtInputPhysicalController* Numbered(unsigned int) override { return one; }
    AtInputPhysicalController* Physical(unsigned int) override { return one; }
};

}  // namespace

int main() {
    AtInputControllerDatabase::SetHeap(&g_heap);
    AtInputController::SetHeap(&g_heap);
    AtInputEventQueue::SetHeap(&g_heap);
    AtInputLinkList::SetHeap(&g_heap);
    AtInputRouter::SetHeap(&g_heap);
    AtInputMapper::SetHeap(&g_heap);
    GmNameMap::s_heap = &g_heap;
    GmNameSpaceDb::s_heap = &g_heap;

    char text[] =
        "AbstractController[2]\n"
        "{\n"
        "  AbstractController \"pad\"  // a comment\n"
        "  {\n"
        "    Buttons 4\n"
        "    Axes 2\n"
        "    POVs 1\n"
        "    EventHandlers 2\n"
        "    DeadZone 15.0\n"
        "    Saturation 95.0\n"
        "  }\n"
        "  AbstractController \"plain\"\n"
        "  {\n"
        "    Buttons 1\n"
        "  }\n"
        "}\n";
    Memory stream;
    stream.m_base = text;
    stream.m_cur = text;
    stream.m_limit = text + sizeof text - 1;
    stream.m_size = sizeof text - 1;
    AtIoTextTokenizer tokenizer;
    tokenizer.SetStream(&stream);

    AtInputOwner* owner = new AtInputOwner;
    owner->m_unknown00 = 0;
    Phys phys;
    Dev dev(&phys);
    owner->m_device = &dev;
    owner->m_controllerSpaces.Init(2);
    owner->m_mapperSpaces.Init(2);
    {
        AtInputControllerDatabase db;
        db.Read(owner, &tokenizer);
        CHECK(db.m_size != 0);
        AtAbstractController* pad = &db.m_items[0];
        CHECK(pad->m_buttons == 4);
        CHECK(pad->m_axes == 2);
        CHECK(pad->m_povs == 1);
        CHECK(pad->m_buttonInputCount == 7);
        CHECK(pad->m_axisInputCount == 3);
        CHECK(pad->m_receivers.m_capacity == 2);
        CHECK(pad->m_started == 1);
        CHECK(pad->m_acquired == 1);
        CHECK(pad->m_buttonInputs[3].m_id == 0x30000003);
        CHECK(pad->m_axisInputs[1].m_id == 0x40000001);
        CHECK(pad->m_povInputs[0].m_id == 0x50000000);
        CHECK(pad->m_povValues[0] == 6.2831855f);
        CHECK(pad->m_povDirections[0] == 8);
        CHECK(pad->m_handlers[1].m_id == 0x40000001);
        CHECK(pad->m_handlers[1].m_index == 5);
        CHECK(pad->m_handlers[1].m_saturation == 95.0f);
        CHECK(pad->m_handlers[1].m_flags == 2);
        CHECK(pad->m_queue.m_freeFirst != nullptr);
        CHECK(pad->m_queue.m_mask == 0xFF);
        CHECK(db.m_items[1].m_buttons == 1);
        CHECK(db.m_items[1].m_handlers == nullptr);
        CHECK(db.m_items[1].m_axisChains == nullptr);
        CHECK(AtAbstractController::s_povAngles[4] == 3.1415927f);

        // The filter: a dead zone of 15 and a saturation of 95 over the handler's own range.
        AtInputAxisHandler& axis = pad->m_handlers[0];
        axis.m_raw = 0;
        CHECK(axis.Filter(0.0f) == 0.0f);
        CHECK(axis.Filter(1400.0f) == 0.0f);
        CHECK(axis.Filter(-1400.0f) == 0.0f);
        CHECK(axis.Filter(10000.0f) == 1.0f);
        CHECK(axis.Filter(-10000.0f) == -1.0f);
        const float partway = axis.Filter(5000.0f);
        CHECK(partway > 0.0f && partway < 1.0f);
        axis.m_flags |= 1;
        axis.Apply(10000.0f);
        CHECK(axis.m_value == 10000.0f);
        CHECK(axis.m_filtered == -1.0f);
        axis.m_flags &= ~1u;

        // A mapper bound to the controller through a physical one, real links both ways, and undone.
        AtInputMapper mapper;
        AtInputMapper::Sizes sizes = {&dev, 2, 2};
        mapper.Allocate(sizes);
        mapper.m_router.Allocate(&dev, 4, 2);
        const int physical[] = {0x30000000, 0x30000001};
        for (int i = 0; i < 2; i++) {
            AtInputButtonMap& b = mapper.m_buttonMaps[mapper.m_buttonMapsUsed++];
            b.m_physical = physical[i];
            b.m_abstract = 0x30000002 + i;
            b.m_state = 1;
        }
        AtInputAxisMap& a = mapper.m_axisMaps[mapper.m_axisMapsUsed++];
        a.m_physical = 0x40000000;
        a.m_abstract = 0x40000001;
        mapper.Bind(&phys, pad);
        CHECK(mapper.m_router.m_boundButtons.m_count == 2);
        CHECK(mapper.m_router.m_boundAxes.m_count == 1);
        CHECK(mapper.m_router.m_passOnTo == &pad->m_queue);
        CHECK(phys.chains[1].m_first != nullptr);
        CHECK(phys.chains[1].m_first->m_abstract == 0x30000003);
        CHECK(phys.chains[1].m_first->m_owner == &mapper.m_router);
        CHECK(phys.axisChains[0].m_first->m_abstract == 0x40000001);
        // A second bind of the same undoes the first and comes back the same.
        mapper.Bind(&phys, pad);
        CHECK(mapper.m_router.m_boundButtons.m_count == 2);
        mapper.m_router.UnbindAll();
        CHECK(mapper.m_router.m_boundButtons.m_count == 0);
        CHECK(phys.chains[1].m_first == nullptr);
        CHECK(phys.axisChains[0].m_first == nullptr);

        // Through the device: one mapping, three ways to the same controller, one link.
        mapper.Bind(nullptr, pad);
        CHECK(mapper.m_router.m_boundButtons.m_count == 2);
        CHECK(mapper.m_router.m_boundAxes.m_count == 1);
        mapper.m_router.UnbindAll();

        // What the router passes on is kept, and flushed by kind.
        int event[7] = {1, 0, 3, 4, 5, 6, 7};
        mapper.Bind(&phys, pad);
        mapper.m_router.Slot0(event);
        mapper.m_router.Slot2(event);
        CHECK(pad->m_queue.m_count == 2);
        pad->m_queue.Flush(1);
        CHECK(pad->m_queue.m_count == 0);
        mapper.m_router.UnbindAll();

        // Bound to the controller itself, its release sends the router's links home.
        mapper.Bind(pad, pad);
        CHECK(mapper.m_router.m_boundButtons.m_count == 2);  // Its own ids start at 0x30000000 too.
        CHECK(pad->CopyStates() == 0);
        pad->Unacquire();
        CHECK(pad->CopyStates() == 5);
        CHECK(pad->Release() == 0);
        CHECK(pad->m_started == 0);
        CHECK(pad->m_handlers == nullptr);
        CHECK(mapper.m_router.m_boundButtons.m_count == 0);
        CHECK(mapper.m_router.m_boundAxes.m_count == 0);
        AtInputControllerSetting setting = {};
        setting.m_buttons = 2;
        setting.m_axes = 1;
        setting.m_eventHandlers = 1;
        setting.m_deadZone = 10.0f;
        setting.m_saturation = 90.0f;
        CHECK(pad->Init(setting) == 0);
        CHECK(pad->m_buttonInputCount == 3);
        CHECK(pad->m_handlers[0].m_deadZone == 10.0f);
        CHECK(pad->AxisHandler(1) == nullptr);

        db.Clear();
        CHECK(db.m_items == nullptr);
        GmNameSpaceDb::Remove(&db);
    }
    delete owner;
    stream.m_base = nullptr;
    stream.m_cur = nullptr;
    stream.m_limit = nullptr;
    tokenizer.SetStream(nullptr);

    if (g_fail == 0) std::printf("test_anaf: passed\n");
    return g_fail != 0;
}
