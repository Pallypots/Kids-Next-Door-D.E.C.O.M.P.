// The SDK's Direct3D entry points the graphics factory's start reaches (include/at/AtGraphicsFactory.h), answered for a host that has no Xbox Direct3D.
// Port code, never recovered code: a bounded adapter, as platform/host_sdk.cpp answers the memory query. It tells the truth about the host: there is an
// interface and one adapter, which describes itself as the host's, and asking for its device capabilities fails (D3DERR_NOTAVAILABLE), so the factory
// keeps the adapter with no device capabilities and no display modes. No renderer is behind it.
#include "at/AtGraphicsFactory.h"

namespace {
const int kNotAvailable = (int)0x8876086Au;  // D3DERR_NOTAVAILABLE
const int kInvalidCall = (int)0x8876086Cu;   // D3DERR_INVALIDCALL, what the SDK's GetAdapterIdentifier gives for an adapter but 0
char g_interface;                            // what Direct3DCreate8 hands back: only ever compared with 0 and handed on
void CopyName(char* to, const char* from) {
    while ((*to++ = *from++) != 0) {
    }
}
}  // namespace

void* AtD3DCreate(unsigned int) { return &g_interface; }

int AtD3DGetAdapterIdentifier(unsigned int adapter, unsigned int, AtD3DAdapterIdentifier* out) {
    if (adapter != 0) return kInvalidCall;
    unsigned char* bytes = (unsigned char*)out;
    for (unsigned int i = 0; i < sizeof *out; i++) bytes[i] = 0;
    CopyName(out->m_driver, "host");
    CopyName(out->m_description, "Host display (no Xbox Direct3D)");
    return 0;
}

int AtD3DGetDeviceCaps(unsigned int, unsigned int, AtD3DCaps*) { return kNotAvailable; }
unsigned int AtD3DGetAdapterModeCount(unsigned int) { return 0; }
int AtD3DEnumAdapterModes(unsigned int, unsigned int, AtD3DDisplayMode*) { return kNotAvailable; }
int AtD3DCheckDeviceType(unsigned int, unsigned int, unsigned int, unsigned int, int) { return kNotAvailable; }
