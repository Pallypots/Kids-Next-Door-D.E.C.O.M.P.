// See include/at/AtFlashSprite.h.
#include "at/AtFlashSprite.h"

#include "AtArray.h"
#include "at/AtAudioBank.h"
#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtCopyName(const char* from, char* to);
void AtFatal(const char* message, int a, int b, int c);
extern "C" {
char* strcpy(char* to, const char* from);
char* strcat(char* to, const char* from);
char* strrchr(const char* text, int c);
AtSize strlen(const char* text);
}

void* AtFlashFrame::s_heap;
void* AtFpPlaceObject2Tag::s_heap;
void* AtFpSprite::s_heap;
AtIoContext* AtAudioBankFile::s_context;

namespace {

enum SpriteWord { kNameSpace = 0x39, kSprites = 0x3A, kSpriteDef = 0x3B, kFrameRefs = 0x3C };
enum FrameWord { kFrames = 0x3A };

const int kBadField = 0x27;

// At 0x00271034.
const AtIoKeyword s_spriteKeywords[] = {
    {kNameSpace, "NameSpace"},
    {kSprites, "Sprites"},
    {kSpriteDef, "SpriteDef"},
    {kFrameRefs, "FrameRefs"},
};

// At 0x00271F98. Only the first two are used here: the rest are the text frames', not recovered.
const AtIoKeyword s_frameKeywords[] = {
    {0x39, "NameSpace"},
    {kFrames, "Frames"},
    {0x3B, "FrameDef"},
    {0x3C, "Tags"},
    {0x3D, "Action"},
    {0x3E, "Place"},
    {0x3F, "Place2"},
    {0x40, "Remove"},
    {0x41, "Remove2"},
    {0x42, "DoSound"},
    {0x43, "Show"},
    {0x44, "SET"},
    {0x45, "MOVE"},
    {0x46, "REPLACE"},
    {0x47, "ShapeID"},
    {0x48, "SpriteID"},
    {0x49, "TextID"},
    {0x4A, "NoID"},
    {0x4B, "CharID"},
    {0x4C, "Name"},
    {0x4D, "ClipDepth"},
    {0x4E, "Trans"},
    {0x4F, "CTrans"},
};

void Stop(const char* what) {
    AtFatal(what, 0, 0, 0);
}

// The file's numbers are little-endian, on any host. The original reads them straight from memory.
unsigned int U32(const unsigned char* p) {
    return (unsigned int)p[0] | (unsigned int)p[1] << 8 | (unsigned int)p[2] << 16 | (unsigned int)p[3] << 24;
}
unsigned short U16(const unsigned char* p) {
    return (unsigned short)(p[0] | p[1] << 8);
}

void* TableHeap(AtFlashResourceTable* table) {
    return table->m_heap != 0 ? table->m_heap : AtFlashResourceTable::s_defaultHeap;
}

// One array, on the table's heap. Null if there was no room, after saying so.
template <class T> T* NewOnTableHeap(AtFlashResourceTable* table, unsigned int count) {
    void* heap = TableHeap(table);
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
    T* items = AtNewArray<T>(count);
    AtPopHeap();
    if (items == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return items;
}

template <class T> T* AddPlacing(AtFlashResourceTable* table, unsigned int count) {
    T* items = NewOnTableHeap<T>(table, count);
    for (unsigned int i = 0; i < count; i++) {
        table->m_items[i] = &items[i];
        items[i].m_heap = TableHeap(table);
    }
    return items;
}

void JoinPath(char* out, const char* directory, const char* name) {
    strcpy(out, directory);
    strcat(out, "\\");
    strcat(out, name);
}

void ReadNameSpace(AtIoTokenizer* tokenizer, char* to) {
    tokenizer->ExpectToken(kNameSpace);
    char* space;
    tokenizer->ExpectStringCopy(&space);
    AtCopyName(space, to);
    operator delete(space);
}

}  // namespace

// ---- a file ---------------------------------------------------------------------------------------------------------

void AtAudioBankFile::Close() {
    if (m_file != 0) {
        m_fs->Close(m_file);
    }
    m_fs = 0;
    m_file = 0;
}

int AtAudioBankFile::Open(const char* path, int flags) {
    AtIoFileSystem* fs;
    if (flags & 1) {
        fs = s_context->Top();
    } else if (s_context->FindFile(path, &fs) != 0) {
        return 1;
    }
    if (m_file != 0) {
        m_fs->Close(m_file);
        m_fs = 0;
        m_file = 0;
    }
    m_fs = fs;
    return fs->Open(path, flags, &m_file);
}

int AtAudioBankFile::OpenWithExtension(const char* path, const char* ext, int flags) {
    char full[0x80];
    strcpy(full, path);
    char* leaf = strrchr(full, '\\');
    if (leaf == 0) {
        leaf = full;
    }
    if (strrchr(leaf, '.') == 0) {
        strcat(full, ".");
        strcat(full, ext);
    }
    return Open(full, flags);
}

int AtAudioBankFile::ReadPending(void* out, unsigned int size, unsigned int* done, AtIoFileRequest** request) {
    return m_file->Read(out, size, done, request);
}

int AtAudioBankFile::Release(AtIoFileRequest* request) {
    if (m_file == 0) {
        return 0;
    }
    m_file->Release(request);
    return 1;
}

int AtAudioBankFile::Read(void* out, unsigned int size, unsigned int* done) {
    AtIoFileRequest* request;
    int status = m_file->Read(out, size, done, &request);
    if (status == AtIoFile::kPending) {
        status = request->Wait();
        if (m_file != 0) {
            m_file->Release(request);
        }
    }
    return status;
}

// ---- tags -----------------------------------------------------------------------------------------------------------

AtFlashTag::AtFlashTag() {
    m_state = 0;
    m_type = -1;
}

AtFlashTag::~AtFlashTag() {
    if (m_state != 0) {
        m_state = 0;
        m_type = -1;
    }
}

void AtFlashTag::Slot1() { Stop("AtFlashTag slot 1 (0x000760A0) is not recovered"); }
void AtFlashTag::Slot6() { Stop("AtFlashTag slot 6 (0x00023BA0) is not recovered"); }

void AtFlashTag::Prepare() {
    if (m_state != 0) {
        Reset();
    }
    m_state = 1;
}

void AtFlashTag::Reset() {
    if (m_state != 0) {
        m_state = 0;
        m_type = -1;
    }
}

void AtFlashIdTag::Prepare() { Stop("AtFlashIdTag::Prepare (0x00023BC0) is not recovered"); }

void AtFlashIdTag::ReadBinary(const unsigned char** data) {
    const unsigned char* at = *data;
    *data = at + 4;
    AtFlashTag::Prepare();
    m_id = (int)U32(at);
    m_read = 1;
}

AtFlashActionTag::AtFlashActionTag() {
    m_type = 4;
    m_id = -1;
    m_read = 0;
}

AtFlashActionTag::~AtFlashActionTag() {
    if (m_read != 0) {
        AtFlashTag::Reset();
        m_type = 4;
        m_id = -1;
        m_read = 0;
    }
}

void AtFlashActionTag::Reset() { Stop("AtFlashActionTag::Reset (0x00023BE0) is not recovered"); }
void AtFlashActionTag::ReadText(AtIoTokenizer*, int) { Stop("AtFlashActionTag::ReadText (0x00023AB0) is not recovered"); }
void AtFlashActionTag::WriteText(AtIoTokenizer*, int) { Stop("AtFlashActionTag::WriteText (0x00023B50) is not recovered"); }

AtFlashRemoveTag::AtFlashRemoveTag() {
    m_type = 2;
    m_id = -1;
    m_read = 0;
}

AtFlashRemoveTag::~AtFlashRemoveTag() {
    if (m_read != 0) {
        AtFlashTag::Reset();
        m_type = 2;
        m_id = -1;
        m_read = 0;
    }
}

void AtFlashRemoveTag::Reset() { Stop("AtFlashRemoveTag::Reset (0x00024690) is not recovered"); }
void AtFlashRemoveTag::ReadText(AtIoTokenizer*, int) { Stop("AtFlashRemoveTag::ReadText (0x000245A0) is not recovered"); }
void AtFlashRemoveTag::WriteText(AtIoTokenizer*, int) { Stop("AtFlashRemoveTag::WriteText (0x00024640) is not recovered"); }

AtFlashSoundTag::AtFlashSoundTag() {
    m_type = 3;
    m_id = -1;
    m_read = 0;
}

AtFlashSoundTag::~AtFlashSoundTag() {
    if (m_read != 0) {
        AtFlashTag::Reset();
        m_type = 3;
        m_id = -1;
        m_read = 0;
    }
}

void AtFlashSoundTag::Reset() { Stop("AtFlashSoundTag::Reset (0x00024880) is not recovered"); }
void AtFlashSoundTag::ReadText(AtIoTokenizer*, int) { Stop("AtFlashSoundTag::ReadText (0x00024790) is not recovered"); }
void AtFlashSoundTag::WriteText(AtIoTokenizer*, int) { Stop("AtFlashSoundTag::WriteText (0x00024830) is not recovered"); }

AtFpPlaceObject2Tag::AtFpPlaceObject2Tag() {
    ResetFields();
}

AtFpPlaceObject2Tag::~AtFpPlaceObject2Tag() {
    AtFlashTag::Reset();
    if (m_loaded != 0) {
        AtFlashTag::Reset();
        if (m_name != 0) {
            operator delete(m_name);
        }
        ResetFields();
    }
}

void AtFpPlaceObject2Tag::Prepare() { Stop("AtFpPlaceObject2Tag::Prepare (0x00023D10) is not recovered"); }
void AtFpPlaceObject2Tag::Reset() { Stop("AtFpPlaceObject2Tag::Reset (0x00023D90) is not recovered"); }
void AtFpPlaceObject2Tag::ReadText(AtIoTokenizer*, int) { Stop("AtFpPlaceObject2Tag::ReadText (0x00023DC0) is not recovered"); }
void AtFpPlaceObject2Tag::WriteText(AtIoTokenizer*, int) { Stop("AtFpPlaceObject2Tag::WriteText (0x00024370) is not recovered"); }

void AtFpPlaceObject2Tag::ResetFields() {
    m_type = 1;
    m_unknown28 = 0xFFFF;
    m_unknown2A = 0xFFFF;
    m_loaded = 0;
    m_name = 0;
    m_unknown30 = -1;
    m_unknown50 = -1;
    m_unknown54 = -1;
    m_transform[0] = 0;
    m_transform[1] = 0;
    m_transform[2] = 0;
    m_transform[3] = 0;
    m_transform[4] = 0x3F800000;
    m_transform[5] = 0x3F800000;
    m_colorOffset[0] = 0;
    m_colorOffset[1] = 0;
    m_colorOffset[2] = 0;
    m_colorOffset[3] = 0;
    m_colorScale[0] = 1.0f;
    m_colorScale[1] = 1.0f;
    m_colorScale[2] = 1.0f;
    m_colorScale[3] = 1.0f;
    m_unknown34 = 0;
}

void AtFpPlaceObject2Tag::SetName(const char* name) {
    if (m_name != 0) {
        operator delete(m_name);
    }
    void* heap = m_heap != 0 ? m_heap : s_heap;
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
    m_name = (char*)operator new((unsigned int)strlen(name) + 1);
    AtPopHeap();
    if (m_name == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    strcpy(m_name, name);
}

void AtFpPlaceObject2Tag::SetColorScale(const unsigned char* bytes) {
    m_colorScale[0] = (float)bytes[0] * 0.0039215689f;
    m_colorScale[1] = (float)bytes[1] * 0.0039215689f;
    m_colorScale[2] = (float)bytes[2] * 0.0039215689f;
    m_colorScale[3] = (float)bytes[3] * 0.0039215689f;
}

void AtFpPlaceObject2Tag::ReadBinary(const unsigned char** data) {
    const unsigned char* record = *data;
    *data = record + 0x18;
    AtFlashTag::Prepare();
    m_unknown34 = record[0];
    m_unknown30 = record[2];
    m_unknown28 = U16(record + 4);
    m_unknown50 = record[3];
    m_unknown54 = (int)U32(record + 8);
    m_unknown2A = U16(record + 6);
    // Offsets from the end of the record, in the part of the data after all the tags. 0 means none.
    const unsigned int nameAt = U32(record + 0xC);
    if (nameAt > 0) {
        SetName((const char*)*data + nameAt);
    }
    const unsigned int transformAt = U32(record + 0x10);
    if (transformAt > 0) {
        const unsigned char* transform = *data + transformAt;
        for (int i = 0; i < 6; i++) {
            m_transform[i] = U32(transform + i * 4);
        }
    }
    const unsigned int colorAt = U32(record + 0x14);
    if (colorAt > 0) {
        const unsigned char* color = *data + colorAt;
        m_colorOffset[0] = color[0];
        m_colorOffset[1] = color[1];
        m_colorOffset[2] = color[2];
        m_colorOffset[3] = color[3];
        SetColorScale(color + 4);
    }
    m_loaded = 1;
}

// ---- a frame --------------------------------------------------------------------------------------------------------

AtFlashFrame::AtFlashFrame() {
    ResetLinks();
    m_tagCapacity = 0;
    m_tagCount = 0;
    m_flags = 0;
    m_tags = 0;
    m_heap = 0;
}

AtFlashFrame::~AtFlashFrame() {
    Clear();
}

void AtFlashFrame::Read(AtIoTokenizer*) {
    Stop("AtFlashFrame::Read (0x000235C0), a text frame of an .AFF, is not recovered");
}

void AtFlashFrame::Clear() {
    if ((m_flags & kUsed) == 0) {
        return;
    }
    if ((m_flags & kTagsNotOwned) == 0) {
        for (unsigned int i = 0; i < m_tagCount; i++) {
            if (m_tags[i] != 0) {
                delete m_tags[i];
            }
        }
    }
    if (m_tags != 0) {
        operator delete(m_tags);
    }
    ResetLinks();
    ResetLinks();
    m_tagCapacity = 0;
    m_tagCount = 0;
    m_flags = 0;
    m_tags = 0;
    m_heap = 0;
}

void AtFlashFrame::ReadBinary(const unsigned char** data, AtFlashActionTag** actions, AtFpPlaceObject2Tag** places, AtFlashRemoveTag** removes, AtFlashSoundTag** sounds) {
    const unsigned char* header = *data;
    *data = header + 8;
    m_index = (int)U32(header);
    const unsigned char placeCount = header[4];
    const unsigned char actionCount = header[5];
    const unsigned char removeCount = header[6];
    const unsigned char soundCount = header[7];
    const unsigned int total = (unsigned int)placeCount + actionCount + removeCount + soundCount;
    m_tagCount = total;
    m_tagCapacity = total;
    if (total > 0) {
        void* heap = m_heap != 0 ? m_heap : s_heap;
        if (heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(heap);
        m_tags = (AtFlashTag**)operator new(m_tagCount * sizeof(AtFlashTag*));
        AtPopHeap();
        if (m_tags == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        unsigned int at = 0;
        for (unsigned int i = 0; i < removeCount; i++) {
            AtFlashRemoveTag* tag = *removes;
            tag->m_heap = m_heap != 0 ? m_heap : s_heap;
            tag->ReadBinary(data);
            m_tags[at++] = tag;
            *removes = tag + 1;
        }
        for (unsigned int i = 0; i < placeCount; i++) {
            AtFpPlaceObject2Tag* tag = *places;
            tag->m_heap = m_heap != 0 ? m_heap : s_heap;
            tag->ReadBinary(data);
            m_tags[at++] = tag;
            *places = tag + 1;
        }
        for (unsigned int i = 0; i < actionCount; i++) {
            AtFlashActionTag* tag = *actions;
            tag->m_heap = m_heap != 0 ? m_heap : s_heap;
            tag->ReadBinary(data);
            m_tags[at++] = tag;
            *actions = tag + 1;
        }
        for (unsigned int i = 0; i < soundCount; i++) {
            AtFlashSoundTag* tag = *sounds;
            tag->m_heap = m_heap != 0 ? m_heap : s_heap;
            tag->ReadBinary(data);
            m_tags[at++] = tag;
            *sounds = tag + 1;
        }
    }
    m_flags |= kUsed | kTagsNotOwned;
}

// ---- the frames ------------------------------------------------------------------------------------------------------

AtFlashFrameTable::AtFlashFrameTable() {
    m_loaded = 0;
    m_actions = 0;
    m_places = 0;
    m_removes = 0;
    m_sounds = 0;
}

AtFlashFrameTable::~AtFlashFrameTable() {
    ClearFrames();
}

void AtFlashFrameTable::Allocate(unsigned int count) {
    AtFlashResourceTable::Allocate(count);
    AddPlacing<AtFlashFrame>(this, count);
}

void AtFlashFrameTable::ClearFrames() {
    if (m_loaded == 0) {
        return;
    }
    if (m_items != 0 && m_items[0] != 0) {
        AtDeleteArray(static_cast<AtFlashFrame*>(m_items[0]));
        for (unsigned int i = 0; i < m_itemCount; i++) {
            m_items[i] = 0;
        }
    }
    if (m_actions != 0) {
        AtDeleteArray(m_actions);
    }
    if (m_places != 0) {
        AtDeleteArray(m_places);
    }
    if (m_removes != 0) {
        AtDeleteArray(m_removes);
    }
    if (m_sounds != 0) {
        AtDeleteArray(m_sounds);
    }
    Clear();
    m_loaded = 0;
    m_actions = 0;
    m_places = 0;
    m_removes = 0;
    m_sounds = 0;
}

void AtFlashFrameTable::Load(const char* name, int mode) {
    if (m_loaded != 0) {
        ClearFrames();
    }
    AtIoTokenFile file;
    char path[0x100];
    JoinPath(path, "Sprites", name);
    if (mode == 2) {
        LoadBinary(path);
        return;
    }
    Init();
    const int status = file.Open(path, "AFF", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AFF");
    }
    AtIoTokenizer* tokenizer = file.m_tokenizer;
    tokenizer->SetKeywords(s_frameKeywords, sizeof(s_frameKeywords) / sizeof(s_frameKeywords[0]), 0);
    ReadNameSpace(tokenizer, m_name);
    if (tokenizer->Next() != kFrames) {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }
    const int count = tokenizer->ExpectCount();
    Allocate((unsigned int)count);
    for (int i = 0; i < count; i++) {
        m_items[i]->Read(tokenizer);
    }
    tokenizer->ExpectRightCurly();
    file.Close();
    m_loaded = 1;
}

void AtFlashFrameTable::LoadBinary(const char* path) {
    AtAudioBankFile file;
    int status = file.OpenWithExtension(path, "XFF", 2);
    if (status != 0) {
        AtReportFileError(status, path, "XFF");
    }
    // 16 bytes: the magic, then what is not read here. 16 more: the counts. See docs/worklog/2026-11-07-afd.md.
    unsigned char ignored[0x10];
    status = file.Read(ignored, sizeof(ignored), 0);
    if (status != 0) {
        AtReportFileError(status, path, "XFF");
    }
    unsigned char counts[0x10];
    status = file.Read(counts, sizeof(counts), 0);
    if (status != 0) {
        AtReportFileError(status, path, "XFF");
    }
    file.m_file->Seek(U16(counts + 2));
    if (AtIoTokenFile::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtIoTokenFile::s_heap);
    const unsigned int size = U32(counts + 4);
    unsigned char* data = (unsigned char*)operator new(size);
    AtPopHeap();
    if (data == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    AtIoFileRequest* request;
    status = file.ReadPending(data, size, 0, &request);
    if (status == AtIoFile::kPending) {
        status = request->Wait();
        file.Release(request);
    }
    if (status != 0) {
        AtReportFileError(status, path, "XFF");
    }
    file.Close();

    Init();
    m_itemCount = U16(counts);
    // The list is not cleared, as Allocate would.
    void* heap = TableHeap(this);
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
    m_items = (AtFlashResource**)operator new(m_itemCount * sizeof(AtFlashResource*));
    AtPopHeap();
    if (m_items == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    AtFlashFrame* frames = NewOnTableHeap<AtFlashFrame>(this, m_itemCount);
    m_actions = NewOnTableHeap<AtFlashActionTag>(this, U16(counts + 10));
    m_places = NewOnTableHeap<AtFpPlaceObject2Tag>(this, U16(counts + 8));
    m_removes = NewOnTableHeap<AtFlashRemoveTag>(this, U16(counts + 12));
    m_sounds = NewOnTableHeap<AtFlashSoundTag>(this, U16(counts + 14));
    const unsigned char* cursor = data;
    AtFlashActionTag* actions = m_actions;
    AtFpPlaceObject2Tag* places = m_places;
    AtFlashRemoveTag* removes = m_removes;
    AtFlashSoundTag* sounds = m_sounds;
    for (unsigned int i = 0; i < m_itemCount; i++) {
        frames[i].m_heap = TableHeap(this);
        m_items[i] = &frames[i];
        frames[i].ReadBinary(&cursor, &actions, &places, &removes, &sounds);
    }
    operator delete(data);
    m_loaded = 1;
}

// ---- a sprite -------------------------------------------------------------------------------------------------------

void AtFpSprite::SetHeap(void* heap) {
    s_heap = heap;
}

AtFpSprite::AtFpSprite() {
    ResetLinks();
    m_refCount = 0;
    m_refCapacity = 0;
    m_refs = 0;
    m_unknown20 = 0;
    m_used = 0;
    m_heap = 0;
}

AtFpSprite::~AtFpSprite() {
    Clear();
}

void AtFpSprite::Read(AtIoTokenizer* tokenizer) {
    ReadData(tokenizer);
}

void AtFpSprite::Clear() {
    if (m_used == 0) {
        return;
    }
    if (m_refs != 0) {
        for (unsigned int i = 0; i < m_refCapacity; i++) {
            if (m_refs[i].m_name != 0) {
                operator delete(m_refs[i].m_name);
            }
        }
        operator delete(m_refs);
    }
    ResetLinks();
    ResetLinks();
    m_refCount = 0;
    m_refCapacity = 0;
    m_refs = 0;
    m_unknown20 = 0;
    m_used = 0;
    m_heap = 0;
}

void AtFpSprite::Allocate(int index, unsigned int capacity) {
    if (m_used != 0) {
        Clear();
    }
    m_index = index;
    m_refCapacity = capacity;
    m_refCount = 0;
    void* heap = m_heap != 0 ? m_heap : s_heap;
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
    m_refs = (FrameRef*)operator new(capacity * sizeof(FrameRef));
    AtPopHeap();
    if (m_refs == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < m_refCapacity; i++) {
        m_refs[i].m_name = 0;
        m_refs[i].m_frame = -1;
        m_refs[i].m_unknown08 = 0;
    }
    m_used = 1;
}

void AtFpSprite::ReadData(AtIoTokenizer* tokenizer) {
    if (tokenizer->Next() != kSpriteDef) {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }
    tokenizer->ExpectLeftCurly();
    const int index = tokenizer->ExpectInt();
    if (tokenizer->Next() != kFrameRefs) {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }
    const unsigned int count = (unsigned int)tokenizer->ExpectCount();
    Allocate(index, count);
    m_refCount = count;
    // "57,"Init"" a frame and its name; "58;" a frame; "3:7" frames 3 to 7; "3*4" frame 3 four times. No bound on the last two: a run past the capacity writes past the list.
    for (unsigned int i = 0; i < m_refCount; i++) {
        m_refs[i].m_unknown08 = 0;
        m_refs[i].m_frame = tokenizer->ExpectInt();
        switch (tokenizer->Next()) {
        case AtIoTokenizer::kComma: {
            const char* text = tokenizer->ExpectString();
            if (*text == 0) {
                AtFatal("AtFpSprite::vReadData Expected string", 0, 0, 0);
                break;
            }
            void* heap = m_heap != 0 ? m_heap : s_heap;
            if (heap == 0) {
                AtRaise(-2, 0, 0, 0);
            }
            AtPushHeap(heap);
            char* copy = (char*)operator new((unsigned int)strlen(text) + 1);
            AtPopHeap();
            if (copy == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            strcpy(copy, text);
            m_refs[i].m_name = copy;
            break;
        }
        case AtIoTokenizer::kSemicolon:
            m_refs[i].m_name = 0;
            break;
        case AtIoTokenizer::kColon: {
            const unsigned int span = (unsigned int)tokenizer->ExpectInt() + 1 - (unsigned int)m_refs[i].m_frame;
            unsigned int k = 1;
            for (; k < span; k++) {
                m_refs[i + k].m_frame = m_refs[i].m_frame + (int)k;
                m_refs[i].m_name = 0;
            }
            i += k - 1;
            break;
        }
        case AtIoTokenizer::kTimes: {
            const unsigned int times = (unsigned int)tokenizer->ExpectInt();
            const unsigned int end = i + times;
            unsigned int k = i + 1;
            if (k < end) {
                for (; k < end; k++) {
                    m_refs[k].m_frame = m_refs[k - 1].m_frame;
                    m_refs[k].m_name = 0;
                }
            }
            i = k - 1;
            break;
        }
        default:
            tokenizer->Error(kBadField);
            break;
        }
    }
    tokenizer->ExpectRightCurly();
    tokenizer->ExpectRightCurly();
}

// ---- the sprites -----------------------------------------------------------------------------------------------------

AtFlashSpriteTable::AtFlashSpriteTable() {
    m_loaded = 0;
    m_heap = 0;
}

AtFlashSpriteTable::~AtFlashSpriteTable() {
    ClearSprites();
}

void AtFlashSpriteTable::Allocate(unsigned int count) {
    AtFlashResourceTable::Allocate(count);
    AddPlacing<AtFpSprite>(this, count);
}

void AtFlashSpriteTable::Release(unsigned int) {
    Stop("AtFlashSpriteTable::Release (0x00012540) is not recovered");
}

void AtFlashSpriteTable::ClearSprites() {
    if (m_loaded == 0) {
        return;
    }
    for (unsigned int i = 0; i < m_groupCount; i++) {
        RecycleGroup(1);
    }
    m_frames.ClearFrames();
    if (m_items != 0 && m_items[0] != 0) {
        AtDeleteArray(static_cast<AtFpSprite*>(m_items[0]));
        for (unsigned int i = 0; i < m_itemCount; i++) {
            m_items[i] = 0;
        }
    }
    Clear();
    m_loaded = 0;
    m_heap = 0;
}

void AtFlashSpriteTable::Load(const char* name, int mode) {
    if (m_loaded != 0) {
        ClearSprites();
    }
    AtIoTokenFile file;
    Init();
    // The first of these stores what is already there.
    AtFlashResourceTable::SetDefaultHeap(AtFlashResourceTable::s_defaultHeap);
    AtFpSprite::SetHeap(AtFlashResourceTable::s_defaultHeap);
    m_frames.m_heap = TableHeap(this);
    m_frames.Load(name, mode);
    char path[0x100];
    JoinPath(path, "Sprites", name);
    const int status = file.Open(path, "AFP", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AFP");
    }
    AtIoTokenizer* tokenizer = file.m_tokenizer;
    tokenizer->SetKeywords(s_spriteKeywords, sizeof(s_spriteKeywords) / sizeof(s_spriteKeywords[0]), 0);
    ReadNameSpace(tokenizer, m_name);
    if (tokenizer->Next() != kSprites) {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }
    const int count = tokenizer->ExpectCount();
    Allocate((unsigned int)count);
    for (int i = 0; i < count; i++) {
        m_items[i]->Read(tokenizer);
    }
    tokenizer->ExpectRightCurly();
    file.Close();
    m_loaded = 1;
}
