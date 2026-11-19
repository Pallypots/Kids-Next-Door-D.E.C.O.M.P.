// The Flash user interface's scripts: AScript\name.AFA ("Atlas Flash Action Script File" by its own first line), blocks of action bytecode, each instruction given as its length and its bytes; with their strings in
// AScript\name.AIS. Original address clusters 0x000133B0-0x0001386F, 0x0001C900-0x0001CB2F, 0x0001CC80-0x0001CCFF, 0x0001EF00-0x0001EF63, 0x00020950-0x00020D10 and 0x0002D1A0-0x0002D64F.
// The keywords are the original's, from table 0x002711D0, and so are the names AtFpActionScriptVM, from its own messages ("AtFpActionScriptVM::vLoadMovie Illegal file name requested" at 0x002725B4); every other name
// is inferred.
//
// What is recovered: the reader, the scripts' storage, the machine's state and its stack of values (at/aft), and the machine itself, its dispatch loop and every handler (at/asvm, 0x0001CB30-0x0001F2F3 and
// 0x0002D1B0-0x0002D81B). What the handlers call out to, the display manager and the rest, is not: include/at/AtFpDisplay.h.
#pragma once

#include "at/AtFlashShape.h"
#include "at/AtText.h"

// One value on the machine's stack: 0x2C bytes. Vftable 0x00272964.
// A value is text, a real or an integer (or undefined), and keeps the other forms it has been asked for: m_haveTextInt says the text and the integer agree, and so on for each pair. Which flag means what
// depends on the type: a text's integer is valid when m_haveTextInt is set, an integer's text likewise, and a real's integer when m_haveIntReal is.
class AtFpActionScriptValue {
public:
    enum { kTypeString = 0, kTypeReal = 1, kTypeNone = 3, kTypeInt = 7 };

    AtFpActionScriptValue();           // 0x00020A50
    ~AtFpActionScriptValue();          // 0x00020CC0 not virtual
    virtual void Reserve(unsigned int size);      // slot 0, 0x00020AD0 at least 0x20 bytes of text, emptied; a larger request makes a new block
    virtual void MoveTo(AtFpActionScriptValue* to);  // slot 1, 0x000209B0 what to held is overwritten, not freed
    virtual void SetString(char* text, unsigned int capacity);  // slot 2, 0x00020A10 takes the block
    virtual void Reset();              // slot 3, 0x00020A80

    // src/at/AtFlashScriptVM.cpp. Names inferred.
    void Clear();                      // 0x00020740 every field, without freeing: undefined
    void SetInt(const int* value);     // 0x00020770 the text is kept, not freed, and no longer agrees
    void SetReal(const float* value);  // 0x00020790 an integral real becomes an integer
    float ToReal();                    // 0x000207E0 a text by atof, once
    int ToInt();                       // 0x00020830 a real truncated, a text by atoi, once
    char* ToText();                    // 0x00020880 a real by "%g", an integer in decimal, into room for 0x20; undefined is ""
    unsigned int Length();             // 0x00020900 0 for undefined
    int IsInteger();                   // 0x00020930 also for undefined, and for a text of digits with a leading '-', the empty text and "-" among them
    AtFpActionScriptValue* CopyFrom(const AtFpActionScriptValue* from);  // 0x0002D200 the text only if from has one
    void SetText(AtText* text);        // 0x00020C00 a null is undefined
    void SetText(const char* text);    // 0x00020C60 a null is undefined

    // Inline in the original wherever a handler wants one form.
    float Real() { return m_type == kTypeReal ? m_real : ToReal(); }
    int Int() { return m_type == kTypeInt ? m_int : ToInt(); }
    char* Text() { return m_type == kTypeString ? m_text : ToText(); }

    int m_type;                        // +0x04
    char* m_text;                      // +0x08
    unsigned int m_capacity;           // +0x0C
    // Two names for each of these: the second, given with the interpreter, says what it is; the first is kept for what used it before.
    union { int m_unknown10; int m_int; };            // +0x10
    union { int m_unknown14; float m_real; };         // +0x14
    union { int m_unknown18; int m_haveTextInt; };    // +0x18
    union { int m_unknown1C; int m_haveTextReal; };   // +0x1C
    union { int m_unknown20; int m_haveIntReal; };    // +0x20
    void* m_unknown24;                 // +0x24 freed by Reset
    void* m_heap;                      // +0x28 null means s_heap

    static void* s_heap;               // 0x002C6318
};

// A value on a handler's own stack: 0x12C bytes, its text in the 0xFF-byte buffer it carries when that will do. Vftable 0x00274878. Name inferred. Its blocks come from AtIoTokenFile::s_heap, not m_heap.
class AtFpActionScriptLocalValue : public AtFpActionScriptValue {
public:
    AtFpActionScriptLocalValue();      // 0x0002D650
    ~AtFpActionScriptLocalValue();     // 0x0002D7C0 not virtual
    void Reserve(unsigned int size) override;      // slot 0, 0x0002D6C0
    void MoveTo(AtFpActionScriptValue* to) override;  // slot 1, 0x0002D820 a copy while in the buffer
    void SetString(char* text, unsigned int capacity) override;  // slot 2, 0x0002D780
    void Reset() override;             // slot 3, 0x0002D6A0

    char m_buffer[0x100];              // +0x2C
};

// The machine's stack of values: 0x14 bytes, no vftable.
class AtFpActionScriptStack {
public:
    AtFpActionScriptStack();           // 0x0002D270
    ~AtFpActionScriptStack();          // 0x0002D570 only a jump to Free
    void Free();                       // 0x0002D290 nothing unless allocated; the heap is forgotten too
    void Allocate(unsigned int count); // 0x0002D580 frees first if allocated

    static void SetHeap(void* heap);   // 0x0002D1A0

    // src/at/AtFlashScriptVM.cpp. Names inferred. Each push gives the new top this stack's heap and returns 0, doing nothing, when the stack is full.
    int Pop(AtFpActionScriptValue* to);    // 0x0002D1B0 0 when empty, and to is left as it was
    int Discard();                         // 0x0002D1E0 the top keeps what it held
    int PushInt(int value);                // 0x0002D2F0
    int PushReal(float value);             // 0x0002D340
    int PushText(const char* text);        // 0x0002D390
    int PushText(AtText* text);            // 0x0002D3E0
    int PushUndefined();                   // 0x0002D430 only the type is set
    int PushString(char* text, unsigned int capacity);  // 0x0002D480 takes the block
    int PushCopy(const AtFpActionScriptValue* from);    // 0x0002D4D0
    int PushFrom(AtFpActionScriptValue* from);          // 0x0002D520 from's MoveTo

    int m_allocated;                   // +0x00
    void* m_heap;                      // +0x04 null means s_heap
    AtFpActionScriptValue* m_values;   // +0x08
    int m_top;                         // +0x0C -1 when empty
    unsigned int m_capacity;           // +0x10

    static void* s_heap;               // 0x002C6390
};

class AtFpScript;

// The machine: 0x20 bytes, no vftable. It runs one script block at a time on its stack, calling out to the application's display manager and remote function calls (include/at/AtFpDisplay.h).
// The handlers are src/at/AtFlashScriptVM.cpp. Each is named for the Flash action it serves, by the action codes Run dispatches on (the SWF numbering); the names are inferred, but for LoadMovie and
// ActionGetURL2, which the original's messages give as vLoadMovie and vActionGetURL2.
class AtFpActionScriptVM {
public:
    AtFpActionScriptVM();              // 0x0001CC80
    ~AtFpActionScriptVM();             // 0x0001EF00
    void Clear();                      // 0x0001CCA0
    void Init(AtFlashApplication* application, AtTextTable* strings);  // 0x0001CCC0 a stack of 30

    // 0x0001F020. From the first instruction to an End (code 0), or to a Call that says stop. Nothing bounds the instructions but the End; Jump and If go by instruction, not by byte, and only below m_read.
    void Run(AtFpScript* script);
    void ActionIf(unsigned short target, unsigned int* index, AtFpScript* script);  // 0x0001EF70 by the popped value's integer
    int ActionCall();                  // 0x0001E520 what ExecuteScripts returns
    void LoadMovie(const char* url, const char* target);  // 0x0001CB30 drops the last four characters of url, or reports a name of one to four
    void ActionGetTime();              // 0x0001CBA0
    void ActionWaitForFrame2(unsigned char skip);   // 0x0001CC10 pops and does nothing else
    void ActionGotoLabel(const unsigned char* data);  // 0x0001CD10 then stops
    void ActionGotoFrame2(unsigned char play);      // 0x0001CDF0
    void ActionSetTarget(const unsigned char* data);  // 0x0001CEA0
    void ActionSetTarget2();           // 0x0001CFB0
    void ActionPush(unsigned int length, const unsigned char* data);  // 0x0001D080 a kind it does not know, or a string not found, never moves on
    void ActionGetVariable();          // 0x0001D1C0
    void ActionSetVariable();          // 0x0001D290
    void ActionLess();                 // 0x0001D360
    void ActionEquals();               // 0x0001D460
    void ActionNot();                  // 0x0001D560
    void ActionAnd();                  // 0x0001D610
    void ActionOr();                   // 0x0001D720
    void ActionAdd();                  // 0x0001D830
    void ActionSubtract();             // 0x0001D920
    void ActionDivide();               // 0x0001DA20
    void ActionMultiply();             // 0x0001DB20
    void ActionStringAdd();            // 0x0001DC10
    void ActionCharToAscii();          // 0x0001DDC0
    void ActionAsciiToChar();          // 0x0001DE70
    void ActionToInteger();            // 0x0001DF10
    void ActionStringEquals();         // 0x0001DFA0
    void ActionStringLess();           // 0x0001E0F0
    void ActionStringLength();         // 0x0001E240
    void ActionStringExtract();        // 0x0001E2F0
    void ActionGetURL2(unsigned char method);  // 0x0001E5F0
    void ActionRandomNumber();         // 0x0001E8D0
    void ActionTrace();                // 0x0001EA20
    void ActionGetProperty();          // 0x0001EB00
    void ActionSetProperty();          // 0x0001EC30
    void ActionCloneSprite();          // 0x0001ED60 pops three, converts them, and does nothing
    void ActionRemoveSprite();         // 0x0001EE80 pops one, converts it, and does nothing

    AtTextTable* m_strings;            // +0x00
    AtFlashApplication* m_application; // +0x04
    AtFpActionScriptStack m_stack;     // +0x08
    void* m_heap;                      // +0x1C null means s_heap

    static void* s_heap;               // 0x002C62EC
};

// One script block: 0x34 bytes. Vftable 0x002725B0, AtFlashResource's one slot.
class AtFpScript : public AtFlashResource {
public:
    AtFpScript();                      // 0x0001C900
    ~AtFpScript();                     // 0x0001CAE0 not virtual
    void Free();                       // 0x0001C920
    void Init(int id, unsigned int instructions, unsigned int size);  // 0x0001C980 room for that many instructions and bytes
    // 0x0001CA40. "{ length bytes... ... }": each instruction's bytes go after the last's. Neither the count nor the size made room for is checked.
    void ReadInstructions(AtIoTokenizer* tokenizer);

    unsigned char** m_instructions;    // +0x1C where each instruction starts in m_bytes
    unsigned char* m_bytes;            // +0x20
    unsigned int m_instructionCount;   // +0x24 what Init made room for
    unsigned int m_read;               // +0x28 instructions read
    int m_id;                          // +0x2C ScriptID
    void* m_heap;                      // +0x30 null means s_heap

    static void* s_heap;               // 0x002C62E8
};

// Vftable 0x0027125C. AScript\name.AFA and its strings. The base's list and groups are not used: the scripts are in a list of their own.
class AtFlashScriptTable : public AtFlashResourceTable {
public:
    AtFlashScriptTable();              // 0x000133B0
    ~AtFlashScriptTable() override;    // slot 1; 0x00013850 deleting, 0x000137E0 complete

    void ClearScripts();               // 0x00013420 nothing unless loaded; the base is not cleared
    // 0x000134A0. The strings first, then the scripts. The words are taken as they come, not checked: only NameSpace is expected.
    void Load(const char* name, int mode, AtFlashApplication* application);

    int m_loaded;                      // +0x28
    AtFlashApplication* m_application; // +0x2C
    AtFpScript** m_scripts;            // +0x30
    AtTextTable m_strings;             // +0x34
    unsigned int m_count;              // +0x68
    AtFpActionScriptVM m_vm;           // +0x6C
    void* m_heap;                      // +0x8C null means s_defaultHeap

    static void* s_defaultHeap;        // 0x002C60B8
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtFpActionScriptValue, m_heap) == 0x28, "AtFpActionScriptValue::m_heap");
static_assert(sizeof(AtFpActionScriptValue) == 0x2C, "AtFpActionScriptValue");
static_assert(offsetof(AtFpActionScriptLocalValue, m_buffer) == 0x2C, "AtFpActionScriptLocalValue::m_buffer");
static_assert(sizeof(AtFpActionScriptLocalValue) == 0x12C, "AtFpActionScriptLocalValue");
static_assert(sizeof(AtFpActionScriptStack) == 0x14, "AtFpActionScriptStack");
static_assert(offsetof(AtFpActionScriptVM, m_heap) == 0x1C, "AtFpActionScriptVM::m_heap");
static_assert(sizeof(AtFpActionScriptVM) == 0x20, "AtFpActionScriptVM");
static_assert(offsetof(AtFpScript, m_heap) == 0x30, "AtFpScript::m_heap");
static_assert(sizeof(AtFpScript) == 0x34, "AtFpScript");
static_assert(offsetof(AtFlashScriptTable, m_strings) == 0x34, "AtFlashScriptTable::m_strings");
static_assert(offsetof(AtFlashScriptTable, m_vm) == 0x6C, "AtFlashScriptTable::m_vm");
static_assert(offsetof(AtFlashScriptTable, m_heap) == 0x8C, "AtFlashScriptTable::m_heap");
static_assert(sizeof(AtFlashScriptTable) == 0x90, "AtFlashScriptTable");
#pragma pop_macro("offsetof")
#endif
