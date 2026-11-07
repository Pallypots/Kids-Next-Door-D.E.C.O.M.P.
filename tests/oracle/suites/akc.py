# Mutants of the .AKC reader. Each text occurs exactly once in the source.
F = "AtCoordSysFile.cpp"

_ROT = "void AtRotTransCoordSys::Read(AtIoTokenizer* tokenizer, AtCoordSysDatabase* database) {\n    char name[8];\n    tokenizer->ExpectName(name);\n    database->Insert(name, this);\n    tokenizer->ExpectLeftCurly();\n"
_SCA = "void AtScaleCoordSys::Read(AtIoTokenizer* tokenizer, AtCoordSysDatabase* database) {\n    char name[8];\n    tokenizer->ExpectName(name);\n    database->Insert(name, this);\n    tokenizer->ExpectLeftCurly();\n"

_ORI = "            orientation.Read(tokenizer);\n            g_atMatrix.SetRotation(m_local, &orientation);\n            m_dirty |= kAllLocal;\n"
_POS = "            position.Read(tokenizer);\n            g_atMatrix.SetTranslation(m_local, &position);\n            m_dirty |= kAllLocal;\n"
_UNI = "            g_atMatrix.SetUniformScale(m_local, scale);\n            m_dirty |= kAllLocal;\n"
_VEC = "            g_atMatrix.SetScale(m_local, &scale);\n            m_dirty |= kAllLocal;\n"

_ROT_ENTRY = "            if (rotTransRead >= rotTrans) {\n                tokenizer->Error(kUnexpectedKeyword);\n            }\n"
_SCA_ENTRY = "            if (scaledRead >= scaled) {\n                tokenizer->Error(kUnexpectedKeyword);\n            }\n"


def _head(word, var):
    start = "        case %s:\n            tokenizer->ExpectLeftBracket();\n            %s = tokenizer->ExpectInt();\n            tokenizer->ExpectRightBracket();\n            break;\n" % (word, var)
    return [
        ("head %s: no left bracket" % word, F, start, start.replace("            tokenizer->ExpectLeftBracket();\n", "")),
        ("head %s: no right bracket" % word, F, start, start.replace("            tokenizer->ExpectRightBracket();\n", "")),
        ("head %s: value not kept" % word, F, start, start.replace("%s = tokenizer->ExpectInt();" % var, "tokenizer->ExpectInt();")),
        ("head %s: adds" % word, F, start, start.replace("%s = tokenizer->ExpectInt();" % var, "%s += tokenizer->ExpectInt();" % var)),
    ]


MUTANTS = [
    # The table.
    ("keywords: an id", F, '{kUniformScale, "UniformScale"}', '{kScale, "UniformScale"}'),
    ("keywords: a word", F, '{kOrientation, "Orientation"}', '{kOrientation, "Orientatio"}'),
    ("keywords: the last word", F, '{kLocalDrawingTransform, "LocalDrawingTransform"}', '{kLocalDrawingTransform, "LocalDrawingTransforms"}'),
    ("keywords: fewer known", F, "tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);", "tokenizer->SetKeywords(s_keywords, kKeywordCount - 1, 0);"),
    ("keywords: none set", F, "    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);\n", ""),
    ("words: Parent's id", F, "kParent = 0x40,", "kParent = 0x3F,"),
    ("words: CoordSys's id", F, "kCoordSys = 0x3B,", "kCoordSys = 0x3C,"),
    ("problem: unknown name's code", F, "kUnknownName = 0x25,", "kUnknownName = 0x26,"),
    ("problem: unexpected keyword's code", F, "kUnexpectedKeyword = 0x27", "kUnexpectedKeyword = 0x28"),
    # The matrix reader.
    ("matrix: one short", F, "for (int i = 0; i < 16; i++) {", "for (int i = 0; i < 15; i++) {"),
    ("matrix: backwards", F, "cell[i] = tokenizer->ExpectReal();", "cell[15 - i] = tokenizer->ExpectReal();"),
    ("matrix: ints", F, "cell[i] = tokenizer->ExpectReal();", "cell[i] = (float)tokenizer->ExpectInt();"),
    # Words of either kind.
    ("parent: no report", F, "        if (parent == 0) {\n            tokenizer->Error(kUnknownName);\n        }\n", ""),
    ("parent: other report", F, "tokenizer->Error(kUnknownName);", "tokenizer->Error(kUnexpectedKeyword);"),
    ("parent: only reported when found", F, "if (parent == 0) {", "if (parent != 0) {"),
    ("parent: the wrong way round", F, "parent->Attach(this);", "this->Attach(parent);"),
    ("parent: not attached", F, "        parent->Attach(this);\n", "        (void)parent;\n"),
    ("parent: base's attach", F, "parent->Attach(this);", "parent->AtCoordSys::Attach(this);"),
    ("drawing: first not kept", F, "m_drawingLocal = database->NextMatrix();", "database->NextMatrix();"),
    ("drawing: no first report", F, "        if (m_drawingLocal == 0) {\n            tokenizer->Error(kUnexpectedKeyword);\n        }\n", ""),
    ("drawing: first's other report", F, "        if (m_drawingLocal == 0) {\n            tokenizer->Error(kUnexpectedKeyword);", "        if (m_drawingLocal == 0) {\n            tokenizer->Error(kUnknownName);"),
    ("drawing: no second report", F, "        if (m_drawingWorld == 0) {\n            tokenizer->Error(kUnexpectedKeyword);\n        }\n", ""),
    ("drawing: second's other report", F, "        if (m_drawingWorld == 0) {\n            tokenizer->Error(kUnexpectedKeyword);", "        if (m_drawingWorld == 0) {\n            tokenizer->Error(kUnknownName);"),
    ("drawing: no read", F, "        g_atMatrix.Read(m_drawingLocal, tokenizer);\n", ""),
    ("drawing: second not taken", F, "        m_drawingWorld = database->NextMatrix();\n", "        m_drawingWorld = m_drawingLocal;\n"),
    ("drawing: read into the second", F, "        g_atMatrix.Read(m_drawingLocal, tokenizer);\n        m_drawingWorld = database->NextMatrix();\n", "        m_drawingWorld = database->NextMatrix();\n        g_atMatrix.Read(m_drawingWorld, tokenizer);\n"),
    ("word: no report", F, "    default:\n        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n        break;\n    }\n}\n\nvoid AtRotTransCoordSys::Read", "    default:\n        break;\n    }\n}\n\nvoid AtRotTransCoordSys::Read"),
    ("word: other report", F, "    default:\n        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n        break;\n    }\n}\n\nvoid AtRotTransCoordSys::Read", "    default:\n        tokenizer->Error(AtIoTokenizer::kSyntaxError);\n        break;\n    }\n}\n\nvoid AtRotTransCoordSys::Read"),
    # Rotation entries.
    ("rot: no name read", F, _ROT, _ROT.replace("    tokenizer->ExpectName(name);\n", "    name[0] = 0;\n")),
    ("rot: not named", F, _ROT, _ROT.replace("    database->Insert(name, this);\n", "")),
    ("rot: no curly", F, _ROT, _ROT.replace("    tokenizer->ExpectLeftCurly();\n", "")),
    ("rot: curly first", F, _ROT, _ROT.replace("    database->Insert(name, this);\n    tokenizer->ExpectLeftCurly();\n", "    tokenizer->ExpectLeftCurly();\n    database->Insert(name, this);\n") ),
    ("orientation: no read", F, _ORI, _ORI.replace("            orientation.Read(tokenizer);\n", "            orientation.x = orientation.y = orientation.z = orientation.w = 0;\n")),
    ("orientation: not set", F, _ORI, _ORI.replace("            g_atMatrix.SetRotation(m_local, &orientation);\n", "")),
    ("orientation: on the world", F, _ORI, _ORI.replace("SetRotation(m_local,", "SetRotation(m_world,")),
    ("orientation: not dirty", F, _ORI, _ORI.replace("            m_dirty |= kAllLocal;\n", "")),
    ("orientation: dirty replaced", F, _ORI, _ORI.replace("m_dirty |= kAllLocal;", "m_dirty = kAllLocal;")),
    ("orientation: other bits", F, _ORI, _ORI.replace("m_dirty |= kAllLocal;", "m_dirty |= kWorld;")),
    ("position: no read", F, _POS, _POS.replace("            position.Read(tokenizer);\n", "            position.x = position.y = position.z = 0;\n")),
    ("position: not set", F, _POS, _POS.replace("            g_atMatrix.SetTranslation(m_local, &position);\n", "")),
    ("position: on the world", F, _POS, _POS.replace("SetTranslation(m_local,", "SetTranslation(m_world,")),
    ("position: not dirty", F, _POS, _POS.replace("            m_dirty |= kAllLocal;\n", "")),
    ("position: other bits", F, _POS, _POS.replace("m_dirty |= kAllLocal;", "m_dirty |= kWorld;")),
    ("rot: other words go nowhere", F, "            ReadWord(tokenizer, database, token);\n            break;\n        }\n    }\n}\n\nvoid AtScaleCoordSys::Read", "            break;\n        }\n    }\n}\n\nvoid AtScaleCoordSys::Read"),
    ("rot: words by the wrong id", F, "        case kOrientation: {", "        case kOrientation + 1000: {"),
    # Scale entries.
    ("scale: no name read", F, _SCA, _SCA.replace("    tokenizer->ExpectName(name);\n", "    name[0] = 0;\n")),
    ("scale: not named", F, _SCA, _SCA.replace("    database->Insert(name, this);\n", "")),
    ("scale: no curly", F, _SCA, _SCA.replace("    tokenizer->ExpectLeftCurly();\n", "")),
    ("uniform: an int", F, "const float scale = tokenizer->ExpectReal();", "const float scale = (float)tokenizer->ExpectInt();"),
    ("uniform: no x", F, "            m_scale.x = scale;\n", ""),
    ("uniform: no y", F, "            m_scale.y = scale;\n", ""),
    ("uniform: no z", F, "            m_scale.z = scale;\n", ""),
    ("uniform: not set", F, _UNI, _UNI.replace("            g_atMatrix.SetUniformScale(m_local, scale);\n", "")),
    ("uniform: on the world", F, _UNI, _UNI.replace("SetUniformScale(m_local,", "SetUniformScale(m_world,")),
    ("uniform: not dirty", F, _UNI, _UNI.replace("            m_dirty |= kAllLocal;\n", "")),
    ("scale: no read", F, "            scale.Read(tokenizer);\n", "            scale.x = scale.y = scale.z = 0;\n"),
    ("scale: not kept", F, "            m_scale = scale;\n", ""),
    ("scale: not set", F, _VEC, _VEC.replace("            g_atMatrix.SetScale(m_local, &scale);\n", "")),
    ("scale: on the world", F, _VEC, _VEC.replace("SetScale(m_local,", "SetScale(m_world,")),
    ("scale: not dirty", F, _VEC, _VEC.replace("            m_dirty |= kAllLocal;\n", "")),
    ("scale: other words go nowhere", F, "            ReadWord(tokenizer, database, token);\n            break;\n        }\n    }\n}\n\nAtMatrix*", "            break;\n        }\n    }\n}\n\nAtMatrix*"),
    # NextMatrix.
    ("next: past the end", F, "if (next >= m_pool->Count()) {", "if (next > m_pool->Count()) {"),
    ("next: no check", F, "    if (next >= m_pool->Count()) {\n        return 0;\n    }\n", ""),
    ("next: does not advance", F, "    m_nextMatrix = next + 1;\n", ""),
    ("next: the one after", F, "return m_pool->At(next);", "return m_pool->At(next + 1);"),
    # The head.
    ("head: stops only at the word", F, "while (token != AtIoTokenizer::kEnd && token != kCoordSys) {", "while (token != kCoordSys && token != AtIoTokenizer::kRightCurly) {"),
    ("head: other words not reported", F, "        default:\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n            break;\n        }\n        token = tokenizer->Next();\n", "        default:\n            break;\n        }\n        token = tokenizer->Next();\n"),
    ("head: other report", F, "        default:\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n            break;\n        }\n        token = tokenizer->Next();\n", "        default:\n            tokenizer->Error(AtIoTokenizer::kSyntaxError);\n            break;\n        }\n        token = tokenizer->Next();\n"),
    ("head: none is fine", F, "    if (rotTrans < 1 && scaled < 1) {\n        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n    }\n", ""),
    ("head: each needed", F, "if (rotTrans < 1 && scaled < 1) {", "if (rotTrans < 1 || scaled < 1) {"),
    ("head: only rotations count", F, "if (rotTrans < 1 && scaled < 1) {", "if (rotTrans < 1) {"),
    ("head: only scales count", F, "if (rotTrans < 1 && scaled < 1) {", "if (scaled < 1) {"),
    ("head: other report for none", F, "if (rotTrans < 1 && scaled < 1) {\n        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);", "if (rotTrans < 1 && scaled < 1) {\n        tokenizer->Error(AtIoTokenizer::kSyntaxError);"),
    ("allocate: not called", F, "    Allocate(device, rotTrans, scaled, drawing);\n", ""),
    ("allocate: kinds swapped", F, "Allocate(device, rotTrans, scaled, drawing);", "Allocate(device, scaled, rotTrans, drawing);"),
    ("allocate: no drawing", F, "Allocate(device, rotTrans, scaled, drawing);", "Allocate(device, rotTrans, scaled, 0);"),
    ("no list: goes on", F, "    if (token != kCoordSys) {\n        return;\n    }\n", ""),
    ("list: no curly", F, "    }\n    tokenizer->ExpectLeftCurly();\n    unsigned int rotTransRead = 0;", "    }\n    unsigned int rotTransRead = 0;"),
    ("list: stops at the end too", F, "for (token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {", "for (token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly && token != AtIoTokenizer::kEnd; token = tokenizer->Next()) {"),
    ("list: other words not reported", F, "        default:\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n            break;\n        }\n    }\n}\n\nvoid AtCoordSysDatabase::Load", "        default:\n            break;\n        }\n    }\n}\n\nvoid AtCoordSysDatabase::Load"),
    ("list: other report", F, "        default:\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n            break;\n        }\n    }\n}\n\nvoid AtCoordSysDatabase::Load", "        default:\n            tokenizer->Error(AtIoTokenizer::kSyntaxError);\n            break;\n        }\n    }\n}\n\nvoid AtCoordSysDatabase::Load"),
    # The entries.
    ("rot entry: no report", F, _ROT_ENTRY, ""),
    ("rot entry: one early", F, "if (rotTransRead >= rotTrans) {", "if (rotTransRead + 1 >= rotTrans) {"),
    ("rot entry: one late", F, "if (rotTransRead >= rotTrans) {", "if (rotTransRead > rotTrans) {"),
    ("rot entry: other report", F, _ROT_ENTRY, _ROT_ENTRY.replace("kUnexpectedKeyword", "kUnknownName")),
    ("rot entry: always the first", F, "&m_rotTrans[rotTransRead];", "&m_rotTrans[0];"),
    ("rot entry: not counted", F, "            rotTransRead++;\n", ""),
    ("rot entry: not read", F, "            AtRotTransCoordSys* entry = &m_rotTrans[rotTransRead];\n            entry->Read(tokenizer, this);\n", "            AtRotTransCoordSys* entry = &m_rotTrans[rotTransRead];\n"),
    ("rot entry: root whatever", F, "            AtRotTransCoordSys* entry = &m_rotTrans[rotTransRead];\n            entry->Read(tokenizer, this);\n            if (entry->m_parent == 0) {\n                m_root = entry;\n            }\n", "            AtRotTransCoordSys* entry = &m_rotTrans[rotTransRead];\n            entry->Read(tokenizer, this);\n            m_root = entry;\n"),
    ("rot entry: root never", F, "            AtRotTransCoordSys* entry = &m_rotTrans[rotTransRead];\n            entry->Read(tokenizer, this);\n            if (entry->m_parent == 0) {\n                m_root = entry;\n            }\n", "            AtRotTransCoordSys* entry = &m_rotTrans[rotTransRead];\n            entry->Read(tokenizer, this);\n"),
    ("rot entry: first root stays", F, "            AtRotTransCoordSys* entry = &m_rotTrans[rotTransRead];\n            entry->Read(tokenizer, this);\n            if (entry->m_parent == 0) {\n", "            AtRotTransCoordSys* entry = &m_rotTrans[rotTransRead];\n            entry->Read(tokenizer, this);\n            if (entry->m_parent == 0 && m_root == 0) {\n"),
    ("scale entry: no report", F, _SCA_ENTRY, ""),
    ("scale entry: one early", F, "if (scaledRead >= scaled) {", "if (scaledRead + 1 >= scaled) {"),
    ("scale entry: one late", F, "if (scaledRead >= scaled) {", "if (scaledRead > scaled) {"),
    ("scale entry: other report", F, _SCA_ENTRY, _SCA_ENTRY.replace("kUnexpectedKeyword", "kUnknownName")),
    ("scale entry: always the first", F, "&m_scaled[scaledRead];", "&m_scaled[0];"),
    ("scale entry: not counted", F, "            scaledRead++;\n", ""),
    ("scale entry: root whatever", F, "            AtScaleCoordSys* entry = &m_scaled[scaledRead];\n            entry->Read(tokenizer, this);\n            if (entry->m_parent == 0) {\n                m_root = entry;\n            }\n", "            AtScaleCoordSys* entry = &m_scaled[scaledRead];\n            entry->Read(tokenizer, this);\n            m_root = entry;\n"),
    ("scale entry: root never", F, "            AtScaleCoordSys* entry = &m_scaled[scaledRead];\n            entry->Read(tokenizer, this);\n            if (entry->m_parent == 0) {\n                m_root = entry;\n            }\n", "            AtScaleCoordSys* entry = &m_scaled[scaledRead];\n            entry->Read(tokenizer, this);\n"),
    # The file.
    ("load: other extension", F, 'const int status = file.Open(name, "AKC", 2, 0);', 'const int status = file.Open(name, "AKD", 2, 0);'),
    ("load: other flags", F, 'const int status = file.Open(name, "AKC", 2, 0);', 'const int status = file.Open(name, "AKC", 1, 0);'),
    ("load: no report", F, '    if (status != 0) {\n        AtReportFileError(status, name, "AKC");\n    }\n', ""),
    ("load: other extension in the report", F, 'AtReportFileError(status, name, "AKC");', 'AtReportFileError(status, name, "AGT");'),
    ("load: not read", F, "    Read(device, file.m_tokenizer);\n", "    (void)device;\n"),
]

MUTANTS += _head("kRotTransCoordSys", "rotTrans") + _head("kScaleCoordSys", "scaled") + _head("kLocalDrawingTransform", "drawing")

SUITE = {"target": "oracle_akc", "subsystem": "at/akc", "src": "at", "var": "KND_AT_SRC", "cases": "tok", "mutants": MUTANTS}
