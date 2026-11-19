// How the game reads and writes a registry's values once they are loaded: the value classes' slots 4 to 9 that loading does not reach, the copies
// a registry made from another takes, and the string held in characters of the caller's that the .ADB tables use. See include/at/AtDataRegistry.h.
// All names are inferred.
#include "at/AtDataRegistry.h"

#include "AtArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtCopyName8(char* to, const char* from);
extern "C" void* memcpy(void* to, const void* from, decltype(sizeof(0)) count);
extern "C" void* memmove(void* to, const void* from, decltype(sizeof(0)) count);

namespace {

const int kWillAcceptParentDataWord = 0x54;  // the .ADR word, written after a value that has the flag
const int kNoCeiling = 0x7FFFFFFF;

unsigned int LengthOf(const char* text) {
    unsigned int length = 0;
    while (text[length] != 0) {
        length++;
    }
    return length;
}

// Inline at each use in the original: no heap is reported and then used anyway.
void PushHeap(void* heap) {
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
}

void* NewBlock(void* heap, unsigned int size) {
    PushHeap(heap);
    void* block = operator new(size);
    AtPopHeap();
    if (block == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return block;
}

// strncmp, as the original calls it (0x001267D0); only equality is used.
bool NamesEqual(const char* a, const char* b, unsigned int count) {
    for (unsigned int i = 0; i < count; i++) {
        if (a[i] != b[i]) {
            return false;
        }
        if (a[i] == 0) {
            return true;
        }
    }
    return true;
}

// Each element through the writer, then a space; four arrays share this shape.
template <class T> void WriteInts(AtDataComponent* self, const T* data, AtIoTokenizer* tokenizer) {
    const unsigned int count = self->m_descriptor->m_count;
    for (unsigned int i = 0; i < count; i++) {
        tokenizer->PutInt((int)data[i]);
        tokenizer->PutSpace();
    }
    self->AtDataComponent::Write(tokenizer);
}

// Element index taken out, the rest moved down, and one fewer kept. Nothing is moved for the last or past it, and one fewer is kept whatever the index.
template <class T> void RemoveElement(AtDataComponent* self, T* data, unsigned int index) {
    const unsigned int count = self->m_descriptor->m_count;
    if (index + 1 < count) {
        memmove(&data[index], &data[index + 1], (count - index - 1) * sizeof(T));
    }
    self->Resize(self->m_descriptor->m_count - 1);
}

// The array of a copy: what it held goes, through Reset, and count elements are copied from the other's. The descriptor's count is not set here.
template <class T> int SetArray(AtDataComponent* self, T*& data, AtDataDescriptor* descriptor, unsigned int count, const T* from) {
    if (data != 0) {
        self->Reset();
    }
    self->AtDataComponent::SetDescriptor(descriptor);
    PushHeap(AtDataComponent::s_heap);
    data = (T*)operator new(count * sizeof(T));
    AtPopHeap();
    if (data == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    if (from != 0) {
        memcpy(data, from, count * sizeof(T));
    }
    return 0;
}

}  // namespace

// ---- text ---------------------------------------------------------------------------------------------------------------

void AtText::CopyFrom(AtText* other) {
    unsigned int length = other->m_end - other->m_start;
    if (length >= m_capacity) {
        length = m_capacity;
    }
    for (unsigned int i = 0; i < length; i++) {
        SetChar(i, other->CharAt(i));
    }
    // At the capacity if the other filled it: one past, as SetNarrow.
    SetChar(length, 0);
    m_end = length;
    m_start = 0;
}

// ---- no type: the stubs in the original's vftable -----------------------------------------------------------------------

int AtDataComponent::Resize(unsigned int) {
    return 0;
}

void* AtDataComponent::ElementAt(unsigned int) {
    return 0;
}

void AtDataComponent::SetElement(const void*, unsigned int, unsigned int) {
}

int AtDataComponent::Equals(const AtDataComponent*) {
    return 0;
}

void AtDataComponent::RemoveAt(unsigned int) {
}

void AtDataComponent::Write(AtIoTokenizer* tokenizer) {
    if (m_descriptor->m_flags & AtDataDescriptor::kAcceptsParentData) {
        tokenizer->PutSpace();
        tokenizer->PutKeyword(kWillAcceptParentDataWord);
    }
}

// ---- one value ----------------------------------------------------------------------------------------------------------

// Each one-value class answers its value whatever the index, and sets it from the first of what it is given, whatever the index and count.
void* AtDataBoolean::ElementAt(unsigned int) {
    return &m_value;
}

void AtDataBoolean::SetElement(const void* from, unsigned int, unsigned int) {
    m_value = *(const int*)from;
}

void AtDataBoolean::Write(AtIoTokenizer* tokenizer) {
    tokenizer->PutInt(m_value);
    AtDataComponent::Write(tokenizer);
}

int AtDataBoolean::Set(AtDataDescriptor* descriptor, int value) {
    AtDataComponent::SetDescriptor(descriptor);
    m_value = value;
    return 0;
}

void* AtDataInt::ElementAt(unsigned int) {
    return &m_value;
}

void AtDataInt::SetElement(const void* from, unsigned int, unsigned int) {
    m_value = *(const int*)from;
}

void AtDataInt::Write(AtIoTokenizer* tokenizer) {
    tokenizer->PutInt(m_value);
    AtDataComponent::Write(tokenizer);
}

int AtDataInt::Set(AtDataDescriptor* descriptor, int value) {
    AtDataComponent::SetDescriptor(descriptor);
    m_value = value;
    return 0;
}

void* AtDataReal::ElementAt(unsigned int) {
    return &m_value;
}

void AtDataReal::SetElement(const void* from, unsigned int, unsigned int) {
    // The word is copied as it is; the original's is folded with the integers'.
    *(unsigned int*)&m_value = *(const unsigned int*)from;
}

// Equal as reals: a NaN equals nothing, and the two zeroes are equal.
int AtDataReal::Equals(const AtDataComponent* other) {
    return m_value == static_cast<const AtDataReal*>(other)->m_value ? 1 : 0;
}

void AtDataReal::Write(AtIoTokenizer* tokenizer) {
    tokenizer->PutReal(m_value);
    AtDataComponent::Write(tokenizer);
}

int AtDataReal::Set(AtDataDescriptor* descriptor, float value) {
    AtDataComponent::SetDescriptor(descriptor);
    m_value = value;
    return 0;
}

void* AtDataShortName::ElementAt(unsigned int) {
    return m_value;
}

void AtDataShortName::SetElement(const void* from, unsigned int, unsigned int) {
    const unsigned int* words = (const unsigned int*)from;
    ((unsigned int*)m_value)[0] = words[0];
    ((unsigned int*)m_value)[1] = words[1];
}

// Up to eight characters or a terminator, through copies of both on the stack.
int AtDataShortName::Equals(const AtDataComponent* other) {
    char mine[8];
    char theirs[8];
    AtCopyName8(mine, m_value);
    AtCopyName8(theirs, static_cast<const AtDataShortName*>(other)->m_value);
    return NamesEqual(mine, theirs, 8) ? 1 : 0;
}

void AtDataShortName::Write(AtIoTokenizer* tokenizer) {
    tokenizer->PutName(m_value);
    AtDataComponent::Write(tokenizer);
}

int AtDataShortName::Set(AtDataDescriptor* descriptor, const char* name) {
    AtDataComponent::SetDescriptor(descriptor);
    const unsigned int* words = (const unsigned int*)name;
    ((unsigned int*)m_value)[0] = words[0];
    ((unsigned int*)m_value)[1] = words[1];
    return 0;
}

// ---- the string ---------------------------------------------------------------------------------------------------------

// Compares what slot 5 answers for each, which for this class is null: texts of different lengths are unequal, two empty ones are equal, and two
// others of one length read from null and crash, as in the original.
int AtDataString::Equals(const AtDataComponent* other) {
    const char* mine = (const char*)ElementAt(0);
    AtDataString* that = (AtDataString*)other;
    const char* theirs = (const char*)that->ElementAt(0);
    const AtText& a = m_unicode != 0 ? (const AtText&)m_wide : (const AtText&)m_narrow;
    const AtText& b = that->m_unicode != 0 ? (const AtText&)that->m_wide : (const AtText&)that->m_narrow;
    if (a.m_end - a.m_start != b.m_end - b.m_start) {
        return 0;
    }
    const unsigned int length = a.m_end - a.m_start;
    for (unsigned int i = 0; i < length; i++) {
        if (mine[i] != theirs[i]) {
            return 0;
        }
    }
    return 1;
}

void AtDataString::Write(AtIoTokenizer* tokenizer) {
    AtText& text = m_unicode != 0 ? (AtText&)m_wide : (AtText&)m_narrow;
    const unsigned int length = text.m_end - text.m_start + 1;
    char* narrow = (char*)NewBlock(s_heap, length + 1);
    text.CopyNarrow(narrow, length);
    tokenizer->PutString(narrow);
    operator delete(narrow);
    AtDataComponent::Write(tokenizer);
}

// Always wide, whatever the other's text was: room for the other's capacity, or the ceiling if that is more.
int AtDataString::CopyText(AtDataDescriptor* descriptor, AtText* other) {
    const AtText& active = m_unicode != 0 ? (const AtText&)m_wide : (const AtText&)m_narrow;
    if (active.m_capacity > 0) {
        Reset();
    }
    AtDataComponent::SetDescriptor(descriptor);
    int floor;
    int ceiling;
    m_descriptor->GetIntBounds(&floor, &ceiling);
    unsigned int room = other->m_capacity;
    if (ceiling != kNoCeiling && (unsigned int)ceiling > room) {
        room = (unsigned int)ceiling;
    }
    unsigned short* chars = (unsigned short*)NewBlock(s_heap, room * 2);
    m_chars = chars;
    m_wide.Init(chars, room);
    m_wide.CopyFrom(other);
    m_unicode = 1;
    return 0;
}

// Lets go of the characters without freeing them: they are the caller's. The base's fields stay.
void AtDataString::Forget() {
    if (m_narrow.m_capacity > 0) {
        m_narrow.Release();
    }
    if (m_wide.m_capacity > 0) {
        m_wide.Release();
    }
    m_unicode = 0;
    m_chars = 0;
}

// Narrow text in the caller's characters, read from the tokenizer. m_chars is not set.
int AtDataString::BindAndRead(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer, char* characters, unsigned int size) {
    const AtText& active = m_unicode != 0 ? (const AtText&)m_wide : (const AtText&)m_narrow;
    if (active.m_capacity > 0) {
        Reset();
    }
    AtDataComponent::SetDescriptor(descriptor);
    const char* text = tokenizer->ExpectString();
    m_narrow.Init(characters, size);
    m_narrow.SetNarrow(text);
    m_unicode = 0;
    return 0;
}

// The caller's characters as they are. Nothing it held before is let go.
int AtDataString::Bind(AtDataDescriptor* descriptor, char* characters, unsigned int size) {
    AtDataComponent::SetDescriptor(descriptor);
    m_narrow.Init(characters, size);
    m_unicode = 0;
    return 0;
}

// ---- arrays -------------------------------------------------------------------------------------------------------------

void* AtDataBooleanArray::ElementAt(unsigned int index) {
    return &m_data[index];
}

void AtDataBooleanArray::SetElement(const void* from, unsigned int index, unsigned int count) {
    memcpy(&m_data[index], from, count * sizeof(int));
}

void AtDataBooleanArray::RemoveAt(unsigned int index) {
    RemoveElement(this, m_data, index);
}

void AtDataBooleanArray::Write(AtIoTokenizer* tokenizer) {
    WriteInts(this, m_data, tokenizer);
}

int AtDataBooleanArray::Copy(AtDataDescriptor* descriptor, const AtDataBooleanArray* other) {
    return SetData(descriptor, other->m_descriptor->m_count, other->m_data);
}

int AtDataBooleanArray::SetData(AtDataDescriptor* descriptor, unsigned int count, const int* data) {
    return SetArray(this, m_data, descriptor, count, data);
}

void* AtDataByteArray::ElementAt(unsigned int index) {
    return &m_data[index];
}

void AtDataByteArray::SetElement(const void* from, unsigned int index, unsigned int count) {
    memcpy(&m_data[index], from, count);
}

void AtDataByteArray::RemoveAt(unsigned int index) {
    RemoveElement(this, m_data, index);
}

void AtDataByteArray::Write(AtIoTokenizer* tokenizer) {
    WriteInts(this, m_data, tokenizer);
}

int AtDataByteArray::Copy(AtDataDescriptor* descriptor, const AtDataByteArray* other) {
    return SetData(descriptor, other->m_descriptor->m_count, other->m_data);
}

int AtDataByteArray::SetData(AtDataDescriptor* descriptor, unsigned int count, const unsigned char* data) {
    return SetArray(this, m_data, descriptor, count, data);
}

void* AtDataIntArray::ElementAt(unsigned int index) {
    return &m_data[index];
}

void AtDataIntArray::SetElement(const void* from, unsigned int index, unsigned int count) {
    memcpy(&m_data[index], from, count * sizeof(int));
}

void AtDataIntArray::RemoveAt(unsigned int index) {
    RemoveElement(this, m_data, index);
}

void AtDataIntArray::Write(AtIoTokenizer* tokenizer) {
    WriteInts(this, m_data, tokenizer);
}

int AtDataIntArray::Copy(AtDataDescriptor* descriptor, const AtDataIntArray* other) {
    return SetData(descriptor, other->m_descriptor->m_count, other->m_data);
}

int AtDataIntArray::SetData(AtDataDescriptor* descriptor, unsigned int count, const int* data) {
    return SetArray(this, m_data, descriptor, count, data);
}

void* AtDataRealArray::ElementAt(unsigned int index) {
    return &m_data[index];
}

void AtDataRealArray::SetElement(const void* from, unsigned int index, unsigned int count) {
    memcpy(&m_data[index], from, count * sizeof(float));
}

void AtDataRealArray::RemoveAt(unsigned int index) {
    RemoveElement(this, m_data, index);
}

void AtDataRealArray::Write(AtIoTokenizer* tokenizer) {
    const unsigned int count = m_descriptor->m_count;
    for (unsigned int i = 0; i < count; i++) {
        tokenizer->PutReal(m_data[i]);
        tokenizer->PutSpace();
    }
    AtDataComponent::Write(tokenizer);
}

int AtDataRealArray::Copy(AtDataDescriptor* descriptor, const AtDataRealArray* other) {
    return SetData(descriptor, other->m_descriptor->m_count, other->m_data);
}

int AtDataRealArray::SetData(AtDataDescriptor* descriptor, unsigned int count, const float* data) {
    return SetArray(this, m_data, descriptor, count, data);
}

void* AtDataShortNameArray::ElementAt(unsigned int index) {
    return m_data[index];
}

void AtDataShortNameArray::SetElement(const void* from, unsigned int index, unsigned int count) {
    memcpy(m_data[index], from, count * sizeof m_data[0]);
}

void AtDataShortNameArray::RemoveAt(unsigned int index) {
    RemoveElement(this, m_data, index);
}

void AtDataShortNameArray::Write(AtIoTokenizer* tokenizer) {
    const unsigned int count = m_descriptor->m_count;
    for (unsigned int i = 0; i < count; i++) {
        tokenizer->PutName(m_data[i]);
        tokenizer->PutSpace();
    }
    AtDataComponent::Write(tokenizer);
}

int AtDataShortNameArray::Copy(AtDataDescriptor* descriptor, const AtDataShortNameArray* other) {
    return SetData(descriptor, other->m_descriptor->m_count, other->m_data);
}

int AtDataShortNameArray::SetData(AtDataDescriptor* descriptor, unsigned int count, char (*data)[8]) {
    return SetArray(this, m_data, descriptor, count, data);
}

// Element index's characters.
void* AtDataStringArray::ElementAt(unsigned int index) {
    return m_chars[index];
}

// The text's characters, without its terminator, over the list of pointers from element index on, not into any element: as the original does.
// The count is not used.
void AtDataStringArray::SetElement(const void* from, unsigned int index, unsigned int) {
    memcpy(&m_chars[index], from, LengthOf((const char*)from));
}

// The removed element's characters are not freed.
void AtDataStringArray::RemoveAt(unsigned int index) {
    const unsigned int count = m_descriptor->m_count;
    if (index + 1 < count) {
        memmove(&m_texts[index], &m_texts[index + 1], (count - index - 1) * sizeof(AtTextUnicode));
        const unsigned int now = m_descriptor->m_count;
        memmove(&m_chars[index], &m_chars[index + 1], (now - index - 1) * sizeof(unsigned short*));
    }
    Resize(m_descriptor->m_count - 1);
}

void AtDataStringArray::Write(AtIoTokenizer* tokenizer) {
    const unsigned int count = m_descriptor->m_count;
    for (unsigned int i = 0; i < count; i++) {
        AtTextUnicode& text = m_texts[i];
        const unsigned int length = text.m_end - text.m_start + 1;
        char* narrow = (char*)NewBlock(s_heap, length);
        text.CopyNarrow(narrow, length);
        tokenizer->PutString(narrow);
        tokenizer->PutSpace();
        operator delete(narrow);
    }
    AtDataComponent::Write(tokenizer);
}

// The other's texts, narrowed one by one into a block as long as the longest text's room. The first text's room is read even when there is none.
int AtDataStringArray::Copy(AtDataDescriptor* descriptor, const AtDataStringArray* other) {
    const unsigned int count = other->m_descriptor->m_count;
    const AtTextUnicode* texts = other->m_texts;
    unsigned int longest = texts[0].m_capacity;
    for (unsigned int i = 1; i < count; i++) {
        if (texts[i].m_capacity > longest) {
            longest = texts[i].m_capacity;
        }
    }
    char* narrow = (char*)NewBlock(AtIoTokenFile::s_heap, longest);
    Make(descriptor, longest, count);
    for (unsigned int i = 0; i < count; i++) {
        other->m_texts[i].CopyNarrow(narrow, longest);
        SetText(i, narrow);
    }
    operator delete(narrow);
    return 0;
}

// count empty texts of size characters each. SetText afterwards makes each a block of its own and the ones made here are lost.
int AtDataStringArray::Make(AtDataDescriptor* descriptor, unsigned int size, unsigned int count) {
    if (m_texts != 0) {
        Reset();
    }
    AtDataComponent::SetDescriptor(descriptor);
    PushHeap(s_heap);
    AtTextUnicode* texts = (AtTextUnicode*)operator new(count * sizeof(AtTextUnicode));
    if (texts != 0 && (int)(count - 1) >= 0) {
        for (unsigned int i = 0; i < count; i++) {
            new (texts + i, AtPlacement()) AtTextUnicode;
        }
    }
    m_texts = texts;
    AtPopHeap();
    if (m_texts == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_chars = (unsigned short**)NewBlock(s_heap, count * sizeof(unsigned short*));
    for (unsigned int i = 0; i < count; i++) {
        PushHeap(s_heap);
        m_chars[i] = (unsigned short*)operator new(size * 2);
        AtPopHeap();
        if (m_chars[i] == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        m_texts[i].Init(m_chars[i], size);
    }
    return 0;
}
