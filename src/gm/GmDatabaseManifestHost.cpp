// What the base's items reach and is not recovered: the loads of scene, cinema and event databases, the registry manager's Add, and the space graph's setter. Each stops with a diagnostic.
// In a file of its own, so that a test can supply them instead. Not recovered code. Recovered code is in src/gm/GmDatabaseManifest.cpp.
// The sound and graphics items' makers (GmAudioDatabaseMaker, AppGraphics) are classes of other branches, reached through their slots: start-up, not recovered, supplies them.
#include "gm/GmDatabaseManifestItems.h"

void AtFatal(const char* message, int a, int b, int c);

namespace {
void StopHere(const char* what) {
    AtFatal(what, 0, 0, 0);
}
}

void AtCinemaDatabase::Load(GmCinemaOwner*, const char*, int) { StopHere("Cinema databases (0x000C5B30) are not recovered"); }
void AtEventDatabase::Load(GmEventOwner*, const char*, int) { StopHere("Event databases (0x000C9A00) are not recovered"); }
void GmSpaceGraphOwner::SetGraph(AtSpaceGraph*) { StopHere("Space graph owner: 0x00140E40 is not recovered"); }
