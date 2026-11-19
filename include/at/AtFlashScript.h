// The Flash user interface's scripts: AScript\name.AFA ("Atlas Flash Action Script File" by its own first line), blocks of action bytecode, each instruction given as its length and its bytes; with their strings in
// AScript\name.AIS. Original address clusters 0x000133B0-0x0001386F, 0x0001C900-0x0001CB2F, 0x0001CC80-0x0001CCFF, 0x0001EF00-0x0001EF63, 0x00020950-0x00020D10 and 0x0002D1A0-0x0002D64F.
// The keywords are the original's, from table 0x002711D0, and so are the names AtFpActionScriptVM, from its own messages ("AtFpActionScriptVM::vLoadMovie Illegal file name requested" at 0x002725B4); every other name
// is inferred.
//
// What is recovered is what loading needs: the reader, the scripts' storage, and the machine's state and its stack of values. The machine itself, which runs the instructions (0x0001CB30 to 0x0001EF70 and beyond), is not:
// nothing that loads reaches it.
#pragma once

#include "at/AtFlashShape.h"
#include "at/AtText.h"

// One value on the machine's stack: 0x2C bytes. Vftable 0x00272964.
class AtFpActionScriptValue {
public:
    enum { kTypeString = 0, kTypeNone = 3 };

    AtFpActionScriptValue();           // 0x00020A50
    ~AtFpActionScriptValue();          // 0x00020CC0 not virtual
    virtual void Reserve(unsigned int size);      // slot 0, 0x00020AD0 at least 0x20 bytes of text, emptied; a larger request makes a new block
    virtual void MoveTo(AtFpActionScriptValue* to);  // slot 1, 0x000209B0 what to held is overwritten, not freed
    virtual void SetString(char* text, unsigned int capacity);  // slot 2, 0x00020A10 takes the block
    virtual void Reset();              // slot 3, 0x00020A80

    int m_type;                        // +0x04
    char* m_text;                      // +0x08
    unsigned int m_capacity;           // +0x0C
    int m_unknown10;                   // +0x10
    int m_unknown14;                   // +0x14
    int m_unknown18;                   // +0x18
    int m_unknown1C;                   // +0x1C
    int m_unknown20;                   // +0x20
    void* m_unknown24;                 // +0x24 freed by Reset
    void* m_heap;                      // +0x28 null means s_heap

    static void* s_heap;               // 0x002C6318
};

// The machine's stack of values: 0x14 bytes, no vftable.
class AtFpActionScriptStack {
public:
    AtFpActionScriptStack();           // 0x0002D270
    ~AtFpActionScriptStack();          // 0x0002D570 only a jump to Free
    void Free();                       // 0x0002D290 nothing unless allocated; the heap is forgotten too
    void Allocate(unsigned int count); // 0x0002D580 frees first if allocated

    static void SetHeap(void* heap);   // 0x0002D1A0

    int m_allocated;                   // +0x00
    void* m_heap;                      // +0x04 null means s_heap
    AtFpActionScriptValue* m_values;   // +0x08
    int m_top;                         // +0x0C -1 when empty
    unsigned int m_capacity;           // +0x10

    static void* s_heap;               // 0x002C6390
};

// The machine's state: 0x20 bytes, no vftable.
class AtFpActionScriptVM {
public:
    AtFpActionScriptVM();              // 0x0001CC80
    ~AtFpActionScriptVM();             // 0x0001EF00
    void Clear();                      // 0x0001CCA0
    void Init(AtFlashApplication* application, AtTextTable* strings);  // 0x0001CCC0 a stack of 30

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
