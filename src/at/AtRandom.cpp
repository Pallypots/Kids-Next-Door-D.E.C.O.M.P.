// See include/at/AtRandom.h.
#include "at/AtRandom.h"

unsigned int g_atRandomCursor;

namespace {

const unsigned int kCount = 0x800;
const unsigned int kMask = kCount - 1;
const unsigned int kState = 0x0332F1A2;      // The generator's state behind the first word. Only the low 27 bits show.
const unsigned int kMultiplier = 214013;     // The Microsoft C library's rand().
const unsigned int kIncrement = 2531011;

unsigned short s_words[kCount];
bool s_ready;

void Build() {
    unsigned int state = kState;
    for (unsigned int i = 0; i < kCount; i++) {
        s_words[i] = (unsigned short)((state >> 16) & kMask);
        state = state * kMultiplier + kIncrement;
    }
    s_ready = true;
}

}  // namespace

unsigned short AtRandomWord(unsigned int index) {
    if (!s_ready) {
        Build();
    }
    return s_words[index];
}

unsigned short AtNextRandomWord() {
    g_atRandomCursor = (g_atRandomCursor + 1) & kMask;
    return AtRandomWord(g_atRandomCursor);
}
