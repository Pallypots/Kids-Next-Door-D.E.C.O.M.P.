// XACT's entry points that a voice's play reaches, which are not in scope: each stops with a diagnostic naming the call. In a file of its own, so that a test can supply them instead. Not recovered
// code. See the declarations at the end of include/at/AtAudioDatabase.h.
#include "at/AtAudioDatabase.h"

void AtFatal(const char* message, int a, int b, int c);

int AtSoXboxXactPrepareCue(void*, const AtSoXboxCuePlay*, void**) {
    AtFatal("Atlas Sound: no XACT on this platform: prepare a cue (XACTENG 0x00242189)", 0, 0, 0);
    return -1;
}

int AtSoXboxXactPlayCue(void*, const AtSoXboxCuePlay*, void**) {
    AtFatal("Atlas Sound: no XACT on this platform: play a cue (XACTENG 0x002421BC)", 0, 0, 0);
    return -1;
}

int AtSoXboxXactStopCue(void*, int, unsigned int, void*) {
    AtFatal("Atlas Sound: no XACT on this platform: stop a cue (XACTENG 0x002421EF)", 0, 0, 0);
    return -1;
}

int AtSoXboxXactPauseCue(void*, void*, int) {
    AtFatal("Atlas Sound: no XACT on this platform: pause a cue (XACTENG 0x00242259)", 0, 0, 0);
    return -1;
}

int AtSoXboxXactGetCueProperties(void*, int, AtSoXboxCueProperties*) {
    AtFatal("Atlas Sound: no XACT on this platform: a cue's properties (XACTENG 0x00242226)", 0, 0, 0);
    return -1;
}

int AtSoXboxXactSetVariables(void*, const AtSoXboxVariables*) {
    AtFatal("Atlas Sound: no XACT on this platform: set cue variables (XACTENG 0x00242121)", 0, 0, 0);
    return -1;
}

int AtSoXboxXactSetPosition(void*, float, float, float, int) {
    AtFatal("Atlas Sound: no XACT on this platform: place a sound source (XACTENG 0x00242291)", 0, 0, 0);
    return -1;
}
