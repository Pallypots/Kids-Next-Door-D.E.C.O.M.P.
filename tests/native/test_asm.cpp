// Host test of modifiers and space graphs through the real text tokenizer, on any pointer size. Not evidence of a byte match. No game data: the text is written here, in the shape real files have.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "at/AtHeap.h"
#include "at/AtIoTokenizer.h"
#include "at/AtModifier.h"
#include "at/AtSpaceGraph.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

class Device : public AtSceneDevice {
public:
    void Slot0() override {} void Slot1() override {} void Slot2() override {} void Slot3() override {} void Slot4() override {} void Slot5() override {} void Slot6() override {} void Slot7() override {} void Slot8() override {}
    void Present(int) override {} void Slot10() override {} void Slot11() override {} void Slot12() override {}
    AtMatrixDevice* MatrixDevice() override { return nullptr; }
};

class Node : public AtSceneNode {
public:
    void FinishBody() override { m_flags &= ~(unsigned int)kMoved; }
};

const char kModifiers[] =
    "Modifier[4]\n"
    "LightModifier[2]\n{\n"
    " LightModifier \"Lamp\" { Light \"Bulb\" \"\" Point Position 1.0 2.0 3.0 Priority 2 }\n"
    " LightModifier \"Sun\" { Light \"Bulb\" \"\" Directional Direction 0.0 3.0 4.0 }\n"
    "}\n"
    "AmbientModifier[1]\n{\n AmbientModifier \"Glow\" { Color 0.25 0.5 0.75 1.0 }\n}\n"
    "GroupModifier[1]\n{\n GroupModifier \"All\" { Modifier[3]\n {\n \"Lamp\" \"\"\n \"Sun\" \"\"\n \"Glow\" \"\"\n }\n }\n}\n"
    "BlockModifier[1]\n{\n BlockModifier \"Dark\" { Block LightModifier }\n}\n"
    "ContextConstantModifier[1]\n{\n ContextConstantModifier \"Ctx\" { VertexProcessConstant[1]\n {\n 1.0 2.0 3.0 4.0 5\n }\n }\n}\n"
    "PerspectiveModifier[1]\n{\n PerspectiveModifier \"Wide\" { FieldOfViewY 2.0 }\n}\n";

const char kGraph[] =
    "SpaceGraph\n{\n"
    " BinarySpace[1]\n XBinarySpace[1]\n LeafSpace[3]\n NodeLink[4]\n NodeIncidence[8]\n"
    " Root BinarySpace 0\n"
    " BinarySpace[1]\n {\n  BinarySpace { Partition 0.0 1.0 0.0 0.0 PosChild LeafSpace 0 NegChild XBinarySpace 0 Modifier \"All\" \"\" }\n }\n"
    " XBinarySpace[1]\n {\n  XBinarySpace { Partition 10.0 PosChild LeafSpace 1 NegChild LeafSpace 2 Draw \"Tree\" \"\" }\n }\n"
    " LeafSpace[3]\n {\n"
    "  LeafSpace { BoundingBox 0.0 1.0 0.0 9.0 0.0 1.0 Modifier \"Dark\" \"\" Cell \"Tab\" \"\" \"One\" }\n"
    "  LeafSpace { BoundingBox 0.0 1.0 0.0 1.0 0.0 9.0 LineSegIntersect \"Tree\" \"\" }\n"
    "  LeafSpace { }\n"
    " }\n"
    "}\n";

void SetText(AtIoTextTokenizer& tokenizer, Memory& stream, const char* text) {
    stream.Free();
    stream.Allocate((unsigned int)std::strlen(text) + 1);
    std::memcpy(stream.m_base, text, std::strlen(text));
    stream.m_limit = stream.m_base + std::strlen(text);
    stream.m_cur = stream.m_base;
    tokenizer.SetStream(&stream);
}

}  // namespace

int main() {
    static AtHeap heap;
    heap.m_alignShift = 2;
    heap.m_allocShift = 2;
    AtModifierDatabase::SetHeap(&heap);
    AtModifier::SetHeap(&heap);
    AtGroupModifier::SetHeap(&heap);
    AtSpaceGraph::SetHeap(&heap);
    AtModifierStack::SetHeap(&heap);
    AtModifierState::SetHeap(&heap);
    AtIoTokenFile::s_heap = &heap;
    AtIoStream::s_heap = &heap;
    GmNameMap::s_heap = &heap;
    GmNameSpaceDb::s_heap = &heap;

    Device device;
    AtNameTableOwner* owner = new AtNameTableOwner;
    owner->m_device = &device;
    owner->m_modifierStack.m_depth = 0;
    owner->m_modifierStack.m_used = 0;
    owner->m_modifierStack.m_states = nullptr;
    owner->m_stackDepth = 8;
    owner->m_stackSlots = 6;

    int bulb = 7;
    GmNameMap lights;
    lights.Init(4);
    lights.Insert("Bulb", &bulb);
    device.m_lights.Add(&lights);

    static Node tree;
    tree.m_class = 2;
    tree.m_worldSphere.center.x = 1.0f;
    tree.m_worldSphere.center.y = -1.0f;
    tree.m_worldSphere.center.z = 0.0f;
    tree.m_worldSphere.radius = 3.0f;
    GmNameMap nodes;
    nodes.Init(4);
    nodes.Insert("Tree", &tree);
    owner->m_nodes.Add(&nodes);

    static AtSpaceCellTable* table = new (std::malloc(sizeof(AtSpaceCellTable))) AtSpaceCellTable;
    int cell = 9;
    table->m_cells.Init(4);
    table->m_cells.Insert("One", &cell);
    GmNameMap tables;
    tables.Init(4);
    tables.Insert("Tab", table);
    owner->m_cellTables.Add(&tables);

    Memory stream;
    AtIoTextTokenizer tokenizer;

    AtModifierDatabase modifiers;
    SetText(tokenizer, stream, kModifiers);
    modifiers.Read(owner, &tokenizer);
    CHECK(modifiers.m_lightCount == 2);
    CHECK(modifiers.m_ambientCount == 1);
    CHECK(modifiers.m_groupCount == 1);
    CHECK(modifiers.m_blockCount == 1);
    CHECK(modifiers.m_contextCount == 1);
    CHECK(modifiers.m_perspectiveCount == 1);
    CHECK(modifiers.m_lights[0].m_light == &bulb);
    CHECK(modifiers.m_lights[0].m_type == AtLightModifier::kPoint);
    CHECK(modifiers.m_lights[0].m_position.z == 3.0f);
    CHECK(modifiers.m_lights[0].m_priority == 2);
    CHECK(modifiers.m_lights[1].m_direction.y > 0.59f && modifiers.m_lights[1].m_direction.y < 0.61f);
    CHECK(modifiers.m_ambients[0].m_color.y == 0.5f);
    CHECK(modifiers.m_groups[0].m_count == 3);
    CHECK(modifiers.m_groups[0].MemberAt(2) == &modifiers.m_ambients[0]);
    CHECK(modifiers.m_blocks[0].m_blocks == kAtModifierLight);
    CHECK(modifiers.m_contexts[0].m_vertexCount == 1);
    CHECK(modifiers.m_contexts[0].m_vertex[0].m_index == 5);
    CHECK(modifiers.m_perspectives[0].m_fieldOfViewY == 2.0f);
    CHECK(modifiers.Find("Lamp") == &modifiers.m_lights[0]);
    CHECK(owner->m_modifiers.Find("Glow") == &modifiers.m_ambients[0]);
    CHECK(heap.m_allocShift == heap.m_alignShift);

    AtSpaceGraph graph;
    SetText(tokenizer, stream, kGraph);
    graph.Read(owner, &tokenizer);
    CHECK(graph.m_root == &graph.m_binary[0]);
    CHECK(graph.m_binary[0].m_posChild == &graph.m_leaves[0]);
    CHECK(graph.m_binary[0].m_negChild == &graph.m_x[0]);
    CHECK(graph.m_binary[0].m_modifier == &modifiers.m_groups[0]);
    CHECK(graph.m_x[0].m_distance == 10.0f);
    CHECK(graph.m_leaves[0].m_sortAxis == 1);
    CHECK(graph.m_leaves[1].m_sortAxis == 2);
    CHECK(graph.m_leaves[0].m_cell == &cell);
    CHECK(owner->m_modifierStack.m_states == nullptr);
    // Everything under the group has its lights, in order of priority, and its ambient; the first leaf's block empties the lights.
    const AtModifierState& blocked = graph.m_leaves[0].m_state;
    const AtModifierState& open = graph.m_leaves[1].m_state;
    CHECK(blocked.m_count == 6);
    CHECK(blocked.m_slots[kAtModifierLight].m_count == 0);
    CHECK(blocked.m_slots[kAtModifierAmbient].m_count == 1);
    CHECK(open.m_slots[kAtModifierLight].m_count == 2);
    CHECK(open.m_slots[kAtModifierLight].m_entries[0].m_modifier == &modifiers.m_lights[0]);
    CHECK(open.m_slots[kAtModifierLight].m_entries[1].m_modifier == &modifiers.m_lights[1]);
    // The draw went to both leaves under the X space, the line segment test to one.
    CHECK(graph.m_leaves[1].m_first[kAtLinkDraw] != nullptr);
    CHECK(graph.m_leaves[2].m_first[kAtLinkDraw] != nullptr);
    CHECK(graph.m_leaves[1].m_first[kAtLinkLineSegIntersect] != nullptr);
    CHECK(tree.m_links[kAtLinkDraw] != nullptr);
    CHECK(tree.m_links[kAtLinkDraw]->m_set == &graph);
    CHECK(AtSpaceGraph::s_itemsInUse == 3);

    // A moving node, placed through the tree: across the plane it has two items.
    static Node mover;
    mover.m_worldSphere.center.y = 0.5f;
    mover.m_worldSphere.radius = 2.0f;
    graph.AddAsKind0(&mover);
    AtSceneLink* link = mover.m_links[0];
    CHECK(link != nullptr);
    CHECK(link->m_items != nullptr && link->m_items->m_next != nullptr);
    CHECK(link->m_margin == 0.25f);
    CHECK(graph.m_leaves[0].m_first[0] != nullptr);
    CHECK(graph.m_leaves[1].m_first[0] != nullptr || graph.m_leaves[2].m_first[0] != nullptr);
    CHECK(AtSpaceGraph::s_itemsInUse == 5);
    graph.RemoveAsKind0(&mover);
    CHECK(mover.m_links[0] == nullptr);
    CHECK(graph.m_leaves[0].m_first[0] == nullptr);
    CHECK(AtSpaceGraph::s_itemsInUse == 3);

    // Plane words, by slot 13.
    float plane[4];
    graph.m_x[0].Plane(plane);
    CHECK(plane[0] == 1.0f && plane[3] == -10.0f);
    graph.m_binary[0].Plane(plane);
    CHECK(plane[1] == 1.0f);

    graph.Clear();
    CHECK(graph.m_links == nullptr);
    CHECK(graph.m_root == nullptr);
    CHECK(tree.m_links[kAtLinkDraw] == nullptr);

    // Read again over itself.
    SetText(tokenizer, stream, kModifiers);
    modifiers.Read(owner, &tokenizer);
    CHECK(modifiers.m_lightCount == 2);
    modifiers.Clear();
    CHECK(modifiers.m_lights == nullptr);

    if (g_fail) { std::printf("%d failures\n", g_fail); return 1; }
    std::printf("ok\n");
    return 0;
}
