// What stands between the table reader and three members of AtDataString that no branch has recovered. Each stops with a diagnostic. In a file of its own, so that a test can supply them instead.
// Not recovered code; each is named in include/at/AtDataLibrary.h. The registry, descriptor and value classes these once also stood for are now the recovered ones of include/at/AtDataRegistry.h.
#include "at/AtDataLibrary.h"

void AtFatal(const char* message, int a, int b, int c);

void AtDataStringReset(AtDataString*) {
    AtFatal("AtDataRecordDatabase: 0x0006A1D0 is not recovered", 0, 0, 0);
}

void AtDataStringBindAndRead(AtDataString*, AtDataDescriptor*, AtIoTokenizer*, char*, unsigned int) {
    AtFatal("AtDataRecordDatabase: 0x0006A4D0 is not recovered", 0, 0, 0);
}

void AtDataStringBind(AtDataString*, AtDataDescriptor*, char*, unsigned int) {
    AtFatal("AtDataRecordDatabase: 0x00069FD0 is not recovered", 0, 0, 0);
}
