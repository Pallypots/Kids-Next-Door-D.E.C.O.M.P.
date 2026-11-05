#!/usr/bin/env python3
"""Bounded compiler-flag experiment: how many recorded functions match under each flag set. Diagnostic only.

  python -I tools/flagscan.py

Results go to docs/toolchain.md by hand, with the date. A better count here does not prove a flag was used for the
original build: the host compiler is not the original compiler.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import verify  # noqa: E402
import xbe as xbemod  # noqa: E402
import xcheck  # noqa: E402

VARIANTS = {
    "baseline (/O2)": [],
    "/O2 /Ob1": ["/Ob1"],
    "/Ox": ["/Ox"],
    "/O1": ["/O1"],
    "/O2 /Os": ["/Os"],
    "/O2 /Oi-": ["/Oi-"],
    "/Oy-": ["/Oy-"],
    "no /Gy": ["/Gy-"],
}


def main():
    fcfg, tcfg = verify.load_config()
    comp = tcfg["compiler"]["msvc14-host"]
    x = xbemod.load(os.path.join(verify.ROOT, "orig", "xiso", "default.xbe"))
    symmap = verify.symbol_map(fcfg)
    for label, extra in VARIANTS.items():
        tag = "".join(c if c.isalnum() else "_" for c in label)
        objs = {}
        for src in sorted({f["source"] for f in fcfg["function"]}):
            obj = os.path.join(verify.ROOT, "build", "flagscan", tag, src.replace("/", "_") + ".obj")
            ok, log = verify.compile_src(comp, src, obj, extra_flags=extra)
            objs[src] = xcheck.load(obj) if ok else None
        matched = 0
        for f in fcfg["function"]:
            coff = objs[f["source"]]
            if coff and xcheck.check_function(coff, f["symbol"], x, f["va"], f["size"], symmap)[0] == "MATCH":
                matched += 1
        print("%-16s %2d / %d" % (label, matched, len(fcfg["function"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
