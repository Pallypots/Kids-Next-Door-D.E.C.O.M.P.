// Host stand-ins for the game's error routines and heap stack (0x00011070, 0x00011170, 0x00011240, 0x00011260, none recovered):
// report and stop; a host build allocates from the normal heap. Not recovered code.
#include <chrono>
#include <cstdio>
#include <cstdlib>

void AtFatal(const char* message, int a, int b, int c) {
    std::fprintf(stderr, "fatal: %s(%d, %d, %d)\n", message, a, b, c);
    std::abort();
}

void AtRaise(int code, int a, int b, int c) {
    std::fprintf(stderr, "fatal: error %d (%d, %d, %d)\n", code, a, b, c);
    std::abort();
}

void AtPushHeap(void*) {}
void AtPopHeap() {}

// What a port reports in place of the Xbox's language setting and game region: English, North America.
int AtPlatformLanguage() { return 1; }
int AtPlatformGameRegion() { return 1; }

// Stands in for 0x00020640, which reads the Xbox performance counter.
unsigned int AtPlatformMilliseconds() {
    using namespace std::chrono;
    return (unsigned int)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
