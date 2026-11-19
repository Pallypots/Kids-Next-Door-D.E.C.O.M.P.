// The game's random numbers: a Mersenne Twister (MT19937), 0x9C8 bytes. Original address cluster 0x00071BB0-0x00071DBA. The class name and every member name are inferred (include/at/AtRandom.h is a different thing: the table of random words the particles draw from): the twist constant
// 0x9908B0DF, the 624 words, the tempering masks and the 69069 seeding identify the generator; nothing in the executable names it.
//
// One is made by the object at 0x002CB0A8 (GmHistoryLogs, include/gm/GmAppSettings.h; that name is a guess from two of its setters, and the object holds more), kept at its +0x10, seeded with
// 4357 twice (0x00071D90, then 0x00071DA0, whose answer goes to 0x00013DF0) and destroyed by its shutdown 0x0006C310. About forty functions draw from it, among them the AI action manager
// (include/gm/GmAiActionManager.h). Its source is in src/gm/AtRandomGenerator.cpp so that the AI action manager's mutant runs, which copy one source directory, reach it; it belongs with the engine's.
#pragma once

class AtRandomGenerator {
public:
    enum { kWords = 624, kDefaultSeed = 4357 };

    // 0x00071BB0, one body for both: the index past the end (625) and the seed 0. Nothing is seeded, so a draw before Seed twists 624 words the heap left there.
    AtRandomGenerator();
    ~AtRandomGenerator();

    void Seed(unsigned int seed);       // 0x00071CA0 the old Knuth seeding, each word 69069 times the one before; then a twist. The loop runs on m_index itself
    void Reload();                      // 0x00071BD0 the twist, over all 624 words; m_index back to 0
    unsigned int Next();                // 0x00071D00 the next word, tempered; a twist first when m_index has reached 624 (a signed comparison)
    unsigned int Below(unsigned int n); // 0x00071D50 Next() % n, or 0 for n of 0 without drawing
    // 0x00071D70: (Next() % 10000) * 0.0001f, in steps of 1/10000 below 1. The product stays in the x87 register, not rounded to a float: double here. The AI action manager has the same code inlined.
    double Unit();
    int SeedDefault();                  // 0x00071D90 Seed(4357); answers 0
    unsigned int ResetSeed();           // 0x00071DA0 the seed 4357, then Seed(4357); answers the seed

    unsigned int m_state[kWords];       // +0x000
    int m_index;                        // +0x9C0 the next word to temper
    unsigned int m_seed;                // +0x9C4 the last seed
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtRandomGenerator, m_index) == 0x9C0, "AtRandomGenerator::m_index");
static_assert(sizeof(AtRandomGenerator) == 0x9C8, "AtRandomGenerator");
#pragma pop_macro("offsetof")
#endif
