// Host test of the material through the real text tokenizer, on any pointer size. Not evidence of a byte match.
#include <cstdio>
#include <cstring>

#include "at/AtIoTokenizer.h"
#include "at/AtMaterial.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

class Concrete : public AtMaterial {
public:
    int m_loaded = 0;
    void OnLoaded(AtMaterialOwner*) override { m_loaded++; }
};

void* g_stages[8];
void* g_textures[8];
void* const kShader = (void*)&g_stages;

}  // namespace

void* AtFindShader(AtMaterialOwner*, const char* name, const char*) { return std::strcmp(name, "Found") == 0 ? kShader : nullptr; }

int main() {
    // A text of the kind the game has, written here. Comes first: "Material" and its name, then what Read takes.
    static char text[] =
        "{\n"
        "  ConstantColor 10 20 30\n ConstantAlpha 40\n AmbientColor 1 2 3 4\n"
        "  GouraudShade\n ColorModulate\n AlphaModulate\n DoubleSided\n Blend BlendAlpha\n"
        "  AlphaCompare Greater 128\n DepthCompare LessEqual\n StencilOperation Zero Invert Never 7\n"
        "  Power 2.5\n BlendSortBias -0.5\n"
        "  Shader \"Found\" 1 2 3 4\n"
        "}\n";
    Memory stream;
    stream.m_base = text;
    stream.m_cur = text;
    stream.m_limit = text + sizeof text - 1;
    stream.m_size = sizeof text - 1;
    AtIoTextTokenizer tokenizer;
    tokenizer.SetStream(&stream);
    tokenizer.SetKeywords(AtMaterial::s_keywords, AtMaterial::kKeywordCount, nullptr);

    AtMaterialStore store = {};
    store.m_stageCapacity = 8;
    store.m_textureCapacity = 8;
    store.m_stages = g_stages;
    store.m_textures = g_textures;
    AtMaterialOwner owner = {};

    Concrete m;
    CHECK(m.m_depthCompare == 2);
    m.Read(&owner, &store, &tokenizer, nullptr);
    CHECK(m.m_loaded == 1);
    CHECK(m.m_constant.r == 10 && m.m_constant.g == 20 && m.m_constant.b == 30 && m.m_constant.a == 40);
    CHECK(m.m_ambient.r == 1 && m.m_ambient.a == 4);
    CHECK(m.m_flags & AtMaterial::kGouraudShade);
    CHECK(m.m_flags & AtMaterial::kDoubleSided);
    CHECK(m.m_blend == 2 && m.m_blendAlpha == 2);
    CHECK(m.m_alphaCompare == 3 && m.m_alphaReference == 128);
    CHECK(m.m_depthCompare == 2);
    CHECK(m.m_stencilFail == 1 && m.m_stencilDepthFail == 7 && m.m_stencilCompare == 7 && m.m_stencilReference == 7);
    CHECK(m.m_power == 2.5f && m.m_sortBias == -0.5f);
    CHECK(m.m_shader == kShader);
    CHECK(m.m_stages == g_stages + 1 && m.m_stageCount == 2);
    CHECK(m.m_textures == g_textures + 3 && m.m_textureCount == 4);

    AtMaterialDesc desc;
    m.Describe(&desc);
    CHECK(desc.m_blend == 2 && desc.m_shader == kShader && (desc.m_flags & AtMaterial::kLoaded) == 0);

    void* stages[8];
    void* textures[8];
    store.m_stages = stages;
    store.m_textures = textures;
    store.m_stageUsed = 1;
    store.m_textureUsed = 2;
    g_textures[3] = kShader;
    Concrete c;
    c.Copy(&owner, &store, m);
    CHECK(store.m_stageUsed == 3 && store.m_textureUsed == 6);
    CHECK(c.m_textures == textures + 2 && c.m_textures[0] == kShader);
    CHECK(c.m_loaded == 1);
    tokenizer.SetStream(nullptr);
    stream.m_base = nullptr;
    stream.m_cur = nullptr;
    stream.m_limit = nullptr;
    std::printf(g_fail ? "FAILED: %d\n" : "ok\n", g_fail);
    return g_fail ? 1 : 0;
}
