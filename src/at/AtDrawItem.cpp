// See include/at/AtDrawItem.h.
#include "at/AtDrawItem.h"

#include "at/AtIoFileSystem.h"
#include "at/AtScene.h"
#include "AtArray.h"
#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int file, int line, int unused);

void* AtImageDrawDatabase::s_heap;
void* AtTextDrawDatabase::s_heap;

// 0x002B18A8. In the original's order.
const AtIoKeyword AtImageDrawDatabase::s_keywords[AtImageDrawDatabase::kKeywordCount] = {
    {0x39, "NameSpace"}, {0x3A, "ImageDraw"}, {0x3F, "Image"}, {0x40, "ClipRectangle"}, {0x41, "ColorValueRectangle"}, {0x3E, "Scale"}, {0x3B, "Position"}, {0x3C, "CenterHorizontally"},
    {0x3D, "CenterVertically"}, {0x42, "NormalizedCoordinates"}, {0x43, "DrawArea"},
};

// 0x002B1910. In the original's order.
const AtIoKeyword AtTextDrawDatabase::s_keywords[AtTextDrawDatabase::kKeywordCount] = {
    {0x39, "NameSpace"}, {0x3A, "TextDraw"}, {0x3B, "String"}, {0x40, "Font"}, {0x41, "WrapWidth"}, {0x42, "Kerning"}, {0x43, "ClipRectangle"}, {0x44, "ColorValueRectangle"}, {0x3F, "Scale"},
    {0x3C, "Position"}, {0x3D, "CenterHorizontally"}, {0x3E, "CenterVertically"}, {0x45, "NormalizedCoordinates"}, {0x46, "DrawArea"},
};

namespace {

enum Word { kNameSpace = 0x39, kItemWord = 0x3A };
enum ImageWord { kImagePosition = 0x3B, kImageCenterH = 0x3C, kImageCenterV = 0x3D, kImageScale = 0x3E, kImageImage = 0x3F, kImageClip = 0x40, kImageColors = 0x41, kImageNormalized = 0x42, kImageDrawArea = 0x43 };
enum TextWord { kTextString = 0x3B, kTextPosition = 0x3C, kTextCenterH = 0x3D, kTextCenterV = 0x3E, kTextScale = 0x3F, kTextFont = 0x40, kTextWrapWidth = 0x41, kTextKerning = 0x42, kTextClip = 0x43, kTextColors = 0x44, kTextNormalized = 0x45, kTextDrawArea = 0x46 };

// A code passed to the tokenizer's Error, by the base class that takes no word. The same as the .AGD list's "list already given".
enum Problem { kNoSuchWord = 0x27 };

const float kLargestReal = 3.4028234663852886e38f;  // 0x7F7FFFFF

}  // namespace

// The colour rectangle, AtColorRectangle, is in src/at/AtImage.cpp.

// ---- the base ---------------------------------------------------------------------------------------------------

void AtSceneDevice::DrawScale(float*) {
    AtFatal("not recovered: a concrete device's scale, in the graphics layer", 0, 0, 0);
}

void* AtSceneDevice::FindFont(const char* name, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        return m_fontSpaces.Find(ns, name);
    }
    return m_fonts.Find(name);
}

void* AtSceneDevice::FindImage(const char* name, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        return m_imageSpaces.Find(ns, name);
    }
    return m_images.Find(name);
}

AtDrawItem::AtDrawItem() {
    Reset();
}

AtDrawItem::~AtDrawItem() {
    Reset();
}

void AtDrawItem::Reset() {
    m_kind = 0;
    m_flags = 0;
    m_unknown0C = 0;
    m_owner = 0;
    m_x = 0;
    m_y = 0;
    m_drawWidth = 0;
    m_drawHeight = 0;
    m_clip[0] = 0;
    m_clip[1] = 0;
    m_clip[2] = 0;
    m_clip[3] = 0;
    AtColor white;
    white.SetWhite();
    m_colors.SetAll(white);
    m_scaleX = 1.0f;
    m_scaleY = 1.0f;
    m_deviceScaleX = 1.0f;
    m_deviceScaleY = 1.0f;
}

void AtDrawItem::ReadWord(AtIoTokenizer* tokenizer, int) {
    tokenizer->Error(kNoSuchWord);
}

void AtDrawItem::Clear() {
    Reset();
}

void AtDrawItem::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_owner != 0) {
        Clear();
    }
    m_owner = owner;
    owner->m_device->DrawScale(&m_deviceScaleX);
    m_scaleX = m_deviceScaleX;
    m_scaleY = m_deviceScaleY;
    tokenizer->ExpectLeftCurly();
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kRightCurly) {
        ReadWord(tokenizer, token);
        token = tokenizer->Next();
    }
}

// ---- an image ---------------------------------------------------------------------------------------------------

AtImageDraw::AtImageDraw() {
    m_image = 0;
}

AtImageDraw::~AtImageDraw() {
    if (m_owner != 0) {
        Reset();
        m_image = 0;
    }
}

void AtImageDraw::Clear() {
    if (m_owner != 0) {
        Reset();
        m_image = 0;
    }
}

void AtImageDraw::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_owner != 0) {
        Clear();
    }
    m_kind = kImage;
    AtDrawItem::Read(owner, tokenizer);
}

void AtImageDraw::ReadWord(AtIoTokenizer* tokenizer, int token) {
    switch (token) {
    case kImageImage: {
        char name[8];
        char ns[8];
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        m_image = m_owner->m_device->FindImage(name, ns);
        break;
    }
    case kImagePosition:
        m_x = tokenizer->ExpectReal();
        m_y = tokenizer->ExpectReal();
        break;
    case kImageDrawArea:
        m_drawWidth = tokenizer->ExpectReal();
        m_drawHeight = tokenizer->ExpectReal();
        break;
    case kImageCenterH:
        m_flags |= kCenterHorizontally;
        break;
    case kImageCenterV:
        m_flags |= kCenterVertically;
        break;
    case kImageScale:
        m_scaleX = tokenizer->ExpectReal() * m_scaleX;
        m_scaleY = tokenizer->ExpectReal() * m_scaleY;
        break;
    case kImageClip:
        m_clip[0] = tokenizer->ExpectReal() * m_deviceScaleX;
        m_clip[1] = tokenizer->ExpectReal() * m_deviceScaleY;
        m_clip[2] = tokenizer->ExpectReal() * m_deviceScaleX;
        m_clip[3] = tokenizer->ExpectReal() * m_deviceScaleY;
        break;
    case kImageColors:
        m_flags |= kHasColors;
        m_colors.Read(tokenizer);
        break;
    case kImageNormalized:
        m_flags |= kNormalizedCoordinates;
        break;
    default:
        AtDrawItem::ReadWord(tokenizer, token);
        break;
    }
}

void AtImageDraw::Draw(void*) {
    AtFatal("not recovered: AtImageDraw::Draw", 0, 0, 0);
}

// ---- text -------------------------------------------------------------------------------------------------------

AtTextDraw::AtTextDraw() {
    m_font = 0;
    m_string = 0;
    m_wrapWidth = 0;
    m_kerning = kLargestReal;
}

AtTextDraw::~AtTextDraw() {
    if (m_owner != 0) {
        Reset();
        m_font = 0;
        m_string = 0;
        m_wrapWidth = 0;
        m_kerning = kLargestReal;
    }
}

void AtTextDraw::Clear() {
    if (m_owner != 0) {
        Reset();
        m_font = 0;
        m_string = 0;
        m_wrapWidth = 0;
        m_kerning = kLargestReal;
    }
}

void AtTextDraw::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_owner != 0) {
        Clear();
    }
    m_kind = kText;
    AtDrawItem::Read(owner, tokenizer);
}

void AtTextDraw::ReadWord(AtIoTokenizer* tokenizer, int token) {
    switch (token) {
    case kTextFont: {
        char name[8];
        char ns[8];
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        m_font = m_owner->m_device->FindFont(name, ns);
        break;
    }
    case kTextString: {
        char name[8];
        tokenizer->ExpectName(name);
        // The owner's word at +0x0C is an AtIoContext on this path, as it is where 0x0002EEFC passes it to a string list; a sound owner's is its bank loader.
        m_string = ((AtIoContext*)m_owner->m_loader)->m_defaultGroup.Find(name);
        break;
    }
    case kTextPosition:
        m_x = tokenizer->ExpectReal() * m_deviceScaleX;
        m_y = tokenizer->ExpectReal() * m_deviceScaleY;
        break;
    case kTextDrawArea:
        m_drawWidth = tokenizer->ExpectReal();
        m_drawHeight = tokenizer->ExpectReal();
        break;
    case kTextCenterH:
        m_flags |= kCenterHorizontally;
        break;
    case kTextCenterV:
        m_flags |= kCenterVertically;
        break;
    case kTextScale:
        m_scaleX = tokenizer->ExpectReal() * m_scaleX;
        m_scaleY = tokenizer->ExpectReal() * m_scaleY;
        break;
    case kTextWrapWidth:
        m_wrapWidth = tokenizer->ExpectReal();
        break;
    case kTextKerning:
        m_kerning = tokenizer->ExpectReal();
        break;
    case kTextClip:
        m_clip[0] = tokenizer->ExpectReal() * m_deviceScaleX;
        m_clip[1] = tokenizer->ExpectReal() * m_deviceScaleY;
        m_clip[2] = tokenizer->ExpectReal() * m_deviceScaleX;
        m_clip[3] = tokenizer->ExpectReal() * m_deviceScaleY;
        break;
    case kTextColors:
        m_flags |= kHasColors;
        m_colors.Read(tokenizer);
        break;
    case kTextNormalized:
        m_flags |= kNormalizedCoordinates;
        break;
    default:
        AtDrawItem::ReadWord(tokenizer, token);
        break;
    }
}

void AtTextDraw::Draw(void*) {
    AtFatal("not recovered: AtTextDraw::Draw", 0, 0, 0);
}

// ---- the lists --------------------------------------------------------------------------------------------------

void AtImageDrawDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtImageDrawDatabase::AtImageDrawDatabase() {
    m_draws = 0;
    m_count = 0;
}

AtImageDrawDatabase::~AtImageDrawDatabase() {
    AtImageDrawDatabase::Clear();
}

void AtImageDrawDatabase::Clear() {
    if (m_draws != 0) {
        AtDeleteArray(m_draws);
    }
    GmNameMap::Clear();
    m_draws = 0;
    m_count = 0;
}

void AtImageDrawDatabase::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_entries != 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    char ns[8];
    const char* space = 0;
    int token = tokenizer->Next();
    if (token == kNameSpace) {
        tokenizer->ExpectName(ns);
        token = tokenizer->Next();
        space = ns;
    }
    while (token != AtIoTokenizer::kEnd) {
        if (token != kItemWord) {
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        } else {
            m_count = (unsigned int)tokenizer->ExpectCount();
            if (s_heap == 0) {
                AtRaise(-2, 0, 0, 0);
            }
            AtPushHeap(s_heap);
            m_draws = AtNewArray<AtImageDraw>(m_count);
            AtPopHeap();
            if (m_draws == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            Init(m_count);
            owner->AddToFirst(this, space);
            for (unsigned int i = 0; i < m_count; i++) {
                char name[8];
                tokenizer->ExpectToken(kItemWord);
                tokenizer->ExpectName(name);
                Insert(name, m_draws + i);
                m_draws[i].Read(owner, tokenizer);
            }
            tokenizer->ExpectRightCurly();
        }
        token = tokenizer->Next();
    }
}

void AtImageDrawDatabase::Load(AtNameTableOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "ASI", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "ASI");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}

void AtTextDrawDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtTextDrawDatabase::AtTextDrawDatabase() {
    m_draws = 0;
    m_count = 0;
}

AtTextDrawDatabase::~AtTextDrawDatabase() {
    AtTextDrawDatabase::Clear();
}

void AtTextDrawDatabase::Clear() {
    if (m_draws != 0) {
        AtDeleteArray(m_draws);
    }
    GmNameMap::Clear();
    m_draws = 0;
    m_count = 0;
}

void AtTextDrawDatabase::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_entries != 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    char ns[8];
    const char* space = 0;
    int token = tokenizer->Next();
    if (token == kNameSpace) {
        tokenizer->ExpectName(ns);
        token = tokenizer->Next();
        space = ns;
    }
    while (token != AtIoTokenizer::kEnd) {
        if (token != kItemWord) {
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        } else {
            m_count = (unsigned int)tokenizer->ExpectCount();
            if (s_heap == 0) {
                AtRaise(-2, 0, 0, 0);
            }
            AtPushHeap(s_heap);
            m_draws = AtNewArray<AtTextDraw>(m_count);
            AtPopHeap();
            if (m_draws == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            Init(m_count);
            owner->AddToSecond(this, space);
            for (unsigned int i = 0; i < m_count; i++) {
                char name[8];
                tokenizer->ExpectToken(kItemWord);
                tokenizer->ExpectName(name);
                Insert(name, m_draws + i);
                m_draws[i].Read(owner, tokenizer);
            }
            tokenizer->ExpectRightCurly();
        }
        token = tokenizer->Next();
    }
}

void AtTextDrawDatabase::Load(AtNameTableOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AST", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AST");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}
