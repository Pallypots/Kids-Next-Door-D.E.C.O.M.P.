// The game's random numbers: a Mersenne twister, MT19937, reached through GmHistoryLogs::s_instance at +0x10 by the sound code and many others. Not polymorphic: the state is at +0.
// Original address cluster 0x00071BB0-0x00071DBA. Names inferred.
#pragma once

struct GmRandom {
    enum { kCount = 624, kDefaultSeed = 4357 };

    GmRandom();                      // 0x00071BB0 unseeded: the index past the end, so the first Next regenerates from whatever the state holds
    void Regenerate();               // 0x00071BD0 the index back to 0
    void Seed(unsigned int seed);    // 0x00071CA0 Knuth's 69069, then a regeneration at once
    unsigned int Next();             // 0x00071D00 tempered; regenerates first if the index has reached the end. Unlike the reference code, it never seeds by itself
    unsigned int NextBelow(unsigned int bound);  // 0x00071D50 0 for a bound of 0
    float NextUnit();                // 0x00071D70 (Next() % 10000) * 0.0001: steps of a ten-thousandth, below 1
    int SeedDefault();               // 0x00071D90 seeds with 4357 and returns 0
    unsigned int SeedDefaultReturningSeed();  // 0x00071DA0 the same, returning the seed

    unsigned int m_state[kCount];    // +0x000
    int m_index;                     // +0x9C0
    unsigned int m_seed;             // +0x9C4
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
static_assert(sizeof(GmRandom) == 0x9C8, "GmRandom");
#endif
