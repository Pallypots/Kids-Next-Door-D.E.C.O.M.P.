#!/usr/bin/env python3
"""Linear disassembly of an address range with string annotations (analysis output: keep it local).

  python -I tools/xlist.py <lo> <hi> [--xbe orig/xiso/default.xbe]
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xbe as xbemod  # noqa: E402
from capstone import CS_ARCH_X86, CS_MODE_32, Cs  # noqa: E402


def ascii_at(x, va):
    try:
        b = x.read_va(va, 96)
    except Exception:  # noqa: BLE001 - not a readable address: simply no annotation
        return None
    e = b.find(b"\0")
    if e >= 3 and all(32 <= c < 127 or c in (9, 10, 13) for c in b[:e]):
        return b[:e].decode("latin-1")
    return None


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("lo")
    ap.add_argument("hi")
    ap.add_argument("--xbe", default="orig/xiso/default.xbe")
    a = ap.parse_args(argv)
    lo, hi = int(a.lo, 16), int(a.hi, 16)
    x = xbemod.load(a.xbe)
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    for i in md.disasm(x.read_va(lo, hi - lo), lo):
        if i.mnemonic == "int3":
            continue
        note = ""
        m = re.search(r"0x([0-9a-f]{5,8})\b", i.op_str)
        if m and i.mnemonic in ("push", "mov", "lea", "cmp"):
            s = ascii_at(x, int(m.group(1), 16))
            if s:
                note = '  ; "%s"' % s.replace("\n", "\\n")
        print("%08X %s %s%s" % (i.address, i.mnemonic, i.op_str, note))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
