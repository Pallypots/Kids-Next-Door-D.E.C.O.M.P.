// Host test of the shader loader through the real text tokenizer, on any pointer size. Not evidence of a byte match.
#include <cstdio>
#include <cstring>

#include "at/AtIoTokenizer.h"
#include "at/AtShaderDatabase.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

class Database : public AtShaderDatabase {
public:
    void OnReady() override { m_ready++; }
    AtShader* ShaderAt(unsigned int index) override { return &m_items[index]; }
    AtShader m_items[4];
    int m_ready = 0;
};

int g_heap;
int g_a, g_b, g_c, g_d, g_e;

void read(Database& db, AtMaterialOwner& owner, char* text, unsigned int size) {
    Memory stream;
    stream.m_base = text;
    stream.m_cur = text;
    stream.m_limit = text + size;
    stream.m_size = size;
    AtIoTextTokenizer tokenizer;
    tokenizer.SetStream(&stream);
    db.Read(&owner, &tokenizer);
    stream.m_base = nullptr;
    stream.m_cur = nullptr;
    stream.m_limit = nullptr;
    tokenizer.SetStream(nullptr);
}

}  // namespace

int main() {
    GmNameMap::s_heap = &g_heap;
    GmNameSpaceDb::s_heap = &g_heap;
    AtShader::SetHeap(&g_heap);
    AtProgShaderComponent::SetHeap(&g_heap);
    AtShaderDatabase::SetHeap(&g_heap);
    CHECK(AtShaderDatabase::s_heap == &g_heap);

    AtMaterialOwner owner = {};
    owner.m_shaderSpaces.Init(4);
    owner.m_textureSpaces.Init(4);
    GmNameMap textures, spaceTextures, vertex, pixel;
    textures.Init(5);
    textures.Insert("wood", &g_a);
    spaceTextures.Init(5);
    spaceTextures.Insert("wood", &g_b);
    vertex.Init(3);
    vertex.Insert("skin", &g_c);
    vertex.Insert("water", &g_d);
    pixel.Init(3);
    pixel.Insert("shiny", &g_e);
    owner.m_textures.Add(&textures);
    owner.m_textureSpaces.Add("Other", &spaceTextures);
    owner.m_vertexProcesses.Add(&vertex);
    owner.m_pixelProcesses.Add(&pixel);

    {
        static char text[] =
            "Shader[2]\n{\n"
            "  Shader \"plain\"\n  {\n"
            "    StagedShaderComponent[2]\n    {\n"
            "      StagedShaderComponent\n      {\n"
            "        TextureStage\n        {\n"
            "          Texture \"wood\"\n          TxtCoordOffset 3\n          Coord2DTransform 300\n          Bilinear\n          Clamp\n"
            "        }\n"
            "        CombineOp BlendAlpha\n        ModulateOp ModulateTextureByDiffuse\n        AlphaSource SelectConstant\n        DisableDepthWrites\n"
            "      }\n"
            "      StagedShaderComponent\n      {\n"
            "        TextureStage\n        {\n"
            "          TextureWithNameSpace \"wood\" \"Other\"\n          Anisotropic 2.5\n          Border 1 2 3 4\n          MipNearest\n          CoordGenCameraSpacePos\n"
            "        }\n"
            "      }\n"
            "    }\n"
            "  }\n"
            "  Shader \"prog\"\n  {\n"
            "    ProgShaderComponent[1]\n    {\n"
            "      ProgShaderComponent\n      {\n"
            "        VertexProcess[2]\n        {\n          \"skin\"\n          \"water\"\n        }\n"
            "        PixelProcess \"shiny\"\n"
            "        TextureStage[2]\n        {\n"
            "          TextureStage\n          {\n            Texture \"wood\"\n            Coord3DTransform 7\n          }\n"
            "          TextureStage\n          {\n            CubeNormalizationTexture\n          }\n"
            "        }\n"
            "        CombineOp Subtract\n"
            "        FlipFaceCulling\n"
            "      }\n"
            "    }\n"
            "  }\n"
            "}\n";
        Database db;
        read(db, owner, text, sizeof text - 1);
        CHECK(db.m_count == 2);
        CHECK(db.m_ready == 1);
        CHECK(db.Find("plain") == &db.m_items[0]);
        CHECK(db.Find("prog") == &db.m_items[1]);
        CHECK(owner.m_shaders.Find("prog") == &db.m_items[1]);
        CHECK(owner.FindShader("plain", nullptr) == &db.m_items[0]);

        const AtShader& plain = db.m_items[0];
        CHECK(plain.m_stagedCount == 2 && plain.m_progCount == 0 && plain.m_progs == nullptr);
        const AtStagedShaderComponent& first = plain.m_stageds[0];
        CHECK(first.m_combine.m_op == 2);
        CHECK(first.m_modulate == AtStagedShaderComponent::kOpTextureByDiffuse);
        CHECK(first.m_alphaSource == AtStagedShaderComponent::kFromConstant);
        CHECK(first.m_switches == kShaderDisableDepthWrites);
        CHECK(first.m_stage.m_texture == &g_a);
        CHECK(first.m_stage.m_coordOffset == 3);
        CHECK(first.m_stage.m_transform == 44);  // Cut to a byte.
        CHECK(first.m_stage.m_flags == (AtTextureStage::kFlagMipLinear | AtTextureStage::kFlagClamp | AtTextureStage::kFlagTransform2D | 1));
        const AtStagedShaderComponent& second = plain.m_stageds[1];
        CHECK(second.m_combine.m_op == 0);
        CHECK(second.m_modulate == AtStagedShaderComponent::kOpSelectTexture);
        CHECK(second.m_alphaSource == AtStagedShaderComponent::kFromModulatedOutput);
        CHECK(second.m_stage.m_texture == &g_b);
        CHECK(second.m_stage.m_anisotropy == 2.5f);
        CHECK(second.m_stage.m_border.r == 1 && second.m_stage.m_border.a == 4);
        CHECK(second.m_stage.m_flags == (AtTextureStage::kFlagBorder | 0xC0 | 2));
        // Only the first has a transform, and Read notes it.
        CHECK(plain.m_transformCount == 1 && plain.m_transforms[0] == 44);

        AtShader& prog = db.m_items[1];
        CHECK(prog.m_progCount == 1 && prog.m_stagedCount == 0 && prog.m_stageds == nullptr);
        const AtProgShaderComponent& p = prog.m_progs[0];
        CHECK(p.m_combine.m_op == 9);
        CHECK(p.m_switches == kShaderFlipFaceCulling);
        CHECK(p.m_vertexProcessCount == 2 && p.m_vertexProcesses[0] == &g_c && p.m_vertexProcesses[1] == &g_d);
        CHECK(p.m_pixelProcess == &g_e);
        CHECK(p.m_stageCount == 2);
        CHECK(p.m_stages[0].m_texture == &g_a && p.m_stages[1].m_texture == nullptr);
        CHECK(p.m_stages[1].m_flags & AtTextureStage::kFlagCubeNormalization);
        CHECK(p.CountTextures() == 1);
        // Read notes only staged transforms; the graphics layer asks for these.
        CHECK(prog.m_transformCount == 0);
        prog.NoteProgTransforms();
        CHECK(prog.m_transformCount == 1 && prog.m_transforms[0] == 7);
        prog.NoteStagedTransforms();
        CHECK(prog.m_transformCount == 0);

        // Read again: each shader clears itself, and the arrays' cookies must let it.
        static char again[] = "Shader[1]\n{\n  Shader \"next\"\n  {\n  }\n}\n";
        read(db, owner, again, sizeof again - 1);
        CHECK(db.m_count == 1 && db.m_ready == 2);
        CHECK(db.Find("plain") == nullptr && db.Find("next") == &db.m_items[0]);
        CHECK(db.m_items[0].m_stageds == nullptr && db.m_items[0].m_stagedCount == 0);

        db.m_items[1].Clear();
        CHECK(db.m_items[1].m_progs == nullptr && db.m_items[1].m_progCount == 0);
        db.Clear();
        CHECK(db.m_count == 0 && db.m_entries == nullptr);
    }
    CHECK(owner.m_shaders.m_first == nullptr);

    // Under a name space: the file's own, for shaders and for its textures.
    {
        static char text[] =
            "NameSpace \"Other\"\n"
            "Shader[1]\n{\n"
            "  Shader \"named\"\n  {\n"
            "    StagedShaderComponent[1]\n    {\n      StagedShaderComponent\n      {\n        TextureStage\n        {\n          Texture \"wood\"\n        }\n      }\n    }\n"
            "  }\n"
            "}\n";
        Database db;
        read(db, owner, text, sizeof text - 1);
        CHECK(owner.m_shaderSpaces.Find("Other", "named") == &db.m_items[0]);
        CHECK(owner.FindShader("named", "Other") == &db.m_items[0]);
        CHECK(owner.m_shaders.m_first == nullptr);
        CHECK(db.m_items[0].m_stageds[0].m_stage.m_texture == &g_b);
    }

    if (g_fail == 0) std::printf("ok\n");
    return g_fail != 0;
}
