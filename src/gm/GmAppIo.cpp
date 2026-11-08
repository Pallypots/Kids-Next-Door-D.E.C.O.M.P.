// AtIoContext: pushing and popping a directory on every file system at once. 0x00019370-0x000193CA. Names inferred.
// Kept with App's start-up, their only caller recovered so far, so that one suite's mutants reach them; they belong with src/at/AtIoFileSystem.cpp.
// Start-up (0x0008DEA0) brackets its two registry files with these, through the mount manager's context.
#include "at/AtIoFileSystem.h"

void AtIoContext::PushPath(const char* directory) {
    for (unsigned int i = 0; i < m_stackCount; i++) {
        m_stack[i]->m_path.Push(directory);
    }
}

void AtIoContext::PopPath() {
    for (unsigned int i = 0; i < m_stackCount; i++) {
        m_stack[i]->m_path.Pop(0);
    }
}
