// Host stand-ins for what is not recovered, or is the platform's: the end of the game's fatal error path (the XAPI's image launch, the C library's printf and exit), a stand-in
// for that path itself, the Xbox's memory and device services, and the C library's free.
// Not recovered code.
//
// The game's fatal error routine, 0x00011070, is recovered in src/at/AtFatal.cpp and ends in AtPlatformLaunchImage, AtLibraryPrint and AtLibraryExit below: on a host there is
// no dashboard, so the launch returns, the game prints its message to stderr, and exit stops the process here. That is the host's way to report and stop.
// A program that does not link the game's routine (every test, which has no App to draw the error screen with) gets the stand-in AtFatal below instead, which reports and stops
// the same way at once. A program that links src/at/AtFatal.cpp defines KND_GAME_FATAL, so that the two do not collide.
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

struct AtLaunchData;

#ifndef KND_GAME_FATAL
void AtFatal(const char* message, int file, int line, int) {
    std::fprintf(stderr, "fatal: %s (%d, %d)\n", message, file, line);
    std::abort();
}
#endif

// 0x00085197, XLaunchNewImage. Nothing to launch on a host.
void AtPlatformLaunchImage(const char*, AtLaunchData*) {}

// 0x001254F3, printf. To stderr, where the stand-in reports too.
int AtLibraryPrint(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int length = std::vfprintf(stderr, format, arguments);
    va_end(arguments);
    return length;
}

// 0x001254DB, exit. The game asks for 1; the host stops as the stand-in does.
void AtLibraryExit(int) {
    std::fflush(stderr);
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
