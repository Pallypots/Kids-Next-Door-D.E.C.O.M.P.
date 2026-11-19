# The Flash action script machine: its dispatch loop, handlers, and the values' and stack's methods only they use. Each mutant's text runs from the head of its function to the text changed, so that it
# occurs once. Dropped as equivalent, each with its reason:
#   * The local value's constructor calling Clear: the base constructor, just run, sets every field the same.
#   * SetReal comparing as float rather than double: the integer is the real truncated, so the two agree wherever the real fits an int, and nowhere else can either be equal but at -2^31.
#   * Equals written as !(x != y): the same under NaN.
#   * TextToReal handing atof a text with no digits, or a bare "1e" or "1e+": atof gives 0, or stops before the 'e', the same as what is handed now.
#   * Run reading code 0x80 as a short instruction: 0x80 has no handler, so its length and data are never used.
#   * SetTarget2 passing an undefined path's Text() for "": the text of undefined is "".
import os
import sys

_SRC = os.path.join(os.path.dirname(os.path.abspath(sys._getframe().f_code.co_filename)), "..", "..", "..", "src", "at")
V = "AtFlashScriptVM.cpp"
H = "AtFlashScriptHost.cpp"
_cache = {}


def _text(f):
    if f not in _cache:
        with open(os.path.join(_SRC, f), encoding="utf-8") as h:
            _cache[f] = h.read()
    return _cache[f]


def m(name, head, old, new, f=V):
    """A mutant replacing the first `old` after the function head `head`."""
    s = _text(f)
    a = s.index(head)
    b = s.index(old, a)
    text = s[a:b + len(old)]
    return (name, f, text, text[:len(text) - len(old)] + new)


def gone(name, head, old, f=V):
    return m(name, head, old, "", f)


VAL = "AtFpActionScriptValue::"
LOC = "AtFpActionScriptLocalValue::"
STK = "AtFpActionScriptStack::"
VM = "AtFpActionScriptVM::"
RUN = "void AtFpActionScriptVM::Run("

MUTANTS = [
    # Helpers.
    m("ReadU16 high byte", "unsigned int ReadU16(", "<< 8", "<< 9"),
    m("ReadU32 top byte", "unsigned int ReadU32(", "<< 24", "<< 23"),
    m("CompareText sign", "int CompareText(", "x < y ? -1 : 1", "x < y ? 1 : -1"),
    m("CompareText signed bytes", "int CompareText(", "const unsigned char x = (unsigned char)*a, y = (unsigned char)*b;", "const signed char x = (signed char)*a, y = (signed char)*b;"),
    m("TextToReal whole text", "float TextToReal(", "return digits > 0 ? AtNarrowToFloat(number) : 0.0f;", "return digits > 0 ? AtNarrowToFloat(text) : 0.0f;"),
    m("TextToReal whitespace", "float TextToReal(", "*p == ' ' ||", "*p == '\\t' ||"),
    m("TextToReal exponent", "float TextToReal(", "if (digits > 0 && (*p == 'e' || *p == 'E'))", "if (digits > 0 && *p == 'e')"),
    m("IntToText sign", "void IntToText(", "*out++ = '-';", "*out++ = '+';"),
    m("Truncate range", "int Truncate(", "return 0;", "return (int)0x80000000;"),
    m("Truncate to 32", "int Truncate(", "return (int)(unsigned int)(unsigned long long)(long long)value;", "return (int)value;"),
    m("PushOrRaise code", "void PushOrRaise(", "AtRaise(-2,", "AtRaise(-3,"),
    # Value forms.
    m("Clear type", "void " + VAL + "Clear()", "m_type = kTypeNone;", "m_type = kTypeInt;"),
    m("Clear int", "void " + VAL + "Clear()", "m_int = 0;", "m_int = 1;"),
    m("Clear text-real", "void " + VAL + "Clear()", "m_haveTextReal = 0;", "m_haveTextReal = 1;"),
    m("Clear heap", "void " + VAL + "Clear()", "m_heap = 0;", "m_heap = &m_int;"),
    m("SetInt type", "void " + VAL + "SetInt(", "m_type = kTypeInt;", "m_type = kTypeReal;"),
    m("SetInt flag", "void " + VAL + "SetInt(", "m_haveTextInt = 0;", "m_haveTextInt = 1;"),
    m("SetInt int-real flag", "void " + VAL + "SetInt(", "m_haveIntReal = 0;", "m_haveIntReal = 1;"),
    m("SetReal int-real flag", "void " + VAL + "SetReal(", "m_haveIntReal = 1;", "m_haveIntReal = 0;"),
    m("SetReal text flag", "void " + VAL + "SetReal(", "m_haveTextReal = 0;", "m_haveTextReal = 1;"),
    m("ToReal flag", "float " + VAL + "ToReal()", "if (m_haveIntReal == 0) {", "if (1) {"),
    m("ToReal int", "float " + VAL + "ToReal()", "m_real = (float)m_int;", "m_real = (float)(m_int + 1);"),
    m("ToReal text cached", "float " + VAL + "ToReal()", "m_haveTextReal == 0", "m_haveTextInt == 0"),
    m("ToInt flag", "int " + VAL + "ToInt()", "if (m_haveIntReal == 0) {", "if (m_haveTextReal == 0) {"),
    m("ToInt text cached", "int " + VAL + "ToInt()", "m_haveTextInt = 1;", "m_haveTextInt = 0;"),
    m("ToInt text", "int " + VAL + "ToInt()", "m_int = AtNarrowToInt(m_text);", "m_int = AtNarrowToInt(m_text + 1);"),
    m("ToText real room", "char* " + VAL + "ToText()", "Reserve(0x20);", "Reserve(0x21);"),
    m("ToText real cached", "char* " + VAL + "ToText()", "m_haveTextReal = 1;", "m_haveTextReal = 0;"),
    m("ToText int cached", "char* " + VAL + "ToText()", "if (m_haveTextInt == 0) {", "if (m_haveTextReal == 0) {"),
    m("ToText undefined", "char* " + VAL + "ToText()", 'return (char*)"";', 'return (char*)"u";'),
    m("Length undefined", "unsigned int " + VAL + "Length()", "return 0;", "return 1;"),
    m("Length text", "unsigned int " + VAL + "Length()", "return ::Length(m_text);", "return ::Length(m_text) + (m_type == kTypeInt);"),
    m("IsInteger undefined", "int " + VAL + "IsInteger()", "m_type == kTypeInt || m_type == kTypeNone", "m_type == kTypeInt"),
    m("IsInteger real", "int " + VAL + "IsInteger()", "return (double)truncated == (double)m_real ? 1 : 0;", "return 0;"),
    m("IsInteger other type", "int " + VAL + "IsInteger()", "    if (m_type != kTypeString) {\n        return 0;", "    if (m_type != kTypeString) {\n        return 1;"),
    m("IsInteger minus", "int " + VAL + "IsInteger()", "if (i == 0 && m_text[0] == '-') {", "if (m_text[i] == '-') {"),
    m("IsInteger digits", "int " + VAL + "IsInteger()", "c > '9'", "c > '8'"),
    m("CopyFrom real", VAL + "CopyFrom(", "m_unknown14 = from->m_unknown14;", ""),
    m("CopyFrom flag", VAL + "CopyFrom(", "m_haveIntReal = from->m_haveIntReal;", ""),
    m("CopyFrom text by real", VAL + "CopyFrom(", " || m_haveTextReal != 0", ""),
    m("CopyFrom text by int", VAL + "CopyFrom(", " || m_haveTextInt != 0", ""),
    m("CopyFrom room", VAL + "CopyFrom(", "Reserve(from->m_capacity);", "Reserve(from->m_capacity + 0x40);"),
    m("SetText(AtText) null type", "void " + VAL + "SetText(AtText* text)", "m_type = kTypeNone;", "m_type = kTypeString;"),
    m("SetText(AtText) size", "void " + VAL + "SetText(AtText* text)", "text->m_end - text->m_start + 1", "text->m_end - text->m_start + 2"),
    m("SetText(AtText) flags", "void " + VAL + "SetText(AtText* text)", "    text->CopyNarrow(m_text, size);\n    m_haveTextInt = 0;", "    text->CopyNarrow(m_text, size);\n    m_haveTextInt = 1;"),
    m("SetText(char) null", "void " + VAL + "SetText(const char* text)", "m_type = kTypeNone;", "m_type = kTypeInt;"),
    m("SetText(char) room", "void " + VAL + "SetText(const char* text)", "Reserve(::Length(text) + 1);", "Reserve(::Length(text) + 0x30);"),
    m("SetText(char) type", "void " + VAL + "SetText(const char* text)", "    m_type = kTypeString;\n    CopyText", "    m_type = kTypeNone;\n    CopyText"),
    # A handler's own value.
    m("local dtor buffer", LOC + "~AtFpActionScriptLocalValue()", "if (m_text == m_buffer) {", "if (0) {"),
    m("local Reserve limit", "void " + LOC + "Reserve(", "if (size < 0xFF &&", "if (size < 0xFE &&"),
    m("local Reserve capacity", "void " + LOC + "Reserve(", "        m_capacity = size;\n        m_text[0] = 0;\n        return;", "        m_capacity = 0xFF;\n        m_text[0] = 0;\n        return;"),
    m("local Reserve room", "void " + LOC + "Reserve(", "m_capacity < 0xFF) ? 0xFF : m_capacity;", "m_capacity < 0xFF) ? m_capacity : m_capacity;"),
    m("local Reserve signed", "void " + LOC + "Reserve(", "if ((int)size <= (int)room) {", "if (size <= room) {"),
    m("local Reserve frees", "void " + LOC + "Reserve(", "    if (m_text != 0 && m_text != m_buffer) {\n        operator delete(m_text);\n    }", ""),
    m("local Reserve heap", "void " + LOC + "Reserve(", "PushOrRaise(0, AtIoTokenFile::s_heap);", "PushOrRaise(m_heap, AtIoTokenFile::s_heap);"),
    m("local Reserve raise", "void " + LOC + "Reserve(", "AtRaise(-4, 0, 0, 0);", "AtRaise(-5, 0, 0, 0);"),
    m("local Reserve keeps size", "void " + LOC + "Reserve(", "    m_capacity = size;\n    m_text[0] = 0;\n}", "    m_capacity = size + 1;\n    m_text[0] = 0;\n}"),
    m("local MoveTo copy", "void " + LOC + "MoveTo(", "if (m_text == m_buffer) {", "if (0) {"),
    m("local SetString frees", "void " + LOC + "SetString(", "if (m_text != 0 && m_text != m_buffer) {", "if (0) {"),
    m("local SetString capacity", "void " + LOC + "SetString(", "m_capacity = capacity;", "m_capacity = capacity + 1;"),
    m("local SetString flag", "void " + LOC + "SetString(", "m_haveTextReal = 0;", "m_haveTextReal = 1;"),
    m("local Reset buffer", "void " + LOC + "Reset()", "if (m_text == m_buffer) {", "if (0) {"),
    # The stack.
    m("Pop empty", "int " + STK + "Pop(", "if (m_top < 0) {", "if (m_top < 1) {"),
    m("Pop top", "int " + STK + "Pop(", "m_top--;", ""),
    m("Discard empty", "int " + STK + "Discard()", "if (m_top < 0) {", "if (m_top < -1) {"),
    m("PushInt full", "int " + STK + "PushInt(", "if (m_top >= (int)(m_capacity - 1)) {", "if (m_top >= (int)(m_capacity - 2)) {"),
    m("PushInt heap", "int " + STK + "PushInt(", "m_heap != 0 ? m_heap : s_heap;", "s_heap;"),
    m("PushInt returns", "int " + STK + "PushInt(", "    return 1;\n}", "    return 2;\n}"),
    m("PushReal full", "int " + STK + "PushReal(", "if (m_top >= (int)(m_capacity - 1)) {", "if (m_top > (int)(m_capacity - 1)) {"),
    m("PushReal heap", "int " + STK + "PushReal(", "m_heap != 0 ? m_heap : s_heap;", "m_heap;"),
    m("PushText(char) full", "int " + STK + "PushText(const char* text)", "if (m_top >= (int)(m_capacity - 1)) {", "if (m_top >= (int)m_capacity) {"),
    m("PushText(char) heap", "int " + STK + "PushText(const char* text)", "m_heap != 0 ? m_heap : s_heap;", "s_heap;"),
    m("PushText(AtText) full", "int " + STK + "PushText(AtText* text)", "if (m_top >= (int)(m_capacity - 1)) {", "if (m_top >= (int)(m_capacity - 2)) {"),
    m("PushText(AtText) heap", "int " + STK + "PushText(AtText* text)", "m_heap != 0 ? m_heap : s_heap;", "s_heap;"),
    m("PushUndefined full", "int " + STK + "PushUndefined()", "if (m_top >= (int)(m_capacity - 1)) {", "if (m_top >= (int)(m_capacity - 2)) {"),
    m("PushUndefined heap", "int " + STK + "PushUndefined()", "m_heap != 0 ? m_heap : s_heap;", "s_heap;"),
    m("PushUndefined type", "int " + STK + "PushUndefined()", "AtFpActionScriptValue::kTypeNone;", "AtFpActionScriptValue::kTypeString;"),
    m("PushString full", "int " + STK + "PushString(", "if (m_top >= (int)(m_capacity - 1)) {", "if (m_top >= (int)(m_capacity - 2)) {"),
    m("PushString heap", "int " + STK + "PushString(", "m_heap != 0 ? m_heap : s_heap;", "s_heap;"),
    m("PushCopy full", "int " + STK + "PushCopy(", "if (m_top >= (int)(m_capacity - 1)) {", "if (m_top >= (int)(m_capacity - 2)) {"),
    m("PushCopy heap", "int " + STK + "PushCopy(", "m_heap != 0 ? m_heap : s_heap;", "s_heap;"),
    m("PushFrom full", "int " + STK + "PushFrom(", "if (m_top >= (int)(m_capacity - 1)) {", "if (m_top >= (int)(m_capacity - 2)) {"),
    m("PushFrom heap", "int " + STK + "PushFrom(", "m_heap != 0 ? m_heap : s_heap;", "s_heap;"),
    m("PushFrom move", "int " + STK + "PushFrom(", "from->MoveTo(&m_values[m_top]);", "m_values[m_top].CopyFrom(from);"),
    # The dispatch loop.
    m("Run next frame", RUN, "GetCurrentFrame() + 1", "GetCurrentFrame() + 2"),
    m("Run previous frame", RUN, "GetCurrentFrame() - 1", "GetCurrentFrame()"),
    m("Run play", RUN, "m_application->m_display.SetPlaying(1);", "m_application->m_display.SetPlaying(2);"),
    gone("Run stop sounds", RUN, "            m_application->m_display.StopSounds();"),
    m("Run add", RUN, "            ActionAdd();", "            ActionMultiply();"),
    m("Run subtract", RUN, "            ActionSubtract();", "            ActionAdd();"),
    m("Run divide", RUN, "            ActionDivide();", "            ActionSubtract();"),
    m("Run equals", RUN, "            ActionEquals();", "            ActionLess();"),
    m("Run less", RUN, "            ActionLess();", "            ActionEquals();"),
    m("Run and", RUN, "            ActionAnd();", "            ActionOr();"),
    m("Run or", RUN, "            ActionOr();", "            ActionAnd();"),
    gone("Run not", RUN, "            ActionNot();"),
    gone("Run get variable", RUN, "            ActionGetVariable();"),
    gone("Run set variable", RUN, "            ActionSetVariable();"),
    m("Run string less", RUN, "            ActionStringLess();", "            ActionStringEquals();"),
    gone("Run string add", RUN, "            ActionStringAdd();"),
    gone("Run char to ascii", RUN, "            ActionCharToAscii();"),
    gone("Run ascii to char", RUN, "            ActionAsciiToChar();"),
    gone("Run to integer", RUN, "            ActionToInteger();"),
    gone("Run string length", RUN, "            ActionStringLength();"),
    gone("Run string extract", RUN, "            ActionStringExtract();"),
    m("Run get url target", RUN, "LoadMovie(url, url + Length(url) + 1);", "LoadMovie(url, url + Length(url));"),
    m("Run get url2 method", RUN, "ActionGetURL2(data[0]);", "ActionGetURL2(data[1]);"),
    gone("Run get property", RUN, "            ActionGetProperty();"),
    gone("Run set property", RUN, "            ActionSetProperty();"),
    gone("Run clone sprite", RUN, "            ActionCloneSprite();"),
    gone("Run remove sprite", RUN, "            ActionRemoveSprite();"),
    gone("Run random", RUN, "            ActionRandomNumber();"),
    gone("Run get time", RUN, "            ActionGetTime();"),
    gone("Run trace", RUN, "            ActionTrace();"),
    m("Run goto frame", RUN, "GotoFrame((int)ReadU16(data));", "GotoFrame((int)data[0]);"),
    m("Run goto frame stops", RUN, "            m_application->m_display.GotoFrame((int)ReadU16(data));\n            m_application->m_display.SetPlaying(0);", "            m_application->m_display.GotoFrame((int)ReadU16(data));"),
    m("Run stop", RUN, "        case kStop:\n            m_application->m_display.SetPlaying(0);", "        case kStop:\n            m_application->m_display.SetPlaying(1);"),
    gone("Run wait for frame", RUN, "            ActionWaitForFrame2(data[0]);"),
    gone("Run goto label", RUN, "            ActionGotoLabel(data);"),
    m("Run goto frame2", RUN, "ActionGotoFrame2(data[0]);", "ActionGotoFrame2(1);"),
    gone("Run set target", RUN, "            ActionSetTarget(data);"),
    gone("Run set target2", RUN, "            ActionSetTarget2();"),
    gone("Run pop", RUN, "            m_stack.Discard();"),
    m("Run push length", RUN, "ActionPush(length, data);", "ActionPush(length > 6 ? length - 6 : length, data);"),
    m("Run jump bound", RUN, "if (target < script->m_read) {", "if (target <= script->m_read) {"),
    m("Run jump target", RUN, "index = target - 1;", "index = target;"),
    m("Run if", RUN, "ActionIf((unsigned short)ReadU16(data), &index, script);", "ActionIf((unsigned short)data[0], &index, script);"),
    m("Run call stops", RUN, "stop = ActionCall();", "ActionCall();"),
    m("Run end", RUN, "} while (action != kEnd && stop == 0);", "} while (action != kEnd);"),
    # Control.
    m("If integer", "void " + VM + "ActionIf(", "condition.Int() != 0", "condition.Real() != 0"),
    m("If bound", "void " + VM + "ActionIf(", "target < script->m_read", "target <= script->m_read"),
    m("If target", "void " + VM + "ActionIf(", "*index = target - 1;", "*index = target;"),
    m("Call integer", "int " + VM + "ActionCall()", "if (frame.IsInteger()) {", "if (frame.m_type == AtFpActionScriptValue::kTypeInt) {"),
    m("Call frame", "int " + VM + "ActionCall()", "frame.Int() - 1", "frame.Int()"),
    m("LoadMovie extension", "void " + VM + "LoadMovie(", "name[length - 4] = 0;", "name[length - 3] = 0;"),
    m("LoadMovie short", "void " + VM + "LoadMovie(", "if (length > 4) {", "if (length > 3) {"),
    m("LoadMovie empty", "void " + VM + "LoadMovie(", "} else if (length != 0) {", "} else {"),
    m("LoadMovie cut", "void " + VM + "LoadMovie(", "name[sizeof(name) - 1] = 0;", "name[sizeof(name) - 2] = 0;"),
    m("GetTime", "void " + VM + "ActionGetTime()", "PushInt((int)AtPlatformMilliseconds());", "PushInt((int)AtPlatformMilliseconds() + 1);"),
    gone("WaitForFrame2 pops", "void " + VM + "ActionWaitForFrame2(", "    m_stack.Pop(&frame);"),
    m("GotoLabel string id", "void " + VM + "ActionGotoLabel(", "} else if (kind == kPushStringId) {", "} else if (kind == 0x21) {"),
    m("GotoLabel not found", "void " + VM + "ActionGotoLabel(", "        if (text == 0) {\n            return;", "        if (text == 0) {\n            text = 0;"),
    m("GotoLabel stops", "void " + VM + "ActionGotoLabel(", "SetPlaying(0);", "SetPlaying(1);"),
    m("GotoLabel text", "void " + VM + "ActionGotoLabel(", "label.SetText((const char*)data + 2);", "label.SetText((const char*)data + 3);"),
    m("GotoFrame2 play", "void " + VM + "ActionGotoFrame2(", "SetPlaying(play);", "SetPlaying(!play);"),
    m("SetTarget undefined", "void " + VM + "ActionSetTarget(const", 'display.SetTarget("");', 'display.SetTarget("x");'),
    m("SetTarget empty", "void " + VM + "ActionSetTarget(const", "} else if (path.Length() == 0) {", "} else if (path.Length() == 1) {"),
    m("SetTarget context", "void " + VM + "ActionSetTarget(const", "contexts.m_entries[contexts.m_count - 1].m_context", "contexts.m_entries[contexts.m_count - 1].m_target"),
    m("SetTarget unknown kind", "void " + VM + "ActionSetTarget(const", "    } else {\n        return;\n    }", "    }"),
    m("SetTarget string id", "void " + VM + "ActionSetTarget(const", "if (text != 0) {", "if (text != 0 && 0) {"),
    m("SetTarget2 empty", "void " + VM + "ActionSetTarget2()", "} else if (path.Length() == 0) {", "} else if (0) {"),
    # Push.
    m("Push string size", "void " + VM + "ActionPush(", "n > 0x20 ? n : 0x20", "n > 0x10 ? n : 0x10"),
    m("Push string heap", "void " + VM + "ActionPush(", "AtPushHeap(m_stack.m_heap != 0 ? m_stack.m_heap : AtFpActionScriptStack::s_heap);", "AtPushHeap(AtFpActionScriptStack::s_heap);"),
    m("Push string raise", "void " + VM + "ActionPush(", "AtRaise(-4, 0, 0, 0);", "AtRaise(-2, 0, 0, 0);"),
    m("Push string step", "void " + VM + "ActionPush(", "i += n + 2;", "i += n + 3;"),
    m("Push id step", "void " + VM + "ActionPush(", "i += 10;", "i += 11;"),
    m("Push real", "void " + VM + "ActionPush(", "m_stack.PushReal(ReadReal(data + i + 2));", "m_stack.PushInt((int)ReadU32(data + i + 2));"),
    m("Push int", "void " + VM + "ActionPush(", "m_stack.PushInt((int)ReadU32(data + i + 2));", "m_stack.PushReal(ReadReal(data + i + 2));"),
    m("Push int step", "void " + VM + "ActionPush(", "            m_stack.PushInt((int)ReadU32(data + i + 2));\n            i += 6;", "            m_stack.PushInt((int)ReadU32(data + i + 2));\n            i += 7;"),
    # Variables, properties, sprites.
    m("GetVariable pushes", "void " + VM + "ActionGetVariable()", "m_stack.PushFrom(&value);", "m_stack.PushCopy(&value);"),
    m("SetVariable order", "void " + VM + "ActionSetVariable()", "    m_stack.Pop(&value);\n    m_stack.Pop(&name);", "    m_stack.Pop(&name);\n    m_stack.Pop(&value);"),
    m("GetProperty order", "void " + VM + "ActionGetProperty()", "    m_stack.Pop(&property);\n    m_stack.Pop(&target);", "    m_stack.Pop(&target);\n    m_stack.Pop(&property);"),
    m("GetProperty pushes", "void " + VM + "ActionGetProperty()", "m_stack.PushCopy(&result);", "m_stack.PushFrom(&result);"),
    m("SetProperty order", "void " + VM + "ActionSetProperty()", "    m_stack.Pop(&property);\n    m_stack.Pop(&target);", "    m_stack.Pop(&target);\n    m_stack.Pop(&property);"),
    m("CloneSprite pops", "void " + VM + "ActionCloneSprite()", "    m_stack.Pop(&source);", ""),
    m("RemoveSprite pops", "void " + VM + "ActionRemoveSprite()", "    m_stack.Pop(&target);", ""),
    m("Trace heap", "void " + VM + "ActionTrace()", "args.m_heap = m_heap != 0 ? m_heap : s_heap;", "args.m_heap = s_heap;"),
    gone("Trace clear", "void " + VM + "ActionTrace()", "    args.Clear();"),
    m("Trace name", "void " + VM + "ActionTrace()", '"TraceString"', '"TraceStrinG"'),
    m("GetURL2 mask", "void " + VM + "ActionGetURL2(", "switch (method & ~3u) {", "switch (method & ~1u) {"),
    m("GetURL2 0x40", "void " + VM + "ActionGetURL2(", "    case 0x40: {", "    case 0x44: {"),
    m("GetURL2 0x80", "void " + VM + "ActionGetURL2(", "    case 0x80:\n", "    case 0x84:\n"),
    m("GetURL2 movie order", "void " + VM + "ActionGetURL2(", "LoadMovie(url.Text(), targetText);", "LoadMovie(targetText, url.Text());"),
    m("GetURL2 rfc args", "void " + VM + "ActionGetURL2(", "m_application->m_rfc.Call(name, args);", "m_application->m_rfc.Call(name, 0);"),
    gone("GetURL2 unsupported", "void " + VM + "ActionGetURL2(", '        AtFatal("AtFpActionScriptVM::vActionGetURL2 - Unsupported method", 0, 0, 0);'),
    # Arithmetic and logic.
    m("Less", "void " + VM + "ActionLess()", "x < b.Real()", "x <= b.Real()"),
    m("Less order", "void " + VM + "ActionLess()", "    m_stack.Pop(&b);\n    m_stack.Pop(&a);", "    m_stack.Pop(&a);\n    m_stack.Pop(&b);"),
    m("Equals", "void " + VM + "ActionEquals()", "x == b.Real() ? 1 : 0", "x == b.Real() ? 1 : 2"),
    m("Not", "void " + VM + "ActionNot()", "value.Real() == 0.0f ? 1 : 0", "value.Int() == 0 ? 1 : 0"),
    m("And", "void " + VM + "ActionAnd()", "a.Real() != 0.0f && b.Real() != 0.0f", "a.Real() != 0.0f"),
    m("And by integer", "void " + VM + "ActionAnd()", "a.Real() != 0.0f && b.Real() != 0.0f", "!(a.Real() == 0.0f) && b.Int() != 0"),
    m("Or", "void " + VM + "ActionOr()", "a.Real() != 0.0f || b.Real() != 0.0f", "b.Real() != 0.0f"),
    m("Add", "void " + VM + "ActionAdd()", "m_stack.PushReal(b.Real() + x);", "m_stack.PushReal(b.Real() + x + 1);"),
    m("Subtract", "void " + VM + "ActionSubtract()", "m_stack.PushReal(x - b.Real());", "m_stack.PushReal(b.Real() - x);"),
    m("Divide", "void " + VM + "ActionDivide()", "m_stack.PushReal(x / b.Real());", "m_stack.PushReal(b.Real() / x);"),
    m("Multiply", "void " + VM + "ActionMultiply()", "m_stack.PushReal(b.Real() * x);", "m_stack.PushReal((float)((double)b.Real() * x * 1.0000001));"),
    m("ToInteger", "void " + VM + "ActionToInteger()", "m_stack.PushInt(value.Int());", "m_stack.PushInt((int)value.Real());"),
    m("Random first word", "void " + VM + "ActionRandomNumber()", "AtNextRandomWord() >> 4;", "AtNextRandomWord() >> 3;"),
    m("Random combine", "void " + VM + "ActionRandomNumber()", "<< 8) + d;", "<< 8) + d + 1;"),
    m("Random unsigned", "void " + VM + "ActionRandomNumber()", "(int)(random % (unsigned int)limit.Int());", "(int)random % limit.Int();"),
    m("Random pushes", "void " + VM + "ActionRandomNumber()", "m_stack.PushCopy(&result);", "m_stack.PushInt(remainder);"),
    # Strings.
    m("StringAdd undefined", "void " + VM + "ActionStringAdd()", "a.m_type == AtFpActionScriptValue::kTypeNone ||", ""),
    m("StringAdd room", "void " + VM + "ActionStringAdd()", "a.m_capacity + b.m_capacity + 1", "a.m_capacity + b.m_capacity + 2"),
    gone("StringAdd converts first", "void " + VM + "ActionStringAdd()", "    a.ToText();\n"),
    m("StringAdd heap", "void " + VM + "ActionStringAdd()", "AtPushHeap(m_stack.m_heap != 0 ? m_stack.m_heap : AtFpActionScriptStack::s_heap);", "AtPushHeap(AtFpActionScriptStack::s_heap);"),
    m("StringAdd raise", "void " + VM + "ActionStringAdd()", "AtRaise(-4, 0, 0, 0);", "AtRaise(-2, 0, 0, 0);"),
    m("StringAdd order", "void " + VM + "ActionStringAdd()", "CopyText(joined, a.Text());", "CopyText(joined, b.Text());"),
    m("CharToAscii undefined", "void " + VM + "ActionCharToAscii()", "m_stack.PushUndefined();", "m_stack.PushInt(0);"),
    m("CharToAscii signed", "void " + VM + "ActionCharToAscii()", "(signed char)value.Text()[0]", "(unsigned char)value.Text()[0]"),
    m("AsciiToChar", "void " + VM + "ActionAsciiToChar()", "text[0] = (char)value.Int();", "text[0] = (char)value.Int(); text[1] = 'x';"),
    m("StringEquals undefined", "void " + VM + "ActionStringEquals()", "|| b.m_type == AtFpActionScriptValue::kTypeNone", ""),
    m("StringEquals", "void " + VM + "ActionStringEquals()", "CompareText(a.Text(), y) == 0 ? 1 : 0", "CompareText(a.Text(), y) <= 0 ? 1 : 0"),
    m("StringLess undefined", "void " + VM + "ActionStringLess()", "a.m_type == AtFpActionScriptValue::kTypeNone ||", ""),
    m("StringLess", "void " + VM + "ActionStringLess()", "CompareText(a.Text(), y) < 0 ? 1 : 0", "CompareText(y, a.Text()) < 0 ? 1 : 0"),
    m("StringLength undefined", "void " + VM + "ActionStringLength()", "m_stack.PushUndefined();", "m_stack.PushInt(0);"),
    m("StringLength", "void " + VM + "ActionStringLength()", "m_stack.PushInt((int)Length(value.Text()));", "m_stack.PushInt((int)value.m_capacity);"),
    m("StringExtract undefined", "void " + VM + "ActionStringExtract()", "m_stack.PushUndefined();", 'm_stack.PushText("");'),
    m("StringExtract index 0", "void " + VM + "ActionStringExtract()", "    if (from == 0) {\n        from = 1;", "    if (from == 0) {\n        from = 0;"),
    m("StringExtract count", "void " + VM + "ActionStringExtract()", "if (n > 0 && from > 0", "if (n >= 0 && from > 0"),
    m("StringExtract last", "void " + VM + "ActionStringExtract()", "from < (int)string.Length()", "from <= (int)string.Length()"),
    m("StringExtract index sign", "void " + VM + "ActionStringExtract()", "&& from > 0 &&", "&& from != 0 &&"),
    m("StringExtract from", "void " + VM + "ActionStringExtract()", "string.Text() + from - 1;", "string.Text() + from;"),
    m("StringExtract pads", "void " + VM + "ActionStringExtract()", "        for (; i < n; i++) {\n            part[i] = 0;\n        }", ""),
    m("StringExtract capacity", "void " + VM + "ActionStringExtract()", "m_stack.PushString(part, (unsigned int)n + 1);", "m_stack.PushString(part, (unsigned int)n + 2);"),
    m("StringExtract heap", "void " + VM + "ActionStringExtract()", "AtPushHeap(m_stack.m_heap != 0 ? m_stack.m_heap : AtFpActionScriptStack::s_heap);", "AtPushHeap(AtFpActionScriptStack::s_heap);"),
    m("StringExtract empty", "void " + VM + "ActionStringExtract()", 'm_stack.PushText("");', 'm_stack.PushUndefined();'),
    # The C library's "%g".
    m("%g exponent digits", "void AtFormatGeneral(", "*to++ = (char)('0' + a / 100);", "", H),
    m("%g rounding", "void AtFormatGeneral(", "if (scientific[7] >= '5') {", "if (scientific[7] > '5') {", H),
    m("%g carry", "void AtFormatGeneral(", "            exponent++;", "", H),
    m("%g e-style low", "void AtFormatGeneral(", "if (exponent < -4 || exponent >= 6) {", "if (exponent < -5 || exponent >= 6) {", H),
    m("%g e-style high", "void AtFormatGeneral(", "if (exponent < -4 || exponent >= 6) {", "if (exponent < -4 || exponent >= 7) {", H),
    m("%g trailing zeros", "void AtFormatGeneral(", "while (count > 1 && digits[count - 1] == '0') {", "while (0) {", H),
    m("%g infinity", "void AtFormatGeneral(", 'Copy(out, negative ? "-1.#INF" : "1.#INF");', 'Copy(out, negative ? "-inf" : "inf");', H),
    m("%g indefinite", "void AtFormatGeneral(", 'Copy(out, "-1.#IND");', 'Copy(out, "-1.#QNAN");', H),
    m("%g quiet NaN", "void AtFormatGeneral(", 'Copy(out, negative ? "-1.#QNAN" : "1.#QNAN");', 'Copy(out, "nan");', H),
    m("%g small zeros", "void AtFormatGeneral(", "for (int i = 0; i < -exponent - 1; i++) {", "for (int i = 0; i < -exponent; i++) {", H),
    m("%g point", "void AtFormatGeneral(", "if (count > exponent + 1) {", "if (count >= exponent + 1) {", H),
]

SUITE = {"target": "oracle_asvm", "subsystem": "at/asvm", "src": "at", "var": "KND_AT_SRC", "cases": "tok", "mutants": MUTANTS}
