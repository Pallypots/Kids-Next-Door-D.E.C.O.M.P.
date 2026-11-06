// GmRumbleEffects and what it holds: the reader of RUMBLE.RMB. Original address clusters 0x000741B0-0x000748D6, 0x000748E0-0x000749ED, 0x001015C0, 0x000EB540.
// Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "gm/GmNameMap.h"
#include "gm/GmLevelManager.h"

void* GmRumbleFrameArray::s_heap;
void* GmRumbleEffectArray::s_heap;

namespace {

enum RumbleToken { kRumbleEffects = 0x39, kRumbleEffect = 0x3A, kKeyFrames = 0x3B, kKeyFrame = 0x3C, kLeft = 0x3D, kRight = 0x3E };

// 0x002B2940.
const AtIoKeyword kKeywords[] = {
    {kRumbleEffects, "RumbleEffects"}, {kRumbleEffect, "RumbleEffect"}, {kKeyFrames, "KeyFrames"}, {kKeyFrame, "KeyFrame"}, {kLeft, "Left"}, {kRight, "Right"},
};

}  // namespace

GmRumbleFrame::GmRumbleFrame() {
    m_time = 0;
    m_left = 0;
    m_right = 0;
}

void GmRumbleFrameArray::Resize(unsigned int count) {
    if (count == 0) {
        if (m_items != 0) {
            operator delete(m_items);
            m_items = 0;
        }
        m_count = 0;
        return;
    }
    if (count == m_count) {
        return;
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    void* memory = operator new(count * sizeof(GmRumbleFrame));
    GmRumbleFrame* items = 0;
    if (memory != 0) {
        items = (GmRumbleFrame*)memory;
        for (unsigned int i = 0; i < count; i++) {
            items[i] = GmRumbleFrame();
        }
    }
    AtPopHeap();
    if (items == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    if (m_items != 0) {
        const unsigned int keep = count < m_count ? count : m_count;
        for (unsigned int i = 0; i < keep; i++) {
            items[i] = m_items[i];
        }
        operator delete(m_items);
    }
    m_items = items;
    m_count = count;
}

void GmRumbleFrameArray::Assign(const GmRumbleFrame* frames, unsigned int count) {
    Resize(count);
    for (unsigned int i = 0; i < count; i++) {
        m_items[i] = frames[i];
    }
}

GmRumbleEffect::GmRumbleEffect() {
    m_frames.m_items = 0;
    m_frames.m_count = 0;
    m_ready = 0;
}

GmRumbleEffect::~GmRumbleEffect() {
    if (m_frames.m_items != 0) {
        operator delete(m_frames.m_items);
        m_frames.m_items = 0;
    }
    m_frames.m_count = 0;
    m_ready = 0;
}

GmRumbleEffect& GmRumbleEffect::operator=(const GmRumbleEffect& other) {
    m_ready = other.m_ready;
    for (int i = 0; i < 8; i++) {
        m_name[i] = other.m_name[i];
    }
    if (&m_frames != &other.m_frames) {
        if (other.m_frames.m_count != 0) {
            m_frames.Assign(other.m_frames.m_items, other.m_frames.m_count);
        } else if (m_frames.m_items != 0) {
            operator delete(m_frames.m_items);
            m_frames.m_count = 0;
            m_frames.m_items = 0;
        }
    }
    return *this;
}

void GmRumbleEffect::Init(const char* name, unsigned int frames) {
    if (m_ready != 0) {
        m_frames.Resize(0);
        m_ready = 0;
    }
    AtCopyName8(m_name, name);
    m_frames.Resize(frames);
    m_ready = 1;
}

GmRumbleFrame* GmRumbleEffect::Frame(unsigned int index) {
    return &m_frames.m_items[index];
}

void GmRumbleEffect::SetFrame(unsigned int index, const GmRumbleFrame& frame) {
    m_frames.m_items[index] = frame;
}

const char* GmRumbleEffect::Name() {
    return m_name;
}

void GmRumbleEffectArray::Free() {
    delete[] m_items;
    m_items = 0;
    m_count = 0;
}

void GmRumbleEffectArray::Resize(unsigned int count) {
    if (count == 0) {
        delete[] m_items;
        m_items = 0;
        m_count = 0;
        return;
    }
    if (count == m_count) {
        return;
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    GmRumbleEffect* items = new GmRumbleEffect[count];
    AtPopHeap();
    if (items == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    if (m_items != 0) {
        const unsigned int keep = count < m_count ? count : m_count;
        for (unsigned int i = 0; i < keep; i++) {
            items[i] = m_items[i];
        }
        delete[] m_items;
    }
    m_items = items;
    m_count = count;
}

GmRumbleEffects::GmRumbleEffects() {
    m_effects.m_items = 0;
    m_effects.m_count = 0;
    m_loaded = 0;
}

GmRumbleEffects::~GmRumbleEffects() {
    m_byName.Clear();
    delete[] m_effects.m_items;
    m_effects.m_items = 0;
    m_effects.m_count = 0;
    m_loaded = 0;
}

GmRumbleEffect* GmRumbleEffects::Find(const char* name) {
    return (GmRumbleEffect*)m_byName.Find(name);
}

void GmRumbleEffects::ReadFrame(AtIoTokenizer* t, GmRumbleFrame* frame) {
    if (t->Next() != kKeyFrame) {
        return;
    }
    frame->m_time = t->ExpectReal();
    t->ExpectLeftCurly();
    if (t->Next() != kLeft) {
        return;
    }
    frame->m_left = t->ExpectReal();
    if (t->Next() != kRight) {
        return;
    }
    frame->m_right = t->ExpectReal();
    t->ExpectRightCurly();
}

void GmRumbleEffects::ReadEffect(AtIoTokenizer* t, GmRumbleEffect* effect) {
    if (t->Next() != kRumbleEffect) {
        return;
    }
    char name[8];
    t->ExpectName(name);
    t->ExpectLeftCurly();
    if (t->Next() != kKeyFrames) {
        return;
    }
    const unsigned int count = (unsigned int)t->ExpectCount();
    effect->Init(name, count);
    for (unsigned int i = 0; i < count; i++) {
        GmRumbleFrame frame;
        ReadFrame(t, &frame);
        effect->SetFrame(i, frame);
    }
    t->ExpectRightCurly();
    t->ExpectRightCurly();
}

void GmRumbleEffects::ReadEffects(AtIoTokenizer* t) {
    const unsigned int count = (unsigned int)t->ExpectCount();
    m_effects.Resize(count);
    m_byName.Init(count);
    for (unsigned int i = 0; i < count; i++) {
        ReadEffect(t, &m_effects.m_items[i]);
        m_byName.Insert(m_effects.m_items[i].Name(), &m_effects.m_items[i]);
    }
    t->ExpectRightCurly();
}

void GmRumbleEffects::Parse(AtIoTokenizer* t) {
    for (int token = t->Next(); token != 0; token = t->Next()) {
        if (token == kRumbleEffects) {
            ReadEffects(t);
        }
    }
}

void GmRumbleEffects::LoadFile(const char* path) {
    AtIoTokenFile file;
    if (file.Open(path, "RMB", 2, 0) == 0) {
        AtIoTokenizer* t = file.m_tokenizer;
        if (t != 0) {
            t->SetKeywords(kKeywords, sizeof kKeywords / sizeof kKeywords[0], 0);
            Parse(t);
        }
    }
    file.Close();
}

void GmRumbleEffects::Load(const char* path) {
    if (m_loaded != 0) {
        m_byName.Clear();
        m_effects.Resize(0);
        m_loaded = 0;
    }
    LoadFile(path);
    m_loaded = 1;
}
