// Host test of the Flash texts and scripts on any pointer size: a text and a script block through the real text tokenizer, the action script values, their stack and the machine's state, and the tables' clears.
// Not evidence of a byte match; the differential oracle is tests/oracle/oracle_aft.cpp.
#include <cstdio>
#include <cstring>

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

const AtIoKeyword kTextWords[] = {{0x3A, "Text"}, {0x3B, "String"}, {0x3C, "Font2D"}, {0x3D, "TextFormat"}, {0x3E, "Variable"}};

int g_heap;

}  // namespace

int main() {
    AtFlashResourceTable::s_defaultHeap = &g_heap;
    AtFpActionScriptValue::s_heap = &g_heap;
    AtFpActionScriptStack::s_heap = &g_heap;
    AtFpActionScriptVM::s_heap = &g_heap;
    AtFpScript::s_heap = &g_heap;
    AtFlashScriptTable::s_defaultHeap = &g_heap;
    GmNameMap::s_heap = &g_heap;

    // A text: each word, in another order than the files use.
    {
        char text[] = "Text 5 { Variable \"_level0:Name\" TextFormat 3 Font2D \"F_Stand\" String \"~ST00004\" }";
        Text t(text, kTextWords, 5);
        AtFlashText item;
        CHECK(item.m_format == -1 && item.m_read == 0 && item.m_variable == nullptr);
        AtFlashResource* resource = &item;
        resource->Read(&t.tokenizer);
        CHECK(item.m_read == 1);
        CHECK(item.m_index == 5);
        CHECK(item.m_format == 3);
        CHECK(std::memcmp(item.m_font, "F_Stand", 8) == 0);
        CHECK(std::memcmp(item.m_string, "~ST00004", 8) == 0);
        CHECK(item.m_variable != nullptr && std::strcmp(item.m_variable, "_level0:Name") == 0);
        item.Reset();
        CHECK(item.m_read == 0 && item.m_variable == nullptr && item.m_format == -1 && item.m_font[0] == 0);
    }

    // A script block: instructions one after another in one block of bytes.
    {
        char text[] = "{ 3 150 0 10 2 29 0 1 7 }";
        Text t(text, kTextWords, 0);
        AtFpScript script;
        script.Init(0x53000001, 3, 6);
        CHECK(script.m_id == 0x53000001 && script.m_instructionCount == 3);
        script.ReadInstructions(&t.tokenizer);
        CHECK(script.m_read == 3);
        CHECK(script.m_instructions[0] == script.m_bytes);
        CHECK(script.m_instructions[1] == script.m_bytes + 3);
        CHECK(script.m_instructions[2] == script.m_bytes + 5);
        const unsigned char want[6] = {150, 0, 10, 29, 0, 7};
        CHECK(std::memcmp(script.m_bytes, want, 6) == 0);
        script.Free();
        CHECK(script.m_read == 0 && script.m_bytes == nullptr && script.m_instructions == nullptr);
    }

    // Values: room for text, moved, given a block.
    {
        AtFpActionScriptValue a, b;
        CHECK(a.m_type == AtFpActionScriptValue::kTypeNone);
        a.Reserve(4);
        CHECK(a.m_capacity == 0x20 && a.m_text != nullptr && a.m_text[0] == 0);
        char* kept = a.m_text;
        a.Reserve(0x10);
        CHECK(a.m_text == kept);
        a.Reserve(0x40);
        CHECK(a.m_capacity == 0x40);
        a.m_type = AtFpActionScriptValue::kTypeString;
        a.MoveTo(&b);
        CHECK(b.m_type == AtFpActionScriptValue::kTypeString && b.m_capacity == 0x40 && b.m_text != nullptr);
        CHECK(a.m_type == AtFpActionScriptValue::kTypeNone && a.m_text == nullptr && a.m_capacity == 0);
        char* block = (char*)operator new(8);
        a.SetString(block, 8);
        CHECK(a.m_type == AtFpActionScriptValue::kTypeString && a.m_text == block && a.m_capacity == 8);
        a.Reset();
        CHECK(a.m_text == nullptr && a.m_type == AtFpActionScriptValue::kTypeNone);
    }

    // The machine's state: a stack of 30, freed by Clear.
    {
        AtFpActionScriptVM vm;
        AtTextTable strings;
        AtFlashApplication* application = (AtFlashApplication*)&g_heap;
        vm.Init(application, &strings);
        CHECK(vm.m_application == application && vm.m_strings == &strings);
        CHECK(vm.m_stack.m_allocated == 1 && vm.m_stack.m_capacity == 30 && vm.m_stack.m_top == -1 && vm.m_stack.m_values != nullptr);
        CHECK(vm.m_stack.m_values[29].m_type == AtFpActionScriptValue::kTypeNone);
        CHECK(AtFpActionScriptStack::s_heap == &g_heap);
        vm.Init(application, &strings);
        CHECK(vm.m_stack.m_allocated == 1 && vm.m_stack.m_capacity == 30);
        vm.Clear();
        CHECK(vm.m_stack.m_allocated == 0 && vm.m_stack.m_values == nullptr && vm.m_application == nullptr);
    }

    // The tables, constructed and cleared without a file.
    {
        AtFlashScriptTable scripts;
        CHECK(scripts.m_loaded == 0 && scripts.m_scripts == nullptr && scripts.m_count == 0);
        scripts.m_loaded = 1;
        scripts.m_count = 2;
        scripts.m_scripts = (AtFpScript**)operator new(2 * sizeof(AtFpScript*));
        for (int i = 0; i < 2; i++) {
            scripts.m_scripts[i] = new AtFpScript;
            scripts.m_scripts[i]->Init(i, 1, 1);
        }
        scripts.ClearScripts();
        CHECK(scripts.m_loaded == 0 && scripts.m_scripts == nullptr && scripts.m_count == 0);

        AtFlashTextTable texts;
        CHECK(texts.m_loaded == 0 && texts.m_fonts.m_loaded == 0 && texts.m_formats.m_loaded == 0);
        texts.Init();
        texts.m_itemCount = 3;
        texts.Allocate(3);
        CHECK(texts.m_itemCount == 3 && ((AtFlashText*)texts.m_items[2])->m_heap == &g_heap);
        texts.m_loaded = 1;
        texts.ClearTexts();
        CHECK(texts.m_loaded == 0 && texts.m_items == nullptr);
    }

    if (g_fail) {
        std::printf("test_aft: %d failures\n", g_fail);
        return 1;
    }
    std::printf("test_aft: all passed\n");
    return 0;
}
