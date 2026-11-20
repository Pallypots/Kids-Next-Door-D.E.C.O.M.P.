// Image files as a texture reads them: the base of the readers, and the DDS reader. See include/at/AtGrD3DTextureLoad.h. Names inferred, but for "pixel map" and "DDS", from the original's messages.
#include "at/AtGrD3DDatabase.h"
#include "at/AtGrD3DTextureLoad.h"
#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
extern "C" void* memcpy(void* to, const void* from, decltype(sizeof(0)) count);
extern "C" decltype(sizeof(0)) strlen(const char* text);
extern "C" char* strcpy(char* to, const char* from);
extern "C" char* strcat(char* to, const char* from);
extern "C" char* strrchr(const char* text, int c);

void* AtGrIPixelMap::s_heap;
AtGrDdsPixelMap AtGrDdsPixelMap::s_map;

// ---- The base -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void AtGrIPixelMap::SetHeap(void* heap) {
    s_heap = heap;
}

AtGrIPixelMap::AtGrIPixelMap() {
    m_name[0] = 0;
    m_width = 0;
    m_height = 0;
    m_pitch = 0;
    m_paletteCount = 0;
}

// The same work as Free, written out; then the file's own destructor.
AtGrIPixelMap::~AtGrIPixelMap() {
    m_format.Clear();
    m_file.Close();
    m_name[0] = 0;
    m_width = 0;
    m_height = 0;
    m_pitch = 0;
    m_paletteCount = 0;
}

void AtGrIPixelMap::Free() {
    m_format.Clear();
    m_file.Close();
    m_name[0] = 0;
    m_width = 0;
    m_height = 0;
    m_pitch = 0;
    m_paletteCount = 0;
}

// The text is made on the base's heap; it is too short for a name of more than four letters, but the name is always empty here.
void AtGrIPixelMap::Fail(const char* message) {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    char* text = (char*)operator new(strlen(message) + 0x18);
    AtPopHeap();
    if (text == 0) {
        AtRaise(-4, 0, 0, 0);
        AtFatal(message, 0, 0, 0);
        return;
    }
    strcpy(text, m_name);
    strcat(text, ": ");
    strcat(text, message);
    AtFatal(text, 0, 0, 0);
    operator delete(text);
}

void AtGrIPixelMap::FailIo(int status) {
    Fail(AtIoStatusText(status));
}

int AtGrIPixelMap::Open(AtIoFileSystem* fs, const char* path) {
    if (m_format.m_flags != 0) {
        Free();
    }
    const int status = m_file.OpenOn(fs, path, 2);
    if (status != 0) {
        return status;
    }
    ReadHeader();
    return 0;
}

int AtGrIPixelMap::TryLoad(const char* name) {
    AtIoFileSystem* fs = 0;
    char path[0x10C];
    strcpy(path, name);
    if (strrchr(path, '.') == 0) {
        strcat(path, ".");
        strcat(path, Extension());
    }
    if (AtIoFileRef::s_context->FindFile(path, &fs) != 0) {
        return 1;
    }
    return Open(fs, path);
}

void AtGrIPixelMap::Load(const char* name) {
    if (TryLoad(name) == 0) {
        return;
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    char* text = (char*)operator new(strlen(name) + 0x40);
    AtPopHeap();
    if (text == 0) {
        AtRaise(-4, 0, 0, 0);
        AtFatal("Pixel map file not found.", 0, 0, 0);
        return;
    }
    strcpy(text, name);
    strcat(text, ": Unable to find ");
    strcat(text, Extension());
    strcat(text, " pixel map file.");
    AtFatal(text, 0, 0, 0);
    operator delete(text);
}

// ---- The DDS reader -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace {

// The file is read a byte at a time, least significant first.
unsigned int Word(const unsigned char* at) {
    return (unsigned int)at[0] | (unsigned int)at[1] << 8 | (unsigned int)at[2] << 16 | (unsigned int)at[3] << 24;
}

unsigned int Pitch(unsigned int width, unsigned int bits) {
    return ((bits * width + 31) >> 3) & ~3u;
}

// A level's bytes: each level a quarter of the one before, no less than a block.
unsigned int LevelSize(unsigned int first, unsigned int level, unsigned int least) {
    const unsigned int size = first >> (2 * level);
    return size < least ? least : size;
}

}  // namespace

AtGrDdsPixelMap::AtGrDdsPixelMap() {
    m_fileSize = 0;
    m_buffer = 0;
    m_data = 0;
    m_depth = 0;
    m_levelSize = 0;
    m_levels = 0;
    m_kind = kPlain;
    for (int i = 0; i < 0x80; i++) {
        m_header[i] = 0;
    }
}

// Nothing of its own: the file's bytes are not freed.
AtGrDdsPixelMap::~AtGrDdsPixelMap() {
}

void AtGrDdsPixelMap::ReadHeader() {
    m_fileSize = m_file.m_file->m_size;
    unsigned int done;
    const int status = m_file.Read(m_header, 0x80, &done);
    if (status != 0) {
        FailIo(status);
    }
    Parse(m_header);
}

void AtGrDdsPixelMap::Parse(const unsigned char* header) {
    if (m_format.m_flags != 0) {
        Free();
    }
    if (Word(header) != 0x20534444) {  // "DDS "
        Fail("Unsupported DDS file type");
    }
    const unsigned int size = Word(header + 0x04);
    const unsigned int flags = Word(header + 0x08);
    const unsigned int height = Word(header + 0x0C);
    const unsigned int width = Word(header + 0x10);
    const unsigned int pitch = Word(header + 0x14);
    const unsigned int depth = Word(header + 0x18);
    const unsigned int levels = Word(header + 0x1C);
    unsigned int format[8];  // at +0x4C: size, flags, four characters, bits, four masks
    for (int i = 0; i < 8; i++) {
        format[i] = Word(header + 0x4C + 4 * i);
    }
    const unsigned int caps = Word(header + 0x6C);
    const unsigned int caps2 = Word(header + 0x70);
    if (size != 0x7C || (flags & 0x1007) != 0x1007) {  // caps, height, width, pixel format
        Fail("Invalid DDS file header size");
    }
    if ((flags & 0x800000) != 0 && (caps2 & 0x200000) != 0) {  // a depth, and a volume
        m_kind = kVolume;
    } else if ((caps & 0x8) != 0 && (caps2 & 0xFC00) == 0xFC00) {  // complex, with all six faces
        m_kind = kCube;
    } else {
        m_kind = kPlain;
    }
    m_width = width;
    m_height = height;
    m_depth = depth;
    m_levels = levels;
    if (levels == 0) {
        m_levels = 1;
    }
    AtGrDdsFormat(format, &m_format);
    m_paletteCount = 0;
    if (header != m_header) {
        m_data = header + 0x80;
    }
    if ((flags & 0x80000) != 0) {  // a linear size
        m_pitch = Pitch(m_width, m_format.m_bits);
        m_levelSize = pitch;
    } else if ((flags & 0x8) != 0) {  // a pitch
        m_levelSize = m_height * pitch;
        m_pitch = pitch;
    } else {
        const unsigned int computed = Pitch(m_width, m_format.m_bits);
        m_levelSize = m_height * computed;
        m_pitch = computed;
    }
}

void AtGrDdsPixelMap::Free() {
    if (m_buffer != 0) {
        operator delete(m_buffer);
    }
    AtGrIPixelMap::Free();
    m_fileSize = 0;
    m_buffer = 0;
    m_data = 0;
    m_depth = 0;
    m_levelSize = 0;
    m_levels = 0;
    m_kind = kPlain;
    for (int i = 0; i < 0x80; i++) {
        m_header[i] = 0;
    }
}

void AtGrDdsPixelMap::Slot5(unsigned int, unsigned int, unsigned int) {
}

unsigned int AtGrDdsPixelMap::FileSize() {
    return m_fileSize;
}

void AtGrDdsPixelMap::ReadFile(void* buffer) {
    memcpy(buffer, m_header, 0x80);
    unsigned int done;
    const int status = m_file.Read((unsigned char*)buffer + 0x80, m_fileSize - 0x80, &done);
    if (status != 0) {
        FailIo(status);
    }
}

const char* AtGrDdsPixelMap::Extension() {
    return "Dds";
}

void AtGrDdsPixelMap::CopyLevel(AtMaterialOwner* owner, AtGrD3DSurface2D* surface, unsigned int level, unsigned int face) {
    if (m_data == 0) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        m_buffer = (unsigned char*)operator new(m_fileSize);
        AtPopHeap();
        if (m_buffer == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        ReadFile(m_buffer);
        m_data = m_buffer + 0x80;
    }
    // A compressed level is in rows of blocks four pixels high, the surface's height deciding how many; a block of 16 pixels is the least a level takes.
    unsigned int least;
    unsigned int rows;
    if ((m_format.m_flags & 0x7C0) != 0) {
        least = (m_format.m_bits << 4) >> 3;
        rows = surface->m_unknown22 >> 2;
        if (rows < 1) {
            rows = 1;
        }
    } else {
        rows = m_height >> level;
        least = 1;
    }
    unsigned int offset = 0;
    if (face > 0) {
        for (unsigned int i = 0; i < m_levels; i++) {
            offset += LevelSize(m_levelSize, i, least);
        }
        offset *= face;
    }
    for (unsigned int i = 0; i < level; i++) {
        offset += LevelSize(m_levelSize, i, least);
    }
    const unsigned int size = LevelSize(m_levelSize, level, least);
    const unsigned int width = m_width >> level;
    const unsigned int height = m_height >> level;
    if (surface->m_format.Equals(m_format) && width == surface->m_unknown20 && height == surface->m_unknown22) {
        if ((surface->m_unknown1C & AtTextureBase::kRenderTarget) != 0) {
            return;
        }
        void* bits;
        int pitch;
        surface->Lock(&bits, &pitch, 2);
        const unsigned char* from = m_data + offset;
        if ((unsigned int)pitch * rows == size) {
            memcpy(bits, from, size);
        } else {
            unsigned char* to = (unsigned char*)bits;
            unsigned char* const end = to + (unsigned int)pitch * rows;
            const unsigned int row = size / rows;
            while (to != end) {
                memcpy(to, from, row);
                to += pitch;
                from += row;
            }
        }
        surface->Unlock();
        return;
    }
    if ((m_format.m_flags & 0x7C0) != 0 || (surface->m_format.m_flags & 0x7C0) != 0) {
        AtFatal("Atlas Graphics: unsupported DDS pixel format or size encountered", 0, 0, 0);
    }
    // Converting a level into a surface of another format or size, through the surface's pixel slots and the base's palette (0x0005F280, 0x00065E50, 0x0005F500, 0x0005F2C0), is not recovered.
    (void)owner;
    AtGrD3DUnrecovered(0x0005F280, surface, level, face);
}

void AtGrDdsFormat(const unsigned int* format, AtPixelFormat* out) {
    const unsigned int flags = format[1];
    if ((flags & 0x4) != 0) {  // four characters
        switch (format[2]) {
        case 0x31545844:  // DXT1
            out->SetDxt1(0);
            return;
        case 0x32545844:
            out->SetDxt2(0);
            return;
        case 0x33545844:
            out->SetDxt3(0);
            return;
        case 0x34545844:
            out->SetDxt4(0);
            return;
        case 0x35545844:
            out->SetDxt5(0);
            return;
        default:
            AtFatal("Atlas Graphics: unknown DDS pixel format fourcc", 0, 0, 0);
            return;
        }
    }
    if ((flags & 0x40) != 0) {  // RGB
        if ((flags & 0x1) != 0) {  // with alpha
            out->SetRgba((unsigned short)format[3], format[4], format[5], format[6], format[7], 0);
        } else {
            out->SetRgba((unsigned short)format[3], format[4], format[5], format[6], 0, 0);
        }
        return;
    }
    if ((flags & 0x20002) != 0) {  // luminance, or alpha alone
        out->SetLum((unsigned short)format[3], format[4], format[7], 0);
        return;
    }
    // The original leaves a word on its stack on this path: had AtFatal come back, it would return to address 0.
    AtFatal("Atlas Graphics: unknown DDS pixel format", 0, 0, 0);
}
