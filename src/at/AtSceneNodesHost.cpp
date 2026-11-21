// What a sound node asks of its emitter, sound playback, which is not recovered: each stops with a diagnostic. In a file of its own, so that a test can supply them. See include/at/AtSceneNodes.h.
#include "at/AtSceneNodes.h"

void AtFatal(const char* message, int a, int b, int c);

void AtSceneSoundSetVelocity(AtAudioEmitter*, const AtVector3*) {
    AtFatal("Atlas Scene: an emitter's slot 25, 0x0011F190, is not recovered", 0, 0, 0);
}

void AtSceneSoundPlay(AtAudioEmitter*, AtAudioSegment*, int) {
    AtFatal("Atlas Scene: an emitter's slot 15, 0x0011F010, is not recovered", 0, 0, 0);
}
