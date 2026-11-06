// Host stand-ins for what is not recovered, or is the platform's: the game's fatal error routine (0x00011070), its calls to the Xbox's memory and device services, and the C library's free.
// Not recovered code.
#include <chrono>
#include <cstdio>
#include <cstdlib>

void AtFatal(const char* message, int file, int line, int) {
    std::fprintf(stderr, "fatal: %s (%d, %d)\n", message, file, line);
    std::abort();
}

void* AtPlatformAllocateRegion(unsigned int size) { return std::malloc(size); }
void AtPlatformFreeRegion(void* region) { std::free(region); }
void AtPlatformInitDevices() {}
void AtLibraryFree(void* data) { std::free(data); }

// What a port reports in place of the Xbox's language setting and game region: English, North America.
int AtPlatformLanguage() { return 1; }
int AtPlatformGameRegion() { return 1; }

// Stands in for 0x00020640, which reads the Xbox performance counter.
unsigned int AtPlatformMilliseconds() {
    using namespace std::chrono;
    return (unsigned int)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
