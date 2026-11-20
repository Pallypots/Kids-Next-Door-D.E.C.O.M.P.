// The registry names' Add (0x00071F20), which start-up and the level manifest's InstanceRegistries items call. Not recovered: stops with a diagnostic. In a file of its own so that a
// test or oracle can supply it.
#include "gm/GmApp.h"

void AtFatal(const char* message, int a, int b, int c);

void GmRegistryNames::Add(AtDataRegistry*) { AtFatal("Registry names: 0x00071F20 is not recovered", 0, 0, 0); }
