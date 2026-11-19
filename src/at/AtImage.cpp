// See include/at/AtImage.h. Images, the colour rectangle, and the graphics seam.
#include "at/AtImage.h"

#include "at/AtImageDatabase.h"

void AtFatal(const char* message, int file, int line, int unused);
void AtCopyName8(char* to, const char* from);

namespace {

// The image's words: the database's table at 0x002B0838.
enum Word {
    kWordMatAssignment = 0x3B, kWordTransparentColor = 0x3C, kWordColorValueRectangle = 0x3D, kWordBmpSource = 0x3E, kWordTgaSource = 0x3F, kWordDdsSource = 0x40, kWordDemandLoad = 0x41, kWordTm2Source = 0x42,
    kWordTplSource = 0x43, kWordPntSource = 0x44, kWordMpegMacroBlocks = 0x45, kWordPlatformSource = 0x46, kWordFlipTextureHorizontally = 0x47, kWordFlipTextureVertically = 0x48, kWordInfrequentModify = 0x49,
    kWordFrequentModify = 0x4A
};

enum Problem { kOutOfRange = 0x23 };

void NotRecovered(const char* what) {
    AtFatal(what, 0, 0, 0);
}

}  // namespace

// ---- the colour --------------------------------------------------------------------------------------------------------

void AtColor::SetBlack() {
    r = 0;
    g = 0;
    b = 0;
    a = 255;
}

AtColorRectangle::AtColorRectangle() {
    m_colors[0].SetWhite();
    m_colors[1].SetWhite();
    m_colors[2].SetWhite();
    m_colors[3].SetWhite();
}

void AtColorRectangle::Read(AtIoTokenizer* tokenizer) {
    m_colors[0].Read(tokenizer);
    m_colors[1].Read(tokenizer);
    m_colors[2].Read(tokenizer);
    m_colors[3].Read(tokenizer);
}

void AtColorRectangle::SetAll(const AtColor& color) {
    m_colors[0] = color;
    m_colors[1] = color;
    m_colors[2] = color;
    m_colors[3] = color;
}

// ---- the seam -----------------------------------------------------------------------------------------------------------

void AtGrNameSink::Slot0() { NotRecovered("Atlas Graphics: the name sink of the graphics layer is not recovered (slot 0)"); }
void AtGrNameSink::Slot1() { NotRecovered("Atlas Graphics: the name sink of the graphics layer is not recovered (slot 1)"); }
void AtGrNameSink::Slot2() { NotRecovered("Atlas Graphics: the name sink of the graphics layer is not recovered (slot 2)"); }
void AtGrNameSink::Slot3() { NotRecovered("Atlas Graphics: the name sink of the graphics layer is not recovered (slot 3)"); }
void AtGrNameSink::Slot4() { NotRecovered("Atlas Graphics: the name sink of the graphics layer is not recovered (slot 4)"); }
void AtGrNameSink::Slot5() { NotRecovered("Atlas Graphics: the name sink of the graphics layer is not recovered (slot 5)"); }
void AtGrNameSink::Slot6() { NotRecovered("Atlas Graphics: the name sink of the graphics layer is not recovered (slot 6)"); }
void AtGrNameSink::Slot7() { NotRecovered("Atlas Graphics: the name sink of the graphics layer is not recovered (slot 7)"); }
void AtGrNameSink::Slot8() { NotRecovered("Atlas Graphics: the name sink of the graphics layer is not recovered (slot 8)"); }
void AtGrNameSink::Slot9() { NotRecovered("Atlas Graphics: the name sink of the graphics layer is not recovered (slot 9)"); }
void AtGrNameSink::Slot10() { NotRecovered("Atlas Graphics: the name sink of the graphics layer is not recovered (slot 10)"); }
void AtGrNameSink::Slot11() { NotRecovered("Atlas Graphics: the name sink of the graphics layer is not recovered (slot 11)"); }
int AtGrNameSink::TakeName(const char*, int) {
    NotRecovered("Atlas Graphics: the name sink of the graphics layer is not recovered (slot 12)");
    return 0;
}

void AtGrDevice::Slot0() { NotRecovered("Atlas Graphics: the graphics device is not recovered (slot 0)"); }
void AtGrDevice::Slot1() { NotRecovered("Atlas Graphics: the graphics device is not recovered (slot 1)"); }
void AtGrDevice::Slot2() { NotRecovered("Atlas Graphics: the graphics device is not recovered (slot 2)"); }
void AtGrDevice::Slot3() { NotRecovered("Atlas Graphics: the graphics device is not recovered (slot 3)"); }
void AtGrDevice::Slot4() { NotRecovered("Atlas Graphics: the graphics device is not recovered (slot 4)"); }
void AtGrDevice::Slot5() { NotRecovered("Atlas Graphics: the graphics device is not recovered (slot 5)"); }
void AtGrDevice::Slot6() { NotRecovered("Atlas Graphics: the graphics device is not recovered (slot 6)"); }
void AtGrDevice::Slot7() { NotRecovered("Atlas Graphics: the graphics device is not recovered (slot 7)"); }
void AtGrDevice::Slot8() { NotRecovered("Atlas Graphics: the graphics device is not recovered (slot 8)"); }
void AtGrDevice::Slot9() { NotRecovered("Atlas Graphics: the graphics device is not recovered (slot 9)"); }
void AtGrDevice::Slot10() { NotRecovered("Atlas Graphics: the graphics device is not recovered (slot 10)"); }
void AtGrDevice::Slot11() { NotRecovered("Atlas Graphics: the graphics device is not recovered (slot 11)"); }
void AtGrDevice::Slot12() { NotRecovered("Atlas Graphics: the graphics device is not recovered (slot 12)"); }
void AtGrDevice::Slot13() { NotRecovered("Atlas Graphics: the graphics device is not recovered (slot 13)"); }
void AtGrDevice::Slot14() { NotRecovered("Atlas Graphics: the graphics device is not recovered (slot 14)"); }
void AtGrDevice::Slot15() { NotRecovered("Atlas Graphics: the graphics device is not recovered (slot 15)"); }
AtGrNameSink* AtGrDevice::NameSink() {
    NotRecovered("Atlas Graphics: the graphics device is not recovered (slot 16)");
    return 0;
}

// ---- the image ----------------------------------------------------------------------------------------------------------

AtImage::AtImage() {
    m_name[0] = 0;
    m_flags = 0;
    m_format = 0;
    m_unknown14 = 0;
    m_unknown16 = 0;
    m_materials = 0;
    m_material = 0xFFFF;
    m_unknown1E = 0;
    m_data = 0;
    m_database = 0;
    m_transparent.SetBlack();
}

AtImage::~AtImage() {
    AtImage::Free();
}

void AtImage::Free() {
    if (m_data != 0) {
        operator delete(m_data);
    }
    m_name[0] = 0;
    m_flags = 0;
    m_format = 0;
    m_unknown14 = 0;
    m_unknown16 = 0;
    m_materials = 0;
    m_unknown1E = 0;
    m_data = 0;
    m_database = 0;
    m_material = 0xFFFF;
    m_transparent.SetBlack();
}

void AtImage::FreeData() {
    if ((m_flags & kOwnsData) != 0) {
        if (m_data != 0) {
            operator delete(m_data);
            m_data = 0;
        }
        m_flags &= ~kOwnsData;
    }
}

unsigned int AtImage::SplitStrips(unsigned int minimum, unsigned int maximum, unsigned int total, unsigned int, unsigned int* out) {
    const unsigned int smallest = minimum < 4 ? 4 : minimum;
    unsigned int count = 0;
    unsigned int width = smallest;
    if (smallest < total) {
        unsigned int next;
        do {
            next = width * 2;
            if (next > maximum) {
                out[count++] = width;
                total -= width;
                next = smallest;
            }
            width = next;
        } while (next < total);
    }
    out[count] = width;
    return count + 1;
}

int AtImage::PassName(AtGrDevice* device, const AtImage* image, int value) {
    return device->NameSink()->TakeName(image->m_name, value);
}

void AtImage::Init(AtMaterialOwner*, AtImageDatabase* database, const char* name, unsigned int flags) {
    if ((m_flags & kRead) != 0) {
        Free();
    }
    m_database = database;
    m_materials = &database->m_materials;
    AtCopyName8(m_name, name);
    m_flags = flags | kRead;
    m_format = flags & kSourceMask;
}

void AtImage::Read(AtMaterialOwner* owner, AtImageDatabase* database, const char* name, AtIoTokenizer* tokenizer) {
    if ((m_flags & kRead) != 0) {
        Free();
    }
    m_database = database;
    m_materials = &database->m_materials;
    AtCopyName8(m_name, name);
    tokenizer->ExpectLeftCurly();
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        ReadWord(owner, tokenizer, token);
    }
    m_flags |= kRead;
}

void AtImage::ReadWord(AtMaterialOwner*, AtIoTokenizer* tokenizer, int token) {
    switch (token) {
    case kWordMatAssignment:
        m_material = (unsigned short)tokenizer->ExpectInt();
        if (m_material >= m_materials->m_count) {
            tokenizer->Error(kOutOfRange);
        }
        break;
    case kWordTransparentColor:
        m_transparent.r = (unsigned char)tokenizer->ExpectInt();
        m_transparent.g = (unsigned char)tokenizer->ExpectInt();
        m_transparent.b = (unsigned char)tokenizer->ExpectInt();
        m_flags |= kTransparent;
        break;
    case kWordColorValueRectangle:
        m_flags |= kColorRectangle;
        m_colorRectangle.Read(tokenizer);
        break;
    case kWordBmpSource:
        m_format = (m_format & ~kSourceMask) | AtImage::kBmpSource;
        break;
    case kWordTgaSource:
        m_format = (m_format & ~kSourceMask) | AtImage::kTgaSource;
        break;
    case kWordPlatformSource:
        m_flags |= AtImage::kPlatformSource;
        m_format = (m_format & ~kSourceMask) | AtImage::kDdsSource;
        break;
    case kWordDdsSource:
        m_format = (m_format & ~kSourceMask) | AtImage::kDdsSource;
        break;
    case kWordDemandLoad:
        m_flags |= AtImage::kDemandLoad;
        break;
    case kWordTm2Source:
        m_format = (m_format & ~kSourceMask) | AtImage::kTm2Source;
        break;
    case kWordTplSource:
        m_format = (m_format & ~kSourceMask) | AtImage::kTplSource;
        break;
    case kWordMpegMacroBlocks:
        m_format |= AtImage::kMpegMacroBlocks;
        break;
    case kWordFlipTextureHorizontally:
        m_flags |= kFlipHorizontally;
        break;
    case kWordFlipTextureVertically:
        m_flags |= kFlipVertically;
        break;
    case kWordInfrequentModify:
        m_format |= AtImage::kInfrequentModify;
        break;
    case kWordFrequentModify:
        m_format |= AtImage::kFrequentModify;
        break;
    default:  // kWordPntSource among them
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        break;
    }
}

void AtImage::SetTransparentColor(const AtColor* color) {
    m_transparent.r = color->r;
    m_transparent.g = color->g;
    m_transparent.b = color->b;
    m_transparent.a = color->a;
    m_flags |= kTransparent;
}

void AtImage::ClearTransparentColor() {
    m_flags &= ~kTransparent;
}
