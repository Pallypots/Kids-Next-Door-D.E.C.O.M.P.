// The I/O layer factory's three platform slots, for a host: the device file system is platform/host_file_system.cpp's, over one host directory, for both
// kinds the mount manager asks for. The Xbox's I/O device has no host counterpart yet: one is made, so that the context holds a device as on the Xbox,
// and each of its slots stops with a diagnostic. Port code; see platform/host_io.h. Replaces src/at/AtIoSystemDevices.cpp in a program that links it.
#include "host_io.h"

#include <string>

#include "at/AtApLayer.h"
#include "host_file_system.h"

namespace {
std::string& Root() {
    static std::string root = ".";
    return root;
}

class HostIoDevice : public AtIoContextDevice {
public:
    explicit HostIoDevice(int requests) : m_requests(requests) {}
    void Unknown0() override { AtFatal("host I/O device: slot 0 has no host counterpart", m_requests, 0, 0); }
    int Unknown1() override { AtFatal("host I/O device: slot 1 has no host counterpart", m_requests, 0, 0); return 0; }
    void* Unknown2(int a, int b) override { AtFatal("host I/O device: slot 2 has no host counterpart", a, b, 0); return 0; }
    int m_requests;
};
}

void AtHostIoSetRoot(const char* directory) { Root() = directory; }

AtIoContextDevice* AtIoSystemFactory::CreateDevice(int requests) { return new HostIoDevice(requests); }
void AtIoSystemFactory::DestroyDevice(AtIoContextDevice* device) { delete static_cast<HostIoDevice*>(device); }
AtIoFileSystem* AtIoSystemFactory::Create(int, int, int) { return new AtIoHostFileSystem(Root()); }
