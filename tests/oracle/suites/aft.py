# The Flash texts (.AFT, the fonts, the formats' load) and scripts (.AFA, the action script machine's state, values and stack). Each mutant's text runs from the head of its function to the text changed, so that it
# occurs once. Dropped as equivalent, each with its reason:
#   * AtFlashText::Read setting m_read: ReadData, which it has just called, set it.
#   * ResetLinks in AtFlashText's constructor, and the first of the two in Reset: the base constructor does the same, and the second does it again.
#   * The nulling of m_refs, m_fonts and m_fontLoader inside ClearFonts' branches, and of m_instructions and m_bytes inside AtFpScript::Free's: the stores at the end of each do it again.
#   * The texts' Clear nulling the list's entries: the base's Clear frees the list next.
#   * Removing the Close of the .AFT and .AFA readers: the token file's destructor, which runs at once, does the same close.
#   * The machine's destructor freeing its stack: the stack's own destructor, which runs next, frees it the same way.
#   * AtFpScript::Free nulling the instruction pointers: the list is freed right after, so what is written there is never read.
#   * The scripts' Clear calling each script's Free: the script's destructor, called next, does the same Free.
#   * Swapping the .AFT's String and Font2D ids: the keyword table and the switch are both this file's, so they swap together.
import os
import sys

# The runner executes this file without __file__; the code object still carries its path.
_SRC = os.path.join(os.path.dirname(os.path.abspath(sys._getframe().f_code.co_filename)), "..", "..", "..", "src", "at")
T = "AtFlashText.cpp"
S = "AtFlashScript.cpp"
_cache = {}


def _text(f):
    if f not in _cache:
        with open(os.path.join(_SRC, f), encoding="utf-8") as h:
            _cache[f] = h.read()
    return _cache[f]


def m(name, f, head, old, new):
    """A mutant replacing the first `old` after the function head `head`."""
    s = _text(f)
    a = s.index(head)
    b = s.index(old, a)
    text = s[a:b + len(old)]
    return (name, f, text, text[:len(text) - len(old)] + new)


def gone(name, f, head, old):
    return m(name, f, head, old, "")


TC = "AtFlashText::AtFlashText() {"
TR = "void AtFlashText::Reset() {"
TD = "void AtFlashText::ReadData(AtIoTokenizer* tokenizer) {"
TL = "void AtFlashText::Release(AtFlashResourceTable* formats, AtFlashFontTable* fonts) {"
FC = "AtFlashFontTable::AtFlashFontTable() {"
FCL = "void AtFlashFontTable::ClearFonts() {"
FL = "void AtFlashFontTable::Load(const char* name, int mode, AtMaterialOwner* owner) {"
FR = "int AtFlashFontTable::ReleaseFont(const char* name) {"
XL = "void AtFlashTextFormatTable::Load(const char* name, int) {"
XC = "AtFlashTextTable::AtFlashTextTable() {"
XA = "void AtFlashTextTable::Allocate(unsigned int count) {"
XR = "void AtFlashTextTable::Release(unsigned int index) {"
XCL = "void AtFlashTextTable::ClearTexts() {"
XF = "void AtFlashTextTable::ReadFile(const char* name) {"
XLD = "void AtFlashTextTable::Load(const char* name, int mode, AtMaterialOwner* owner, int) {"

MUTANTS = [
    # The words.
    *[("aft words: %s" % w, T, '{%s, "%s"}' % (k, w), '{%s, "%sX"}' % (k, w)) for k, w in (("kNameSpace", "NameSpace"), ("kText", "Text"), ("kString", "String"), ("kFont2D", "Font2D"), ("kTextFormat", "TextFormat"), ("kVariable", "Variable"))],
    ("aft: bad field's report", T, "const int kBadField = 0x27;", "const int kBadField = 0x28;"),
    ("text directory", T, '    strcat(directory, "Text");\n    path.Push(directory);\n    return &path;', '    strcat(directory, "Texts");\n    path.Push(directory);\n    return &path;'),
    # A text.
    *[m("text ctor: " + f, T, TC, "    %s = %s;" % (f, v), "    %s = %s;" % (f, w)) for f, v, w in (("m_read", "0", "1"), ("m_unknown20", "0", "1"), ("m_unknown28", "0", "1"), ("m_unknown34", "0", "1"), ("m_unknown40", "0", "1"), ("m_unknown48", "0", "1"), ("m_format", "-1", "0"), ("m_heap", "0", "&m_read"))],
    m("text ctor: variable", T, TC, "    m_variable = 0;", "    m_variable = m_string;"),
    m("text ctor: string", T, TC, "    ((int*)m_string)[0] = 0;", "    ((int*)m_string)[0] = 1;"),
    m("text ctor: string end", T, TC, "    ((int*)m_string)[1] = 0;", "    ((int*)m_string)[1] = 1;"),
    m("text ctor: font", T, TC, "    ((int*)m_font)[0] = 0;", "    ((int*)m_font)[0] = 1;"),
    m("text ctor: font end", T, TC, "    ((int*)m_font)[1] = 0;", "    ((int*)m_font)[1] = 1;"),
    m("text reset: when not read", T, TR, "    if (m_read == 0) {", "    if (m_read == 0 && false) {"),
    gone("text reset: variable kept", T, TR, "        operator delete(m_variable);\n"),
    *[m("text reset: " + f, T, TR, "    %s = %s;" % (f, v), "    %s = %s;" % (f, w)) for f, v, w in (("m_read", "0", "1"), ("m_unknown20", "0", "1"), ("m_unknown28", "0", "1"), ("m_unknown34", "0", "1"), ("m_unknown40", "0", "1"), ("m_unknown48", "0", "1"), ("m_format", "-1", "0"), ("m_heap", "0", "&m_read"))],
    m("text reset: variable", T, TR, "\n    m_variable = 0;", "\n    m_variable = m_string;"),
    m("text reset: string", T, TR, "    ((int*)m_string)[0] = 0;", "    ((int*)m_string)[0] = 1;"),
    m("text reset: font end", T, TR, "    ((int*)m_font)[1] = 0;", "    ((int*)m_font)[1] = 1;"),
    m("text read: word", T, TD, "tokenizer->ExpectToken(kText);", "tokenizer->ExpectToken(kString);"),
    gone("text read: index", T, TD, "    m_index = tokenizer->ExpectInt();\n"),
    m("text read: brace", T, TD, "    tokenizer->ExpectLeftCurly();", "    tokenizer->Next();"),
    m("text read: string", T, TD, "tokenizer->ExpectName(m_string);", "tokenizer->ExpectName(m_font);"),
    m("text read: font", T, TD, "tokenizer->ExpectName(m_font);", "tokenizer->ExpectName(m_string);"),
    m("text read: format", T, TD, "m_format = tokenizer->ExpectInt();", "m_index = tokenizer->ExpectInt();"),
    gone("text read: variable", T, TD, "            tokenizer->ExpectStringCopy(&m_variable);\n"),
    m("text read: bad word", T, TD, "            tokenizer->Error(kBadField);", "            tokenizer->Report(kBadField);"),
    m("text read: read noted", T, TD, "    m_read = 1;\n}", "}"),
    m("text release: format", T, TL, "formats->Release((unsigned int)m_format);", "formats->Release(0);"),
    m("text release: +48", T, TL, "    m_unknown48 = 0;", "    m_unknown48 = 1;"),
    m("text release: font kept", T, TL, "    if (m_unknown28 == 0) {", "    if (m_unknown28 != 0) {"),
    m("text release: font's name", T, TL, "fonts->ReleaseFont(m_font);", "fonts->ReleaseFont(m_string);"),
    m("text release: +40", T, TL, "        m_unknown40 = 0;", "        m_unknown40 = 1;"),
    m("text release: +28", T, TL, "    m_unknown28 = 0;\n    m_unknown34", "    m_unknown28 = 1;\n    m_unknown34"),
    m("text release: +34", T, TL, "    m_unknown34 = 0;", "    m_unknown34 = 1;"),
    m("text release: +20", T, TL, "    m_unknown20 = 0;", "    m_unknown20 = 1;"),
    # The fonts.
    *[m("fonts ctor: " + f, T, FC, "    %s = 0;" % f, "    %s = %s;" % (f, w)) for f, w in (("m_loaded", "1"), ("m_owner", "(AtMaterialOwner*)8"), ("m_factory", "(AtFlashGraphicsFactory*)8"), ("m_refCount", "1"), ("m_heap", "&m_loaded"))],
    m("fonts ctor: refs", T, FC, "    m_refs = 0;", "    m_refs = (FontRef*)8;"),
    m("fonts clear: when not loaded", T, FCL, "    if (m_loaded == 0) {", "    if (m_loaded == 0 && false) {"),
    m("fonts clear: held", T, FCL, "if (m_refs[i].m_references > 0) {", "if (m_refs[i].m_references > 1) {"),
    gone("fonts clear: held not reported", T, FCL, '                AtFatal("Error: Font reference exists. Unable to release.", 0, 0, 0);\n'),
    gone("fonts clear: refs kept", T, FCL, "        operator delete(m_refs);\n"),
    gone("fonts clear: database kept", T, FCL, "        m_factory->FreeFontDatabase(m_fonts);\n"),
    gone("fonts clear: loader kept", T, FCL, "        m_factory->FreeFontLoader(m_fontLoader);\n"),
    gone("fonts clear: base", T, FCL, "    Clear();\n"),
    *[m("fonts clear: " + f, T, FCL, "    }\n    Clear();\n" + "".join("    %s = 0;\n" % g for g in ("m_loaded", "m_fonts", "m_fontLoader", "m_owner", "m_factory", "m_refs", "m_refCount", "m_heap")[:i]) + "    %s = 0;" % f,
        "    }\n    Clear();\n" + "".join("    %s = 0;\n" % g for g in ("m_loaded", "m_fonts", "m_fontLoader", "m_owner", "m_factory", "m_refs", "m_refCount", "m_heap")[:i]) + "    %s = %s;" % (f, w))
      for i, (f, w) in enumerate((("m_loaded", "1"), ("m_fonts", "(AtFontDatabase*)8"), ("m_fontLoader", "(AtFlashFontLoader*)8"), ("m_owner", "(AtMaterialOwner*)8"), ("m_factory", "(AtFlashGraphicsFactory*)8"), ("m_refs", "(FontRef*)8"), ("m_refCount", "1"), ("m_heap", "&m_loaded")))],
    gone("fonts load: not cleared", T, FL, "        ClearFonts();\n"),
    gone("fonts load: no init", T, FL, "    Init();\n"),
    m("fonts load: directory", T, FL, 'strcat(directory, "Text");', 'strcat(directory, "Texts");'),
    m("fonts load: extension", T, FL, "{'A', 'G', 'F', 0}", "{'A', 'G', 'X', 0}"),
    m("fonts load: format", T, FL, 'AtFormat(file, "%s.%s", name, extension);', 'AtFormat(file, "%s", name);'),
    m("fonts load: missing", T, FL, "if (fs->IsFileMissing(file) == 0) {", "if (fs->IsFileMissing(file) != 0) {"),
    m("fonts load: owner", T, FL, "        m_owner = owner;", "        m_owner = 0;"),
    m("fonts load: loader's mode", T, FL, "m_fontLoader->Load(m_owner, name, mode);", "m_fontLoader->Load(m_owner, name, 0);"),
    m("fonts load: loader's name", T, FL, "m_fontLoader->Load(m_owner, name, mode);", 'm_fontLoader->Load(m_owner, "Fon", mode);'),
    m("fonts load: database's third", T, FL, "m_fonts->Load(m_owner, name, 0);", "m_fonts->Load(m_owner, name, mode);"),
    m("fonts load: count", T, FL, "m_refCount = m_fonts->m_count;", "m_refCount = m_fonts->m_count + 1;"),
    m("fonts load: heap", T, FL, "void* heap = TableHeap(this);", "void* heap = AtFlashResourceTable::s_defaultHeap;"),
    gone("fonts load: no heap unreported", T, FL, "            AtRaise(-2, 0, 0, 0);\n"),
    gone("fonts load: no push", T, FL, "        AtPushHeap(heap);\n"),
    gone("fonts load: no pop", T, FL, "        AtPopHeap();\n"),
    gone("fonts load: no room unreported", T, FL, "            AtRaise(-4, 0, 0, 0);\n"),
    gone("fonts load: ref's font", T, FL, "            m_refs[i].m_font = 0;\n"),
    gone("fonts load: ref's count", T, FL, "            m_refs[i].m_references = 0;\n"),
    gone("fonts load: no pop of the path", T, FL, "    path.Pop(0);\n"),
    m("fonts load: loaded", T, FL, "    m_loaded = 1;", "    m_loaded = 2;"),
    m("release font: none", T, FR, "        return 0;", "        return 1;"),
    m("release font: kind", T, FR, "if ((font->m_flags & AtFont::kDemandLoad) != 0) {", "if ((font->m_flags & 1) != 0) {"),
    m("release font: first found", T, FR, "                found = i;\n                break;", "                found = i;"),
    m("release font: none found", T, FR, "unsigned int found = 0;", "unsigned int found = 1;"),
    gone("release font: no count down", T, FR, "        m_refs[found].m_references--;\n"),
    m("release font: at none", T, FR, "if (m_refs[found].m_references == 0) {", "if (m_refs[found].m_references <= 1) {"),
    gone("release font: not unloaded", T, FR, "            font->FreeData();\n"),
    gone("release font: kept", T, FR, "            m_refs[found].m_font = 0;\n"),
    m("release font: answer", T, FR, "    return 1;", "    return 2;"),
    # The formats' load.
    gone("formats load: not released", T, XL, "        ReleaseFormats();\n"),
    gone("formats load: no init", T, XL, "    Init();\n"),
    m("formats load: no directory", T, XL, "    AtIoPath* path = PushText();\n    Read(name);\n    path->Pop(0);", "    Read(name);"),
    m("formats load: loaded", T, XL, "    m_loaded = 1;", "    m_loaded = 2;"),
    # The texts.
    m("texts ctor: loaded", T, XC, "    m_loaded = 0;", "    m_loaded = 1;"),
    m("texts ctor: heap", T, XC, "    m_heap = 0;", "    m_heap = &m_loaded;"),
    m("texts allocate: base", T, XA, "    AtFlashResourceTable::Allocate(count);", "    AtFlashResourceTable::Allocate(count + 1);"),
    m("texts allocate: heap", T, XA, "    void* heap = TableHeap(this);", "    void* heap = AtFlashResourceTable::s_defaultHeap;"),
    gone("texts allocate: no heap unreported", T, XA, "        AtRaise(-2, 0, 0, 0);\n"),
    gone("texts allocate: no push", T, XA, "    AtPushHeap(heap);\n"),
    gone("texts allocate: no pop", T, XA, "    AtPopHeap();\n"),
    gone("texts allocate: no room unreported", T, XA, "        AtRaise(-4, 0, 0, 0);\n"),
    m("texts allocate: list", T, XA, "        m_items[i] = &texts[i];", "        m_items[i] = &texts[0];"),
    m("texts allocate: text's heap", T, XA, "        texts[i].m_heap = TableHeap(this);", "        texts[i].m_heap = 0;"),
    m("texts release: last", T, XR, "if (text->m_references == 1) {", "if (text->m_references != 1) {"),
    gone("texts release: text kept", T, XR, "        text->Release(&m_formats, &m_fonts);\n"),
    gone("texts release: base", T, XR, "    AtFlashResourceTable::Release(index);\n"),
    m("texts clear: when not loaded", T, XCL, "    if (m_loaded == 0) {", "    if (m_loaded == 0 && false) {"),
    m("texts clear: groups", T, XCL, "for (unsigned int i = 0; i < m_groupCount; i++) {", "for (unsigned int i = 1; i < m_groupCount; i++) {"),
    gone("texts clear: strings", T, XCL, "    m_strings.Clear();\n"),
    gone("texts clear: fonts", T, XCL, "    m_fonts.ClearFonts();\n"),
    gone("texts clear: formats", T, XCL, "    m_formats.ReleaseFormats();\n"),
    gone("texts clear: texts kept", T, XCL, "        AtDeleteArray(static_cast<AtFlashText*>(m_items[0]));\n"),
    gone("texts clear: base", T, XCL, "    Clear();\n"),
    m("texts clear: loaded", T, XCL, "    m_loaded = 0;", "    m_loaded = 1;"),
    m("texts clear: heap", T, XCL, "    m_heap = 0;", "    m_heap = &m_loaded;"),
    m("aft: open", T, XF, 'file.Open(name, "AFT", 2, 0);', 'file.Open(name, "AFX", 2, 0);'),
    m("aft: report", T, XF, 'AtReportFileError(status, name, "AFT");', 'AtReportFileError(status, name, "AFX");'),
    m("aft: keywords", T, XF, "sizeof(s_keywords) / sizeof(s_keywords[0]), 0);", "sizeof(s_keywords) / sizeof(s_keywords[0]) - 1, 0);"),
    m("aft: name space", T, XF, "tokenizer->ExpectToken(kNameSpace);", "tokenizer->ExpectToken(kText);"),
    gone("aft: name not kept", T, XF, "    AtCopyName(space, m_name);\n"),
    gone("aft: name not freed", T, XF, "    operator delete(space);\n"),
    m("aft: Text", T, XF, "    tokenizer->ExpectToken(kText);", "    tokenizer->ExpectToken(kNameSpace);"),
    m("aft: bracket", T, XF, "    tokenizer->ExpectLeftBracket();", "    tokenizer->Next();"),
    m("aft: count", T, XF, "m_itemCount = (unsigned int)tokenizer->ExpectInt();", "m_itemCount = (unsigned int)tokenizer->ExpectInt() & 1;"),
    m("aft: close bracket", T, XF, "    tokenizer->ExpectRightBracket();", "    tokenizer->Next();"),
    m("aft: brace", T, XF, "    tokenizer->ExpectLeftCurly();", "    tokenizer->Next();"),
    m("aft: items", T, XF, "            m_items[i]->Read(tokenizer);", "            m_items[0]->Read(tokenizer);"),
    m("aft: end", T, XF, "    tokenizer->ExpectRightCurly();", "    tokenizer->Next();"),
    gone("texts load: not cleared", T, XLD, "        ClearTexts();\n"),
    gone("texts load: no init", T, XLD, "    Init();\n"),
    m("texts load: formats' heap", T, XLD, "    m_formats.m_heap = TableHeap(this);", "    m_formats.m_heap = 0;"),
    m("texts load: formats' name", T, XLD, "    m_formats.Load(name, mode);", '    m_formats.Load("T", mode);'),
    m("texts load: fonts' heap", T, XLD, "    m_fonts.m_heap = TableHeap(this);", "    m_fonts.m_heap = 0;"),
    m("texts load: fonts' name", T, XLD, 'm_fonts.Load("Font", mode, owner);', "m_fonts.Load(name, mode, owner);"),
    m("texts load: fonts' mode", T, XLD, 'm_fonts.Load("Font", mode, owner);', 'm_fonts.Load("Font", 0, owner);'),
    m("texts load: no directory", T, XLD, "    AtIoPath* path = PushText();\n    ReadFile(name);", "    AtIoPath* path = &AtIoFileRef::s_context->Top()->m_path;\n    ReadFile(name);"),
    gone("texts load: no file", T, XLD, "    ReadFile(name);\n"),
    m("texts load: strings' mode", T, XLD, "m_strings.Load(AtIoFileRef::s_context, name, mode);", "m_strings.Load(AtIoFileRef::s_context, name, 0);"),
    m("texts load: strings outside", T, XLD, "    m_strings.Load(AtIoFileRef::s_context, name, mode);\n    path->Pop(0);", "    path->Pop(0);\n    m_strings.Load(AtIoFileRef::s_context, name, mode);"),
    m("texts load: loaded", T, XLD, "    m_loaded = 1;", "    m_loaded = 2;"),
]

VC = "AtFpActionScriptValue::AtFpActionScriptValue() {"
VD = "AtFpActionScriptValue::~AtFpActionScriptValue() {"
VR = "void AtFpActionScriptValue::Reserve(unsigned int size) {"
VM = "void AtFpActionScriptValue::MoveTo(AtFpActionScriptValue* to) {"
VS = "void AtFpActionScriptValue::SetString(char* text, unsigned int capacity) {"
VX = "void AtFpActionScriptValue::Reset() {"
KC = "AtFpActionScriptStack::AtFpActionScriptStack() {"
KD = "AtFpActionScriptStack::~AtFpActionScriptStack() {"
KH = "void AtFpActionScriptStack::SetHeap(void* heap) {"
KF = "void AtFpActionScriptStack::Free() {"
KA = "void AtFpActionScriptStack::Allocate(unsigned int count) {"
MC = "AtFpActionScriptVM::AtFpActionScriptVM() {"
MD = "AtFpActionScriptVM::~AtFpActionScriptVM() {"
ML = "void AtFpActionScriptVM::Clear() {"
MI = "void AtFpActionScriptVM::Init(AtFlashApplication* application, AtTextTable* strings) {"
SC = "AtFpScript::AtFpScript() {"
SF = "void AtFpScript::Free() {"
SI = "void AtFpScript::Init(int id, unsigned int instructions, unsigned int size) {"
SR = "void AtFpScript::ReadInstructions(AtIoTokenizer* tokenizer) {"
AC = "AtFlashScriptTable::AtFlashScriptTable() {"
AL = "void AtFlashScriptTable::ClearScripts() {"
AD = "void AtFlashScriptTable::Load(const char* name, int, AtFlashApplication* application) {"
_VFIELDS = ("m_text", "m_unknown24", "m_capacity", "m_unknown10", "m_unknown14", "m_unknown18", "m_unknown1C", "m_unknown20", "m_heap")
_WRONG = {"m_text": "(char*)0", "m_unknown24": "(void*)0", "m_capacity": "1", "m_unknown10": "1", "m_unknown14": "1", "m_unknown18": "1", "m_unknown1C": "1", "m_unknown20": "1", "m_heap": "&s_heap"}

MUTANTS += [
    *[("afa words: %s" % w, S, '{%s, "%s"}' % (k, w), '{%s, "%sX"}' % (k, w)) for k, w in (("kNameSpace", "NameSpace"), ("kActionScriptFileVersion", "ActionScriptFileVersion"))],
    ("afa ids: version", S, "kActionScriptFileVersion = 0x3A,", "kActionScriptFileVersion = 0x3B,"),
    m("push: own heap", S, "void PushOrRaise(void* own, void* fallback) {", "void* heap = own != 0 ? own : fallback;", "void* heap = fallback;"),
    m("push: fallback", S, "void PushOrRaise(void* own, void* fallback) {", "void* heap = own != 0 ? own : fallback;", "void* heap = own;"),
    gone("push: unreported", S, "void PushOrRaise(void* own, void* fallback) {", "        AtRaise(-2, 0, 0, 0);\n"),
    gone("push: none", S, "void PushOrRaise(void* own, void* fallback) {", "    AtPushHeap(heap);\n"),
    # A value.
    *[m("value ctor: " + f, S, VC, "    %s = 0;" % f, "    %s = %s;" % (f, _WRONG[f] if f not in ("m_text", "m_unknown24") else "(%s)8" % ("char*" if f == "m_text" else "void*"))) for f in _VFIELDS if f not in ("m_text", "m_unknown24")],
    m("value ctor: type", S, VC, "    m_type = kTypeNone;", "    m_type = kTypeString;"),
    gone("value dtor: text kept", S, VD, "        operator delete(m_text);\n"),
    gone("value dtor: +24 kept", S, VD, "        operator delete(m_unknown24);\n"),
    *[m("value dtor: " + f, S, VD, "    %s = 0;" % f, "    %s = %s;" % (f, _WRONG[f] if f not in ("m_text", "m_unknown24") else "(%s)8" % ("char*" if f == "m_text" else "void*"))) for f in _VFIELDS],
    m("value dtor: type", S, VD, "    m_type = kTypeNone;", "    m_type = kTypeString;"),
    m("reserve: least", S, VR, "    if (size < 0x20) {\n        size = 0x20;", "    if (size < 0x10) {\n        size = 0x10;"),
    m("reserve: unsigned", S, VR, "if ((int)size > (int)m_capacity) {", "if (size > m_capacity) {"),
    m("reserve: equal", S, VR, "if ((int)size > (int)m_capacity) {", "if ((int)size >= (int)m_capacity) {"),
    gone("reserve: old kept", S, VR, "            operator delete(m_text);\n"),
    m("reserve: heap", S, VR, "PushOrRaise(m_heap, s_heap);", "PushOrRaise(0, s_heap);"),
    gone("reserve: no pop", S, VR, "        AtPopHeap();\n"),
    gone("reserve: unreported", S, VR, "            AtRaise(-4, 0, 0, 0);\n"),
    gone("reserve: capacity", S, VR, "        m_capacity = size;\n"),
    gone("reserve: not emptied", S, VR, "    m_text[0] = 0;\n"),
    *[m("move: " + w, S, VM, line, new) for w, line, new in (
        ("type", "    to->m_type = m_type;", "    to->m_type = 1;"), ("own type", "    m_type = kTypeNone;", "    m_type = 1;"), ("text", "    to->m_text = m_text;", "    to->m_text = 0;"),
        ("capacity", "    to->m_capacity = m_capacity;", "    to->m_capacity = 0;"), ("own text", "    m_text = 0;", "    m_text = to->m_text;"), ("own capacity", "    m_capacity = 0;", "    m_capacity = 1;"),
        ("+10", "    to->m_unknown10 = m_unknown10;", "    to->m_unknown10 = 0;"), ("+14", "    to->m_unknown14 = m_unknown14;", "    to->m_unknown14 = 0;"), ("+18", "    to->m_unknown18 = m_unknown18;", "    to->m_unknown18 = 0;"),
        ("+20", "    to->m_unknown20 = m_unknown20;", "    to->m_unknown20 = 0;"), ("+1C", "    to->m_unknown1C = m_unknown1C;", "    to->m_unknown1C = 0;"), ("+24", "    to->m_unknown24 = m_unknown24;", "    to->m_unknown24 = 0;"),
        ("own +24", "    m_unknown24 = 0;", "    m_unknown24 = to->m_unknown24;"))],
    m("set string: type", S, VS, "    m_type = kTypeString;", "    m_type = kTypeNone;"),
    gone("set string: old kept", S, VS, "        operator delete(m_text);\n"),
    m("set string: +18", S, VS, "    m_unknown18 = 0;", "    m_unknown18 = 1;"),
    m("set string: +20", S, VS, "    m_unknown20 = 0;", "    m_unknown20 = 1;"),
    m("set string: +1C", S, VS, "    m_unknown1C = 0;", "    m_unknown1C = 1;"),
    m("set string: text", S, VS, "    m_text = text;", "    m_text = 0;"),
    m("set string: capacity", S, VS, "    m_capacity = capacity;", "    m_capacity = 0;"),
    gone("value reset: text kept", S, VX, "        operator delete(m_text);\n"),
    gone("value reset: +24 kept", S, VX, "        operator delete(m_unknown24);\n"),
    *[m("value reset: " + f, S, VX, "    %s = 0;" % f, "    %s = %s;" % (f, _WRONG[f] if f not in ("m_text", "m_unknown24") else "(%s)8" % ("char*" if f == "m_text" else "void*"))) for f in _VFIELDS],
    m("value reset: type", S, VX, "    m_type = kTypeNone;", "    m_type = kTypeString;"),
    # The stack.
    m("stack ctor: allocated", S, KC, "    m_allocated = 0;", "    m_allocated = 1;"),
    m("stack ctor: top", S, KC, "    m_top = -1;", "    m_top = 0;"),
    m("stack ctor: capacity", S, KC, "    m_capacity = 0;", "    m_capacity = 1;"),
    m("stack ctor: values", S, KC, "    m_values = 0;", "    m_values = (AtFpActionScriptValue*)8;"),
    m("stack ctor: heap", S, KC, "    m_heap = 0;", "    m_heap = &s_heap;"),
    gone("stack dtor: not freed", S, KD, "    Free();\n"),
    m("stack heap", S, KH, "    s_heap = heap;", "    s_heap = 0;"),
    m("stack free: when not allocated", S, KF, "    if (m_allocated == 0) {", "    if (m_allocated == 0 && false) {"),
    gone("stack free: values kept", S, KF, "        AtDeleteArray(m_values);\n"),
    m("stack free: allocated", S, KF, "    m_allocated = 0;", "    m_allocated = 1;"),
    m("stack free: top", S, KF, "    m_top = -1;", "    m_top = 0;"),
    m("stack free: capacity", S, KF, "    m_capacity = 0;", "    m_capacity = 1;"),
    m("stack free: values", S, KF, "    m_values = 0;", "    m_values = (AtFpActionScriptValue*)8;"),
    m("stack free: heap", S, KF, "    m_heap = 0;", "    m_heap = &s_heap;"),
    gone("stack allocate: not freed", S, KA, "        Free();\n"),
    m("stack allocate: capacity", S, KA, "    m_capacity = count;", "    m_capacity = 0;"),
    m("stack allocate: heap", S, KA, "PushOrRaise(m_heap, s_heap);", "PushOrRaise(0, s_heap);"),
    m("stack allocate: count", S, KA, "AtNewArray<AtFpActionScriptValue>(count);", "AtNewArray<AtFpActionScriptValue>(count + 1);"),
    gone("stack allocate: no pop", S, KA, "    AtPopHeap();\n"),
    gone("stack allocate: unreported", S, KA, "        AtRaise(-4, 0, 0, 0);\n"),
    m("stack allocate: allocated", S, KA, "    m_allocated = 1;", "    m_allocated = 2;"),
    # The machine.
    m("vm ctor: strings", S, MC, "    m_strings = 0;", "    m_strings = (AtTextTable*)8;"),
    m("vm ctor: application", S, MC, "    m_application = 0;", "    m_application = (AtFlashApplication*)8;"),
    m("vm ctor: heap", S, MC, "    m_heap = 0;", "    m_heap = &s_heap;"),
    m("vm dtor: strings", S, MD, "    m_strings = 0;", "    m_strings = (AtTextTable*)8;"),
    m("vm dtor: application", S, MD, "    m_application = 0;", "    m_application = (AtFlashApplication*)8;"),
    m("vm dtor: heap", S, MD, "    m_heap = 0;", "    m_heap = &s_heap;"),
    gone("vm clear: stack kept", S, ML, "    m_stack.Free();\n"),
    m("vm clear: strings", S, ML, "    m_strings = 0;", "    m_strings = (AtTextTable*)8;"),
    m("vm clear: application", S, ML, "    m_application = 0;", "    m_application = (AtFlashApplication*)8;"),
    m("vm clear: heap", S, ML, "    m_heap = 0;", "    m_heap = &s_heap;"),
    m("vm init: stack's static", S, MI, "AtFpActionScriptStack::SetHeap(m_heap != 0 ? m_heap : s_heap);", "AtFpActionScriptStack::SetHeap(s_heap);"),
    m("vm init: application", S, MI, "    m_application = application;", "    m_application = 0;"),
    m("vm init: strings", S, MI, "    m_strings = strings;", "    m_strings = 0;"),
    m("vm init: stack's heap", S, MI, "    m_stack.m_heap = m_heap != 0 ? m_heap : s_heap;", "    m_stack.m_heap = s_heap;"),
    m("vm init: stack's size", S, MI, "    m_stack.Allocate(30);", "    m_stack.Allocate(29);"),
    # A script.
    m("script ctor: read", S, SC, "    m_read = 0;", "    m_read = 1;"),
    m("script ctor: instructions", S, SC, "    m_instructions = 0;", "    m_instructions = (unsigned char**)8;"),
    m("script ctor: bytes", S, SC, "    m_bytes = 0;", "    m_bytes = (unsigned char*)8;"),
    m("script ctor: heap", S, SC, "    m_heap = 0;", "    m_heap = &s_heap;"),
    gone("script free: list kept", S, SF, "        operator delete(m_instructions);\n"),
    gone("script free: bytes kept", S, SF, "        operator delete(m_bytes);\n"),
    m("script free: read", S, SF, "    m_read = 0;", "    m_read = 1;"),
    m("script free: heap", S, SF, "    m_heap = 0;", "    m_heap = &s_heap;"),
    m("script init: id", S, SI, "    m_id = id;", "    m_id = 0;"),
    m("script init: heap", S, SI, "PushOrRaise(m_heap, s_heap);", "PushOrRaise(0, s_heap);"),
    m("script init: list's size", S, SI, "operator new(instructions * sizeof(unsigned char*));", "operator new(instructions);"),
    gone("script init: no pop", S, SI, "    AtPopHeap();\n"),
    gone("script init: unreported", S, SI, "        AtRaise(-4, 0, 0, 0);\n"),
    m("script init: bytes' size", S, SI, "    m_bytes = (unsigned char*)operator new(size);", "    m_bytes = (unsigned char*)operator new(size + 1);"),
    m("script init: bytes' heap", S, SI, "    PushOrRaise(m_heap, s_heap);\n    m_bytes", "    PushOrRaise(0, s_heap);\n    m_bytes"),
    m("script init: count", S, SI, "    m_instructionCount = instructions;", "    m_instructionCount = size;"),
    gone("script read: brace", S, SR, "    tokenizer->Next();\n"),
    m("script read: end", S, SR, "token != AtIoTokenizer::kRightCurly;", "token != AtIoTokenizer::kEnd;"),
    m("script read: start", S, SR, "m_instructions[index] = m_bytes + offset;", "m_instructions[index] = m_bytes;"),
    m("script read: byte", S, SR, "m_instructions[index][i] = (unsigned char)tokenizer->m_int;", "m_instructions[index][i] = (unsigned char)tokenizer->m_type;"),
    m("script read: index", S, SR, "        index++;", "        index += 0;"),
    m("script read: count", S, SR, "        m_read++;", "        m_read += 2;"),
    m("script read: offset", S, SR, "        offset += length;", "        offset += length + 1;"),
    # The scripts.
    m("scripts ctor: loaded", S, AC, "    m_loaded = 0;", "    m_loaded = 1;"),
    m("scripts ctor: list", S, AC, "    m_scripts = 0;", "    m_scripts = (AtFpScript**)8;"),
    m("scripts ctor: count", S, AC, "    m_count = 0;", "    m_count = 1;"),
    m("scripts ctor: heap", S, AC, "    m_heap = 0;", "    m_heap = &s_defaultHeap;"),
    m("scripts clear: when not loaded", S, AL, "    if (m_loaded == 0) {", "    if (m_loaded == 0 && false) {"),
    m("scripts clear: empty list freed", S, AL, "if (m_scripts != 0 && m_count > 0) {", "if (m_scripts != 0) {"),
    gone("scripts clear: script kept", S, AL, "                operator delete(script);\n"),
    gone("scripts clear: list kept", S, AL, "        operator delete(m_scripts);\n"),
    gone("scripts clear: strings", S, AL, "    m_strings.Clear();\n"),
    gone("scripts clear: vm", S, AL, "    m_vm.Clear();\n"),
    m("scripts clear: loaded", S, AL, "    m_loaded = 0;", "    m_loaded = 1;"),
    m("scripts clear: list", S, AL, "    m_scripts = 0;", "    m_scripts = (AtFpScript**)8;"),
    m("scripts clear: count", S, AL, "    m_count = 0;", "    m_count = 1;"),
    m("scripts clear: heap", S, AL, "    m_heap = 0;", "    m_heap = &s_defaultHeap;"),
    gone("afa: not cleared", S, AD, "        ClearScripts();\n"),
    m("afa: application", S, AD, "    m_application = application;", "    m_application = 0;"),
    m("afa: strings' path", S, AD, 'AtFormat(path, "%s\\\\%s", "AScript", name);', 'AtFormat(path, "%s%s", "AScript", name);'),
    m("afa: strings' context", S, AD, "m_strings.Load(AtIoContext::s_instance, path, 0);", "m_strings.Load(AtIoFileRef::s_context + 1, path, 0);"),
    m("afa: strings' third", S, AD, "m_strings.Load(AtIoContext::s_instance, path, 0);", "m_strings.Load(AtIoContext::s_instance, path, 1);"),
    m("afa: vm's heap", S, AD, "    m_vm.m_heap = m_heap != 0 ? m_heap : s_defaultHeap;", "    m_vm.m_heap = s_defaultHeap;"),
    m("afa: vm's fallback", S, AD, "    m_vm.m_heap = m_heap != 0 ? m_heap : s_defaultHeap;", "    m_vm.m_heap = m_heap;"),
    m("afa: vm's application", S, AD, "    m_vm.Init(application, &m_strings);", "    m_vm.Init(0, &m_strings);"),
    m("afa: open", S, AD, 'file.Open(path, "AFA", 2, 0);', 'file.Open(path, "AFX", 2, 0);'),
    m("afa: report", S, AD, 'AtReportFileError(status, path, "AFA");', 'AtReportFileError(status, name, "AFA");'),
    m("afa: keywords", S, AD, "sizeof(s_keywords) / sizeof(s_keywords[0]), 0);", "sizeof(s_keywords) / sizeof(s_keywords[0]) - 6, 0);"),
    m("afa: name space", S, AD, "tokenizer->ExpectToken(kNameSpace);", "tokenizer->ExpectToken(kScript);"),
    gone("afa: name not kept", S, AD, "    AtCopyName(space, m_name);\n"),
    gone("afa: name not freed", S, AD, "    operator delete(space);\n"),
    m("afa: version", S, AD, "if (tokenizer->Next() == kActionScriptFileVersion) {", "if (tokenizer->Next() != kActionScriptFileVersion) {"),
    m("afa: count", S, AD, "m_count = (unsigned int)tokenizer->m_int;", "m_count = (unsigned int)tokenizer->m_int & 1;"),
    m("afa: list's heap", S, AD, "PushOrRaise(m_heap, s_defaultHeap);\n    m_scripts", "PushOrRaise(0, s_defaultHeap);\n    m_scripts"),
    m("afa: list's size", S, AD, "operator new(m_count * sizeof(AtFpScript*));", "operator new(m_count);"),
    m("afa: list's pop", S, AD, "    AtPopHeap();\n    if (m_scripts == 0) {", "    if (m_scripts == 0) {"),
    gone("afa: list unreported", S, AD, "        AtRaise(-4, 0, 0, 0);\n"),
    m("afa: id", S, AD, "        const int id = tokenizer->m_int;", "        const int id = 0;"),
    m("afa: size", S, AD, "        const unsigned int size = (unsigned int)tokenizer->m_int;", "        const unsigned int size = (unsigned int)tokenizer->m_int + 1;"),
    m("afa: instructions", S, AD, "        const unsigned int instructions = (unsigned int)tokenizer->m_int;", "        const unsigned int instructions = (unsigned int)tokenizer->m_int + 1;"),
    m("afa: script's heap", S, AD, "        PushOrRaise(m_heap, s_defaultHeap);", "        PushOrRaise(0, s_defaultHeap);"),
    m("afa: script's pop", S, AD, "        AtPopHeap();\n        if (m_scripts[i] == 0) {", "        if (m_scripts[i] == 0) {"),
    gone("afa: script unreported", S, AD, "            AtRaise(-4, 0, 0, 0);\n"),
    m("afa: script's own heap", S, AD, "        m_scripts[i]->m_heap = m_heap != 0 ? m_heap : s_defaultHeap;", "        m_scripts[i]->m_heap = s_defaultHeap;"),
    m("afa: script's fallback", S, AD, "        m_scripts[i]->m_heap = m_heap != 0 ? m_heap : s_defaultHeap;", "        m_scripts[i]->m_heap = m_heap;"),
    m("afa: script's id", S, AD, "        m_scripts[i]->Init(id, instructions, size);", "        m_scripts[i]->Init(id + 1, instructions, size);"),
    m("afa: script's arguments", S, AD, "        m_scripts[i]->Init(id, instructions, size);", "        m_scripts[i]->Init(id, size, instructions);"),
    m("afa: loaded", S, AD, "    m_loaded = 1;", "    m_loaded = 2;"),
]

SUITE = {"target": "oracle_aft", "subsystem": "at/aft", "src": "at", "var": "KND_AT_SRC", "cases": "tok", "mutants": MUTANTS}
