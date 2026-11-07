// See include/at/AtLensFlare.h.
#include "at/AtLensFlare.h"

#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

void* AtLensFlare::s_heap;
void* AtLensFlareDatabase::s_heap;

// 0x002B20A8.
const AtIoKeyword AtLensFlareDatabase::s_keywords[AtLensFlareDatabase::kKeywordCount] = {
    {0x39, "NameSpace"}, {0x3A, "LensFlare"}, {0x3B, "LensCenter"}, {0x3C, "LensInnerRing"}, {0x3D, "LensOuterRing"}, {0x3E, "Flare"}, {0x3F, "MatAssignment"},
    {0x40, "MatAssignmentWithNameSpace"}, {0x41, "AmountTowardsLight"}, {0x42, "SquareSize"}, {0x43, "RotateFlare"}, {0x44, "DisableDistanceColoring"}, {0x45, "DisableDepthColoring"},
};

namespace {

enum Word {
    kNameSpace = 0x39, kLensFlareWord = 0x3A, kLensCenter = 0x3B, kLensInnerRing = 0x3C, kLensOuterRing = 0x3D, kFlare = 0x3E, kMatAssignment = 0x3F, kMatAssignmentWithNameSpace = 0x40,
    kAmountTowardsLight = 0x41, kSquareSize = 0x42, kRotateFlare = 0x43, kDisableDistanceColoring = 0x44, kDisableDepthColoring = 0x45
};

// Codes passed to the tokenizer's Error; the words for them are inferred from where they are used.
enum Problem { kListAlreadyGiven = 0x27, kOutOfRange = 0x23 };

void ReadColor(AtLensColor* color, AtIoTokenizer* tokenizer) {
    color->Read(tokenizer);
    color->x = (float)((double)color->x * 255.0f);
    color->y = (float)((double)color->y * 255.0f);
    color->z = (float)((double)color->z * 255.0f);
    color->w = (float)((double)color->w * 255.0f);
}

}  // namespace

void AtNameTableOwner::AddLensFlares(GmNameMap* map, const char* ns) {
    if (ns != 0) {
        m_lensFlareSpaces.Add(ns, map);
        return;
    }
    m_lensFlares.Add(map);
}

void AtLensColor::SetWhite() {
    x = 1.0f;
    y = 1.0f;
    z = 1.0f;
    w = 1.0f;
}

void AtLensFlare::SetHeap(void* heap) {
    s_heap = heap;
}

AtLensFlare::AtLensFlare() {
    m_owner = 0;
    m_flags = 0;
    m_centerColor.SetWhite();
    m_innerColor.SetWhite();
    m_outerColor.SetWhite();
    m_innerStart = 1.0f;
    m_outerStart = 1.0f;
    m_innerEnd = 500.0f;
    m_outerEnd = 1000.0f;
    m_squareCount = 0;
    m_squares = 0;
    m_unknown58 = 1.0f;
    m_unknown5C = 0;
}

void AtLensFlare::Reset() {
    if (m_squares != 0) {
        operator delete(m_squares);
    }
    m_materials.~AtMatAssignment();
    m_owner = 0;
    m_flags = 0;
    m_centerColor.SetWhite();
    m_innerColor.SetWhite();
    m_outerColor.SetWhite();
    m_innerStart = 1.0f;
    m_outerStart = 1.0f;
    m_innerEnd = 500.0f;
    m_outerEnd = 1000.0f;
    m_squareCount = 0;
    m_squares = 0;
    m_unknown58 = 1.0f;
    m_unknown5C = 0;
}

AtLensFlare::~AtLensFlare() {
    Reset();
}

void AtLensFlare::ReadSquare(AtLensFlareSquare* square, AtIoTokenizer* tokenizer) {
    tokenizer->ExpectToken(kFlare);
    tokenizer->ExpectLeftCurly();
    square->m_rotate = 0;
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        switch (token) {
        case kMatAssignment:
            square->m_material = (unsigned int)tokenizer->ExpectInt();
            if (square->m_material >= m_materials.m_count) {
                tokenizer->Error(kOutOfRange);
            }
            break;
        case kAmountTowardsLight:
            square->m_amountTowardsLight = tokenizer->ExpectReal();
            break;
        case kSquareSize:
            square->m_halfSize = (float)((double)tokenizer->ExpectReal() * 0.5f);
            break;
        case kRotateFlare:
            square->m_rotate = 1;
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
    }
}

void AtLensFlare::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_owner != 0) {
        Reset();
    }
    m_owner = owner;
    m_flags = kDistanceColoring | kDepthColoring;
    AtMaterialOwner* materials = (AtMaterialOwner*)owner->m_device;  // By its use here it has AtMaterialOwner's tables. AtSceneDevice and AtMaterialOwner may be one class.
    tokenizer->ExpectLeftCurly();
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        switch (token) {
        case kLensCenter:
            ReadColor(&m_centerColor, tokenizer);
            break;
        case kLensInnerRing:
            m_innerStart = tokenizer->ExpectReal();
            m_innerEnd = tokenizer->ExpectReal();
            ReadColor(&m_innerColor, tokenizer);
            break;
        case kLensOuterRing:
            m_outerStart = tokenizer->ExpectReal();
            m_outerEnd = tokenizer->ExpectReal();
            ReadColor(&m_outerColor, tokenizer);
            break;
        case kFlare: {
            if (m_squareCount > 0) {
                tokenizer->Error(kListAlreadyGiven);
            }
            m_squareCount = (unsigned int)tokenizer->ExpectCount();
            if (m_squareCount < 1) {
                tokenizer->Error(kOutOfRange);
            }
            if (s_heap == 0) {
                AtRaise(-2, 0, 0, 0);
            }
            AtPushHeap(s_heap);
            m_squares = (AtLensFlareSquare*)operator new(m_squareCount * sizeof(AtLensFlareSquare));
            AtPopHeap();
            if (m_squares == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            unsigned char* bytes = (unsigned char*)m_squares;
            for (unsigned int i = 0; i < m_squareCount * sizeof(AtLensFlareSquare); i++) {
                bytes[i] = 0;
            }
            for (unsigned int i = 0; i < m_squareCount; i++) {
                ReadSquare(&m_squares[i], tokenizer);
            }
            tokenizer->ExpectRightCurly();
            break;
        }
        case kMatAssignment:
            m_materials.Read(materials, tokenizer, 0);
            break;
        case kMatAssignmentWithNameSpace:
            m_materials.ReadWithNameSpaces(materials, tokenizer);
            break;
        case kDisableDistanceColoring:
            m_flags &= ~(unsigned int)kDistanceColoring;
            break;
        case kDisableDepthColoring:
            m_flags &= ~(unsigned int)kDepthColoring;
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
    }
}

void AtLensFlareDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtLensFlareDatabase::AtLensFlareDatabase() {
    m_count = 0;
    m_flares = 0;
}

void AtLensFlareDatabase::Clear() {
    if (m_flares != 0) {
        delete[] m_flares;
    }
    GmNameMap::Clear();
    m_count = 0;
    m_flares = 0;
}

AtLensFlareDatabase::~AtLensFlareDatabase() {
    if (m_flares != 0) {
        delete[] m_flares;
    }
    GmNameMap::Clear();
    m_count = 0;
    m_flares = 0;
}

void AtLensFlareDatabase::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_flares != 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    const char* ns = 0;
    char space[8];
    if (tokenizer->Next() == kNameSpace) {
        tokenizer->ExpectName(space);
        tokenizer->Next();
        ns = space;
    }
    m_count = (unsigned int)tokenizer->ExpectCount();
    if (m_count < 1) {
        tokenizer->Error(kOutOfRange);
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_flares = new AtLensFlare[m_count];
    AtPopHeap();
    if (m_flares == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    Init(m_count);
    owner->AddLensFlares(this, ns);
    for (unsigned int i = 0; i < m_count; i++) {
        tokenizer->ExpectToken(kLensFlareWord);
        char name[8];
        tokenizer->ExpectName(name);
        Insert(name, &m_flares[i]);
        m_flares[i].Read(owner, tokenizer);
    }
    tokenizer->ExpectRightCurly();
}

void AtLensFlareDatabase::Load(AtNameTableOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "ASL", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "ASL");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}
