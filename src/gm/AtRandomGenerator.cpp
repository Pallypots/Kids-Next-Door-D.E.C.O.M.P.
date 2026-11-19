// The game's random numbers, a Mersenne Twister: see include/at/AtRandomGenerator.h. Names are inferred.
#include "at/AtRandomGenerator.h"

namespace {
// One step of the twist: the top bit of `a` and the low 31 of `b`, shifted, and the matrix when b is odd. The original takes the low bit from `a` too, which the shift drops.
inline unsigned int Mix(unsigned int a, unsigned int b) {
    return ((((a ^ b) & 0x7FFFFFFEu) ^ a) >> 1) ^ ((b & 1) != 0 ? 0x9908B0DFu : 0u);
}
}  // namespace

AtRandomGenerator::AtRandomGenerator() {
    m_index = kWords + 1;
    m_seed = 0;
}

AtRandomGenerator::~AtRandomGenerator() {
    m_index = kWords + 1;
    m_seed = 0;
}

void AtRandomGenerator::Seed(unsigned int seed) {
    m_seed = seed;
    m_state[0] = seed;
    for (m_index = 1; m_index < kWords; m_index++) {
        m_state[m_index] = m_state[m_index - 1] * 69069u;
    }
    Reload();
}

void AtRandomGenerator::Reload() {
    int i = 0;
    for (; i < kWords - 397; i++) {
        m_state[i] = Mix(m_state[i], m_state[i + 1]) ^ m_state[i + 397];
    }
    for (; i < kWords - 1; i++) {
        m_state[i] = Mix(m_state[i], m_state[i + 1]) ^ m_state[i - (kWords - 397)];
    }
    m_index = 0;
    m_state[kWords - 1] = Mix(m_state[kWords - 1], m_state[0]) ^ m_state[396];
}

// The original writes the masks pre-shifted: (y & 0xFF3A58AD) << 7 and (y & 0xFFFFDF8C) << 15, which are these.
unsigned int AtRandomGenerator::Next() {
    if (m_index >= kWords) {
        Reload();
    }
    unsigned int y = m_state[m_index];
    m_index++;
    y ^= y >> 11;
    y ^= (y << 7) & 0x9D2C5680u;
    y ^= (y << 15) & 0xEFC60000u;
    y ^= y >> 18;
    return y;
}

unsigned int AtRandomGenerator::Below(unsigned int n) {
    if (n == 0) {
        return 0;
    }
    return Next() % n;
}

double AtRandomGenerator::Unit() {
    const int step = (int)(Next() % 10000);
    return (double)step * (double)0.0001f;
}

int AtRandomGenerator::SeedDefault() {
    Seed(kDefaultSeed);
    return 0;
}

unsigned int AtRandomGenerator::ResetSeed() {
    m_seed = kDefaultSeed;
    Seed(kDefaultSeed);
    return m_seed;
}
