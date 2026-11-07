// What a file's shaders are held in by name. Read from .AGS files. Original address cluster 0x0002B1B0-0x0002B41D. The class is abstract: slots 2 and 3 are filled by a concrete database, in the
// graphics layer, which is not recovered. Names are inferred.
#pragma once

#include "at/AtShader.h"

// Vftable 0x00273D20.
class AtShaderDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 56 };

    AtShaderDatabase();                        // 0x0002B1C0
    ~AtShaderDatabase() override;              // slot 0; 0x0002B400 deleting, 0x0002B3A0 complete
    void Clear() override;                     // slot 1, 0x0002B380

    virtual void OnReady() = 0;                // slot 2: m_count is known, and the concrete database makes room for that many shaders
    virtual AtShader* ShaderAt(unsigned int index) = 0;  // slot 3

    void Load(AtMaterialOwner* owner, const char* name, int unused);  // 0x0002B2E0 name + ".AGS"
    void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer);      // 0x0002B1E0

    static void SetHeap(void* heap);           // 0x0002B1B0 only the graphics layer reads it

    unsigned int m_count;                      // +0x14

    static void* s_heap;                       // 0x002C6380
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B0CC0: for the whole file
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtShaderDatabase, m_count) == 0x14, "AtShaderDatabase::m_count");
static_assert(sizeof(AtShaderDatabase) == 0x18, "AtShaderDatabase");
#pragma pop_macro("offsetof")
#endif
