// Statics of start-up's objects that other subsystems read, in a file of their own so that those programs do not pull in App's seams (src/gm/GmAppHost.cpp).
#include "gm/GmApp.h"

GmDataRoot* GmDataRoot::s_instance;            // 0x003B357C
