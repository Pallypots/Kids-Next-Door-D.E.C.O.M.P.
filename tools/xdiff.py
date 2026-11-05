#!/usr/bin/env python3
"""Side-by-side disassembly of a candidate object function and the original bytes (diagnostic only; the verdict
comes from xcheck). Relocated fields show the symbol name on the candidate side.

  python -I tools/xdiff.py <obj> <symbol> <va> [--xbe orig/xiso/default.xbe]
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xbe as xbemod  # noqa: E402
import xcheck  # noqa: E402
from capstone import CS_ARCH_X86, CS_MODE_32, Cs  # noqa: E402


def lines(md, code, va):
    return ["%08X %-20s %s %s" % (i.address, i.bytes.hex(), i.mnemonic, i.op_str) for i in md.disasm(code, va)]


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("obj")
    ap.add_argument("symbol")
    ap.add_argument("va")
    ap.add_argument("--size", type=int, default=0)
    ap.add_argument("--xbe", default="orig/xiso/default.xbe")
    a = ap.parse_args(argv)
    va = int(a.va, 16)
    x = xbemod.load(a.xbe)
    cand, relocs = xcheck.load(a.obj).function(a.symbol)
    orig = x.read_va(va, a.size or len(cand))
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    L, R = lines(md, orig, va), lines(md, cand, va)
    print("%-62s | %s" % ("ORIGINAL", "CANDIDATE"))
    for k in range(max(len(L), len(R))):
        l = L[k] if k < len(L) else ""
        r = R[k] if k < len(R) else ""
        print("%-62s %s %s" % (l, "|" if l.split(" ", 1)[-1] == r.split(" ", 1)[-1] else "X", r))
    for off, typ, name in relocs:
        print("  reloc +0x%X %s -> %s" % (off, xcheck.REL_I386.get(typ, typ), name))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
