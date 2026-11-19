// What a port puts in place of the Xbox's I/O device and device file system, which the I/O layer's factory makes (include/at/AtApLayer.h). Port code,
// never recovered code: on the Xbox these are 0x0011E4E0 and 0x001030C0, not recovered.
#pragma once

// The host directory the factory's file systems read from. Until it is set, they read from the working directory.
void AtHostIoSetRoot(const char* directory);
