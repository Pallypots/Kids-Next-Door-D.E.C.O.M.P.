// The heap stack, new and delete, and the error routine. Original address cluster 0x00011170-0x000112A9.
// Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "at/AtMemory.h"

void AtFatal(const char* message, int file, int line, int unused);  // 0x00011070: not recovered, it reaches into graphics and sound. platform/ supplies a diagnostic.

int AtHeapStack::s_top = -1;
AtHeap* AtHeapStack::s_heaps[16];
AtHeap* AtHeapStack::s_contentHeap;

// 0x00011170. The fourth argument is not read.
void AtRaise(int code, int file, int line, int unused) {
    (void)unused;
    const char* message;
    switch (code) {
    case 0: message = "Invalid error condition"; break;
    case -1: message = "General error occurred"; break;
    case -2: message = "Invalid parameter detected"; break;
    case -3: message = "Invalid value detected"; break;
    case -4: message = "Out of memory error occured"; break;
    case -5: message = "Critical resource busy"; break;
    case -7: message = "Assertion Failed"; break;
    default: message = "Unknown error condition occured"; break;
    }
    AtFatal(message, file, line, 0);
}

void* AtHeapNew(unsigned int size) {
    return AtHeapManager::s_instance->Allocate(AtHeapStack::s_heaps[AtHeapStack::s_top], size);
}

void AtHeapDelete(void* data) {
    if (data != 0 && AtHeapManager::s_instance->Free(data) == 0) {
        AtLibraryFree(data);
    }
}

void AtPushHeap(void* heap) {
    if (++AtHeapStack::s_top < 16) {
        AtHeapStack::s_heaps[AtHeapStack::s_top] = (AtHeap*)heap;
    }
}

void AtPopHeap() {
    AtHeapStack::s_heaps[AtHeapStack::s_top] = 0;
    AtHeapStack::s_top--;
}

AtHeap* AtCurrentHeap() {
    if (AtHeapStack::s_top >= 0) {
        return AtHeapStack::s_heaps[AtHeapStack::s_top];
    }
    return 0;
}

void AtSetContentHeap(AtHeap* heap) {
    AtHeapStack::s_contentHeap = heap;
}
