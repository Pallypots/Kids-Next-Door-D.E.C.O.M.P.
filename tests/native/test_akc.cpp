// Host test of the skeleton reader and the texture item's reader through the real text tokenizer, on any pointer size. Not evidence of a byte match. Load is left out, for want of a file system here.
#include <cstdio>
#include <cstring>
#include <vector>

#include "at/AtCoordSys.h"
#include "at/AtIoTokenizer.h"
#include "at/AtTextureDatabase.h"

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
    std::vector<char> bytes;
    explicit Text(const char* source) : bytes(source, source + std::strlen(source)) {
        stream.m_base = bytes.data();
        stream.m_cur = bytes.data();
        stream.m_limit = bytes.data() + bytes.size();
        stream.m_size = (unsigned int)bytes.size();
        tokenizer.SetStream(&stream);
    }
    ~Text() {
        stream.m_base = nullptr;
        stream.m_cur = nullptr;
        stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

struct Mat { float cell[16]; };

class Pool : public AtMatrixPool {
public:
    std::vector<Mat> m_mats;
    void Slot0() override {}
    void Reserve(unsigned int count) override { m_mats.assign(count, Mat()); }
    void Clear() override {}
    unsigned int Count() override { return (unsigned int)m_mats.size(); }
    AtMatrix* At(unsigned int index) override { return (AtMatrix*)&m_mats.at(index); }
};

class Matrices : public AtMatrixDevice {
public:
    Pool m_pool;
    AtMatrix* NewMatrix() override { return nullptr; }
    AtMatrixPool* NewPool() override { return &m_pool; }
    void Slot2() override {}
    void Slot3() override {}
    void FreeMatrix(AtMatrix*) override {}
    void FreePool(AtMatrixPool*) override {}
};

void Identity(AtMatrix* m) { std::memset(m, 0, sizeof(Mat)); }
void Translation(AtMatrix* m, const AtVector3* p) { ((Mat*)m)->cell[12] = p->x; ((Mat*)m)->cell[13] = p->y; ((Mat*)m)->cell[14] = p->z; }
void Rotation(AtMatrix* m, const AtQuaternion* q) { ((Mat*)m)->cell[0] = q->x; ((Mat*)m)->cell[1] = q->y; ((Mat*)m)->cell[2] = q->z; ((Mat*)m)->cell[3] = q->w; }
void Scale(AtMatrix* m, const AtVector3* s) { ((Mat*)m)->cell[0] = s->x; ((Mat*)m)->cell[5] = s->y; ((Mat*)m)->cell[10] = s->z; }
void UniformScale(AtMatrix* m, float s) { ((Mat*)m)->cell[0] = s; ((Mat*)m)->cell[5] = s; ((Mat*)m)->cell[10] = s; }

int g_heap;

void TestSkeleton() {
    g_atMatrix.Identity = Identity;
    g_atMatrix.SetTranslation = Translation;
    g_atMatrix.SetRotation = Rotation;
    g_atMatrix.SetScale = Scale;
    g_atMatrix.SetUniformScale = UniformScale;
    Text text(
        "RotTransCoordSys[2]\nScaleCoordSys[2]\nLocalDrawingTransform[1]\nCoordSys\n{\n"
        " RotTransCoordSys \"root\"\n {\n  Position 1.5 2.5 3.5\n  Orientation 0.1 0.2 0.3 0.4\n }\n"
        " ScaleCoordSys \"size\"\n {\n  Scale 2.5 3.5 4.5\n  Parent \"root\"\n }\n"
        " ScaleCoordSys \"even\"\n {\n  UniformScale 0.5\n  Parent \"size\"\n }\n"
        " RotTransCoordSys \"arm\"\n {\n  Position 7.5 8.5 9.5\n  Parent \"even\"\n"
        "  LocalDrawingTransform 1.5 2.5 3.5 4.5 5.5 6.5 7.5 8.5 9.5 10.5 11.5 12.5 13.5 14.5 15.5 16.5\n }\n}\n");
    Matrices device;
    AtCoordSysDatabase database;
    database.Read(&device, &text.tokenizer);
    CHECK(database.m_rotTransCount == 2);
    CHECK(database.m_scaledCount == 2);
    CHECK(database.m_extra == 1);
    CHECK(device.m_pool.Count() == 10);
    CHECK(database.m_nextMatrix == 10);
    AtCoordSys* root = (AtCoordSys*)database.Find("root");
    AtCoordSys* size = (AtCoordSys*)database.Find("size");
    AtCoordSys* even = (AtCoordSys*)database.Find("even");
    AtCoordSys* arm = (AtCoordSys*)database.Find("arm");
    CHECK(root == &database.m_rotTrans[0]);
    CHECK(arm == &database.m_rotTrans[1]);
    CHECK(size == &database.m_scaled[0]);
    CHECK(even == &database.m_scaled[1]);
    CHECK(database.m_root == root);
    CHECK(root->m_parent == nullptr && size->m_parent == root && even->m_parent == size && arm->m_parent == even);
    CHECK(((Mat*)root->m_local)->cell[12] == 1.5f && ((Mat*)root->m_local)->cell[3] == 0.4f);
    CHECK(database.m_scaled[0].m_scale.y == 3.5f);
    CHECK(database.m_scaled[1].m_scale.x == 0.5f && database.m_scaled[1].m_scale.z == 0.5f);
    CHECK(root->m_drawingLocal == nullptr && root->m_drawingWorld == nullptr);
    CHECK(arm->m_drawingLocal == (AtMatrix*)&device.m_pool.m_mats[8]);
    CHECK(arm->m_drawingWorld == (AtMatrix*)&device.m_pool.m_mats[9]);
    CHECK(((Mat*)arm->m_drawingLocal)->cell[0] == 1.5f && ((Mat*)arm->m_drawingLocal)->cell[15] == 16.5f);
    CHECK(database.NextMatrix() == nullptr);
    database.Clear();
    CHECK(database.m_root == nullptr);
}

void ReadItem(AtTextureBase& item, const char* body) {
    Text text(body);
    text.tokenizer.SetKeywords(AtTextureDatabase::s_keywords, AtTextureDatabase::kKeywordCount, nullptr);
    item.Read(nullptr, "NameIs8c", &text.tokenizer);
}

void TestItem() {
    AtTextureBase item;
    CHECK(item.m_levels == 0 && item.m_transparent[3] == 0xFF && item.m_lightDirection[0] == 0x7F && item.m_lightDirection[2] == 0);
    ReadItem(item, "{\n}\n");
    CHECK(std::memcmp(item.m_name, "NameIs8c", 8) == 0);
    CHECK(item.m_levels == 1 && item.m_levelsAsked == 1 && item.m_usage == 0 && item.m_flags == 0);
    ReadItem(item, "{\n TgaSource\n MipMap 3\n TextureStage 2\n DemandLoad\n}\n");
    CHECK((item.m_usage & AtTextureBase::kSourceMask) == AtTextureBase::kSourceTga);
    CHECK(item.m_levels == 3 && item.m_levelsAsked == 3 && item.m_textureStage == 2);
    CHECK(item.m_flags == AtTextureBase::kDemandLoad);
    ReadItem(item, "{\n TgaSource\n PlatformSource\n MipMap 0\n}\n");
    CHECK((item.m_usage & AtTextureBase::kSourceMask) == AtTextureBase::kSourceDds);
    CHECK(item.m_flags == AtTextureBase::kPlatformSource);
    CHECK(item.m_levels == 1);
    ReadItem(item, "{\n TplSource\n RenderTarget\n FlipVertically\n TransparentColor 1 2 3\n LightDirection 0.0 0.0 1.0\n}\n");
    CHECK(item.m_usage == (AtTextureBase::kSourceTpl | AtTextureBase::kRenderTarget | AtTextureBase::kFlipVertically));
    CHECK(item.m_transparent[0] == 1 && item.m_transparent[1] == 2 && item.m_transparent[2] == 3 && item.m_transparent[3] == 0xFF);
    CHECK(item.m_flags == AtTextureBase::kTransparentColor);
    CHECK(item.m_lightDirection[0] == 0x7F && item.m_lightDirection[2] == 0xFF);
    ReadItem(item, "{\n ProgSource 128 64 32 1 16711680 65280 255 -16777216\n}\n");
    CHECK((item.m_usage & AtTextureBase::kSourceMask) == AtTextureBase::kSourceProg);
    CHECK(item.m_progSource != nullptr);
    if (item.m_progSource != nullptr) {
        CHECK(item.m_progSource->m_width == 128 && item.m_progSource->m_height == 64);
        CHECK(item.m_progSource->m_format.m_mask0 == 16711680u);
    }
    item.FreeProgSource();
    item.m_usage = 0xFFFFFFFFu;
    item.ClearModifyUsage(0);
    CHECK(item.m_usage == 0xFFFFFFCEu);
    item.m_flags = AtTextureBase::kPending | AtTextureBase::kTransparentColor;
    item.ClearPending();
    item.ClearTransparentColor();
    CHECK(item.m_flags == 0);
    item.Describe(nullptr, "Other", 0x205, 1, 0, 300);
    CHECK(item.m_usage == 0x205 && item.m_flags == AtTextureBase::kAutoGenMipLevels && item.m_levels == 1 && item.m_textureStage == 44);
    item.Reset();
    CHECK(item.m_progSource == nullptr && item.m_levels == 0 && item.m_name[0] == 0);
}

}  // namespace

int main() {
    AtIoTokenFile::s_heap = &g_heap;
    GmNameMap::s_heap = &g_heap;
    AtCoordSysDatabase::SetHeap(&g_heap);
    TestSkeleton();
    TestItem();
    std::printf(g_fail ? "FAILED: %d\n" : "ok\n", g_fail);
    return g_fail ? 1 : 0;
}
