// The seams of GmAcObject, GmAcCharacter and the record reader: each one original function that is not recovered, standing where the recovered code calls it, and stopping with a diagnostic. In a
// file of their own so that no recovered caller has one inlined: replacing a seam means deleting its body here. Each is mapped to its original address in config/functions.d/gmchar.toml.
#include "gm/GmAcCharacter.h"

void AtFatal(const char* message, int file, int line, int unused);

void GmAcNode::ApplyMatAssignment(int) { AtFatal("Gm character: 0x0014D590, applying a node's material assignment to its scene node, is not recovered", 0, 0, 0); }
int GmAcNode::IsVisible() { AtFatal("Gm character: 0x0014C1B0, whether a node is in view, is not recovered", 0, 0, 0); return 0; }

GmAcInteraction::GmAcInteraction() { AtFatal("Gm character: 0x001553C0, an object's interaction, is not recovered", 0, 0, 0); }
GmAcInteraction::~GmAcInteraction() { AtFatal("Gm character: 0x00155B50, deleting an object's interaction, is not recovered", 0, 0, 0); }
void GmAcInteraction::Attach(GmAcObject*) { AtFatal("Gm character: 0x001645D0, attaching an object's interaction, is not recovered", 0, 0, 0); }
void GmAcInteraction::Slot2() { AtFatal("Gm character: 0x00154780, an interaction's slot 2, is not recovered", 0, 0, 0); }
void GmAcInteraction::Slot3() { AtFatal("Gm character: 0x00154BC0, an interaction's slot 3, is not recovered", 0, 0, 0); }
