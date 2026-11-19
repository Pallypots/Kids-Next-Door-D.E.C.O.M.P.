// Host test of images and fonts (include/at/AtImage.h) through the real text tokenizer and the real databases, on any pointer size. Not evidence of a byte match.
#include <cstdio>
#include <cstring>

#include "at/AtImage.h"
#include "at/AtImageDatabase.h"
#include "at/AtIoTokenizer.h"

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

// The concrete slots the graphics layer fills are not recovered; these leave them empty.
class Image : public AtImage {
public:
    int loaded = 0;
    AtMaterial* Material() override { return nullptr; }
    void Slot1() override {}
    void Slot2() override {}
    void DrawStrip(AtGr2DRenderer*, unsigned int, AtMaterial*, const AtRect*, const AtRect*, const AtColorRectangle*) override {}
    void OnLoaded(AtMaterialOwner*) override { loaded++; }
    void Slot14() override {}
};
class Font : public AtGrI2DFont {
public:
    int loaded = 0;
    void Slot4() override {}
    void OnLoaded(AtMaterialOwner*) override { loaded++; }
    void Slot11() override {}
    void Slot12() override {}
};

class Images : public AtImageDatabase {
public:
    Image items[3];
    void MakeItems() override {}
    void OnReady(AtMaterialOwner*) override {}
    AtImage* ImageAt(unsigned int i) override { return &items[i]; }
};
class Fonts : public AtFontDatabase {
public:
    Font items[2];
    void OnReady(AtMaterialOwner*) override {}
    void MakeFonts2D() override {}
    AtGrI2DFont* Font2DAt(unsigned int i) override { return &items[i]; }
};

class Letters : public AtGrCharacterSource {
public:
    const char* text;
    explicit Letters(const char* t) : text(t) {
        m_unknown04 = 0;
        m_begin = t;
        m_end = t + std::strlen(t);
    }
    unsigned short At(unsigned int i) override { return (unsigned char)text[i]; }
};

}  // namespace

int main() {
    AtMatAssignment::SetHeap(&g_heap);
    AtImageDatabase::SetHeap(&g_heap);
    AtFontDatabase::SetHeap(&g_heap);
    AtGrI2DFont::SetHeap(&g_heap);
    GmNameMap::s_heap = &g_heap;
    GmNameSpaceDb::s_heap = &g_heap;
    AtMaterialOwner owner;
    GmNameMap materials;
    int a = 1;
    materials.Init(4);
    materials.Insert("M", &a);
    owner.m_materials.Add(&materials);

    {
        Image image;
        CHECK(image.m_material == 0xFFFF);
        CHECK(image.m_transparent.a == 255 && image.m_transparent.r == 0);
        CHECK(image.m_colorRectangle.m_colors[2].g == 255);
    }
    {
        char text[] = "MatAssignment[1]\n{\n \"M\"\n}\nImage[3]\n{\n"
                      " Image \"A\"\n {\n TgaSource\n FrequentModify\n MatAssignment 0\n TransparentColor 1 2 3\n FlipTextureVertically\n }\n"
                      " Image \"B\"\n {\n PlatformSource\n DemandLoad\n ColorValueRectangle 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16\n }\n"
                      " Image \"C\"\n {\n Tm2Source\n TplSource\n MpegMacroBlocks\n InfrequentModify\n FlipTextureHorizontally\n }\n}\n";
        Images db;
        Text t(text, nullptr, 0);
        db.Read(&owner, &t.tokenizer);
        const AtImage& x = db.items[0];
        CHECK(std::strncmp(x.m_name, "A", 8) == 0);
        CHECK(x.m_format == (AtImage::kTgaSource | AtImage::kFrequentModify));
        CHECK(x.m_material == 0 && x.m_materials == &db.m_materials && x.m_database == &db);
        CHECK(x.m_transparent.r == 1 && x.m_transparent.g == 2 && x.m_transparent.b == 3 && x.m_transparent.a == 255);
        CHECK(x.m_flags == (AtImage::kRead | AtImage::kTransparent | AtImage::kFlipVertically));
        const AtImage& y = db.items[1];
        CHECK(y.m_flags == (AtImage::kRead | AtImage::kPlatformSource | AtImage::kDemandLoad | AtImage::kColorRectangle));
        CHECK(y.m_format == AtImage::kDdsSource);
        CHECK(y.m_colorRectangle.m_colors[3].a == 16 && y.m_colorRectangle.m_colors[0].r == 1);
        const AtImage& z = db.items[2];
        CHECK(z.m_format == (AtImage::kTplSource | AtImage::kMpegMacroBlocks | AtImage::kInfrequentModify));
        CHECK(z.m_flags == (AtImage::kRead | AtImage::kFlipHorizontally));
        CHECK(db.items[0].loaded == 1 && db.items[1].loaded == 0 && db.items[2].loaded == 1);
        CHECK(AtImageDatabase::s_demandLoadCount == 1);

        // Made with no file, over what was read.
        db.items[0].Init(&owner, &db, "Fresh", AtImage::kTm2Source | AtImage::kOwnsData | 0x1000);
        CHECK(db.items[0].m_flags == (AtImage::kRead | AtImage::kTm2Source | AtImage::kOwnsData | 0x1000u));
        CHECK(db.items[0].m_format == AtImage::kTm2Source);
        CHECK(db.items[0].m_material == 0xFFFF && std::strncmp(db.items[0].m_name, "Fresh", 8) == 0);
        db.items[0].FreeData();
        CHECK((db.items[0].m_flags & AtImage::kOwnsData) == 0);
        AtColor c = {9, 8, 7, 6};
        db.items[0].SetTransparentColor(&c);
        CHECK(db.items[0].m_transparent.a == 6 && (db.items[0].m_flags & AtImage::kTransparent));
        db.items[0].ClearTransparentColor();
        CHECK((db.items[0].m_flags & AtImage::kTransparent) == 0 && db.items[0].m_transparent.a == 6);
    }
    {
        // Widths: powers of two of the smallest, the largest under the maximum, the rest rounded up.
        Image image;
        unsigned int out[8] = {};
        CHECK(image.SplitStrips(4, 64, 100, 0, out) == 2);
        CHECK(out[0] == 64 && out[1] == 64);
        CHECK(image.SplitStrips(1, 256, 3, 0, out) == 1 && out[0] == 4);
        CHECK(image.SplitStrips(16, 16, 40, 0, out) == 3 && out[0] == 16 && out[1] == 16 && out[2] == 16);
    }
    {
        char text[] = "NameSpace \"S\"\nFont[2]\nMatAssignment[1]\n{\n \"M\"\n}\nFont2D[2]\n{\n"
                      " Font2D \"A\"\n {\n FrequentModify\n TgaSource\n Kerning -2.5\n Leading 3.0\n Scale 0.5 2.0\n MatAssignment 0\n TransparentColor 4 5 6\n"
                      "  ColorRectangleCharX\n  CharacterSet[5]\n  {\n   32 \"AB\" 10 \"\xE9\"\n  }\n }\n"
                      " Font2D \"B\"\n {\n InfrequentModify\n DemandLoad\n }\n}\n";
        Fonts db;
        owner.m_fontSpaces.Init(2);
        owner.m_materialSpaces.Init(2);
        Text t(text, nullptr, 0);
        db.Read(&owner, &t.tokenizer);
        const AtGrI2DFont& x = db.items[0];
        CHECK(x.m_format == AtGrI2DFont::kTgaSource);
        CHECK(x.m_kerning == -2.5f && x.m_leading == 3.0f && x.m_scaleX == 0.5f && x.m_scaleY == 2.0f);
        CHECK(x.m_material == 0 && x.m_materials == &db.m_materials);
        CHECK(x.m_flags == (AtFont::kTransparent | AtFont::kColorRectangleCharX));
        CHECK(x.m_count == 5 && x.m_chars != nullptr);
        CHECK(x.m_chars[0].m_code == 32 && x.m_chars[0].m_space == 1);
        CHECK(x.m_chars[1].m_code == 'A' && x.m_chars[2].m_code == 'B');
        CHECK(x.m_chars[3].m_code == 10 && x.m_chars[3].m_newline == 1 && x.m_chars[3].m_space == 0);
        CHECK(x.m_chars[4].m_code == 0xFFE9);  // a byte of a string, sign extended
        CHECK(db.items[1].m_format == AtGrI2DFont::kInfrequentModify && (db.items[1].m_flags & AtFont::kDemandLoad));
        CHECK(db.items[0].loaded == 1 && db.items[1].loaded == 0);

        // A search by halves, on a sorted table made with no file.
        Font f;
        Letters letters("BDFH");
        f.Allocate(&owner, &db, "Made", &letters, AtFont::kOwnsData);
        CHECK(f.m_count == 4 && f.m_flags == AtFont::kOwnsData && f.m_scaleX == 1.0f);
        CHECK(f.FindCharacter('B') == 0 && f.FindCharacter('D') == 1 && f.FindCharacter('H') == 3);
        CHECK(f.FindCharacter('C') == 0 && f.FindCharacter('Z') == 0 && f.FindCharacter(10) == -1);
        Letters none("");
        Font g;
        g.Allocate(&owner, &db, "None", &none, 3);
        CHECK(g.m_count == 0 && g.m_flags == 0 && g.m_chars == nullptr);
    }
    if (g_fail) return 1;
    std::printf("test_imgfont: ok\n");
    return 0;
}
