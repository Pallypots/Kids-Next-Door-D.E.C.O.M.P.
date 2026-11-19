// The SDK's GlobalMemoryStatus (0x00085451, XAPI), where nothing answers it: it stops with a diagnostic. In a file of its own so that a port's answer
// (platform/host_sdk.cpp) replaces it, and a harness can keep the stop or supply a double. Not recovered code.
#include "gm/GmApp.h"

void AtSdkGlobalMemoryStatus(AtSdkMemoryStatus*) {
    AtFatal("App start-up: GlobalMemoryStatus (0x00085451, SDK) is not in this build", 0, 0, 0);
}
