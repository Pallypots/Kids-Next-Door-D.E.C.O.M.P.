// Stand-ins for three heap setters the graphics layer calls that are recovered on another branch (integrate-batch6, at/imgfont and at/matdev):
// AtGrI2DFont::SetHeap 0x000FE870, AtMatrixDeviceXbox::SetHeap 0x000F7BD0 and AtMatrixPoolXbox::SetHeap 0x000F5FC0. Same effect, not recorded and not
// counted here. At integration, src/at/AtGraphicsLayer.cpp should call those and this file be deleted. See include/at/AtGraphicsLayer.h.
#include "at/AtGraphicsLayer.h"

void* g_heap3B1FCC;
void* g_heap3B1F94;
void* g_heap3B1F58;

void AtSetHeap3B1FCC(void* heap) { g_heap3B1FCC = heap; }
void AtSetHeap3B1F94(void* heap) { g_heap3B1F94 = heap; }
void AtSetHeap3B1F58(void* heap) { g_heap3B1F58 = heap; }
