// The action script machine: its dispatch loop, its handlers, and what of the values and their stack only they use. See include/at/AtFlashScript.h; what it calls out to is include/at/AtFpDisplay.h.
//
// Every handler takes its operands into values of its own (AtFpActionScriptLocalValue), each popped off the stack, and pushes what it makes. The order the values are declared in below is the order the
// original constructs them in, so that they are destroyed, and what they hold freed, in its order too. Where the order in which a handler reads its operands matters, it is kept with locals.
#include "at/AtFlashScript.h"

#include "at/AtRandom.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
unsigned int AtPlatformMilliseconds();  // 0x00020640, from the performance counter: the platform's, as for include/gm/GmManifestLoader.h
void AtFormatGeneral(char* out, double value);  // The C library's sprintf with "%g", 0x00125559: src/at/AtFlashScriptHost.cpp

namespace {

// The action codes Run dispatches on, by the SWF numbering. Only these have a handler; any other code does nothing.
enum Action {
    kEnd = 0x00,
    kNextFrame = 0x04,
    kPreviousFrame = 0x05,
    kPlay = 0x06,
    kStop = 0x07,
    kStopSounds = 0x09,
    kAdd = 0x0A,
    kSubtract = 0x0B,
    kMultiply = 0x0C,
    kDivide = 0x0D,
    kEquals = 0x0E,
    kLess = 0x0F,
    kAnd = 0x10,
    kOr = 0x11,
    kNot = 0x12,
    kStringEquals = 0x13,
    kStringLength = 0x14,
    kStringExtract = 0x15,
    kPop = 0x17,
    kToInteger = 0x18,
    kGetVariable = 0x1C,
    kSetVariable = 0x1D,
    kSetTarget2 = 0x20,
    kStringAdd = 0x21,
    kGetProperty = 0x22,
    kSetProperty = 0x23,
    kCloneSprite = 0x24,
    kRemoveSprite = 0x25,
    kTrace = 0x26,
    kStringLess = 0x29,
    kRandomNumber = 0x30,
    kCharToAscii = 0x32,
    kAsciiToChar = 0x33,
    kGetTime = 0x34,
    kGotoFrame = 0x81,
    kGetUrl = 0x83,
    kSetTarget = 0x8B,
    kGotoLabel = 0x8C,
    kWaitForFrame2 = 0x8D,
    kPush = 0x96,
    kJump = 0x99,
    kGetUrl2 = 0x9A,
    kIf = 0x9D,
    kCall = 0x9E,
    kGotoFrame2 = 0x9F,
};

// What Push and the label handlers find in an instruction's data: a kind, then the value.
enum PushKind { kPushString = 0x00, kPushReal = 0x01, kPushInt = 0x07, kPushStringId = 0x20 };

// The instruction's fields are little-endian and need not be aligned.
unsigned int ReadU16(const unsigned char* p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}
unsigned int ReadU32(const unsigned char* p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}
float ReadReal(const unsigned char* p) {
    union {
        unsigned int bits;
        float real;
    } u;
    u.bits = ReadU32(p);
    return u.real;
}

unsigned int Length(const char* text) {
    unsigned int n = 0;
    while (text[n] != 0) {
        n++;
    }
    return n;
}
void CopyText(char* to, const char* from) {
    while ((*to++ = *from++) != 0) {
    }
}
// strcmp, by unsigned bytes: below 0, 0 or above.
int CompareText(const char* a, const char* b) {
    for (;; a++, b++) {
        const unsigned char x = (unsigned char)*a, y = (unsigned char)*b;
        if (x != y) {
            return x < y ? -1 : 1;
        }
        if (x == 0) {
            return 0;
        }
    }
}

// The C library's atof, as the original's has it: decimal only. A modern library also reads "inf", "nan" and hexadecimal, which the original's stops short of, so only the decimal form is handed on.
float TextToReal(const char* text) {
    char number[64];
    unsigned int n = 0;
    const char* p = text;
    while (*p == ' ' || (*p >= 9 && *p <= 13)) {
        p++;
    }
    if (*p == '+' || *p == '-') {
        number[n++] = *p++;
    }
    int digits = 0;
    while (*p >= '0' && *p <= '9' && n < sizeof(number) - 8) {
        number[n++] = *p++;
        digits++;
    }
    if (*p == '.') {
        number[n++] = *p++;
        while (*p >= '0' && *p <= '9' && n < sizeof(number) - 8) {
            number[n++] = *p++;
            digits++;
        }
    }
    if (digits > 0 && (*p == 'e' || *p == 'E')) {
        const char* q = p + 1;
        if (*q == '+' || *q == '-') {
            q++;
        }
        if (*q >= '0' && *q <= '9') {
            number[n++] = 'e';
            p++;
            if (*p == '+' || *p == '-') {
                number[n++] = *p++;
            }
            while (*p >= '0' && *p <= '9' && n < sizeof(number) - 1) {
                number[n++] = *p++;
            }
        }
    }
    number[n] = 0;
    return digits > 0 ? AtNarrowToFloat(number) : 0.0f;
}

// The C library's _itoa in base 10.
void IntToText(int value, char* out) {
    unsigned int magnitude = value < 0 ? 0u - (unsigned int)value : (unsigned int)value;
    char reversed[12];
    int count = 0;
    do {
        reversed[count++] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude != 0);
    if (value < 0) {
        *out++ = '-';
    }
    while (count > 0) {
        *out++ = reversed[--count];
    }
    *out = 0;
}

// The integer a real truncates to, as the original's _ftol gives it (0x001260BC): toward zero to 64 bits, and the low 32 of those. So 3e9 gives 0xB2D05E00; past 64 bits, and a NaN, 0.
int Truncate(float value) {
    if (!(value > -9223372036854775808.0f && value < 9223372036854775808.0f)) {
        return 0;
    }
    return (int)(unsigned int)(unsigned long long)(long long)value;
}

void PushOrRaise(void* own, void* fallback) {
    void* heap = own != 0 ? own : fallback;
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
}

}  // namespace

// ---- a value's forms ------------------------------------------------------------------------------------------------------

void AtFpActionScriptValue::Clear() {
    m_text = 0;
    m_unknown24 = 0;
    m_capacity = 0;
    m_type = kTypeNone;
    m_int = 0;
    m_unknown14 = 0;
    m_haveTextInt = 0;
    m_haveTextReal = 0;
    m_haveIntReal = 0;
    m_heap = 0;
}

void AtFpActionScriptValue::SetInt(const int* value) {
    m_type = kTypeInt;
    m_int = *value;
    m_haveTextInt = 0;
    m_haveTextReal = 0;
    m_haveIntReal = 0;
}

void AtFpActionScriptValue::SetReal(const float* value) {
    m_type = kTypeReal;
    m_unknown14 = *(const int*)value;  // The bits, as the original moves them.
    m_haveTextInt = 0;
    m_haveTextReal = 0;
    m_int = Truncate(*value);
    m_haveIntReal = 1;
    // Exactly, as the original's x87 compares them.
    if ((double)m_int == (double)m_real) {
        m_type = kTypeInt;
    }
}

float AtFpActionScriptValue::ToReal() {
    if (m_type == kTypeInt) {
        if (m_haveIntReal == 0) {
            m_haveIntReal = 1;
            m_real = (float)m_int;
        }
    } else if (m_type == kTypeString && m_haveTextReal == 0) {
        m_real = TextToReal(m_text);
        m_haveTextReal = 1;
    }
    return m_real;
}

int AtFpActionScriptValue::ToInt() {
    if (m_type == kTypeReal) {
        if (m_haveIntReal == 0) {
            m_haveIntReal = 1;
            m_int = Truncate(m_real);
        }
    } else if (m_type == kTypeString && m_haveTextInt == 0) {
        m_int = AtNarrowToInt(m_text);
        m_haveTextInt = 1;
    }
    return m_int;
}

char* AtFpActionScriptValue::ToText() {
    if (m_type == kTypeReal) {
        if (m_haveTextReal == 0) {
            Reserve(0x20);
            AtFormatGeneral(m_text, (double)m_real);
            m_haveTextReal = 1;
        }
    } else if (m_type == kTypeInt) {
        if (m_haveTextInt == 0) {
            Reserve(0x20);
            IntToText(m_int, m_text);
            m_haveTextInt = 1;
        }
    } else if (m_type == kTypeNone) {
        return (char*)"";  // The original's is a pointer to an empty string, at 0x002B03C8; nothing writes it.
    }
    return m_text;
}

unsigned int AtFpActionScriptValue::Length() {
    if (m_type == kTypeNone) {
        return 0;
    }
    ToText();
    return ::Length(m_text);
}

int AtFpActionScriptValue::IsInteger() {
    if (m_type == kTypeInt || m_type == kTypeNone) {
        return 1;
    }
    if (m_type == kTypeReal) {
        const int truncated = ToInt();
        return (double)truncated == (double)m_real ? 1 : 0;
    }
    if (m_type != kTypeString) {
        return 0;
    }
    const unsigned int length = Length();
    unsigned int i = 0;
    for (; i < length; i++) {
        if (i == 0 && m_text[0] == '-') {
            continue;
        }
        const signed char c = (signed char)m_text[i];
        if (c < '0' || c > '9') {
            break;
        }
    }
    return i == length ? 1 : 0;
}

AtFpActionScriptValue* AtFpActionScriptValue::CopyFrom(const AtFpActionScriptValue* from) {
    m_type = from->m_type;
    m_int = from->m_int;
    m_unknown14 = from->m_unknown14;
    m_haveIntReal = from->m_haveIntReal;
    m_haveTextReal = from->m_haveTextReal;
    m_haveTextInt = from->m_haveTextInt;
    // Only when from has text: its own, or a number's that has been written out.
    if (m_type == kTypeString || m_haveTextReal != 0 || m_haveTextInt != 0) {
        Reserve(from->m_capacity);
        CopyText(m_text, from->m_text);
    }
    return this;
}

void AtFpActionScriptValue::SetText(AtText* text) {
    if (text == 0) {
        m_haveTextInt = 0;
        m_haveTextReal = 0;
        m_haveIntReal = 0;
        m_type = kTypeNone;
        return;
    }
    const unsigned int size = text->m_end - text->m_start + 1;
    Reserve(size);
    m_type = kTypeString;
    text->CopyNarrow(m_text, size);
    m_haveTextInt = 0;
    m_haveTextReal = 0;
    m_haveIntReal = 0;
}

void AtFpActionScriptValue::SetText(const char* text) {
    if (text == 0) {
        m_haveTextInt = 0;
        m_haveTextReal = 0;
        m_haveIntReal = 0;
        m_type = kTypeNone;
        return;
    }
    Reserve(::Length(text) + 1);
    m_type = kTypeString;
    CopyText(m_text, text);
    m_haveTextInt = 0;
    m_haveTextReal = 0;
    m_haveIntReal = 0;
}

// ---- a handler's own value --------------------------------------------------------------------------------------------------

AtFpActionScriptLocalValue::AtFpActionScriptLocalValue() {
    Clear();
}

AtFpActionScriptLocalValue::~AtFpActionScriptLocalValue() {
    if (m_text == m_buffer) {
        m_text = 0;
    }
    AtFpActionScriptValue::Reset();
}

void AtFpActionScriptLocalValue::Reserve(unsigned int size) {
    if (size < 0xFF && (m_text == 0 || m_text == m_buffer)) {
        m_text = m_buffer;
        m_capacity = size;
        m_text[0] = 0;
        return;
    }
    // In the buffer, the buffer's room counts, not what was asked for. Compared signed, as in the base.
    const unsigned int room = (m_text == m_buffer && m_capacity < 0xFF) ? 0xFF : m_capacity;
    if ((int)size <= (int)room) {
        m_text[0] = 0;
        return;
    }
    if (m_text != 0 && m_text != m_buffer) {
        operator delete(m_text);
    }
    PushOrRaise(0, AtIoTokenFile::s_heap);
    m_text = (char*)operator new(size);
    AtPopHeap();
    if (m_text == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_capacity = size;
    m_text[0] = 0;
}

void AtFpActionScriptLocalValue::MoveTo(AtFpActionScriptValue* to) {
    if (m_text == m_buffer) {
        to->CopyFrom(this);
        return;
    }
    AtFpActionScriptValue::MoveTo(to);
}

void AtFpActionScriptLocalValue::SetString(char* text, unsigned int capacity) {
    m_type = kTypeString;
    if (m_text != 0 && m_text != m_buffer) {
        operator delete(m_text);
    }
    m_haveTextInt = 0;
    m_haveIntReal = 0;
    m_haveTextReal = 0;
    m_text = text;
    m_capacity = capacity;
}

void AtFpActionScriptLocalValue::Reset() {
    if (m_text == m_buffer) {
        m_text = 0;
    }
    AtFpActionScriptValue::Reset();
}

// ---- the stack's pushes and pops ----------------------------------------------------------------------------------------------

int AtFpActionScriptStack::Pop(AtFpActionScriptValue* to) {
    if (m_top < 0) {
        return 0;
    }
    m_values[m_top].MoveTo(to);
    m_top--;
    return 1;
}

int AtFpActionScriptStack::Discard() {
    if (m_top < 0) {
        return 0;
    }
    m_top--;
    return 1;
}

int AtFpActionScriptStack::PushInt(int value) {
    if (m_top >= (int)(m_capacity - 1)) {
        return 0;
    }
    m_top++;
    m_values[m_top].m_heap = m_heap != 0 ? m_heap : s_heap;
    m_values[m_top].SetInt(&value);
    return 1;
}

int AtFpActionScriptStack::PushReal(float value) {
    if (m_top >= (int)(m_capacity - 1)) {
        return 0;
    }
    m_top++;
    m_values[m_top].m_heap = m_heap != 0 ? m_heap : s_heap;
    m_values[m_top].SetReal(&value);
    return 1;
}

int AtFpActionScriptStack::PushText(const char* text) {
    if (m_top >= (int)(m_capacity - 1)) {
        return 0;
    }
    m_top++;
    m_values[m_top].m_heap = m_heap != 0 ? m_heap : s_heap;
    m_values[m_top].SetText(text);
    return 1;
}

int AtFpActionScriptStack::PushText(AtText* text) {
    if (m_top >= (int)(m_capacity - 1)) {
        return 0;
    }
    m_top++;
    m_values[m_top].m_heap = m_heap != 0 ? m_heap : s_heap;
    m_values[m_top].SetText(text);
    return 1;
}

int AtFpActionScriptStack::PushUndefined() {
    if (m_top >= (int)(m_capacity - 1)) {
        return 0;
    }
    m_top++;
    m_values[m_top].m_heap = m_heap != 0 ? m_heap : s_heap;
    m_values[m_top].m_type = AtFpActionScriptValue::kTypeNone;
    return 1;
}

int AtFpActionScriptStack::PushString(char* text, unsigned int capacity) {
    if (m_top >= (int)(m_capacity - 1)) {
        return 0;  // The block is not freed.
    }
    m_top++;
    m_values[m_top].m_heap = m_heap != 0 ? m_heap : s_heap;
    m_values[m_top].SetString(text, capacity);
    return 1;
}

int AtFpActionScriptStack::PushCopy(const AtFpActionScriptValue* from) {
    if (m_top >= (int)(m_capacity - 1)) {
        return 0;
    }
    m_top++;
    m_values[m_top].m_heap = m_heap != 0 ? m_heap : s_heap;
    m_values[m_top].CopyFrom(from);
    return 1;
}

int AtFpActionScriptStack::PushFrom(AtFpActionScriptValue* from) {
    if (m_top >= (int)(m_capacity - 1)) {
        return 0;
    }
    m_top++;
    m_values[m_top].m_heap = m_heap != 0 ? m_heap : s_heap;
    from->MoveTo(&m_values[m_top]);
    return 1;
}

// ---- the dispatch loop ---------------------------------------------------------------------------------------------------------

void AtFpActionScriptVM::Run(AtFpScript* script) {
    int stop = 0;
    unsigned int index = 0;
    unsigned int action;
    do {
        // A code below 0x80 is the whole instruction; from 0x80 on, a length and data follow.
        const unsigned char* instruction = script->m_instructions[index];
        action = ReadU16(instruction);
        unsigned int length = 0;
        const unsigned char* data = 0;
        if (action >= 0x80) {
            length = ReadU16(instruction + 2);
            data = instruction + 4;
        }
        switch (action) {
        case kNextFrame:
            m_application->m_display.GotoFrame(m_application->m_display.GetCurrentFrame() + 1);
            break;
        case kPreviousFrame:
            m_application->m_display.GotoFrame(m_application->m_display.GetCurrentFrame() - 1);
            break;
        case kPlay:
            m_application->m_display.SetPlaying(1);
            break;
        case kStopSounds:
            m_application->m_display.StopSounds();
            break;
        case kAdd:
            ActionAdd();
            break;
        case kSubtract:
            ActionSubtract();
            break;
        case kMultiply:
            ActionMultiply();
            break;
        case kDivide:
            ActionDivide();
            break;
        case kEquals:
            ActionEquals();
            break;
        case kLess:
            ActionLess();
            break;
        case kAnd:
            ActionAnd();
            break;
        case kOr:
            ActionOr();
            break;
        case kNot:
            ActionNot();
            break;
        case kGetVariable:
            ActionGetVariable();
            break;
        case kSetVariable:
            ActionSetVariable();
            break;
        case kStringLess:
            ActionStringLess();
            break;
        case kStringEquals:
            ActionStringEquals();
            break;
        case kStringAdd:
            ActionStringAdd();
            break;
        case kCharToAscii:
            ActionCharToAscii();
            break;
        case kAsciiToChar:
            ActionAsciiToChar();
            break;
        case kToInteger:
            ActionToInteger();
            break;
        case kStringLength:
            ActionStringLength();
            break;
        case kStringExtract:
            ActionStringExtract();
            break;
        case kGetUrl: {
            // The URL, then the target after its terminator.
            const char* url = (const char*)data;
            LoadMovie(url, url + Length(url) + 1);
            break;
        }
        case kGetUrl2:
            ActionGetURL2(data[0]);
            break;
        case kGetProperty:
            ActionGetProperty();
            break;
        case kSetProperty:
            ActionSetProperty();
            break;
        case kCloneSprite:
            ActionCloneSprite();
            break;
        case kRemoveSprite:
            ActionRemoveSprite();
            break;
        case kRandomNumber:
            ActionRandomNumber();
            break;
        case kGetTime:
            ActionGetTime();
            break;
        case kTrace:
            ActionTrace();
            break;
        case kGotoFrame:
            m_application->m_display.GotoFrame((int)ReadU16(data));
            m_application->m_display.SetPlaying(0);
            break;
        case kStop:
            m_application->m_display.SetPlaying(0);
            break;
        case kWaitForFrame2:
            ActionWaitForFrame2(data[0]);
            break;
        case kGotoLabel:
            ActionGotoLabel(data);
            break;
        case kGotoFrame2:
            ActionGotoFrame2(data[0]);
            break;
        case kSetTarget:
            ActionSetTarget(data);
            break;
        case kSetTarget2:
            ActionSetTarget2();
            break;
        case kPop:
            m_stack.Discard();
            break;
        case kPush:
            ActionPush(length, data);
            break;
        case kJump: {
            const unsigned int target = ReadU16(data);
            if (target < script->m_read) {
                index = target - 1;
            }
            break;
        }
        case kIf:
            ActionIf((unsigned short)ReadU16(data), &index, script);
            break;
        case kCall:
            stop = ActionCall();
            break;
        }
        index++;
    } while (action != kEnd && stop == 0);
}

// ---- control ---------------------------------------------------------------------------------------------------------------

void AtFpActionScriptVM::ActionIf(unsigned short target, unsigned int* index, AtFpScript* script) {
    AtFpActionScriptLocalValue condition;
    m_stack.Pop(&condition);
    // By the integer: a real between -1 and 1 is false.
    if (condition.Int() != 0 && target < script->m_read) {
        *index = target - 1;
    }
}

int AtFpActionScriptVM::ActionCall() {
    AtFpActionScriptLocalValue frame;
    m_stack.Pop(&frame);
    if (frame.IsInteger()) {
        return m_application->m_display.ExecuteScripts(frame.Int() - 1);
    }
    return m_application->m_display.ExecuteScripts(frame.Text());
}

void AtFpActionScriptVM::LoadMovie(const char* url, const char* target) {
    const unsigned int length = Length(url);
    char name[0x40];
    // strncpy: at most 0x40 characters, the rest of the buffer zeros.
    unsigned int i = 0;
    for (; i < sizeof(name) && url[i] != 0; i++) {
        name[i] = url[i];
    }
    for (; i < sizeof(name); i++) {
        name[i] = 0;
    }
    name[sizeof(name) - 1] = 0;
    if (length > 4) {
        // The extension goes. A name of 0x44 characters or more makes the original write its zero past the buffer, into its own stack frame; that is not reproduced.
        if (length - 4 < sizeof(name)) {
            name[length - 4] = 0;
        }
    } else if (length != 0) {
        AtFatal("AtFpActionScriptVM::vLoadMovie Illegal file name requested", 0, 0, 0);
    }
    m_application->m_display.LoadMovie(name, target);
}

void AtFpActionScriptVM::ActionGetTime() {
    AtFpActionScriptLocalValue unused;  // Made and destroyed; nothing else.
    m_stack.PushInt((int)AtPlatformMilliseconds());
}

void AtFpActionScriptVM::ActionWaitForFrame2(unsigned char) {
    AtFpActionScriptLocalValue frame;
    m_stack.Pop(&frame);
}

void AtFpActionScriptVM::ActionGotoLabel(const unsigned char* data) {
    AtFpActionScriptLocalValue label;
    const unsigned int kind = ReadU16(data);
    if (kind == kPushString) {
        label.SetText((const char*)data + 2);
    } else if (kind == kPushStringId) {
        char name[8];
        AtCopyName((const char*)data + 2, name);
        AtText* text = (AtText*)m_strings->Find(name);
        if (text == 0) {
            return;
        }
        label.SetText(text);
    } else {
        return;
    }
    m_application->m_display.GotoFrameByLabel(label.Text());
    m_application->m_display.SetPlaying(0);
}

void AtFpActionScriptVM::ActionGotoFrame2(unsigned char play) {
    AtFpActionScriptLocalValue frame;
    m_stack.Pop(&frame);
    m_application->m_display.GotoFrameByExpression(frame.Text());
    m_application->m_display.SetPlaying(play);
}

void AtFpActionScriptVM::ActionSetTarget(const unsigned char* data) {
    AtFpActionScriptLocalValue path;
    const unsigned int kind = ReadU16(data);
    if (kind == kPushString) {
        path.SetText((const char*)data + 2);
    } else if (kind == kPushStringId) {
        // A string not found leaves the path undefined, which goes on as one.
        char name[8];
        AtCopyName((const char*)data + 2, name);
        AtText* text = (AtText*)m_strings->Find(name);
        if (text != 0) {
            path.SetText(text);
        }
    } else {
        return;
    }
    AtFpDisplayManager& display = m_application->m_display;
    if (path.m_type == AtFpActionScriptValue::kTypeNone) {
        display.SetTarget("");
    } else if (path.Length() == 0) {
        // An empty path: back to the current context.
        AtFpDisplayMgrContextStack& contexts = display.m_contexts;
        contexts.SetTarget(contexts.m_entries[contexts.m_count - 1].m_context);
    } else {
        display.SetTarget(path.Text());
    }
}

void AtFpActionScriptVM::ActionSetTarget2() {
    AtFpActionScriptLocalValue path;
    m_stack.Pop(&path);
    AtFpDisplayManager& display = m_application->m_display;
    if (path.m_type == AtFpActionScriptValue::kTypeNone) {
        display.SetTarget("");
    } else if (path.Length() == 0) {
        AtFpDisplayMgrContextStack& contexts = display.m_contexts;
        contexts.SetTarget(contexts.m_entries[contexts.m_count - 1].m_context);
    } else {
        display.SetTarget(path.Text());
    }
}

// ---- values in and out ----------------------------------------------------------------------------------------------------------

void AtFpActionScriptVM::ActionPush(unsigned int length, const unsigned char* data) {
    unsigned int i = 0;
    while (i < length) {
        const unsigned int kind = ReadU16(data + i);
        if (kind == kPushString) {
            // A copy of at least 0x20 bytes, from the stack's heap.
            const char* text = (const char*)data + i + 2;
            const unsigned int n = Length(text) + 1;
            const unsigned int size = n > 0x20 ? n : 0x20;
            AtPushHeap(m_stack.m_heap != 0 ? m_stack.m_heap : AtFpActionScriptStack::s_heap);
            char* copy = (char*)operator new(size);
            AtPopHeap();
            if (copy == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            CopyText(copy, text);
            m_stack.PushString(copy, size);
            i += n + 2;
        } else if (kind == kPushStringId) {
            // Eight characters naming a string of the script's own. One not found is not stepped over: the loop never ends.
            char name[8];
            AtCopyName((const char*)data + i + 2, name);
            AtText* text = (AtText*)m_strings->Find(name);
            if (text != 0) {
                m_stack.PushText(text);
                i += 10;
            }
        } else if (kind == kPushReal) {
            m_stack.PushReal(ReadReal(data + i + 2));
            i += 6;
        } else if (kind == kPushInt) {
            m_stack.PushInt((int)ReadU32(data + i + 2));
            i += 6;
        }
        // Any other kind is not stepped over either.
    }
}

void AtFpActionScriptVM::ActionGetVariable() {
    AtFpActionScriptLocalValue name;
    AtFpActionScriptLocalValue value;
    m_stack.Pop(&name);
    m_application->m_display.GetVariable(name.Text(), &value);
    m_stack.PushFrom(&value);
}

void AtFpActionScriptVM::ActionSetVariable() {
    AtFpActionScriptLocalValue name;
    AtFpActionScriptLocalValue value;
    m_stack.Pop(&value);
    m_stack.Pop(&name);
    m_application->m_display.SetVariable(name.Text(), &value);
}

void AtFpActionScriptVM::ActionGetProperty() {
    AtFpActionScriptLocalValue result;
    AtFpActionScriptLocalValue property;
    AtFpActionScriptLocalValue target;
    m_stack.Pop(&property);
    m_stack.Pop(&target);
    const int index = property.Int();
    m_application->m_display.GetProperty(target.Text(), index, &result);
    m_stack.PushCopy(&result);
}

void AtFpActionScriptVM::ActionSetProperty() {
    AtFpActionScriptLocalValue value;
    AtFpActionScriptLocalValue property;
    AtFpActionScriptLocalValue target;
    m_stack.Pop(&value);
    m_stack.Pop(&property);
    m_stack.Pop(&target);
    const int index = property.Int();
    m_application->m_display.SetProperty(target.Text(), index, &value);
}

void AtFpActionScriptVM::ActionCloneSprite() {
    AtFpActionScriptLocalValue source;
    AtFpActionScriptLocalValue target;
    AtFpActionScriptLocalValue depth;
    m_stack.Pop(&depth);
    m_stack.Pop(&target);
    m_stack.Pop(&source);
    if (depth.m_type != AtFpActionScriptValue::kTypeInt) {
        depth.ToInt();
    }
    if (target.m_type != AtFpActionScriptValue::kTypeString) {
        target.ToText();
    }
    if (source.m_type != AtFpActionScriptValue::kTypeString) {
        source.ToText();
    }
}

void AtFpActionScriptVM::ActionRemoveSprite() {
    AtFpActionScriptLocalValue target;
    m_stack.Pop(&target);
    if (target.m_type != AtFpActionScriptValue::kTypeString) {
        target.ToText();
    }
}

void AtFpActionScriptVM::ActionTrace() {
    AtFpActionScriptLocalValue value;
    m_stack.Pop(&value);
    AtFpRfcArgs args;
    args.m_heap = m_heap != 0 ? m_heap : s_heap;
    args.Init();
    args.Add("TraceString", &value);
    m_application->m_rfc.Call("TraceOutput", &args);
    args.Clear();
}

void AtFpActionScriptVM::ActionGetURL2(unsigned char method) {
    // The original masks the method with a word it keeps at 0x002B0344, 0xFFFFFFFC, which nothing writes.
    switch (method & ~3u) {
    case 0x00:
    case 0x40: {
        AtFpActionScriptLocalValue url;
        AtFpActionScriptLocalValue target;
        m_stack.Pop(&target);
        m_stack.Pop(&url);
        const char* targetText = target.Text();
        LoadMovie(url.Text(), targetText);
        break;
    }
    case 0x80:
    case 0xC0: {
        // A remote function call into the game, named by the URL, with what the target names.
        AtFpActionScriptLocalValue url;
        AtFpActionScriptLocalValue target;
        m_stack.Pop(&target);
        m_stack.Pop(&url);
        const char* targetText = target.Text();
        const char* name = url.Text();
        AtFpRfcArgs* args = m_application->m_display.ResolveTarget(targetText);
        m_application->m_rfc.Call(name, args);
        break;
    }
    default:
        AtFatal("AtFpActionScriptVM::vActionGetURL2 - Unsupported method", 0, 0, 0);
        break;
    }
}

// ---- arithmetic and logic: reals, compared and combined as x87 does them, then stored as float --------------------------------

void AtFpActionScriptVM::ActionLess() {
    AtFpActionScriptLocalValue b;
    AtFpActionScriptLocalValue a;
    m_stack.Pop(&b);
    m_stack.Pop(&a);
    const float x = a.Real();
    m_stack.PushInt(x < b.Real() ? 1 : 0);
}

void AtFpActionScriptVM::ActionEquals() {
    AtFpActionScriptLocalValue b;
    AtFpActionScriptLocalValue a;
    m_stack.Pop(&b);
    m_stack.Pop(&a);
    const float x = a.Real();
    m_stack.PushInt(x == b.Real() ? 1 : 0);
}

void AtFpActionScriptVM::ActionNot() {
    AtFpActionScriptLocalValue value;
    m_stack.Pop(&value);
    m_stack.PushInt(value.Real() == 0.0f ? 1 : 0);
}

void AtFpActionScriptVM::ActionAnd() {
    AtFpActionScriptLocalValue a;
    AtFpActionScriptLocalValue b;
    m_stack.Pop(&b);
    m_stack.Pop(&a);
    m_stack.PushInt(a.Real() != 0.0f && b.Real() != 0.0f ? 1 : 0);
}

void AtFpActionScriptVM::ActionOr() {
    AtFpActionScriptLocalValue a;
    AtFpActionScriptLocalValue b;
    m_stack.Pop(&b);
    m_stack.Pop(&a);
    m_stack.PushInt(a.Real() != 0.0f || b.Real() != 0.0f ? 1 : 0);
}

// The sum, difference, product and quotient of two floats come out of the x87's double precision the same as from float arithmetic, so float is used.
void AtFpActionScriptVM::ActionAdd() {
    AtFpActionScriptLocalValue a;
    AtFpActionScriptLocalValue b;
    m_stack.Pop(&b);
    m_stack.Pop(&a);
    const float x = a.Real();
    m_stack.PushReal(b.Real() + x);
}

void AtFpActionScriptVM::ActionSubtract() {
    AtFpActionScriptLocalValue a;
    AtFpActionScriptLocalValue b;
    m_stack.Pop(&b);
    m_stack.Pop(&a);
    const float x = a.Real();
    m_stack.PushReal(x - b.Real());
}

void AtFpActionScriptVM::ActionDivide() {
    AtFpActionScriptLocalValue a;
    AtFpActionScriptLocalValue b;
    m_stack.Pop(&b);
    m_stack.Pop(&a);
    const float x = a.Real();
    m_stack.PushReal(x / b.Real());
}

void AtFpActionScriptVM::ActionMultiply() {
    AtFpActionScriptLocalValue a;
    AtFpActionScriptLocalValue b;
    m_stack.Pop(&b);
    m_stack.Pop(&a);
    const float x = a.Real();
    m_stack.PushReal(b.Real() * x);
}

void AtFpActionScriptVM::ActionToInteger() {
    AtFpActionScriptLocalValue value;
    m_stack.Pop(&value);
    m_stack.PushInt(value.Int());
}

void AtFpActionScriptVM::ActionRandomNumber() {
    AtFpActionScriptLocalValue limit;
    AtFpActionScriptLocalValue result;
    m_stack.Pop(&limit);
    // 31 bits from four words of the table: 7, then 8, 8 and 8.
    const unsigned int a = AtNextRandomWord() >> 4;
    const unsigned int b = AtNextRandomWord() >> 3;
    const unsigned int c = AtNextRandomWord() >> 3;
    const unsigned int d = AtNextRandomWord() >> 3;
    const unsigned int random = (((((a << 8) + b) << 8) + c) << 8) + d;
    // Unsigned; a limit of 0 faults, as in the original.
    int remainder = (int)(random % (unsigned int)limit.Int());
    result.SetInt(&remainder);
    m_stack.PushCopy(&result);
}

// ---- strings -------------------------------------------------------------------------------------------------------------------

void AtFpActionScriptVM::ActionStringAdd() {
    AtFpActionScriptLocalValue a;
    AtFpActionScriptLocalValue b;
    m_stack.Pop(&b);
    m_stack.Pop(&a);
    if (a.m_type == AtFpActionScriptValue::kTypeNone || b.m_type == AtFpActionScriptValue::kTypeNone) {
        m_stack.PushUndefined();
        return;
    }
    // Room by the two capacities, not the lengths.
    a.ToText();
    b.ToText();
    const unsigned int size = a.m_capacity + b.m_capacity + 1;
    AtPushHeap(m_stack.m_heap != 0 ? m_stack.m_heap : AtFpActionScriptStack::s_heap);
    char* joined = (char*)operator new(size);
    AtPopHeap();
    if (joined == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    CopyText(joined, a.Text());
    const char* second = b.Text();
    CopyText(joined + Length(joined), second);
    m_stack.PushString(joined, size);
}

void AtFpActionScriptVM::ActionCharToAscii() {
    AtFpActionScriptLocalValue value;
    m_stack.Pop(&value);
    if (value.m_type == AtFpActionScriptValue::kTypeNone) {
        m_stack.PushUndefined();
        return;
    }
    m_stack.PushInt((signed char)value.Text()[0]);
}

void AtFpActionScriptVM::ActionAsciiToChar() {
    AtFpActionScriptLocalValue value;
    m_stack.Pop(&value);
    char text[2];
    text[0] = 0;
    text[1] = 0;
    text[0] = (char)value.Int();
    m_stack.PushText(text);
}

void AtFpActionScriptVM::ActionStringEquals() {
    AtFpActionScriptLocalValue a;
    AtFpActionScriptLocalValue b;
    m_stack.Pop(&b);
    m_stack.Pop(&a);
    if (a.m_type == AtFpActionScriptValue::kTypeNone || b.m_type == AtFpActionScriptValue::kTypeNone) {
        m_stack.PushUndefined();
        return;
    }
    const char* y = b.Text();
    m_stack.PushInt(CompareText(a.Text(), y) == 0 ? 1 : 0);
}

void AtFpActionScriptVM::ActionStringLess() {
    AtFpActionScriptLocalValue a;
    AtFpActionScriptLocalValue b;
    m_stack.Pop(&b);
    m_stack.Pop(&a);
    if (a.m_type == AtFpActionScriptValue::kTypeNone || b.m_type == AtFpActionScriptValue::kTypeNone) {
        m_stack.PushUndefined();
        return;
    }
    const char* y = b.Text();
    m_stack.PushInt(CompareText(a.Text(), y) < 0 ? 1 : 0);
}

void AtFpActionScriptVM::ActionStringLength() {
    AtFpActionScriptLocalValue value;
    m_stack.Pop(&value);
    if (value.m_type == AtFpActionScriptValue::kTypeNone) {
        m_stack.PushUndefined();
        return;
    }
    m_stack.PushInt((int)Length(value.Text()));
}

void AtFpActionScriptVM::ActionStringExtract() {
    AtFpActionScriptLocalValue index;
    AtFpActionScriptLocalValue count;
    AtFpActionScriptLocalValue string;
    m_stack.Pop(&count);
    m_stack.Pop(&index);
    m_stack.Pop(&string);
    if (string.m_type == AtFpActionScriptValue::kTypeNone) {
        m_stack.PushUndefined();
        return;
    }
    const int n = count.Int();
    int from = index.Int();
    if (from == 0) {
        from = 1;
    }
    // Counted from 1. The last character cannot be the first taken: the index must be below the length, signed.
    if (n > 0 && from > 0 && from < (int)string.Length()) {
        AtPushHeap(m_stack.m_heap != 0 ? m_stack.m_heap : AtFpActionScriptStack::s_heap);
        char* part = (char*)operator new((unsigned int)n + 1);
        AtPopHeap();
        if (part == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        // strncpy: up to n characters, then zeros to n.
        const char* source = string.Text() + from - 1;
        int i = 0;
        for (; i < n && source[i] != 0; i++) {
            part[i] = source[i];
        }
        for (; i < n; i++) {
            part[i] = 0;
        }
        part[n] = 0;
        m_stack.PushString(part, (unsigned int)n + 1);
    } else {
        m_stack.PushText("");  // The original's empty string at 0x00270F0B.
    }
}
