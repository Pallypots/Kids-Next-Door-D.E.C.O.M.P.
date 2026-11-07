# The Flash text formats' and level files' mutants, .AFX and .AFL. Dropped as equivalent, each with its reason:
#   * Anything inside `if (m_heapSize > 0)` in AtFlashLevel::Load: m_heapSize is set to 0 on the line before, so the branch does not run, on either side.
#   * AtFlashResourceTable::Slot3: not recovered, it stops with a diagnostic; nothing here reaches it.
#   * The first of the two zeroings of m_heapSize in Load, on its own: the file's word or the second zeroing overwrites it before anything reads it, and nothing is compared until the end.
#   * Removing file.Close() from AtFlashTextFormatTable::Read: the token file's destructor, which runs at once, does the same close.
#   * m_heapSize's value from the word: read and thrown away, as above; the word's being read at all is tested.
F = "AtFlashTextFormat.cpp"
L = "AtFlashLevel.cpp"

_RCTOR = "AtFlashResource::AtFlashResource() {\n    m_index = -1;\n    m_references = 0;\n    m_unknown14 = 0;\n    m_table = 0;\n"
_RDTOR = "AtFlashResource::~AtFlashResource() {\n    m_index = -1;\n    m_references = 0;\n    m_unknown14 = 0;\n    m_table = 0;\n"
_RRST = "void AtFlashResource::ResetLinks() {\n    m_index = -1;\n    m_references = 0;\n    m_unknown14 = 0;\n    m_table = 0;\n"
_RFIELDS = (("m_index", "    m_index = -1;\n"), ("m_references", "    m_references = 0;\n"), ("m_unknown14", "    m_unknown14 = 0;\n"), ("m_table", "    m_table = 0;\n"))
_RESET = "    ResetLinks();\n    m_heap = 0;\n    m_read = 0;\n    m_wordWrap = 0;\n    m_multiLine = 0;\n    m_password = 0;\n    m_readOnly = 0;\n    m_noSelect = 0;\n    m_border = 0;\n    m_fontHeight = 12.0f;\n    m_textMaxLength = -1;\n    m_alignment = kAlignmentLeft;\n    m_leftMargin = 0.0f;\n    m_rightMargin = 0.0f;\n    m_indent = 0.0f;\n    m_leading = 0.0f;\n    m_bounds[0] = 0.0f;\n    m_bounds[1] = 0.0f;\n    m_bounds[2] = 0.0f;\n    m_bounds[3] = 0.0f;\n    m_color[0] = 255;\n    m_color[1] = 255;\n    m_color[2] = 255;\n    m_color[3] = 255;\n"
_CTOR = "AtFlashResourceTable::AtFlashResourceTable() {\n    m_initialised = 0;\n    m_firstGroup = 0;\n    m_lastGroup = 0;\n    m_items = 0;\n    m_itemCount = 0;\n    m_groupCount = 0;\n    AtCopyName8(m_name, \"NotInitd\");\n    m_heap = 0;\n"
_CLEAR = "    m_initialised = 0;\n    m_items = 0;\n    m_itemCount = 0;\n    m_groupCount = 0;\n    m_firstGroup = 0;\n    m_lastGroup = 0;\n    AtCopyName8(m_name, \"NotInitd\");\n    m_heap = 0;\n}\n\nvoid AtFlashResourceTable::Allocate"
_FIELDS = ("m_initialised", "m_items", "m_itemCount", "m_groupCount", "m_firstGroup", "m_lastGroup", "m_heap")
_GROUP = "            m_firstGroup = group->m_next;\n            if (m_firstGroup != 0) {\n                m_firstGroup->m_prev = 0;\n            } else {\n                m_lastGroup = 0;\n            }\n            group->m_first = 0;\n            group->m_last = 0;\n            operator delete(group);\n"
_ALIGN = "            if (alignment == kWordAlignmentLeft) {\n                m_alignment = kAlignmentLeft;\n            } else if (alignment == kWordAlignmentRight) {\n                m_alignment = kAlignmentRight;\n            } else if (alignment == kWordAlignmentCenter) {\n                m_alignment = kAlignmentCenter;\n            } else if (alignment == kWordAlignmentJustify) {\n                m_alignment = kAlignmentJustify;\n            } else {\n                tokenizer->Error(kBadField);\n            }\n"
_LOADEND = "    m_heapInUse = heap;\n    m_heap = heap;\n    m_heap204 = heap;\n    m_first.SetHeap(heap);\n    m_second.SetHeap(m_heapInUse);\n    m_heap1DC = m_heapInUse;\n    m_heap150 = m_heapInUse;\n"


def _drop(head, line):
    return head, head.replace(line, "", 1)


MUTANTS = [
    # Keywords: every word's id and text, against the file's own use.
    *[("afx words: %s" % w, F, '{%s, "%s"}' % (k, w), '{%s, "%sX"}' % (k, w)) for k, w in (("kNameSpace", "NameSpace"), ("kTextFormat", "TextFormat"), ("kBoundsRectangle", "BoundsRectangle"), ("kWordWrap", "WordWrap"), ("kMultiLine", "MultiLine"), ("kPassword", "Password"), ("kReadOnly", "ReadOnly"), ("kNoSelect", "NoSelect"), ("kBorder", "Border"), ("kFontHeight", "FontHeight"), ("kTextColor", "TextColor"), ("kTextMaxLength", "TextMaxLength"), ("kAlignment", "Alignment"), ("kLeftMargin", "LeftMargin"), ("kRightMargin", "RightMargin"), ("kIndent", "Indent"), ("kLeading", "Leading"), ("kWordAlignmentLeft", "AlignmentLeft"), ("kWordAlignmentRight", "AlignmentRight"), ("kWordAlignmentCenter", "AlignmentCenter"), ("kWordAlignmentJustify", "AlignmentJustify"))],
    *[("afl words: %s" % w, L, '{%s, "%s"}' % (k, w), '{%s, "%sX"}' % (k, w)) for k, w in (("kLevel", "Level"), ("kHeapSize", "HeapSize"), ("kMaxVariableValueLength", "MaxVariableValueLength"), ("kMaxVariables", "MaxVariables"), ("kFontSourceLevel", "FontSourceLevel"), ("kSelf", "Self"), ("kAll", "All"))],
    ("afx ids: border", F, "kBorder = 0x41,", "kBorder = 0x4E,"),
    ("afx ids: leading", F, "kLeading = 0x49,", "kLeading = 0x4E,"),
    # The base resource.
    *[("resource ctor: no " + n, F, *_drop(_RCTOR, l)) for n, l in _RFIELDS],
    *[("resource dtor: no " + n, F, *_drop(_RDTOR, l)) for n, l in _RFIELDS],
    *[("resource reset: no " + n, F, *_drop(_RRST, l)) for n, l in _RFIELDS],
    ("resource ctor: index", F, _RCTOR, _RCTOR.replace("m_index = -1;", "m_index = 0;")),
    # The format's reset.
    *[("reset: no " + l.strip(), F, *_drop(_RESET, l)) for l in _RESET.splitlines(True)[1:]],
    ("reset: no links", F, _RESET, _RESET.replace("    ResetLinks();\n", "")),
    ("reset: font height", F, "m_fontHeight = 12.0f;", "m_fontHeight = 13.0f;"),
    ("reset: max length", F, "m_textMaxLength = -1;", "m_textMaxLength = 0;"),
    ("format ctor: no reset", F, "AtFlashTextFormat::AtFlashTextFormat() {\n    Reset();\n", "AtFlashTextFormat::AtFlashTextFormat() {\n"),
    ("format dtor: reset unread", F, "    if (m_read != 0) {\n        Reset();\n    }\n", "    Reset();\n"),
    ("format dtor: never", F, "    if (m_read != 0) {\n        Reset();\n    }\n", ""),
    ("format read: no fields", F, "    ReadFields(tokenizer);\n    m_read = 1;\n", "    m_read = 1;\n"),
    ("format read: not noted", F, "    ReadFields(tokenizer);\n    m_read = 1;\n", "    ReadFields(tokenizer);\n"),
    # The fields.
    ("fields: word", F, "    tokenizer->ExpectToken(kTextFormat);\n    m_index = tokenizer->ExpectInt();", "    tokenizer->Next();\n    m_index = tokenizer->ExpectInt();"),
    ("fields: index", F, "    m_index = tokenizer->ExpectInt();\n", "    tokenizer->ExpectInt();\n"),
    ("fields: no brace", F, "    m_index = tokenizer->ExpectInt();\n    tokenizer->ExpectLeftCurly();\n", "    m_index = tokenizer->ExpectInt();\n"),
    *[("fields: bounds %d" % i, F, "m_bounds[%d] = tokenizer->ExpectReal();" % i, "m_bounds[%d] = 0.0f + tokenizer->ExpectReal() * 2.0f;" % i) for i in range(4)],
    *[("fields: bounds %d not read" % i, F, "            m_bounds[%d] = tokenizer->ExpectReal();\n" % i, "") for i in range(4)],
    *[("fields: " + f, F, "            %s = 1;\n" % f, "            %s = 2;\n" % f) for f in ("m_wordWrap", "m_multiLine", "m_password", "m_readOnly", "m_noSelect", "m_border")],
    ("fields: font height", F, "m_fontHeight = tokenizer->ExpectReal();", "tokenizer->ExpectReal();"),
    *[("fields: color %d" % i, F, "m_color[%d] = (unsigned char)tokenizer->ExpectInt();" % i, "m_color[%d] = (unsigned char)(tokenizer->ExpectInt() + 1);" % i) for i in range(4)],
    ("fields: max length", F, "m_textMaxLength = tokenizer->ExpectInt();", "tokenizer->ExpectInt();"),
    ("fields: align left", F, _ALIGN, _ALIGN.replace("m_alignment = kAlignmentLeft;", "m_alignment = kAlignmentRight;")),
    ("fields: align right", F, _ALIGN, _ALIGN.replace("m_alignment = kAlignmentRight;", "m_alignment = kAlignmentLeft;")),
    ("fields: align center", F, _ALIGN, _ALIGN.replace("m_alignment = kAlignmentCenter;", "m_alignment = kAlignmentLeft;")),
    ("fields: align justify", F, _ALIGN, _ALIGN.replace("m_alignment = kAlignmentJustify;", "m_alignment = kAlignmentLeft;")),
    ("fields: align bad word passes", F, _ALIGN, _ALIGN.replace("            } else {\n                tokenizer->Error(kBadField);\n            }\n", "            }\n")),
    ("fields: left margin", F, "m_leftMargin = tokenizer->ExpectReal();", "m_rightMargin = tokenizer->ExpectReal();"),
    ("fields: right margin", F, "m_rightMargin = tokenizer->ExpectReal();", "m_leftMargin = tokenizer->ExpectReal();"),
    ("fields: indent", F, "m_indent = tokenizer->ExpectReal();", "m_leading = tokenizer->ExpectReal();"),
    ("fields: leading", F, "m_leading = tokenizer->ExpectReal();", "m_indent = tokenizer->ExpectReal();"),
    ("fields: unknown word passes", F, "            m_leading = tokenizer->ExpectReal();\n            break;\n        default:\n            tokenizer->Error(kBadField);\n", "            m_leading = tokenizer->ExpectReal();\n            break;\n        default:\n"),
    ("fields: report's code", F, "const int kBadField = 0x27;", "const int kBadField = 0x1E;"),
    # The base table.
    *[("base ctor: no " + f, F, *_drop(_CTOR, "    %s = 0;\n" % f)) for f in ("m_initialised", "m_firstGroup", "m_lastGroup", "m_items", "m_itemCount", "m_groupCount", "m_heap")],
    ("base ctor: no name", F, _CTOR, _CTOR.replace("    AtCopyName8(m_name, \"NotInitd\");\n", "")),
    ("base dtor: no clear", F, "AtFlashResourceTable::~AtFlashResourceTable() {\n    Clear();\n", "AtFlashResourceTable::~AtFlashResourceTable() {\n"),
    ("clear: when not initialised", F, "    if (m_initialised == 0) {\n        return;\n    }\n", ""),
    ("clear: list kept", F, "    if (m_items != 0) {\n        operator delete(m_items);\n    }\n    for (unsigned int i = 0; i < m_groupCount; i++) {", "    for (unsigned int i = 0; i < m_groupCount; i++) {"),
    ("clear: one group fewer", F, "for (unsigned int i = 0; i < m_groupCount; i++) {", "for (unsigned int i = 1; i < m_groupCount; i++) {"),
    ("clear: group's next", F, _GROUP, _GROUP.replace("            m_firstGroup = group->m_next;\n", "")),
    ("clear: group not freed", F, _GROUP, _GROUP.replace("            operator delete(group);\n", "")),
    ("clear: no check for a group", F, "        if (group != 0) {\n            m_firstGroup = group->m_next;", "        {\n            m_firstGroup = group->m_next;"),
    *[("clear: no " + f, F, *_drop(_CLEAR, "    %s = 0;\n" % f)) for f in _FIELDS],
    ("clear: name", F, _CLEAR, _CLEAR.replace("    AtCopyName8(m_name, \"NotInitd\");\n", "")),
    ("base allocate: count", F, "    m_itemCount = count;\n    void* heap", "    void* heap"),
    ("base allocate: heap fallback", F, "    return table->m_heap != 0 ? table->m_heap : AtFlashResourceTable::s_defaultHeap;", "    return table->m_heap;"),
    ("base allocate: heap own ignored", F, "    return table->m_heap != 0 ? table->m_heap : AtFlashResourceTable::s_defaultHeap;", "    return AtFlashResourceTable::s_defaultHeap;"),
    ("base allocate: no raise without heap", F, "    m_itemCount = count;\n    void* heap = HeapOf(this);\n    if (heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n", "    m_itemCount = count;\n    void* heap = HeapOf(this);\n"),
    ("base allocate: no pop", F, "(AtFlashResource**)operator new(count * sizeof(AtFlashResource*));\n    AtPopHeap();\n", "(AtFlashResource**)operator new(count * sizeof(AtFlashResource*));\n"),
    ("base allocate: size", F, "operator new(count * sizeof(AtFlashResource*));", "operator new(count * sizeof(AtFlashResource*) + 4);"),
    ("base allocate: no raise without room", F, "    if (m_items == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("base allocate: not cleared", F, "    for (unsigned int i = 0; i < count; i++) {\n        m_items[i] = 0;\n    }\n", ""),
    ("base release: a reference more", F, "m_items[index]->m_references--;", "m_items[index]->m_references++;"),
    ("base release: another item", F, "m_items[index]->m_references--;", "m_items[0]->m_unknown14--;"),
    # The table of formats.
    ("table ctor: loaded", F, "AtFlashTextFormatTable::AtFlashTextFormatTable() {\n    m_loaded = 0;\n", "AtFlashTextFormatTable::AtFlashTextFormatTable() {\n"),
    ("table dtor: no release", F, "AtFlashTextFormatTable::~AtFlashTextFormatTable() {\n    ReleaseFormats();\n", "AtFlashTextFormatTable::~AtFlashTextFormatTable() {\n"),
    ("table allocate: no list", F, "    AtFlashResourceTable::Allocate(count);\n", ""),
    ("table allocate: no raise without heap", F, "    AtFlashResourceTable::Allocate(count);\n    void* heap = HeapOf(this);\n    if (heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n", "    AtFlashResourceTable::Allocate(count);\n    void* heap = HeapOf(this);\n"),
    ("table allocate: no pop", F, "    AtFlashTextFormat* formats = AtNewArray<AtFlashTextFormat>(count);\n    AtPopHeap();\n", "    AtFlashTextFormat* formats = AtNewArray<AtFlashTextFormat>(count);\n"),
    ("table allocate: no raise without room", F, "    if (formats == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("table allocate: list not filled", F, "        m_items[i] = &formats[i];\n", ""),
    ("table allocate: heap not given", F, "        formats[i].m_heap = HeapOf(this);\n", ""),
    ("release formats: when not loaded", F, "    if (m_loaded == 0) {\n        return;\n    }\n", ""),
    ("release formats: no list check", F, "    if (m_items != 0 && m_items[0] != 0) {", "    if (m_items != 0 && (m_itemCount != 0 || m_items[0] != 0)) {"),
    ("release formats: no array", F, "        AtDeleteArray(static_cast<AtFlashTextFormat*>(m_items[0]));\n", ""),
    ("release formats: list stays", F, "        for (unsigned int i = 0; i < m_itemCount; i++) {\n            m_items[i] = 0;\n        }\n", ""),
    ("release formats: no clear", F, "    Clear();\n    m_loaded = 0;\n", "    m_loaded = 0;\n"),
    ("release formats: loaded", F, "    Clear();\n    m_loaded = 0;\n", "    Clear();\n"),
    ("release formats: heap", F, "    Clear();\n    m_loaded = 0;\n    m_heap = 0;\n", "    Clear();\n    m_loaded = 0;\n"),
    # Reading a table.
    ("afx: extension", F, 'file.Open(name, "AFX", 2, 0)', 'file.Open(name, "AFD", 2, 0)'),
    ("afx: flags", F, 'file.Open(name, "AFX", 2, 0)', 'file.Open(name, "AFX", 3, 0)'),
    ("afx: no report", F, '    if (status != 0) {\n        AtReportFileError(status, name, "AFX");\n    }\n', ""),
    ("afx: report's extension", F, 'AtReportFileError(status, name, "AFX");', 'AtReportFileError(status, name, "AFD");'),
    ("afx: no words", F, "    tokenizer->SetKeywords(s_keywords, sizeof(s_keywords) / sizeof(s_keywords[0]), 0);\n", ""),
    ("afx: fewer words", F, "sizeof(s_keywords) / sizeof(s_keywords[0]), 0);", "sizeof(s_keywords) / sizeof(s_keywords[0]) - 1, 0);"),
    ("afx: name space word", F, "    tokenizer->ExpectToken(kNameSpace);\n", ""),
    ("afx: name not kept", F, "    AtCopyName(space, m_name);\n", ""),
    ("afx: name not freed", F, "    operator delete(space);\n", ""),
    ("afx: format word", F, "    operator delete(space);\n    tokenizer->ExpectToken(kTextFormat);\n", "    operator delete(space);\n"),
    ("afx: left bracket", F, "    tokenizer->ExpectLeftBracket();\n", ""),
    ("afx: right bracket", F, "    tokenizer->ExpectRightBracket();\n", ""),
    ("afx: count", F, "m_itemCount = (unsigned int)tokenizer->ExpectInt();", "tokenizer->ExpectInt();"),
    ("afx: left brace", F, "    tokenizer->ExpectRightBracket();\n    tokenizer->ExpectLeftCurly();\n", "    tokenizer->ExpectRightBracket();\n"),
    ("afx: list with none", F, "    if (m_itemCount > 0) {\n        Allocate(m_itemCount);", "    {\n        Allocate(m_itemCount);"),
    ("afx: not through the table", F, "        Allocate(m_itemCount);\n", "        AtFlashResourceTable::Allocate(m_itemCount);\n"),
    ("afx: formats not read", F, "            m_items[i]->Read(tokenizer);\n", ""),
    ("afx: right brace", F, "    tokenizer->ExpectRightCurly();\n    file.Close();\n", "    file.Close();\n"),
    # The level.
    ("afl: 330", L, "m_unknown330 = 20000;", "m_unknown330 = 20001;"),
    ("afl: value length", L, "m_maxVariableValueLength = 1000;", "m_maxVariableValueLength = 1001;"),
    ("afl: variables", L, "m_maxVariables = 10000;", "m_maxVariables = 10001;"),
    ("afl: font source", L, "    m_fontSourceLevel = -1;\n    AtCopyName9", "    m_fontSourceLevel = 0;\n    AtCopyName9"),
    ("afl: name kept whole", L, 'file.Open(path, "AFL", 2, 0)', 'file.Open(name, "AFL", 2, 0)'),
    ("afl: extension", L, 'file.Open(path, "AFL", 2, 0)', 'file.Open(path, "AFD", 2, 0)'),
    ("afl: flags", L, 'file.Open(path, "AFL", 2, 0)', 'file.Open(path, "AFL", 3, 0)'),
    ("afl: no report", L, '    if (status != 0) {\n        AtReportFileError(status, path, "AFL");\n    }\n', ""),
    ("afl: report's name", L, 'AtReportFileError(status, path, "AFL");', 'AtReportFileError(status, name, "AFL");'),
    ("afl: no words", L, "    tokenizer->SetKeywords(s_keywords, sizeof(s_keywords) / sizeof(s_keywords[0]), 0);\n", ""),
    ("afl: fewer words", L, "sizeof(s_keywords) / sizeof(s_keywords[0]), 0);", "sizeof(s_keywords) / sizeof(s_keywords[0]) - 1, 0);"),
    ("afl: level's brace", L, "        case kLevel:\n            tokenizer->ExpectLeftCurly();\n", "        case kLevel:\n"),
    ("afl: heap size not read", L, "            m_heapSize = (unsigned int)tokenizer->ExpectInt();\n", ""),
    ("afl: value length read", L, "m_maxVariableValueLength = tokenizer->ExpectInt();", "m_maxVariables = tokenizer->ExpectInt();"),
    ("afl: variables read", L, "m_maxVariables = tokenizer->ExpectInt();", "m_maxVariableValueLength = tokenizer->ExpectInt();"),
    ("afl: Self", L, "            if (source == kSelf) {\n                m_fontSourceLevel = -1;", "            if (source == kSelf) {\n                m_fontSourceLevel = -3;"),
    ("afl: All", L, "                m_fontSourceLevel = -2;", "                m_fontSourceLevel = -3;"),
    ("afl: number", L, "m_fontSourceLevel = tokenizer->m_int;", "m_fontSourceLevel = source;"),
    ("afl: unknown word passes", L, "        default:\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n", "        default:\n"),
    ("afl: report's code", L, "tokenizer->Error(AtIoTokenizer::kExpectedKeyword);", "tokenizer->Error(AtIoTokenizer::kSyntaxError);"),
    ("afl: no close", L, "    file.Close();\n", ""),
    ("afl: heap size kept", L, "    file.Close();\n\n    m_heapSize = 0;\n", "    file.Close();\n\n"),
    ("afl: parent ignores own", L, "void* parent = m_heap != 0 ? m_heap : AtHeapStack::s_contentHeap;", "void* parent = AtHeapStack::s_contentHeap;"),
    ("afl: parent no fallback", L, "void* parent = m_heap != 0 ? m_heap : AtHeapStack::s_contentHeap;", "void* parent = m_heap;"),
    ("afl: no raise without heap", L, "    if (parent == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n", ""),
    ("afl: no push", L, "    AtPushHeap(parent);\n", ""),
    ("afl: block size", L, "operator new(m_heapSize);", "operator new(m_heapSize + 4);"),
    ("afl: no pop", L, "    m_block = operator new(m_heapSize);\n    AtPopHeap();\n", "    m_block = operator new(m_heapSize);\n"),
    ("afl: no raise without room", L, "    if (m_block == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("afl: heap ignores own", L, "        heap = m_heap != 0 ? m_heap : AtHeapStack::s_contentHeap;", "        heap = AtHeapStack::s_contentHeap;"),
    ("afl: heap no fallback", L, "        heap = m_heap != 0 ? m_heap : AtHeapStack::s_contentHeap;", "        heap = m_heap;"),
    *[("afl: no " + l.strip(), L, *_drop(_LOADEND, l)) for l in _LOADEND.splitlines(True)],
]

# Each text must occur exactly once; the runner checks.
SUITE = {"target": "oracle_afx", "subsystem": "at/afx", "src": "at", "var": "KND_AT_SRC", "cases": "tok", "mutants": MUTANTS}
