// See include/gm/GmRumble.h. An effect playing on a controller, and the two accessors of an effect it uses. Original address cluster 0x00073E00-0x00073F26, with 0x00074930,
// 0x000ED690 and 0x000B9A90. Verification status per function: config/functions.d/fatal.toml.
#include "gm/GmRumble.h"

GmRumbleSystem* GmRumbleSystem::s_instance;

float GmRumbleEffect::Duration() {
    return m_frames.m_items[m_frames.m_count - 1].m_time;
}

unsigned int GmRumbleEffect::FrameCount() {
    return m_frames.m_count;
}

void GmRumblePlayback::Stop() {
    m_loop = 0;
    m_time = m_effect->Duration();
}

void GmRumblePlayback::Advance(float seconds) {
    m_time = (float)((double)seconds + m_time);
    if (m_loop != 0 && m_effect->Duration() < m_time) {
        m_time = (float)((double)m_time - m_effect->Duration());
    }
}

float GmRumblePlayback::Intensity(int motor) {
    unsigned int found = 0;
    for (unsigned int i = 0; i < m_effect->FrameCount(); i++) {
        if (m_effect->Frame(i)->m_time < m_time) {
            found = i;
        }
    }
    if (found + 1 >= m_effect->FrameCount()) {
        GmRumbleFrame* frame = m_effect->Frame(found);
        return motor == 1 ? frame->m_right : frame->m_left;
    }
    GmRumbleFrame* frame = m_effect->Frame(found);
    m_effect->Frame(found + 1);  // ORIGINAL: the next frame is fetched and not used: no blending between frames
    return motor == 1 ? frame->m_right : frame->m_left;
}

int GmRumblePlayback::Finished() {
    if (m_loop == 0 && m_effect->Duration() <= m_time) {
        return 1;
    }
    return 0;
}

void GmRumblePlayback::Clear() {
    m_unknown00 = 0;
    m_effect = 0;
    m_time = 0.0f;
    m_loop = 0;
}
