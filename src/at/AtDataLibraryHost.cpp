// What stands between the library and classes held by the .ADR work on another branch. Each stops with a diagnostic. In a file of its own, so that a test can supply them instead. Not recovered code; each is
// named in include/at/AtDataLibrary.h with the line it becomes.
#include "at/AtDataLibrary.h"

void AtFatal(const char* message, int a, int b, int c);

AtDataRegistry* AtDataRegistryNew() {
    AtFatal("AtDataLibrary: the .ADR registry is not in this build (0x000674E0)", 0, 0, 0);
    return 0;
}

int AtDataRegistryIsLoaded(AtDataRegistry*) {
    AtFatal("AtDataLibrary: the .ADR registry is not in this build (+0x1EC)", 0, 0, 0);
    return 0;
}

void AtDataRegistryClear(AtDataRegistry*) {
    AtFatal("AtDataLibrary: the .ADR registry is not in this build (0x00067540)", 0, 0, 0);
}

void AtDataRegistryDelete(AtDataRegistry*) {
    AtFatal("AtDataLibrary: the .ADR registry is not in this build (slot 0)", 0, 0, 0);
}

void AtDataRegistryLoad(AtDataRegistry*, AtDataOwner*, const char*, int, char*, AtDataRegistry*) {
    AtFatal("AtDataLibrary: the .ADR registry is not in this build (0x00068880)", 0, 0, 0);
}

AtDataDescriptorFields* AtDataDescriptorArrayNew(unsigned int) {
    AtFatal("AtDatabaseManager: the field descriptor is not in this build (0x0006BC10)", 0, 0, 0);
    return 0;
}

void AtDataDescriptorArrayDelete(AtDataDescriptorFields*) {
    AtFatal("AtDatabaseManager: the field descriptor is not in this build (0x0006BFB0)", 0, 0, 0);
}

void AtDataDescriptorClear(AtDataDescriptorFields*) {
    AtFatal("AtDatabaseManager: the field descriptor is not in this build (0x0006BC60)", 0, 0, 0);
}

AtDataValue* AtDataValueArrayNew(unsigned int, unsigned int) {
    AtFatal("AtDataRecordDatabase: the value classes are not in this build", 0, 0, 0);
    return 0;
}

void AtDataValueArrayDelete(AtDataValue*) {
    AtFatal("AtDataRecordDatabase: the value classes are not in this build", 0, 0, 0);
}

unsigned int AtDataValueSize(unsigned int) {
    AtFatal("AtDataRecordDatabase: the value classes are not in this build", 0, 0, 0);
    return 0;
}

void AtDataStringReset(AtDataValue*) {
    AtFatal("AtDataRecordDatabase: 0x0006A1D0 is not recovered", 0, 0, 0);
}

void AtDataStringBindAndRead(AtDataValue*, AtDataDescriptorFields*, AtIoTokenizer*, char*, unsigned int) {
    AtFatal("AtDataRecordDatabase: 0x0006A4D0 is not recovered", 0, 0, 0);
}

void AtDataStringBind(AtDataValue*, AtDataDescriptorFields*, char*, unsigned int) {
    AtFatal("AtDataRecordDatabase: 0x00069FD0 is not recovered", 0, 0, 0);
}
