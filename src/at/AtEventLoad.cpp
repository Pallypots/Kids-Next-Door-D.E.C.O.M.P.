// AtEventDatabase::Load, 0x000C9A00: see include/at/AtEvent.h. In a file of its own, so that a harness can stand its own Load in and still link the rest of the class from
// src/at/AtEvent.cpp. Verification status: config/functions.d/aee.toml.
#include "at/AtEvent.h"

#include "at/AtText.h"

void AtEventDatabase::Load(AtDataRegistryOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AEE", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AEE");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}
