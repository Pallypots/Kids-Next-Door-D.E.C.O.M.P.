// The factory's slots that make and free the Xbox's I/O device and file system (0x00102D50, 0x00102E00 and the device's deleting destructor). Not
// recovered: each stops with a diagnostic. A port defines these three in its platform layer instead (platform/host_io.cpp), and then this file is not
// linked. Nothing here counts as recovered.
#include "at/AtApLayer.h"

AtIoContextDevice* AtIoSystemFactory::CreateDevice(int requests) {
    AtFatal("AtIoSystemFactory: the Xbox's I/O device (0x00102D50, 0x0011E4E0) is not recovered", requests, 0, 0);
    return 0;
}

void AtIoSystemFactory::DestroyDevice(AtIoContextDevice* device) {
    AtFatal("AtIoSystemFactory: the Xbox's I/O device (0x000EB9D0) is not recovered", device != 0, 0, 0);
}

AtIoFileSystem* AtIoSystemFactory::Create(int kind, int a, int) {
    AtFatal("AtIoSystemFactory: the Xbox's file system (0x00102E00, 0x001030C0) is not recovered", kind, a, 0);
    return 0;
}
