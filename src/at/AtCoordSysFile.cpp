// .AKC files: see include/at/AtCoordSys.h.
#include "at/AtCoordSys.h"

#include "at/AtIoTokenizer.h"
#include "at/AtText.h"

namespace {

// In the table at 0x002B05F8, in this order.
enum Word {
    kRotTransCoordSys = 0x39, kScaleCoordSys = 0x3A, kCoordSys = 0x3B, kOrientation = 0x3C, kPosition = 0x3D, kUniformScale = 0x3E, kScale = 0x3F, kParent = 0x40, kLocalDrawingTransform = 0x41
};

// The argument of Error(): "unknown name", and "unexpected keyword". The second stands for having no more room, in the entries and in the matrices.
enum Problem { kUnknownName = 0x25, kUnexpectedKeyword = 0x27 };

}  // namespace

const AtIoKeyword AtCoordSysDatabase::s_keywords[AtCoordSysDatabase::kKeywordCount] = {
    {kRotTransCoordSys, "RotTransCoordSys"},
    {kScaleCoordSys, "ScaleCoordSys"},
    {kCoordSys, "CoordSys"},
    {kOrientation, "Orientation"},
    {kPosition, "Position"},
    {kUniformScale, "UniformScale"},
    {kScale, "Scale"},
    {kParent, "Parent"},
    {kLocalDrawingTransform, "LocalDrawingTransform"},
};

void AtMatrixRead(AtMatrix* m, AtIoTokenizer* tokenizer) {
    float* cell = (float*)m;
    for (int i = 0; i < 16; i++) {
        cell[i] = tokenizer->ExpectReal();
    }
}

void AtCoordSys::ReadWord(AtIoTokenizer* tokenizer, AtCoordSysDatabase* database, int token) {
    switch (token) {
    case kParent: {
        char name[8];
        tokenizer->ExpectName(name);
        AtCoordSys* parent = (AtCoordSys*)database->Find(name);
        if (parent == 0) {
            tokenizer->Error(kUnknownName);
        }
        parent->Attach(this);
        break;
    }
    case kLocalDrawingTransform:
        m_drawingLocal = database->NextMatrix();
        if (m_drawingLocal == 0) {
            tokenizer->Error(kUnexpectedKeyword);
        }
        g_atMatrix.Read(m_drawingLocal, tokenizer);
        m_drawingWorld = database->NextMatrix();
        if (m_drawingWorld == 0) {
            tokenizer->Error(kUnexpectedKeyword);
        }
        break;
    default:
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        break;
    }
}

void AtRotTransCoordSys::Read(AtIoTokenizer* tokenizer, AtCoordSysDatabase* database) {
    char name[8];
    tokenizer->ExpectName(name);
    database->Insert(name, this);
    tokenizer->ExpectLeftCurly();
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        switch (token) {
        case kOrientation: {
            AtQuaternion orientation;
            orientation.Read(tokenizer);
            g_atMatrix.SetRotation(m_local, &orientation);
            m_dirty |= kAllLocal;
            break;
        }
        case kPosition: {
            AtVector3 position;
            position.Read(tokenizer);
            g_atMatrix.SetTranslation(m_local, &position);
            m_dirty |= kAllLocal;
            break;
        }
        default:
            ReadWord(tokenizer, database, token);
            break;
        }
    }
}

void AtScaleCoordSys::Read(AtIoTokenizer* tokenizer, AtCoordSysDatabase* database) {
    char name[8];
    tokenizer->ExpectName(name);
    database->Insert(name, this);
    tokenizer->ExpectLeftCurly();
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        switch (token) {
        case kUniformScale: {
            const float scale = tokenizer->ExpectReal();
            m_scale.x = scale;
            m_scale.y = scale;
            m_scale.z = scale;
            g_atMatrix.SetUniformScale(m_local, scale);
            m_dirty |= kAllLocal;
            break;
        }
        case kScale: {
            AtVector3 scale;
            scale.Read(tokenizer);
            m_scale = scale;
            g_atMatrix.SetScale(m_local, &scale);
            m_dirty |= kAllLocal;
            break;
        }
        default:
            ReadWord(tokenizer, database, token);
            break;
        }
    }
}

AtMatrix* AtCoordSysDatabase::NextMatrix() {
    const unsigned int next = m_nextMatrix;
    if (next >= m_pool->Count()) {
        return 0;
    }
    m_nextMatrix = next + 1;
    return m_pool->At(next);
}

void AtCoordSysDatabase::Read(AtMatrixDevice* device, AtIoTokenizer* tokenizer) {
    unsigned int rotTrans = 0;
    unsigned int scaled = 0;
    unsigned int drawing = 0;
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kEnd && token != kCoordSys) {
        switch (token) {
        case kRotTransCoordSys:
            tokenizer->ExpectLeftBracket();
            rotTrans = tokenizer->ExpectInt();
            tokenizer->ExpectRightBracket();
            break;
        case kScaleCoordSys:
            tokenizer->ExpectLeftBracket();
            scaled = tokenizer->ExpectInt();
            tokenizer->ExpectRightBracket();
            break;
        case kLocalDrawingTransform:
            tokenizer->ExpectLeftBracket();
            drawing = tokenizer->ExpectInt();
            tokenizer->ExpectRightBracket();
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        token = tokenizer->Next();
    }
    if (rotTrans < 1 && scaled < 1) {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }
    Allocate(device, rotTrans, scaled, drawing);
    if (token != kCoordSys) {
        return;
    }
    tokenizer->ExpectLeftCurly();
    unsigned int rotTransRead = 0;
    unsigned int scaledRead = 0;
    for (token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        switch (token) {
        case kRotTransCoordSys: {
            if (rotTransRead >= rotTrans) {
                tokenizer->Error(kUnexpectedKeyword);
            }
            AtRotTransCoordSys* entry = &m_rotTrans[rotTransRead];
            entry->Read(tokenizer, this);
            if (entry->m_parent == 0) {
                m_root = entry;
            }
            rotTransRead++;
            break;
        }
        case kScaleCoordSys: {
            if (scaledRead >= scaled) {
                tokenizer->Error(kUnexpectedKeyword);
            }
            AtScaleCoordSys* entry = &m_scaled[scaledRead];
            entry->Read(tokenizer, this);
            if (entry->m_parent == 0) {
                m_root = entry;
            }
            scaledRead++;
            break;
        }
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
    }
}

void AtCoordSysDatabase::Load(AtMatrixDevice* device, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AKC", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AKC");
    }
    Read(device, file.m_tokenizer);
    file.Close();
}
