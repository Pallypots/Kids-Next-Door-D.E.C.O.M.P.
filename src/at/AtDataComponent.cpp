// The values of a data registry, as far as loading reaches. See include/at/AtDataRegistry.h.
#include "at/AtDataRegistry.h"

#include "AtSceneArray.h"

void AtRaise(int code, int a, int b, int c);
void AtFatal(const char* message, int file, int line, int unused);
void AtPushHeap(void* heap);
void AtPopHeap();
extern "C" int toupper(int c);
extern "C" void* memcpy(void* to, const void* from, decltype(sizeof(0)) count);
extern "C" void* memset(void* to, int value, decltype(sizeof(0)) count);

void* AtDataComponent::s_heap;

namespace {

const int kNoCeiling = 0x7FFFFFFF;

unsigned int LengthOf(const char* text) {
    unsigned int length = 0;
    while (text[length] != 0) {
        length++;
    }
    return length;
}

int EqualIgnoringCase(const char* a, const char* b) {
    for (;; a++, b++) {
        const int x = toupper((unsigned char)*a);
        if (x != toupper((unsigned char)*b)) {
            return 0;
        }
        if (x == 0) {
            return 1;
        }
    }
}

// The original has this inline at each use: no heap is reported and then used, no memory is reported after the pop.
void PushHeap() {
    if (AtDataComponent::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtDataComponent::s_heap);
}

void* NewBlock(unsigned int size) {
    PushHeap();
    void* block = operator new(size);
    AtPopHeap();
    return block;
}

// What the file's text takes: its length, or the ceiling if that is more. The largest int means no ceiling.
unsigned int RoomFor(AtDataDescriptor* descriptor, unsigned int length) {
    int floor;
    int ceiling;
    descriptor->GetIntBounds(&floor, &ceiling);
    if (ceiling != kNoCeiling && (unsigned int)ceiling > length) {
        return (unsigned int)ceiling;
    }
    return length;
}

// A new block for count elements, with what fits of the old ones, and the old block gone. Four arrays share this shape.
template <class T> int ResizeData(AtDataComponent* self, T*& data, unsigned int count) {
    T* grown = (T*)NewBlock(count * sizeof(T));
    if (grown == 0) {
        AtRaise(-4, 0, 0, 0);
        return -4;
    }
    if (data != 0) {
        const unsigned int had = self->m_descriptor->m_count;
        memcpy(grown, data, (count > had ? had : count) * sizeof(T));
        operator delete(data);
    }
    data = grown;
    self->m_descriptor->m_count = count;
    return 0;
}

template <class T> void FreeData(AtDataComponent* self, T*& data) {
    if (data != 0) {
        operator delete(data);
    }
    data = 0;
    self->AtDataComponent::Reset();
}

void NotRecovered(const char* what) {
    AtFatal(what, 0, 0, 0);
}

}  // namespace

void AtText::SetNarrow(const char* text) {
    unsigned int length = LengthOf(text);
    if (length >= m_capacity) {
        length = m_capacity;
    }
    for (unsigned int i = 0; i < length; i++) {
        SetChar(i, (unsigned int)(int)(signed char)text[i]);
    }
    SetChar(length, 0);
    m_end = length;
    m_start = 0;
}

void AtTextAscii::Release() {
    m_data = 0;
    ClearFields();
}

// ---- no type ------------------------------------------------------------------------------------------------------------

void AtDataComponent::SetHeap(void* heap) {
    s_heap = heap;
}

AtDataComponent::AtDataComponent() {
    m_descriptor = 0;
    m_registry = 0;
}

AtDataComponent::~AtDataComponent() {
}

int AtDataComponent::Read(AtDataDescriptor* descriptor, AtIoTokenizer*) {
    m_descriptor = descriptor;
    return 0;
}

int AtDataComponent::SetDescriptor(AtDataDescriptor* descriptor) {
    m_descriptor = descriptor;
    return 0;
}

void AtDataComponent::Reset() {
    m_descriptor = 0;
    m_registry = 0;
}

int AtDataComponent::Resize(unsigned int) {
    NotRecovered("AtDataComponent::Resize: slot 4 of this class is not recovered");
    return 0;
}

void* AtDataComponent::ElementAt(unsigned int) {
    NotRecovered("AtDataComponent::ElementAt: slot 5 is not recovered");
    return 0;
}

void AtDataComponent::SetElement(const void*, unsigned int, unsigned int) {
    NotRecovered("AtDataComponent::SetElement: slot 6 is not recovered");
}

int AtDataComponent::Equals(const AtDataComponent*) {
    NotRecovered("AtDataComponent::Equals: slot 7 is not recovered");
    return 0;
}

void AtDataComponent::RemoveAt(unsigned int) {
    NotRecovered("AtDataComponent::RemoveAt: slot 8 is not recovered");
}

void AtDataComponent::Write(AtIoTokenizer*) {
    NotRecovered("AtDataComponent::Write: slot 9 is not recovered");
}

// ---- one value ----------------------------------------------------------------------------------------------------------

AtDataBoolean::AtDataBoolean() {
    m_value = 0;
}

AtDataBoolean::~AtDataBoolean() {
}

int AtDataBoolean::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {
    SetDescriptor(descriptor);
    m_value = tokenizer->ExpectInt();
    return 0;
}

void AtDataBoolean::Reset() {
    AtDataComponent::Reset();
}

AtDataInt::AtDataInt() {
}

AtDataInt::~AtDataInt() {
}

int AtDataInt::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {
    SetDescriptor(descriptor);
    m_value = tokenizer->ExpectInt();
    return 0;
}

void AtDataInt::Reset() {
    AtDataComponent::Reset();
}

AtDataReal::AtDataReal() {
}

AtDataReal::~AtDataReal() {
}

int AtDataReal::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {
    SetDescriptor(descriptor);
    m_value = tokenizer->ExpectReal();
    return 0;
}

void AtDataReal::Reset() {
    AtDataComponent::Reset();
}

AtDataShortName::AtDataShortName() {
}

AtDataShortName::~AtDataShortName() {
}

int AtDataShortName::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {
    SetDescriptor(descriptor);
    tokenizer->ExpectName(m_value);
    return 0;
}

void AtDataShortName::Reset() {
    AtDataComponent::Reset();
}

// ---- text ---------------------------------------------------------------------------------------------------------------

AtDataString::AtDataString() {
    m_unicode = 0;
    m_chars = 0;
}

AtDataString::~AtDataString() {
    if (m_narrow.m_capacity > 0) {
        m_narrow.Release();
    }
    if (m_wide.m_capacity > 0) {
        m_wide.Release();
    }
    if (m_chars != 0) {
        operator delete(m_chars);
    }
    m_unicode = 0;
    m_chars = 0;
    AtDataComponent::Reset();
}

void AtDataString::Reset() {
    if (m_narrow.m_capacity > 0) {
        m_narrow.Release();
    }
    if (m_wide.m_capacity > 0) {
        m_wide.Release();
    }
    if (m_chars != 0) {
        operator delete(m_chars);
    }
    m_unicode = 0;
    m_chars = 0;
    AtDataComponent::Reset();
}

int AtDataString::Assign(AtDataDescriptor* descriptor, const char* text) {
    const AtText& active = m_unicode != 0 ? (const AtText&)m_wide : (const AtText&)m_narrow;
    if (active.m_capacity > 0) {
        Reset();
    }
    SetDescriptor(descriptor);
    if (m_unicode != 0) {
        const unsigned int room = RoomFor(descriptor, LengthOf(text) + 2);
        unsigned short* chars = (unsigned short*)NewBlock(room * sizeof(unsigned short));
        if (chars == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        m_chars = chars;
        memset(chars, 0, room);
        m_wide.Init(chars, room);
        m_wide.SetNarrow(text);
    } else {
        const unsigned int room = RoomFor(descriptor, LengthOf(text) + 1);
        char* chars = (char*)NewBlock(room);
        if (chars == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        m_chars = chars;
        memset(chars, 0, room);
        m_narrow.Init(chars, room);
        m_narrow.SetNarrow(text);
    }
    return 0;
}

int AtDataString::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {
    const char* text = tokenizer->ExpectString();
    if (tokenizer->Next() == AtIoTokenizer::kString) {
        m_unicode = EqualIgnoringCase(tokenizer->m_string, "Unicode");
    } else {
        tokenizer->m_pushedBack = 1;
    }
    Assign(descriptor, text);
    return 0;
}

// ---- arrays -------------------------------------------------------------------------------------------------------------

AtDataBooleanArray::AtDataBooleanArray() {
    m_data = 0;
}

AtDataBooleanArray::~AtDataBooleanArray() {
    FreeData(this, m_data);
}

int AtDataBooleanArray::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {
    SetDescriptor(descriptor);
    const unsigned int count = m_descriptor->m_count;
    Resize(count);
    for (unsigned int i = 0; i < count; i++) {
        m_data[i] = tokenizer->ExpectInt();
    }
    return 0;
}

void AtDataBooleanArray::Reset() {
    FreeData(this, m_data);
}

int AtDataBooleanArray::Resize(unsigned int count) {
    return ResizeData(this, m_data, count);
}

AtDataByteArray::AtDataByteArray() {
    m_data = 0;
}

AtDataByteArray::~AtDataByteArray() {
    FreeData(this, m_data);
}

int AtDataByteArray::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {
    SetDescriptor(descriptor);
    const unsigned int count = m_descriptor->m_count;
    Resize(count);
    for (unsigned int i = 0; i < count; i++) {
        m_data[i] = (unsigned char)tokenizer->ExpectInt();
    }
    return 0;
}

void AtDataByteArray::Reset() {
    FreeData(this, m_data);
}

int AtDataByteArray::Resize(unsigned int count) {
    return ResizeData(this, m_data, count);
}

AtDataIntArray::AtDataIntArray() {
    m_data = 0;
}

AtDataIntArray::~AtDataIntArray() {
    FreeData(this, m_data);
}

int AtDataIntArray::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {
    SetDescriptor(descriptor);
    const unsigned int count = m_descriptor->m_count;
    Resize(count);
    for (unsigned int i = 0; i < count; i++) {
        m_data[i] = tokenizer->ExpectInt();
    }
    return 0;
}

void AtDataIntArray::Reset() {
    FreeData(this, m_data);
}

int AtDataIntArray::Resize(unsigned int count) {
    return ResizeData(this, m_data, count);
}

AtDataRealArray::AtDataRealArray() {
    m_data = 0;
}

AtDataRealArray::~AtDataRealArray() {
    FreeData(this, m_data);
}

int AtDataRealArray::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {
    SetDescriptor(descriptor);
    const unsigned int count = m_descriptor->m_count;
    Resize(count);
    for (unsigned int i = 0; i < count; i++) {
        m_data[i] = tokenizer->ExpectReal();
    }
    return 0;
}

void AtDataRealArray::Reset() {
    FreeData(this, m_data);
}

int AtDataRealArray::Resize(unsigned int count) {
    return ResizeData(this, m_data, count);
}

AtDataShortNameArray::AtDataShortNameArray() {
    m_data = 0;
}

AtDataShortNameArray::~AtDataShortNameArray() {
    FreeData(this, m_data);
}

int AtDataShortNameArray::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {
    SetDescriptor(descriptor);
    const unsigned int count = m_descriptor->m_count;
    Resize(count);
    for (unsigned int i = 0; i < count; i++) {
        // Through a name on the stack, so the bytes past a short name's terminator are whatever was there.
        char name[8];
        tokenizer->ExpectName(name);
        memcpy(m_data[i], name, sizeof name);
    }
    return 0;
}

void AtDataShortNameArray::Reset() {
    FreeData(this, m_data);
}

int AtDataShortNameArray::Resize(unsigned int count) {
    return ResizeData(this, m_data, count);
}

AtDataStringArray::AtDataStringArray() {
    m_chars = 0;
    m_texts = 0;
}

AtDataStringArray::~AtDataStringArray() {
    Reset();
}

void AtDataStringArray::Reset() {
    if (m_texts == 0) {
        return;
    }
    const unsigned int count = m_descriptor->m_count;
    for (unsigned int i = 0; i < count; i++) {
        m_texts[i].Release();
        if (m_chars[i] != 0) {
            operator delete(m_chars[i]);
        }
    }
    operator delete(m_chars);
    operator delete(m_texts);
    m_texts = 0;
    m_chars = 0;
    AtDataComponent::Reset();
}

int AtDataStringArray::Resize(unsigned int count) {
    AtTextUnicode* texts = (AtTextUnicode*)NewBlock(count * sizeof(AtTextUnicode));
    if (texts != 0) {
        for (unsigned int i = 0; i < count; i++) {
            new (texts + i, AtPlacement()) AtTextUnicode;
        }
    }
    unsigned short** chars = 0;
    if (texts != 0) {
        chars = (unsigned short**)NewBlock(count * sizeof(unsigned short*));
    }
    if (chars == 0) {
        AtRaise(-4, 0, 0, 0);
        return -4;
    }
    for (unsigned int i = 0; i < count; i++) {
        chars[i] = 0;
    }
    if (m_texts != 0) {
        const unsigned int had = m_descriptor->m_count;
        const unsigned int kept = count > had ? had : count;
        memcpy((void*)texts, (const void*)m_texts, kept * sizeof(AtTextUnicode));
        memcpy(chars, m_chars, kept * sizeof(unsigned short*));
        operator delete(m_chars);
        operator delete(m_texts);
    }
    m_chars = chars;
    m_texts = texts;
    m_descriptor->m_count = count;
    return 0;
}

int AtDataStringArray::SetText(unsigned int index, const char* text) {
    const unsigned int room = RoomFor(m_descriptor, LengthOf(text) + 1);
    PushHeap();
    m_chars[index] = (unsigned short*)operator new(room * sizeof(unsigned short));
    AtPopHeap();
    if (m_chars[index] == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_texts[index].Init(m_chars[index], room);
    m_texts[index].SetNarrow(text);
    return 0;
}

int AtDataStringArray::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {
    SetDescriptor(descriptor);
    const unsigned int count = m_descriptor->m_count;
    Resize(count);
    for (unsigned int i = 0; i < count; i++) {
        const char* text = tokenizer->ExpectString();
        SetText(i, text);
    }
    return 0;
}
