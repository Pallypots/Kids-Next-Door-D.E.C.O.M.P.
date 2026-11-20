// The SDK's Direct3D entry points the graphics factory's start reaches (at/grfactory, include/at/AtGraphicsFactory.h). Not game code: the Xbox SDK's
// library, and hardware. Where nothing answers them they stop with a diagnostic. In a file of their own so that a program can answer them instead:
// the oracle and the host tests with doubles, a port with platform/host_d3d.cpp. Not recovered, not recorded, not counted.
#include "at/AtGraphicsFactory.h"

void AtFatal(const char* message, int file, int line, int unused);  // 0x00011070

void* AtD3DCreate(unsigned int) {
    AtFatal("Graphics factory: Direct3DCreate8 (0x00206070, SDK) is not in this build", 0, 0, 0);
    return 0;
}
int AtD3DGetAdapterIdentifier(unsigned int, unsigned int, AtD3DAdapterIdentifier*) {
    AtFatal("Graphics factory: Direct3D's GetAdapterIdentifier (0x00206080, SDK) is not in this build", 0, 0, 0);
    return -1;
}
int AtD3DGetDeviceCaps(unsigned int, unsigned int, AtD3DCaps*) {
    AtFatal("Graphics factory: Direct3D's GetDeviceCaps (0x002062B0, SDK) is not in this build", 0, 0, 0);
    return -1;
}
unsigned int AtD3DGetAdapterModeCount(unsigned int) {
    AtFatal("Graphics factory: Direct3D's GetAdapterModeCount (0x002060B0, SDK) is not in this build", 0, 0, 0);
    return 0;
}
int AtD3DEnumAdapterModes(unsigned int, unsigned int, AtD3DDisplayMode*) {
    AtFatal("Graphics factory: Direct3D's EnumAdapterModes (0x00206150, SDK) is not in this build", 0, 0, 0);
    return -1;
}
int AtD3DCheckDeviceType(unsigned int, unsigned int, unsigned int, unsigned int, int) {
    AtFatal("Graphics factory: Direct3D's CheckDeviceType (0x002063F0, SDK) is not in this build", 0, 0, 0);
    return -1;
}
