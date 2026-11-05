// Host stand-in for the game's error routine (0x00011070, not recovered): report and stop. Not recovered code.
#include <cstdio>
#include <cstdlib>

void AtFatal(const char* message, int a, int b, int c) {
    std::fprintf(stderr, "fatal: %s(%d, %d, %d)\n", message, a, b, c);
    std::abort();
}
