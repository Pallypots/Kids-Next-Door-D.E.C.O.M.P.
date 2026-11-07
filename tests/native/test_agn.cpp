// Host test of the AGN family of loaders through the real text tokenizer, on any pointer size. Not evidence of a byte match.
#include <cstdio>
#include <cstring>

#include "at/AtImageDatabase.h"
#include "at/AtIoTokenizer.h"
#include "at/AtLensFlare.h"
#include "at/AtLight.h"
#include "at/AtMatAnimation.h"
#include "at/AtMatAssignment.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

int g_heap;

struct Text {
    Memory stream;
    AtIoTextTokenizer tokenizer;
    Text(char* text, const AtIoKeyword* words, unsigned int count) {
        stream.m_base = stream.m_cur = text;
        stream.m_limit = text + std::strlen(text);
        stream.m_size = (unsigned int)std::strlen(text);
        tokenizer.SetStream(&stream);
        if (words != nullptr) tokenizer.SetKeywords(words, count, nullptr);
    }
    ~Text() {
        stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

class TestFont : public AtFontItem {
public:
    int loaded = 0;
    void Slot0() override {}
    void Slot1() override {}
    void Slot2() override {}
    void Slot3() override {}
    void Slot4() override {}
    void Slot5() override {}
    void Slot6() override {}
    void Slot8() override {}
    void Read(AtMaterialOwner*, AtFontDatabase*, const char*, AtIoTokenizer* tokenizer) override {
        m_flags = 0;
        tokenizer->ExpectLeftCurly();
        for (int t = tokenizer->Next(); t != AtIoTokenizer::kRightCurly && t != AtIoTokenizer::kEnd; t = tokenizer->Next()) {
            if (t == 0x48) m_flags |= 8;
        }
    }
    void OnLoaded(AtMaterialOwner*) override { loaded++; }
};

class TestImage : public AtImageItem {
public:
    int loaded = 0;
    void Slot0() override {}
    void Slot1() override {}
    void Slot2() override {}
    void Slot3() override {}
    void Slot4() override {}
    void Slot5() override {}
    void Slot6() override {}
    void Slot7() override {}
    void Slot9() override {}
    void Read(AtMaterialOwner*, AtImageDatabase*, const char*, AtIoTokenizer* tokenizer) override {
        m_flags = 0;
        tokenizer->ExpectLeftCurly();
        for (int t = tokenizer->Next(); t != AtIoTokenizer::kRightCurly && t != AtIoTokenizer::kEnd; t = tokenizer->Next()) {
            if (t == 0x41) m_flags |= 8;
        }
    }
    void OnLoaded(AtMaterialOwner*) override { loaded++; }
};

class Fonts : public AtFontDatabase {
public:
    TestFont items[2];
    int ready = 0;
    void OnReady(AtMaterialOwner*) override { ready++; }
    void MakeFonts2D() override {}
    AtFontItem* Font2DAt(unsigned int i) override { return &items[i]; }
};
class Images : public AtImageDatabase {
public:
    TestImage items[2];
    int made = 0;
    void MakeItems() override { made++; }
    void OnReady(AtMaterialOwner*) override {}
    AtImageItem* ImageAt(unsigned int i) override { return &items[i]; }
};
class TestLight : public AtLight {
public:
    void Slot0() override {}
    void Reset() override {}
};
class Lights : public AtLightDatabase {
public:
    TestLight items[2];
    void MakeLights(AtMaterialOwner*) override { std::memset((char*)&items[0] + sizeof(void*), 0, sizeof(TestLight) - sizeof(void*)); }
    AtLight* LightAt(unsigned int i) override { return &items[i]; }
};

void setup(AtMaterialOwner& owner, GmNameMap& materials, int& a, int& b) {
    materials.Init(4);
    materials.Insert("M", &a);
    materials.Insert("N", &b);
    owner.m_materials.Add(&materials);
}

}  // namespace

int main() {
    AtMatAssignment::SetHeap(&g_heap);
    AtMatAssignmentDatabase::SetHeap(&g_heap);
    AtGrFrameNumberSet::SetHeap(&g_heap);
    AtMatAnimationDatabase::SetHeap(&g_heap);
    AtImageDatabase::SetHeap(&g_heap);
    AtFontDatabase::SetHeap(&g_heap);
    AtLensFlare::SetHeap(&g_heap);
    AtLensFlareDatabase::SetHeap(&g_heap);
    GmNameMap::s_heap = &g_heap;
    GmNameSpaceDb::s_heap = &g_heap;
    AtMaterialOwner owner;
    GmNameMap materials;
    int a = 1, b = 2;
    setup(owner, materials, a, b);

    {
        char text[] = "MatAssignment[2]\n{\n MatAssignment \"X\"[2]\n {\n \"M\"\n \"N\"\n }\n MatAssignment \"Y\"[1]\n {\n \"\"\n }\n}\n";
        AtMatAssignmentDatabase db;
        Text t(text, AtMatAssignmentDatabase::s_keywords, AtMatAssignmentDatabase::kKeywordCount);
        db.Read(&owner, &t.tokenizer);
        CHECK(db.m_count == 2);
        CHECK(db.m_lists[0].m_count == 2);
        CHECK(db.m_lists[0].m_items[0] == &a);
        CHECK(db.m_lists[0].m_items[1] == &b);
        CHECK(db.m_lists[1].m_items[0] == nullptr);
        CHECK(db.Find("Y") == &db.m_lists[1]);
        CHECK(db.m_group == &owner.m_matAssignments);
        AtMatAssignment copy;
        copy.Copy(db.m_lists[0]);
        CHECK(copy.m_items[1] == &b && copy.m_items != db.m_lists[0].m_items);
    }
    {
        char text[] = "FrameNumber[4]\n{\n 0\n 2\n 4\n 6\n}\nMaterialKey[2]\n{\n \"M\" \"\"\n \"N\" \"\"\n}\nMatAnimation[1]\n{\n MatAnimation \"A\"\n {\n Keyframes 0 0 2\n Rate 15.5\n Length 8\n }\n}\n";
        AtMatAnimationDatabase db;
        Text t(text, nullptr, 0);
        db.Read(&owner, &t.tokenizer);
        CHECK(db.m_frameNumbers.m_count == 4);
        CHECK(db.m_frameNumbers.At(3) == 6);
        CHECK(db.m_keys[1] == &b);
        CHECK(db.m_animationCount == 1);
        const AtMatAnimation& run = db.m_animations[0];
        CHECK(run.m_rate == 15.5f);
        CHECK(run.m_length == 8 && run.m_loopPeriod == 9);
        CHECK(run.m_keyCount == 2);
        int from = -1, to = -1;
        float along = -1;
        db.m_frameNumbers.Interpolate(0, 4, 3.0f, 8, &from, &to, &along);
        CHECK(from == 1 && to == 2 && along == 0.5f);
        db.m_frameNumbers.Interpolate(0, 4, 7.0f, 8, &from, &to, &along);
        CHECK(from == 3 && to == 0 && along == 0.5f);
        CHECK(db.m_frameNumbers.Find(0, 4, 5.5f, 8) == 2);
        CHECK(db.m_frameNumbers.Find(0, 4, 100.0f, 8) == 3);
        void* slots[1] = {nullptr};
        AtMatAssignment list;
        list.m_count = 1;
        list.m_items = slots;
        db.m_animations[0].Apply(&list, 0, 2.5f);
        CHECK(slots[0] == &b);
        list.m_items = nullptr;
        list.m_count = 0;
    }
    {
        char text[] = "NameSpace \"S\"\nFont[2]\nMatAssignment[1]\n{\n \"XNone\"\n}\nFont2D[2]\n{\n Font2D \"A\"\n {\n Kerning\n }\n Font2D \"B\"\n {\n DemandLoad\n }\n}\n";
        Fonts db;
        owner.m_fontSpaces.Init(2);
        owner.m_materialSpaces.Init(2);
        Text t(text, nullptr, 0);
        db.Read(&owner, &t.tokenizer);
        CHECK(db.m_count == 2);
        CHECK(db.ready == 1);
        CHECK(db.items[0].loaded == 1);
        CHECK(db.items[1].loaded == 0);
        CHECK(AtFontDatabase::s_demandLoadCount == 1);
        CHECK(db.Find("B") == &db.items[1]);
    }
    {
        char text[] = "MatAssignment[1]\n{\n \"M\"\n}\nImage[2]\n{\n Image \"A\"\n {\n TgaSource\n }\n Image \"B\"\n {\n TgaSource\n }\n}\n";
        Images db;
        Text t(text, nullptr, 0);
        db.Read(&owner, &t.tokenizer);
        CHECK(db.made == 1);
        CHECK(db.m_count == 2);
        CHECK(db.m_materials.m_items[0] == &a);
        CHECK(db.items[1].loaded == 1);
        CHECK(AtImageDatabase::s_demandLoadCount == 0);
        CHECK(db.m_group == &owner.m_images);
    }
    {
        char text[] = "Light[1]\n{\n Light \"L\"\n {\n AmbientColor 1.0 2.0 3.0 4.0\n DiffuseColor 5.0 6.0 7.0 8.0\n Range 4.5\n Attenuation 1.0 2.0 3.5\n InnerConeAngle 12.0\n OuterConeAngle 13.0\n }\n}\n";
        Lights db;
        Text t(text, nullptr, 0);
        db.Read(&owner, &t.tokenizer);
        const AtLight& light = db.items[0];
        CHECK(light.m_owner == &owner);
        CHECK(light.m_ambient.w == 4.0f);
        CHECK(light.m_diffuse.x == 5.0f);
        CHECK(light.m_range == 4.5f);
        CHECK(light.m_attenuation[2] == 3.5f);
        CHECK(light.m_innerConeAngle == 12.0f && light.m_outerConeAngle == 13.0f);
        CHECK(db.Find("L") == &db.items[0]);
    }
    {
        char text[] = "LensFlare[1]\n{\n LensFlare \"F\"\n {\n LensCenter 1.0 0.5 0.0 1.0\n DisableDepthColoring\n MatAssignment[1]\n {\n \"N\"\n }\n Flare[1]\n {\n Flare\n {\n MatAssignment 0\n SquareSize 3.0\n RotateFlare\n }\n }\n }\n}\n";
        AtNameTableOwner scene;
        scene.m_device = (AtSceneDevice*)&owner;
        AtLensFlareDatabase db;
        Text t(text, nullptr, 0);
        db.Read(&scene, &t.tokenizer);
        const AtLensFlare& flare = db.m_flares[0];
        CHECK(flare.m_flags == AtLensFlare::kDistanceColoring);
        CHECK(flare.m_centerColor.y == 127.5f);
        CHECK(flare.m_innerEnd == 500.0f && flare.m_outerEnd == 1000.0f);
        CHECK(flare.m_materials.m_items[0] == &b);
        CHECK(flare.m_squareCount == 1);
        CHECK(flare.m_squares[0].m_halfSize == 1.5f);
        CHECK(flare.m_squares[0].m_rotate == 1);
        CHECK(db.m_group == &scene.m_lensFlares);
    }
    if (g_fail) return 1;
    std::printf("test_agn: ok\n");
    return 0;
}
