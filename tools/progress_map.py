#!/usr/bin/env python3
"""Draw docs/progress-map.svg: the game's code as a grid of cells in address order, lit where source is recovered.

Each cell stands for a fixed run of `.text` bytes. A cell's colour is the subsystem family that owns most of its
recovered game bytes, and its strength is the recovered share of its game bytes; grey is game code not recovered;
dim cells are SDK, C runtime and middleware code, which is not counted. Inputs are measured data only:

  analysis-local/inv/functions.json   (tools/funcs.py)      candidate function ranges
  analysis-local/inv/ownership.json   (tools/ownership.py)  who owns each function (game, xdk, crt, cri, unknown)
  config/functions.toml, config/functions.d/                functions with recovered source

Run before every publish, so the picture on the project page matches the code it shows:

  python -I tools/progress_map.py
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fnconfig  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Subsystem families, by the record's subsystem string; the first matching prefix wins.
FAMILIES = [
    ("Files and archives", "#f2b134", ("at/jam", "at/mount", "at/memory", "at/heap", "gm/loader", "gm/manifest")),
    ("Text and game data", "#5cd17a", ("at/text", "at/gametext", "at/tok", "at/adr", "at/add", "gm/names", "gm/settings", "gm/levels")),
    ("Graphics and materials", "#3ec7e0", ("at/agn", "at/ags", "at/agt", "at/agd", "at/akc", "at/pixelformat", "at/material", "at/matdb", "at/loader")),
    ("Scenes, actors and collision", "#a77bf3", ("at/asn", "at/actor", "at/asm", "at/asb", "at/ase", "at/asd", "at/particle", "at/vector", "at/quaternion")),
    ("Sound", "#f06ba8", ("at/aod", "at/aob", "gm/audiodata", "gm/gon")),
    ("Input and interface", "#ff8a4c", ("at/controller", "at/ana", "at/afx", "at/afd", "gm/rumble")),
    ("Core containers", "#6ea8ff", ("gm/containers",)),
]
OTHER = ("Other recovered code", "#e6edf3")
BG, PANEL, TEXT, MUTED = "#0d1117", "#161b22", "#e6edf3", "#8b949e"
NOT_YET, LIBRARY = "#3a414b", "#232a33"
LIBRARY_CLASSES = {"xdk", "crt", "cri"}


def family(subsystem):
    for name, colour, prefixes in FAMILIES:
        if any(subsystem == p or subsystem.startswith(p + "/") for p in prefixes):
            return name, colour
    return OTHER


def load(rel):
    with open(os.path.join(ROOT, rel)) as f:
        return json.load(f)


def num(v):
    return int(v, 16) if isinstance(v, str) else int(v)


def spans(functions, owners, recorded, sizes={}):
    """Unique byte spans [lo, hi) with a label: ('done', family), ('left', None), ('library', None) or ('unknown', None).
    Recorded functions are laid first, at their recorded size (as verify.py checks it), since candidate ranges from the
    inventory can run into the next function; the rest fill only what is left, so no byte counts twice."""
    import bisect
    items = []
    for f in functions:
        va = num(f["va"])
        owner = owners.get(va, "unknown")
        if owner == "game":
            sub = recorded.get(va)
            label = ("done", family(sub)) if sub is not None else ("left", None)
        elif owner in LIBRARY_CLASSES:
            label = ("library", None)
        else:
            label = ("unknown", None)
        hi = va + sizes[va] if label[0] == "done" and va in sizes else num(f["max_end"])
        items.append((0 if label[0] == "done" else 1, va, hi, label))
    starts, ends, out = [], [], []  # claimed, disjoint, sorted by start
    for _, lo, hi, label in sorted(items):
        pieces, cur = [], lo
        i = max(bisect.bisect_right(starts, cur) - 1, 0)
        while cur < hi:
            while i < len(starts) and ends[i] <= cur:
                i += 1
            if i < len(starts) and starts[i] <= cur:
                cur = ends[i]
                continue
            nxt = starts[i] if i < len(starts) else hi
            end = min(hi, nxt)
            if end > cur:
                pieces.append((cur, end))
            cur = end
        for a, b in pieces:
            j = bisect.bisect_left(starts, a)
            starts.insert(j, a)
            ends.insert(j, b)
            out.append((a, b, label))
    return sorted(out)


def cells(span_list, base, size, cell):
    """For each cell of `cell` bytes from `base`, the bytes of each label it holds."""
    n = (size + cell - 1) // cell
    grid = [dict() for _ in range(n)]
    for lo, hi, label in span_list:
        lo, hi = max(lo, base), min(hi, base + size)
        while lo < hi:
            i = (lo - base) // cell
            end = min(hi, base + (i + 1) * cell)
            key = label if label[0] == "done" else (label[0],)
            grid[i][key] = grid[i].get(key, 0) + end - lo
            lo = end
    return grid


def cell_style(c):
    """Fill colour and opacity of one cell."""
    done = {k: v for k, v in c.items() if k[0] == "done"}
    done_bytes = sum(done.values())
    left = c.get(("left",), 0)
    if done_bytes:
        (_, (_, colour)), _ = max(done.items(), key=lambda kv: kv[1])
        share = done_bytes / (done_bytes + left)
        return colour, round(0.35 + 0.65 * share, 2)
    if left:
        return NOT_YET, 1
    if c.get(("library",), 0) >= c.get(("unknown",), 0) and c:
        return LIBRARY, 1
    return LIBRARY if c else BG, 1 if c else 0


def svg(grid, stats, cols=64, px=11, gap=2):
    rows = (len(grid) + cols - 1) // cols
    pad, head = 24, 92
    gw = cols * (px + gap) - gap
    legend_rows = (len(FAMILIES) + 1 + 2 + 1) // 2
    w = gw + 2 * pad
    h = head + rows * (px + gap) - gap + 28 + legend_rows * 22 + 40
    o = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" font-family="Segoe UI, Helvetica, Arial, sans-serif">' % (w, h, w, h),
         '<rect width="%d" height="%d" rx="12" fill="%s"/>' % (w, h, BG),
         '<text x="%d" y="38" font-size="22" font-weight="700" fill="%s">Decompiled: %.1f%% of the game\'s code</text>' % (pad, TEXT, stats["byte_pct"]),
         '<text x="%d" y="62" font-size="13" fill="%s">%s of about %s game functions (%.1f%%) · %s of about %s game code bytes (%.1f%%)</text>' % (
             pad, MUTED, f"{stats['fn_done']:,}", f"{stats['fn_total']:,}", stats["fn_pct"], f"{stats['b_done']:,}", f"{stats['b_total']:,}", stats["byte_pct"]),
         '<text x="%d" y="80" font-size="11" fill="%s">Each square is %s bytes of the executable\'s code, in address order. Lit: source recovered. Grey: not yet.</text>' % (pad, MUTED, f"{stats['cell']:,}")]
    for i, c in enumerate(grid):
        colour, alpha = cell_style(c)
        if not alpha:
            continue
        x, y = pad + (i % cols) * (px + gap), head + (i // cols) * (px + gap)
        o.append('<rect x="%d" y="%d" width="%d" height="%d" rx="2" fill="%s"%s/>' % (x, y, px, px, colour, '' if alpha == 1 else ' fill-opacity="%s"' % alpha))
    y0 = head + rows * (px + gap) + 22
    items = [(n, c) for n, c, _ in FAMILIES] + [OTHER, ("Not decompiled yet", NOT_YET), ("SDK, C runtime and middleware (not counted)", LIBRARY)]
    for i, (name, colour) in enumerate(items):
        x, y = pad + (i % 2) * (gw // 2), y0 + (i // 2) * 22
        o.append('<rect x="%d" y="%d" width="12" height="12" rx="2" fill="%s"%s/>' % (x, y, colour, ' stroke="#30363d"' if colour == LIBRARY else ''))
        o.append('<text x="%d" y="%d" font-size="12" fill="%s">%s</text>' % (x + 18, y + 11, TEXT, name))
    o.append('<text x="%d" y="%d" font-size="11" fill="%s">Estimates: the owner of most code is inferred. Recovered code is behaviour-tested against the original; no byte matches with the original compiler yet.</text>' % (pad, h - 16, MUTED))
    o.append("</svg>")
    return "\n".join(o) + "\n"


def main():
    functions = [f for f in load("analysis-local/inv/functions.json") if f.get("kind") == "source"]
    owners = {num(k): v[0] for k, v in load("analysis-local/inv/ownership.json")["functions"].items()}
    cov = load("analysis-local/inv/coverage.json")[".text"]
    records = fnconfig.load(ROOT)["function"]
    recorded = {num(f["va"]): f.get("subsystem", "") for f in records}
    sp = spans(functions, owners, recorded, {num(f["va"]): int(f["size"]) for f in records})
    text = [f for f in functions if cov["va"] <= num(f["va"]) < cov["va"] + cov["raw_size"]] if "va" in cov else functions
    base = min(num(f["va"]) for f in text)
    size = max(num(f["max_end"]) for f in text) - base
    cell = 512
    while (size + cell - 1) // cell > 64 * 32:
        cell *= 2
    grid = cells(sp, base, size, cell)
    game = {k for k, v in owners.items() if v == "game"}
    b_done = sum(hi - lo for lo, hi, l in sp if l[0] == "done")
    b_total = sum(hi - lo for lo, hi, l in sp if l[0] in ("done", "left"))
    fn_done = len(game & set(recorded))
    stats = dict(fn_done=fn_done, fn_total=len(game), fn_pct=100.0 * fn_done / len(game),
                 b_done=b_done, b_total=b_total, byte_pct=100.0 * b_done / b_total, cell=cell)
    with open(os.path.join(ROOT, "docs", "progress-map.svg"), "w", encoding="utf-8", newline="\n") as f:
        f.write(svg(grid, stats))
    print("wrote docs/progress-map.svg: %d cells of %d bytes; game functions %d / %d (%.2f%%), game bytes %d / %d (%.2f%%)" % (
        len(grid), cell, fn_done, len(game), stats["fn_pct"], b_done, b_total, stats["byte_pct"]))


if __name__ == "__main__":
    main()
