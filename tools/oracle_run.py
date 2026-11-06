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
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "build", "oracle")
MUTANT_BUILD = os.path.join(ROOT, "build", "oracle-mutant")
MUTANT_SRC = os.path.join(ROOT, "build", "oracle-mutant-src")
CONFIG = "RelWithDebInfo"
XBE = os.path.join(ROOT, "orig", "xiso", "default.xbe")

CASES = os.path.join(ROOT, "analysis-local", "oracle-jam")
TOK_CASES = os.path.join(ROOT, "analysis-local", "oracle-tok")

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
    ('Detach: parent not told', 'GmNameMap.cpp', '            if (m_first == 0 && m_parent != 0) {\n                m_parent->RemoveFromParent(this);\n            }\n', ''),
    ('Detach: parent told early', 'GmNameMap.cpp', 'if (m_first == 0 && m_parent != 0) {', 'if (m_parent != 0) {'),
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


SUITES = {
    "gm": {"target": "oracle_gm", "subsystem": "gm/containers", "mutants": GM_MUTANTS, "src": "gm", "var": "KND_GM_SRC"},
    "jam": {"target": "oracle_jam", "subsystem": "at/jam", "mutants": JAM_MUTANTS, "src": "at", "var": "KND_AT_SRC"},
    "lvl": {"target": "oracle_lvl", "subsystem": "gm/levels", "mutants": LVL_MUTANTS, "src": "gm", "var": "KND_GM_SRC"},
    "mnt": {"target": "oracle_mnt", "subsystem": "at/mount", "mutants": MNT_MUTANTS, "src": "at", "var": "KND_AT_SRC"},
    "rmb": {"target": "oracle_rmb", "subsystem": ("gm/names", "gm/rumble"), "mutants": RMB_MUTANTS, "src": "gm", "var": "KND_GM_SRC"},
    "tok": {"target": "oracle_tok", "subsystem": "at/text", "mutants": TOK_MUTANTS, "src": "at", "var": "KND_AT_SRC"},
}


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


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


def write_variant(src, mutant):
    """Write every source file with a fresh timestamp. A copy that kept old timestamps would leave the previous mutant's
    object file in place, and the build system would silently reuse it."""
    shutil.rmtree(MUTANT_SRC, ignore_errors=True)
    os.makedirs(MUTANT_SRC)
    for leaf in sorted(os.listdir(src)):
        with open(os.path.join(src, leaf), encoding="utf-8") as f:
            text = f.read()
        if mutant and leaf == mutant[1]:
            if text.count(mutant[2]) != 1:
                raise SystemExit("mutant %r: text occurs %d times, expected 1" % (mutant[0], text.count(mutant[2])))
            text = text.replace(mutant[2], mutant[3])
        with open(os.path.join(MUTANT_SRC, leaf), "w", encoding="utf-8") as f:
            f.write(text)


def command(suite, build_dir, ops=500, seed=1, entries=None, mutate=False):
    target = SUITES[suite]["target"]
    if suite == "gm":
        cmd = [exe(build_dir, target), XBE, "--seed", str(seed), "--ops", str(ops)]
    elif suite == "mnt":
        cmd = [exe(build_dir, target), XBE, CASES]
    elif suite in ("tok", "lvl", "rmb"):
        cmd = [exe(build_dir, target), XBE, TOK_CASES]
    else:
        cmd = [exe(build_dir, target), XBE, CASES]
    if entries:
        cmd += ["--entries", entries]
    if mutate:
        cmd += ["--mutate"]
    return cmd


def quick_cases():
    """A few cases, so most mutants are decided in a second. A mutant that survives them gets the full set before it counts as
    surviving, so this changes the run time and never the verdict."""
    quick = os.path.join(ROOT, "build", "oracle-jam-quick")
    shutil.rmtree(quick, ignore_errors=True)
    os.makedirs(quick)
    for leaf in os.listdir(CASES):
        if leaf.split(".")[0] in ("synth_fsta", "synth_fstr", "synth_jam2", "fstr_Credits", "fstr_Aamalgam", "Persist", "FeCred", "CoDialog", "MainMenu"):
            shutil.copy(os.path.join(CASES, leaf), quick)
    return quick


def run_mutant(suite):
    def attempt(cmd, seconds):
        try:
            return run(cmd, timeout=seconds).returncode
        except subprocess.TimeoutExpired:
            return "timeout"
    if suite == "jam":
        cmd = [exe(MUTANT_BUILD, "oracle_jam"), XBE, quick_cases()]
        code = attempt(cmd, 60)
        if code != 0:
            return code
    if suite == "tok":
        code = attempt(command(suite, MUTANT_BUILD) + ["--limit", "40"], 120)
        if code != 0:
            return code
        return attempt(command(suite, MUTANT_BUILD), 600)
    return attempt(command(suite, MUTANT_BUILD), 180)


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
        src = os.path.join(ROOT, "src", cfg["src"])
        var = ["-D%s=%s" % (cfg["var"], MUTANT_SRC.replace("\\", "/"))]
        survived = []
        write_variant(src, None)
        ok, log = configure_and_build(MUTANT_BUILD, target, var)
        if not ok:
            raise SystemExit("positive control does not compile:\n" + log)
        code = run_mutant(suite)
        print("  %-46s %s" % ("(unmodified copy: positive control)", "passes" if code == 0 else "FAILS: %s" % code))
        report["mutant_positive_control_exit"] = code
        if code != 0:
            # Every mutant would then look detected, whatever it does. No count from such a run means anything.
            raise SystemExit("positive control failed (exit %s): mutant results would be meaningless" % code)
        for mutant in cfg["mutants"]:
            write_variant(src, mutant)
            ok, log = configure_and_build(MUTANT_BUILD, target, var)
            if not ok:
                raise SystemExit("mutant %r does not compile:\n%s" % (mutant[0], log))
            code = run_mutant(suite)
            verdict = ("detected (difference)" if code == 1 else "survived" if code == 0
                       else "detected (crash/hang: %s)" % code)
            print("  %-46s %s" % (mutant[0], verdict))
            report["mutants"].append({"name": mutant[0], "verdict": verdict})
            if code == 0:
                survived.append(mutant[0])
        print("mutants detected: %d / %d" % (len(cfg["mutants"]) - len(survived), len(cfg["mutants"])))
        good &= not survived
        shutil.rmtree(MUTANT_BUILD, ignore_errors=True)  # The cached source directory belongs to this suite.
    return good, report


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", type=int, default=5)
    ap.add_argument("--ops", type=int, default=3000)
    ap.add_argument("--no-mutants", action="store_true")
    ap.add_argument("--suite", choices=["gm", "jam", "tok", "lvl", "mnt", "rmb", "all"], default="all")
    a = ap.parse_args(argv)
    if not os.path.exists(XBE):
        print("reference XBE not found (owner input)")
        return 2
    with open(os.path.join(ROOT, "config", "functions.toml"), "rb") as f:
        funcs = tomllib.load(f)["function"]
    report = {"xbe_sha256": hashlib.sha256(open(XBE, "rb").read()).hexdigest(), "suites": {}}
    path = os.path.join(ROOT, "build", "oracle-report.json")
    if a.suite != "all" and os.path.exists(path):
        with open(path) as f:
            report["suites"] = json.load(f).get("suites", {})
    good = True
    for suite in (["gm", "jam", "tok", "lvl", "mnt", "rmb"] if a.suite == "all" else [a.suite]):
        print("== suite %s" % suite)
        ok, rep = run_suite(suite, a, funcs)
        report["suites"][suite] = rep
        good &= ok
    with open(path, "w") as f:
        json.dump(report, f, indent=1)
    return 0 if good else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
