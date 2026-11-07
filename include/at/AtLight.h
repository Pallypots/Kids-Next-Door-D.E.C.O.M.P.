// Lights, as read from .AGL files. Original address cluster 0x0002AB00-0x0002AD4B, with 0x000368C0. The words are the original's, from its table at 0x002B0A48; other names are inferred.
// Colours are four reals, read as a quaternion is. AtLightDatabase is abstract: slots 2 and 3 are filled by a concrete database at vftable 0x0028A764, in the graphics layer, with lights of 0x50 bytes.
#pragma once

#include "at/AtMaterial.h"
#include "at/AtQuaternion.h"

class AtLightDatabase;

// A light's fields, as far as reading goes. Its other slots and its base class's constructor are not recovered.
class AtLight {
public:
    virtual void Slot0() = 0;
    virtual void Reset() = 0;   // slot 1

    // 0x000368C0. Its body, a left curly through its right curly. Any word not in order is reported, and reading goes on. Text that ends inside the body never leaves the loop, unless the report stops.
    void Read(AtMaterialOwner* owner, AtLightDatabase* database, AtIoTokenizer* tokenizer);

    AtMaterialOwner* m_owner;   // +0x04
    AtQuaternion m_ambient;     // +0x08
    AtQuaternion m_diffuse;     // +0x18
    AtQuaternion m_specular;    // +0x28
    float m_innerConeAngle;     // +0x38
    float m_outerConeAngle;     // +0x3C
    float m_range;              // +0x40
    float m_attenuation[3];     // +0x44
};

// Vftable 0x00273610.
class AtLightDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 9 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B0A48 for the whole file, the lights' words among them

    AtLightDatabase();                       // 0x0002AB00
    ~AtLightDatabase() override;             // slot 0; 0x0002AD30 deleting, 0x0002ACD0 complete
    void Clear() override;                   // slot 1, 0x0002ACB0

    virtual void MakeLights(AtMaterialOwner* owner) = 0;  // slot 2: the concrete database makes room for m_count
    virtual AtLight* LightAt(unsigned int index) = 0;     // slot 3

    void Load(AtMaterialOwner* owner, const char* name, int unused);  // 0x0002AC10 name + ".AGL"
    void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer);      // 0x0002AB20

    unsigned int m_count;                    // +0x14
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtLight, m_innerConeAngle) == 0x38, "AtLight::m_innerConeAngle");
static_assert(offsetof(AtLight, m_range) == 0x40, "AtLight::m_range");
static_assert(sizeof(AtLight) == 0x50, "AtLight");
static_assert(sizeof(AtLightDatabase) == 0x18, "AtLightDatabase");
#pragma pop_macro("offsetof")
#endif
