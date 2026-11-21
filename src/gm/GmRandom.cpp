// The game's Mersenne twister. See include/gm/GmRandom.h. Verification status per function: config/functions.d/gmsound.toml.
#include "gm/GmRandom.h"

namespace {

const int kMiddle = 397;
const unsigned int kMatrix = 0x9908B0DF;
const unsigned int kUpper = 0x80000000;
const unsigned int kLower = 0x7FFFFFFF;

// The twist of one word from two neighbours and the one kMiddle on. The original masks the xor of the two, 0x7FFFFFFE, which keeps the same bits once shifted.
unsigned int Twist(unsigned int word, unsigned int next, unsigned int far) {
    const unsigned int mixed = (word & kUpper) | (next & kLower);
    return far ^ (mixed >> 1) ^ ((next & 1) != 0 ? kMatrix : 0);
}

}  // namespace

GmRandom::GmRandom() {
    m_index = kCount + 1;
    m_seed = 0;
}

void GmRandom::Regenerate() {
    int i = 0;
    for (; i < kCount - kMiddle; i++) {
        m_state[i] = Twist(m_state[i], m_state[i + 1], m_state[i + kMiddle]);
    }
    for (; i < kCount - 1; i++) {
        m_state[i] = Twist(m_state[i], m_state[i + 1], m_state[i + kMiddle - kCount]);
    }
    m_index = 0;
    m_state[kCount - 1] = Twist(m_state[kCount - 1], m_state[0], m_state[kMiddle - 1]);
}

void GmRandom::Seed(unsigned int seed) {
    m_seed = seed;
    m_state[0] = seed;
    for (m_index = 1; m_index < kCount; m_index++) {
        m_state[m_index] = m_state[m_index - 1] * 69069;
    }
    Regenerate();
}

unsigned int GmRandom::Next() {
    if (m_index >= kCount) {
        Regenerate();
    }
    unsigned int y = m_state[m_index++];
    y ^= y >> 11;
    y ^= (y << 7) & 0x9D2C5680;
    y ^= (y << 15) & 0xEFC60000;
    return y ^ (y >> 18);
}

unsigned int GmRandom::NextBelow(unsigned int bound) {
    if (bound == 0) {
        return 0;
    }
    return Next() % bound;
}

double GmRandom::NextUnit() {
    const int step = (int)(Next() % 10000);
    return (double)step * (double)0.0001f;
}

int GmRandom::SeedDefault() {
    Seed(kDefaultSeed);
    return 0;
}

unsigned int GmRandom::SeedDefaultReturningSeed() {
    m_seed = kDefaultSeed;
    Seed(kDefaultSeed);
    return m_seed;
}
