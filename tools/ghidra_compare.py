#!/usr/bin/env python3
"""Cross-check tools/xbe.py and tools/funcs.py against a Ghidra export (tools/ghidra/ExportInventory.java).

  python -I tools/ghidra_compare.py [--export analysis-local/ghidra/export] [--inv analysis-local/inv]

Two independent tools agreeing is corroboration, not proof. A disagreement is a question, not a verdict: neither tool
is the reference. Detail goes to analysis-local/ghidra/compare.json (local, derived from the game); stdout has aggregates.
Exit 1 if the section map or entry point disagree; function boundary disagreements are expected and are reported.
"""
import argparse
import bisect
import json
import os
import sys
import tomllib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fnconfig  # noqa: E402
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def tsv(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        head = f.readline().rstrip("\n").split("\t")
        return [dict(zip(head, line.rstrip("\n").split("\t"))) for line in f]


def hx(s):
    return int(s.split(":")[-1], 16)


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--export", default=os.path.join(ROOT, "analysis-local", "ghidra", "export"))
    ap.add_argument("--inv", default=os.path.join(ROOT, "analysis-local", "inv"))
    a = ap.parse_args(argv)
    with open(os.path.join(ROOT, "analysis-local", "xbe_inventory.json")) as f:
        xi = json.load(f)
    with open(os.path.join(a.inv, "functions.json")) as f:
        ours = {int(x["va"], 16): x for x in json.load(f)}
    blocks = tsv(os.path.join(a.export, "blocks.tsv"))
    gf = {hx(r["entry"]): r for r in tsv(os.path.join(a.export, "functions.tsv")) if ":" not in r["entry"]}
    entrypoints = tsv(os.path.join(a.export, "entrypoints.tsv"))
    detail = {}
    fatal = False

    # ---- sections: every XBE section must exist in Ghidra at the same address with the same mapped size
    gb = {b["name"]: b for b in blocks}
    bad = []
    for s in xi["sections"]:
        b = gb.get(s["name"])
        if b is None:
            bad.append("%s missing in Ghidra" % s["name"])
        elif hx(b["start"]) != int(s["va"], 16):
            bad.append("%s at %s, xbe.py 0x%X" % (s["name"], b["start"], int(s["va"], 16)))
        elif int(b["size"]) != s["vsize"]:
            bad.append("%s size %s, xbe.py %d" % (s["name"], b["size"], s["vsize"]))
        elif ("EXECUTABLE" in s["flags"]) != (b["execute"] == "true"):
            bad.append("%s execute flag differs" % s["name"])
    print("sections: %d in xbe.py, %d blocks in Ghidra, disagreements: %d" % (len(xi["sections"]), len(blocks), len(bad)))
    for line in bad[:20]:
        print("   ", line)
    extra = sorted(set(gb) - {s["name"] for s in xi["sections"]})
    print("    blocks only in Ghidra:", extra)
    detail["section_disagreements"] = bad
    fatal |= bool(bad)

    # ---- entry point
    want = int(xi["entry_point"], 16)
    got = [hx(e["address"]) for e in entrypoints if ":" not in e["address"]]
    named = [hx(k) for k, r in ((v["entry"], v) for v in gf.values()) if r["name"] == "entry"]
    ok = want in got or want in named
    print("entry point: xbe.py 0x%08X, Ghidra entry points %s, function named entry %s: %s" % (
        want, ["0x%08X" % v for v in got[:6]], ["0x%08X" % v for v in named[:3]], "agree" if ok else "DISAGREE"))
    fatal |= not ok

    # ---- function starts
    both = sorted(set(ours) & set(gf))
    only_ours = sorted(set(ours) - set(gf))
    only_ghidra = sorted(set(gf) - set(ours))
    text = next(s for s in xi["sections"] if s["name"] == ".text")
    tlo, thi = int(text["va"], 16), int(text["va"], 16) + text["vsize"]
    print("function starts: ours %d, Ghidra %d, both %d, only ours %d, only Ghidra %d" % (
        len(ours), len(gf), len(both), len(only_ours), len(only_ghidra)))
    for conf in ("high", "medium", "low"):
        n = sum(1 for v in ours.values() if v["confidence"] == conf)
        m = sum(1 for v in only_ours if ours[v]["confidence"] == conf)
        print("    our %-6s %6d: confirmed by Ghidra %6d (%.1f%%), not a Ghidra function start %6d" % (
            conf, n, n - m, 100.0 * (n - m) / max(n, 1), m))
    for kind in ("source", "eh_unwind", "eh_thunk", "eh_catch"):
        n = sum(1 for v in ours.values() if v.get("kind") == kind)
        m = sum(1 for v in only_ours if ours[v].get("kind") == kind)
        print("    our kind %-9s %6d: also a Ghidra function start %6d, not %6d" % (kind, n, n - m, m))
    in_text = sum(1 for v in only_ghidra if tlo <= v < thi)
    print("    only-Ghidra starts in .text: %d; in other sections: %d" % (in_text, len(only_ghidra) - in_text))
    detail["only_ours"] = ["0x%08X" % v for v in only_ours]
    detail["only_ghidra"] = ["0x%08X" % v for v in only_ghidra]

    # ---- bodies of functions both tools know, measured as contiguous extent
    same = differ = 0
    samples = []
    for v in both:
        o = ours[v]
        g = gf[v]
        oend = int(o["max_end"], 16)
        gend = hx(g["max"]) + 1
        if oend == gend:
            same += 1
        else:
            differ += 1
            samples.append(("0x%08X" % v, "ours end 0x%08X" % oend, "Ghidra end 0x%08X" % gend, "ranges " + g["ranges"]))
    print("extents of shared starts: same end %d, different end %d" % (same, differ))
    detail["extent_differences"] = samples

    # ---- recorded (recovered) functions: the claims that matter most
    cfg = fnconfig.load(ROOT)
    rec_bad = []
    for r in cfg["function"]:
        g = gf.get(r["va"])
        if g is None:
            rec_bad.append((r["symbol"], "0x%08X" % r["va"], "not a function start in Ghidra"))
            continue
        if int(g["bytes"]) != r["size"] or int(g["ranges"]) != 1:
            rec_bad.append((r["symbol"], "0x%08X" % r["va"], "recorded size %d; Ghidra %s bytes in %s range(s), %s" % (
                r["size"], g["bytes"], g["ranges"], g["name"])))
    print("recorded functions: %d, boundary disagreements with Ghidra: %d" % (len(cfg["function"]), len(rec_bad)))
    for line in rec_bad:
        print("   ", *line)
    detail["recorded_disagreements"] = rec_bad

    # ---- why starts disagree
    ranges = sorted((hx(r["start"]), hx(r["end"])) for r in tsv(os.path.join(a.export, "code_ranges.tsv"))
                    if ":" not in r["start"])
    starts = [lo for lo, _ in ranges]
    gbodies = sorted((hx(r["min"]), hx(r["max"]), v) for v, r in gf.items())
    gmins = [b[0] for b in gbodies]

    def is_code(va):
        i = bisect.bisect_right(starts, va) - 1
        return i >= 0 and ranges[i][0] <= va <= ranges[i][1]

    def in_ghidra_body(va):
        i = bisect.bisect_right(gmins, va) - 1
        # Bodies can overlap in extent; look back a little instead of assuming they are disjoint.
        for b in gbodies[max(0, i - 8):i + 1]:
            if b[0] <= va <= b[1] and b[2] != va:
                return b[2]
        return None

    classes = {}
    examples = {}
    for v in only_ours:
        kind = "inside a Ghidra function" if in_ghidra_body(v) else "code, no function" if is_code(v) else "not disassembled by Ghidra"
        key = (ours[v]["kind"], ours[v]["confidence"], "+".join(sorted(set(ours[v]["evidence"]))), kind)
        classes[key] = classes.get(key, 0) + 1
        examples.setdefault(key, "0x%08X" % v)
    print("only-ours starts by confidence, our evidence and what Ghidra has there:")
    for key, n in sorted(classes.items(), key=lambda kv: -kv[1])[:24]:
        print("    %5d  %-9s %-6s %-24s %-28s e.g. %s" % (n, key[0], key[1], key[2][:24], key[3], examples[key]))
    detail["only_ours_classes"] = {" | ".join(k): n for k, n in classes.items()}

    osorted = sorted((v, int(o["max_end"], 16)) for v, o in ours.items())
    omins = [x[0] for x in osorted]
    def in_our_body(va):
        i = bisect.bisect_right(omins, va) - 1
        return any(b[0] < va < b[1] for b in osorted[max(0, i - 8):i + 1])
    secs = [(int(x["va"], 16), int(x["va"], 16) + x["vsize"], x["name"]) for x in xi["sections"]]
    gclasses = {}
    for v in only_ghidra:
        sec = next((n for lo, hi, n in secs if lo <= v < hi), "?")
        key = (sec, "inside a function of ours" if in_our_body(v) else "outside our functions")
        gclasses[key] = gclasses.get(key, 0) + 1
    print("only-Ghidra starts by section:")
    for key, n in sorted(gclasses.items(), key=lambda kv: -kv[1])[:16]:
        print("    %5d  %-10s %s" % (n, key[0], key[1]))

    # ---- recorded functions that Ghidra did not make functions: is the code at least there, and how is it reached?
    print("recorded functions without a Ghidra function: code state")
    for r in cfg["function"]:
        if r["va"] not in gf:
            print("    0x%08X %-46s %s; ours: %s" % (r["va"], r["symbol"][:46], "disassembled" if is_code(r["va"]) else "not disassembled",
                                                     ",".join(ours[r["va"]]["evidence"]) if r["va"] in ours else "not in inventory"))

    # ---- kernel imports
    symbols = tsv(os.path.join(a.export, "symbols.tsv"))
    by_addr = {}
    for r in symbols:
        if ":" not in r["address"]:
            by_addr.setdefault(hx(r["address"]), []).append(r)
    named = sum(1 for k in xi["kernel_imports"] if int(k["slot_va"], 16) in by_addr)
    print("kernel import slots: xbe.py %d, with a Ghidra symbol at the same slot %d" % (len(xi["kernel_imports"]), named))
    print("    sample:", [(k["ordinal"], by_addr[int(k["slot_va"], 16)][0]["name"]) for k in xi["kernel_imports"][:4]
                          if int(k["slot_va"], 16) in by_addr])

    # ---- database name hints, grouped; these are hints, never recovered names
    groups = {}
    for r in gf.values():
        if r["source"] == "ANALYSIS" and "::" in r["name"]:
            g = r["name"].split("::")[0]
            v = hx(r["entry"])
            lo, hi, n = groups.get(g, (v, v, 0))
            groups[g] = (min(lo, v), max(hi, v), n + 1)
    print("name hints by library group (Ghidra ANALYSIS names; extent is first to last hit, not a verified range):")
    for g, (lo, hi, n) in sorted(groups.items(), key=lambda kv: kv[1][0]):
        print("    %-14s %4d  0x%08X-0x%08X" % (g, n, lo, hi))
    detail["hint_groups"] = {g: ["0x%08X" % lo, "0x%08X" % hi, n] for g, (lo, hi, n) in groups.items()}
    print("hints at addresses inferred independently here:")
    for v, what in ((0x125F25, "__purecall"), (0x1256A3, "free?"), (0x125691, "atexit?"), (0x125D44, "vector ctor?"),
                    (0x12526B, "vector dtor?"), (0x125559, "sprintf?"), (0x125C20, "memcpy?"), (0x1267D0, "strncmp?"),
                    (0x1261EA, "toupper?"), (0x125E10, "strchr?"), (0x85375, "XAPI entry")):
        names = [r["name"] for r in by_addr.get(v, [])]
        print("    0x%08X %-14s %s" % (v, what, names or "no hint"))

    # ---- names, by source: anything not USER/DEFAULT comes from an analyzer and is a hint only
    sources = {}
    for r in gf.values():
        sources[r["source"]] = sources.get(r["source"], 0) + 1
    print("Ghidra function name sources:", sources)
    with open(os.path.join(ROOT, "analysis-local", "ghidra", "compare.json"), "w") as f:
        json.dump(detail, f, indent=1)
    return 1 if fatal else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
