// Host test of the Flash sprites, frames and shapes on any pointer size: a sprite's frame list through the real text tokenizer, a frame and its tags from .XFF bytes, a shape, and the resource table's groups. Not evidence of
// a byte match; the differential oracle is tests/oracle/oracle_afd.cpp.
#include <cstdio>
#include <cstring>

#include "../../src/at/AtSceneArray.h"
#include "at/AtFlashLevel.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

struct Text {
    Memory stream;
    AtIoTextTokenizer tokenizer;
    Text(char* text, const AtIoKeyword* words, unsigned count) {
        stream.m_base = text;
        stream.m_cur = text;
        stream.m_limit = text + std::strlen(text);
        stream.m_size = (unsigned)std::strlen(text);
        tokenizer.SetStream(&stream);
        tokenizer.SetKeywords(words, count, nullptr);
    }
    ~Text() { tokenizer.SetStream(nullptr); }
};

const AtIoKeyword kSpriteWords[] = {{0x3B, "SpriteDef"}, {0x3C, "FrameRefs"}};
const AtIoKeyword kShapeWords[] = {{0x3B, "Shape"}, {0x3C, "Rectangle"}, {0x3D, "MeshAssignment"}};

int g_heap;

void put32(unsigned char* p, unsigned v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24); }

// A table whose releases are counted, to see the groups hand their members back.
class Counting : public AtFlashResourceTable {
public:
    unsigned released[4] = {};
    void Release(unsigned int index) override { released[index]++; }
};

}  // namespace

int main() {
    AtFlashResourceTable::s_defaultHeap = &g_heap;
    AtFpSprite::s_heap = &g_heap;
    AtFlashFrame::s_heap = &g_heap;
    AtFpPlaceObject2Tag::s_heap = &g_heap;

    // A sprite: names, plain frames, a run and a repeat.
    {
        char text[] = "SpriteDef { 7 FrameRefs[ 8 ] { 57,\"Init\" 0; 3:5 9*3 } }";
        Text t(text, kSpriteWords, 2);
        AtFpSprite sprite;
        AtFlashResource* resource = &sprite;
        resource->Read(&t.tokenizer);
        CHECK(sprite.m_index == 7);
        CHECK(sprite.m_used == 1);
        CHECK(sprite.m_refCount == 8);
        CHECK(sprite.m_refCapacity == 8);
        CHECK(sprite.m_refs[0].m_frame == 57 && std::strcmp(sprite.m_refs[0].m_name, "Init") == 0);
        CHECK(sprite.m_refs[1].m_frame == 0 && sprite.m_refs[1].m_name == nullptr);
        CHECK(sprite.m_refs[2].m_frame == 3 && sprite.m_refs[3].m_frame == 4 && sprite.m_refs[4].m_frame == 5);
        CHECK(sprite.m_refs[5].m_frame == 9 && sprite.m_refs[6].m_frame == 9 && sprite.m_refs[7].m_frame == 9);
        CHECK(t.stream.m_cur == t.stream.m_limit);
    }

    // A shape.
    {
        char text[] = "Shape 3 { Rectangle -320.0 -240.0 320.0 240.5 MeshAssignment 4 \"00000004\" }";
        Text t(text, kShapeWords, 3);
        AtFlashShape shape;
        CHECK(shape.m_mesh == -1);
        AtFlashResource* resource = &shape;
        resource->Read(&t.tokenizer);
        CHECK(shape.m_index == 3);
        CHECK(shape.m_rectangle[0] == -320.0f && shape.m_rectangle[3] == 240.5f);
        CHECK(shape.m_mesh == 4);
    }

    // A frame: one of each tag, a place with a name, a transform and a color.
    {
        unsigned char data[0x100] = {};
        unsigned char* p = data;
        put32(p, 12); p[4] = 1; p[5] = 1; p[6] = 1; p[7] = 1; p += 8;
        put32(p, 21); p += 4;                                  // remove
        unsigned char* place = p; p += 0x18;                   // place
        put32(p, 33); p += 4;                                  // action
        put32(p, 44); p += 4;                                  // sound
        place[0] = 2; place[2] = 5; place[3] = 6; place[4] = 0x34; place[5] = 0x12; place[6] = 0x78; place[7] = 0x56; put32(place + 8, 99);
        // Offsets count from the end of the place record.
        const unsigned name = 8, transform = 0x10, color = 0x30;
        put32(place + 0xC, name); put32(place + 0x10, transform); put32(place + 0x14, color);
        std::strcpy((char*)place + 0x18 + name, "Clip");
        for (int i = 0; i < 6; i++) put32(place + 0x18 + transform + i * 4, 0x100 + i);
        unsigned char* c = place + 0x18 + color;
        c[0] = 1; c[1] = 2; c[2] = 3; c[3] = 4; c[4] = 255; c[5] = 0; c[6] = 51; c[7] = 255;

        AtFlashActionTag* actions = AtNewArray<AtFlashActionTag>(1);
        AtFpPlaceObject2Tag* places = AtNewArray<AtFpPlaceObject2Tag>(1);
        AtFlashRemoveTag* removes = AtNewArray<AtFlashRemoveTag>(1);
        AtFlashSoundTag* sounds = AtNewArray<AtFlashSoundTag>(1);
        CHECK(places[0].m_type == 1 && actions[0].m_type == 4 && removes[0].m_type == 2 && sounds[0].m_type == 3);
        AtFlashActionTag* a = actions; AtFpPlaceObject2Tag* b = places; AtFlashRemoveTag* r = removes; AtFlashSoundTag* s = sounds;
        const unsigned char* cursor = data;
        AtFlashFrame frame;
        frame.ReadBinary(&cursor, &a, &b, &r, &s);
        CHECK(frame.m_index == 12);
        CHECK(frame.m_tagCount == 4);
        CHECK(frame.m_flags == 3);
        CHECK(frame.m_tags[0] == &removes[0] && frame.m_tags[1] == &places[0] && frame.m_tags[2] == &actions[0] && frame.m_tags[3] == &sounds[0]);
        CHECK(cursor == data + 8 + 4 + 0x18 + 4 + 4);
        CHECK(removes[0].m_id == 21 && actions[0].m_id == 33 && sounds[0].m_id == 44);
        CHECK(places[0].m_unknown34 == 2 && places[0].m_unknown30 == 5 && places[0].m_unknown50 == 6);
        CHECK(places[0].m_unknown28 == 0x1234 && places[0].m_unknown2A == 0x5678 && places[0].m_unknown54 == 99);
        CHECK(places[0].m_name && std::strcmp(places[0].m_name, "Clip") == 0);
        CHECK(places[0].m_transform[0] == 0x100 && places[0].m_transform[5] == 0x105);
        CHECK(places[0].m_colorOffset[3] == 4);
        // 255 times the original's constant for 1/255 is not quite 1.
        CHECK(places[0].m_colorScale[0] > 0.9999f && places[0].m_colorScale[0] <= 1.0f && places[0].m_colorScale[1] == 0.0f);
        CHECK(places[0].m_colorScale[2] > 0.1999f && places[0].m_colorScale[2] < 0.2001f);
        CHECK(places[0].m_loaded == 1 && places[0].m_state == 1);
        frame.Clear();
        CHECK(frame.m_tags == nullptr && frame.m_flags == 0);
        AtDeleteArray(actions);
        AtDeleteArray(places);
        AtDeleteArray(removes);
        AtDeleteArray(sounds);
    }

    // Groups: six made, members handed back on recycling, the group kept last or freed.
    {
        Counting table;
        table.Init();
        CHECK(table.m_groupCount == 6 && table.m_initialised == 1);
        AtFlashResourceTable::Group* first = table.m_firstGroup;
        AtFlashResource items[2];
        items[0].m_index = 1; items[0].m_uses = 2;
        items[1].m_index = 3; items[1].m_uses = 1;
        items[0].m_groupNext = &items[1].m_groupNext; items[0].m_groupPrev = nullptr;
        items[1].m_groupNext = nullptr; items[1].m_groupPrev = &items[0].m_groupNext;
        first->m_first = &items[0].m_groupNext;
        first->m_last = &items[1].m_groupNext;
        table.Slot3(0);
        CHECK(table.released[1] == 2 && table.released[3] == 1);
        CHECK(items[0].m_uses == 0 && items[1].m_uses == 0);
        CHECK(first->m_first == nullptr && first->m_last == nullptr);
        CHECK(table.m_lastGroup == first && table.m_firstGroup != first && first->m_next == nullptr);
        table.SetGroupCount(2);
        CHECK(table.m_groupCount == 2);
        CHECK(table.m_firstGroup->m_next == table.m_lastGroup && table.m_lastGroup == first);
        table.SetGroupCount(3);
        CHECK(table.m_groupCount == 3 && table.m_lastGroup != first && table.m_lastGroup->m_prev == first);
    }

    if (g_fail) std::printf("%d failures\n", g_fail);
    else std::printf("test_afd: all checks passed\n");
    return g_fail ? 1 : 0;
}
