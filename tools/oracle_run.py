#!/usr/bin/env python3
"""Build and run the differential oracle (tests/oracle/oracle_gm.cpp), and qualify it with deliberately broken source.

  python -I tools/oracle_run.py [--seeds 5] [--ops 3000] [--no-mutants] [--suite gm|jam|all]

The jam suite reads analysis-local/oracle-jam/ (python -I tools/jam.py cases <image> analysis-local/oracle-jam).

Windows x86 only, local only: needs the owner's XBE. Writes build/oracle-report.json. Exit 0 only if every seed shows no
difference, every recorded function's entry was reached, and every source mutant is detected.

A mutant is one small edit to a copy of the recovered source. "Detected" means the oracle exited with a difference or
crashed; a mutant that survives means the oracle cannot see that behaviour, which fails this run.
"""
import argparse
import concurrent.futures
import glob
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
import threading
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fnconfig  # noqa: E402
BUILD = os.path.join(ROOT, "build", "oracle")
MUTANT_BUILD = os.path.join(ROOT, "build", "oracle-mutant")
MUTANT_SRC = os.path.join(ROOT, "build", "oracle-mutant-src")
CONFIG = "RelWithDebInfo"
PRIVATE = fnconfig.private_root(ROOT)
XBE = os.path.join(PRIVATE, "orig", "xiso", "default.xbe")

CASES = os.path.join(PRIVATE, "analysis-local", "oracle-jam")
TOK_CASES = os.path.join(PRIVATE, "analysis-local", "oracle-tok")

# (name, file, exact text to replace, replacement). The text must occur exactly once.
GM_MUTANTS = [
    ("Append: prev link dropped", "GmLinkedList.cpp", "node->prev = last;", "node->prev = 0;"),
    ("Append: head never set", "GmLinkedList.cpp", "    if (!head) {\n        head = node;\n    }\n", ""),
    ("Node::Disconnect: tail not updated", "GmLinkedList.cpp", "list->tail = prev;", ""),
    ("Node::Disconnect: entry not freed", "GmLinkedList.cpp", "    m_active = 0;\n    Unlink();", "    Unlink();"),
    ("Node::Disconnect: plug link kept", "GmLinkedList.cpp", "    m_active = 0;\n    Unlink();", "    m_active = 0;"),
    ("Allocate: no wrap-around", "GmLinkedList.cpp", "if (index == kPoolSize) {\n            index = 0;\n        }",
     "if (index == kPoolSize) {\n            index = kPoolSize - 1;\n        }"),
    ("Allocate: cursor not stored", "GmLinkedList.cpp", "            s_cursor = index;\n            s_pool", "            s_pool"),
    # Not used: removing the `return` in GmPlug::Disconnect is an equivalent mutant. Disconnect -> Unlink zeroes
    # m_plugNext, so the loop stops by itself.
    ("Plug::Disconnect: matches the wrong socket", "GmLinkedList.cpp", "        if (c->m_socket == socket) {\n            c->Disconnect();",
     "        if (c->m_socket != socket) {\n            c->Disconnect();"),
    ("IsConnectedTo: wrong result", "GmLinkedList.cpp", "            return 1;", "            return 2;"),
    ("~GmLinkedList: context not cleared", "GmLinkedList.cpp", "    SetContext(0);\n", ""),
    ("Iterator ctor: does not start at head", "GmLinkedList.cpp", "        m_cur = list->head;\n    }\n}", "        m_cur = list->tail;\n    }\n}"),
    ("Iterator::Prev: moves to null", "GmLinkedList.cpp", "    if (prev) {\n        m_cur = prev;\n    }\n}", "    m_cur = prev;\n}"),
    ("Iterator::NextData: does not advance", "GmLinkedList.cpp", "    m_cur = node->next;\n    return data;", "    return data;"),
    ("Iterator::PrevData: goes forward", "GmLinkedList.cpp", "    m_cur = node->prev;", "    m_cur = node->next;"),
    ("Iterator::At: off by one", "GmLinkedList.cpp", "for (int i = 0; i < index; i++) {", "for (int i = 0; i <= index; i++) {"),
    ("Iterator::Count: counts one extra", "GmLinkedList.cpp", "    int count = 0;", "    int count = 1;"),
    ("RemoveCurrent: position not advanced", "GmLinkedList.cpp", "        node->Disconnect();\n        m_cur = following;", "        node->Disconnect();"),
    ("Connect: not added to plug", "GmConnection.cpp", "    plug->m_connections = this;\n", ""),
    ("Connect: previous first not linked", "GmConnection.cpp", "        first->m_plugPrev = this;", ""),
    ("Unlink: plug head not updated", "GmConnection.cpp", "        m_plug->m_connections = m_plugNext;", ""),
    ("Unlink: next link not repaired", "GmConnection.cpp", "        m_plugNext->m_plugPrev = m_plugPrev;", ""),
    ("Socket::Contains: inverted", "GmSocket.cpp", "return item->IsConnectedTo(this);", "return !item->IsConnectedTo(this);"),
    ("Socket::Remove: does nothing", "GmSocket.cpp", "    item->Disconnect(this);\n", ""),
]

A = "AtIoFastAlignedJamHeader.cpp"
J = "AtIoFastJamHeader.cpp"
T = "AtIoJamNameTable.cpp"
JAM_MUTANTS = [
    ("Info: tokens swapped", A, "m_token = kFsta;", "m_token = kFstr;"),
    ("Info: header size from the wrong field", A, "m_headerSize = AtReadU32(p + 8);", "m_headerSize = AtReadU32(p + 4);"),
    ("Info: label one byte short", A, "for (int i = 0; i < 15; i++) {", "for (int i = 0; i < 14; i++) {"),
    ("Info: extension count is 16 bits", A, "m_extensionCount = p[0x1E];", "m_extensionCount = AtReadU16(p + 0x1E) + 1;"),
    ("Info: Init does not clear first", A, "    if (m_token != 0) {\n        Clear();\n    }\n", ""),
    ("Info: Clear leaves the label", A, "    label[3] = 0;\n", ""),
    ("FSTR: flag not set by the constructor", A, "AtIoFastRawJamHeader::AtIoFastRawJamHeader() {\n    AtIoFastAlignedJamHeader::Reset();\n    m_swappedFields = 1;",
     "AtIoFastRawJamHeader::AtIoFastRawJamHeader() {\n    AtIoFastAlignedJamHeader::Reset();"),
    ("FSTR: flag not kept by Reset", A, "void AtIoFastRawJamHeader::Reset() {\n    AtIoFastAlignedJamHeader::Reset();\n    m_swappedFields = 1;",
     "void AtIoFastRawJamHeader::Reset() {\n    AtIoFastAlignedJamHeader::Reset();"),
    ("FSTA dir: no case folding", A, "        copy[i] = (char)toupper(copy[i]);\n    }\n    unsigned int foundFiles", "    }\n    unsigned int foundFiles"),
    ("FSTA dir: FSTR fields not swapped", A, "if (m_swappedFields) {\n                    count = word >> 21;\n                    foundFiles = (word & 0x1FF000) >> 12;",
     "if (m_swappedFields) {\n                    count = (word & 0x1FF000) >> 12;\n                    foundFiles = word >> 21;"),
    ("FSTA dir: quirk: entry rewound after a miss", A, "    do {\n        char* separator = strchr(cursor, '\\\\');", "    do {\n        if (!found) {\n            entry = m_root;\n        }\n        char* separator = strchr(cursor, '\\\\');"),
    ("FSTA dir: quirk: a middle miss is an error", A, "        found = 0;\n        for (unsigned int i = 0; i < count; i++) {\n            unsigned int word = AtReadU32(entry);",
     "        if (cursor != copy && !found) {\n            return 0;\n        }\n        found = 0;\n        for (unsigned int i = 0; i < count; i++) {\n            unsigned int word = AtReadU32(entry);"),
    ("FSTA dir: child offset not from the buffer", A, "entry = m_buffer + offset;", "entry = m_root + offset;"),
    ("FSTA file: separator found from the left", A, "char* separator = strrchr(copy, '\\\\');", "char* separator = strchr(copy, '\\\\');"),
    ("FSTA file: extension ignored", A, "if (entryName == name && entryExt == ext) {\n            *size = entrySize;", "if (entryName == name) {\n            *size = entrySize;"),
    ("FSTA file: offset and size swapped", A, "*size = entrySize;\n            return (int)offset;", "*size = offset;\n            return (int)entrySize;"),
    ("FSTA file: no extension means absent", A, "    int ext = 0;\n    char* dot", "    int ext = -1;\n    char* dot"),
    ("FSTA file: entries read from the table start", A, "const unsigned char* entry = table + dirs * 8;", "const unsigned char* entry = table;"),
    ("FSTA: IsFileMissing inverted", A, "return FindFile(path, &size) < 0;", "return FindFile(path, &size) >= 0;"),
    ("FSTA: GetEntryType wrong type", A, "*type = kEntryIsFile;", "*type = 1;"),
    ("JAM2 dir: fields swapped", J, "count = (word >> 12) & 0x1FF;\n                foundFiles = word >> 21;", "count = word >> 21;\n                foundFiles = (word >> 12) & 0x1FF;"),
    ("JAM2 dir: quirk: entry rewound after a miss", J, "    do {\n        char* separator = strchr(cursor, '\\\\');", "    do {\n        if (!found) {\n            entry = m_root;\n        }\n        char* separator = strchr(cursor, '\\\\');"),
    ("JAM2 Locate: extension 0 when absent is lost", J, "    } else {\n        *ext = 0;\n    }", "    } else {\n        *ext = -1;\n    }"),
    ("JAM2 record: entry stride wrong", J, "        entry += 8;\n        if (entryName == name", "        entry += 12;\n        if (entryName == name"),
    ("JAM2 info: size from the first field", J, "*size = AtReadU32(sizes + 4);", "*size = AtReadU32(sizes);"),
    ("JAM2 info: pending read not released", J, "        file->Release(request);\n", ""),
    ("JAM2 info: pending status ignored", J, "        status = request->Wait();\n", "        request->Wait();\n        status = 0;\n"),
    ("JAM2 info: seek failure ignored", J, "    if (file->Seek((unsigned int)record) != 0) {\n        return 0;\n    }\n", "    file->Seek((unsigned int)record);\n"),
    ("JAM2 offset: missing keeps old output", J, "        *offset = 0;\n        return 0;", "        return 0;"),
    ("Table: stride wrong for names", T, "return Search(name, m_names, 8, m_nameCount);", "return Search(name, m_names, 4, m_nameCount);"),
    ("Table: empty extension absent", T, "    if (ext == 0 || *ext == 0) {\n        return 0;\n    }", "    if (ext == 0 || *ext == 0) {\n        return -1;\n    }"),
    ("Table: empty name found", T, "    if (*name == 0) {\n        return -1;\n    }\n", ""),
    ("Table: search bound moved too far", T, "            if (low < mid) {\n                low = mid;\n            }", "            low = mid + stride;"),
    # Not used: `order >= 0` in the scan is equivalent, because order == 0 has already returned; removing the break is
    # equivalent on a sorted table. Both were tried and explained (docs/verification.md).
    ("Table: scan reports a hit on passing the key", T, "        if (order > 0) {\n            break;\n        }", "        if (order > 0) {\n            return i;\n        }"),
    ("Table: extensions start in the wrong place", T, "m_extensions = names + nameCount * 8;", "m_extensions = names + nameCount * 4;"),
    ("Table: GetFullName bounds off by one", T, "if (name >= m_nameCount || extension >= m_extensionCount) {", "if (name > m_nameCount || extension > m_extensionCount) {"),
    # Not used: not clearing words 0 or 1 is equivalent, because strncpy pads those eight bytes itself.
    ("Table: GetFullName not cleared", T, "    ((unsigned int*)out)[2] = 0;\n", ""),
    ("Table: wildcard not honoured", T, "if (strncmp(namePattern, name, 8) != 0 && *namePattern != '*') {", "if (strncmp(namePattern, name, 8) != 0) {"),
    ("Table: no-dot extension wrong", T, "        ext = name + strlen(name);", "        ext = name;"),
    ("Table: destructor leaves the sentinel", T, "AtIoJamNameTable::~AtIoJamNameTable() {\n    m_names = 0;\n    m_extensions = 0;\n    m_unknown08 = -1;", "AtIoJamNameTable::~AtIoJamNameTable() {\n    m_names = 0;\n    m_extensions = 0;"),
]

F = "AtIoJamFile.cpp"
JAM_MUTANTS += [
    ("vInit FSTA: no shutdown on re-init", A, "    if (m_fs) {\n        Shutdown();\n    }\n", ""),
    ("vInit FSTA: heap not pushed", A, "    AtPushHeap(s_heap);\n    m_buffer", "    m_buffer"),
    ("vInit FSTA: heap never checked", A, "    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    m_buffer", "    AtPushHeap(s_heap);\n    m_buffer"),
    ("vInit FSTA: seek failure ignored", A, 'if (fs->File()->Seek(0) != 0) {', 'if (fs->File()->Seek(0) == 99) {'),
    ("vInit FSTA: wrong fatal message", A, "AtIoFastAlignedJamHeader::vInit:  error reading header!\\n\", 0, 0, 0);\n        }\n", "AtIoFastAlignedJamHeader::vInit:  error reading the header!\\n\", 0, 0, 0);\n        }\n"),
    ("vInit FSTA: request not released", A, "        fs->File()->Release(request);\n", ""),
    ("vInit FSTA: root counts swapped", A, "    m_rootDirs = AtReadU16(root);\n    root += 2;\n    m_rootFiles = AtReadU16(root);", "    m_rootFiles = AtReadU16(root);\n    root += 2;\n    m_rootDirs = AtReadU16(root);"),
    ("vInit FSTA: one file object too few", A, "m_files = new AtIoFastAlignedJamFile[m_fs->m_maxOpen];", "m_files = new AtIoFastAlignedJamFile[m_fs->m_maxOpen - 1];"),
    ("Shutdown FSTA: buffer leaked", A, "void AtIoFastAlignedJamHeader::Shutdown() {\n    operator delete(m_buffer);\n", "void AtIoFastAlignedJamHeader::Shutdown() {\n"),
    ("Shutdown FSTA: fields kept", A, "    m_table.Clear();\n    Reset();\n}\n\nAtIoFastAlignedJamHeader::~", "    m_table.Clear();\n}\n\nAtIoFastAlignedJamHeader::~"),
    ("Dtor FSTA: buffer leaked", A, "AtIoFastAlignedJamHeader::~AtIoFastAlignedJamHeader() {\n    operator delete(m_buffer);\n", "AtIoFastAlignedJamHeader::~AtIoFastAlignedJamHeader() {\n"),
    ("FindFileInfo: missing keeps old offset", A, "        *offset = 0;\n        return 0;", "        return 0;"),
    ("vInit JAM2: no shutdown on re-init", J, "    if (m_fs) {\n        Shutdown();\n    }\n", ""),
    ("vInit JAM2: wrong file class size", J, "m_files = new AtIoFastJamFile[m_fs->m_maxOpen];", "m_files = new AtIoFastJamFile[m_fs->m_maxOpen + 1];"),
    ("Shutdown JAM2: fields kept", J, "    m_table.Clear();\n    m_fs = 0;", "    m_table.Clear();"),
    ("Dtor JAM2: no shutdown", J, "AtIoFastJamHeader::~AtIoFastJamHeader() {\n    Shutdown();\n", "AtIoFastJamHeader::~AtIoFastJamHeader() {\n"),
    ("File base: flags not reset", F, "    m_sizeCopy = 0;\n    m_flags = 0;\n    return 0;", "    m_sizeCopy = 0;\n    return 0;"),
    ("File Clear: path kept", F, "int AtIoJamFile::Clear() {\n    m_flags = 0;\n    for (int i = 0; i < 0x20; i++) {\n        ((unsigned int*)m_path)[i] = 0;\n    }\n", "int AtIoJamFile::Clear() {\n    m_flags = 0;\n"),
    ("File Attach: no close first", F, "    if (m_owner) {\n        Close();\n    }\n", ""),
    ("File Close: owner kept", F, "    m_owner = 0;\n    m_base = 0;\n    m_secondSize = 0;\n    return ResetFile();", "    m_base = 0;\n    m_secondSize = 0;\n    return ResetFile();"),
    ("File Read: done not cleared", F, "    if (done) {\n        *done = 0;\n    }\n", ""),
    ("File Read: open flag ignored", F, "if (m_owner == 0 || !(m_flags & kOpen)) {", "if (m_owner == 0) {"),
    ("File Read: end flag not set at the end", F, "    if (m_position >= m_size) {\n        m_flags |= kReachedEnd;\n", "    if (m_position >= m_size) {\n"),
    ("File Read: clip boundary off by one", F, "if (m_position + size >= m_size) {", "if (m_position + size > m_size) {"),
    ("File Read: no clip", F, "        size = m_size - m_position;\n", ""),
    ("File Read: ignores the base", F, "archive->Seek(m_base + m_position)", "archive->Seek(m_position)"),
    ("File Read: position from the request size", F, "m_position += archive->m_position - before;", "m_position += size;"),
    ("File Write: done kept", F, "    *done = 0;\n    return kUnsupported;", "    return kUnsupported;"),
    ("File Release: forwards to slot 8", F, "return m_owner->m_fs->File()->Release(request);", "return m_owner->m_fs->File()->Unknown8(request);"),
    ("Dtor file: owner kept", F, "AtIoFastAlignedJamFile::~AtIoFastAlignedJamFile() {\n    if (m_owner) {\n        for (int i = 0; i < 0x20; i++) {\n            ((unsigned int*)m_path)[i] = 0;\n        }\n        m_owner = 0;\n", "AtIoFastAlignedJamFile::~AtIoFastAlignedJamFile() {\n    if (m_owner) {\n        for (int i = 0; i < 0x20; i++) {\n            ((unsigned int*)m_path)[i] = 0;\n        }\n"),
    ("Dtor JAM2 file: base kept", F, "AtIoFastJamFile::~AtIoFastJamFile() {\n    if (m_owner) {\n        for (int i = 0; i < 0x20; i++) {\n            ((unsigned int*)m_path)[i] = 0;\n        }\n        m_owner = 0;\n        m_base = 0;\n", "AtIoFastJamFile::~AtIoFastJamFile() {\n    if (m_owner) {\n        for (int i = 0; i < 0x20; i++) {\n            ((unsigned int*)m_path)[i] = 0;\n        }\n        m_owner = 0;\n"),
    ("Open FSTA: ignored flags kept", F, "    if (flags & kFlagsIgnored) {\n        m_flags = flags & ~kFlagsIgnored;\n    }\n    if (m_flags & kFlagsWrite) {\n        m_flags = 0;\n        return kUnsupported;\n    }\n    unsigned int size;", "    if (m_flags & kFlagsWrite) {\n        m_flags = 0;\n        return kUnsupported;\n    }\n    unsigned int size;"),
    ("Open FSTA: write access allowed", F, "    if (m_flags & kFlagsWrite) {\n        m_flags = 0;\n        return kUnsupported;\n    }\n    unsigned int size;", "    unsigned int size;"),
    ("Open FSTA: path copied from the wrong argument", F, "    m_position = 0;\n    strcpy(m_path, displayName);\n    return kOk;\n}\n\n// Original behaviour preserved: the current position", "    m_position = 0;\n    strcpy(m_path, path);\n    return kOk;\n}\n\n// Original behaviour preserved: the current position"),
    ("Open FSTA: seek failure ignored", F, "    m_sizeCopy = size;\n    if (m_owner->m_fs->File()->Seek(m_base) != 0) {\n        return kIoError;\n    }", "    m_sizeCopy = size;\n    m_owner->m_fs->File()->Seek(m_base);"),
    ("Seek FSTA: quirk: position not added", F, "Seek(m_base + offset + m_position)", "Seek(m_base + offset)"),
    ("Seek FSTA: end flag kept", F, "    m_position = offset;\n    m_flags &= ~kReachedEnd;\n    return kOk;\n}\n\nAtIoFastJamFile::AtIoFastJamFile", "    m_position = offset;\n    return kOk;\n}\n\nAtIoFastJamFile::AtIoFastJamFile"),
    ("Seek FSTA: boundary off by one", F, "    if (offset >= m_size) {\n        return kEndOfFile;\n    }\n    if (m_owner->m_fs->File()->Seek(m_base + offset + m_position)", "    if (offset > m_size) {\n        return kEndOfFile;\n    }\n    if (m_owner->m_fs->File()->Seek(m_base + offset + m_position)"),
    ("Open JAM2: no record header skip", F, "    m_base += 0x20;\n", ""),
    ("Open JAM2: sizes swapped", F, "    m_size = AtReadU32(sizes);\n    m_sizeCopy = m_size;\n    m_secondSize = AtReadU32(sizes + 4);", "    m_size = AtReadU32(sizes + 4);\n    m_sizeCopy = m_size;\n    m_secondSize = AtReadU32(sizes);"),
    ("Open JAM2: pending status ignored", F, "        status = request->Wait();\n        archive->Release(request);", "        request->Wait();\n        status = 0;\n        archive->Release(request);"),
    ("Open JAM2: request not released", F, "        status = request->Wait();\n        archive->Release(request);", "        status = request->Wait();"),
    ("Seek JAM2: quirk: position added", F, "Seek(m_base + offset) != 0", "Seek(m_base + offset + m_position) != 0"),
    ("OpenFile: first slot not preferred", F, "for (unsigned int i = 0; i < m_fs->m_maxOpen; i++) {\n        if (m_files[i].m_owner == 0) {", "for (unsigned int i = m_fs->m_maxOpen; i-- > 0;) {\n        if (m_files[i].m_owner == 0) {"),
    ("OpenFile: owned slot taken", F, "if (m_files[i].m_owner == 0) {", "if (true) {"),
    ("CloseFile: does nothing", F, "    return file->Close();\n", "    return 0;\n"),
]

P = "AtIoFileSystem.cpp"
H = "AtIoJamFileSystem.cpp"
JAM_MUTANTS += [
    ("Path SetRoot: no trailing separator", P, "m_root[strlen(m_root) - 1] != '\\\\') {", "m_root[strlen(m_root) - 1] == '\\\\') {"),
    ("Path SetRoot: not terminated at the limit", P, "    m_root[0x7F] = 0;\n", ""),
    ("Path Resolve: root skipped", P, "    strcpy(m_full, m_root);\n    if (m_top != -1) {\n        strcat(m_full, m_stack[m_top]);\n    }\n    strcat(m_full, relative);", "    m_full[0] = 0;\n    if (m_top != -1) {\n        strcat(m_full, m_stack[m_top]);\n    }\n    strcat(m_full, relative);"),
    ("Path Resolve: current skipped", P, "        strcat(m_full, m_stack[m_top]);\n    }\n    strcat(m_full, relative);", "    }\n    strcat(m_full, relative);"),
    ("Path current: boundary off by one", P, "    if (length >= size) {\n        return 0;\n    }\n    strcpy(out, m_root);", "    if (length > size) {\n        return 0;\n    }\n    strcpy(out, m_root);"),
    ("Path Join: out not emptied", P, "    *out = 0;\n    unsigned int length = relative", "    unsigned int length = relative"),
    ("Path Join: null relative counted", P, "unsigned int length = relative ? strlen(relative) : 0;", "unsigned int length = relative ? strlen(relative) + 1 : 0;"),
    ("Path Join: boundary off by one", P, "if (strlen(current) + length >= size) {", "if (strlen(current) + length > size) {"),
    ("Path Push: depth limit wrong", P, "    if (m_top >= 7) {", "    if (m_top >= 6) {"),
    ("Path Push: no trailing separator", P, "if (length != 0 && directory[length - 1] != '\\\\') {", "if (false) {"),
    ("Path PushFromFile: slashes kept", P, "                *to = '\\\\';\n", ""),
    ("Path PushFromFile: no directory is ignored", P, "        Push(\"\");\n", ""),
    ("Path Pop: last ignored", P, "    if (last) {\n        strcpy(m_stack[m_top], last);\n    }\n", ""),
    ("Path Init: not cleared when used", P, "    if (m_initialised) {\n        ClearPath(this);\n    }\n", ""),
    ("Base: kind not reset", P, "    m_path.Clear();\n    m_kind = 0;\n", "    m_path.Clear();\n"),
    ("Base: path not reset", P, "    m_path.Clear();\n    m_kind = 0;\n", "    m_kind = 0;\n"),
    ("Base: default stat leaves the size", P, "    stat->m_size = 0;\n    stat->m_name[0] = 0;", "    stat->m_name[0] = 0;"),
    ("Context: full stack accepted", P, "    if (m_stackCount < m_stackSize) {", "    if (true) {"),
    ("Context: Pop leaves the slot", P, "        m_stack[m_stackCount - 1] = 0;\n", ""),
    ("Context: FindFile from the bottom", P, "for (int i = (int)m_stackCount - 1; i >= 0; i--) {", "for (int i = 0; i < (int)m_stackCount; i++) {"),
    ("Context: FindFile keeps the old out", P, "    if (fs) {\n        *fs = 0;\n    }\n", ""),
    ("Context: empty stack means found", P, "    if (m_stackCount == 0) {\n        return 1;\n    }\n    for", "    if (m_stackCount == 0) {\n        return 0;\n    }\n    for"),
    ("JamFs Open: no archive not checked", H, "    if (m_info.m_token == 0) {\n        return AtIoFile::kIoError;\n    }\n    if (AtIoContext::s_openHook)", "    if (AtIoContext::s_openHook)"),
    ("JamFs Open: hook skipped", H, "        AtIoContext::s_openHook();\n", ""),
    ("JamFs Open: display name is the full path", H, "file->Open(m_path.m_full, relative, flags);", "file->Open(m_path.m_full, m_path.m_full, flags);"),
    ("JamFs Open: failed file kept", H, "        m_header->CloseFile(file);\n        *out = 0;", "        *out = 0;"),
    ("JamFs Open: count not raised", H, "    m_openFiles++;\n", ""),
    ("JamFs Open: peak not kept", H, "        m_peakOpenFiles = m_openFiles;\n", ""),
    ("JamFs Close: count not lowered", H, "    m_openFiles--;\n", ""),
    ("JamFs Close: file kept", H, "    m_header->CloseFile(file);\n    return result;", "    return result;"),
    ("JamFs IsFileMissing: relative path used", H, "    return m_header->IsFileMissing(m_path.m_full);", "    return m_header->IsFileMissing(path);"),
    ("JamFs Stat: record header not added", H, "    stat->m_offset += recordHeader;\n", ""),
    ("JamFs Stat: time not copied", H, "    stat->m_time = m_archiveTime;\n", ""),
    ("JamFs Stat: name not copied", H, "    strncpy(stat->m_name, m_archiveName, 0x10C);\n    return 0;", "    return 0;"),
    ("JamFs Shutdown: archive not closed", H, "    if (m_file) {\n        m_device->Close(m_file);\n    }\n", ""),
    ("JamFs Shutdown: header kept", H, "    if (m_header) {\n        delete m_header;\n    }\n", ""),
    ("JamFs Shutdown: info kept", H, "    m_info.Clear();\n    m_header = 0;", "    m_header = 0;"),
    ("JamFs Shutdown: base not reset", H, "    ResetFileSystem();\n", ""),
    ("JamFs vInit: no shutdown on re-init", H, "    if (m_info.m_token != 0) {\n        Shutdown();\n    }\n", ""),
    ("JamFs vInit: zero nMaxOpen unreported", H, "    if (maxOpen == 0) {\n        AtFatal(\"AtIoJamFileSystem::vInit:  nMaxOpen has to be greater than zero!\\n\", 0, 0, 0);\n    }\n", ""),
    ("JamFs vInit: path not initialised", H, "    InitPath();\n", ""),
    ("JamFs vInit: kind wrong", H, "    m_kind = 5;\n", "    m_kind = 4;\n"),
    ("JamFs vInit: size and time swapped", H, "    m_archiveSize = stat.m_size;\n    m_archiveTime = stat.m_time;", "    m_archiveSize = stat.m_time;\n    m_archiveTime = stat.m_size;"),
    ("JamFs vInit: flags not passed on", H, "m_device->Open(path, flags, &m_file)", "m_device->Open(path, 0, &m_file)"),
    ("JamFs vInit: pending status ignored", H, "        status = request->Wait();\n", "        request->Wait();\n        status = 0;\n"),
    ("JamFs vInit: request not released", H, "        m_file->Release(request);\n", ""),
    ("JamFs vInit: message spelled correctly", H, "AtIoJamFileSytem::vInit", "AtIoJamFileSystem::vInit"),
    ("JamFs vInit: JAM2 builds the FSTA class", H, "m_header = new AtIoFastJamHeader;", "m_header = new AtIoFastAlignedJamHeader;"),
    ("JamFs vInit: device field not copied", H, "    m_unknown6B8 = m_device->m_unknown6B8;\n", ""),
    ("JamFs Create: max and flags swapped", H, "fs->vInit(maxOpen, path, flags);", "fs->vInit((unsigned int)flags, path, (int)maxOpen);"),
]

_F, _T, _X = "AtIoStream.cpp", "AtIoTokenizer.cpp", "AtIoTextTokenizer.cpp"
_UNREAD_DEC = "                s->m_cur--;\n                if (negative) {\n                    m_int = (int)(0u - (unsigned int)m_int);\n                }\n                m_type = kInt;\n                return kInt;\n            }\n            break;\n        case kHex:"
TOK_MUTANTS = [
    ("Ref Close: file not closed", _F, "        m_fs->Close(m_file);\n", ""),
    ("Ref Read: no wait", _F, "        status = request->Wait();\n        m_file->Release(request);", "        m_file->Release(request);"),
    ("Ref Read: no release", _F, "        status = request->Wait();\n        m_file->Release(request);", "        status = request->Wait();"),
    ("Ref Write: no wait", _F, "        status = result.request->Wait();\n", ""),
    ("Ref Write: no release", _F, "        m_file->Release(result.request);\n", ""),
    ("Ref Flush: status lost", _F, "return m_file->Flush();", "m_file->Flush(); return 0;"),
    ("Ref OpenOn: previous not closed", _F, "    Close();\n    m_fs = fs;", "    m_fs = fs;"),
    ("Ref Open: write mask", _F, "flags & AtIoFile::kFlagsIgnored", "flags & 1"),
    ("Ref Open: write mask 2", _F, "flags & AtIoFile::kFlagsIgnored", "flags & 0x20"),
    ("Ref Open: a miss is success", _F, "        return 1;\n    }\n    return OpenOn", "        return 0;\n    }\n    return OpenOn"),
    ("Ref extension: first separator", _F, "strrchr(full, '\\\\')", "strchr(full, '\\\\')"),
    ("Ref extension: dot anywhere", _F, "strrchr(leaf, '.') == 0", "strrchr(full, '.') == 0"),
    ("Stream Refill: window", _F, "m_limit += 0x100;", "m_limit += 0x80;"),
    ("Stream Refill: no clamp", _F, "    if (m_limit > end) {\n        m_limit = end;\n    }\n", ""),
    ("Stream Refill: cursor kept", _F, "    m_cur = m_limit;\n", ""),
    ("Stream SkipLine: line not counted", _F, "                m_line++;\n                return 1;", "                return 1;"),
    ("Stream SkipWhitespace: line not counted", _F, "if (c == '\\n') {\n                m_line++;\n            } else if", "if (c == '\\n') {\n            } else if"),
    ("Stream SkipWhitespace: CR", _F, "c != ' ' && c != '\\t' && c != '\\r'", "c != ' ' && c != '\\t'"),
    ("Stream SkipWhitespace: tab", _F, "c != ' ' && c != '\\t' && c != '\\r'", "c != ' ' && c != '\\r'"),
    ("Stream Put: message", _F, "Unable to write buffer to media", "Unable to write buffer"),
    ("Stream Put: boundary", _F, "if (m_cur >= m_base + m_size) {", "if (m_cur >= m_base + m_size - 1) {"),
    ("Stream Free: not freed", _F, "        operator delete(m_allocated);\n", ""),
    ("Stream Allocate: line", _F, "    m_line = 1;\n", ""),
    ("Stream Allocate: no heap check", _F, "        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);", "    }\n    AtPushHeap(s_heap);"),
    ("Stream Allocate: heap not pushed", _F, "    AtPushHeap(s_heap);\n", ""),
    ("File stream Refill: no seek", _F, "            file->Seek(m_filePosition);\n", ""),
    ("File stream Refill: seek always", _F, "if (file->m_position != m_filePosition) {", "if (1) {"),
    ("File stream Refill: position after write", _F, "        m_filePosition += (unsigned int)(m_cur - m_base);\n", ""),
    ("File stream Refill: read error ignored", _F, "if (m_ref->Read(m_base, m_size, &done) != 0) {\n            return 0;\n        }", "m_ref->Read(m_base, m_size, &done);"),
    ("File stream Refill: position after read", _F, "        m_filePosition = m_ref->m_file->m_position;\n", ""),
    ("File stream Refill: write window", _F, "        done = m_size;\n", ""),
    ("File stream Refill: empty read succeeds", _F, "return m_size == done || done != 0;", "return 1;"),
    ("File stream Detach: nothing written", _F, "        Refill();\n    }\n    m_ref = 0;", "    }\n    m_ref = 0;"),
    ("File stream Attach: no detach", _F, "    if (m_ref != 0) {\n        Detach();\n    }\n", ""),
    ("File stream Attach: size", _F, "Allocate(bufferSize);", "Allocate(0x200);"),

    ("CopyName: length", _T, "for (int i = 0; i < 8; i++) {\n        to[i] = from[i];\n        if", "for (int i = 0; i < 7; i++) {\n        to[i] = from[i];\n        if"),
    ("CopyName9: terminator", _T, "    to[8] = 0;\n", ""),
    ("FormatInt: no minus", _T, "        *out++ = '-';\n        magnitude = 0u - magnitude;\n", "        magnitude = 0u - magnitude;\n"),
    ("FormatInt: no terminator", _T, "    *out = 0;\n}\n\n// The original calls", "}\n\n// The original calls"),
    ("FormatReal: half to even", _T, "keep < 17 && digits[1 + keep] >= '5'", "keep < 17 && digits[1 + keep] > '5'"),
    ("FormatReal: carry lost", _T, "        digits[i]++;\n", ""),
    ("FormatReal: new first digit", _T, "if (keep >= 0 && digits[0] != '0') {", "if (0) {"),
    ("FormatReal: zero", _T, 'strcpy(to, "0.000000");', 'strcpy(to, "0.0");'),
    ("FormatReal: NaN sign", _T, "(bits >> 31) ?", "(bits >> 30) ?"),
    ("FormatReal: infinity", _T, '"1.#INF00"', '"1.#INF"'),
    ("FormatReal: minus lost", _T, "    if (v < 0) {\n        *to++ = '-';\n    }\n", ""),
    ("FormatReal: rounds at the first digit", _T, "if (keep >= 0 && keep < 17 &&", "if (keep > 0 && keep < 17 &&"),
    ("FormatReal: leading zero", _T, "    if (exponent <= 0) {\n        *to++ = '0';\n    }\n", ""),
    ("Reset: stream", _T, "    m_stream = 0;\n    m_pushedBack = 0;", "    m_pushedBack = 0;"),
    ("Reset: pushed back", _T, "    m_pushedBack = 0;\n    m_type = 0;", "    m_type = 0;"),
    ("Reset: type", _T, "    m_type = 0;\n    m_int = 0;", "    m_int = 0;"),
    ("Reset: int", _T, "    m_int = 0;\n    m_real = 0;", "    m_real = 0;"),
    ("Reset: real", _T, "    m_real = 0;\n    m_string[0] = 0;", "    m_string[0] = 0;"),
    ("Reset: string", _T, "    m_string[0] = 0;\n    m_stringLength = 0;", "    m_stringLength = 0;"),
    ("Reset: length", _T, "    m_stringLength = 0;\n    m_fileName = 0;", "    m_fileName = 0;"),
    ("Reset: file name", _T, "    m_fileName = 0;\n    m_extension[0] = 0;", "    m_extension[0] = 0;"),
    ("Reset: extension", _T, "    m_extension[0] = 0;\n    m_escapes = 0;", "    m_escapes = 0;"),
    ("Reset: escapes", _T, "    m_extension[0] = 0;\n    m_escapes = 0;", "    m_extension[0] = 0;"),
    ("Base GetIdentifier", _T, 'return "";', 'return "x";'),
    ("ExpectInt: token", _T, "int AtIoTokenizer::ExpectInt() {\n    if (Next() != kInt)", "int AtIoTokenizer::ExpectInt() {\n    if (Next() != kReal)"),
    ("ExpectReal: token", _T, "float AtIoTokenizer::ExpectReal() {\n    if (Next() != kReal)", "float AtIoTokenizer::ExpectReal() {\n    if (Next() != kInt)"),
    ("ExpectString: limit", _T, "if (m_stringLength > maxLength)", "if (m_stringLength >= maxLength)"),
    ("ExpectStringCopy: size", _T, "(unsigned int)strlen(m_string) + 1;", "(unsigned int)strlen(m_string) + 2;"),
    ("ExpectName: limit", _T, "if (m_stringLength > 8)", "if (m_stringLength > 7)"),
    ("ExpectToken: code", _T, "Error(kExpectedKeyword);", "Error(kExpectedInt);"),
    ("ExpectLeftBracket: token", _T, "void AtIoTokenizer::ExpectLeftBracket() {\n    if (Next() != kLeftBracket)", "void AtIoTokenizer::ExpectLeftBracket() {\n    if (Next() != kRightBracket)"),
    ("ExpectRightBracket: token", _T, "void AtIoTokenizer::ExpectRightBracket() {\n    if (Next() != kRightBracket)", "void AtIoTokenizer::ExpectRightBracket() {\n    if (Next() != kLeftBracket)"),
    ("ExpectLeftCurly: token", _T, "void AtIoTokenizer::ExpectLeftCurly() {\n    if (Next() != kLeftCurly)", "void AtIoTokenizer::ExpectLeftCurly() {\n    if (Next() != kRightCurly)"),
    ("ExpectRightCurly: token", _T, "void AtIoTokenizer::ExpectRightCurly() {\n    if (Next() != kRightCurly)", "void AtIoTokenizer::ExpectRightCurly() {\n    if (Next() != kLeftCurly)"),
    ("ExpectCount: int unchecked", _T, "    if (Next() != kInt) {\n        Error(kExpectedInt);\n    }\n    if (Next() != kRightBracket)", "    Next();\n    if (Next() != kRightBracket)"),
    ("CopyName member: length", _T, "for (unsigned int i = 0; i < 8; i++) {\n        out[i]", "for (unsigned int i = 0; i < 7; i++) {\n        out[i]"),
    ("ErrorText: spelling kept", _T, "expected righ parenthesis", "expected right parenthesis"),
    ("ErrorText: tilde", _T, "expected at tilde", "expected tilde"),
    ("ErrorText: default", _T, "unknown error", "unknown"),
    ("Message: name limit", _T, "strncpy(message, t->m_fileName, 0x3F);\n        message[0x3F] = 0;", "strncpy(message, t->m_fileName, 0x3E);\n        message[0x3E] = 0;"),
    ("Message: extension limit", _T, "t->m_extension, 4);", "t->m_extension, 3);"),
    ("Message: line word", _T, '": line "', '" line "'),
    ("Message: separator", _T, '" - "', '"-"'),
    ("Message: line number", _T, "AtFormatInt(t->m_stream->m_line, message + strlen(message));", "AtFormatInt(0, message + strlen(message));"),
    ("Error: fatal argument", _T, "AtFatal(message, 0, 0, 0);", "AtFatal(message, 1, 0, 0);"),
    ("SetStream: no reset", _T, "    if (m_stream != 0) {\n        Reset();\n    }\n", ""),
    ("TokenFile Close: tokenizer kept", _T, "        delete m_tokenizer;\n", ""),
    ("TokenFile Close: not owned deleted", _T, "if (m_ownsTokenizer != 0 && m_tokenizer != 0) {", "if (m_tokenizer != 0) {"),
    ("TokenFile Close: name kept", _T, "        operator delete(m_name);\n", ""),
    ("TokenFile Close: no detach", _T, "    m_stream.Detach();\n", ""),
    ("TokenFile Close: file kept", _T, "    m_file.Close();\n", ""),
    ("TokenFile Open: buffer", _T, "Attach(&f->m_file, 0x400);", "Attach(&f->m_file, 0x200);"),
    ("TokenFile Open: ownership", _T, "        f->m_ownsTokenizer = 1;\n", ""),
    ("TokenFile Open: stale ownership", _T, "    } else {\n        f->m_ownsTokenizer = 0;\n    }\n", "    }\n"),
    ("TokenFile Open: name size", _T, "operator new(strlen(path) + 1);", "operator new(strlen(path) + 2);"),
    ("TokenFile Open: file name unset", _T, "    f->m_tokenizer->m_fileName = f->m_name;\n", ""),
    ("TokenFile Open: extension dot", _T, "m_tokenizer->m_extension[0] = '.';", "m_tokenizer->m_extension[0] = '_';"),
    ("TokenFile Open: extension length", _T, "m_tokenizer->m_extension + 1, ext, 3);", "m_tokenizer->m_extension + 1, ext, 2);"),
    ("TokenFile Open: no extension", _T, "    if (ext == 0) {\n        m_tokenizer->m_extension[0] = 0;\n    } else {", "    if (ext != 0) {"),
    ("TokenFile Open: no close before", _T, "    if (m_tokenizer != 0) {\n        Close();\n    }\n    const int status = m_file.OpenWithExtension", "    const int status = m_file.OpenWithExtension"),
    ("TokenFile Open 2: no close before", _T, "    if (m_tokenizer != 0) {\n        Close();\n    }\n    const int status = m_file.Open(", "    const int status = m_file.Open("),

    ("Table: percent", _X, "12, 2, 1, 1, 13,", "12, 2, 1, 1, 14,"),
    ("Table: underscore", _X, "7, 1, 8, 14, 1, 1,", "7, 1, 8, 14, 57, 1,"),
    ("Hash: shift", _X, "shift += 6;", "shift += 5;"),
    ("Hash: signed base", _X, "- 0x30) <<", "- 0x31) <<"),
    ("Insert: no wrap", _X, "        h++;\n        if (h >= AtIoTextTokenizer::kHashSize) {\n            h = 0;\n        }\n    }\n    t->m_hash[h] = keyword;", "        h++;\n        if (h >= AtIoTextTokenizer::kHashSize - 1) {\n            h = 0;\n        }\n    }\n    t->m_hash[h] = keyword;"),
    ("SetKeywords: hash not cleared", _X, "    for (unsigned int i = 0; i < kHashSize; i++) {\n        m_hash[i] = 0;\n    }\n", ""),
    ("SetKeywords: inherited skipped", _X, "                Insert(this, from->m_hash[i]);\n", ""),
    ("SetKeywords: inherited count", _X, "    m_keywordCount += inherited;\n", ""),
    ("LookUp: no table", _X, "    if (m_keywords == 0) {\n        Error(kInvalidKeyword);\n    }\n    unsigned int h", "    unsigned int h"),
    ("LookUp: case", _X, "!= toupper((signed char)*b)", "!= (int)(signed char)*b"),
    ("LookUp: miss", _X, "    m_type = kError;\n}", "    m_type = 0;\n}"),
    ("LookUp: wrap", _X, "        h++;\n        if (h >= kHashSize) {", "        h++;\n        if (h >= kHashSize - 1) {"),
    ("LookUp: prefix accepted", _X, "            if (*a == 0) {\n                m_type", "            if (*a == 0 || a[1] == 0) {\n                m_type"),
    ("Next: pushed back kept", _X, "        m_pushedBack = 0;\n        return type;", "        return type;"),
    ("Next: pushed back ignored", _X, "    if (m_pushedBack != 0) {", "    if (0) {"),
    ("Next: type not cleared", _X, "    m_type = 0;\n    if (!m_stream->SkipWhitespace())", "    if (!m_stream->SkipWhitespace())"),
    ("Next: first scale", _X, "float scale = 0.1f;", "float scale = 0.2f;"),
    ("Next: error char silent", _X, "            case kError:\n                Error(kSyntaxError);\n                break;", "            case kError:\n                break;"),
    ("Next: out of range silent", _X, "                Error(kSyntaxError);\n                m_type = kError;\n                return kError;", "                m_type = kError;\n                return kError;"),
    ("Next: string length kept", _X, "                state = kInString;\n                m_stringLength = 0;", "                state = kInString;"),
    ("Next: '.' start keeps real", _X, "                state = kFraction;\n                m_real = 0;\n                break;\n            case kInt:", "                state = kFraction;\n                break;\n            case kInt:"),
    ("Next: first digit", _X, "                state = kDecimal;\n                m_int = (int)(c - '0');\n                break;", "                state = kDecimal;\n                m_int = (int)(c - '1');\n                break;"),
    ("Next: zero keeps int", _X, "                state = kAfterZero;\n                m_int = 0;\n                break;\n            case kStartMinus:", "                state = kAfterZero;\n                break;\n            case kStartMinus:"),
    ("Next: minus keeps int", _X, "                state = kAfterMinus;\n                m_int = 0;\n", "                state = kAfterMinus;\n"),
    ("Next: minus ignored", _X, "negative = 1;", "negative = 0;"),
    ("Next: identifier case", _X, "m_identifier[0] = (char)toupper((int)c);", "m_identifier[0] = (char)c;"),
    ("Next: hex marker", _X, "c == 'x' || c == 'X'", "c == 'x'"),
    ("Next: decimal base", _X, "(unsigned int)m_int * 10", "(unsigned int)m_int * 8"),
    ("Next: integer part unsigned", _X, "m_real = (float)m_int;", "m_real = (float)(unsigned int)m_int;"),
    ("Next: decimal end not unread", _X, _UNREAD_DEC, _UNREAD_DEC.replace("                s->m_cur--;\n", "", 1)),
    ("Next: decimal sign", _X, _UNREAD_DEC, _UNREAD_DEC.replace("                if (negative) {\n                    m_int = (int)(0u - (unsigned int)m_int);\n                }\n", "", 1)),
    ("Next: hex digits", _X, "((unsigned int)m_int << 4) + c - '0'", "((unsigned int)m_int << 3) + c - '0'"),
    ("Next: hex letters", _X, "((unsigned int)m_int << 4) + upper - 0x37", "((unsigned int)m_int << 4) + upper - 0x36"),
    ("Next: hex range", _X, "upper >= 'A' && upper <= 'F'", "upper >= 'A' && upper <= 'G'"),
    ("Next: fraction in single precision", _X, "m_real = (float)((double)(int)(c - '0') * (double)scale + (double)m_real);", "m_real = (float)(int)(c - '0') * scale + m_real;"),
    ("Next: scale constant", _X, "(double)scale * (double)0.1f", "(double)scale * 0.1"),
    ("Next: second dot", _X, "                if (c == '.') {\n                    Error(kSyntaxError);\n                }\n", ""),
    ("Next: fraction sign", _X, "                if (negative) {\n                    m_real = -m_real;\n                }\n                m_type = kReal;\n                return kReal;\n            }\n            break;", "                m_type = kReal;\n                return kReal;\n            }\n            break;"),
    ("Next: minus digit range", _X, "if (c >= '1' && c <= '9') {\n                m_int", "if (c >= '0' && c <= '9') {\n                m_int"),
    ("Next: lone minus type", _X, "return kMinus;", "m_type = kMinus; return kMinus;"),
    ("Next: lone minus unread", _X, "                s->m_cur--;\n                return kMinus;", "                return kMinus;"),
    ("Next: identifier limit", _X, "if (length >= 0x3F) {", "if (length >= 0x40) {"),
    ("Next: identifier underscore", _X, " || upper == '_')", ")"),
    ("Next: identifier digits", _X, "(upper >= '0' && upper <= '9') || ", ""),
    ("Next: comment end", _X, "if (!m_stream->SkipLine() || !m_stream->SkipWhitespace()) {", "if (!m_stream->SkipLine()) {"),
    ("Next: slash unread", _X, "                s->m_cur--;\n                return m_type;\n            }\n            break;\n        case kInString:", "                return m_type;\n            }\n            break;\n        case kInString:"),
    ("Next: string terminator", _X, "                m_string[m_stringLength] = 0;\n                return kString;", "                return kString;"),
    ("Next: escapes always on", _X, "c == '\\\\' && m_escapes != 0", "c == '\\\\'"),
    ("Next: string limit", _X, "if (m_stringLength >= 0xFF) {\n                Error(kInvalidString);\n            }\n            m_string[m_stringLength] = (char)c;", "m_string[m_stringLength] = (char)c;"),
    ("Next: escape limit", _X, "if (m_stringLength >= 0xFF) {\n                Error(kInvalidString);\n            }\n            switch (c) {", "switch (c) {"),
    ("Next: escape n", _X, "case 'n': case 'N': m_string[m_stringLength] = '\\n';", "case 'n': case 'N': m_string[m_stringLength] = '\\r';"),
    ("Next: escape capital T", _X, "case 't': case 'T':", "case 't':"),
    ("Next: escape quote", _X, "case '\"': m_string[m_stringLength] = '\"'; break;\n", ""),
    ("Next: bad escape silent", _X, "default: Error(kInvalidString); break;", "default: break;"),
    ("Next: escape length", _X, "            m_stringLength++;\n            state = kInString;", "            state = kInString;"),
    ("End: hex sign", _X, "    case kDecimal:\n    case kHex:\n        if (negative) {\n            m_int = (int)(0u - (unsigned int)m_int);\n        }\n", "    case kDecimal:\n    case kHex:\n"),
    ("End: real sign", _X, "    case kFraction:\n        if (negative) {\n            m_real = -m_real;\n        }\n", "    case kFraction:\n"),
    ("End: lone minus silent", _X, "    case kAfterMinus:\n    case kAfterSlash:\n        Error(kSyntaxError);\n", "    case kAfterMinus:\n    case kAfterSlash:\n"),
    ("End: identifier", _X, "    case kIdentifier:\n        m_identifier[length] = 0;\n        LookUpIdentifier();\n", "    case kIdentifier:\n        LookUpIdentifier();\n"),
    ("End: string code", _X, "    case kAfterBackslash:\n        Error(kInvalidString);", "    case kAfterBackslash:\n        Error(kSyntaxError);"),
    ("End: zero", _X, "    case kAfterZero:\n        m_type = kInt;\n        return kInt;\n    case kDecimal:", "    case kDecimal:"),
    ("PutInt: type", _X, "    m_type = kInt;\n    AtFormatInt(value, text);", "    AtFormatInt(value, text);"),
    ("PutInt: value", _X, "    m_int = value;\n", ""),
    ("PutReal: value", _X, "    m_real = value;\n", ""),
    ("PutString: limit", _X, "strncpy(m_string, text, 0xFF);\n    m_string[0xFF] = 0;\n    m_type = kString;", "strncpy(m_string, text, 0xFE);\n    m_string[0xFE] = 0;\n    m_type = kString;"),
    ("PutString: closing quote", _X, "    PutText(m_stream, m_string);\n    m_stream->Put('\"');\n", "    PutText(m_stream, m_string);\n"),
    ("PutComment: prefix", _X, "    m_stream->Put(' ');\n    strncpy", "    strncpy"),
    ("PutComment: limit", _X, "strncpy(m_string, text, 0xFF);\n    m_string[0xFF] = 0;\n    PutText", "strncpy(m_string, text, 0x10);\n    m_string[0x10] = 0;\n    PutText"),
    ("PutKeyword: type", _X, "            m_type = id;\n", ""),
    ("PutKeyword: unknown silent", _X, "    Error(kInvalidKeyword);\n}\n\nvoid AtIoTextTokenizer::PutRaw", "}\n\nvoid AtIoTextTokenizer::PutRaw"),
    ("Put punctuation: type", _X, "void AtIoTextTokenizer::PutAt() { m_type = kAt;", "void AtIoTextTokenizer::PutAt() { m_type = kTilde;"),
    ("Put punctuation: character", _X, "m_stream->Put('@');", "m_stream->Put('#');"),
    ("PutNewline: CR", _X, "    m_stream->Put('\\r');\n", ""),
    ("PutTabs: character", _X, "void AtIoTextTokenizer::PutTabs(unsigned int count) {\n    for (; count != 0; count--) {\n        m_stream->Put('\\t');", "void AtIoTextTokenizer::PutTabs(unsigned int count) {\n    for (; count != 0; count--) {\n        m_stream->Put(' ');"),
    ("PutSpaces: count", _X, "void AtIoTextTokenizer::PutSpaces(unsigned int count) {\n    for (; count != 0; count--) {", "void AtIoTextTokenizer::PutSpaces(unsigned int count) {\n    for (count++; count != 0; count--) {"),
    ("GetIdentifier", _X, "return m_identifier;", "return m_string;"),
]


LVL_MUTANTS = [
    ('Copy8: null', 'GmLevelManager.cpp', '    if (from == 0) {\n        to[0] = 0;\n        return;\n    }\n', ''),
    ('Copy8: length', 'GmLevelManager.cpp', 'for (int i = 0; i < 8; i++) {\n        to[i] = from[i];', 'for (int i = 0; i < 7; i++) {\n        to[i] = from[i];'),
    ('Name assign: nothing copied', 'GmLevelManager.cpp', '    AtCopyName8(m_name, other.m_name);\n', ''),
    ('Name SetName: nothing copied', 'GmLevelManager.cpp', '    AtCopyName8(m_name, name);\n', ''),
    ('Resize: zero keeps count', 'GmLevelManager.cpp', '        m_items = 0;\n        m_count = 0;\n        return;', '        m_items = 0;\n        return;'),
    ('Resize: zero keeps items', 'GmLevelManager.cpp', '        delete[] m_items;\n        m_items = 0;\n        m_count = 0;', '        m_items = 0;\n        m_count = 0;'),
    ('Resize: same size reallocates', 'GmLevelManager.cpp', '    if (count == m_count) {\n        return;\n    }\n', ''),
    ('Resize: heap check', 'GmLevelManager.cpp', '    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    GmLevelName*', '    AtPushHeap(s_heap);\n    GmLevelName*'),
    ('Resize: heap not pushed', 'GmLevelManager.cpp', '    AtPushHeap(s_heap);\n    GmLevelName*', '    GmLevelName*'),
    ('Resize: names lost', 'GmLevelManager.cpp', '            items[i] = m_items[i];\n', ''),
    ('Resize: keep too few', 'GmLevelManager.cpp', 'count < m_count ? count : m_count', 'count < m_count ? count : (m_count ? m_count - 1 : 0)'),
    ('Resize: old kept', 'GmLevelManager.cpp', '            items[i] = m_items[i];\n        }\n        delete[] m_items;', '            items[i] = m_items[i];\n        }'),
    ('Resize: count unset', 'GmLevelManager.cpp', '    m_items = items;\n    m_count = count;', '    m_items = items;'),
    ('Reset: current index', 'GmLevelManager.cpp', '    m_currentIndex = -1;\n', ''),
    ('Reset: current name', 'GmLevelManager.cpp', '        m_currentName[i] = 0;\n', ''),
    ('Reset: previous index', 'GmLevelManager.cpp', '    m_previousIndex = -1;\n', ''),
    ('Reset: level count', 'GmLevelManager.cpp', '    m_levelCount = 0;\n', ''),
    ('Reset: 5C', 'GmLevelManager.cpp', '    m_unknown05C = 0;\n', ''),
    ('Reset: kinds', 'GmLevelManager.cpp', '    m_kinds = 0;\n', ''),
    ('Reset: load items', 'GmLevelManager.cpp', '    m_loadItems = 0;\n', ''),
    ('Reset: 60', 'GmLevelManager.cpp', '    m_unknown060 = 0;\n', ''),
    ('Reset: 20', 'GmLevelManager.cpp', '    m_unknown020 = 1;\n', '    m_unknown020 = 0;\n'),
    ('Reset: start point', 'GmLevelManager.cpp', 'AtCopyName("Start", m_startPoint);', 'AtCopyName("Stop", m_startPoint);'),
    ('Reset: 2C', 'GmLevelManager.cpp', '        m_unknown02C[i] = 0;\n', ''),
    ('Reset: loaded', 'GmLevelManager.cpp', '    m_loaded = 0;\n}', '}'),
    ('Load: no unload first', 'GmLevelManager.cpp', '    if (m_loaded != 0) {\n        Unload();\n    }\n', ''),
    ('Load: open flags', 'GmLevelManager.cpp', 'file.Open(path, 2, 0)', 'file.Open(path, 4, 0)'),
    ('Load: message', 'GmLevelManager.cpp', '"  \\n return code :"', '" return code :"'),
    ('Load: message number', 'GmLevelManager.cpp', 'AtFormatInt(status, number);', 'AtFormatInt(0, number);'),
    ('Load: fatal argument', 'GmLevelManager.cpp', 'AtFatal(message, 0, 0, 0);', 'AtFatal(message, 1, 0, 0);'),
    ('Load: first keyword id', 'GmLevelManager.cpp', '{kLevels, "Levels"},', '{kLevels, "Level_s"},'),
    ('Load: other token stops', 'GmLevelManager.cpp', '        if (token != kLevels) {\n            continue;\n        }\n', ''),
    ('Load: heap for names', 'GmLevelManager.cpp', '        GmLevelNameArray::s_heap = s_heap;\n', ''),
    ('Load: names not sized', 'GmLevelManager.cpp', '        m_names.Resize((unsigned int)m_levelCount);\n', ''),
    # Not used: swapping the order of two same-sized allocations is an equivalent mutant; only the addresses differ.
    ('Load: table size', 'GmLevelManager.cpp', '(unsigned int)count * elementSize', '(unsigned int)count * elementSize * 2'),
    ('Load: table heap check', 'GmLevelManager.cpp', '    if (GmLevelManager::s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n', ''),
    ('Load: table heap', 'GmLevelManager.cpp', '    AtPushHeap(GmLevelManager::s_heap);\n    void* table', '    void* table'),
    ('Load: level token', 'GmLevelManager.cpp', 't->ExpectToken(kLevel);', 't->ExpectToken(kLevels);'),
    ('Load: name not set', 'GmLevelManager.cpp', '            m_names.m_items[i].SetName(name);\n', ''),
    ('Load: description unset', 'GmLevelManager.cpp', '            m_descriptions[i] = 0;\n            switch', '            switch'),
    ('Load: movie kind', 'GmLevelManager.cpp', 'm_kinds[i] = kMovie;', 'm_kinds[i] = kNormal;'),
    ('Load: normal kind', 'GmLevelManager.cpp', 'm_kinds[i] = kNormal;', 'm_kinds[i] = kHidden;'),
    ('Load: hidden kind', 'GmLevelManager.cpp', 'm_kinds[i] = kHidden;', 'm_kinds[i] = kNormal;'),
    ('Load: non-selectable kind', 'GmLevelManager.cpp', 'm_kinds[i] = kNonSelectable;', 'm_kinds[i] = kNormal;'),
    ('Load: designer cinema acted on', 'GmLevelManager.cpp', '            case kFmvCinema:', '            case kDesignerCinema: case kFmvCinema:'),
    ('Load: tool cinema acted on', 'GmLevelManager.cpp', '            case kFmvCinema:', '            case kToolCinema: case kFmvCinema:'),
    ('Load: conversation cinema acted on', 'GmLevelManager.cpp', '            case kFmvCinema:', '            case kConversationCinema: case kFmvCinema:'),
    ('Load: type acted on', 'GmLevelManager.cpp', '            case kFmvCinema:', '            case kType: case kFmvCinema:'),
    ('Load: load items', 'GmLevelManager.cpp', 'm_loadItems[i] = t->ExpectInt();', 't->ExpectInt();'),
    ('Load: description', 'GmLevelManager.cpp', 't->ExpectStringCopy(&m_descriptions[i]);', 't->ExpectString();'),
    ('Load: token not handed back', 'GmLevelManager.cpp', '                t->m_pushedBack = 1;\n', ''),
    ('Load: end brace', 'GmLevelManager.cpp', '        t->ExpectRightCurly();\n', ''),
    ('Load: never marked', 'GmLevelManager.cpp', '    m_loaded = 1;\n', ''),
    ('Unload: content kept', 'GmLevelManager.cpp', '        ReleaseLevelData();\n', ''),
    ('Unload: ignores loaded', 'GmLevelManager.cpp', 'void GmLevelManager::Unload() {\n    if (m_loaded != 0) {', 'void GmLevelManager::Unload() {\n    if (1) {'),
    ('Unload: description kept', 'GmLevelManager.cpp', '                operator delete(m_descriptions[i]);\n', ''),
    ('Unload: description table kept', 'GmLevelManager.cpp', '        operator delete(m_descriptions);\n', ''),
    ('Unload: kinds kept', 'GmLevelManager.cpp', '            operator delete(m_kinds);\n', ''),
    ('Unload: load items kept', 'GmLevelManager.cpp', '            operator delete(m_loadItems);\n', ''),
    ('Unload: no reset', 'GmLevelManager.cpp', '    }\n    Reset();\n}\n\nGmLevelManager::GmLevelManager', '    }\n}\n\nGmLevelManager::GmLevelManager'),
]


MNT_MUTANTS = [
    ('Notify mounted: nothing', 'AtIoMountManager.cpp', '    fs->m_header->Unknown4();\n', ''),
    ('Notify unmounted: nothing', 'AtIoMountManager.cpp', '    fs->m_header->Unknown5();\n', ''),
    ('Delete: not deleted', 'AtIoMountManager.cpp', '        delete fs;\n', ''),
    ('Space heap', 'AtIoMountManager.cpp', 'AtIoSpaceManager::SetHeap(s_heap);', ''),
    ('Space ctor: instance', 'AtIoMountManager.cpp', '    s_instance = this;\n    m_unknown04 = 0;', '    m_unknown04 = 0;'),
    ('Space dtor: instance', 'AtIoMountManager.cpp', '    s_instance = 0;\n}\n\nvoid AtIoSpaceManager::Clear', '}\n\nvoid AtIoSpaceManager::Clear'),
    ('Space Clear: field', 'AtIoMountManager.cpp', 'void AtIoSpaceManager::Clear() {\n    m_unknown04 = 0;\n', 'void AtIoSpaceManager::Clear() {\n'),
    ('Space Clear: device', 'AtIoMountManager.cpp', 'void AtIoSpaceManager::Clear() {\n    m_unknown04 = 0;\n    m_device = 0;\n', 'void AtIoSpaceManager::Clear() {\n    m_unknown04 = 0;\n'),
    ('Space Init: device not kept', 'AtIoMountManager.cpp', '    m_device = *(AtIoContextDevice**)AtIoMountManager::s_instance->m_context;\n    return m_device->Unknown1();', '    return (*(AtIoContextDevice**)AtIoMountManager::s_instance->m_context)->Unknown1();'),
    ('Space Init: result', 'AtIoMountManager.cpp', 'return m_device->Unknown1();', 'm_device->Unknown1(); return 0;'),
    ('Ctor: instance', 'AtIoMountManager.cpp', 'AtIoMountManager::AtIoMountManager() {\n    s_instance = this;\n', 'AtIoMountManager::AtIoMountManager() {\n'),
    ('Ctor: root', 'AtIoMountManager.cpp', '    s_instance = this;\n    m_device = 0;\n    m_root = 0;\n', '    s_instance = this;\n    m_device = 0;\n'),
    ('Dtor: instance', 'AtIoMountManager.cpp', '    s_instance = 0;\n    AtIoMountManager::Shutdown();', '    AtIoMountManager::Shutdown();'),
    ('Dtor: no shutdown', 'AtIoMountManager.cpp', '    s_instance = 0;\n    AtIoMountManager::Shutdown();', '    s_instance = 0;'),
    ('Init: no shutdown first', 'AtIoMountManager.cpp', '    if (s_heap != 0) {\n        Shutdown();\n    }\n', ''),
    ('Init: owner', 'AtIoMountManager.cpp', '    m_owner = params->m_owner;\n', ''),
    ('Init: heap check', 'AtIoMountManager.cpp', '    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n', ''),
    ('Init: heap not pushed', 'AtIoMountManager.cpp', '    AtPushHeap(s_heap);\n', ''),
    ('Init: space not initialised', 'AtIoMountManager.cpp', '    m_space->Init();\n', ''),
    ('Init: no file systems', 'AtIoMountManager.cpp', '    m_space->Init();\n    CreateFileSystems();', '    m_space->Init();'),
    ('Shutdown: space kept', 'AtIoMountManager.cpp', '        m_space->Clear();\n        delete m_space;\n        m_space = 0;', '        m_space = 0;'),
    ('Shutdown: device kept', 'AtIoMountManager.cpp', '            m_factory->Destroy(m_device);\n', ''),
    ('Shutdown: root kept', 'AtIoMountManager.cpp', '            m_factory->Destroy(m_root);\n', ''),
    ('Shutdown: 28 kept', 'AtIoMountManager.cpp', '            m_factory->Destroy(m_unknown28);\n', ''),
    ('Shutdown: destroy without factory check', 'AtIoMountManager.cpp', '    if (m_factory != 0) {\n        if (m_device != 0)', '    if (1) {\n        if (m_device != 0)'),
    ('Shutdown: heap', 'AtIoMountManager.cpp', '    m_factory = 0;\n    s_heap = 0;\n    m_mounted = 0;\n    m_side = 0;\n    m_region[0] = 0;\n    m_space = 0;\n}\n\nvoid AtIoMountManager::CreateFileSystems', '    m_factory = 0;\n    m_mounted = 0;\n    m_side = 0;\n    m_region[0] = 0;\n    m_space = 0;\n}\n\nvoid AtIoMountManager::CreateFileSystems'),
    ('Shutdown: mounted', 'AtIoMountManager.cpp', '    s_heap = 0;\n    m_mounted = 0;\n    m_side = 0;\n    m_region[0] = 0;\n    m_space = 0;\n}\n\nvoid AtIoMountManager::CreateFileSystems', '    s_heap = 0;\n    m_side = 0;\n    m_region[0] = 0;\n    m_space = 0;\n}\n\nvoid AtIoMountManager::CreateFileSystems'),
    ('Shutdown: region', 'AtIoMountManager.cpp', '    m_side = 0;\n    m_region[0] = 0;\n    m_space = 0;\n}\n\nvoid AtIoMountManager::CreateFileSystems', '    m_side = 0;\n    m_space = 0;\n}\n\nvoid AtIoMountManager::CreateFileSystems'),
    ('CreateFileSystems: kinds', 'AtIoMountManager.cpp', 'm_device = m_factory->Create(AtIoFileSystemFactory::kDevice, 0, 1);', 'm_device = m_factory->Create(AtIoFileSystemFactory::kRoot, 0, 1);'),
    ('CreateFileSystems: argument', 'AtIoMountManager.cpp', 'm_root = m_factory->Create(AtIoFileSystemFactory::kRoot, 0, 1);', 'm_root = m_factory->Create(AtIoFileSystemFactory::kRoot, 0, 0);'),
    ('CreateFileSystems: not pushed', 'AtIoMountManager.cpp', '    m_context->Push(m_device);\n', ''),
    ('SetRegion: null', 'AtIoMountManager.cpp', '    if (name == 0) {\n        m_region[0] = 0;\n    } else {\n', '    {\n'),
    ('SetRegion: length', 'AtIoMountManager.cpp', 'for (int i = 0; i < 8; i++) {\n            m_region[i]', 'for (int i = 0; i < 7; i++) {\n            m_region[i]'),
    ('SetRegion: device not told', 'AtIoMountManager.cpp', '    m_device->SetRoot(root);\n', ''),
    ('WriteList: length', 'AtIoMountManager.cpp', 'm_listFile.Write(text, (unsigned int)strlen(text));', 'm_listFile.Write(text, (unsigned int)strlen(text) - 1);'),
    ('Mount: separator', 'AtIoMountManager.cpp', 'strcat(prefix, "\\\\");', 'strcat(prefix, "/");'),
    ('Mount: separator when empty', 'AtIoMountManager.cpp', '    if (strlen(prefix) != 0) {\n        strcat', '    {\n        strcat'),
    ('Mount: base path', 'AtIoMountManager.cpp', 'strcpy(path, "JamFiles\\\\XBox\\\\");', 'strcpy(path, "JamFiles\\\\Xbox\\\\");'),
    ('Mount: limits', 'AtIoMountManager.cpp', 'm_mounted = AtIoJamFileSystem::Create(path, 4, 2);', 'm_mounted = AtIoJamFileSystem::Create(path, 3, 2);'),
    ('Mount: flags', 'AtIoMountManager.cpp', 'm_mounted = AtIoJamFileSystem::Create(path, 4, 2);', 'm_mounted = AtIoJamFileSystem::Create(path, 4, 0);'),
    ('Mount: no check', 'AtIoMountManager.cpp', '    if (m_device->IsFileMissing(path) == 0) {', '    {'),
    ('Mount: not notified', 'AtIoMountManager.cpp', '        AtIoNotifyMounted(m_mounted);\n        m_context->Push(m_mounted);\n    }\n}\n\nvoid AtIoMountManager::MountPath', '        m_context->Push(m_mounted);\n    }\n}\n\nvoid AtIoMountManager::MountPath'),
    ('Mount: not pushed', 'AtIoMountManager.cpp', '        AtIoNotifyMounted(m_mounted);\n        m_context->Push(m_mounted);\n    }\n}\n\nvoid AtIoMountManager::MountPath', '        AtIoNotifyMounted(m_mounted);\n    }\n}\n\nvoid AtIoMountManager::MountPath'),
    ('MountPath: limit', 'AtIoMountManager.cpp', 'if ((int)(length + 4) >= 0x100) {\n        return;\n    }\n    char full[0x100];\n    strcpy(full, path);\n    strcpy(full + length, ".JAM");\n    if (m_device->IsFileMissing(full) == 0) {\n        m_mounted', 'if ((int)(length + 4) >= 0xFF) {\n        return;\n    }\n    char full[0x100];\n    strcpy(full, path);\n    strcpy(full + length, ".JAM");\n    if (m_device->IsFileMissing(full) == 0) {\n        m_mounted'),
    ('MountPath: no check', 'AtIoMountManager.cpp', '    if (m_device->IsFileMissing(full) == 0) {\n        m_mounted', '    {\n        m_mounted'),
    ('MountPath: not pushed', 'AtIoMountManager.cpp', '        AtIoNotifyMounted(m_mounted);\n        m_context->Push(m_mounted);\n    }\n}\n\nvoid AtIoMountManager::Unmount', '        AtIoNotifyMounted(m_mounted);\n    }\n}\n\nvoid AtIoMountManager::Unmount'),
    ('Unmount: not popped', 'AtIoMountManager.cpp', '        m_context->Pop();\n        AtIoNotifyUnmounted(m_mounted);', '        AtIoNotifyUnmounted(m_mounted);'),
    ('Unmount: not notified', 'AtIoMountManager.cpp', '        AtIoNotifyUnmounted(m_mounted);\n', ''),
    ('Unmount: not deleted', 'AtIoMountManager.cpp', '        AtIoDeleteObject(m_mounted);\n', ''),
    ('Unmount: kept', 'AtIoMountManager.cpp', '        m_mounted = 0;\n    }\n}\n\nvoid AtIoMountManager::DropSideArchive', '    }\n}\n\nvoid AtIoMountManager::DropSideArchive'),
    ('Drop: always popped', 'AtIoMountManager.cpp', '        if (m_context->Top() == m_side) {\n            m_context->Pop();\n        }\n', '        m_context->Pop();\n'),
    ('Drop: never popped', 'AtIoMountManager.cpp', '        if (m_context->Top() == m_side) {\n            m_context->Pop();\n        }\n', ''),
    ('Drop: not notified', 'AtIoMountManager.cpp', '        AtIoNotifyUnmounted(m_side);\n', ''),
    ('Drop: not deleted', 'AtIoMountManager.cpp', '        AtIoDeleteObject(m_side);\n', ''),
    ('Swap: previous kept', 'AtIoMountManager.cpp', '    DropSideArchive();\n', ''),
    ('Swap: limit', 'AtIoMountManager.cpp', 'if ((int)(length + 4) >= 0x100) {\n        return;\n    }\n    char full[0x100];\n    strcpy(full, path);\n    strcpy(full + length, ".JAM");\n    if (m_device->IsFileMissing(full) == 0) {\n        m_side', 'if ((int)(length + 4) >= 0x101) {\n        return;\n    }\n    char full[0x101];\n    strcpy(full, path);\n    strcpy(full + length, ".JAM");\n    if (m_device->IsFileMissing(full) == 0) {\n        m_side'),
    ('Swap: no check', 'AtIoMountManager.cpp', '    if (m_device->IsFileMissing(full) == 0) {\n        m_side', '    {\n        m_side'),
    ('Swap: not notified', 'AtIoMountManager.cpp', '        AtIoNotifyMounted(m_side);\n', ''),
    ('OpenList: flags', 'AtIoMountManager.cpp', 'm_listFile.OpenOn(m_root, name, 0x24)', 'm_listFile.OpenOn(m_root, name, 0x21)'),
    ('OpenList: pointer set on failure', 'AtIoMountManager.cpp', 'if (m_listFile.OpenOn(m_root, name, 0x24) == 0) {', 'm_listFile.OpenOn(m_root, name, 0x24); {'),
    ('OpenList: pointer unset', 'AtIoMountManager.cpp', '        m_device->m_unknown6B4 = &m_listFile;\n', ''),
    ('OpenList: path kept', 'AtIoMountManager.cpp', '    m_root->m_path.Pop(0);\n', ''),
    ('CloseList: pointer kept', 'AtIoMountManager.cpp', '    m_device->m_unknown6B4 = 0;\n    m_listFile.Close();', '    m_listFile.Close();'),
    ('CloseList: file kept', 'AtIoMountManager.cpp', '    m_device->m_unknown6B4 = 0;\n    m_listFile.Close();', '    m_device->m_unknown6B4 = 0;'),
]


RMB_MUTANTS = [
    ('Size: last entry', 'GmNameMap.cpp', '10939, 50159, 100237}', '10939, 50159, 100000}'),
    ('Size: comparison', 'GmNameMap.cpp', 'if (count < kSizes[i]) {', 'if (count <= kSizes[i]) {'),
    ('Size: past the table', 'GmNameMap.cpp', '    return count;\n}\n\nGmNameMap::Entry* NewEntries', '    return kSizes[0];\n}\n\nGmNameMap::Entry* NewEntries'),
    ('New: heap check', 'GmNameMap.cpp', '    if (GmNameMap::s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n', ''),
    ('New: heap not pushed', 'GmNameMap.cpp', '    AtPushHeap(GmNameMap::s_heap);\n', ''),
    ('New: names not cleared', 'GmNameMap.cpp', '            entries[i].m_name[0] = 0;\n', ''),
    ('New: values not cleared', 'GmNameMap.cpp', '            entries[i].m_value = 0;\n', ''),
    ('Duplicate: message', 'GmNameMap.cpp', ': Duplicate name encountered', ': Duplicate name'),
    ('Equal: length', 'GmNameMap.cpp', 'for (int i = 0; i < 7; i++) {\n        if (a[i] != b[i])', 'for (int i = 0; i < 6; i++) {\n        if (a[i] != b[i])'),
    ('Equal: terminator', 'GmNameMap.cpp', '        if (a[i] == 0) {\n            return 1;\n        }\n', ''),
    ('Equal: last byte', 'GmNameMap.cpp', 'return a[7] == b[7];', 'return 1;'),
    ('Detach: link', 'GmNameMap.cpp', '            *link = m->m_next;\n', ''),
    ('Detach: group kept', 'GmNameMap.cpp', '            m->m_group = 0;\n', ''),
    ('Detach: parent not told', 'GmNameMap.cpp', '            if (m_first == 0 && m_database != 0) {\n                m_database->Release(this);\n            }\n', ''),
    ('Detach: walk', 'GmNameMap.cpp', '        link = &m->m_next;\n', '        return;\n'),
    ('Clear: no detach', 'GmNameMap.cpp', '    if (m_group != 0) {\n        m_group->Detach(this);\n    }\n', ''),
    ('Clear: entries kept', 'GmNameMap.cpp', '        operator delete(m_entries);\n', ''),
    ('Clear: size kept', 'GmNameMap.cpp', '    m_entries = 0;\n    m_size = 0;\n    m_next = 0;\n    m_group = 0;\n}\n\nvoid GmNameMap::GetName', '    m_entries = 0;\n    m_next = 0;\n    m_group = 0;\n}\n\nvoid GmNameMap::GetName'),
    ('Clear: next kept', 'GmNameMap.cpp', '    m_size = 0;\n    m_next = 0;\n    m_group = 0;\n}\n\nvoid GmNameMap::GetName', '    m_size = 0;\n    m_group = 0;\n}\n\nvoid GmNameMap::GetName'),
    ('Hash: start', 'GmNameMap.cpp', 'unsigned int shift = 7;', 'unsigned int shift = 6;'),
    ('Hash: step', 'GmNameMap.cpp', 'shift = (shift + 7) & 0x1F;', 'shift = (shift + 6) & 0x1F;'),
    # Not used: 'Hash: wrap'. x86 masks a shift count to five bits anyway, so the mask changes nothing here. It stays in the source for hosts where it does.
    ('Hash: length', 'GmNameMap.cpp', 'for (int i = 1; i < 8 && name[i] != 0; i++) {', 'for (int i = 1; i < 7 && name[i] != 0; i++) {'),
    ('Hash: unsigned bytes', 'GmNameMap.cpp', '(unsigned int)(int)(signed char)name[i] << shift', '(unsigned int)(unsigned char)name[i] << shift'),
    ('Hash: first byte unsigned', 'GmNameMap.cpp', '(unsigned int)(int)(signed char)name[0]', '(unsigned int)(unsigned char)name[0]'),
    ('Grow: same size', 'GmNameMap.cpp', 'm_size = SizeFor(oldSize + 1);', 'm_size = SizeFor(oldSize + 2);'),
    ('Grow: old kept', 'GmNameMap.cpp', '    operator delete(old);\n', ''),
    ('Grow: entries lost', 'GmNameMap.cpp', '            Insert(old[i].m_name, old[i].m_value);\n', ''),
    ('Init: no clear', 'GmNameMap.cpp', '    if (m_entries != 0) {\n        Clear();\n    }\n', ''),
    ('Insert: value', 'GmNameMap.cpp', '                AtCopyName8(m_entries[i].m_name, name);\n                m_entries[i].m_value = value;', '                AtCopyName8(m_entries[i].m_name, name);\n                m_entries[i].m_value = name == 0 ? value : (void*)name;'),
    ('Insert: duplicate silent', 'GmNameMap.cpp', '            if (AtNamesEqual(m_entries[i].m_name, name)) {\n                ReportDuplicate(name);\n            }\n            i++;\n            if (i >= m_size) {\n                i = 0;\n            }\n        } while (i != home);\n        Grow();\n    }\n}\n\nvoid GmNameMap::Insert(const char* name, void* value, unsigned int* index)', '            i++;\n            if (i >= m_size) {\n                i = 0;\n            }\n        } while (i != home);\n        Grow();\n    }\n}\n\nvoid GmNameMap::Insert(const char* name, void* value, unsigned int* index)'),
    ('Insert index: not written', 'GmNameMap.cpp', '                    *index = i;\n', ''),
    ('Insert index: null not checked', 'GmNameMap.cpp', '                if (index != 0) {\n                    *index = i;\n                }\n', '                *index = i;\n'),
    ('Insert index: duplicate silent', 'GmNameMap.cpp', '            if (AtNamesEqual(m_entries[i].m_name, name)) {\n                ReportDuplicate(name);\n            }\n            i++;\n            if (i >= m_size) {\n                i = 0;\n            }\n        } while (i != home);\n        Grow();\n    }\n}\n\nvoid GmNameMap::Remove', '            i++;\n            if (i >= m_size) {\n                i = 0;\n            }\n        } while (i != home);\n        Grow();\n    }\n}\n\nvoid GmNameMap::Remove'),
    ('Remove: value kept', 'GmNameMap.cpp', '    p->m_value = 0;\n    p++;\n', '    p++;\n'),
    ('Remove: cluster not rehashed', 'GmNameMap.cpp', '    while (p->m_value != 0) {\n        void* value = p->m_value;\n        p->m_value = 0;\n        Insert(p->m_name, value);\n        p++;\n        if (p >= end) {\n            p = m_entries;\n        }\n    }\n', ''),
    ('Remove: free slot ends the search first', 'GmNameMap.cpp', '    while (!AtNamesEqual(p->m_name, name)) {\n        if (p->m_value == 0) {', '    while (p->m_value == 0 || !AtNamesEqual(p->m_name, name)) {\n        if (p->m_value == 0) {'),
    ('Find: index not written', 'GmNameMap.cpp', '        *index = i;\n    }\n    return', '    }\n    return'),
    ('Find: free slot not checked', 'GmNameMap.cpp', 'void* GmNameMap::Find(const char* name) {\n    const unsigned int home = HashOf(name);\n    unsigned int i = home;\n    while (!AtNamesEqual(m_entries[i].m_name, name)) {\n        if (m_entries[i].m_value == 0) {\n            return 0;\n        }\n', 'void* GmNameMap::Find(const char* name) {\n    const unsigned int home = HashOf(name);\n    unsigned int i = home;\n    while (!AtNamesEqual(m_entries[i].m_name, name)) {\n'),
    ('Find: wrap', 'GmNameMap.cpp', 'void* GmNameMap::Find(const char* name) {\n    const unsigned int home = HashOf(name);\n    unsigned int i = home;\n    while (!AtNamesEqual(m_entries[i].m_name, name)) {\n        if (m_entries[i].m_value == 0) {\n            return 0;\n        }\n        i++;\n        if (i >= m_size) {\n            i = 0;\n        }\n        if (i == home) {\n            return 0;\n        }\n', 'void* GmNameMap::Find(const char* name) {\n    const unsigned int home = HashOf(name);\n    unsigned int i = home;\n    while (!AtNamesEqual(m_entries[i].m_name, name)) {\n        if (m_entries[i].m_value == 0) {\n            return 0;\n        }\n        i++;\n        if (i >= m_size) {\n            return 0;\n        }\n        if (i == home) {\n            return 0;\n        }\n'),
    ('Keyword: left', 'GmRumble.cpp', '{kLeft, "Left"}', '{kLeft, "Leftt"}'),
    ('Frame: left', 'GmRumble.cpp', '    m_left = 0;\n', ''),
    ('Frames Resize: zero keeps items', 'GmRumble.cpp', '            operator delete(m_items);\n            m_items = 0;\n        }\n        m_count = 0;', '            m_items = 0;\n        }\n        m_count = 0;'),
    ('Frames Resize: same size', 'GmRumble.cpp', '    if (count == m_count) {\n        return;\n    }\n    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    void* memory', '    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    void* memory'),
    ('Frames Resize: heap check', 'GmRumble.cpp', '    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    void* memory', '    AtPushHeap(s_heap);\n    void* memory'),
    ('Frames Resize: not cleared', 'GmRumble.cpp', '            items[i] = GmRumbleFrame();\n', ''),
    ('Frames Resize: frames lost', 'GmRumble.cpp', '            items[i] = m_items[i];\n        }\n        operator delete(m_items);', '        }\n        operator delete(m_items);'),
    ('Frames Resize: old kept', 'GmRumble.cpp', '            items[i] = m_items[i];\n        }\n        operator delete(m_items);', '            items[i] = m_items[i];\n        }'),
    ('Frames Assign: not sized', 'GmRumble.cpp', '    Resize(count);\n    for', '    for'),
    ('Effect dtor: frames kept', 'GmRumble.cpp', '    if (m_frames.m_items != 0) {\n        operator delete(m_frames.m_items);\n        m_frames.m_items = 0;\n    }\n', '    m_frames.m_items = 0;\n'),
    ('Effect dtor: ready', 'GmRumble.cpp', '    m_frames.m_count = 0;\n    m_ready = 0;\n}\n\nGmRumbleEffect& ', '    m_frames.m_count = 0;\n}\n\nGmRumbleEffect& '),
    ('Effect assign: ready', 'GmRumble.cpp', '    m_ready = other.m_ready;\n', ''),
    ('Effect assign: name', 'GmRumble.cpp', '        m_name[i] = other.m_name[i];\n', ''),
    ('Effect assign: frames', 'GmRumble.cpp', '            m_frames.Assign(other.m_frames.m_items, other.m_frames.m_count);\n', ''),
    ('Effect assign: frames kept', 'GmRumble.cpp', '            operator delete(m_frames.m_items);\n            m_frames.m_count = 0;\n            m_frames.m_items = 0;\n', ''),
    # Not used: 'Effect assign: to itself'. copying a frame array onto itself changes nothing.
    ('Effect Init: frames kept', 'GmRumble.cpp', '    if (m_ready != 0) {\n        m_frames.Resize(0);\n        m_ready = 0;\n    }\n', ''),
    ('Effect Init: name', 'GmRumble.cpp', '    AtCopyName8(m_name, name);\n', ''),
    ('Effect Init: ready', 'GmRumble.cpp', '    m_frames.Resize(frames);\n    m_ready = 1;', '    m_frames.Resize(frames);'),
    ('Effect Frame', 'GmRumble.cpp', 'return &m_frames.m_items[index];', 'return &m_frames.m_items[index + 1];'),
    ('Effect Name', 'GmRumble.cpp', 'return m_name;', 'return (const char*)this;'),
    ('Effects Free: items', 'GmRumble.cpp', '    delete[] m_items;\n    m_items = 0;\n    m_count = 0;\n}\n\nvoid GmRumbleEffectArray::Resize', '    m_items = 0;\n    m_count = 0;\n}\n\nvoid GmRumbleEffectArray::Resize'),
    ('Effects Free: count', 'GmRumble.cpp', '    delete[] m_items;\n    m_items = 0;\n    m_count = 0;\n}\n\nvoid GmRumbleEffectArray::Resize', '    delete[] m_items;\n    m_items = 0;\n}\n\nvoid GmRumbleEffectArray::Resize'),
    ('Effects Resize: zero', 'GmRumble.cpp', '    if (count == 0) {\n        delete[] m_items;\n        m_items = 0;\n        m_count = 0;\n        return;\n    }\n    if (count == m_count) {\n        return;\n    }\n    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    GmRumbleEffect*', '    if (count == 0) {\n        m_items = 0;\n        m_count = 0;\n        return;\n    }\n    if (count == m_count) {\n        return;\n    }\n    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    GmRumbleEffect*'),
    ('Effects Resize: same size', 'GmRumble.cpp', '    if (count == m_count) {\n        return;\n    }\n    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    GmRumbleEffect*', '    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    GmRumbleEffect*'),
    ('Effects Resize: heap check', 'GmRumble.cpp', '    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    GmRumbleEffect*', '    AtPushHeap(s_heap);\n    GmRumbleEffect*'),
    ('Effects Resize: effects lost', 'GmRumble.cpp', '            items[i] = m_items[i];\n        }\n        delete[] m_items;', '        }\n        delete[] m_items;'),
    ('Effects Resize: old kept', 'GmRumble.cpp', '            items[i] = m_items[i];\n        }\n        delete[] m_items;', '            items[i] = m_items[i];\n        }'),
    # Not used: 'Effects dtor: map kept'. the member's own destructor clears it just after, so only the order of two frees changes.
    ('Effects dtor: effects kept', 'GmRumble.cpp', '    m_byName.Clear();\n    delete[] m_effects.m_items;', '    m_byName.Clear();'),
    ('ReadFrame: keyword', 'GmRumble.cpp', 'if (t->Next() != kKeyFrame) {', 'if (t->Next() != kKeyFrames) {'),
    ('ReadFrame: time', 'GmRumble.cpp', 'frame->m_time = t->ExpectReal();', 't->ExpectReal();'),
    ('ReadFrame: curly', 'GmRumble.cpp', '    frame->m_time = t->ExpectReal();\n    t->ExpectLeftCurly();', '    frame->m_time = t->ExpectReal();\n    t->Next();'),
    ('ReadFrame: left keyword', 'GmRumble.cpp', 'if (t->Next() != kLeft) {\n        return;\n    }', 't->Next();'),
    ('ReadFrame: left', 'GmRumble.cpp', 'frame->m_left = t->ExpectReal();', 'frame->m_right = t->ExpectReal();'),
    ('ReadFrame: right keyword', 'GmRumble.cpp', 'if (t->Next() != kRight) {\n        return;\n    }', 't->Next();'),
    ('ReadFrame: right', 'GmRumble.cpp', 'frame->m_right = t->ExpectReal();', 't->ExpectReal();'),
    ('ReadFrame: end', 'GmRumble.cpp', '    frame->m_right = t->ExpectReal();\n    t->ExpectRightCurly();', '    frame->m_right = t->ExpectReal();\n    t->Next();'),
    ('ReadEffect: keyword', 'GmRumble.cpp', 'if (t->Next() != kRumbleEffect) {\n        return;\n    }', 't->Next();'),
    ('ReadEffect: curly', 'GmRumble.cpp', '    t->ExpectName(name);\n    t->ExpectLeftCurly();', '    t->ExpectName(name);\n    t->Next();'),
    ('ReadEffect: frames keyword', 'GmRumble.cpp', 'if (t->Next() != kKeyFrames) {\n        return;\n    }', 't->Next();'),
    ('ReadEffect: frame kept', 'GmRumble.cpp', '        effect->SetFrame(i, frame);\n', ''),
    ('ReadEffect: inner end', 'GmRumble.cpp', '    t->ExpectRightCurly();\n    t->ExpectRightCurly();\n', '    t->ExpectRightCurly();\n'),
    ('ReadEffects: not sized', 'GmRumble.cpp', '    m_effects.Resize(count);\n', ''),
    ('ReadEffects: no map', 'GmRumble.cpp', '    m_byName.Init(count);\n', ''),
    ('ReadEffects: not in map', 'GmRumble.cpp', '        m_byName.Insert(m_effects.m_items[i].Name(), &m_effects.m_items[i]);\n', ''),
    ('ReadEffects: end', 'GmRumble.cpp', '    }\n    t->ExpectRightCurly();\n}\n\nvoid GmRumbleEffects::Parse', '    }\n}\n\nvoid GmRumbleEffects::Parse'),
    ('Parse: anything is effects', 'GmRumble.cpp', '        if (token == kRumbleEffects) {\n            ReadEffects(t);\n        }\n', '        ReadEffects(t);\n'),
    ('LoadFile: extension', 'GmRumble.cpp', '"RMB"', '"RMX"'),
    ('LoadFile: flags', 'GmRumble.cpp', 'file.Open(path, "RMB", 2, 0)', 'file.Open(path, "RMB", 4, 0)'),
    ('LoadFile: keywords', 'GmRumble.cpp', '            t->SetKeywords(kKeywords, sizeof kKeywords / sizeof kKeywords[0], 0);\n', ''),
    ('Load: map kept', 'GmRumble.cpp', '        m_byName.Clear();\n        m_effects.Resize(0);\n', '        m_effects.Resize(0);\n'),
    ('Load: effects kept', 'GmRumble.cpp', '        m_byName.Clear();\n        m_effects.Resize(0);\n', '        m_byName.Clear();\n'),
    ('Load: not marked', 'GmRumble.cpp', '    LoadFile(path);\n    m_loaded = 1;', '    LoadFile(path);'),
]


TXT_MUTANTS = [
    ('Unicode CharAt', 'AtText.cpp', 'unsigned int AtTextUnicode::CharAt(unsigned int index) {\n    return m_data[m_start + index];', 'unsigned int AtTextUnicode::CharAt(unsigned int index) {\n    return m_data[index];'),
    ('Unicode SetChar', 'AtText.cpp', 'void AtTextUnicode::SetChar(unsigned int index, unsigned int c) {\n    m_data[m_start + index]', 'void AtTextUnicode::SetChar(unsigned int index, unsigned int c) {\n    m_data[index]'),
    ('Unicode RemoveRange: end', 'AtText.cpp', 'void AtTextUnicode::RemoveRange(unsigned int first, unsigned int last) {\n    unsigned short* to = m_data + m_start + first;\n    const unsigned short* from = m_data + m_start + last + 1;\n    for (unsigned int n = m_end - m_start - last; n != 0; n--) {\n        *to++ = *from++;\n    }\n    m_end += first - last - 1;', 'void AtTextUnicode::RemoveRange(unsigned int first, unsigned int last) {\n    unsigned short* to = m_data + m_start + first;\n    const unsigned short* from = m_data + m_start + last + 1;\n    for (unsigned int n = m_end - m_start - last; n != 0; n--) {\n        *to++ = *from++;\n    }\n    m_end += first - last;'),
    ('Unicode RemoveAt: end', 'AtText.cpp', 'void AtTextUnicode::RemoveAt(unsigned int index) {\n    unsigned short* to = m_data + m_start + index;\n    const unsigned short* from = to + 1;\n    for (unsigned int n = m_end - m_start - index + 1; n != 0; n--) {\n        *to++ = *from++;\n    }\n    m_end--;', 'void AtTextUnicode::RemoveAt(unsigned int index) {\n    unsigned short* to = m_data + m_start + index;\n    const unsigned short* from = to + 1;\n    for (unsigned int n = m_end - m_start - index + 1; n != 0; n--) {\n        *to++ = *from++;\n    }'),
    ('Unicode Insert: end', 'AtText.cpp', '    *at = (unsigned short)c;\n    m_end++;', '    *at = (unsigned short)c;'),
    ('Unicode Init: terminator', 'AtText.cpp', 'void AtTextUnicode::Init(unsigned short* data, unsigned int capacity) {\n    m_capacity = capacity;\n    m_data = data;\n    data[capacity - 1] = 0;\n', 'void AtTextUnicode::Init(unsigned short* data, unsigned int capacity) {\n    m_capacity = capacity;\n    m_data = data;\n'),
    ('Ascii CharAt', 'AtText.cpp', 'unsigned int AtTextAscii::CharAt(unsigned int index) {\n    return (unsigned char)m_data[m_start + index];', 'unsigned int AtTextAscii::CharAt(unsigned int index) {\n    return (unsigned char)m_data[index];'),
    ('Ascii SetChar', 'AtText.cpp', 'void AtTextAscii::SetChar(unsigned int index, unsigned int c) {\n    m_data[m_start + index]', 'void AtTextAscii::SetChar(unsigned int index, unsigned int c) {\n    m_data[index]'),
    ('Ascii RemoveRange: end', 'AtText.cpp', 'void AtTextAscii::RemoveRange(unsigned int first, unsigned int last) {\n    char* to = m_data + m_start + first;\n    const char* from = m_data + m_start + last + 1;\n    for (unsigned int n = m_end - m_start - last; n != 0; n--) {\n        *to++ = *from++;\n    }\n    m_end += first - last - 1;', 'void AtTextAscii::RemoveRange(unsigned int first, unsigned int last) {\n    char* to = m_data + m_start + first;\n    const char* from = m_data + m_start + last + 1;\n    for (unsigned int n = m_end - m_start - last; n != 0; n--) {\n        *to++ = *from++;\n    }\n    m_end += first - last;'),
    ('Ascii RemoveAt: end', 'AtText.cpp', 'void AtTextAscii::RemoveAt(unsigned int index) {\n    char* to = m_data + m_start + index;\n    const char* from = to + 1;\n    for (unsigned int n = m_end - m_start - index + 1; n != 0; n--) {\n        *to++ = *from++;\n    }\n    m_end--;', 'void AtTextAscii::RemoveAt(unsigned int index) {\n    char* to = m_data + m_start + index;\n    const char* from = to + 1;\n    for (unsigned int n = m_end - m_start - index + 1; n != 0; n--) {\n        *to++ = *from++;\n    }'),
    ('Ascii Insert: end', 'AtText.cpp', '    *at = (char)c;\n    m_end++;', '    *at = (char)c;'),
    ('Ascii Init: terminator', 'AtText.cpp', 'void AtTextAscii::Init(char* data, unsigned int capacity) {\n    m_capacity = capacity;\n    m_data = data;\n    data[capacity - 1] = 0;\n', 'void AtTextAscii::Init(char* data, unsigned int capacity) {\n    m_capacity = capacity;\n    m_data = data;\n'),
    ('Unicode Recount: start', 'AtText.cpp', 'void AtTextUnicode::Recount() {\n    m_start = 0;\n', 'void AtTextUnicode::Recount() {\n'),
    ('Ascii Recount: start', 'AtText.cpp', 'void AtTextAscii::Recount() {\n    m_start = 0;\n', 'void AtTextAscii::Recount() {\n'),
    ('Unicode Rewind: end', 'AtText.cpp', 'void AtTextUnicode::Rewind() {\n    m_start = 0;\n    m_end = ComputeEnd();', 'void AtTextUnicode::Rewind() {\n    m_start = 0;'),
    ('Unicode Rewind: start', 'AtText.cpp', 'void AtTextUnicode::Rewind() {\n    m_start = 0;\n', 'void AtTextUnicode::Rewind() {\n'),
    ('Release: fields', 'AtText.cpp', 'void AtTextUnicode::Release() {\n    m_data = 0;\n    ClearFields();', 'void AtTextUnicode::Release() {\n    ClearFields();'),
    ('Unicode CopyNarrow: bound', 'AtText.cpp', 'void AtTextUnicode::CopyNarrow(char* out, unsigned int size) {\n    const unsigned short* from = m_data + m_start;\n    unsigned int n = m_end - m_start;\n    if (size <= n) {', 'void AtTextUnicode::CopyNarrow(char* out, unsigned int size) {\n    const unsigned short* from = m_data + m_start;\n    unsigned int n = m_end - m_start;\n    if (size < n) {'),
    ('Unicode CopyWide: bound', 'AtText.cpp', 'void AtTextUnicode::CopyWide(unsigned short* out, unsigned int size) {\n    const unsigned short* from = m_data + m_start;\n    unsigned int n = m_end - m_start;\n    if (size <= n) {', 'void AtTextUnicode::CopyWide(unsigned short* out, unsigned int size) {\n    const unsigned short* from = m_data + m_start;\n    unsigned int n = m_end - m_start;\n    if (size < n) {'),
    ('Ascii CopyNarrow: bound', 'AtText.cpp', 'void AtTextAscii::CopyNarrow(char* out, unsigned int size) {\n    const char* from = m_data + m_start;\n    unsigned int n = m_end - m_start;\n    if (size <= n) {', 'void AtTextAscii::CopyNarrow(char* out, unsigned int size) {\n    const char* from = m_data + m_start;\n    unsigned int n = m_end - m_start;\n    if (size < n) {'),
    ('Ascii CopyWide: bound', 'AtText.cpp', 'void AtTextAscii::CopyWide(unsigned short* out, unsigned int size) {\n    const char* from = m_data + m_start;\n    unsigned int n = m_end - m_start;\n    if (size <= n) {', 'void AtTextAscii::CopyWide(unsigned short* out, unsigned int size) {\n    const char* from = m_data + m_start;\n    unsigned int n = m_end - m_start;\n    if (size < n) {'),
    ('Ascii CopyWide: sign', 'AtText.cpp', 'out[n] = (unsigned char)from[n];', 'out[n] = (unsigned short)(short)from[n];'),
    ('Unicode ToInt', 'AtText.cpp', 'return AtWideToInt(m_data);', 'return 0;'),
    ('Ascii ToInt', 'AtText.cpp', 'return AtNarrowToInt(m_data);', 'return 0;'),
    ('Ascii ToFloat', 'AtText.cpp', 'return AtNarrowToFloat(m_data);', 'return 0;'),
    ('ComputeEnd: index', 'AtText.cpp', 'return m_start + i;', 'return i;'),
    ('ComputeEnd: none', 'AtText.cpp', 'return m_capacity - 1;', 'return m_capacity;'),
    ('ClearFields: capacity', 'AtText.cpp', 'void AtText::ClearFields() {\n    m_capacity = 0;\n', 'void AtText::ClearFields() {\n'),
    ('Status: 20', 'AtText.cpp', 'case 20: return "feature is unimplemented";', 'case 19: return "feature is unimplemented";'),
    ('Report: separator', 'AtText.cpp', 'strcat(message, ": ");\n    strcat(message, path);', 'strcat(message, " ");\n    strcat(message, path);'),
    ('Report: dot', 'AtText.cpp', 'strcat(message, ".");', 'strcat(message, "");'),
    ('Report: fatal argument', 'AtText.cpp', 'strcat(message, extension);\n    AtFatal(message, 0, 0, 0);', 'strcat(message, extension);\n    AtFatal(message, 1, 0, 0);'),
    ('Table ctor: loaded', 'AtText.cpp', '    m_ascii = 0;\n    m_loaded = 0;\n    m_charCount = 0;\n    m_charsUsed = 0;\n    m_stringCount = 0;\n    m_stringsUsed = 0;\n    m_strings = 0;\n    m_chars = 0;\n}\n\nAtTextTable::~AtTextTable', '    m_ascii = 0;\n    m_charCount = 0;\n    m_charsUsed = 0;\n    m_stringCount = 0;\n    m_stringsUsed = 0;\n    m_strings = 0;\n    m_chars = 0;\n}\n\nAtTextTable::~AtTextTable'),
    ('Clear: map kept', 'AtText.cpp', 'void AtTextTable::Clear() {\n    GmNameMap::Clear();\n', 'void AtTextTable::Clear() {\n'),
    ('Clear: ignores loaded', 'AtText.cpp', 'void AtTextTable::Clear() {\n    GmNameMap::Clear();\n    if (m_loaded != 0) {', 'void AtTextTable::Clear() {\n    GmNameMap::Clear();\n    if (1) {'),
    ('Clear: strings kept', 'AtText.cpp', '            operator delete(m_strings);\n', ''),
    ('Clear: chars kept', 'AtText.cpp', '            operator delete(m_chars);\n', ''),
    ('Clear: used kept', 'AtText.cpp', '        m_charsUsed = 0;\n        m_stringCount = 0;', '        m_stringCount = 0;'),
    ('Clear: strings used kept', 'AtText.cpp', '        m_stringCount = 0;\n        m_stringsUsed = 0;\n        m_strings = 0;\n        m_chars = 0;\n    }\n}', '        m_stringCount = 0;\n        m_strings = 0;\n        m_chars = 0;\n    }\n}'),
    ('StringAt: unit', 'AtText.cpp', '(AtText*)((AtTextUnicode*)m_strings + index)', '(AtText*)((AtTextUnicode*)m_strings + index + 1)'),
    ('Ascii allocate: map', 'AtText.cpp', 'void AtTextTable::AllocateAscii(unsigned int strings, unsigned int chars) {\n    GmNameMap::Init(strings);\n', 'void AtTextTable::AllocateAscii(unsigned int strings, unsigned int chars) {\n'),
    ('Ascii allocate: flag', 'AtText.cpp', '    m_ascii = 1;\n    m_loaded = 1;\n', '    m_loaded = 1;\n'),
    ('Ascii allocate: loaded', 'AtText.cpp', '    m_ascii = 1;\n    m_loaded = 1;\n', '    m_ascii = 1;\n'),
    ('Unicode allocate: size', 'AtText.cpp', 'operator new(m_charCount * 2)', 'operator new(m_charCount)'),
    ('Unicode allocate: flag', 'AtText.cpp', '    m_ascii = 0;\n    m_loaded = 1;\n', '    m_loaded = 1;\n'),
    ('Parse: clears first', 'AtText.cpp', '    if (m_loaded != 0) {\n        Clear();\n    }\n    t->SetKeywords', '    t->SetKeywords'),
    ('Parse: keyword', 'AtText.cpp', '{kUnicodeChar, "UnicodeChar"}', '{kUnicodeChar, "UnicodeCh"}'),
    ('Parse: escapes on', 'AtText.cpp', '    t->m_escapes = 1;\n', ''),
    ('Parse: escapes off', 'AtText.cpp', '    t->m_escapes = 0;\n', ''),
    ('Parse: namespace ignored', 'AtText.cpp', '        spaceName = space;\n', ''),
    ('Parse: strings counted again', 'AtText.cpp', '                if (strings == 0) {\n                    t->ExpectLeftBracket();', '                {\n                    t->ExpectLeftBracket();'),
    ('Parse: ascii char flag', 'AtText.cpp', '                chars = (unsigned int)t->ExpectInt();\n                t->ExpectRightBracket();\n                m_ascii = 1;\n', '                chars = (unsigned int)t->ExpectInt();\n                t->ExpectRightBracket();\n'),
    ('Parse: unicode char flag', 'AtText.cpp', '                chars = (unsigned int)t->ExpectInt();\n                t->ExpectRightBracket();\n                m_ascii = 0;\n', '                chars = (unsigned int)t->ExpectInt();\n                t->ExpectRightBracket();\n'),
    ('Parse: no chars check', 'AtText.cpp', 'if (strings != 0 && chars == 0) {', 'if (0) {'),
    ('Parse: ascii length', 'AtText.cpp', 'strncpy(to, text, length);', 'strncpy(to, text, length ? length - 1 : 0);'),
    ('Parse: ascii capacity', 'AtText.cpp', 'item->Init(to, length + 1);', 'item->Init(to, length + 2);'),
    ('Parse: ascii not in map', 'AtText.cpp', '            GmNameMap::Insert(name, item);\n            m_charsUsed += length + 1;', '            m_charsUsed += length + 1;'),
    ('Parse: ascii used', 'AtText.cpp', 'm_charsUsed += length + 1;', 'm_charsUsed += length;'),
    ('Parse: wide end test', 'AtText.cpp', '(unsigned short)value != 0; value', 'value != 0; value'),
    ('Parse: wide item', 'AtText.cpp', '(unsigned short*)m_chars + (m_charsUsed - length), length + 1);', '(unsigned short*)m_chars + (m_charsUsed - length), length + 2);'),
    ('Parse: wide terminator', 'AtText.cpp', '            GmNameMap::Insert(name, item);\n            m_charsUsed++;\n            m_stringsUsed++;', '            GmNameMap::Insert(name, item);\n            m_stringsUsed++;'),
    ('Parse: not registered', 'AtText.cpp', '    context->AddMap(this, spaceName);\n', ''),
    ('Load: extension', 'AtText.cpp', 'file.Open(path, "AIS", 2, 0);', 'file.Open(path, "AIX", 2, 0);'),
    ('Load: report', 'AtText.cpp', 'AtReportFileError(status, path, "AIS");', 'AtReportFileError(0, path, "AIS");'),
    # Not used: 'Load: file kept'. the destructor closes it just after, the same way.
]

RMB_MUTANTS += [
    ('Group: name', 'GmNameMap.cpp', '    AtCopyName8(m_name, name);\n    m_used = 1;', '    m_used = 1;'),
    ('Group: used', 'GmNameMap.cpp', '    AtCopyName8(m_name, name);\n    m_used = 1;', '    AtCopyName8(m_name, name);'),
    ('Group Find: only the first', 'GmNameMap.cpp', '        void* value = m->Find(name);\n        if (value != 0) {\n            return value;\n        }\n', '        return m->Find(name);\n'),
    ('Group GetNameOf: only the first', 'GmNameMap.cpp', '        m->GetNameOf(value, out);\n        if (out[0] != 0) {\n            return;\n        }\n', '        m->GetNameOf(value, out);\n        return;\n'),
    ('Group Add: other group kept', 'GmNameMap.cpp', 'void GmNameMapGroup::Add(GmNameMap* map) {\n    if (map->m_group != 0) {\n        map->m_group->Detach(map);\n    }\n', 'void GmNameMapGroup::Add(GmNameMap* map) {\n'),
    ('Group Add: next', 'GmNameMap.cpp', 'void GmNameMapGroup::Add(GmNameMap* map) {\n    if (map->m_group != 0) {\n        map->m_group->Detach(map);\n    }\n    map->m_next = m_first;\n', 'void GmNameMapGroup::Add(GmNameMap* map) {\n    if (map->m_group != 0) {\n        map->m_group->Detach(map);\n    }\n'),
    ('Group Add: group', 'GmNameMap.cpp', '    m_first = map;\n    map->m_group = this;\n}\n\nvoid GmNameMapGroup::MoveToFront', '    m_first = map;\n}\n\nvoid GmNameMapGroup::MoveToFront'),
    ('Move: name ignored', 'GmNameMap.cpp', 'if (AtNamesEqual(name, m_name) && m_first != map) {', 'if (m_first != map) {'),
    # Not used: 'Move: first not checked'. taking the first map out and putting it back changes nothing unless it is the only one, and that case is tested by Db Find below only through the database: see oracle_rmb.cpp.
    # Not used: 'Move: detach'. the next lines detach it from the group it says it is in, which is this one whenever the list and the field agree.
    ('Move: from elsewhere', 'GmNameMap.cpp', '        Detach(map);\n        if (map->m_group != 0) {\n            map->m_group->Detach(map);\n        }\n', '        Detach(map);\n'),
    ('Detach: database not told', 'GmNameMap.cpp', '            if (m_first == 0 && m_database != 0) {\n                m_database->Release(this);\n            }\n', ''),
    ('Detach: told while not empty', 'GmNameMap.cpp', 'if (m_first == 0 && m_database != 0) {', 'if (m_database != 0) {'),
    ('Map GetNameOf: absent', 'GmNameMap.cpp', '    out[0] = 0;\n}\n\nvoid GmNameMap::GetName(', '}\n\nvoid GmNameMap::GetName('),
    ('Db Clear: pool kept', 'GmNameMap.cpp', '        delete[] m_pool;\n', ''),
    ('Db Clear: free kept', 'GmNameMap.cpp', '    m_poolSize = 0;\n    m_free = 0;\n    m_last = 0;\n    GmNameMap::Clear();', '    m_poolSize = 0;\n    m_last = 0;\n    GmNameMap::Clear();'),
    ('Db Clear: map kept', 'GmNameMap.cpp', '    m_last = 0;\n    GmNameMap::Clear();', '    m_last = 0;'),
    # Not used: 'Db Init: no clear first'. the map's Init right after clears through the vftable when the map is set, and the two are always set together.
    ('Db Init: map size', 'GmNameMap.cpp', '    GmNameMap::Init(count);\n    m_poolSize = count;', '    GmNameMap::Init(count + 1);\n    m_poolSize = count;'),
    ('Db Init: heap check', 'GmNameMap.cpp', '    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n    m_pool', '    AtPushHeap(s_heap);\n    m_pool'),
    ('Db Init: heap not pushed', 'GmNameMap.cpp', '    AtPushHeap(s_heap);\n    m_pool', '    m_pool'),
    ('Db Init: free list head', 'GmNameMap.cpp', '    m_free = m_pool;\n', ''),
    ('Db Init: links', 'GmNameMap.cpp', '        g->m_nextFree = g + 1;\n', ''),
    ('Db Release: not removed', 'GmNameMap.cpp', '    GmNameMap::Remove(group->m_name);\n', ''),
    ('Db Release: not reset', 'GmNameMap.cpp', '    group->Reset();\n', ''),
    ('Db Release: not returned', 'GmNameMap.cpp', '    group->m_nextFree = m_free;\n    m_free = group;\n', ''),
    ('Db Add: free not advanced', 'GmNameMap.cpp', '        m_free = group->m_nextFree;\n', ''),
    ('Db Add: not in the map', 'GmNameMap.cpp', '        GmNameMap::Insert(space, group);\n', ''),
    ('Db Add: name', 'GmNameMap.cpp', '        group->SetName(space);\n', ''),
    ('Db Add: database', 'GmNameMap.cpp', '        group->m_database = this;\n', ''),
    ('Db Add: last', 'GmNameMap.cpp', '    m_last = group;\n    group->Add(map);', '    group->Add(map);'),
    ('Db Add: not added', 'GmNameMap.cpp', '    m_last = group;\n    group->Add(map);', '    m_last = group;'),
    ('Db Remove: no check', 'GmNameMap.cpp', 'void GmNameSpaceDb::Remove(GmNameMap* map) {\n    if (map->m_group != 0) {', 'void GmNameSpaceDb::Remove(GmNameMap* map) {\n    {'),
    ('Db Find: no cache', 'GmNameMap.cpp', 'void* GmNameSpaceDb::Find(const char* space, const char* name) {\n    if (m_last == 0 || !AtNamesEqual(m_last->m_name, space)) {', 'void* GmNameSpaceDb::Find(const char* space, const char* name) {\n    if (1) {'),
    ('Db Find: cache not stored', 'GmNameMap.cpp', '            return 0;\n        }\n        m_last = group;\n    }\n    return m_last->Find(name);', '            return 0;\n        }\n        return group->Find(name);\n    }\n    return m_last->Find(name);'),
    ('Db GetNameOf: absent', 'GmNameMap.cpp', '            out[0] = 0;\n            return;\n', '            return;\n'),
    ('Db GetNameOf: cache not stored', 'GmNameMap.cpp', '            return;\n        }\n        m_last = group;\n    }\n    m_last->GetNameOf(value, out);', '            return;\n        }\n        group->GetNameOf(value, out);\n        return;\n    }\n    m_last->GetNameOf(value, out);'),
]


SET_MUTANTS = [
    ('Command: first word read', 'GmAppSettings.cpp', 'for (unsigned int i = 1; i < s_argc; i++) {', 'for (unsigned int i = 0; i < s_argc; i++) {'),
    ('Command: last word skipped', 'GmAppSettings.cpp', 'for (unsigned int i = 1; i < s_argc; i++) {', 'for (unsigned int i = 1; i + 1 < s_argc; i++) {'),
    ('Command: level value not skipped', 'GmAppSettings.cpp', 'arg = s_argv[++i];\n            for (unsigned int k', 'arg = s_argv[i + 1];\n            for (unsigned int k'),
    ('Command: uppercase stops at end', 'GmAppSettings.cpp', 'k < s_levelUppercaseCount; k++) {', 'k < s_levelUppercaseCount && arg[k]; k++) {'),
    ('Command: uppercase range low', 'GmAppSettings.cpp', "arg[k] >= 'a' && arg[k] <= 'z'", "arg[k] >= '`' && arg[k] <= 'z'"),
    ('Command: uppercase range high', 'GmAppSettings.cpp', "arg[k] >= 'a' && arg[k] <= 'z'", "arg[k] >= 'a' && arg[k] <= '{'"),
    ('Command: uppercase none', 'GmAppSettings.cpp', 'arg[k] -= 0x20;', 'arg[k] -= 0;'),
    ('Command: level length', 'GmAppSettings.cpp', 'strlen(arg) <= 8 && arg[0]', 'strlen(arg) < 8 && arg[0]'),
    ('Command: level no length', 'GmAppSettings.cpp', "strlen(arg) <= 8 && arg[0] == 'L'", "arg[0] == 'L'"),
    ('Command: level any letter', 'GmAppSettings.cpp', "strlen(arg) <= 8 && arg[0] == 'L'", 'strlen(arg) <= 8'),
    ('Command: level not stored', 'GmAppSettings.cpp', '                AtCopyName(arg, m_level);\n', ''),
    ('Command: fullscreen', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "fullscreen")) {\n            m_fullscreen = 1;', 'EqualIgnoringCase(arg, "fullscreen")) {\n            m_fullscreen = 0;'),
    ('Command: name fullscreen', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "fullscreen"))', 'EqualIgnoringCase(arg, "fullscreenx"))'),
    ('Command: testalllevels', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "testalllevels")) {\n            m_testAllLevels = 1;', 'EqualIgnoringCase(arg, "testalllevels")) {\n            m_testAllLevels = 0;'),
    ('Command: name testalllevels', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "testalllevels"))', 'EqualIgnoringCase(arg, "testalllevelsx"))'),
    ('Command: openlist', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "openlist")) {\n            m_openList = 1;', 'EqualIgnoringCase(arg, "openlist")) {\n            m_openList = 0;'),
    ('Command: name openlist', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "openlist"))', 'EqualIgnoringCase(arg, "openlistx"))'),
    ('Command: verticalsyncoff', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "verticalsyncoff")) {\n            m_verticalSyncOff = 1;', 'EqualIgnoringCase(arg, "verticalsyncoff")) {\n            m_verticalSyncOff = 0;'),
    ('Command: name verticalsyncoff', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "verticalsyncoff"))', 'EqualIgnoringCase(arg, "verticalsyncoffx"))'),
    ('Command: skipintro', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "skipintro")) {\n            m_skipIntro = 1;', 'EqualIgnoringCase(arg, "skipintro")) {\n            m_skipIntro = 0;'),
    ('Command: name skipintro', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "skipintro"))', 'EqualIgnoringCase(arg, "skipintrox"))'),
    ('Command: mainmenu', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "skipmainmenu")) {\n            m_skipMainMenu = 1;', 'EqualIgnoringCase(arg, "skipmainmenu")) {\n            m_skipMainMenu = 0;'),
    ('Command: name mainmenu', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "skipmainmenu"))', 'EqualIgnoringCase(arg, "skipmainmenux"))'),
    ('Command: nomusic', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "nomusic")) {\n            GmAppAudioOptions::s_instance->m_noMusic = 1;', 'EqualIgnoringCase(arg, "nomusic")) {\n            GmAppAudioOptions::s_instance->m_noMusic = 0;'),
    ('Command: name nomusic', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "nomusic"))', 'EqualIgnoringCase(arg, "nomusicx"))'),
    ('Command: noambientaudio', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "noambientaudio")) {\n            GmAppAudioOptions::s_instance->m_noAmbientAudio = 1;', 'EqualIgnoringCase(arg, "noambientaudio")) {\n            GmAppAudioOptions::s_instance->m_noAmbientAudio = 0;'),
    ('Command: name noambientaudio', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "noambientaudio"))', 'EqualIgnoringCase(arg, "noambientaudiox"))'),
    ('Command: noaudio', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "noaudio")) {\n            GmAppAudioOptions::s_instance->m_noAudio = 1;', 'EqualIgnoringCase(arg, "noaudio")) {\n            GmAppAudioOptions::s_instance->m_noAudio = 0;'),
    ('Command: name noaudio', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "noaudio"))', 'EqualIgnoringCase(arg, "noaudiox"))'),
    ('Command: displaybanks', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "displaybanks")) {\n            GmAppAudioOptions::s_instance->m_displayBanks = 1;', 'EqualIgnoringCase(arg, "displaybanks")) {\n            GmAppAudioOptions::s_instance->m_displayBanks = 0;'),
    ('Command: name displaybanks', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "displaybanks"))', 'EqualIgnoringCase(arg, "displaybanksx"))'),
    ('Command: displaysounds', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "displaysounds")) {\n            GmAppAudioOptions::s_instance->m_displaySounds = 1;', 'EqualIgnoringCase(arg, "displaysounds")) {\n            GmAppAudioOptions::s_instance->m_displaySounds = 0;'),
    ('Command: name displaysounds', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "displaysounds"))', 'EqualIgnoringCase(arg, "displaysoundsx"))'),
    ('Command: displaymusic', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "displaymusic")) {\n            GmAppAudioOptions::s_instance->m_displayMusic = 1;', 'EqualIgnoringCase(arg, "displaymusic")) {\n            GmAppAudioOptions::s_instance->m_displayMusic = 0;'),
    ('Command: name displaymusic', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "displaymusic"))', 'EqualIgnoringCase(arg, "displaymusicx"))'),
    ('Command: audiotest', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "audiotest")) {\n            GmAppAudioOptions::s_instance->m_audioTest = 1;', 'EqualIgnoringCase(arg, "audiotest")) {\n            GmAppAudioOptions::s_instance->m_audioTest = 0;'),
    ('Command: name audiotest', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "audiotest"))', 'EqualIgnoringCase(arg, "audiotestx"))'),
    ('Command: maximized eats next', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "maximized")) {\n', 'EqualIgnoringCase(arg, "maximized")) {\n            i++;\n'),
    ('Command: page eats next', 'GmAppSettings.cpp', 'EqualIgnoringCase(arg, "page")) {\n', 'EqualIgnoringCase(arg, "page")) {\n            i++;\n'),
    ('Command: dead zone not skipped', 'GmAppSettings.cpp', 'const int value = AtNarrowToInt(s_argv[++i]);\n            if (value >= 0 && value < 100)', 'const int value = AtNarrowToInt(s_argv[i + 1]);\n            if (value >= 0 && value < 100)'),
    ('Command: dead zone low', 'GmAppSettings.cpp', '            const int value = AtNarrowToInt(s_argv[++i]);\n            if (value >= 0 && value < 100', '            const int value = AtNarrowToInt(s_argv[++i]);\n            if (value >= 1 && value < 100'),
    ('Command: dead zone negative', 'GmAppSettings.cpp', '            const int value = AtNarrowToInt(s_argv[++i]);\n            if (value >= 0 && value < 100', '            const int value = AtNarrowToInt(s_argv[++i]);\n            if (value < 100'),
    ('Command: dead zone high', 'GmAppSettings.cpp', '            const int value = AtNarrowToInt(s_argv[++i]);\n            if (value >= 0 && value < 100', '            const int value = AtNarrowToInt(s_argv[++i]);\n            if (value >= 0 && value <= 100'),
    ('Command: dead zone scaled', 'GmAppSettings.cpp', '            const int value = AtNarrowToInt(s_argv[++i]);\n            if (value >= 0 && value < 100) {\n                m_controllerDeadZone = (float)value;', '            const int value = AtNarrowToInt(s_argv[++i]);\n            if (value >= 0 && value < 100) {\n                m_controllerDeadZone = (float)value / 100;'),
    ('Command: add channel off', 'GmAppSettings.cpp', 'SetChannel(s_argv[++i], 1);', 'SetChannel(s_argv[++i], 0);'),
    ('Command: no channel on', 'GmAppSettings.cpp', 'SetChannel(s_argv[++i], 0);', 'SetChannel(s_argv[++i], 1);'),
    ('Command: add not skipped', 'GmAppSettings.cpp', 'SetChannel(s_argv[++i], 1);', 'SetChannel(s_argv[i + 1], 1);'),
    ('Command: no not skipped', 'GmAppSettings.cpp', 'SetChannel(s_argv[++i], 0);', 'SetChannel(s_argv[i + 1], 0);'),
    ('Command: stats field', 'GmAppSettings.cpp', '        } else if (EqualIgnoringCase(arg, "disableconsolememstats")) {\n            m_memStats = 0;\n            g_consoleMemStats = 0;\n', '        } else if (EqualIgnoringCase(arg, "disableconsolememstats")) {\n            g_consoleMemStats = 0;\n'),
    ('Command: stats global', 'GmAppSettings.cpp', '        } else if (EqualIgnoringCase(arg, "disableconsolememstats")) {\n            m_memStats = 0;\n            g_consoleMemStats = 0;\n', '        } else if (EqualIgnoringCase(arg, "disableconsolememstats")) {\n            m_memStats = 0;\n'),
    ('Command: audio mode not skipped', 'GmAppSettings.cpp', 'const int value = AtNarrowToInt(s_argv[++i]);\n            if (value >= 0 && value < 3)', 'const int value = AtNarrowToInt(s_argv[i + 1]);\n            if (value >= 0 && value < 3)'),
    ('Command: audio mode high', 'GmAppSettings.cpp', 'value >= 0 && value < 3', 'value >= 0 && value <= 3'),
    ('Command: audio mode negative', 'GmAppSettings.cpp', 'value >= 0 && value < 3', 'value < 3'),
    ('Command: audio mode low', 'GmAppSettings.cpp', 'value >= 0 && value < 3', 'value >= 1 && value < 3'),
    ('Channel: table name', 'GmAppSettings.cpp', '{"event", 0x1000}', '{"events", 0x1000}'),
    ('Channel: top bit', 'GmAppSettings.cpp', '{"innerloop", 0x8000}', '{"innerloop", 0x4000}'),
    ('Channel: low bit', 'GmAppSettings.cpp', '{"framework", 0x0004}', '{"framework", 0x0002}'),
    ('Channel: case', 'GmAppSettings.cpp', 'const int x = toupper((unsigned char)*a);\n        if (x != toupper((unsigned char)*b)) {', 'const int x = (unsigned char)*a;\n        if (x != (unsigned char)*b) {'),
    ('Channel: prefix accepted', 'GmAppSettings.cpp', '        if (x == 0) {\n            return 1;\n        }\n', '        if (x == 0 || b[1] == 0) {\n            return 1;\n        }\n'),
    ('Channel: on means nonzero', 'GmAppSettings.cpp', 'if (on == 1) {', 'if (on != 0) {'),
    ('Channel: clear', 'GmAppSettings.cpp', 'g_debugChannels &= (unsigned short)~kChannels[i].m_bit;', 'g_debugChannels &= (unsigned short)kChannels[i].m_bit;'),
    ('Channel: null name', 'GmAppSettings.cpp', '    if (name == 0) {\n        return;\n    }\n', ''),
    ('Size: fullscreen kept', 'GmAppSettings.cpp', '    s->m_fullscreen = 0;\n', ''),
    ('SetDebugChannels: nothing', 'GmAppSettings.cpp', 'g_debugChannels = channels;', ''),
    ('Trace: null not checked', 'GmAppSettings.cpp', 'void GmHistoryLogs::SetTraceHistory(int lines) {\n    if (m_traces != 0) {', 'void GmHistoryLogs::SetTraceHistory(int lines) {\n    {'),
    ('Trace: wrong log', 'GmAppSettings.cpp', 'm_traces->SetLength(lines);', 'm_errors->SetLength(lines);'),
    ('Error: tests its own', 'GmAppSettings.cpp', 'void GmHistoryLogs::SetErrorHistory(int lines) {\n    if (m_traces != 0) {', 'void GmHistoryLogs::SetErrorHistory(int lines) {\n    {'),
    ('Error: wrong log', 'GmAppSettings.cpp', 'm_errors->SetLength(lines);', 'm_traces->SetLength(lines);'),
    ('Language: german', 'GmAppSettings.cpp', 'case 3: AtCopyName("GERMAN", language); break;', 'case 3: AtCopyName("FRENCH", language); break;'),
    ('Language: french', 'GmAppSettings.cpp', 'case 4: AtCopyName("FRENCH", language); break;', 'case 5: AtCopyName("FRENCH", language); break;'),
    ('Language: italian', 'GmAppSettings.cpp', 'case 6: AtCopyName("ITALIAN", language); break;', 'case 7: AtCopyName("ITALIAN", language); break;'),
    ('Language: default', 'GmAppSettings.cpp', 'default: AtCopyName("ENGLISH", language); break;', 'default: AtCopyName("JAPANES", language); break;'),
    ('Language: region', 'GmAppSettings.cpp', 'if (AtPlatformGameRegion() == 1) {', 'if (AtPlatformGameRegion() != 0) {'),
    ('Language: region ignored', 'GmAppSettings.cpp', '    if (AtPlatformGameRegion() == 1) {\n        AtCopyName8(language, "ENGLISH\\0");\n    }\n', ''),
    ('Ctor: channels', 'GmAppSettings.cpp', '    GmSetDebugChannels(3);\n', ''),
    ('Ctor: level', 'GmAppSettings.cpp', 'AtCopyName8(m_level, "ALMOND\\0\\0");', 'AtCopyName8(m_level, "ALMONE\\0\\0");'),
    ('Ctor: data folder', 'GmAppSettings.cpp', '    AtCopyName8(m_dataFolder, "\\0\\0\\0\\0\\0\\0\\0");\n', ''),
    ('Ctor: language', 'GmAppSettings.cpp', '    SetDefaultLanguage();\n', ''),
    ('Ctor: width', 'GmAppSettings.cpp', 'm_width = 640;', 'm_width = 641;'),
    ('Ctor: height', 'GmAppSettings.cpp', 'm_height = 480;', 'm_height = 481;'),
    ('Ctor: dead zone', 'GmAppSettings.cpp', 'm_controllerDeadZone = 25.0f;', 'm_controllerDeadZone = 24.0f;'),
    ('Ctor: ready', 'GmAppSettings.cpp', '    m_ready = 1;\n', ''),
    ('Load: no file check', 'GmAppSettings.cpp', '    if (AtIoMountManager::s_instance->m_device->IsFileMissing("App.ini") != 0) {\n        return;\n    }\n', ''),
    ('Load: file name', 'GmAppSettings.cpp', 'file.Open("App", "INI", 2, 0)', 'file.Open("App", "INX", 2, 0)'),
    ('Load: flags', 'GmAppSettings.cpp', 'file.Open("App", "INI", 2, 0)', 'file.Open("App", "INI", 4, 0)'),
    ('Load: report', 'GmAppSettings.cpp', 'AtReportFileErrorText(status, "App.ini");', 'AtReportFileErrorText(0, "App.ini");'),
    ('Load: keywords', 'GmAppSettings.cpp', '    t->SetKeywords(kKeywords, sizeof kKeywords / sizeof kKeywords[0], 0);\n', ''),
    ('Load: opening brace', 'GmAppSettings.cpp', '    t->ExpectLeftCurly();\n', ''),
    # Not used: 'Load: close'. The destructor closes it just after, the same way.
    ('Data folder', 'GmAppSettings.cpp', 't->ExpectName(m_dataFolder);', 't->ExpectName(m_level);'),
    ('Level: limit', 'GmAppSettings.cpp', 'if (strlen(text) <= 8) {', 'if (strlen(text) < 8) {'),
    ('Level: copy', 'GmAppSettings.cpp', '                AtCopyName(text, m_level);\n', ''),
    ('Resolution: width', 'GmAppSettings.cpp', 'm_width = t->ExpectInt();', 't->ExpectInt();'),
    ('Resolution: height', 'GmAppSettings.cpp', 'm_height = t->ExpectInt();', 't->ExpectInt();'),
    ('Fullscreen', 'GmAppSettings.cpp', '        case kFullscreen:\n            m_fullscreen = 1;\n', '        case kFullscreen:\n'),
    ('Maximized acted on', 'GmAppSettings.cpp', '        case kFullscreen:\n', '        case kMaximized:\n        case kFullscreen:\n'),
    ('NTSC', 'GmAppSettings.cpp', 'SetSize(this, 640, 448);', 'SetSize(this, 640, 480);'),
    ('PS2 not as NTSC', 'GmAppSettings.cpp', '        case kNtsc:\n        case kPs2:\n', '        case kNtsc:\n'),
    ('PAL', 'GmAppSettings.cpp', 'SetSize(this, 512, 512);', 'SetSize(this, 512, 448);'),
    ('XB', 'GmAppSettings.cpp', 'SetSize(this, 720, 480);', 'SetSize(this, 720, 576);'),
    ('XB720', 'GmAppSettings.cpp', 'SetSize(this, 1280, 720);', 'SetSize(this, 1280, 768);'),
    ('XB1080', 'GmAppSettings.cpp', 'SetSize(this, 1920, 1080);', 'SetSize(this, 1920, 1088);'),
    ('Keyword: second XB name', 'GmAppSettings.cpp', '{kXb, "XB480pScreen"}', '{kXb720, "XB480pScreen"}'),
    ('Test all', 'GmAppSettings.cpp', '        case kTestAllLevels:\n            m_testAllLevels = 1;\n', '        case kTestAllLevels:\n'),
    ('Open list', 'GmAppSettings.cpp', '        case kOpenList:\n            m_openList = 1;\n', '        case kOpenList:\n'),
    ('Dead zone: low', 'GmAppSettings.cpp', '            const int value = t->ExpectInt();\n            if (value >= 0 && value < 100) {', '            const int value = t->ExpectInt();\n            if (value < 100) {'),
    ('Dead zone: high', 'GmAppSettings.cpp', '            const int value = t->ExpectInt();\n            if (value >= 0 && value < 100) {', '            const int value = t->ExpectInt();\n            if (value >= 0 && value <= 100) {'),
    ('Dead zone: value', 'GmAppSettings.cpp', '            const int value = t->ExpectInt();\n            if (value >= 0 && value < 100) {\n                m_controllerDeadZone = (float)value;', '            const int value = t->ExpectInt();\n            if (value >= 0 && value < 100) {\n                m_controllerDeadZone = (float)value + 1;'),
    ('Trace: range', 'GmAppSettings.cpp', '            if (value >= 0 && value < 1000) {\n                GmHistoryLogs::s_instance->SetTraceHistory(value);', '            if (value >= 0 && value <= 1000) {\n                GmHistoryLogs::s_instance->SetTraceHistory(value);'),
    ('Trace: negative', 'GmAppSettings.cpp', '            if (value >= 0 && value < 1000) {\n                GmHistoryLogs::s_instance->SetTraceHistory(value);', '            if (value < 1000) {\n                GmHistoryLogs::s_instance->SetTraceHistory(value);'),
    ('Error: range', 'GmAppSettings.cpp', '            if (value >= 0 && value < 1000) {\n                GmHistoryLogs::s_instance->SetErrorHistory(value);', '            if (value >= 0 && value <= 1000) {\n                GmHistoryLogs::s_instance->SetErrorHistory(value);'),
    ('Error: negative', 'GmAppSettings.cpp', '            if (value >= 0 && value < 1000) {\n                GmHistoryLogs::s_instance->SetErrorHistory(value);', '            if (value < 1000) {\n                GmHistoryLogs::s_instance->SetErrorHistory(value);'),
    ('Vsync', 'GmAppSettings.cpp', '        case kVerticalSyncOff:\n            m_verticalSyncOff = 1;\n', '        case kVerticalSyncOff:\n'),
    ('add_dio', 'GmAppSettings.cpp', 'SetChannel(t->ExpectString(), 1);', 'SetChannel(t->ExpectString(), 0);'),
    ('no_dio', 'GmAppSettings.cpp', 'SetChannel(t->ExpectString(), 0);', 'SetChannel(t->ExpectString(), 1);'),
    ('Platform binary', 'GmAppSettings.cpp', '            g_usePlatformBinary = 0;\n', ''),
    ('Music', 'GmAppSettings.cpp', '            g_backgroundMusic = 0;\n', ''),
    ('Language key', 'GmAppSettings.cpp', 'AtCopyName8(m_language, name);\n            break;', 'break;'),
    ('Controller', 'GmAppSettings.cpp', 'm_activeControllers[t->ExpectInt()] = 1;', 'm_activeControllers[0] = t->ExpectInt();'),
    ('Character: index', 'GmAppSettings.cpp', 'AtCopyName8(m_character + index * 8, name);', 'AtCopyName8(m_character, name);'),
    ('Character: name', 'GmAppSettings.cpp', 'AtCopyName8(m_character + index * 8, name);', '(void)index;'),
    ('Skip intro', 'GmAppSettings.cpp', '        case kSkipIntro:\n            m_skipIntro = 1;\n', '        case kSkipIntro:\n'),
    ('Mem stats field', 'GmAppSettings.cpp', '        case kDisableConsoleMemStats:\n            m_memStats = 0;\n', '        case kDisableConsoleMemStats:\n'),
    ('Mem stats global', 'GmAppSettings.cpp', '        case kDisableConsoleMemStats:\n            m_memStats = 0;\n            g_consoleMemStats = 0;\n', '        case kDisableConsoleMemStats:\n            m_memStats = 0;\n'),
    ('Main menu field', 'GmAppSettings.cpp', '        case kSkipMainMenu:\n            m_skipMainMenu = 1;\n', '        case kSkipMainMenu:\n'),
    ('Main menu global', 'GmAppSettings.cpp', '            m_skipMainMenu = 1;\n            g_skipProfileLoad = 1;\n', '            m_skipMainMenu = 1;\n'),
    ('Profile', 'GmAppSettings.cpp', '        case kSkipProfileLoadCreate:\n            g_skipProfileLoad = 1;\n            break;\n', ''),
    ('Debug menu', 'GmAppSettings.cpp', '            m_enableDebugMenu = 1;\n', ''),
    ('Skip legal', 'GmAppSettings.cpp', '            m_skipLegal = 1;\n', ''),
    ('Audio: m_noMusic', 'GmAppSettings.cpp', '        case kNoMusic:\n            GmAppAudioOptions::s_instance->m_noMusic = 1;', '        case kNoMusic:\n            '),
    ('Audio: m_noAmbientAudio', 'GmAppSettings.cpp', '        case kNoAmbientAudio:\n            GmAppAudioOptions::s_instance->m_noAmbientAudio = 1;', '        case kNoAmbientAudio:\n            '),
    ('Audio: m_noAudio', 'GmAppSettings.cpp', '        case kNoAudio:\n            GmAppAudioOptions::s_instance->m_noAudio = 1;', '        case kNoAudio:\n            '),
    ('Audio: m_displayBanks', 'GmAppSettings.cpp', '        case kDisplayBanks:\n            GmAppAudioOptions::s_instance->m_displayBanks = 1;', '        case kDisplayBanks:\n            '),
    ('Audio: m_displaySounds', 'GmAppSettings.cpp', '        case kDisplaySounds:\n            GmAppAudioOptions::s_instance->m_displaySounds = 1;', '        case kDisplaySounds:\n            '),
    ('Audio: m_displayMusic', 'GmAppSettings.cpp', '        case kDisplayMusic:\n            GmAppAudioOptions::s_instance->m_displayMusic = 1;', '        case kDisplayMusic:\n            '),
    ('Audio: m_audioTest', 'GmAppSettings.cpp', '        case kAudioTest:\n            GmAppAudioOptions::s_instance->m_audioTest = 1;', '        case kAudioTest:\n            '),
    ('Audio mode: range', 'GmAppSettings.cpp', 'if (value < 3) {', 'if (value <= 3) {'),
    ('Audio mode: value', 'GmAppSettings.cpp', 'GmAppAudioOptions::s_instance->m_audioMode = (int)value;', 'GmAppAudioOptions::s_instance->m_audioMode = 1;'),
    ('No dialogs', 'GmAppSettings.cpp', '            g_noDialogs = 1;\n', ''),
    ('Flash heap', 'GmAppSettings.cpp', '            g_showFlashHeapInfo = 1;\n', ''),
    ('Unlockables', 'GmAppSettings.cpp', '            g_enableAllUnlockables = 1;\n', ''),
]


AUD_MUTANTS = [
    ('Preset ctor: name', 'GmAudioData.cpp', '    m_name[0] = 0;\n', ''),
    ('Preset ctor: rest', 'GmAudioData.cpp', '        m_unknown20[i] = 0;\n', ''),
    ('Presets: extension', 'GmAudioData.cpp', 'file.Open(path, "GOV", 2, 0)', 'file.Open(path, "GOX", 2, 0)'),
    ('Presets: flags', 'GmAudioData.cpp', 'file.Open(path, "GOV", 2, 0)', 'file.Open(path, "GOV", 4, 0)'),
    ('Presets: open failure', 'GmAudioData.cpp', 'if (file.Open(path, "GOV", 2, 0) != 0) {\n        return;\n    }', 'file.Open(path, "GOV", 2, 0);'),
    ('Presets: keywords', 'GmAudioData.cpp', '{kUi, "UI"}', '{kUi, "UX"}'),
    ('Presets: first keyword', 'GmAudioData.cpp', '    t->ExpectToken(kVolumePresets);\n', '    t->Next();\n'),
    ('Presets: heap check', 'GmAudioData.cpp', '    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n', ''),
    ('Presets: heap not pushed', 'GmAudioData.cpp', '    AtPushHeap(s_heap);\n', ''),
    ('Presets: preset keyword', 'GmAudioData.cpp', '        t->ExpectToken(kPreset);\n', '        t->Next();\n'),
    ('Presets: name', 'GmAudioData.cpp', '        AtCopyName8(preset->m_name, name);\n', ''),
    ('Presets: defaults', 'GmAudioData.cpp', 'preset->m_ambience = preset->m_sfx = preset->m_voice = preset->m_music = 1.0f;', 'preset->m_ambience = preset->m_sfx = preset->m_voice = preset->m_music = 0.5f;'),
    ('Presets: default ui', 'GmAudioData.cpp', 'preset->m_ui = preset->m_cinema = 1.0f;', 'preset->m_ui = preset->m_cinema = 0.5f;'),
    ('Presets: rest default', 'GmAudioData.cpp', '            preset->m_unknown20[k] = 1.0f;\n', ''),
    ('Presets: curly', 'GmAudioData.cpp', '        t->ExpectLeftCurly();\n        for (int token = t->Next(); token != AtIoTokenizer::kRightCurly; token = t->Next()) {\n            switch (token) {\n            case kAmbience', '        t->Next();\n        for (int token = t->Next(); token != AtIoTokenizer::kRightCurly; token = t->Next()) {\n            switch (token) {\n            case kAmbience'),
    ('Presets: ambience', 'GmAudioData.cpp', 'case kAmbience: preset->m_ambience = t->ExpectReal(); break;', 'case kAmbience: preset->m_ambience = t->ExpectReal() + 1; break;'),
    ('Presets: sfx', 'GmAudioData.cpp', 'case kSfx: preset->m_sfx = t->ExpectReal(); break;', 'case kSfx: preset->m_sfx = t->ExpectReal() + 1; break;'),
    ('Presets: voice', 'GmAudioData.cpp', 'case kVoice: preset->m_voice = t->ExpectReal(); break;', 'case kVoice: preset->m_voice = t->ExpectReal() + 1; break;'),
    ('Presets: music', 'GmAudioData.cpp', 'case kMusic: preset->m_music = t->ExpectReal(); break;', 'case kMusic: preset->m_music = t->ExpectReal() + 1; break;'),
    ('Presets: ui', 'GmAudioData.cpp', 'case kUi: preset->m_ui = t->ExpectReal(); break;', 'case kUi: preset->m_ui = t->ExpectReal() + 1; break;'),
    ('Presets: cinema', 'GmAudioData.cpp', 'case kCinema: preset->m_cinema = t->ExpectReal(); break;', 'case kCinema: preset->m_cinema = t->ExpectReal() + 1; break;'),
    ('Presets: end', 'GmAudioData.cpp', '    t->ExpectRightCurly();\n    file.Close();\n}\n\nvoid GmAudioData::LoadMemoryBlocks', '    file.Close();\n}\n\nvoid GmAudioData::LoadMemoryBlocks'),
    ('Blocks: extension', 'GmAudioData.cpp', 'file.Open(path, "GOB", 2, 0)', 'file.Open(path, "GOX", 2, 0)'),
    ('Blocks: open failure', 'GmAudioData.cpp', 'if (file.Open(path, "GOB", 2, 0) != 0) {\n        return;\n    }', 'file.Open(path, "GOB", 2, 0);'),
    ('Blocks: keyword', 'GmAudioData.cpp', '{kSize, "Size"}', '{kSize, "Sise"}'),
    ('Blocks: sections ignored', 'GmAudioData.cpp', 'if (t->Next() == kPlatformSpecific) {', 'if (0) {'),
    ('Blocks: section name', 'GmAudioData.cpp', '"XBOX"', '"XBOY"'),
    ('Blocks: section limit', 'GmAudioData.cpp', 't->ExpectString(8u)', 't->ExpectString(9u)'),
    ('Blocks: case', 'GmAudioData.cpp', '        const int x = toupper((unsigned char)*a);\n        if (x != toupper((unsigned char)*b)) {', '        const int x = (unsigned char)*a;\n        if (x != (unsigned char)*b) {'),
    ('Blocks: prefix', 'GmAudioData.cpp', '        if (x == 0) {\n            return 1;\n        }\n', '        if (x == 0 || b[1] == 0) {\n            return 1;\n        }\n'),
    ('Blocks: no section reads anyway', 'GmAudioData.cpp', '        if (!found) {\n            file.Close();\n            return;\n        }\n', ''),
    ('Blocks: token not handed back', 'GmAudioData.cpp', '        t->m_pushedBack = 1;\n', ''),
    ('Blocks: database keyword', 'GmAudioData.cpp', '    t->ExpectToken(kBlockDatabase);\n', '    t->Next();\n'),
    ('Blocks: database name', 'GmAudioData.cpp', '    t->ExpectName(database);\n', ''),
    ('Blocks: block keyword', 'GmAudioData.cpp', '        t->ExpectToken(kBlock);\n', '        t->Next();\n'),
    ('Blocks: where default', 'GmAudioData.cpp', 'int where = 2;', 'int where = 0;'),
    ('Blocks: id default', 'GmAudioData.cpp', 'int id = 0;', 'int id = 1;'),
    ('Blocks: size default', 'GmAudioData.cpp', 'int size = 0;', 'int size = 1;'),
    ('Blocks: instances default', 'GmAudioData.cpp', 'unsigned int instances = 1;', 'unsigned int instances = 2;'),
    ('Blocks: curly', 'GmAudioData.cpp', '        t->ExpectLeftCurly();\n        for (int token = t->Next(); token != AtIoTokenizer::kRightCurly; token = t->Next()) {\n            switch (token) {\n            case kPrimaryRam', '        t->Next();\n        for (int token = t->Next(); token != AtIoTokenizer::kRightCurly; token = t->Next()) {\n            switch (token) {\n            case kPrimaryRam'),
    ('Blocks: primary', 'GmAudioData.cpp', 'case kPrimaryRam: where = 0; break;', 'case kPrimaryRam: where = 1; break;'),
    ('Blocks: secondary', 'GmAudioData.cpp', 'case kSecondaryRam: where = 1; break;', 'case kSecondaryRam: where = 0; break;'),
    ('Blocks: sound', 'GmAudioData.cpp', 'case kSoundRam: where = 2; break;', 'case kSoundRam: where = 0; break;'),
    ('Blocks: instances', 'GmAudioData.cpp', 'case kInstanceCount: instances = (unsigned int)t->ExpectInt(); break;', 'case kInstanceCount: t->ExpectInt(); break;'),
    ('Blocks: id', 'GmAudioData.cpp', 'case kBlockId: id = t->ExpectInt(); break;', 'case kBlockId: t->ExpectInt(); break;'),
    ('Blocks: size', 'GmAudioData.cpp', 'case kSize: size = t->ExpectInt(); break;', 'case kSize: t->ExpectInt(); break;'),
    ('Blocks: allocate order', 'GmAudioData.cpp', 'Allocate(where, size, id, 0)', 'Allocate(where, id, size, 0)'),
    ('Blocks: null kept', 'GmAudioData.cpp', 'if (memory != 0) {', 'if (1) {'),
    ('Blocks: record where', 'GmAudioData.cpp', '                m_blocks[m_blockCount].m_where = where;\n', ''),
    ('Blocks: record id', 'GmAudioData.cpp', '                m_blocks[m_blockCount].m_id = id;\n', ''),
    ('Blocks: record size', 'GmAudioData.cpp', '                m_blocks[m_blockCount].m_size = size;\n', ''),
    ('Blocks: record memory', 'GmAudioData.cpp', '                m_blocks[m_blockCount].m_memory = memory;\n', ''),
    ('Blocks: count', 'GmAudioData.cpp', '                m_blockCount++;\n', ''),
]


LDR_MUTANTS = [
    ('Configure: path', 'GmManifestLoader.cpp', '    strcpy(m_path, request->m_path);\n', ''),
    ('Configure: tag', 'GmManifestLoader.cpp', '    m_heap = request->m_heap;\n', ''),
    ('Configure: name', 'GmManifestLoader.cpp', '    AtCopyName8(m_name, request->m_name);\n', ''),
    ('Reset: path', 'GmManifestLoader.cpp', '    m_path[0] = 0;\n', ''),
    ('Reset: name', 'GmManifestLoader.cpp', '    m_name[0] = 0;\n', ''),
    ('Reset: tail', 'GmManifestLoader.cpp', '    m_tail = 0;\n    m_head = 0;\n', '    m_head = 0;\n'),
    ('Reset: head', 'GmManifestLoader.cpp', '    m_tail = 0;\n    m_head = 0;\n', '    m_tail = 0;\n'),
    ('Reset: heap', 'GmManifestLoader.cpp', '    m_heap = 0;\n', ''),
    ('Reset: done', 'GmManifestLoader.cpp', '    m_done = 0;\n', ''),
    ('Reset: state', 'GmManifestLoader.cpp', '    m_state = kOpening;\n', ''),
    ('Tokens: not passed', 'GmManifestLoader.cpp', '        OnToken(token, tokenizer);\n', ''),
    ('Tokens: stop on one', 'GmManifestLoader.cpp', 'token != 0; token = tokenizer->Next()', 'token > 1; token = tokenizer->Next()'),
    ('Open: extension', 'GmManifestLoader.cpp', '"MNG"', '"MNH"'),
    ('Open: flags', 'GmManifestLoader.cpp', 'm_file.Open(m_path, "MNG", 2, 0)', 'm_file.Open(m_path, "MNG", 4, 0)'),
    ('Open: failure state', 'GmManifestLoader.cpp', '        m_state = kFailed;\n        return;\n', '        m_state = kDone;\n        return;\n'),
    ('Open: failure ignored', 'GmManifestLoader.cpp', '    if (m_file.Open(m_path, "MNG", 2, 0) != 0) {\n        m_state = kFailed;\n        return;\n    }\n', '    m_file.Open(m_path, "MNG", 2, 0);\n'),
    ('Open: success state', 'GmManifestLoader.cpp', '    m_state = kParsing;\n', '    m_state = kRunning;\n'),
    ('Run: tag', 'GmManifestLoader.cpp', '    AtHeapManager::s_instance->m_override = m_heap;\n', ''),
    ('Run: restore', 'GmManifestLoader.cpp', '    AtHeapManager::s_instance->m_override = saved;\n', ''),
    ('Run: not run', 'GmManifestLoader.cpp', '        m_head->Run();\n', ''),
    ('Run: no advance', 'GmManifestLoader.cpp', '        m_head = m_head->m_next;\n', '        if (m_head->m_next == 0) m_head = 0;\n'),
    ('Run: tail kept', 'GmManifestLoader.cpp', '            m_tail = 0;\n            Finished();\n', '            Finished();\n'),
    ('Run: not finished', 'GmManifestLoader.cpp', '            m_tail = 0;\n            Finished();\n', '            m_tail = 0;\n'),
    ('Run: finish before tail', 'GmManifestLoader.cpp', '            m_tail = 0;\n            Finished();\n', '            Finished();\n            m_tail = 0;\n'),
    ('Enqueue: tail not set first', 'GmManifestLoader.cpp', '        m_head = item;\n        m_tail = item;\n', '        m_head = item;\n'),
    ('Enqueue: head from tail', 'GmManifestLoader.cpp', 'if (m_head == 0) {\n        m_head = item;', 'if (m_tail == 0) {\n        m_head = item;'),
    ('Enqueue: first link cleared', 'GmManifestLoader.cpp', '        m_head = item;\n        m_tail = item;\n', '        m_head = item;\n        m_tail = item;\n        item->m_next = 0;\n'),
    ('Enqueue: not linked', 'GmManifestLoader.cpp', '    m_tail->m_next = item;\n', ''),
    ('Enqueue: link kept', 'GmManifestLoader.cpp', '    item->m_next = 0;\n', ''),
    ('Enqueue: tail not moved', 'GmManifestLoader.cpp', '    item->m_next = 0;\n    m_tail = item;\n', '    item->m_next = 0;\n'),
    ('Update: no first clock', 'GmManifestLoader.cpp', 'const unsigned int start = AtPlatformMilliseconds();\n    if (budgetMilliseconds == 0) {\n        return 0;\n    }\n', 'if (budgetMilliseconds == 0) {\n        return 0;\n    }\n    const unsigned int start = AtPlatformMilliseconds();\n'),
    ('Update: zero budget runs', 'GmManifestLoader.cpp', '    if (budgetMilliseconds == 0) {\n        return 0;\n    }\n', ''),
    ('Update: zero budget result', 'GmManifestLoader.cpp', '    if (budgetMilliseconds == 0) {\n        return 0;\n    }\n', '    if (budgetMilliseconds == 0) {\n        return 1;\n    }\n'),
    ('Update: null tokenizer state', 'GmManifestLoader.cpp', '                m_file.Close();\n                m_state = kFailed;\n', '                m_file.Close();\n                m_state = kDone;\n'),
    ('Update: null tokenizer not closed', 'GmManifestLoader.cpp', '                m_file.Close();\n                m_state = kFailed;\n', '                m_state = kFailed;\n'),
    ('Update: no Parse', 'GmManifestLoader.cpp', '            Parse(tokenizer);\n', ''),
    ('Update: no tokens', 'GmManifestLoader.cpp', '            ReadTokens(tokenizer);\n', ''),
    ('Update: not closed', 'GmManifestLoader.cpp', '            m_file.Close();\n            AfterParse();\n', '            AfterParse();\n'),
    ('Update: closed after', 'GmManifestLoader.cpp', '            m_file.Close();\n            AfterParse();\n', '            AfterParse();\n            m_file.Close();\n'),
    ('Update: no AfterParse', 'GmManifestLoader.cpp', '            AfterParse();\n            m_state', '            m_state'),
    ('Update: after running state', 'GmManifestLoader.cpp', '            m_state = kRunning;\n', '            m_state = kDone;\n'),
    ('Update: AfterParse sees next state', 'GmManifestLoader.cpp', '            AfterParse();\n            m_state = kRunning;\n', '            m_state = kRunning;\n            AfterParse();\n'),
    ('Update: done flag', 'GmManifestLoader.cpp', '            m_done = m_head == 0;\n', '            m_done = 0;\n'),
    ('Update: done flag kept', 'GmManifestLoader.cpp', '            m_done = m_head == 0;\n            if (m_done != 0) {', '            if (m_head == 0) {\n                m_done = 1;\n            }\n            if (m_done != 0) {'),
    ('Update: done state', 'GmManifestLoader.cpp', '                m_state = kDone;\n            }\n            break;', '                m_state = kFailed;\n            }\n            break;'),
    ('Update: done result', 'GmManifestLoader.cpp', '        case kDone:\n        case kFailed:\n            return 1;', '        case kDone:\n        case kFailed:\n            return 0;'),
    ('Update: failed runs out the clock', 'GmManifestLoader.cpp', '        case kDone:\n        case kFailed:\n            return 1;', '        case kDone:\n            return 1;\n        case kFailed:\n            break;'),
    ('Update: done runs out the clock', 'GmManifestLoader.cpp', '        case kDone:\n        case kFailed:\n            return 1;', '        case kFailed:\n            return 1;\n        case kDone:\n            break;'),
    ('Update: unknown state ends', 'GmManifestLoader.cpp', '        default:\n            break;\n', '        default:\n            return 1;\n'),
    ('Update: budget edge', 'GmManifestLoader.cpp', 'AtPlatformMilliseconds() - start < budgetMilliseconds', 'AtPlatformMilliseconds() - start <= budgetMilliseconds'),
    ('Update: absolute clock', 'GmManifestLoader.cpp', 'AtPlatformMilliseconds() - start < budgetMilliseconds', 'AtPlatformMilliseconds() < start + budgetMilliseconds'),
    ('Update: one step only', 'GmManifestLoader.cpp', 'while (AtPlatformMilliseconds() - start < budgetMilliseconds);', 'while (0);'),
    ('Update: signed clock', 'GmManifestLoader.cpp', 'AtPlatformMilliseconds() - start < budgetMilliseconds', '(int)(AtPlatformMilliseconds() - start) < (int)budgetMilliseconds - 1'),
    ('Update: out of time result', 'GmManifestLoader.cpp', '    } while (AtPlatformMilliseconds() - start < budgetMilliseconds);\n    return 0;\n', '    } while (AtPlatformMilliseconds() - start < budgetMilliseconds);\n    return 1;\n'),
    ('Destructor: no reset', 'GmManifestLoader.cpp', 'GmManifestLoader::~GmManifestLoader() {\n    Reset();\n}\n', 'GmManifestLoader::~GmManifestLoader() {\n}\n'),
    ('Constructor: no reset', 'GmManifestLoader.cpp', 'GmManifestLoader::GmManifestLoader() {\n    Reset();\n}\n', 'GmManifestLoader::GmManifestLoader() {\n}\n'),
]


MAT_MUTANTS = [
    ('Flag: color modulate', 'AtMaterial.cpp', 'case kColorModulateWord:\n            m_flags |= kColorModulate;\n', 'case kColorModulateWord:\n'),
    ('Flag: alpha modulate', 'AtMaterial.cpp', 'case kAlphaModulateWord:\n            m_flags |= kAlphaModulate;\n', 'case kAlphaModulateWord:\n'),
    ('Flag: clamp', 'AtMaterial.cpp', 'case kClampWord:\n            m_flags |= kClamp;\n', 'case kClampWord:\n'),
    ('Flag: gouraud', 'AtMaterial.cpp', 'case kGouraudShadeWord:\n            m_flags |= kGouraudShade;\n', 'case kGouraudShadeWord:\n'),
    ('Flag: anti aliased', 'AtMaterial.cpp', 'case kAntiAliasedTransparencyWord:\n            m_flags |= kAntiAliasedTransparency;\n', 'case kAntiAliasedTransparencyWord:\n'),
    ('Flag: emissive', 'AtMaterial.cpp', 'case kEmissiveWord:\n            m_flags |= kEmissive;\n', 'case kEmissiveWord:\n'),
    ('Flag: lighting', 'AtMaterial.cpp', 'case kDisableLightingWord:\n            m_flags |= kDisableLighting;\n', 'case kDisableLightingWord:\n'),
    ('Flag: over bright', 'AtMaterial.cpp', 'case kOverBrightWord:\n            m_flags |= kOverBright;\n', 'case kOverBrightWord:\n'),
    ('Flag: fog', 'AtMaterial.cpp', 'case kDisableFoggingWord:\n            m_flags |= kDisableFogging;\n', 'case kDisableFoggingWord:\n'),
    ('Flag: color writes', 'AtMaterial.cpp', 'case kDisableColorWritesWord:\n            m_flags |= kDisableColorWrites;\n', 'case kDisableColorWritesWord:\n'),
    ('Flag: depth buffer', 'AtMaterial.cpp', 'case kDisableDepthBufferWord:\n            m_flags |= kDisableDepthBuffer;\n', 'case kDisableDepthBufferWord:\n'),
    ('Flag: depth writes', 'AtMaterial.cpp', 'case kDisableDepthWritesWord:\n            m_flags |= kDisableDepthWrites;\n', 'case kDisableDepthWritesWord:\n'),
    ('Word: decal', 'AtMaterial.cpp', 'case kColorDecal:\n            m_flags &= ~(unsigned int)kColorModulate;\n', 'case kColorDecal:\n'),
    ('Word: alpha decal', 'AtMaterial.cpp', 'case kAlphaDecal:\n            m_flags &= ~(unsigned int)kAlphaModulate;\n', 'case kAlphaDecal:\n'),
    ('Word: wrap', 'AtMaterial.cpp', 'case kWrap:\n            m_flags &= ~(unsigned int)kClamp;\n', 'case kWrap:\n'),
    ('Word: flat', 'AtMaterial.cpp', 'case kFlatShade:\n            m_flags &= ~(unsigned int)kGouraudShade;\n', 'case kFlatShade:\n'),
    ('Word: vertex alpha', 'AtMaterial.cpp', 'case kVertexAlpha:\n            m_flags &= ~(unsigned int)kConstantAlpha;\n', 'case kVertexAlpha:\n'),
    ('Word: ids shifted', 'AtMaterial.cpp', 'kDisableDepthWritesWord, kDisableColorWritesWord, kDepthCompareWord,', 'kDisableColorWritesWord, kDisableDepthWritesWord, kDepthCompareWord,'),
    ('Word: unknown', 'AtMaterial.cpp', '        default:\n            tokenizer->Error(kBadKeyword);\n            break;\n        }\n    }\n    if (owner', '        default:\n            break;\n        }\n    }\n    if (owner'),
    ('Single: other', 'AtMaterial.cpp', 'm_flags = (m_flags & ~(unsigned int)kDoubleSided) | kSingleSided;', 'm_flags |= kSingleSided;'),
    ('Double: other', 'AtMaterial.cpp', 'm_flags = (m_flags & ~(unsigned int)kSingleSided) | kDoubleSided;', 'm_flags |= kDoubleSided;'),
    ('Ambient: field', 'AtMaterial.cpp', '            m_ambient.Read(tokenizer);\n', '            m_diffuse.Read(tokenizer);\n'),
    ('Specular: field', 'AtMaterial.cpp', '            m_specular.Read(tokenizer);\n', '            m_diffuse.Read(tokenizer);\n'),
    ('Constant: flag', 'AtMaterial.cpp', '            m_flags |= kConstantColor;\n', ''),
    ('Constant: blue', 'AtMaterial.cpp', '            m_constant.b = (unsigned char)tokenizer->ExpectInt();\n', ''),
    ('ConstantAlpha: flag', 'AtMaterial.cpp', '            m_flags |= kConstantAlpha;\n', ''),
    ('AlphaCompare: never', 'AtMaterial.cpp', 'if (m_alphaCompare != 0 && m_alphaCompare != 7) {', 'if (m_alphaCompare != 0) {'),
    ('AlphaCompare: always', 'AtMaterial.cpp', 'if (m_alphaCompare != 0 && m_alphaCompare != 7) {', 'if (m_alphaCompare != 7) {'),
    ('AlphaCompare: flag', 'AtMaterial.cpp', '            m_flags |= kAlphaCompare;\n            break;\n', '            break;\n'),
    ('DepthCompare: flag', 'AtMaterial.cpp', '            m_flags |= kDepthCompare;\n', ''),
    ('Blend: alpha field', 'AtMaterial.cpp', 'm_blendAlpha = (unsigned char)ReadBlendMode(tokenizer, next);', 'm_blend = (unsigned char)ReadBlendMode(tokenizer, next);'),
    ('Blend: flag', 'AtMaterial.cpp', '            m_flags |= kBlend;\n', ''),
    ('Blend: separate flag', 'AtMaterial.cpp', '            m_flags |= kBlendSeparateAlpha;\n', ''),
    ('Simple: shader', 'AtMaterial.cpp', '            m_shader = 0;\n            const unsigned int first', '            const unsigned int first'),
    ('Simple: bound', 'AtMaterial.cpp', 'if (first + 1 > store->m_textureCapacity) {', 'if (first > store->m_textureCapacity) {'),
    ('Simple: count', 'AtMaterial.cpp', '            m_textureCount = 1;\n', ''),
    ('Simple: index', 'AtMaterial.cpp', 'm_textures = store->m_textures + first;\n            break;', 'm_textures = store->m_textures;\n            break;'),
    ('Shader: name space', 'AtMaterial.cpp', '                look = nameSpace;\n', ''),
    ('Shader: caller name space', 'AtMaterial.cpp', '            const char* look = ns;\n', '            const char* look = nullptr;\n'),
    ('Shader: missing', 'AtMaterial.cpp', '            if (m_shader == 0) {\n                tokenizer->Error(kNoSuchShader);\n            }\n', ''),
    ('Shader: stage bound', 'AtMaterial.cpp', 'if (m_stageCount + firstStage > store->m_stageCapacity) {', 'if (m_stageCount + firstStage >= store->m_stageCapacity) {'),
    ('Shader: no stages', 'AtMaterial.cpp', '            if (m_stageCount != 0) {\n', '            {\n'),
    ('Shader: texture bound', 'AtMaterial.cpp', 'if (m_textureCount + firstTexture > store->m_textureCapacity) {', 'if (m_textureCount + firstTexture >= store->m_textureCapacity) {'),
    ('Shader: no textures', 'AtMaterial.cpp', '            if (m_textureCount != 0) {\n', '            {\n'),
    ('Ranged: real', 'AtMaterial.cpp', '            m_range = tokenizer->ExpectReal();\n', '            tokenizer->ExpectReal();\n'),
    ('Ranged: flag', 'AtMaterial.cpp', '            m_flags |= kRangedAntiAliasedTransparency;\n', ''),
    ('Stencil: order', 'AtMaterial.cpp', '            m_stencilFail = (unsigned char)ReadStencilAction(tokenizer);\n            m_stencilDepthFail = (unsigned char)ReadStencilAction(tokenizer);\n', '            m_stencilDepthFail = (unsigned char)ReadStencilAction(tokenizer);\n            m_stencilFail = (unsigned char)ReadStencilAction(tokenizer);\n'),
    ('Stencil: flag', 'AtMaterial.cpp', '            m_flags |= kStencilOperation;\n', ''),
    ('Power: flag', 'AtMaterial.cpp', '            m_flags |= kPower;\n', ''),
    ('Sort: field', 'AtMaterial.cpp', '            m_sortBias = tokenizer->ExpectReal();\n', '            tokenizer->ExpectReal();\n'),
    ('Projection: field', 'AtMaterial.cpp', '            m_projectionMask = tokenizer->ExpectInt();\n', '            tokenizer->ExpectInt();\n'),
    ('After: no textures', 'AtMaterial.cpp', 'if (owner->m_noTextures < 0) {', 'if (owner->m_noTextures <= 0) {'),
    ('After: shader kept', 'AtMaterial.cpp', '    if (owner->m_noTextures < 0) {\n        m_shader = 0;\n', '    if (owner->m_noTextures < 0) {\n'),
    # Dropped, equivalent: After: fog already: it only ORs in a flag that is already set.
    ('After: fog without blend', 'AtMaterial.cpp', 'if (!(m_flags & kDisableFogging) && (m_flags & kBlend) && (m_blend == 1 || m_blend == 9)) {', 'if (!(m_flags & kDisableFogging) && (m_blend == 1 || m_blend == 9)) {'),
    ('After: fog mode 1', 'AtMaterial.cpp', '(m_blend == 1 || m_blend == 9)', '(m_blend == 9)'),
    ('After: fog mode 9', 'AtMaterial.cpp', '(m_blend == 1 || m_blend == 9)', '(m_blend == 1)'),
    ('After: blend alpha', 'AtMaterial.cpp', '    if (!(m_flags & kBlendSeparateAlpha)) {\n        m_blendAlpha = m_blend;\n    }\n', ''),
    ('After: no hook', 'AtMaterial.cpp', '    OnLoaded(owner);\n    m_flags |= kLoaded;\n}\n\nvoid AtMaterial::Copy', '    m_flags |= kLoaded;\n}\n\nvoid AtMaterial::Copy'),
    ('After: not loaded', 'AtMaterial.cpp', '    OnLoaded(owner);\n    m_flags |= kLoaded;\n}\n\nvoid AtMaterial::Copy', '    OnLoaded(owner);\n}\n\nvoid AtMaterial::Copy'),
    ('Read: no reset', 'AtMaterial.cpp', 'void AtMaterial::Read(AtMaterialOwner* owner, AtMaterialDatabase* store, AtIoTokenizer* tokenizer, const char* ns) {\n    if (m_flags & kLoaded) {\n        Reset();\n    }\n', 'void AtMaterial::Read(AtMaterialOwner* owner, AtMaterialDatabase* store, AtIoTokenizer* tokenizer, const char* ns) {\n'),
    ('Read: store', 'AtMaterial.cpp', '    m_store = store;\n    tokenizer->ExpectLeftCurly();\n', '    tokenizer->ExpectLeftCurly();\n'),
    ('Read: curly', 'AtMaterial.cpp', '    tokenizer->ExpectLeftCurly();\n', ''),
    ('Copy: no reset', 'AtMaterial.cpp', 'const AtMaterial& from) {\n    if (m_flags & kLoaded) {\n        Reset();\n    }\n', 'const AtMaterial& from) {\n'),
    ('Copy: flags', 'AtMaterial.cpp', '    m_flags = from.m_flags;\n', ''),
    ('Copy: unknown28', 'AtMaterial.cpp', '    m_unknown28 = from.m_unknown28;\n', ''),
    ('Copy: power too', 'AtMaterial.cpp', '    m_range = from.m_range;\n', '    m_range = from.m_range;\n    m_power = from.m_power;\n'),
    ('Copy: bias too', 'AtMaterial.cpp', '    m_range = from.m_range;\n', '    m_range = from.m_range;\n    m_sortBias = from.m_sortBias;\n'),
    ('Copy: stage use', 'AtMaterial.cpp', '        store->m_stageUsed = first + m_stageCount;\n', ''),
    ('Copy: stage data', 'AtMaterial.cpp', '        memcpy(m_stages, from.m_stages, m_stageCount * sizeof(void*));\n', ''),
    ('Copy: stage test', 'AtMaterial.cpp', '    if (from.m_stages != 0) {\n', '    if (from.m_stageCount != 0) {\n'),
    ('Copy: texture use', 'AtMaterial.cpp', '        store->m_textureUsed = first + m_textureCount;\n', ''),
    ('Copy: texture data', 'AtMaterial.cpp', '        memcpy(m_textures, from.m_textures, m_textureCount * sizeof(void*));\n', ''),
    ('Copy: texture test', 'AtMaterial.cpp', '    if (from.m_textures != 0) {\n', '    if (from.m_textureCount != 0) {\n'),
    ('Describe: loaded', 'AtMaterial.cpp', 'out->m_flags = m_flags & ~(unsigned int)kLoaded;', 'out->m_flags = m_flags;'),
    ('Describe: store', 'AtMaterial.cpp', '    out->m_store = m_store;\n', ''),
    ('Describe: compare', 'AtMaterial.cpp', '    out->m_depthCompare = m_depthCompare;\n', ''),
    ('Reset: depth', 'AtMaterial.cpp', '    m_projectionMask = -1;\n    m_sortBias = 0.0f;\n    m_power = 0.0f;\n    m_store = 0;\n}\n\nvoid AtMaterial::SetAlphaCompare', '    m_sortBias = 0.0f;\n    m_power = 0.0f;\n    m_store = 0;\n}\n\nvoid AtMaterial::SetAlphaCompare'),
    ('SetAlpha: flag', 'AtMaterial.cpp', '    m_flags |= kAlphaCompare;\n    m_alphaCompare = compare;\n', '    m_alphaCompare = compare;\n'),
    ('Modes: base', 'AtMaterial.cpp', 'return token - kFirstBlendMode + 1;', 'return token - kFirstBlendMode;'),
    ('Modes: top', 'AtMaterial.cpp', 'token <= kLastBlendMode', 'token < kLastBlendMode'),
    ('Compare: equal', 'AtMaterial.cpp', 'case kEqual: return 5;', 'case kEqual: return 6;'),
    ('Compare: less', 'AtMaterial.cpp', 'case kLess: return 1;', 'case kLess: return 2;'),
    ('Stencil word: invert', 'AtMaterial.cpp', 'case kInvert: return 7;', 'case kInvert: return 6;'),
    ('Color: white', 'AtMaterial.cpp', '    a = 255;\n', '    a = 0;\n'),
    ('Color: read order', 'AtMaterial.cpp', '    r = (unsigned char)tokenizer->ExpectInt();\n    g = (unsigned char)tokenizer->ExpectInt();\n', '    g = (unsigned char)tokenizer->ExpectInt();\n    r = (unsigned char)tokenizer->ExpectInt();\n'),
    ('Desc: white', 'AtMaterial.cpp', '    m_specular.SetWhite();\n', ''),
    ('Desc: mask', 'AtMaterial.cpp', '    m_projectionMask = -1;\n    m_range = 3.4028234663852886e38f;  // 0x7F7FFFFF\n', '    m_range = 3.4028234663852886e38f;  // 0x7F7FFFFF\n'),
]


QUA_MUTANTS = [
    ('Product: x sign', 'AtQuaternion.cpp', 'x = (float)(D(a.x) * b.w + D(b.z) * a.y + D(b.x) * a.w - D(b.y) * a.z);', 'x = (float)(D(a.x) * b.w + D(b.z) * a.y + D(b.x) * a.w + D(b.y) * a.z);'),
    ('Product: y term', 'AtQuaternion.cpp', 'y = (float)(D(b.y) * a.w + D(b.x) * a.z + D(a.y) * b.w - D(a.x) * b.z);', 'y = (float)(D(b.y) * a.w + D(b.x) * a.z + D(a.y) * b.y - D(a.x) * b.z);'),
    ('Product: z term', 'AtQuaternion.cpp', 'z = (float)(D(a.x) * b.y + D(b.z) * a.w + D(a.z) * b.w - D(b.x) * a.y);', 'z = (float)(D(a.x) * b.y + D(b.z) * a.w + D(a.z) * b.w - D(b.y) * a.x);'),
    ('Product: w dropped', 'AtQuaternion.cpp', 'w = (float)(D(a.w) * b.w - (D(a.x) * b.x + D(b.z) * a.z + D(b.y) * a.y));', 'w = (float)(D(a.w) * b.w - (D(a.x) * b.x + D(b.z) * a.z));'),
    ('Product: single', 'AtQuaternion.cpp', 'x = (float)(D(a.x) * b.w + D(b.z) * a.y + D(b.x) * a.w - D(b.y) * a.z);', 'x = a.x * b.w + b.z * a.y + b.x * a.w - b.y * a.z;'),
    ('Lerp: dot term', 'AtQuaternion.cpp', 'const double dot = D(to.w) * from.w + D(to.z) * from.z + D(from.y) * to.y + D(from.x) * to.x;\n    if (dot > 0.0) {', 'const double dot = D(to.w) * from.w + D(to.z) * from.z + D(from.y) * to.y;\n    if (dot > 0.0) {'),
    ('Lerp: at zero', 'AtQuaternion.cpp', '    if (dot > 0.0) {\n        x = (float)((D(to.x) - from.x) * t + from.x);', '    if (dot >= 0.0) {\n        x = (float)((D(to.x) - from.x) * t + from.x);'),
    ('Lerp: unordered', 'AtQuaternion.cpp', '    if (dot > 0.0) {\n        x = (float)((D(to.x) - from.x) * t + from.x);', '    if (!(dot <= 0.0)) {\n        x = (float)((D(to.x) - from.x) * t + from.x);'),
    ('Lerp: w', 'AtQuaternion.cpp', 'w = (float)((D(to.w) - from.w) * t + from.w);', 'w = from.w;'),
    ('Lerp: far x', 'AtQuaternion.cpp', 'x = (float)(from.x - (D(from.x) + to.x) * t);', 'x = (float)(from.x + (D(from.x) + to.x) * t);'),
    ('Lerp: far w', 'AtQuaternion.cpp', 'w = (float)(from.w - (D(to.w) + from.w) * t);', 'w = (float)(from.w - (D(to.w) - from.w) * t);'),
    ('Rows: stored sum', 'AtQuaternion.cpp', 'sqrt(D(row0.x) - storedSum12 + 1.0)', 'sqrt(D(row0.x) - sum12 + 1.0)'),
    ('Rows: trace', 'AtQuaternion.cpp', '    if (trace > 0.0) {\n', '    if (trace >= 0.0) {\n'),
    ('Rows: unordered', 'AtQuaternion.cpp', '    if (trace > 0.0) {\n', '    if (!(trace <= 0.0)) {\n'),
    ('Rows: half', 'AtQuaternion.cpp', 'w = (float)(0.5f * root);\n        const double scale = 0.5f / root;\n        x = (float)((D(row2.y) - row1.z) * scale);', 'w = (float)(0.5f * root);\n        const double scale = 1.0f / root;\n        x = (float)((D(row2.y) - row1.z) * scale);'),
    ('Rows: y sign', 'AtQuaternion.cpp', 'y = (float)((D(row0.z) - row2.x) * scale);', 'y = (float)((D(row2.x) - row0.z) * scale);'),
    ('Rows: first test', 'AtQuaternion.cpp', '    if (row1.y > row0.x) {\n', '    if (row1.y >= row0.x) {\n'),
    ('Rows: second test', 'AtQuaternion.cpp', '        if (row2.z > row1.y) {\n', '        if (row2.z >= row1.y) {\n'),
    ('Rows: third test', 'AtQuaternion.cpp', '    } else if (row2.z > row0.x) {\n', '    } else if (row2.z >= row0.x) {\n'),
    ('Rows: case 2 root', 'AtQuaternion.cpp', 'sqrt(row2.z - (D(row0.x) + row1.y) + 1.0)', 'sqrt(row2.z - (D(row0.x) - row1.y) + 1.0)'),
    ('Rows: case 2 w', 'AtQuaternion.cpp', 'w = (float)((D(row1.x) - row0.y) * scale);\n        x = (float)((D(row0.z) + row2.x) * scale);', 'w = (float)((D(row1.x) + row0.y) * scale);\n        x = (float)((D(row0.z) + row2.x) * scale);'),
    ('Rows: case 1 root', 'AtQuaternion.cpp', 'sqrt(row1.y - (D(row0.x) + row2.z) + 1.0)', 'sqrt(row1.y - (D(row0.x) + row1.z) + 1.0)'),
    ('Rows: case 1 z', 'AtQuaternion.cpp', 'z = (float)((D(row1.z) + row2.y) * scale);\n        x = (float)((D(row0.y) + row1.x) * scale);', 'z = (float)((D(row1.z) - row2.y) * scale);\n        x = (float)((D(row0.y) + row1.x) * scale);'),
    ('Rows: case 0 y', 'AtQuaternion.cpp', 'y = (float)((D(row0.y) + row1.x) * scale);\n        z = (float)((D(row0.z) + row2.x) * scale);', 'y = (float)((D(row0.y) - row1.x) * scale);\n        z = (float)((D(row0.z) + row2.x) * scale);'),
    ('Reflection: factor', 'AtQuaternion.cpp', '* 2.0);', '* 1.0);'),
    ('Reflection: dot', 'AtQuaternion.cpp', 'D(normal.x) * value.x + D(value.z) * normal.z + D(value.y) * normal.y', 'D(normal.x) * value.x + D(value.z) * normal.z'),
    ('Reflection: sign', 'AtQuaternion.cpp', 'x = (float)-(scale * normal.x + value.x);', 'x = (float)(scale * normal.x + value.x);'),
    ('Reflection: w', 'AtQuaternion.cpp', '    w = value.w;\n', '    w = normal.w;\n'),
    ('Multiply: stale', 'AtQuaternion.cpp', 'y = (float)(d * other.y + a * other.z + b * other.w - c * other.x);', 'y = (float)(d * other.y + x * other.z + b * other.w - c * other.x);'),
    ('Multiply: x sign', 'AtQuaternion.cpp', 'x = (float)(c * other.y + d * other.x + a * other.w - b * other.z);', 'x = (float)(c * other.y + d * other.x + a * other.w + b * other.z);'),
    ('Multiply: z term', 'AtQuaternion.cpp', 'z = (float)(d * other.z + b * other.x + c * other.w - a * other.y);', 'z = (float)(d * other.z + b * other.x + c * other.w);'),
    ('Multiply: w term', 'AtQuaternion.cpp', 'w = (float)(d * other.w - (b * other.y + c * other.z + a * other.x));', 'w = (float)(d * other.w - (b * other.y + c * other.z));'),
    ('Normalize: zero', 'AtQuaternion.cpp', '    if (lengthSquared == 0.0) {\n        x = 0.0f;\n        y = 0.0f;\n        z = 0.0f;\n        w = 1.0f;\n        return;\n    }\n', ''),
    ('Normalize: w', 'AtQuaternion.cpp', '        w = 1.0f;\n', '        w = 0.0f;\n'),
    ('Normalize: term', 'AtQuaternion.cpp', 'a * a + b * b + c * c + d * d;', 'a * a + b * b + c * c;'),
    ('Normalize: no root', 'AtQuaternion.cpp', 'const double scale = 1.0 / sqrt(lengthSquared);', 'const double scale = 1.0 / lengthSquared;'),
    ('Slerp: dot term', 'AtQuaternion.cpp', 'const double dot = D(to.w) * from.w + D(to.z) * from.z + D(from.x) * to.x + D(to.y) * from.y;', 'const double dot = D(to.w) * from.w + D(to.z) * from.z + D(from.x) * to.x;'),
    ('Slerp: no flip', 'AtQuaternion.cpp', '    if (dot < 0.0) {\n', '    if (dot < -2.0) {\n'),
    ('Slerp: cosine', 'AtQuaternion.cpp', '        cosine = -cosine;\n', ''),
    ('Slerp: flip y', 'AtQuaternion.cpp', '        target.y = -to.y;\n', '        target.y = to.y;\n'),
    ('Slerp: threshold', 'AtQuaternion.cpp', 'kSmallAngle = 0.01f;', 'kSmallAngle = 0.02f;'),
    ('Slerp: rounded angle', 'AtQuaternion.cpp', '(float)(1.0 / AtSin(exactAngle));', '(float)(1.0 / AtSin(angle));'),
    ('Slerp: exact scale', 'AtQuaternion.cpp', 'k0 = AtSin((1.0 - D(t)) * angle) * scale;', 'k0 = AtSin((1.0 - D(t)) * exactAngle) * scale;'),
    ('Slerp: k1', 'AtQuaternion.cpp', 'k1 = AtSin(D(angle) * t) * scale;', 'k1 = AtSin(D(angle) * (1.0 - t)) * scale;'),
    ('Slerp: near k0', 'AtQuaternion.cpp', '        k0 = 1.0 - D(t);\n', '        k0 = t;\n'),
    ('Slerp: near k1', 'AtQuaternion.cpp', '        k1 = t;\n', '        k1 = 1.0 - D(t);\n'),
    ('Slerp: from x', 'AtQuaternion.cpp', 'x = (float)(k0 * from.x + k1 * target.x);', 'x = (float)(k1 * from.x + k0 * target.x);'),
    ('Slerp: w', 'AtQuaternion.cpp', 'w = (float)(k1 * target.w + k0 * from.w);', 'w = (float)(k1 * to.w + k0 * from.w);'),
    ('Rotate: order', 'AtQuaternion.cpp', 'result.SetProduct(pure, *this);', 'result.SetProduct(*this, pure);'),
    ('Rotate: no conjugate', 'AtQuaternion.cpp', '    pure.x = -x;\n', '    pure.x = x;\n'),
    ('Rotate: pure w', 'AtQuaternion.cpp', '    pure.w = 0.0f;\n', '    pure.w = 1.0f;\n'),
    ('Rotate: last', 'AtQuaternion.cpp', '    vector->z = result.z;\n', ''),
    ('Axis: half', 'AtQuaternion.cpp', 'const double half = D(angle) * 0.5f;', 'const double half = D(angle);'),
    ('Axis: w', 'AtQuaternion.cpp', 'w = (float)AtCos(half);', 'w = (float)AtSin(half);'),
    ('Axis: z', 'AtQuaternion.cpp', 'z = (float)(s * axis.z);\n}', 'z = axis.z;\n}'),
    ('Cross: x', 'AtVector3.cpp', 'x = (float)((double)b.z * a.y - (double)b.y * a.z);', 'x = (float)((double)b.y * a.z - (double)b.z * a.y);'),
    ('Cross: y', 'AtVector3.cpp', 'y = (float)((double)b.x * a.z - (double)a.x * b.z);', 'y = (float)((double)a.x * b.z - (double)b.x * a.z);'),
    ('Cross: z', 'AtVector3.cpp', 'z = (float)((double)a.x * b.y - (double)b.x * a.y);', 'z = (float)((double)a.x * b.y);'),
    ('Difference: x', 'AtVector3.cpp', 'x = (float)((double)a.x - b.x);', 'x = (float)((double)b.x - a.x);'),
    ('Difference: z', 'AtVector3.cpp', 'z = (float)((double)a.z - b.z);', 'z = a.z;'),
]


FMT_MUTANTS = [
    ('Names: inverted', 'AtPixelFormat.cpp', 'if (!EqualIgnoringCase(tokenizer->m_string, entry->m_name)) {', 'if (EqualIgnoringCase(tokenizer->m_string, entry->m_name)) {'),
    ('Names: LUMD', 'AtPixelFormat.cpp', '{"LUMD", AtPixelFormat::kLumd},   ', ''),
    ('Names: LUM flag', 'AtPixelFormat.cpp', '{"LUM", AtPixelFormat::kLum},     ', '{"LUM", AtPixelFormat::kIndex},     '),
    ('Names: TILED', 'AtPixelFormat.cpp', '{"TILED", AtPixelFormat::kTiled}, ', '{"TILED", AtPixelFormat::kSwizz}, '),
    ('Run: gap', 'AtPixelFormat.cpp', '    while ((mask & 1) != 0) {\n        mask >>= 1;\n        count++;\n    }\n', '    while (mask != 0) {\n        count += mask & 1;\n        mask >>= 1;\n    }\n'),
    ('Run: leading', 'AtPixelFormat.cpp', '    while ((mask & 1) == 0) {\n        mask >>= 1;\n    }\n    unsigned int count = 0;\n', '    unsigned int count = 0;\n'),
    ('Run: empty', 'AtPixelFormat.cpp', 'inline unsigned int RunLength(unsigned int mask) {\n    if (mask == 0) {\n        return 0;\n    }\n', 'inline unsigned int RunLength(unsigned int mask) {\n    if (mask == 0) {\n        return 32;\n    }\n'),
    ('Lowest: empty', 'AtPixelFormat.cpp', '        return 32;\n', '        return 0;\n'),
    # Dropped, equivalent: Shift: right unbounded: a count past 31 here only ever meets a value that is 0 at either shift.
    # Dropped, equivalent: Shift: left unbounded: likewise, or its result is cut to a byte.
    ('Ctor: flags', 'AtPixelFormat.cpp', 'AtPixelFormat::AtPixelFormat() {\n    m_flags = 0;\n', 'AtPixelFormat::AtPixelFormat() {\n'),
    ('Ctor: mask3', 'AtPixelFormat.cpp', '    m_mask3 = 0;\n}\n\nvoid AtPixelFormat::Clear', '}\n\nvoid AtPixelFormat::Clear'),
    ('Clear: bits', 'AtPixelFormat.cpp', 'void AtPixelFormat::Clear() {\n    m_flags = 0;\n    m_bits = 0;\n', 'void AtPixelFormat::Clear() {\n    m_flags = 0;\n'),
    ('Clear: mask1', 'AtPixelFormat.cpp', '    m_mask0 = 0;\n    m_mask1 = 0;\n    m_mask2 = 0;\n    m_mask3 = 0;\n}\n\nvoid AtPixelFormat::Copy', '    m_mask0 = 0;\n    m_mask2 = 0;\n    m_mask3 = 0;\n}\n\nvoid AtPixelFormat::Copy'),
    ('Copy: mask3', 'AtPixelFormat.cpp', '    m_mask3 = other.m_mask3;\n', ''),
    ('Copy: flags', 'AtPixelFormat.cpp', '    m_flags = other.m_flags;\n', ''),
    ('Set: order', 'AtPixelFormat.cpp', 'void AtPixelFormat::Set(unsigned short flags, unsigned short bits, unsigned int mask0, unsigned int mask1, unsigned int mask2, unsigned int mask3) {\n    m_flags = flags;\n    m_bits = bits;\n', 'void AtPixelFormat::Set(unsigned short flags, unsigned short bits, unsigned int mask0, unsigned int mask1, unsigned int mask2, unsigned int mask3) {\n    m_flags = bits;\n    m_bits = flags;\n'),
    ('Rgba: swizz', 'AtPixelFormat.cpp', '        m_flags = kRgba | kSwizz;\n', '        m_flags = kRgba;\n'),
    ('Rgba: test', 'AtPixelFormat.cpp', '    m_flags = kRgba;\n    if (swizzled != 0) {', '    m_flags = kRgba;\n    if (swizzled == 1) {'),
    ('Rgba: mask2', 'AtPixelFormat.cpp', '    m_mask1 = mask1;\n    m_mask2 = mask2;\n    m_mask3 = mask3;\n    m_flags = kRgba;', '    m_mask1 = mask1;\n    m_mask2 = mask3;\n    m_mask3 = mask3;\n    m_flags = kRgba;'),
    ('Index: flag', 'AtPixelFormat.cpp', '    m_flags = kIndex;\n', '    m_flags = kLum;\n'),
    ('Index: mask1', 'AtPixelFormat.cpp', '    m_mask0 = mask0;\n    m_mask1 = 0;\n    m_mask2 = 0;\n    m_mask3 = mask3;\n    m_flags = kIndex;', '    m_mask0 = mask0;\n    m_mask2 = 0;\n    m_mask3 = mask3;\n    m_flags = kIndex;'),
    ('Lum: flag', 'AtPixelFormat.cpp', '    m_flags = kLum;\n', '    m_flags = kIndex;\n'),
    ('Lum: swizz', 'AtPixelFormat.cpp', '        m_flags = kLum | kSwizz;\n', '        m_flags = kLum;\n'),
    ('Lum: mask3', 'AtPixelFormat.cpp', '    m_mask3 = mask3;\n    m_flags = kLum;', '    m_mask3 = 0;\n    m_flags = kLum;'),
    ('Depth: mask1', 'AtPixelFormat.cpp', '    m_mask1 = mask1;\n    m_mask2 = 0;\n    m_mask3 = 0;\n    m_flags = kDepth;', '    m_mask1 = 0;\n    m_mask2 = 0;\n    m_mask3 = 0;\n    m_flags = kDepth;'),
    ('Depth: swizz', 'AtPixelFormat.cpp', '        m_flags = kDepth | kSwizz;\n', '        m_flags = kDepth;\n'),
    ('Dxt1: bits', 'AtPixelFormat.cpp', '    m_flags = kDxt1;\n    m_bits = 4;\n', '    m_flags = kDxt1;\n    m_bits = 8;\n'),
    ('Dxt2: bits', 'AtPixelFormat.cpp', '    m_flags = kDxt2;\n    m_bits = 8;\n', '    m_flags = kDxt2;\n    m_bits = 4;\n'),
    ('Dxt3: flag', 'AtPixelFormat.cpp', '    m_flags = kDxt3;\n', '    m_flags = kDxt2;\n'),
    ('Dxt4: swizz', 'AtPixelFormat.cpp', '        m_flags = kDxt4 | kSwizz;\n', '        m_flags = kDxt4;\n'),
    ('Dxt5: flag', 'AtPixelFormat.cpp', '    m_flags = kDxt5;\n', '    m_flags = kDxt4;\n'),
    ('Bits1: mask', 'AtPixelFormat.cpp', 'Bits1() const { return RunLength(m_mask1); }', 'Bits1() const { return RunLength(m_mask0); }'),
    ('Bits2: mask', 'AtPixelFormat.cpp', 'Bits2() const { return RunLength(m_mask2); }', 'Bits2() const { return RunLength(m_mask3); }'),
    ('Shift0: mask', 'AtPixelFormat.cpp', 'Shift0() const { return LowestBit(m_mask0); }', 'Shift0() const { return LowestBit(m_mask1); }'),
    ('Shift3: mask', 'AtPixelFormat.cpp', 'Shift3() const { return LowestBit(m_mask3); }', 'Shift3() const { return LowestBit(m_mask2); }'),
    ('Pack: truncates', 'AtPixelFormat.cpp', 'ShiftRight(bytes[0], 8 - RunLength(m_mask0));', 'ShiftRight(bytes[0], 7 - RunLength(m_mask0));'),
    ('Pack: byte', 'AtPixelFormat.cpp', 'ShiftRight(bytes[3], 8 - RunLength(m_mask3));', 'ShiftRight(bytes[2], 8 - RunLength(m_mask3));'),
    ('Pack: position', 'AtPixelFormat.cpp', '| ShiftLeft(v2, LowestBit(m_mask2))', '| ShiftLeft(v2, LowestBit(m_mask1))'),
    ('Pack: last', 'AtPixelFormat.cpp', ' | ShiftLeft(v3, LowestBit(m_mask3));', ';'),
    # Dropped, equivalent: Unpack: and: bits above a channel are shifted out of the byte in any case.
    ('Unpack: width', 'AtPixelFormat.cpp', '(unsigned char)ShiftLeft(v2, 8 - RunLength(m_mask2));', '(unsigned char)ShiftLeft(v2, 8 - RunLength(m_mask1));'),
    ('Unpack: last', 'AtPixelFormat.cpp', '    bytes[3] = (unsigned char)ShiftLeft(v3, 8 - RunLength(m_mask3));\n', ''),
    ('Equals: flags', 'AtPixelFormat.cpp', 'if (m_flags == other.m_flags && m_bits == other.m_bits && m_mask0 == other.m_mask0', 'if (m_bits == other.m_bits && m_mask0 == other.m_mask0'),
    ('Equals: mask3', 'AtPixelFormat.cpp', ' && m_mask3 == other.m_mask3) {', ') {'),
    ('Layout: bits', 'AtPixelFormat.cpp', 'if (m_flags == other.m_flags && m_bits == other.m_bits && Bits0() == other.Bits0()', 'if (m_flags == other.m_flags && Bits0() == other.Bits0()'),
    ('Layout: last', 'AtPixelFormat.cpp', ' && Bits3() == other.Bits3()) {', ') {'),
    ('AtLeast: flags', 'AtPixelFormat.cpp', 'if (m_flags != other.m_flags || m_bits < other.m_bits) {', 'if (m_bits < other.m_bits) {'),
    ('AtLeast: bits', 'AtPixelFormat.cpp', 'if (m_flags != other.m_flags || m_bits < other.m_bits) {', 'if (m_flags != other.m_flags) {'),
    # Dropped, no longer there: AtLeast: empty. The original's test of an empty mask is inlining showing through, since nothing is less than a width of 0.
    ('AtLeast: first', 'AtPixelFormat.cpp', '    if (Bits0() < other.Bits0()) {\n        return 0;\n    }\n', ''),
    ('AtLeast: others', 'AtPixelFormat.cpp', 'if (Bits1() < other.Bits1() || Bits2() < other.Bits2() || Bits3() < other.Bits3()) {', 'if (Bits1() < other.Bits1() || Bits2() < other.Bits2()) {'),
    ('Read: number', 'AtPixelFormat.cpp', '        m_flags = (unsigned short)tokenizer->m_int;\n', '        m_flags |= (unsigned short)tokenizer->m_int;\n'),
    ('Read: names clear', 'AtPixelFormat.cpp', '                    m_flags |= entry->m_flag;\n', '                    m_flags = entry->m_flag;\n'),
    ('Read: one name', 'AtPixelFormat.cpp', '        while (token == AtIoTokenizer::kString) {', '        if (token == AtIoTokenizer::kString) {'),
    ('Read: put back', 'AtPixelFormat.cpp', '        tokenizer->m_pushedBack = 1;\n', ''),
    ('Read: put back on number', 'AtPixelFormat.cpp', '        m_flags = (unsigned short)tokenizer->m_int;\n', '        m_flags = (unsigned short)tokenizer->m_int;\n        tokenizer->m_pushedBack = 1;\n'),
    ('Read: bits', 'AtPixelFormat.cpp', '    m_bits = (unsigned short)tokenizer->ExpectInt();\n', '    tokenizer->ExpectInt();\n'),
    ('Read: mask order', 'AtPixelFormat.cpp', '    m_mask0 = tokenizer->ExpectInt();\n    m_mask1 = tokenizer->ExpectInt();\n', '    m_mask1 = tokenizer->ExpectInt();\n    m_mask0 = tokenizer->ExpectInt();\n'),
    ('Read: last mask', 'AtPixelFormat.cpp', '    m_mask3 = tokenizer->ExpectInt();\n', ''),
]


VEC_MUTANTS = [
    ('Lerp: x', 'AtVector3.cpp', 'x = (float)(((double)to.x - from.x) * t + from.x);', 'x = (float)(((double)to.x - from.x) * t + to.x);'),
    ('Lerp: y swapped', 'AtVector3.cpp', 'y = (float)(((double)to.y - from.y) * t + from.y);', 'y = (float)(((double)from.y - to.y) * t + from.y);'),
    ('Lerp: z single', 'AtVector3.cpp', 'z = (float)(((double)to.z - from.z) * t + from.z);', 'z = (to.z - from.z) * t + from.z;'),
    # Not distinguished by test, and not an equivalent: Dist2: order. See docs/verification.md.
    ('Dist2: dropped', 'AtVector3.cpp', '    return (float)(dz * dz + dy * dy + dx * dx);', '    return (float)(dz * dz + dy * dy);'),
    # Not distinguished by test, and not an equivalent: Dist: order. See docs/verification.md.
    ('Dist: no root', 'AtVector3.cpp', '    return (float)sqrt(dz * dz + dy * dy + dx * dx);', '    return (float)(dz * dz + dy * dy + dx * dx);'),
    ('Read: order', 'AtVector3.cpp', '    x = tokenizer->ExpectReal();\n    y = tokenizer->ExpectReal();\n', '    y = tokenizer->ExpectReal();\n    x = tokenizer->ExpectReal();\n'),
    ('Read: last', 'AtVector3.cpp', '    z = tokenizer->ExpectReal();\n', ''),
    ('Shortest: value', 'AtVector3.cpp', 'kShortest = 5.0e-6f;', 'kShortest = 5.0000005e-6f;'),
    ('Normalize: comparison', 'AtVector3.cpp', 'void AtVector3::Normalize() {\n    const double a = x, b = y, c = z;\n    const double length = sqrt(a * a + b * b + c * c);\n    if (length <= kShortest) {', 'void AtVector3::Normalize() {\n    const double a = x, b = y, c = z;\n    const double length = sqrt(a * a + b * b + c * c);\n    if (length < kShortest) {'),
    ('Normalize: nan', 'AtVector3.cpp', 'void AtVector3::Normalize() {\n    const double a = x, b = y, c = z;\n    const double length = sqrt(a * a + b * b + c * c);\n    if (length <= kShortest) {', 'void AtVector3::Normalize() {\n    const double a = x, b = y, c = z;\n    const double length = sqrt(a * a + b * b + c * c);\n    if (!(length > kShortest)) {'),
    ('Normalize: order', 'AtVector3.cpp', 'void AtVector3::Normalize() {\n    const double a = x, b = y, c = z;\n    const double length = sqrt(a * a + b * b + c * c);', 'void AtVector3::Normalize() {\n    const double a = x, b = y, c = z;\n    const double length = sqrt(c * c + b * b + a * a);'),
    # Not distinguished by test, and not an equivalent: Normalize: direct. See docs/verification.md.
    ('Normalize: x when short', 'AtVector3.cpp', 'void AtVector3::Normalize() {\n    const double a = x, b = y, c = z;\n    const double length = sqrt(a * a + b * b + c * c);\n    if (length <= kShortest) {\n        x = 1.0f;\n', 'void AtVector3::Normalize() {\n    const double a = x, b = y, c = z;\n    const double length = sqrt(a * a + b * b + c * c);\n    if (length <= kShortest) {\n        x = 0.0f;\n'),
    ('SetNormalized: short keeps', 'AtVector3.cpp', 'void AtVector3::SetNormalized(const AtVector3& from) {\n    const double a = from.x, b = from.y, c = from.z;\n    const double length = sqrt(a * a + b * b + c * c);\n    if (length <= kShortest) {\n        x = 1.0f;\n        y = 0.0f;\n        z = 0.0f;\n        return;\n    }\n', 'void AtVector3::SetNormalized(const AtVector3& from) {\n    const double a = from.x, b = from.y, c = from.z;\n    const double length = sqrt(a * a + b * b + c * c);\n    if (length <= kShortest) {\n        x = 1.0f;\n        y = 0.0f;\n        return;\n    }\n'),
    ('SetNormalized: this', 'AtVector3.cpp', '    x = (float)(scale * from.x);\n    y = (float)(scale * from.y);\n    z = (float)(scale * from.z);\n}\n\nfloat AtVector3::NormalizeReturningLength', '    x = (float)(scale * x);\n    y = (float)(scale * from.y);\n    z = (float)(scale * from.z);\n}\n\nfloat AtVector3::NormalizeReturningLength'),
    ('Length: short returns', 'AtVector3.cpp', '        return 0.0f;\n', '        return (float)length;\n'),
    ('Length: returns', 'AtVector3.cpp', '    return (float)length;\n', '    return 1.0f;\n'),
    ('Length: z', 'AtVector3.cpp', '    z = (float)(scale * z);\n    return (float)length;\n', '    return (float)length;\n'),
    ('Rotate: zero', 'AtVector3.cpp', 'void AtVector3::Rotate(const AtVector3& axis, float angle) {\n    if (angle == 0.0f) {\n        return;\n    }\n', 'void AtVector3::Rotate(const AtVector3& axis, float angle) {\n'),
    ('Rotate: minus cos', 'AtVector3.cpp', 'void AtVector3::Rotate(const AtVector3& axis, float angle) {\n    if (angle == 0.0f) {\n        return;\n    }\n    const double s = AtSin(angle);\n    const double c = AtCos(angle);\n    const float oldX = x;\n    const float oldY = y;\n    const double d = AtOneMinusCos(angle);\n', 'void AtVector3::Rotate(const AtVector3& axis, float angle) {\n    if (angle == 0.0f) {\n        return;\n    }\n    const double s = AtSin(angle);\n    const double c = AtCos(angle);\n    const float oldX = x;\n    const float oldY = y;\n    const double d = 1.0 - c;\n'),
    ('Rotate: oldX', 'AtVector3.cpp', '((double)xy - sz) * oldX + ((double)yy + c) * y);', '((double)xy - sz) * x + ((double)yy + c) * y);'),
    ('Rotate: oldY', 'AtVector3.cpp', '((double)yz - sx) * oldY + ((double)sy + xz) * oldX);', '((double)yz - sx) * y + ((double)sy + xz) * oldX);'),
    ('Rotate: oldX in z', 'AtVector3.cpp', '((double)yz - sx) * oldY + ((double)sy + xz) * oldX);', '((double)yz - sx) * oldY + ((double)sy + xz) * x);'),
    ('Rotate: x term sign', 'AtVector3.cpp', 'x = (float)((dax * axis.x + c) * x + ((double)xz - sy) * z + ((double)sz + xy) * y);', 'x = (float)((dax * axis.x + c) * x + ((double)xz + sy) * z + ((double)sz + xy) * y);'),
    ('Rotate: x order', 'AtVector3.cpp', 'x = (float)((dax * axis.x + c) * x + ((double)xz - sy) * z + ((double)sz + xy) * y);', 'x = (float)(((double)sz + xy) * y + ((double)xz - sy) * z + (dax * axis.x + c) * x);'),
    # Not distinguished by test, and not an equivalent: Rotate: y order. See docs/verification.md.
    # Not distinguished by test, and not an equivalent: Rotate: z order. See docs/verification.md.
    ('Rotated: zero', 'AtVector3.cpp', '    if (angle == 0.0f) {\n        x = from.x;\n        y = from.y;\n        z = from.z;\n        return;\n    }\n', ''),
    ('Rotated: zero z', 'AtVector3.cpp', '        y = from.y;\n        z = from.z;\n        return;\n    }\n    const double s = AtSin(angle);', '        y = from.y;\n        return;\n    }\n    const double s = AtSin(angle);'),
    ('Rotated: x order', 'AtVector3.cpp', 'x = (float)((dax * axis.x + c) * from.x + ((double)xz - sy) * from.z + ((double)sz + xy) * from.y);', 'x = (float)(((double)sz + xy) * from.y + ((double)xz - sy) * from.z + (dax * axis.x + c) * from.x);'),
    ('Rotated: y order', 'AtVector3.cpp', 'y = (float)(((double)xy - sz) * from.x + ((double)sx + yz) * from.z + ((double)yy + c) * from.y);', 'y = (float)(((double)yy + c) * from.y + ((double)sx + yz) * from.z + ((double)xy - sz) * from.x);'),
    # Not distinguished by test, and not an equivalent: Rotated: z order. See docs/verification.md.
    ('RotateZ: sign', 'AtVector3.cpp', '    x = (float)(c * a - s * b);\n', '    x = (float)(c * a + s * b);\n'),
    ('RotateZ: stale', 'AtVector3.cpp', '    y = (float)(c * b + s * a);\n', '    y = (float)(c * b + s * x);\n'),
    ('Project: zero x', 'AtVector3.cpp', '    if (lengthSquared == 0.0) {\n        y = 0.0f;\n        z = 0.0f;\n        return;\n    }\n', '    if (lengthSquared == 0.0) {\n        x = 0.0f;\n        y = 0.0f;\n        z = 0.0f;\n        return;\n    }\n'),
    ('Project: zero', 'AtVector3.cpp', '    if (lengthSquared == 0.0) {\n        y = 0.0f;\n        z = 0.0f;\n        return;\n    }\n', ''),
    # Not distinguished by test, and not an equivalent: Project: order. See docs/verification.md.
    ('SetProjected: zero', 'AtVector3.cpp', '    if (lengthSquared == 0.0) {\n        x = from.x;\n        y = 0.0f;\n', '    if (lengthSquared == 0.0) {\n        x = 0.0f;\n        y = 0.0f;\n'),
    # Not distinguished by test, and not an equivalent: SetProjected: order. See docs/verification.md.
    ('Reject: zero', 'AtVector3.cpp', '    if (lengthSquared == 0.0) {\n        x = 0.0f;\n        return;\n    }\n', '    if (lengthSquared == 0.0) {\n        return;\n    }\n'),
    ('Reject: order', 'AtVector3.cpp', '((double)direction.y * y + (double)z * direction.z + (double)direction.x * x)', '((double)direction.x * x + (double)z * direction.z + (double)direction.y * y)'),
    ('Reject: float x', 'AtVector3.cpp', '    const double alongX = scale * direction.x;\n    const float alongY = (float)(scale * direction.y);\n    const float alongZ = (float)(scale * direction.z);\n    x = (float)(x - alongX);', '    const float alongX = (float)(scale * direction.x);\n    const float alongY = (float)(scale * direction.y);\n    const float alongZ = (float)(scale * direction.z);\n    x = (float)(x - (double)alongX);'),
    ('Reject: double y', 'AtVector3.cpp', '    const float alongY = (float)(scale * direction.y);\n    const float alongZ = (float)(scale * direction.z);\n    x = (float)(x - alongX);', '    const double alongY = scale * direction.y;\n    const float alongZ = (float)(scale * direction.z);\n    x = (float)(x - alongX);'),
    ('SetRejected: zero', 'AtVector3.cpp', '        x = 0.0f;\n        y = from.y;\n', '        x = from.x;\n        y = from.y;\n'),
    ('SetRejected: zero z', 'AtVector3.cpp', '        y = from.y;\n        z = from.z;\n        return;\n    }\n    const double scale', '        y = from.y;\n        return;\n    }\n    const double scale'),
    ('SetRejected: order', 'AtVector3.cpp', '((double)from.x * direction.x + (double)from.z * direction.z + (double)from.y * direction.y)', '((double)from.y * direction.y + (double)from.z * direction.z + (double)from.x * direction.x)'),
    ('SetRejected: double z', 'AtVector3.cpp', '    const float alongZ = (float)(scale * direction.z);\n    x = (float)(from.x - alongX);', '    const double alongZ = scale * direction.z;\n    x = (float)(from.x - alongX);'),
]


MEM_MUTANTS = [
    ('Raise: 0', 'AtMemory.cpp', 'case 0: message = "Invalid error condition"; break;\n', ''),
    ('Raise: -1', 'AtMemory.cpp', 'case -1: message = "General error occurred"; break;\n', ''),
    ('Raise: -2', 'AtMemory.cpp', 'case -2: message = "Invalid parameter detected"; break;\n', ''),
    ('Raise: -3', 'AtMemory.cpp', 'case -3: message = "Invalid value detected"; break;\n', ''),
    ('Raise: -4', 'AtMemory.cpp', 'case -4: message = "Out of memory error occured"; break;\n', ''),
    ('Raise: -5', 'AtMemory.cpp', 'case -5: message = "Critical resource busy"; break;\n', ''),
    ('Raise: -6 named', 'AtMemory.cpp', 'case -7: message = "Assertion Failed"; break;\n', 'case -7: case -6: message = "Assertion Failed"; break;\n'),
    ('Raise: -7', 'AtMemory.cpp', 'case -7: message = "Assertion Failed"; break;\n', ''),
    ('Raise: args', 'AtMemory.cpp', 'AtFatal(message, file, line, 0);', 'AtFatal(message, line, file, 0);'),
    ('Raise: fourth passed', 'AtMemory.cpp', 'AtFatal(message, file, line, 0);', 'AtFatal(message, file, line, unused);'),
    ('New: top', 'AtMemory.cpp', 'Allocate(AtHeapStack::s_heaps[AtHeapStack::s_top], size)', 'Allocate(AtHeapStack::s_heaps[0], size)'),
    ('Delete: null', 'AtMemory.cpp', 'if (data != 0 && AtHeapManager::s_instance->Free(data) == 0) {', 'if (AtHeapManager::s_instance->Free(data) == 0) {'),
    ('Delete: owned too', 'AtMemory.cpp', 'if (data != 0 && AtHeapManager::s_instance->Free(data) == 0) {', 'if (data != 0) {\n        AtHeapManager::s_instance->Free(data);'),
    ('Delete: no library', 'AtMemory.cpp', '        AtLibraryFree(data);\n', ''),
    ('Push: bound', 'AtMemory.cpp', 'if (++AtHeapStack::s_top < 16) {', 'if (++AtHeapStack::s_top < 17) {'),
    ('Push: bound low', 'AtMemory.cpp', 'if (++AtHeapStack::s_top < 16) {', 'if (++AtHeapStack::s_top < 15) {'),
    ('Pop: no clear', 'AtMemory.cpp', '    AtHeapStack::s_heaps[AtHeapStack::s_top] = 0;\n', ''),
    ('Pop: no decrement', 'AtMemory.cpp', '    AtHeapStack::s_top--;\n', ''),
    ('Current: bound', 'AtMemory.cpp', 'if (AtHeapStack::s_top >= 0) {', 'if (AtHeapStack::s_top > 0) {'),
    ('Content: not set', 'AtMemory.cpp', '    AtHeapStack::s_contentHeap = heap;\n', ''),
    ('Top: starts', 'AtMemory.cpp', 'int AtHeapStack::s_top = -1;', 'int AtHeapStack::s_top = 0;'),
    ('Ctor: started', 'AtMemorySystem.cpp', 'AtMemorySystem::AtMemorySystem() {\n    m_started = 0;\n', 'AtMemorySystem::AtMemorySystem() {\n'),
    ('Dtor: pop always', 'AtMemorySystem.cpp', 'AtMemorySystem::~AtMemorySystem() {\n    if (AtIoTokenFile::s_heap != 0 && AtCurrentHeap() != 0) {\n', 'AtMemorySystem::~AtMemorySystem() {\n    {\n'),
    ('Dtor: no reset', 'AtMemorySystem.cpp', '        AtPopHeap();\n    }\n    m_manager.ResetAll();\n    m_started = 0;\n}\n\nvoid AtMemorySystem::Startup', '        AtPopHeap();\n    }\n    m_started = 0;\n}\n\nvoid AtMemorySystem::Startup'),
    ('Dtor: started', 'AtMemorySystem.cpp', '    m_manager.ResetAll();\n    m_started = 0;\n}\n\nvoid AtMemorySystem::Startup', '    m_manager.ResetAll();\n}\n\nvoid AtMemorySystem::Startup'),
    ('Startup: no shutdown', 'AtMemorySystem.cpp', '    if (m_started != 0) {\n        Shutdown();\n    }\n', ''),
    ('Startup: always shutdown', 'AtMemorySystem.cpp', '    if (m_started != 0) {\n        Shutdown();\n    }\n', '    Shutdown();\n'),
    ('Startup: no init', 'AtMemorySystem.cpp', '    Initialize(params);\n', ''),
    ('Startup: instance', 'AtMemorySystem.cpp', '    AtHeapManager::s_instance = &m_manager;\n', ''),
    ('Startup: default', 'AtMemorySystem.cpp', '    AtIoTokenFile::s_heap = m_defaultHeap;\n    if (m_defaultHeap != 0) {\n', '    if (m_defaultHeap != 0) {\n'),
    ('Startup: push always', 'AtMemorySystem.cpp', '    if (m_defaultHeap != 0) {\n        AtPushHeap(m_defaultHeap);\n    }\n', '    AtPushHeap(m_defaultHeap);\n'),
    ('Startup: no push', 'AtMemorySystem.cpp', '        AtPushHeap(m_defaultHeap);\n', ''),
    ('Startup: started', 'AtMemorySystem.cpp', '    m_started = 1;\n', ''),
    ('Shutdown: heap condition', 'AtMemorySystem.cpp', 'void AtMemorySystem::Shutdown() {\n    if (AtIoTokenFile::s_heap != 0 && AtCurrentHeap() != 0) {\n', 'void AtMemorySystem::Shutdown() {\n    if (AtCurrentHeap() != 0) {\n'),
    ('Shutdown: top condition', 'AtMemorySystem.cpp', 'void AtMemorySystem::Shutdown() {\n    if (AtIoTokenFile::s_heap != 0 && AtCurrentHeap() != 0) {\n', 'void AtMemorySystem::Shutdown() {\n    if (AtIoTokenFile::s_heap != 0) {\n'),
    ('Shutdown: no reset', 'AtMemorySystem.cpp', '    m_manager.ResetAll();\n    m_started = 0;\n}\n\nAtRegionMemorySystem::AtRegionMemorySystem', '    m_started = 0;\n}\n\nAtRegionMemorySystem::AtRegionMemorySystem'),
    ('Shutdown: started', 'AtMemorySystem.cpp', '    m_manager.ResetAll();\n    m_started = 0;\n}\n\nAtRegionMemorySystem::AtRegionMemorySystem', '    m_manager.ResetAll();\n}\n\nAtRegionMemorySystem::AtRegionMemorySystem'),
    ('Region ctor: region', 'AtMemorySystem.cpp', '    m_region = 0;\n    s_unknown3ADE44', '    s_unknown3ADE44'),
    ('Region ctor: short', 'AtMemorySystem.cpp', '    s_unknown3ADE44 = 0;\n', ''),
    ('Region ctor: word', 'AtMemorySystem.cpp', '    s_unknown3ADE40 = 0;\n', ''),
    ('Init: no raise', 'AtMemorySystem.cpp', '    if (m_region == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n', ''),
    ('Init: other code', 'AtMemorySystem.cpp', 'AtRaise(-4, 0, 0, 0);', 'AtRaise(-2, 0, 0, 0);'),
    ('Init: manager size', 'AtMemorySystem.cpp', 'm_manager.Init(m_region, params->m_regionSize, params->m_unused);', 'm_manager.Init(m_region, params->m_regionSize / 2, params->m_unused);'),
    ('Init: default size', 'AtMemorySystem.cpp', '        size = 0x10000;\n', '        size = 0x8000;\n'),
    ('Init: no default size', 'AtMemorySystem.cpp', '    if (size == 0) {\n        size = 0x10000;\n    }\n', ''),
    ('Init: heap shift', 'AtMemorySystem.cpp', 'm_manager.CreateHeap(size, 2);', 'm_manager.CreateHeap(size, 3);'),
    ('Init: heap kept', 'AtMemorySystem.cpp', 'm_defaultHeap = m_manager.CreateHeap(size, 2);', 'm_manager.CreateHeap(size, 2);'),
    ('Init: no devices', 'AtMemorySystem.cpp', '    AtPlatformInitDevices();\n', ''),
    ('Region shutdown: base', 'AtMemorySystem.cpp', 'void AtRegionMemorySystem::Shutdown() {\n    AtMemorySystem::Shutdown();\n', 'void AtRegionMemorySystem::Shutdown() {\n'),
    ('Region shutdown: free', 'AtMemorySystem.cpp', 'void AtRegionMemorySystem::Shutdown() {\n    AtMemorySystem::Shutdown();\n    if (m_region != 0) {\n        AtPlatformFreeRegion(m_region);\n    }\n', 'void AtRegionMemorySystem::Shutdown() {\n    AtMemorySystem::Shutdown();\n'),
    ('Region shutdown: forgets', 'AtMemorySystem.cpp', 'void AtRegionMemorySystem::Shutdown() {\n    AtMemorySystem::Shutdown();\n    if (m_region != 0) {\n        AtPlatformFreeRegion(m_region);\n', 'void AtRegionMemorySystem::Shutdown() {\n    AtMemorySystem::Shutdown();\n    if (m_region != 0) {\n        AtPlatformFreeRegion(m_region);\n        m_region = 0;\n'),
    ('Region dtor: base', 'AtMemorySystem.cpp', 'AtRegionMemorySystem::~AtRegionMemorySystem() {\n    AtMemorySystem::Shutdown();\n', 'AtRegionMemorySystem::~AtRegionMemorySystem() {\n'),
    ('Region dtor: free', 'AtMemorySystem.cpp', 'AtRegionMemorySystem::~AtRegionMemorySystem() {\n    AtMemorySystem::Shutdown();\n    if (m_region != 0) {\n        AtPlatformFreeRegion(m_region);\n    }\n', 'AtRegionMemorySystem::~AtRegionMemorySystem() {\n    AtMemorySystem::Shutdown();\n'),
    ('Save: manager', 'AtMemorySystem.cpp', '    m_manager = AtHeapManager::s_instance;\n', ''),
    ('Save: heap', 'AtMemorySystem.cpp', '    m_defaultHeap = (AtHeap*)AtIoTokenFile::s_heap;\n', ''),
    ('Save: fatal', 'AtMemorySystem.cpp', '    m_fatal = s_fatal;\n', ''),
    ('Restore: manager', 'AtMemorySystem.cpp', '    AtHeapManager::s_instance = m_manager;\n', ''),
    ('Restore: heap', 'AtMemorySystem.cpp', '    AtIoTokenFile::s_heap = m_defaultHeap;\n    if (m_defaultHeap != 0 && AtCurrentHeap() == 0) {\n', '    if (m_defaultHeap != 0 && AtCurrentHeap() == 0) {\n'),
    ('Restore: push with null', 'AtMemorySystem.cpp', 'if (m_defaultHeap != 0 && AtCurrentHeap() == 0) {', 'if (AtCurrentHeap() == 0) {'),
    ('Restore: push when full', 'AtMemorySystem.cpp', 'if (m_defaultHeap != 0 && AtCurrentHeap() == 0) {', 'if (m_defaultHeap != 0) {'),
    ('Restore: fatal', 'AtMemorySystem.cpp', '    s_fatal = m_fatal;\n', ''),
]


HEP_MUTANTS = [
    ('Overhead: round', 'AtHeap.cpp', '((unsigned int)offsetof(AtHeapBlock, m_nextFree) + mask) & ~mask', '(unsigned int)offsetof(AtHeapBlock, m_nextFree)'),
    ('Size: flags lost', 'AtHeap.cpp', 'block->m_word = (block->m_word & ~(unsigned int)AtHeapBlock::kSizeMask) | (size & AtHeapBlock::kSizeMask);', 'block->m_word = size & AtHeapBlock::kSizeMask;'),
    ('Size: uncut', 'AtHeap.cpp', '| (size & AtHeapBlock::kSizeMask);', '| size;'),
    ('SizeOf: uncut', 'AtHeap.cpp', 'return block->m_word & AtHeapBlock::kSizeMask;', 'return block->m_word & 0x1FFFFFF;'),
    ('Tag: flags lost', 'AtHeap.cpp', '(block->m_word & 0x3FFFFFF)', '(block->m_word & 0xFFFFFF)'),
    ('Tag: shift', 'AtHeap.cpp', '((unsigned int)tag << AtHeapBlock::kTagShift);', '((unsigned int)tag << 25);'),
    ('Ctor: shift', 'AtHeap.cpp', 'AtHeap::AtHeap() {\n    m_base = 0;\n    m_size = 0;\n    m_used = 0;\n    m_alignShift = 2;', 'AtHeap::AtHeap() {\n    m_base = 0;\n    m_size = 0;\n    m_used = 0;\n    m_alignShift = 3;'),
    ('Ctor: alloc shift', 'AtHeap.cpp', 'AtHeap::AtHeap() {\n    m_base = 0;\n    m_size = 0;\n    m_used = 0;\n    m_alignShift = 2;\n    m_allocShift = 2;', 'AtHeap::AtHeap() {\n    m_base = 0;\n    m_size = 0;\n    m_used = 0;\n    m_alignShift = 2;\n    m_allocShift = 3;'),
    ('Ctor: fixed', 'AtHeap.cpp', 'AtHeap::AtHeap() {\n    m_base = 0;\n    m_size = 0;\n    m_used = 0;\n    m_alignShift = 2;\n    m_allocShift = 2;\n    m_tag = 0;\n    m_free = 0;\n    m_equivalencyId = 0;\n    m_fixed = 0;', 'AtHeap::AtHeap() {\n    m_base = 0;\n    m_size = 0;\n    m_used = 0;\n    m_alignShift = 2;\n    m_allocShift = 2;\n    m_tag = 0;\n    m_free = 0;\n    m_equivalencyId = 0;'),
    ('Dtor: size', 'AtHeap.cpp', 'AtHeap::~AtHeap() {\n    m_base = 0;\n    m_size = 0;\n', 'AtHeap::~AtHeap() {\n    m_base = 0;\n'),
    ('Dtor: id', 'AtHeap.cpp', 'AtHeap::~AtHeap() {\n    m_base = 0;\n    m_size = 0;\n    m_used = 0;\n    m_alignShift = 2;\n    m_allocShift = 2;\n    m_tag = 0;\n    m_free = 0;\n    m_equivalencyId = 0;\n', 'AtHeap::~AtHeap() {\n    m_base = 0;\n    m_size = 0;\n    m_used = 0;\n    m_alignShift = 2;\n    m_allocShift = 2;\n    m_tag = 0;\n    m_free = 0;\n'),
    ('Init: tag', 'AtHeap.cpp', '    m_tag = tag;\n    m_equivalencyId = tag;\n', '    m_equivalencyId = tag;\n'),
    ('Init: id', 'AtHeap.cpp', '    m_tag = tag;\n    m_equivalencyId = tag;\n', '    m_tag = tag;\n'),
    ('Init: no reset', 'AtHeap.cpp', '    m_equivalencyId = tag;\n    Reset();\n', '    m_equivalencyId = tag;\n'),
    ('Init: alloc shift', 'AtHeap.cpp', '    m_allocShift = (unsigned int)alignShift;\n', ''),
    ('Reset: used', 'AtHeap.cpp', '    m_free = BlockAt(m_base);\n    m_used = 0;\n', '    m_free = BlockAt(m_base);\n'),
    ('Reset: flags', 'AtHeap.cpp', '    m_free->m_word &= ~(unsigned int)(AtHeapBlock::kBackOffset | AtHeapBlock::kUsed);\n', ''),
    ('Reset: back flag', 'AtHeap.cpp', '~(unsigned int)(AtHeapBlock::kBackOffset | AtHeapBlock::kUsed);', '~(unsigned int)AtHeapBlock::kUsed;'),
    ('Reset: tag', 'AtHeap.cpp', '    SetTag(m_free, m_tag);\n', ''),
    ('Reset: prev', 'AtHeap.cpp', '    m_free->m_prev = 0;\n', ''),
    ('Reset: next', 'AtHeap.cpp', '    m_free->m_nextFree = 0;\n', ''),
    ('Reset: prev free', 'AtHeap.cpp', '    m_free->m_prevFree = 0;\n', ''),
    ('Alloc: no rounding', 'AtHeap.cpp', 'const unsigned int rounded = (size + mask) & ~mask;', 'const unsigned int rounded = size;'),
    ('Alloc: wide gap', 'AtHeap.cpp', '((wide - unit) + wideMask + overhead + (unsigned int)offsetof(AtHeapBlock, m_nextFree)) & ~wideMask', '(wideMask + overhead + (unsigned int)offsetof(AtHeapBlock, m_nextFree)) & ~wideMask'),
    ('Alloc: wide extra', 'AtHeap.cpp', '+ overhead + (unsigned int)offsetof(AtHeapBlock, m_nextFree)) & ~wideMask', '+ overhead) & ~wideMask'),
    ('Alloc: minimum', 'AtHeap.cpp', '    if (need < sizeof(AtHeapBlock)) {\n        need = sizeof(AtHeapBlock);\n    }\n', ''),
    ('Alloc: fit edge', 'AtHeap.cpp', 'SizeOf(block) < need', 'SizeOf(block) <= need'),
    ('Alloc: best not first', 'AtHeap.cpp', 'while (block != 0 && SizeOf(block) < need) {\n        block = block->m_nextFree;\n    }', 'for (AtHeapBlock* b = m_free; b != 0; b = b->m_nextFree) {\n        if (SizeOf(b) >= need && (block == m_free && SizeOf(block) < need || SizeOf(b) < SizeOf(block))) block = b;\n    }\n    if (block != 0 && SizeOf(block) < need) block = 0;'),
    ('Alloc: split edge', 'AtHeap.cpp', 'if (need + sizeof(AtHeapBlock) < have) {', 'if (need + sizeof(AtHeapBlock) <= have) {'),
    ('Alloc: split size', 'AtHeap.cpp', '        SetSize(block, need);\n', ''),
    ('Alloc: rest size', 'AtHeap.cpp', 'rest->m_word = (have - need) & AtHeapBlock::kSizeMask;', 'rest->m_word = have & AtHeapBlock::kSizeMask;'),
    ('Alloc: rest next', 'AtHeap.cpp', '        rest->m_nextFree = block->m_nextFree;\n', '        rest->m_nextFree = 0;\n'),
    ('Alloc: rest prev', 'AtHeap.cpp', '        rest->m_prevFree = block->m_prevFree;\n', '        rest->m_prevFree = 0;\n'),
    ('Alloc: rest back link', 'AtHeap.cpp', '        if (rest->m_nextFree != 0) {\n            rest->m_nextFree->m_prevFree = rest;\n        }\n', ''),
    ('Alloc: rest as head', 'AtHeap.cpp', '        if (m_free == block) {\n            m_free = rest;\n        } else if', '        if (0) {\n            m_free = rest;\n        } else if'),
    ('Alloc: rest forward link', 'AtHeap.cpp', '        } else if (rest->m_prevFree != 0) {\n            rest->m_prevFree->m_nextFree = rest;\n        }\n', '        }\n'),
    ('Alloc: rest prev block', 'AtHeap.cpp', '        rest->m_prev = block;\n', ''),
    ('Alloc: after edge', 'AtHeap.cpp', '        if ((char*)after < m_base + m_size) {\n            after->m_prev = rest;\n        }\n', '        after->m_prev = rest;\n'),
    ('Alloc: after not linked', 'AtHeap.cpp', '        if ((char*)after < m_base + m_size) {\n            after->m_prev = rest;\n        }\n', ''),
    ('Alloc: rest tag', 'AtHeap.cpp', '        SetTag(rest, m_tag);\n', ''),
    ('Alloc: taken split', 'AtHeap.cpp', '        taken = need;\n', '        taken = need + 1;\n'),
    ('Alloc: unlink head', 'AtHeap.cpp', '        if (m_free == block) {\n            m_free = block->m_nextFree;\n        } else {\n            block->m_prevFree->m_nextFree = block->m_nextFree;\n        }\n        if (block->m_nextFree != 0) {\n            block->m_nextFree->m_prevFree = block->m_prevFree;\n        }\n        taken = have;', '        block->m_prevFree->m_nextFree = block->m_nextFree;\n        if (block->m_nextFree != 0) {\n            block->m_nextFree->m_prevFree = block->m_prevFree;\n        }\n        taken = have;'),
    ('Alloc: unlink back', 'AtHeap.cpp', '            block->m_prevFree->m_nextFree = block->m_nextFree;\n        }\n        if (block->m_nextFree != 0) {\n            block->m_nextFree->m_prevFree = block->m_prevFree;\n        }\n        taken = have;', '            block->m_prevFree->m_nextFree = block->m_nextFree;\n        }\n        taken = have;'),
    ('Alloc: taken whole', 'AtHeap.cpp', '        taken = have;\n', '        taken = need;\n'),
    ('Alloc: used', 'AtHeap.cpp', '    m_used += taken;\n', ''),
    ('Alloc: used flag', 'AtHeap.cpp', '    block->m_word |= AtHeapBlock::kUsed;\n', ''),
    ('Alloc: tag', 'AtHeap.cpp', '    block->m_word |= AtHeapBlock::kUsed;\n    SetTag(block, m_tag);\n', '    block->m_word |= AtHeapBlock::kUsed;\n'),
    ('Alloc: wide ignored', 'AtHeap.cpp', 'if (m_allocShift == m_alignShift) {\n        return (char*)block + overhead;', 'if (1) {\n        return (char*)block + overhead;'),
    ('Alloc: marker top byte', 'AtHeap.cpp', '(marker->m_word & 0xFF000000) | ', ''),
    ('Alloc: marker flag', 'AtHeap.cpp', ' | AtHeapBlock::kBackOffset;\n    return (char*)marker', ';\n    return (char*)marker'),
    ('Alloc: marker distance', 'AtHeap.cpp', '((unsigned int)((char*)marker - (char*)block) & AtHeapBlock::kSizeMask)', '((unsigned int)((char*)marker - (char*)block + 4) & AtHeapBlock::kSizeMask)'),
    ('Free: back ignored', 'AtHeap.cpp', '    if (block->m_word & AtHeapBlock::kBackOffset) {\n        block = BlockAt((char*)data - SizeOf(block) - overhead);\n    }\n', ''),
    ('Free: used flag', 'AtHeap.cpp', '    block->m_word &= ~(unsigned int)AtHeapBlock::kUsed;\n', ''),
    ('Free: used count', 'AtHeap.cpp', '    m_used -= size;\n', ''),
    ('Free: end ignored', 'AtHeap.cpp', 'if ((char*)next < end && !(next->m_word & AtHeapBlock::kUsed)) {', 'if (!(next->m_word & AtHeapBlock::kUsed)) {'),
    ('Free: no merge after', 'AtHeap.cpp', 'if ((char*)next < end && !(next->m_word & AtHeapBlock::kUsed)) {', 'if (0) {'),
    ('Free: merged size', 'AtHeap.cpp', '        size += SizeOf(next);\n', ''),
    ('Free: merged next', 'AtHeap.cpp', '        block->m_nextFree = next->m_nextFree;\n', ''),
    ('Free: merged prev', 'AtHeap.cpp', '        block->m_prevFree = next->m_prevFree;\n', ''),
    ('Free: head moves', 'AtHeap.cpp', '        if (m_free == next) {\n            m_free = block;\n        } else if', '        if (0) {\n            m_free = block;\n        } else if'),
    ('Free: forward link', 'AtHeap.cpp', '        } else if (next->m_prevFree != 0) {\n            next->m_prevFree->m_nextFree = block;\n        }\n', '        }\n'),
    ('Free: back link', 'AtHeap.cpp', '        if (next->m_nextFree != 0) {\n            next->m_nextFree->m_prevFree = block;\n        }\n', ''),
    ('Free: after link', 'AtHeap.cpp', '        if ((char*)after < end) {\n            after->m_prev = block;\n        }\n', ''),
    ('Free: flag merged', 'AtHeap.cpp', '        merged = 1;\n', ''),
    ('Free: no merge before', 'AtHeap.cpp', 'if (before != 0 && !(before->m_word & AtHeapBlock::kUsed)) {', 'if (0) {'),
    ('Free: before size', 'AtHeap.cpp', '        size += SizeOf(before);\n', ''),
    ('Free: leave list', 'AtHeap.cpp', '            if (m_free == block) {\n                m_free = block->m_nextFree;\n            } else {\n                block->m_prevFree->m_nextFree = block->m_nextFree;\n            }\n', ''),
    ('Free: leave back link', 'AtHeap.cpp', '            if (block->m_nextFree != 0) {\n                block->m_nextFree->m_prevFree = block->m_prevFree;\n            }\n', ''),
    ('Free: after before', 'AtHeap.cpp', '        if ((char*)after < end) {\n            after->m_prev = before;\n        }\n', ''),
    ('Free: list push next', 'AtHeap.cpp', '        block->m_nextFree = m_free;\n', ''),
    ('Free: list push prev', 'AtHeap.cpp', '        block->m_prevFree = 0;\n', ''),
    ('Free: list push back', 'AtHeap.cpp', '        if (m_free != 0) {\n            m_free->m_prevFree = block;\n        }\n', ''),
    ('Free: list push head', 'AtHeap.cpp', '        m_free = block;\n    }\n}\n', '    }\n}\n'),
    ('Owns: low', 'AtHeap.cpp', 'if (p < m_base || p >= m_base + m_size) {', 'if (p >= m_base + m_size) {'),
    ('Owns: high', 'AtHeap.cpp', 'if (p < m_base || p >= m_base + m_size) {', 'if (p < m_base || p > m_base + m_size) {'),
    ('Owns: odd', 'AtHeap.cpp', 'if ((back & 3) != 0 || back > 0x800) {', 'if (back > 0x800) {'),
    ('Owns: far', 'AtHeap.cpp', 'if ((back & 3) != 0 || back > 0x800) {', 'if ((back & 3) != 0 || back >= 0x800) {'),
    ('Owns: no limit', 'AtHeap.cpp', 'if ((back & 3) != 0 || back > 0x800) {', 'if ((back & 3) != 0) {'),
    ('Owns: back ignored', 'AtHeap.cpp', '        block = (const AtHeapBlock*)(p - back - overhead);\n', ''),
    ('Owns: tag', 'AtHeap.cpp', '== (unsigned int)m_tag ? 1 : 0', '!= 99 ? 1 : 0'),
    ('Largest: first', 'AtHeap.cpp', 'if (SizeOf(b) > largest) {', 'if (largest == 0) {'),
    ('Count: off', 'AtHeap.cpp', 'unsigned int count = 0;', 'unsigned int count = 1;'),
    ('Aligned: not restored', 'AtHeap.cpp', '    m_allocShift = saved;\n', ''),
    ('Aligned: narrower', 'AtHeap.cpp', 'alignShift > m_alignShift ? alignShift : m_alignShift', 'alignShift'),
    ('Manager ctor: last id', 'AtHeap.cpp', 'AtHeapManager::AtHeapManager() {\n    m_base = 0;\n    m_chunks = 0;\n    m_size = 0;\n    m_lastId = 0;\n', 'AtHeapManager::AtHeapManager() {\n    m_base = 0;\n    m_chunks = 0;\n    m_size = 0;\n'),
    ('Manager ctor: override', 'AtHeap.cpp', 'AtHeapManager::AtHeapManager() {\n    m_base = 0;\n    m_chunks = 0;\n    m_size = 0;\n    m_lastId = 0;\n    m_override = 0;\n', 'AtHeapManager::AtHeapManager() {\n    m_base = 0;\n    m_chunks = 0;\n    m_size = 0;\n    m_lastId = 0;\n'),
    ('Init: second call keeps', 'AtHeap.cpp', '    if (m_size != 0) {\n        ResetAll();\n    }\n', ''),
    ('Init: no aligning', 'AtHeap.cpp', '(char*)(((size_t)base + 0x3F) & ~(size_t)0x3F);', '(char*)base;'),
    ('Init: size adjusted', 'AtHeap.cpp', '    m_size = (unsigned int)((char*)base - aligned) + size;\n', '    m_size = size;\n'),
    ('Init: chunk size adjusted', 'AtHeap.cpp', '    m_chunks->m_size = (int)size;\n', '    m_chunks->m_size = (int)m_size;\n'),
    ('Init: chunk next', 'AtHeap.cpp', '    m_chunks->m_next = 0;\n', ''),
    ('Init: chunk prev', 'AtHeap.cpp', '    m_chunks->m_next = 0;\n    m_chunks->m_prev = 0;\n', '    m_chunks->m_next = 0;\n'),
    ('ResetAll: heaps kept', 'AtHeap.cpp', '            m_heaps[i].~AtHeap();\n', ''),
    ('ResetAll: last id', 'AtHeap.cpp', '    m_size = 0;\n    m_lastId = 0;\n    m_override = 0;\n}\n\nint AtHeapManager::NextId', '    m_size = 0;\n    m_override = 0;\n}\n\nint AtHeapManager::NextId'),
    ('ResetAll: override', 'AtHeap.cpp', '    m_lastId = 0;\n    m_override = 0;\n}\n\nint AtHeapManager::NextId', '    m_lastId = 0;\n}\n\nint AtHeapManager::NextId'),
    ('NextId: uses in use', 'AtHeap.cpp', '            if (m_heaps[i].m_equivalencyId == id) {\n                used = 1;\n                break;\n            }\n', ''),
    ('Create: rounding', 'AtHeap.cpp', 'int need = (int)((size + 0x3F) & ~0x3Fu);', 'int need = (int)size;'),
    ('Create: fit edge', 'AtHeap.cpp', 'chunk != 0 && chunk->m_size < need', 'chunk != 0 && chunk->m_size <= need'),
    ('Create: rest edge', 'AtHeap.cpp', 'if (rest > 0x40) {', 'if (rest >= 0x40) {'),
    ('Create: rest kept', 'AtHeap.cpp', '    } else {\n        need = chunk->m_size;\n    }\n', '    }\n'),
    ('Create: after next', 'AtHeap.cpp', '        after->m_next = chunk->m_next;\n', '        after->m_next = 0;\n'),
    ('Create: after back', 'AtHeap.cpp', '        if (chunk->m_next != 0) {\n            chunk->m_next->m_prev = after;\n        }\n', ''),
    # Dropped, equivalent: Create: after prev: the next lines overwrite it: chunk's successor is that block on every path.
    ('Create: unlink middle', 'AtHeap.cpp', '        chunk->m_prev->m_next = chunk->m_next;\n', ''),
    # Dropped, equivalent: Create: unlink head prev: the next lines overwrite it with chunk's own prev.
    ('Create: unlink back', 'AtHeap.cpp', '    if (chunk->m_next != 0) {\n        chunk->m_next->m_prev = chunk->m_prev;\n    }\n', ''),
    ('Create: id', 'AtHeap.cpp', '    heap->Init(chunk, (unsigned int)need, slot, alignShift);\n    heap->m_equivalencyId = id;\n', '    heap->Init(chunk, (unsigned int)need, slot, alignShift);\n'),
    ('Create: fixed', 'AtHeap.cpp', '    heap->m_equivalencyId = id;\n    heap->m_fixed = 0;\n', '    heap->m_equivalencyId = id;\n'),
    ('Create at: id', 'AtHeap.cpp', '    heap->Init(base, size, slot, alignShift);\n    heap->m_equivalencyId = id;\n', '    heap->Init(base, size, slot, alignShift);\n'),
    ('Create at: fixed', 'AtHeap.cpp', '    heap->m_fixed = 1;\n', ''),
    ('Mgr alloc: override always', 'AtHeap.cpp', 'if (m_override != 0 && heap != (AtHeap*)AtIoTokenFile::s_heap) {', 'if (m_override != 0) {'),
    ('Mgr alloc: override never', 'AtHeap.cpp', 'if (m_override != 0 && heap != (AtHeap*)AtIoTokenFile::s_heap) {', 'if (0) {'),
    ('Mgr alloc: override plain', 'AtHeap.cpp', 'result = first->AllocateAligned(size, heap->m_allocShift);', 'result = first->Allocate(size);'),
    ('Mgr alloc: no fallback', 'AtHeap.cpp', '    for (int i = 0; i < kHeaps; i++) {\n        AtHeap* const other = &m_heaps[i];\n        if (other != first && ', '    for (int i = 0; i < 0; i++) {\n        AtHeap* const other = &m_heaps[i];\n        if (other != first && '),
    ('Mgr alloc: first again', 'AtHeap.cpp', 'if (other != first && other->m_size != 0 &&', 'if (other->m_size != 0 &&'),
    ('Mgr alloc: empty slot', 'AtHeap.cpp', 'if (other != first && other->m_size != 0 &&', 'if (other != first &&'),
    ('Mgr alloc: any heap', 'AtHeap.cpp', '&& other->m_equivalencyId == first->m_equivalencyId) {', ') {'),
    ('Mgr alloc: group of asked', 'AtHeap.cpp', '&& other->m_equivalencyId == first->m_equivalencyId) {', '&& other->m_equivalencyId == heap->m_equivalencyId) {'),
    ('Mgr alloc: fallback plain', 'AtHeap.cpp', 'result = other->AllocateAligned(size, heap->m_allocShift);', 'result = other->Allocate(size);'),
    ('Mgr free: fixed order', 'AtHeap.cpp', 'for (int pass = 1; pass >= 0; pass--) {', 'for (int pass = 0; pass <= 1; pass++) {'),
    # Dropped, equivalent: Mgr free: empty slot: Owns of a heap of size 0 is 0 for every pointer.
    ('Mgr free: not freed', 'AtHeap.cpp', '                heap->Free(data);\n', ''),
    ('Mgr free: result', 'AtHeap.cpp', '                heap->Free(data);\n                return 1;\n', '                heap->Free(data);\n                return 0;\n'),
    ('Equivalent: first group', 'AtHeap.cpp', 'int group = a->m_equivalencyId;\n    for (int i = 0; i < kHeaps; i++) {\n        if (m_heaps[i].m_equivalencyId == group) {\n            m_heaps[i].m_equivalencyId = id;\n        }\n    }\n', 'int group = a->m_equivalencyId;\n'),
    ('Equivalent: second group', 'AtHeap.cpp', '    group = b->m_equivalencyId;\n    for (int i = 0; i < kHeaps; i++) {\n        if (m_heaps[i].m_equivalencyId == group) {\n            m_heaps[i].m_equivalencyId = id;\n        }\n    }\n', '    group = b->m_equivalencyId;\n'),
    # Dropped, equivalent: Equivalent: b read first: both orders leave the same ids: when the groups are one, neither second pass changes anything.
    ('Destroy: id', 'AtHeap.cpp', '    heap->m_equivalencyId = NextId();\n', ''),
    ('Destroy: range low', 'AtHeap.cpp', '(char*)chunk >= m_base && (char*)chunk < m_base + m_size && heap->m_fixed == 0', '(char*)chunk < m_base + m_size && heap->m_fixed == 0'),
    ('Destroy: range high', 'AtHeap.cpp', '(char*)chunk >= m_base && (char*)chunk < m_base + m_size && heap->m_fixed == 0', '(char*)chunk >= m_base && heap->m_fixed == 0'),
    ('Destroy: fixed too', 'AtHeap.cpp', ' && heap->m_fixed == 0) {', ') {'),
    ('Destroy: unsorted', 'AtHeap.cpp', 'while (after != 0 && (char*)chunk > (char*)after) {', 'while (0) {'),
    ('Destroy: size', 'AtHeap.cpp', '        chunk->m_size = (int)heap->m_size;\n', ''),
    ('Destroy: links', 'AtHeap.cpp', '        chunk->m_prev = before;\n', ''),
    ('Destroy: head', 'AtHeap.cpp', '            m_chunks = chunk;\n', ''),
    ('Destroy: forward', 'AtHeap.cpp', '            before->m_next = chunk;\n', ''),
    ('Destroy: no join after', 'AtHeap.cpp', 'if ((char*)chunk + chunk->m_size == (char*)after) {', 'if (0) {'),
    ('Destroy: join next', 'AtHeap.cpp', '                chunk->m_next = after->m_next;\n', ''),
    ('Destroy: join size', 'AtHeap.cpp', '                chunk->m_size += after->m_size;\n', ''),
    ('Destroy: join back', 'AtHeap.cpp', '                if (after->m_next != 0) {\n                    after->m_next->m_prev = chunk;\n                }\n', ''),
    ('Destroy: after prev', 'AtHeap.cpp', '            } else {\n                after->m_prev = chunk;\n', '            } else {\n'),
    ('Destroy: no join before', 'AtHeap.cpp', 'if (before != 0 && (char*)before + before->m_size == (char*)chunk) {', 'if (0) {'),
    ('Destroy: join before next', 'AtHeap.cpp', '            before->m_next = chunk->m_next;\n', ''),
    ('Destroy: join before size', 'AtHeap.cpp', '            before->m_size += chunk->m_size;\n', ''),
    ('Destroy: join before back', 'AtHeap.cpp', '            if (chunk->m_next != 0) {\n                chunk->m_next->m_prev = before;\n            }\n', ''),
    ('Destroy: heap kept', 'AtHeap.cpp', '    heap->~AtHeap();\n}\n\nvoid AtHeapManager::Append', '}\n\nvoid AtHeapManager::Append'),
    ('Append: full', 'AtHeap.cpp', '    if (*length >= capacity) {\n        return;\n    }\n', ''),
    ('Append: edge', 'AtHeap.cpp', 'if (total < capacity) {', 'if (total <= capacity) {'),
    ('Append: length', 'AtHeap.cpp', '        *length = total;\n', ''),
    # Dropped, equivalent: Append: copy length: the extra byte copied is at capacity - 1, which the next line writes.
    ('Append: terminator', 'AtHeap.cpp', '        out[capacity - 1] = 0;\n', ''),
    ('Append: full length', 'AtHeap.cpp', '        *length = capacity;\n', ''),
    ('Append: strcat at length', 'AtHeap.cpp', '        strcat(out, text);\n', '        strncpy(out + *length, text, strlen(text) + 1);\n'),
    ('Report: empty slot', 'AtHeap.cpp', '        if (heap.m_size != 0) {\n            // The two zeros', '        if (1) {\n            // The two zeros'),
    ('Report: cleared', 'AtHeap.cpp', '    out[0] = 0;\n', ''),
    ('Report: index', 'AtHeap.cpp', 'Equivalency ID: %2d\\n", i,', 'Equivalency ID: %2d\\n", i + 1,'),
    ('Report: free', 'AtHeap.cpp', 'heap.m_size - heap.m_used, 0, 0,', 'heap.m_used, 0, 0,'),
    ('Report: fragments', 'AtHeap.cpp', 'heap.m_size, heap.FreeBlockCount(), heap.LargestFree()', 'heap.m_size, heap.LargestFree(), heap.FreeBlockCount()'),
    ('Report: id', 'AtHeap.cpp', 'heap.m_alignShift, heap.m_equivalencyId);', 'heap.m_alignShift, heap.m_tag);'),
    ('Report: shift', 'AtHeap.cpp', '0, 0, heap.m_alignShift,', '0, 0, heap.m_allocShift,'),
]


MDB_MUTANTS = [
    ('NewList: no raise without heap', 'AtMaterialDatabase.cpp', 'if (AtMaterialDatabase::s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }', ''),
    ('NewList: other raise', 'AtMaterialDatabase.cpp', 'AtRaise(-2, 0, 0, 0);', 'AtRaise(-3, 0, 0, 0);'),
    ('NewList: no push', 'AtMaterialDatabase.cpp', '    AtPushHeap(AtMaterialDatabase::s_heap);\n', ''),
    ('NewList: no pop', 'AtMaterialDatabase.cpp', '    AtPopHeap();\n', ''),
    ('NewList: size', 'AtMaterialDatabase.cpp', 'operator new(count * sizeof(void*))', 'operator new((count + 1) * sizeof(void*))'),
    ('NewList: no raise on null', 'AtMaterialDatabase.cpp', 'if (list == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }', ''),
    ('NewList: other raise on null', 'AtMaterialDatabase.cpp', 'AtRaise(-4, 0, 0, 0);', 'AtRaise(-5, 0, 0, 0);'),
    ('NewClearedList: not cleared', 'AtMaterialDatabase.cpp', '        list[i] = 0;\n    }\n    return list;', '    }\n    return list;'),
    ('NewClearedList: short', 'AtMaterialDatabase.cpp', 'for (unsigned int i = 0; i < count; i++) {\n        list[i] = 0;', 'for (unsigned int i = 0; i + 1 < count; i++) {\n        list[i] = 0;'),
    ('SetHeap: nothing', 'AtMaterialDatabase.cpp', '    s_heap = heap;\n', '    (void)heap;\n'),
    ('ctor: count', 'AtMaterialDatabase.cpp', 'AtMaterialDatabase::AtMaterialDatabase() {\n    m_count = 0;\n', 'AtMaterialDatabase::AtMaterialDatabase() {\n'),
    ('ctor: stage capacity', 'AtMaterialDatabase.cpp', 'AtMaterialDatabase::AtMaterialDatabase() {\n    m_count = 0;\n    m_stageCapacity = 0;\n', 'AtMaterialDatabase::AtMaterialDatabase() {\n    m_count = 0;\n'),
    ('ctor: unknown', 'AtMaterialDatabase.cpp', 'm_textures = 0;\n    m_unknown30 = 0;\n}\n\nAtMaterialDatabase::~', 'm_textures = 0;\n}\n\nAtMaterialDatabase::~'),
    ('dtor: stages kept', 'AtMaterialDatabase.cpp', 'AtMaterialDatabase::~AtMaterialDatabase() {\n    if (m_stages != 0) {\n        operator delete(m_stages);\n    }\n', 'AtMaterialDatabase::~AtMaterialDatabase() {\n'),
    ('dtor: textures kept', 'AtMaterialDatabase.cpp', '    if (m_textures != 0) {\n        operator delete(m_textures);\n    }\n    GmNameMap::Clear();\n    m_count = 0;\n    m_stageCapacity = 0;\n    m_stageUsed = 0;\n    m_stages = 0;\n    m_textureCapacity = 0;\n    m_textureUsed = 0;\n    m_textures = 0;\n    m_unknown30 = 0;\n}\n\nvoid AtMaterialDatabase::Clear', '    GmNameMap::Clear();\n    m_count = 0;\n    m_stageCapacity = 0;\n    m_stageUsed = 0;\n    m_stages = 0;\n    m_textureCapacity = 0;\n    m_textureUsed = 0;\n    m_textures = 0;\n    m_unknown30 = 0;\n}\n\nvoid AtMaterialDatabase::Clear'),
    ('dtor: used kept', 'AtMaterialDatabase.cpp', '    m_textureUsed = 0;\n    m_textures = 0;\n    m_unknown30 = 0;\n}\n\nvoid AtMaterialDatabase::Clear', '    m_textures = 0;\n    m_unknown30 = 0;\n}\n\nvoid AtMaterialDatabase::Clear'),
    ('dtor: unknown kept', 'AtMaterialDatabase.cpp', '    m_textures = 0;\n    m_unknown30 = 0;\n}\n\nvoid AtMaterialDatabase::Clear', '    m_textures = 0;\n}\n\nvoid AtMaterialDatabase::Clear'),
    ('Clear: stages kept', 'AtMaterialDatabase.cpp', 'void AtMaterialDatabase::Clear() {\n    if (m_stages != 0) {\n        operator delete(m_stages);\n    }\n', 'void AtMaterialDatabase::Clear() {\n'),
    ('Clear: textures kept', 'AtMaterialDatabase.cpp', 'void AtMaterialDatabase::Clear() {\n    if (m_stages != 0) {\n        operator delete(m_stages);\n    }\n    if (m_textures != 0) {\n        operator delete(m_textures);\n    }\n', 'void AtMaterialDatabase::Clear() {\n    if (m_stages != 0) {\n        operator delete(m_stages);\n    }\n'),
    ('Clear: map kept', 'AtMaterialDatabase.cpp', '    GmNameMap::Clear();\n    m_count = 0;\n    m_stageCapacity = 0;\n    m_stageUsed = 0;\n    m_stages = 0;\n    m_textureCapacity = 0;\n    m_textureUsed = 0;\n    m_textures = 0;\n    m_unknown30 = 0;\n}\n\nvoid AtMaterialDatabase::Allocate', '    m_count = 0;\n    m_stageCapacity = 0;\n    m_stageUsed = 0;\n    m_stages = 0;\n    m_textureCapacity = 0;\n    m_textureUsed = 0;\n    m_textures = 0;\n    m_unknown30 = 0;\n}\n\nvoid AtMaterialDatabase::Allocate'),
    ('Clear: count kept', 'AtMaterialDatabase.cpp', '    GmNameMap::Clear();\n    m_count = 0;\n    m_stageCapacity = 0;\n    m_stageUsed = 0;\n    m_stages = 0;\n    m_textureCapacity = 0;\n    m_textureUsed = 0;\n    m_textures = 0;\n    m_unknown30 = 0;\n}\n\nvoid AtMaterialDatabase::Allocate', '    GmNameMap::Clear();\n    m_stageCapacity = 0;\n    m_stageUsed = 0;\n    m_stages = 0;\n    m_textureCapacity = 0;\n    m_textureUsed = 0;\n    m_textures = 0;\n    m_unknown30 = 0;\n}\n\nvoid AtMaterialDatabase::Allocate'),
    ('Clear: used kept', 'AtMaterialDatabase.cpp', '    m_stageUsed = 0;\n    m_stages = 0;\n    m_textureCapacity = 0;\n    m_textureUsed = 0;\n    m_textures = 0;\n    m_unknown30 = 0;\n}\n\nvoid AtMaterialDatabase::Allocate', '    m_stages = 0;\n    m_textureCapacity = 0;\n    m_textureUsed = 0;\n    m_textures = 0;\n    m_unknown30 = 0;\n}\n\nvoid AtMaterialDatabase::Allocate'),
    ('Clear: textures used kept', 'AtMaterialDatabase.cpp', '    m_textureUsed = 0;\n    m_textures = 0;\n    m_unknown30 = 0;\n}\n\nvoid AtMaterialDatabase::Allocate', '    m_textures = 0;\n    m_unknown30 = 0;\n}\n\nvoid AtMaterialDatabase::Allocate'),
    ('Clear: unknown kept', 'AtMaterialDatabase.cpp', '    m_textures = 0;\n    m_unknown30 = 0;\n}\n\nvoid AtMaterialDatabase::Allocate', '    m_textures = 0;\n}\n\nvoid AtMaterialDatabase::Allocate'),
    ('Allocate: no clear', 'AtMaterialDatabase.cpp', 'unsigned int count, const char* ns) {\n    if (m_count > 0) {\n        Clear();\n    }\n', 'unsigned int count, const char* ns) {\n'),
    ('Allocate: swapped', 'AtMaterialDatabase.cpp', '    m_stageCapacity = stages;\n    m_textureCapacity = textures;\n', '    m_stageCapacity = textures;\n    m_textureCapacity = stages;\n'),
    ('Allocate: no count', 'AtMaterialDatabase.cpp', '    m_count = count;\n', ''),
    ('Allocate: no init', 'AtMaterialDatabase.cpp', '    Init(count);\n', ''),
    ('Allocate: no stages', 'AtMaterialDatabase.cpp', '    m_textureCapacity = textures;\n    m_count = count;\n    Init(count);\n    if (m_stageCapacity != 0) {\n        m_stages = NewClearedList(m_stageCapacity);\n    }\n', '    m_textureCapacity = textures;\n    m_count = count;\n    Init(count);\n'),
    ('Allocate: always stages', 'AtMaterialDatabase.cpp', '    Init(count);\n    if (m_stageCapacity != 0) {\n', '    Init(count);\n    {\n'),
    ('Allocate: always textures', 'AtMaterialDatabase.cpp', '        m_stages = NewClearedList(m_stageCapacity);\n    }\n    if (m_textureCapacity != 0) {\n        m_textures = NewClearedList(m_textureCapacity);\n    }\n    OnReady(owner);\n    owner->AddMaterials(this, ns);', '        m_stages = NewClearedList(m_stageCapacity);\n    }\n    {\n        m_textures = NewClearedList(m_textureCapacity);\n    }\n    OnReady(owner);\n    owner->AddMaterials(this, ns);'),
    ('Allocate: not ready', 'AtMaterialDatabase.cpp', '    OnReady(owner);\n    owner->AddMaterials(this, ns);\n}\n\nvoid AtMaterialDatabase::CopyFrom', '    owner->AddMaterials(this, ns);\n}\n\nvoid AtMaterialDatabase::CopyFrom'),
    ('Allocate: not added', 'AtMaterialDatabase.cpp', '    OnReady(owner);\n    owner->AddMaterials(this, ns);\n}\n\nvoid AtMaterialDatabase::CopyFrom', '    OnReady(owner);\n}\n\nvoid AtMaterialDatabase::CopyFrom'),
    ('Allocate: order', 'AtMaterialDatabase.cpp', '    OnReady(owner);\n    owner->AddMaterials(this, ns);\n}\n\nvoid AtMaterialDatabase::CopyFrom', '    owner->AddMaterials(this, ns);\n    OnReady(owner);\n}\n\nvoid AtMaterialDatabase::CopyFrom'),
    ('CopyFrom: no clear', 'AtMaterialDatabase.cpp', 'AtMaterialDatabase& from, const char* ns) {\n    if (m_count > 0) {\n        Clear();\n    }\n', 'AtMaterialDatabase& from, const char* ns) {\n'),
    ('CopyFrom: stage capacity', 'AtMaterialDatabase.cpp', 'm_stageCapacity = from.m_stageCapacity;', 'm_stageCapacity = from.m_textureCapacity;'),
    ('CopyFrom: texture capacity', 'AtMaterialDatabase.cpp', 'm_textureCapacity = from.m_textureCapacity;', 'm_textureCapacity = from.m_stageCapacity;'),
    ('CopyFrom: no init', 'AtMaterialDatabase.cpp', '    Init(m_count);\n    if (m_stageCapacity', '    if (m_stageCapacity'),
    ('CopyFrom: every material', 'AtMaterialDatabase.cpp', 'if (material->m_flags & AtMaterial::kLoaded) {', 'if (true) {'),
    ('CopyFrom: no material', 'AtMaterialDatabase.cpp', 'if (material->m_flags & AtMaterial::kLoaded) {', 'if (false) {'),
    ('CopyFrom: from itself', 'AtMaterialDatabase.cpp', 'AtMaterial* material = from.MaterialAt(i);', 'AtMaterial* material = MaterialAt(i);'),
    ('CopyFrom: not ready', 'AtMaterialDatabase.cpp', '    OnReady(owner);\n    for (unsigned int i = 0; i < m_count; i++) {\n        AtMaterial* material', '    for (unsigned int i = 0; i < m_count; i++) {\n        AtMaterial* material'),
    ('CopyFrom: not added', 'AtMaterialDatabase.cpp', '        }\n    }\n    owner->AddMaterials(this, ns);\n}\n\nvoid AtMaterialDatabase::Load', '        }\n    }\n}\n\nvoid AtMaterialDatabase::Load'),
    ('CopyFrom: no space', 'AtMaterialDatabase.cpp', '        }\n    }\n    owner->AddMaterials(this, ns);\n}\n\nvoid AtMaterialDatabase::Load', '        }\n    }\n    owner->AddMaterials(this, 0);\n}\n\nvoid AtMaterialDatabase::Load'),
    ('Load: other extension', 'AtMaterialDatabase.cpp', 'file.Open(name, "AGM", 2, 0)', 'file.Open(name, "AGT", 2, 0)'),
    ('Load: other flags', 'AtMaterialDatabase.cpp', 'file.Open(name, "AGM", 2, 0)', 'file.Open(name, "AGM", 0, 0)'),
    ('Load: no report', 'AtMaterialDatabase.cpp', 'if (status != 0) {\n        AtReportFileError(status, name, "AGM");\n    }', ''),
    ('Load: reports other status', 'AtMaterialDatabase.cpp', 'AtReportFileError(status, name, "AGM");', 'AtReportFileError(status + 1, name, "AGM");'),
    ('Load: not read', 'AtMaterialDatabase.cpp', '    Read(owner, file.m_tokenizer);\n', ''),
    # Dropped, equivalent: Load: not closed: the token file's destructor closes it, and a second Close does nothing.
    ('ReadTextures: not a string', 'AtMaterialDatabase.cpp', 'if (token == AtIoTokenizer::kString) {', 'if (token == AtIoTokenizer::kInt) {'),
    ('ReadTextures: entry for no texture kept', 'AtMaterialDatabase.cpp', '            list[i] = texture;\n', '            if (texture != 0) list[i] = texture;\n'),
    ('ReadTextures: no missing error', 'AtMaterialDatabase.cpp', 'if (texture == 0) {\n                tokenizer->Error(kTextureNotFound);\n            }', ''),
    ('ReadTextures: other missing error', 'AtMaterialDatabase.cpp', 'enum Problem { kListAlreadyGiven = 0x27, kListIsEmpty = 0x23, kTextureNotFound = 0x25 };', 'enum Problem { kListAlreadyGiven = 0x27, kListIsEmpty = 0x23, kTextureNotFound = 0x24 };'),
    ('ReadTextures: default not null', 'AtMaterialDatabase.cpp', '} else if (token == kDefault) {\n            list[i] = 0;\n', '} else if (token == kDefault) {\n'),
    ('ReadTextures: default errors', 'AtMaterialDatabase.cpp', '} else if (token == kDefault) {\n            list[i] = 0;\n        } else {', '} else {'),
    ('ReadTextures: other kind error', 'AtMaterialDatabase.cpp', 'tokenizer->Error(AtIoTokenizer::kExpectedString);', 'tokenizer->Error(AtIoTokenizer::kExpectedInt);'),
    ('ReadTextures: no right curly', 'AtMaterialDatabase.cpp', '    tokenizer->ExpectRightCurly();\n}\n\nvoid AtMaterialDatabase::Read', '}\n\nvoid AtMaterialDatabase::Read'),
    ('ReadTextures: spaces ignored', 'AtMaterialDatabase.cpp', 'if (withSpaces != 0) {', 'if (false) {'),
    ('ReadTextures: always spaces', 'AtMaterialDatabase.cpp', 'if (withSpaces != 0) {', 'if (true) {'),
    ('ReadTextures: only the flag 1', 'AtMaterialDatabase.cpp', 'if (withSpaces != 0) {', 'if (withSpaces == 1) {'),
    ('ReadTextures: empty space looked up', 'AtMaterialDatabase.cpp', 'texture = space[0] != 0 ? owner->m_textureSpaces.Find(space, name) : owner->m_textures.Find(name);', 'texture = owner->m_textureSpaces.Find(space, name);'),
    ('ReadTextures: space not used', 'AtMaterialDatabase.cpp', 'texture = space[0] != 0 ? owner->m_textureSpaces.Find(space, name) : owner->m_textures.Find(name);', 'texture = owner->m_textures.Find(name);'),
    ('ReadTextures: ns not used', 'AtMaterialDatabase.cpp', 'texture = ns != 0 && ns[0] != 0 ? owner->m_textureSpaces.Find(ns, name) : owner->m_textures.Find(name);', 'texture = owner->m_textures.Find(name);'),
    ('ReadTextures: empty ns looked up', 'AtMaterialDatabase.cpp', 'texture = ns != 0 && ns[0] != 0 ? owner->m_textureSpaces', 'texture = ns != 0 ? owner->m_textureSpaces'),
    ('Read: no clear', 'AtMaterialDatabase.cpp', 'AtIoTokenizer* tokenizer) {\n    if (m_count > 0) {\n        Clear();\n    }\n', 'AtIoTokenizer* tokenizer) {\n'),
    ('Read: other keyword count', 'AtMaterialDatabase.cpp', 'tokenizer->SetKeywords(AtMaterial::s_keywords, AtMaterial::kKeywordCount, 0);', 'tokenizer->SetKeywords(AtMaterial::s_keywords, 7, 0);'),
    ('Read: no keywords', 'AtMaterialDatabase.cpp', '    tokenizer->SetKeywords(AtMaterial::s_keywords, AtMaterial::kKeywordCount, 0);\n', ''),
    ('Read: name space not read', 'AtMaterialDatabase.cpp', '    if (token == kNameSpace) {\n        tokenizer->ExpectName(space);\n        ns = space;\n        token = tokenizer->Next();\n    }\n', ''),
    ('Read: name space not used', 'AtMaterialDatabase.cpp', '        ns = space;\n', ''),
    ('Read: name space anywhere', 'AtMaterialDatabase.cpp', '    while (token != AtIoTokenizer::kEnd) {\n        switch (token) {\n', '    while (token != AtIoTokenizer::kEnd) {\n        switch (token) {\n        case kNameSpace:\n            tokenizer->ExpectName(space);\n            ns = space;\n            break;\n'),
    ('Read: stages given twice', 'AtMaterialDatabase.cpp', 'if (m_stageCapacity > 0) {\n                tokenizer->Error(kListAlreadyGiven);\n            }\n', ''),
    ('Read: textures given twice', 'AtMaterialDatabase.cpp', 'if (m_textureCapacity > 0) {\n                tokenizer->Error(kListAlreadyGiven);\n            }\n', ''),
    ('Read: other twice error', 'AtMaterialDatabase.cpp', 'enum Problem { kListAlreadyGiven = 0x27,', 'enum Problem { kListAlreadyGiven = 0x28,'),
    ('Read: no stages empty error', 'AtMaterialDatabase.cpp', 'if (m_stageCapacity < 1) {\n                tokenizer->Error(kListIsEmpty);\n            }\n', ''),
    ('Read: no textures empty error', 'AtMaterialDatabase.cpp', 'if (m_textureCapacity < 1) {\n                tokenizer->Error(kListIsEmpty);\n            }\n', ''),
    ('Read: other empty error', 'AtMaterialDatabase.cpp', 'kListIsEmpty = 0x23,', 'kListIsEmpty = 0x22,'),
    ('Read: stages cleared', 'AtMaterialDatabase.cpp', 'm_stages = NewList(m_stageCapacity);', 'm_stages = NewClearedList(m_stageCapacity);'),
    ('Read: textures cleared', 'AtMaterialDatabase.cpp', 'm_textures = NewList(m_textureCapacity);', 'm_textures = NewClearedList(m_textureCapacity);'),
    ('Read: stage word confused', 'AtMaterialDatabase.cpp', 'ReadTextures(owner, tokenizer, m_stages, m_stageCapacity, ns, token == kProgShaderTextureWithNameSpace);', 'ReadTextures(owner, tokenizer, m_stages, m_stageCapacity, ns, token == kProgShaderTexture);'),
    ('Read: texture word confused', 'AtMaterialDatabase.cpp', 'ReadTextures(owner, tokenizer, m_textures, m_textureCapacity, ns, token == kStagedShaderTextureWithNameSpace);', 'ReadTextures(owner, tokenizer, m_textures, m_textureCapacity, ns, token == kStagedShaderTexture);'),
    ('Read: stages without ns', 'AtMaterialDatabase.cpp', 'ReadTextures(owner, tokenizer, m_stages, m_stageCapacity, ns,', 'ReadTextures(owner, tokenizer, m_stages, m_stageCapacity, 0,'),
    ('Read: textures without ns', 'AtMaterialDatabase.cpp', 'ReadTextures(owner, tokenizer, m_textures, m_textureCapacity, ns,', 'ReadTextures(owner, tokenizer, m_textures, m_textureCapacity, 0,'),
    ('Read: lists swapped', 'AtMaterialDatabase.cpp', 'kProgShaderTexture = 58, kStagedShaderTexture = 59,', 'kProgShaderTexture = 59, kStagedShaderTexture = 58,'),
    ('Read: with-space lists swapped', 'AtMaterialDatabase.cpp', 'kProgShaderTextureWithNameSpace = 60, kStagedShaderTextureWithNameSpace = 61,', 'kProgShaderTextureWithNameSpace = 61, kStagedShaderTextureWithNameSpace = 60,'),
    ('Read: material no count', 'AtMaterialDatabase.cpp', '            m_count = (unsigned int)tokenizer->ExpectCount();\n            Init(m_count);\n', '            tokenizer->ExpectCount();\n            Init(m_count);\n'),
    ('Read: material no init', 'AtMaterialDatabase.cpp', 'tokenizer->ExpectCount();\n            Init(m_count);\n', 'tokenizer->ExpectCount();\n'),
    ('Read: material not added', 'AtMaterialDatabase.cpp', '            owner->AddMaterials(this, ns);\n            OnReady(owner);\n            for', '            OnReady(owner);\n            for'),
    ('Read: material added without ns', 'AtMaterialDatabase.cpp', '            owner->AddMaterials(this, ns);\n            OnReady(owner);\n            for', '            owner->AddMaterials(this, 0);\n            OnReady(owner);\n            for'),
    ('Read: material added last', 'AtMaterialDatabase.cpp', '            owner->AddMaterials(this, ns);\n            OnReady(owner);\n            for (unsigned int i = 0; i < m_count; i++) {\n                tokenizer->ExpectToken(kMaterial);\n                char name[8];\n                tokenizer->ExpectName(name);\n                AtMaterial* material = MaterialAt(i);\n                Insert(name, material);\n                material->Read(owner, this, tokenizer, ns);\n            }\n', '            OnReady(owner);\n            for (unsigned int i = 0; i < m_count; i++) {\n                tokenizer->ExpectToken(kMaterial);\n                char name[8];\n                tokenizer->ExpectName(name);\n                AtMaterial* material = MaterialAt(i);\n                Insert(name, material);\n                material->Read(owner, this, tokenizer, ns);\n            }\n            owner->AddMaterials(this, ns);\n'),
    ('Read: material not ready', 'AtMaterialDatabase.cpp', '            OnReady(owner);\n            for (unsigned int i = 0; i < m_count; i++) {\n                tokenizer->ExpectToken', '            for (unsigned int i = 0; i < m_count; i++) {\n                tokenizer->ExpectToken'),
    ('Read: no material word', 'AtMaterialDatabase.cpp', '                tokenizer->ExpectToken(kMaterial);\n', ''),
    ('Read: other material word', 'AtMaterialDatabase.cpp', 'kDefault = 62, kMaterial = 63', 'kDefault = 62, kMaterial = 64'),
    ('Read: other default', 'AtMaterialDatabase.cpp', 'kDefault = 62,', 'kDefault = 65,'),
    ('Read: no name', 'AtMaterialDatabase.cpp', '                tokenizer->ExpectName(name);\n', '                name[0] = 0;\n'),
    ('Read: not inserted', 'AtMaterialDatabase.cpp', '                Insert(name, material);\n', ''),
    ('Read: inserted after', 'AtMaterialDatabase.cpp', '                Insert(name, material);\n                material->Read(owner, this, tokenizer, ns);\n', '                material->Read(owner, this, tokenizer, ns);\n                Insert(name, material);\n'),
    ('Read: material not read', 'AtMaterialDatabase.cpp', '                material->Read(owner, this, tokenizer, ns);\n', ''),
    ('Read: material without ns', 'AtMaterialDatabase.cpp', 'material->Read(owner, this, tokenizer, ns);', 'material->Read(owner, this, tokenizer, 0);'),
    ('Read: no closing curly', 'AtMaterialDatabase.cpp', '            tokenizer->ExpectRightCurly();\n            break;\n', '            break;\n'),
    ('Read: other word error', 'AtMaterialDatabase.cpp', 'tokenizer->Error(AtIoTokenizer::kExpectedKeyword);', 'tokenizer->Error(AtIoTokenizer::kSyntaxError);'),
    ('Read: no word error', 'AtMaterialDatabase.cpp', '            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n', ''),
    ('Read: stops at a word', 'AtMaterialDatabase.cpp', '            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n            break;\n', '            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n            return;\n'),
    ('FindShader: empty ns looked up', 'AtMaterialOwner.cpp', 'if (ns != 0 && ns[0] != 0) {\n        return m_shaderSpaces.Find(ns, name);', 'if (ns != 0) {\n        return m_shaderSpaces.Find(ns, name);'),
    ('FindShader: ns not used', 'AtMaterialOwner.cpp', 'return m_shaderSpaces.Find(ns, name);', 'return m_shaders.Find(name);'),
    ('FindShader: other space table', 'AtMaterialOwner.cpp', 'return m_shaderSpaces.Find(ns, name);', 'return m_textureSpaces.Find(ns, name);'),
    ('FindShader: other group', 'AtMaterialOwner.cpp', 'return m_shaders.Find(name);', 'return m_textures.Find(name);'),
    ('AddMaterials: empty ns used', 'AtMaterialOwner.cpp', 'if (ns != 0 && ns[0] != 0) {\n        m_materialSpaces.Add(ns, map);', 'if (ns != 0) {\n        m_materialSpaces.Add(ns, map);'),
    ('AddMaterials: other space table', 'AtMaterialOwner.cpp', 'm_materialSpaces.Add(ns, map);', 'm_textureSpaces.Add(ns, map);'),
    ('AddMaterials: and to the group', 'AtMaterialOwner.cpp', 'm_materialSpaces.Add(ns, map);\n        return;', 'm_materialSpaces.Add(ns, map);'),
    ('AddMaterials: other group', 'AtMaterialOwner.cpp', 'm_materials.Add(map);', 'm_textures.Add(map);'),
]

SUITES = {
    "gm": {"target": "oracle_gm", "subsystem": "gm/containers", "mutants": GM_MUTANTS, "src": "gm", "var": "KND_GM_SRC"},
    "jam": {"target": "oracle_jam", "subsystem": "at/jam", "mutants": JAM_MUTANTS, "src": "at", "var": "KND_AT_SRC"},
    "lvl": {"target": "oracle_lvl", "subsystem": "gm/levels", "mutants": LVL_MUTANTS, "src": "gm", "var": "KND_GM_SRC"},
    "mnt": {"target": "oracle_mnt", "subsystem": "at/mount", "mutants": MNT_MUTANTS, "src": "at", "var": "KND_AT_SRC"},
    "rmb": {"target": "oracle_rmb", "subsystem": ("gm/names", "gm/rumble"), "mutants": RMB_MUTANTS, "src": "gm", "var": "KND_GM_SRC"},
    "txt": {"target": "oracle_txt", "subsystem": "at/gametext", "mutants": TXT_MUTANTS, "src": "at", "var": "KND_AT_SRC"},
    "set": {"target": "oracle_set", "subsystem": "gm/settings", "mutants": SET_MUTANTS, "src": "gm", "var": "KND_GM_SRC"},
    "mat": {"target": "oracle_mat", "subsystem": "at/material", "mutants": MAT_MUTANTS, "src": "at", "var": "KND_AT_SRC"},
    "mdb": {"target": "oracle_mdb", "subsystem": "at/matdb", "mutants": MDB_MUTANTS, "src": "at", "var": "KND_AT_SRC"},
    "qua": {"target": "oracle_qua", "subsystem": "at/quaternion", "mutants": QUA_MUTANTS, "src": "at", "var": "KND_AT_SRC"},
    "fmt": {"target": "oracle_fmt", "subsystem": "at/pixelformat", "mutants": FMT_MUTANTS, "src": "at", "var": "KND_AT_SRC"},
    "vec": {"target": "oracle_vec", "subsystem": "at/vector", "mutants": VEC_MUTANTS, "src": "at", "var": "KND_AT_SRC"},
    "mem": {"target": "oracle_mem", "subsystem": "at/memory", "mutants": MEM_MUTANTS, "src": "at", "var": "KND_AT_SRC"},
    "hep": {"target": "oracle_hep", "subsystem": "at/heap", "mutants": HEP_MUTANTS, "src": "at", "var": "KND_AT_SRC"},
    "ldr": {"target": "oracle_ldr", "subsystem": "gm/loader", "mutants": LDR_MUTANTS, "src": "gm", "var": "KND_GM_SRC"},
    "aud": {"target": "oracle_aud", "subsystem": "gm/audiodata", "mutants": AUD_MUTANTS, "src": "gm", "var": "KND_GM_SRC"},
    "tok": {"target": "oracle_tok", "subsystem": "at/text", "mutants": TOK_MUTANTS, "src": "at", "var": "KND_AT_SRC"},
}


# Suites kept apart from this file, one to a subsystem: tests/oracle/suites/<name>.py defines SUITE, a dict like those above with "cases": "tok" for the
# text cases, "jam" for the archive cases, or "none" for no directory.
for _path in sorted(glob.glob(os.path.join(ROOT, "tests", "oracle", "suites", "*.py"))):
    _names = {}
    with open(_path, encoding="utf-8") as _f:
        exec(compile(_f.read(), _path, "exec"), _names)
    SUITES[os.path.splitext(os.path.basename(_path))[0]] = _names["SUITE"]


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors="replace", **kw)


def configure_and_build(build_dir, target, extra=()):
    r = run(["cmake", "-S", ROOT, "-B", build_dir, "-A", "Win32"] + list(extra))
    if r.returncode:
        raise SystemExit("configure failed:\n" + r.stdout + r.stderr)
    r = run(["cmake", "--build", build_dir, "--config", CONFIG, "--target", target])
    if r.returncode:
        return False, r.stdout + r.stderr
    return True, ""


def exe(build_dir, target):
    return os.path.join(build_dir, CONFIG, target + ".exe")


def write_source(src, dest, leaf, mutant=None):
    """Writing gives the file a fresh timestamp, so the build system recompiles it."""
    with open(os.path.join(src, leaf), encoding="utf-8") as f:
        text = f.read()
    if mutant:
        if text.count(mutant[2]) != 1:
            raise SystemExit("mutant %r: text occurs %d times, expected 1" % (mutant[0], text.count(mutant[2])))
        text = text.replace(mutant[2], mutant[3])
    with open(os.path.join(dest, leaf), "w", encoding="utf-8") as f:
        f.write(text)


class MutantWorker:
    """One private copy of the sources and one build tree. Moving from one mutant to the next rewrites only the file the last
    one changed and the file this one changes. build() refuses a build that did not recompile the mutated file, because a
    reused object would be judged as if it were the mutant."""

    def __init__(self, index, suite, cfg):
        self.suite, self.cfg = suite, cfg
        self.src = os.path.join(ROOT, "src", cfg["src"])
        self.dest = "%s-%d" % (MUTANT_SRC, index)
        self.build_dir = "%s-%d" % (MUTANT_BUILD, index)
        self.previous = None

    def start(self):
        shutil.rmtree(self.dest, ignore_errors=True)
        shutil.rmtree(self.build_dir, ignore_errors=True)
        os.makedirs(self.dest)
        for leaf in sorted(os.listdir(self.src)):
            write_source(self.src, self.dest, leaf)
        var = ["-D%s=%s" % (self.cfg["var"], self.dest.replace("\\", "/"))]
        ok, log = configure_and_build(self.build_dir, self.cfg["target"], var)
        if not ok:
            raise SystemExit("positive control does not compile:\n" + log)
        return run_mutant(self.suite, self.build_dir)

    def build(self, mutant):
        if self.previous and self.previous != mutant[1]:
            write_source(self.src, self.dest, self.previous)
        write_source(self.src, self.dest, mutant[1], mutant)
        self.previous = mutant[1]
        target = self.cfg["target"]
        # An output or debug file can be held for a moment, by a scanner or a parallel build. Only those errors are retried.
        log = ""
        for _ in range(6):
            r = run(["cmake", "--build", self.build_dir, "--config", CONFIG, "--target", target])
            log += r.stdout + r.stderr  # All attempts: a retry after a link error has nothing left to compile.
            if r.returncode == 0 or not re.search(r"LNK1104|LNK1201|C1041|C1090", r.stdout + r.stderr):
                break
            time.sleep(2)
        if r.returncode:
            raise SystemExit("mutant %r does not compile:\n%s" % (mutant[0], log))
        if mutant[1].endswith(".cpp") and mutant[1] not in log:
            raise SystemExit("mutant %r: the build did not recompile %s" % (mutant[0], mutant[1]))

    def stop(self):
        shutil.rmtree(self.build_dir, ignore_errors=True)  # The cached source directory belongs to this suite.
        shutil.rmtree(self.dest, ignore_errors=True)


def command(suite, build_dir, ops=500, seed=1, entries=None, mutate=False):
    target = SUITES[suite]["target"]
    apart = SUITES[suite].get("cases")
    if apart:
        cmd = [exe(build_dir, target), XBE] + ({"tok": [TOK_CASES], "jam": [CASES], "none": []}[apart])
    elif suite == "gm":
        cmd = [exe(build_dir, target), XBE, "--seed", str(seed), "--ops", str(ops)]
    elif suite == "mnt":
        cmd = [exe(build_dir, target), XBE, CASES]
    elif suite in ("hep", "mem", "vec", "fmt", "qua"):
        cmd = [exe(build_dir, target), XBE]
    elif suite in ("tok", "lvl", "rmb", "txt", "set", "aud", "ldr", "mat", "mdb"):
        cmd = [exe(build_dir, target), XBE, TOK_CASES]
    else:
        cmd = [exe(build_dir, target), XBE, CASES]
    if entries:
        cmd += ["--entries", entries]
    if mutate:
        cmd += ["--mutate"]
    return cmd


QUICK_READY = set()


def quick_cases():
    """A few cases, so most mutants are decided in a second. A mutant that survives them gets the full set before it counts as
    surviving, so this changes the run time and never the verdict."""
    quick = os.path.join(ROOT, "build", "oracle-jam-quick")
    if quick in QUICK_READY:
        return quick
    QUICK_READY.add(quick)
    shutil.rmtree(quick, ignore_errors=True)
    os.makedirs(quick)
    for leaf in os.listdir(CASES):
        if leaf.split(".")[0] in ("synth_fsta", "synth_fstr", "synth_jam2", "fstr_Credits", "fstr_Aamalgam", "Persist", "FeCred", "CoDialog", "MainMenu"):
            shutil.copy(os.path.join(CASES, leaf), quick)
    return quick


def run_mutant(suite, build_dir):
    def attempt(cmd, seconds):
        try:
            return run(cmd, timeout=seconds).returncode
        except subprocess.TimeoutExpired:
            return "timeout"
    if suite == "jam":
        cmd = [exe(build_dir, "oracle_jam"), XBE, quick_cases()]
        code = attempt(cmd, 60)
        if code != 0:
            return code
    if suite == "tok":
        code = attempt(command(suite, build_dir) + ["--limit", "40"], 120)
        if code != 0:
            return code
        return attempt(command(suite, build_dir), 600)
    return attempt(command(suite, build_dir), 180)


def check_mutants(cfg):
    """Every mutant's text must occur exactly once, so a long run cannot stop halfway over a stale one."""
    src = os.path.join(ROOT, "src", cfg["src"])
    bad = []
    for name, file, old, _ in cfg["mutants"]:
        with open(os.path.join(src, file), encoding="utf-8") as f:
            count = f.read().count(old)
        if count != 1:
            bad.append("%s: text occurs %d times" % (name, count))
    if bad:
        raise SystemExit("stale mutants:\n  " + "\n  ".join(bad))


def run_suite(suite, a, funcs):
    cfg = SUITES[suite]
    check_mutants(cfg)
    target = cfg["target"]
    funcs = [f for f in funcs if f["subsystem"].startswith(cfg["subsystem"])]
    ok, log = configure_and_build(BUILD, target)
    if not ok:
        print(log)
        raise SystemExit(2)
    entries = os.path.join(ROOT, "build", "oracle-entries-%s.txt" % suite)
    with open(entries, "w") as f:
        f.write("\n".join("%X" % fn["va"] for fn in funcs) + "\n")
    report = {"ops_per_seed": a.ops if suite == "gm" else None, "seeds": [], "mutants": []}
    good = True
    unreached = set()
    for seed in range(1, (a.seeds if suite == "gm" else 1) + 1):
        r = run(command(suite, BUILD, a.ops, seed, entries), timeout=900)
        print(r.stdout.strip().splitlines()[-1] if r.stdout.strip() else "no output, exit %d" % r.returncode)
        missing = re.findall(r"entry not reached: 0x([0-9A-F]+)", r.stdout)
        unreached.update(int(m, 16) for m in missing)
        report["seeds"].append({"seed": seed, "exit": r.returncode, "summary": r.stdout.strip().splitlines()[-1:],
                                "not_reached": missing})
        good &= r.returncode == 0
    distinct = len({fn["va"] for fn in funcs})
    report["entries_reached"] = distinct - len(unreached)
    report["entries_total"] = distinct
    report["entries_not_reached"] = ["0x%08X" % v for v in sorted(unreached)]
    print("entries reached in every run: %d / %d %s" % (report["entries_reached"], distinct,
                                                        sorted(hex(v) for v in unreached) or ""))
    good &= not unreached

    r = run(command(suite, BUILD, mutate=True), timeout=300)
    print("harness mutation (--mutate): exit %d" % r.returncode)
    report["harness_mutation_exit"] = r.returncode
    good &= r.returncode == 1

    if not a.no_mutants:
        survived = []
        quick_cases()  # Made once, before any worker can want it.
        jobs = max(1, min(a.jobs, len(cfg["mutants"])))
        workers = [MutantWorker(i, suite, cfg) for i in range(jobs)]
        # Every tree has to pass unmodified, or every mutant built in it would look detected, whatever it does.
        with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
            codes = list(pool.map(lambda w: w.start(), workers))
        print("  %-46s %s" % ("(unmodified copy: positive control)", "passes" if not any(codes) else "FAILS: %s" % codes))
        report["mutant_positive_control_exit"] = next((c for c in codes if c), 0)
        if any(codes):
            raise SystemExit("positive control failed (exit %s): mutant results would be meaningless" % codes)
        pending = list(enumerate(cfg["mutants"]))
        pending.reverse()
        results = {}
        lock = threading.Lock()
        stop = threading.Event()

        def work(worker):
            while not stop.is_set():
                with lock:
                    if not pending:
                        return
                    i, mutant = pending.pop()
                try:
                    worker.build(mutant)
                    results[i] = run_mutant(suite, worker.build_dir)
                except BaseException:
                    stop.set()
                    raise

        with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
            for f in [pool.submit(work, w) for w in workers]:
                f.result()
        # A busy machine can run a slow test out of time. A hang counts as detection, so a hang has to be shown on an idle one.
        for i, code in sorted(results.items()):
            if code == "timeout" and jobs > 1:
                workers[0].build(cfg["mutants"][i])
                results[i] = run_mutant(suite, workers[0].build_dir)
        for i, mutant in enumerate(cfg["mutants"]):
            code = results[i]
            verdict = ("detected (difference)" if code == 1 else "survived" if code == 0
                       else "detected (crash/hang: %s)" % code)
            print("  %-46s %s" % (mutant[0], verdict))
            report["mutants"].append({"name": mutant[0], "verdict": verdict})
            if code == 0:
                survived.append(mutant[0])
        print("mutants detected: %d / %d" % (len(cfg["mutants"]) - len(survived), len(cfg["mutants"])))
        good &= not survived
        for worker in workers:
            worker.stop()
    return good, report


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", type=int, default=5)
    ap.add_argument("--ops", type=int, default=3000)
    ap.add_argument("--no-mutants", action="store_true")
    ap.add_argument("--jobs", type=int, default=max(1, min(6, (os.cpu_count() or 2) - 1)), help="mutants built and run at once")
    ap.add_argument("--suite", choices=list(SUITES) + ["all"], default="all")
    a = ap.parse_args(argv)
    if not os.path.exists(XBE):
        print("reference XBE not found (owner input)")
        return 2
    funcs = fnconfig.load(ROOT)["function"]
    report = {"xbe_sha256": hashlib.sha256(open(XBE, "rb").read()).hexdigest(), "suites": {}}
    path = os.path.join(ROOT, "build", "oracle-report.json")
    earlier = {}
    if os.path.exists(path):
        with open(path) as f:
            earlier = json.load(f).get("suites", {})
    if a.suite != "all":
        report["suites"] = dict(earlier)
    good = True
    for suite in (list(SUITES) if a.suite == "all" else [a.suite]):
        print("== suite %s" % suite)
        ok, rep = run_suite(suite, a, funcs)
        if a.no_mutants and earlier.get(suite, {}).get("mutants"):
            # What mutants showed stays on record, marked as older than this run.
            rep["mutants"] = earlier[suite]["mutants"]
            rep["mutant_positive_control_exit"] = earlier[suite].get("mutant_positive_control_exit")
            rep["mutants_from_earlier_run"] = True
        report["suites"][suite] = rep
        good &= ok
    with open(path, "w") as f:
        json.dump(report, f, indent=1)
    return 0 if good else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
