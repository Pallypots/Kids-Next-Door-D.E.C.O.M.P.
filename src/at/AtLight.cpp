// See include/at/AtLight.h.
#include "at/AtLight.h"

#include "at/AtText.h"

// 0x002B0A48.
const AtIoKeyword AtLightDatabase::s_keywords[AtLightDatabase::kKeywordCount] = {
    {0x39, "NameSpace"}, {0x3A, "Light"}, {0x3B, "AmbientColor"}, {0x3C, "DiffuseColor"}, {0x3D, "SpecularColor"},
    {0x3E, "Range"}, {0x3F, "Attenuation"}, {0x40, "InnerConeAngle"}, {0x41, "OuterConeAngle"},
};

namespace {

enum Word { kNameSpace = 0x39, kLight = 0x3A, kAmbientColor = 0x3B, kDiffuseColor = 0x3C, kSpecularColor = 0x3D, kRange = 0x3E, kAttenuation = 0x3F, kInnerConeAngle = 0x40, kOuterConeAngle = 0x41 };

}  // namespace

void AtLight::Read(AtMaterialOwner* owner, AtLightDatabase*, AtIoTokenizer* tokenizer) {
    if (m_owner != 0) {
        Reset();
    }
    m_owner = owner;
    tokenizer->ExpectLeftCurly();
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        switch (token) {
        case kAmbientColor:
            m_ambient.Read(tokenizer);
            break;
        case kDiffuseColor:
            m_diffuse.Read(tokenizer);
            break;
        case kSpecularColor:
            m_specular.Read(tokenizer);
            break;
        case kRange:
            m_range = tokenizer->ExpectReal();
            break;
        case kAttenuation:
            m_attenuation[0] = tokenizer->ExpectReal();
            m_attenuation[1] = tokenizer->ExpectReal();
            m_attenuation[2] = tokenizer->ExpectReal();
            break;
        case kInnerConeAngle:
            m_innerConeAngle = tokenizer->ExpectReal();
            break;
        case kOuterConeAngle:
            m_outerConeAngle = tokenizer->ExpectReal();
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
    }
}

AtLightDatabase::AtLightDatabase() {
    m_count = 0;
}

void AtLightDatabase::Clear() {
    GmNameMap::Clear();
    m_count = 0;
}

AtLightDatabase::~AtLightDatabase() {
    GmNameMap::Clear();
    m_count = 0;
}

void AtLightDatabase::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_count > 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    const int token = tokenizer->Next();
    const char* ns = 0;
    char space[8];
    if (token == kNameSpace) {
        tokenizer->ExpectName(space);
        tokenizer->ExpectToken(kLight);
        ns = space;
    } else if (token != kLight) {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }
    m_count = (unsigned int)tokenizer->ExpectCount();
    Init(m_count);
    owner->AddLights(this, ns);
    MakeLights(owner);
    for (unsigned int i = 0; i < m_count; i++) {
        tokenizer->ExpectToken(kLight);
        char name[8];
        tokenizer->ExpectName(name);
        AtLight* light = LightAt(i);
        Insert(name, light);
        light->Read(owner, this, tokenizer);
    }
    tokenizer->ExpectRightCurly();
}

void AtLightDatabase::Load(AtMaterialOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AGL", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AGL");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}
