// The report of a failed file, 0x00014230 and its body 0x000141C0: see include/at/AtFatal.h. Verification status: config/functions.d/fatal.toml.
//
// Recovered callers use the two fixed forms in src/at/AtText.cpp, AtReportFileError and AtReportFileErrorText, which give the same message for what they pass; these are the
// original's own variadic functions.
#include "at/AtFatal.h"

#include "at/AtText.h"

int AtFormat(char* out, const char* format, ...);  // the C library's sprintf, 0x00125559: src/at/AtIoFormatHost.cpp

void AtReport(int status, const char* format, ...) {
    AtVaList arguments;
    AT_VA_START(arguments, format);
    AtReportV(status, format, arguments);
    AT_VA_END(arguments);
}

void AtReportV(int status, const char* format, AtVaList arguments) {
    char message[0x200];
    if (format == 0) {
        format = "<No Message>";
    }
    const int length = AtFormat(message, "%s: ", AtIoStatusText(status));
    AtFormatV(message + length, format, arguments);
    AtFatal(message, 0, 0, 0);
}
