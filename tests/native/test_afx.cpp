// Host test of the Flash text formats and level files through the real file and text tokenizer code's reader, on any pointer size. Not evidence of a byte match.
#include <cstdio>
#include <cstring>

#include "at/AtFlashLevel.h"
#include "at/AtFlashTextFormat.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

// The seam: see include/at/AtFlashLevel.h.
void AtFlashHeapUser::SetHeap(void* heap) { m_heap = heap; }

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

// The keywords are private to the reader, so a format's fields go through the word table the reader itself sets.
const AtIoKeyword kWords[] = {
    {0x3A, "TextFormat"}, {0x3B, "BoundsRectangle"}, {0x3F, "ReadOnly"}, {0x42, "FontHeight"}, {0x43, "TextColor"}, {0x44, "TextMaxLength"}, {0x45, "Alignment"}, {0x49, "Leading"}, {0x4C, "AlignmentCenter"},
};

int g_heap;

}  // namespace

int main() {
    AtFlashResourceTable::s_defaultHeap = &g_heap;

    char text[] = "TextFormat 2\n{\nBoundsRectangle -2.0 -2.5 70.75 26.5\nReadOnly\nFontHeight 20.0\nTextColor 1 2 3 4\nTextMaxLength 9\nAlignment AlignmentCenter\nLeading 2.5\n}\n";
    Memory stream;
    stream.m_base = text;
    stream.m_cur = text;
    stream.m_limit = text + sizeof(text) - 1;
    stream.m_size = sizeof(text) - 1;
    AtIoTextTokenizer tokenizer;
    tokenizer.SetStream(&stream);
    tokenizer.SetKeywords(kWords, sizeof(kWords) / sizeof(kWords[0]), nullptr);

    AtFlashTextFormat format;
    CHECK(format.m_index == -1);
    CHECK(format.m_fontHeight == 12.0f);
    CHECK(format.m_textMaxLength == -1);
    CHECK(format.m_color[3] == 255);
    AtFlashResource* resource = &format;
    resource->Read(&tokenizer);
    CHECK(format.m_read == 1);
    CHECK(format.m_index == 2);
    CHECK(format.m_bounds[0] == -2.0f);
    CHECK(format.m_bounds[3] == 26.5f);
    CHECK(format.m_readOnly == 1);
    CHECK(format.m_wordWrap == 0);
    CHECK(format.m_fontHeight == 20.0f);
    CHECK(format.m_color[0] == 1);
    CHECK(format.m_color[3] == 4);
    CHECK(format.m_textMaxLength == 9);
    CHECK(format.m_alignment == AtFlashTextFormat::kAlignmentCenter);
    CHECK(format.m_leading == 2.5f);
    format.Reset();
    CHECK(format.m_read == 0);
    CHECK(format.m_fontHeight == 12.0f);

    stream.m_base = nullptr;
    stream.m_cur = nullptr;
    stream.m_limit = nullptr;
    tokenizer.SetStream(nullptr);

    {
        AtFlashTextFormatTable table;
        table.m_initialised = 1;
        table.m_loaded = 1;
        table.Allocate(3);
        CHECK(table.m_itemCount == 3);
        CHECK(table.m_items[2] == static_cast<AtFlashTextFormat*>(table.m_items[0]) + 2);
        CHECK(static_cast<AtFlashTextFormat*>(table.m_items[1])->m_heap == &g_heap);
        table.m_items[0]->m_references = 2;
        table.Release(0);
        CHECK(table.m_items[0]->m_references == 1);
        table.ReleaseFormats();
        CHECK(table.m_items == nullptr);
        CHECK(std::memcmp(table.m_name, "NotInitd", 8) == 0);
        CHECK(table.m_loaded == 0);
    }

    if (g_fail == 0) std::printf("test_afx: passed\n");
    return g_fail != 0;
}
