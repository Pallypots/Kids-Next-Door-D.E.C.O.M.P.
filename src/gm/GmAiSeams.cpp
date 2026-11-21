// The seams of the AI action manager's commands: each one original function that is not recovered, stopping with a diagnostic. In a file of its own so that no recovered caller has one inlined:
// replacing a seam means deleting its body here. Mapped to its original address in config/functions.d/gmai.toml.
#include "gm/GmAiActionManager.h"

void AtFatal(const char* message, int file, int line, int unused);

void GmChartTextArgument(char* out, unsigned int, GmChartContext*, void*, void**, int, int) {
    AtFatal("Gm AI: 0x001659F0, the behaviour chart's text argument, is not recovered", 0, 0, 0);
    out[0] = 0;
}
