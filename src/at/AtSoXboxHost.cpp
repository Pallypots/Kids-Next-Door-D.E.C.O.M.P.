// What stands between the sound classes and XACT and the Xbox's sound library, neither of which is in scope. Each stops with a diagnostic. In a file of its own, so that a test can supply them instead. Not recovered code.
#include "at/AtAudioDatabase.h"

void AtFatal(const char* message, int a, int b, int c);

int AtSoXboxXactBankLoad(AtSoXboxXactBank*) {
    AtFatal("Atlas Sound: AtSoXboxXactBank's load is not recovered: it opens the .WBA file and reaches XACT", 0, 0, 0);
    return 3;
}

void AtSoXboxXactBankProcess(AtSoXboxXactBank*) {
    AtFatal("Atlas Sound: AtSoXboxXactBank's load is not recovered: it opens the .WBA file and reaches XACT", 0, 0, 0);
}

void AtSoXboxXactBankUpdate(AtSoXboxXactBank*) {
    AtFatal("Atlas Sound: AtSoXboxXactBank's unload is not recovered: it reaches XACT", 0, 0, 0);
}

int AtSoXboxCreateStream(void*, int, void**) {
    AtFatal("Atlas Sound: no sound library on this platform", 0, 0, 0);
    return -1;
}

int AtSoXboxSetMixBins(void*, const AtSoXboxMixBins*) {
    AtFatal("Atlas Sound: no sound library on this platform", 0, 0, 0);
    return -1;
}

void AtSoXboxStopStream(void*) {
    AtFatal("Atlas Sound: no sound library on this platform", 0, 0, 0);
}

void AtSoXboxGetStreamStatus(void*, unsigned int* status) {
    AtFatal("Atlas Sound: no sound library on this platform", 0, 0, 0);
    *status = 0;
}

void AtSoXboxReleaseStream(void*) {
    AtFatal("Atlas Sound: no sound library on this platform", 0, 0, 0);
}
