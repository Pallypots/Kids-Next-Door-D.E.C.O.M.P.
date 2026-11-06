// Vftable slots of the JAM header classes that are NOT recovered yet. Each fails with a clear diagnostic. This file is
// never recorded in config/functions.toml and never counts as recovery. A method moves out of here when it is recovered.
#include "at/AtIoJam.h"

#define AT_NOT_RECOVERED(what) AtFatal("not recovered yet: " what "\n", 0, 0, 0)

void AtIoFastAlignedJamHeader::List(void*, void*, void*) { AT_NOT_RECOVERED("AtIoFastAlignedJamHeader::List (0x0007F9E0)"); }
void AtIoFastJamHeader::List(void*, void*, void*) { AT_NOT_RECOVERED("AtIoFastJamHeader::List (0x00080710)"); }

void* AtIoJamFileSystem::List(const char*) { AT_NOT_RECOVERED("AtIoJamFileSystem::List (0x0007D4D0)"); return 0; }
void AtIoJamFileSystem::FreeList(void*) { AT_NOT_RECOVERED("AtIoJamFileSystem::FreeList (0x00103640)"); }

// JAM1: no archive on this disc uses it.
AtIoJam1Header::AtIoJam1Header() { AT_NOT_RECOVERED("JAM1 header (0x00081020)"); }
AtIoJam1Header::~AtIoJam1Header() {}
void AtIoJam1Header::vInit(AtIoJamFileSystem*) { AT_NOT_RECOVERED("JAM1 vInit"); }
AtIoJamFile* AtIoJam1Header::OpenFile() { AT_NOT_RECOVERED("JAM1 OpenFile"); return 0; }
int AtIoJam1Header::CloseFile(AtIoFile*) { AT_NOT_RECOVERED("JAM1 CloseFile"); return 0; }
void AtIoJam1Header::Unknown4() {}
void AtIoJam1Header::Unknown5() {}
int AtIoJam1Header::IsDirectoryMissing(const char*) { AT_NOT_RECOVERED("JAM1 IsDirectoryMissing"); return 1; }
int AtIoJam1Header::IsFileMissing(const char*) { AT_NOT_RECOVERED("JAM1 IsFileMissing"); return 1; }
int AtIoJam1Header::GetEntryType(const char*, int*) { AT_NOT_RECOVERED("JAM1 GetEntryType"); return 1; }
void AtIoJam1Header::List(void*, void*, void*) { AT_NOT_RECOVERED("JAM1 List"); }
int AtIoJam1Header::GetFileInfo(const char*, unsigned int*, unsigned int*) { AT_NOT_RECOVERED("JAM1 GetFileInfo (0x00081000)"); return 0; }
