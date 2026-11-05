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
    ("File Clear: path kept", F, "void AtIoJamFile::Clear() {\n    m_flags = 0;\n    for (int i = 0; i < 0x20; i++) {\n        ((unsigned int*)m_path)[i] = 0;\n    }\n", "void AtIoJamFile::Clear() {\n    m_flags = 0;\n"),
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

SUITES = {
    "gm": {"target": "oracle_gm", "subsystem": "gm/", "mutants": GM_MUTANTS, "src": "gm", "var": "KND_GM_SRC"},
    "jam": {"target": "oracle_jam", "subsystem": "at/", "mutants": JAM_MUTANTS, "src": "at", "var": "KND_AT_SRC"},
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
    else:
        cmd = [exe(build_dir, target), XBE, CASES]
    if entries:
        cmd += ["--entries", entries]
    if mutate:
        cmd += ["--mutate"]
    return cmd


def run_mutant(suite):
    try:
        return run(command(suite, MUTANT_BUILD), timeout=90).returncode
    except subprocess.TimeoutExpired:
        return "timeout"


def run_suite(suite, a, funcs):
    cfg = SUITES[suite]
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
        good &= code == 0
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
    ap.add_argument("--suite", choices=["gm", "jam", "all"], default="all")
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
    for suite in (["gm", "jam"] if a.suite == "all" else [a.suite]):
        print("== suite %s" % suite)
        ok, rep = run_suite(suite, a, funcs)
        report["suites"][suite] = rep
        good &= ok
    with open(path, "w") as f:
        json.dump(report, f, indent=1)
    return 0 if good else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
