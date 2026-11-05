#!/usr/bin/env python3
"""Read-only disassembly of the reference XBE for analysis (output is derived from game code: keep it local).

  python -I tools/xdis.py <va> [<va> ...] [--max N] [--xbe orig/xiso/default.xbe]
  python -I tools/xdis.py --xrefs <va> [<va> ...]     # direct E8/E9 callers and absolute-address references in code

A function listing stops at the first return or unconditional jump past every branch target seen so far.
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xbe as xbemod  # noqa: E402
from capstone import CS_ARCH_X86, CS_MODE_32, Cs  # noqa: E402


def listing(md, x, va, limit):
    data = x.read_va(va, limit)
    furthest, out = va, []
    for i in md.disasm(data, va):
        out.append("%08X  %-22s %s %s" % (i.address, i.bytes.hex(), i.mnemonic, i.op_str))
        if i.mnemonic.startswith("j") or i.mnemonic == "loop":
            try:
                t = int(i.op_str, 16)
                if t > furthest and t < va + limit:
                    furthest = t
            except ValueError:
                pass
        if i.mnemonic in ("ret", "retn", "jmp", "int3") and i.address >= furthest:
            break
    return out


def xrefs(x, targets):
    """Byte scan of the executable sections. E8/E9 hits are candidates: a scan cannot prove an instruction boundary."""
    found = {t: [] for t in targets}
    for s in x.sections:
        if not s.flags & 0x4:  # executable flag
            continue
        d = x.read_va(s.va, s.rsize)
        for off in range(len(d) - 5):
            if d[off] in (0xE8, 0xE9):
                t = (s.va + off + 5 + struct.unpack_from("<i", d, off + 1)[0]) & 0xFFFFFFFF
                if t in found:
                    found[t].append(("call" if d[off] == 0xE8 else "jmp", s.va + off))
        for t in targets:
            pat = struct.pack("<I", t)
            p = d.find(pat)
            while p >= 0:
                found[t].append(("abs", s.va + p))
                p = d.find(pat, p + 1)
    return found


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("va", nargs="+")
    ap.add_argument("--max", type=int, default=0x400)
    ap.add_argument("--xrefs", action="store_true")
    ap.add_argument("--xbe", default="orig/xiso/default.xbe")
    a = ap.parse_args(argv)
    x = xbemod.load(a.xbe)
    vas = [int(v, 16) for v in a.va]
    if a.xrefs:
        for t, hits in xrefs(x, vas).items():
            print("0x%08X: %d refs" % (t, len(hits)))
            for k, v in hits[:40]:
                print("   %-4s 0x%08X" % (k, v))
        return 0
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    for v in vas:
        print("--- 0x%08X" % v)
        print("\n".join(listing(md, x, v, a.max)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
