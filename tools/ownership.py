#!/usr/bin/env python3
"""Which code in the executable is the game's and which is a library's, with the evidence for each.

  python -I tools/ownership.py        writes analysis-local/inv/ownership.json and prints the totals

Direct evidence, per function:
  section     a section named for an XDK library (D3D, DSOUND, ...)
  recovered   recorded in config/functions.toml as game code (gm/, at/); library symbols there count as library evidence
  rtti        in a vftable of a class with the game's name prefixes
  string      refers to a string tagged as the game's, the C library's or CRI's (analysis-local/inv/string_tags.json), or that matches GAME_STRINGS below
  hint        named by the Ghidra plugin's signature database. A hint is not proof, and is kept apart in the output

Then one inference: a function with no evidence lying between two functions with the same class, by address, takes that class at low confidence. The linker lays out each
library in one run, so this is a reasonable guess, and a wrong one if a run holds something else. Anything else stays unknown, and so does anything with conflicting evidence.
A denominator should use "direct + inferred" for an estimate and always show unknown beside it.
"""
import json
import os
import re
import sys

try:
    import tomllib
except ImportError:  # Python older than 3.11.
    import tomli as tomllib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fnconfig  # noqa: E402
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SECTION_CLASS = {"D3D": "xdk", "D3DX": "xdk", "XGRPH": "xdk", "DSOUND": "xdk", "XACTENG": "xdk", "WMADEC": "xdk", "XPP": "xdk", "DOLBY": "xdk"}
GAME_CLASS = re.compile(r"^(At|Gm|Fa|App)")
LIBRARY_SYMBOL = {"_strncmp", "_strrchr", "_strchr", "_toupper", "_strncpy", "_strncat", "__purecall"}
HINT_CLASS = {"XAPILIB": "xdk", "XGRAPHC": "xdk", "D3D8": "xdk", "DSOUND": "xdk", "XACTENG": "xdk"}
STRING_CLASS = {"game": "game", "crt": "crt", "cri": "cri"}
ORDER = ["game", "crt", "xdk", "cri", "unknown"]
# Strings no library would hold: this game's script callbacks and data paths, and its device names. Added after reading what the unknown runs refer to.
GAME_STRINGS = [re.compile(p) for p in (r"^[A-Z][A-Za-z]+Callback$", r"^Design\\", r"^Art\\", r"^DataXbox\\", r"^UI\\", r"^Audio\\",
                                        r"^XBox GamePad", r"^Joystick Button", r"^(In|Out)erDist", r"^MaxNumberOfDynamicPathNodes$", r"ShadowBone$", r"^Mod[A-Z][a-z]+$")]


def tag_for_string(text):
    return "game" if any(p.search(text) for p in GAME_STRINGS) else None


def direct_evidence(f, strings, recovered, library, hints):
    """Every (class, kind) this function has direct evidence for."""
    out = []
    va = int(f["va"], 16)
    section = f["section"]
    if section in SECTION_CLASS or section.startswith("PSFD"):
        out.append(("xdk", "section"))
    if va in recovered:
        out.append(("game", "recovered"))
    if va in library:
        out.append(("crt", "recovered"))
    for slot in f.get("vtable_slots", []):
        if GAME_CLASS.match(slot):
            out.append(("game", "rtti"))
            break
    tag = strings.get(f["va"])
    if tag in STRING_CLASS:
        out.append((STRING_CLASS[tag], "string"))
    if va in hints:
        out.append((hints[va], "hint"))
    return out


def classify(functions, strings, recovered, library, hints):
    """Returns {va: (class, confidence, kinds)}. `functions` need va, section, kind (only "source" ones are classified) and may have vtable_slots."""
    source = sorted((f for f in functions if f.get("kind") == "source"), key=lambda f: int(f["va"], 16))
    result = {}
    for f in source:
        evidence = direct_evidence(f, strings, recovered, library, hints)
        strong = {c for c, k in evidence if k != "hint"}
        classes = strong or {c for c, _ in evidence}
        kinds = sorted({k for _, k in evidence})
        if len(classes) == 1:
            result[f["va"]] = (next(iter(classes)), "direct" if strong else "hint", kinds)
        elif classes:
            result[f["va"]] = ("unknown", "conflict", kinds)
        else:
            result[f["va"]] = ("unknown", "none", [])
    # Fill runs between agreeing neighbours, within a section.
    last = None
    pending = []
    last_section = None
    for f in source:
        va = f["va"]
        cls, conf, _ = result[va]
        if f["section"] != last_section:
            last, pending, last_section = None, [], f["section"]
        if conf == "conflict":
            last, pending = None, []
        elif cls == "unknown":
            pending.append(va)
        else:
            if last == cls:
                for p in pending:
                    result[p] = (cls, "inferred", [])
            last, pending = cls, []
    return result


def unique_bytes(ranges):
    total, end = 0, 0
    for a, b in sorted(ranges):
        if b > end:
            total += b - max(a, end)
            end = b
    return total


def summarise(functions, result):
    by = {f["va"]: f for f in functions}
    table = {}
    for va, (cls, conf, _) in result.items():
        conf = "direct" if conf == "direct" else ("hint" if conf == "hint" else ("inferred" if conf == "inferred" else "none"))
        row = table.setdefault(cls, {"direct": 0, "hint": 0, "inferred": 0, "none": 0, "ranges": []})
        row[conf] += 1
        f = by[va]
        row["ranges"].append((int(f["va"], 16), int(f["max_end"], 16)))
    out = {}
    for cls in ORDER:
        row = table.get(cls)
        if row:
            out[cls] = {k: row[k] for k in ("direct", "hint", "inferred", "none")}
            out[cls]["functions"] = sum(row[k] for k in ("direct", "hint", "inferred", "none"))
            out[cls]["unique_bytes"] = unique_bytes(row["ranges"])
    return out


def load_hints():
    hints = {}
    path = os.path.join(ROOT, "analysis-local", "ghidra", "export", "symbols.tsv")
    if not os.path.exists(path):
        return hints
    with open(path, encoding="utf-8", errors="replace") as f:
        next(f)
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if len(parts) < 4 or "::" not in parts[3] or parts[2] != "Function":
                continue
            lib = parts[3].split("::", 1)[0]
            if lib in HINT_CLASS:
                try:
                    hints[int(parts[0], 16)] = HINT_CLASS[lib]
                except ValueError:
                    pass
    return hints


def add_string_rules(functions, strings):
    """Scans each untagged function for pointers to strings GAME_STRINGS recognises."""
    sys.path.insert(0, os.path.join(ROOT, "tools"))
    import struct

    import xbe as xbemod
    x = xbemod.load(os.path.join(ROOT, "orig", "xiso", "default.xbe"))
    rdata = [s for s in x.sections if s.name == ".rdata"][0]
    blob = x.read_va(rdata.va, rdata.rsize)
    text = [s for s in x.sections if s.name == ".text"][0]
    code = x.read_va(text.va, text.rsize)
    cache = {}
    for f in functions:
        if f.get("kind") != "source" or f["section"] != ".text" or f["va"] in strings:
            continue
        a, b = int(f["va"], 16), int(f["max_end"], 16)
        for o in range(a - text.va, min(b, text.va + text.rsize - 4) - text.va):
            v = struct.unpack_from("<I", code, o)[0]
            if not rdata.va <= v < rdata.va + rdata.rsize:
                continue
            if v not in cache:
                raw = blob[v - rdata.va:v - rdata.va + 80].split(b"\0", 1)[0]
                cache[v] = tag_for_string(raw.decode("latin-1")) if len(raw) >= 4 else None
            if cache[v]:
                strings[f["va"]] = cache[v]
                break


def main():
    with open(os.path.join(ROOT, "analysis-local", "inv", "functions.json")) as f:
        functions = json.load(f)
    with open(os.path.join(ROOT, "analysis-local", "inv", "string_tags.json")) as f:
        strings = {k: (v if isinstance(v, str) else v.get("tag")) for k, v in json.load(f).items()}
    cfg = fnconfig.load(ROOT)
    recovered = {r["va"] for r in cfg["function"]}
    library = {v for k, v in cfg.get("symbols", {}).items() if k in LIBRARY_SYMBOL}
    hints = load_hints()
    add_string_rules(functions, strings)
    result = classify(functions, strings, recovered, library, hints)
    summary = summarise(functions, result)
    path = os.path.join(ROOT, "analysis-local", "inv", "ownership.json")
    with open(path, "w") as f:
        json.dump({"summary": summary, "functions": {k: list(v) for k, v in result.items()}}, f, indent=0)
    for cls, row in summary.items():
        print("%-8s %6d functions, %8d bytes  (direct %d, hint %d, inferred %d, none %d)" % (cls, row["functions"], row["unique_bytes"], row["direct"], row["hint"], row["inferred"], row["none"]))
    conflicts = [k for k, v in result.items() if v[1] == "conflict"]
    print("conflicts: %d %s" % (len(conflicts), conflicts[:12]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
