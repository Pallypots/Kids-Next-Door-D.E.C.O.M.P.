// See include/at/AtControllerManager.h. The controller manager's update and the rumble players, as the fatal error path reaches them.
// Verification status per function: config/functions.d/fatal.toml.
#include "at/AtControllerManager.h"

AtControllerManager* AtControllerManager::s_instance;
int AtControllerManager::s_ready;

namespace {

const float kMillisecondsToSeconds = 0.001f;  // 0x002779DC
const int kMotorMilliseconds = 5000;          // what each motor is set for

// Slot 15 takes the intensity as a float in an int's place: the slot's declaration is other work's (include/at/AtInputController.h).
int FloatBits(float value) {
    union {
        float f;
        int i;
    } bits;
    bits.f = value;
    return bits.i;
}

}  // namespace

int AtInputControllerSlot::Update(int milliseconds) {
    if (m_physical != 0) {
        return m_controller->Tick(milliseconds);
    }
    m_controller->m_queue.Flush(0xFF);
    return 0;
}

void AtRumblePlayer::StopEffects() {
    for (unsigned int i = 0; i < m_count; i++) {
        m_playing[i]->Stop();
    }
}

void AtRumblePlayer::Update(float seconds) {
    if (m_enabled == 0) {
        return;
    }
    if (m_paused == 0) {
        for (unsigned int i = 0; i < m_count; i++) {
            m_playing[i]->Advance(seconds);
        }
    }
    // The device is asked for the controller afresh each time it is used.
    unsigned int motors = (unsigned int)m_manager->m_device->Physical(m_port)->m_unknown10;
    if (motors > 2) {
        motors = 2;
    }
    for (unsigned int motor = 0; motor < motors; motor++) {
        float total = 0.0f;
        for (int i = (int)m_count - 1; i >= 0; i--) {
            GmRumblePlayback* playback = m_playing[i];
            total = (float)((double)playback->Intensity((int)motor) + total);
            if (motors == 1) {  // one motor carries both sides
                total = (float)((double)playback->Intensity(1) + total);
            }
        }
        if (total > 0.0f && m_paused == 0 && m_motorsOn != 0) {
            m_manager->m_device->Physical(m_port)->Slot15((int)motor, FloatBits(total), kMotorMilliseconds);
        } else {
            m_manager->m_device->Physical(m_port)->Slot16((int)motor);
        }
    }
    for (int i = (int)m_count - 1; i >= 0; i--) {
        if (m_playing[i]->Finished() != 0) {
            m_playing[i]->Clear();
            m_count--;
            GmRumblePlayback* done = m_playing[i];
            if (i < kPoolSize - 1) {
                // ORIGINAL: the whole rest of the pool moves down, past the count, and the finished one goes to the last place.
                for (int k = i; k < kPoolSize - 1; k++) {
                    m_playing[k] = m_playing[k + 1];
                }
            }
            m_playing[kPoolSize - 1] = done;
        }
    }
}

int AtControllerManager::Update(int milliseconds, int force) {
    if (m_suspended != 0) {
        if (force == 0) {
            return 5;
        }
        if (m_resumeContext != 0) {
            m_resume(m_resumeContext);
        }
    }
    const int result = m_device->Update(milliseconds);
    if (m_slots != 0) {
        for (unsigned int i = 0; i < m_slotCount; i++) {
            if (m_slots[i] != 0) {
                m_slots[i]->Update(milliseconds);
            }
        }
    }
    const float seconds = (float)((double)milliseconds * kMillisecondsToSeconds);
    for (unsigned int i = 0; i < m_playerCount; i++) {
        m_players[i].Update(seconds);
    }
    return result;
}
