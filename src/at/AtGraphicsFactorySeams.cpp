// What the graphics factory's start reaches and at/grfactory does not recover: six of the render table's seven functions and two slots of the adapter
// list. Each stops with a diagnostic; none is recorded or counted. In a file of its own, so that a harness can supply them instead.
#include "at/AtGraphicsFactory.h"

void AtFatal(const char* message, int file, int line, int unused);  // 0x00011070

void AtRenderFunction0() { AtFatal("Render table: its function 0 (0x00117130) is not recovered", 0, 0, 0); }
void AtRenderFunction1() { AtFatal("Render table: its function 1 (0x001171D0) is not recovered", 0, 0, 0); }
void AtRenderFunction3() { AtFatal("Render table: its function 3 (0x00117250) is not recovered", 0, 0, 0); }
void AtRenderFunction4() { AtFatal("Render table: its function 4 (0x00117380) is not recovered", 0, 0, 0); }
void AtRenderFunction5() { AtFatal("Render table: its function 5 (0x00117470) is not recovered", 0, 0, 0); }
void AtRenderFunction6() { AtFatal("Render table: its function 6 (0x001175A0) is not recovered", 0, 0, 0); }

AtGfxAdapter* AtGfxAdapterList::Best() {
    AtFatal("Graphics factory: choosing an adapter (0x000FA600) is not recovered", 0, 0, 0);
    return 0;
}
AtGfxAdapter* AtGfxAdapterList::Find(const AtD3DAdapterIdentifier*) {
    AtFatal("Graphics factory: finding an adapter (0x000FA700) is not recovered", 0, 0, 0);
    return 0;
}
