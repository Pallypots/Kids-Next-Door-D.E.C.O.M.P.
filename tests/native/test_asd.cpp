// Host test of cell graphs, flat image and text lists, and the scene list's own state, through the real text tokenizer, on any pointer size. Not evidence of a byte match. No game data: the text is written here, in the shape real
// files have. The .ASD loader itself opens files, and is run against the original only, in tests/oracle/oracle_asd.cpp.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "at/AtHeap.h"
#include "at/AtIoFileSystem.h"
#include "at/AtIoTokenizer.h"
#include "at/AtSceneDatabaseLoader.h"

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
    void Slot9() override {} void Slot10() override {} void Slot11() override {} void Slot12() override {} void Slot13() override {} void Slot14() override {} void Slot15() override {} void Slot16() override {}
    AtMatrixDevice* MatrixDevice() override { return nullptr; }
    void DrawScale(float* out) override { out[0] = 2.0f; out[1] = 0.5f; }
};

float g_positions[2][64 * 4];
int g_locks = 0;
int g_unlocks = 0;

class Array : public AtVertexArray {
public:
    int which = 0;
    float* PositionOf(float*) override { return nullptr; }
    void Slot1() override {} void Destroy(unsigned int) override {} void Slot3() override {} void Slot4() override {} void Slot5() override {}
    void Slot6() override { g_unlocks++; }
    void Slot7() override {} void Slot8() override {} void Slot9() override {} void Slot10() override {} void Slot11() override {} void Slot12() override {} void Slot13() override {} void Slot14() override {}
    void Slot15() override {} void Finish() override {} void Slot17() override {} void Slot18() override {} void Slot19() override {} void Slot20() override {}
    void Read(AtMaterialOwner*, AtIoTokenizer*, unsigned int) override {}
    void Lock(AtVertexLock* out, int) override { g_locks++; out->m_data = g_positions[which]; out->m_stride = 16; }
};

Array g_arrays[2];

class Mesh : public AtMesh {
public:
    void Slot0() override {} void Slot1() override {}
    AtVertexArray* OwnVertexArrayAt(unsigned int) override { return nullptr; }
    AtIndexArray* OwnIndexArrayAt(unsigned int) override { return nullptr; }
    void Slot4() override {} void Slot5() override {} void Slot6() override {} void Slot7() override {} void Slot8() override {} void Slot9() override {} void Slot10() override {} void Slot11() override {}
    void Read(AtMaterialOwner*, AtMeshDatabase*, AtIoTokenizer*, const char*) override {}
    AtVertexArray* VertexArrayAt(unsigned int index) override { return &g_arrays[index]; }
};

const char kCells[] =
    "NameSpace \"Lvl\"\n"
    "CellGraph[1]\n{\n"
    " CellGraph \"Rooms\"\n {\n"
    "  Mesh \"Walls\" \"\"\n"
    "  Portal[3]\n  {\n"
    "   Portal \"Door\" { OppositeCell \"Hall\" Vertices 0 0 4 ClosedByViewerDist 3.0 }\n"
    "   Portal \"Back\" { OppositeCell \"Room\" Vertices 1 4 3 Closed }\n"
    "   Portal \"Out\" { Vertices 0 8 4 }\n"
    "  }\n"
    "  Cell[2]\n  {\n"
    "   Cell \"Room\" { Portals 0 1 }\n"
    "   Cell \"Hall\" { Portals 1 2 }\n"
    "  }\n"
    "  ExteriorPortals 2 1\n"
    " }\n"
    "}\n";

const char kImages[] =
    "ImageDraw[2]\n{\n"
    " ImageDraw \"Logo\" { Image \"Pic\" \"\" Position 10.0 20.0 Scale 2.0 3.0 CenterHorizontally ClipRectangle 1.0 2.0 3.0 4.0 NormalizedCoordinates DrawArea 5.0 6.0 }\n"
    " ImageDraw \"Bar\" { ColorValueRectangle 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 CenterVertically }\n"
    "}\n";

const char kTexts[] =
    "NameSpace \"Ui\"\n"
    "TextDraw[1]\n{\n"
    " TextDraw \"Title\" { Font \"Big\" \"Ui\" String \"Hello\" Position 10.0 20.0 WrapWidth 300.0 Kerning 1.5 }\n"
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
    AtCellGraph::SetHeap(&heap);
    AtCellGraphDatabase::SetHeap(&heap);
    AtImageDrawDatabase::SetHeap(&heap);
    AtTextDrawDatabase::SetHeap(&heap);
    AtSceneDatabaseLoader::SetHeap(&heap);
    AtIoTokenFile::s_heap = &heap;
    AtIoStream::s_heap = &heap;
    GmNameMap::s_heap = &heap;
    GmNameSpaceDb::s_heap = &heap;

    for (int a = 0; a < 2; a++) {
        g_arrays[a].which = a;
        for (int i = 0; i < 64; i++) {
            g_positions[a][i * 4 + 0] = (float)(i % 2) + a;
            g_positions[a][i * 4 + 1] = (float)((i / 2) % 2);
            g_positions[a][i * 4 + 2] = 0.0f;
        }
    }

    Device device;
    static Mesh mesh;
    mesh.m_ownVertexArrays = 2;
    GmNameMap meshes;
    meshes.Init(4);
    meshes.Insert("Walls", &mesh);
    device.m_meshes.Add(&meshes);
    int picture = 1, font = 2, hello = 3;
    GmNameMap images, fonts, strings;
    images.Init(4);
    images.Insert("Pic", &picture);
    device.m_images.Add(&images);
    fonts.Init(4);
    fonts.Insert("Big", &font);
    device.m_fontSpaces.Init(4);
    device.m_fontSpaces.Add("Ui", &fonts);
    static AtIoContext context;
    strings.Init(4);
    strings.Insert("Hello", &hello);
    context.m_defaultGroup.Add(&strings);

    AtNameTableOwner* owner = new AtNameTableOwner;
    owner->m_device = &device;
    owner->m_loader = (AtAudioBankLoader*)&context;
    owner->m_cellTableSpaces.Init(4);
    owner->m_firstSpaces.Init(4);
    owner->m_secondSpaces.Init(4);

    static AtIoTextTokenizer tokenizer;
    static Memory stream;

    // A cell graph: portals, cells, their links, centres and normals.
    AtCellGraphDatabase cells;
    SetText(tokenizer, stream, kCells);
    cells.Read(owner, &tokenizer);
    CHECK(cells.m_count == 1);
    AtCellGraph* graph = (AtCellGraph*)owner->m_cellTableSpaces.Find("Lvl", "Rooms");
    CHECK(graph == cells.m_graphs);
    CHECK(owner->FindCellTable("Rooms", "Lvl") == graph);
    CHECK(graph->m_mesh == &mesh);
    CHECK(graph->m_portalCount == 3);
    CHECK(graph->m_cellCount == 2);
    AtCell* room = (AtCell*)graph->m_cells.Find("Room");
    AtCell* hall = (AtCell*)graph->m_cells.Find("Hall");
    CHECK(room == &graph->m_cellArray[0] && hall == &graph->m_cellArray[1]);
    CHECK(room->m_portals == &graph->m_portals[0] && room->m_portalCount == 1);
    CHECK(hall->m_portals == &graph->m_portals[1] && hall->m_portalCount == 2);
    CHECK(graph->m_exteriorPortals == &graph->m_portals[2] && graph->m_exteriorPortalCount == 1);
    AtCellPortal* door = (AtCellPortal*)graph->m_portalNames.Find("Door");
    CHECK(door == &graph->m_portals[0]);
    CHECK(door->m_opposite == hall && door->m_oppositeGraph == graph);
    CHECK(door->m_flags == AtCellPortal::kClosedByViewerDist && door->m_closedDistanceSquared == 9.0f);
    CHECK(door->m_vertexCount == 4 && door->m_stride == 16);
    CHECK(door->m_center[0] == 0.5f && door->m_center[1] == 0.5f && door->m_center[2] == 0.0f);
    CHECK(door->m_normal[0] == 0.0f && door->m_normal[1] == 0.0f && door->m_normal[2] == -1.0f);
    AtCellPortal* back = &graph->m_portals[1];
    CHECK(back->m_opposite == room && back->m_flags == (AtCellPortal::kClosedBit1 | AtCellPortal::kClosedBit3));
    CHECK(back->m_vertices == (const char*)&g_positions[1][4 * 4] && back->m_vertexArray == 1 && back->m_firstVertex == 4);
    CHECK(graph->m_portals[2].m_opposite == nullptr && graph->m_portals[2].m_oppositeGraph == nullptr);
    CHECK(g_locks == 3 && g_unlocks == 3);
    CHECK(AtCellGraph::s_cellNames == nullptr);

    // Flat images: words, scales, colours, and the owner's first table.
    AtImageDrawDatabase imageList;
    SetText(tokenizer, stream, kImages);
    imageList.Read(owner, &tokenizer);
    CHECK(imageList.m_count == 2);
    CHECK(owner->FindInFirst("Logo", nullptr) == &imageList.m_draws[0]);
    AtImageDraw& logo = imageList.m_draws[0];
    CHECK(logo.m_kind == AtDrawItem::kImage && logo.m_image == &picture);
    CHECK(logo.m_x == 10.0f && logo.m_y == 20.0f);
    CHECK(logo.m_scaleX == 4.0f && logo.m_scaleY == 1.5f);
    CHECK(logo.m_clip[0] == 2.0f && logo.m_clip[1] == 1.0f && logo.m_clip[2] == 6.0f && logo.m_clip[3] == 2.0f);
    CHECK(logo.m_flags == (AtDrawItem::kCenterHorizontally | AtDrawItem::kNormalizedCoordinates));
    CHECK(logo.m_drawWidth == 5.0f && logo.m_drawHeight == 6.0f);
    AtImageDraw& bar = imageList.m_draws[1];
    CHECK(bar.m_flags == (AtDrawItem::kHasColors | AtDrawItem::kCenterVertically));
    CHECK(bar.m_colors.m_colors[3].a == 16 && bar.m_colors.m_colors[0].r == 1);
    CHECK(logo.m_colors.m_colors[2].g == 255);

    // Flat text: a font in a name space, a string from the context.
    AtTextDrawDatabase textList;
    SetText(tokenizer, stream, kTexts);
    textList.Read(owner, &tokenizer);
    CHECK(textList.m_count == 1);
    AtTextDraw* title = (AtTextDraw*)owner->m_secondSpaces.Find("Ui", "Title");
    CHECK(title == &textList.m_draws[0]);
    CHECK(title->m_kind == AtDrawItem::kText && title->m_font == &font && title->m_string == &hello);
    CHECK(title->m_x == 20.0f && title->m_y == 10.0f);
    CHECK(title->m_wrapWidth == 300.0f && title->m_kerning == 1.5f);
    textList.Clear();
    CHECK(textList.m_draws == nullptr && textList.m_count == 0);

    // The scene list's own state.
    AtSceneDatabaseLoader* loader = new AtSceneDatabaseLoader;
    CHECK(loader->m_owner == nullptr && loader->m_nodeCount == 0 && loader->m_loaderNames == nullptr);
    CHECK(loader->SpaceGraphAt(0) == nullptr);
    delete loader;

    cells.Clear();
    CHECK(cells.m_graphs == nullptr && cells.m_count == 0);

    if (g_fail) {
        std::printf("test_asd: %d failures\n", g_fail);
        return 1;
    }
    std::printf("test_asd: all passed\n");
    return 0;
}
