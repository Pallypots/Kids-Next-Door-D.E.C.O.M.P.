// The controller manager, as far as the game's fatal error path reaches it: its update, which ticks the device, the controllers and the rumble, and the rumble players that
// stop. Original address clusters 0x0006CE30-0x0006CEDF, 0x0006D7E0-0x0006D9BD and 0x00072610-0x00072633. "Controller manager" is the original's word, from its messages at
// 0x0006D0E0 ("Unable to release failed controller manager"); every other name here is inferred.
//
// The manager's constructor (0x0006CA50), its vftable (0x002779B0, no RTTI), its Init (0x0006D0E0) and everything else of it are not recovered: its words are laid out flat, the
// vftable among them, and nothing here makes one. Bodies in src/at/AtControllerManager.cpp.
#pragma once

#include "at/AtInputController.h"
#include "gm/GmRumble.h"

class AtControllerManager;

// One of the manager's controllers, as its update sees it. Its class (near 0x00072574, where receivers are added) is not recovered beyond these words.
class AtInputControllerSlot {
public:
    // 0x00072610. A physical one is ticked, and what that returns comes back; any other has every event of its queue dropped, and 0.
    int Update(int milliseconds);

    int m_unknown00;                    // +0x00
    int m_unknown04;                    // +0x04
    AtAbstractController* m_controller; // +0x08
    int m_unknown0C;                    // +0x0C
    int m_unknown10;                    // +0x10
    int m_physical;                     // +0x14
};

// The rumble of one player's controller: up to ten effects playing at once, from a pool of ten. 0x40 bytes, in an array of the manager. Its reset, 0x0006D9C0, and the start of an
// effect, 0x0006D750, are not recovered.
class AtRumblePlayer {
public:
    enum { kPoolSize = 10 };

    void StopEffects();                 // 0x0006D7E0 each playing effect to its end, not looping; the motors are told on the next Update
    // 0x0006D810. Nothing unless enabled. Advances each effect unless paused, sets each motor (at most two) to the sum of the effects on it, then returns finished effects to
    // the end of the pool.
    void Update(float seconds);

    int m_enabled;                      // +0x00
    unsigned int m_port;                // +0x04 which of the device's physical controllers
    AtControllerManager* m_manager;     // +0x08
    int m_paused;                       // +0x0C
    int m_motorsOn;                     // +0x10 1 after a reset
    GmRumblePlayback* m_playing[kPoolSize];  // +0x14 the first m_count are playing
    unsigned int m_count;               // +0x3C
};

class AtControllerManager {
public:
    typedef void (*Callback)(void* context);

    // 0x0006CE30. While suspended, 5 unless forced, and when forced the resume callback first. Then the device's update, which is what comes back, each controller's, and the
    // rumble with the time in seconds.
    int Update(int milliseconds, int force);

    void* m_vftable;                    // +0x00 0x002779B0
    int m_unknown04[4];                 // +0x04
    void* m_unknown14;                  // +0x14 from App+0x60A8; makes the device
    AtInputDevice* m_device;            // +0x18
    AtInputControllerDatabase* m_controllers;  // +0x1C
    int m_unknown20;                    // +0x20
    struct AtInputOwner* m_owner;       // +0x24 handed to the .ANA load by 0x0006CEE0
    int m_unknown28[2];                 // +0x28
    AtInputControllerSlot** m_slots;    // +0x30
    unsigned int m_slotCount;           // +0x34
    AtInputEventQueue m_queue;          // +0x38
    int m_unknown60[3];                 // +0x60
    AtRumblePlayer* m_players;          // +0x6C
    unsigned int m_playerCount;         // +0x70
    int m_unknown74[4];                 // +0x74 1 each after the constructor
    int m_suspended;                    // +0x84
    // ORIGINAL: called with the context in ecx and nothing on the stack, as a member function; here a plain function given the context. What sets it is not recovered.
    Callback m_resume;                  // +0x88
    void* m_resumeContext;              // +0x8C the callback is called only when this is set

    static AtControllerManager* s_instance;  // 0x002CD0C4 set by the constructor
    static int s_ready;                 // 0x002CD0C8 set by Init; the fatal path stops the rumble only when it is
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtInputControllerSlot, m_physical) == 0x14, "AtInputControllerSlot::m_physical");
static_assert(offsetof(AtRumblePlayer, m_playing) == 0x14, "AtRumblePlayer::m_playing");
static_assert(sizeof(AtRumblePlayer) == 0x40, "AtRumblePlayer");
static_assert(offsetof(AtControllerManager, m_device) == 0x18, "AtControllerManager::m_device");
static_assert(offsetof(AtControllerManager, m_slots) == 0x30, "AtControllerManager::m_slots");
static_assert(offsetof(AtControllerManager, m_queue) == 0x38, "AtControllerManager::m_queue");
static_assert(offsetof(AtControllerManager, m_players) == 0x6C, "AtControllerManager::m_players");
static_assert(offsetof(AtControllerManager, m_suspended) == 0x84, "AtControllerManager::m_suspended");
static_assert(sizeof(AtControllerManager) == 0x90, "AtControllerManager");
#pragma pop_macro("offsetof")
#endif
