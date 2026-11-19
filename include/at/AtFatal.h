// The game's fatal error path: the report of a failed file (0x00014230, 0x000141C0) and AtFatal (0x00011070), which stops the rumble, shows the error screen and leaves for the
// dashboard. All names are inferred.
//
// Game and port, and how they meet. src/at/AtFatal.cpp is the game's AtFatal, and is alone in its file. It ends in three things that are not the game's, declared below and
// supplied by the port: the XAPI's image launch, the C library's printf and its exit. platform/fatal.cpp supplies them for a host: there is no dashboard to launch, so that
// returns, and the game then prints its message and exits through the host's stop. platform/fatal.cpp also keeps a stand-in AtFatal for the programs that do not link the game's
// (every test and most oracles, which have no App to draw with); a program that links src/at/AtFatal.cpp's defines KND_GAME_FATAL when it compiles platform/fatal.cpp.
#pragma once

void AtFatal(const char* message, int file, int line, int unused);  // 0x00011070; file is in truth a file name, printed with %s: every recovered caller passes 0

extern int g_atFatalScreenShown;  // 0x002C6060 set before the error screen, so a fatal error inside it goes straight on to leaving
extern int g_atFatalExiting;      // 0x002C605C set before exit; nothing here reads it

// What the XAPI's XLaunchNewImage takes: LAUNCH_DATA, 3072 bytes. AtFatal sets words 0 and 2 (1 and 5) and clears the rest; what they mean to the dashboard is not established here.
struct AtLaunchData {
    unsigned int m_words[0x300];
};

// ---- the port's: not the game's code, not recovered, not counted -----------------------------------------------------
void AtPlatformLaunchImage(const char* path, AtLaunchData* data);  // 0x00085197 XAPI's XLaunchNewImage (library: see docs/worklog/2026-11-07-anaf.md). On a host it returns.
int AtLibraryPrint(const char* format, ...);  // 0x001254F3 the C library's printf
void AtLibraryExit(int code);                 // 0x001254DB the C library's exit; does not return

// ---- the report of a failed file --------------------------------------------------------------------------------------
// Variadic arguments without the C library's headers (src/ is compiled with none). The same type as the host's va_list, so that src/at/AtReportHost.cpp can pass it on.
#if defined(__GNUC__) || defined(__clang__)
typedef __builtin_va_list AtVaList;
#define AT_VA_START(list, last) __builtin_va_start(list, last)
#define AT_VA_END(list) __builtin_va_end(list)
#elif defined(_M_IX86)
typedef char* AtVaList;
#define AT_VA_START(list, last) ((void)((list) = (char*)&(last) + ((sizeof(last) + 3) & ~3)))
#define AT_VA_END(list) ((void)((list) = 0))
#elif defined(_M_X64)
typedef char* AtVaList;
extern "C" void __cdecl __va_start(AtVaList*, ...);
#define AT_VA_START(list, last) ((void)(__va_start(&(list), last)))
#define AT_VA_END(list) ((void)((list) = 0))
#else
#error "AtVaList: no variadic arguments for this compiler"
#endif

void AtReport(int status, const char* format, ...);                 // 0x00014230
void AtReportV(int status, const char* format, AtVaList arguments);  // 0x000141C0 "<status text>: <message>", then AtFatal; a null format is "<No Message>"
int AtFormatV(char* out, const char* format, AtVaList arguments);   // 0x00125ECE the C library's vsprintf: src/at/AtReportHost.cpp
