// Vftable slots of the JAM header classes that are NOT recovered yet. Each fails with a clear diagnostic. This file is
// never recorded in config/functions.toml and never counts as recovery. A method moves out of here when it is recovered.
#include "at/AtIoJam.h"

#define AT_NOT_RECOVERED(what) AtFatal("not recovered yet: " what "\n", 0, 0, 0)

AtIoFastAlignedJamHeader::~AtIoFastAlignedJamHeader() { AT_NOT_RECOVERED("~AtIoFastAlignedJamHeader (0x0007F6B0)"); }
void AtIoFastAlignedJamHeader::vInit(AtIoJamFileSystem*) { AT_NOT_RECOVERED("AtIoFastAlignedJamHeader::vInit (0x0007F750)"); }
void* AtIoFastAlignedJamHeader::OpenFile() { AT_NOT_RECOVERED("OpenFile (0x000806A0)"); return 0; }
void AtIoFastAlignedJamHeader::CloseFile(void*) { AT_NOT_RECOVERED("CloseFile (0x00080D90)"); }
void AtIoFastAlignedJamHeader::List(void*, void*, void*) { AT_NOT_RECOVERED("AtIoFastAlignedJamHeader::List (0x0007F9E0)"); }

AtIoFastJamHeader::~AtIoFastJamHeader() { AT_NOT_RECOVERED("~AtIoFastJamHeader (0x000803F0)"); }
void AtIoFastJamHeader::vInit(AtIoJamFileSystem*) { AT_NOT_RECOVERED("AtIoFastJamHeader::vInit (0x00080450)"); }
void* AtIoFastJamHeader::OpenFile() { AT_NOT_RECOVERED("OpenFile (0x000806A0)"); return 0; }
void AtIoFastJamHeader::CloseFile(void*) { AT_NOT_RECOVERED("CloseFile (0x00080D90)"); }
void AtIoFastJamHeader::List(void*, void*, void*) { AT_NOT_RECOVERED("AtIoFastJamHeader::List (0x00080710)"); }
