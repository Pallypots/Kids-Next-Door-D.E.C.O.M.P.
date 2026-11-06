#!/usr/bin/env python3
"""Writes inputs for tests/native/test_gm_levels.cpp (git-ignored directory only: real game data).

  python -I tools/levels.py <image> <out dir>

out dir receives JamFiles/XBox/AppInit.JAM, where the mount manager looks for it, and levels.tsv: name, kind, load items, description, one line per entry. The list comes from this file's own small
parse of Levels.txt, written from the text alone, so a test against it owes nothing to the recovered reader.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import jam  # noqa: E402

KINDS = {"NORMALLEVEL": 0, "HIDDENLEVEL": 1, "NONSELECTABLELEVEL": 2, "FMVCINEMA": 3}
TOKEN = re.compile(r'//[^\n]*|"([^"]*)"|([A-Za-z][A-Za-z0-9_]*)|(-?\d+)|([\[\]{}])|\s+')


def tokens(text):
    pos = 0
    while pos < len(text):
        m = TOKEN.match(text, pos)
        if not m:
            raise ValueError("unexpected text at %d: %r" % (pos, text[pos:pos + 20]))
        pos = m.end()
        if m.group(1) is not None:
            yield "str", m.group(1)
        elif m.group(2):
            yield "word", m.group(2).upper()
        elif m.group(3):
            yield "int", int(m.group(3))
        elif m.group(4):
            yield m.group(4), m.group(4)


def parse(text):
    toks = list(tokens(text))
    i = 0
    if toks[0] != ("word", "LEVELS") or toks[1][0] != "[" or toks[2][0] != "int" or toks[3][0] != "]" or toks[4][0] != "{":
        raise ValueError("unexpected start")
    count = toks[2][1]
    i = 5
    out = []
    for _ in range(count):
        if toks[i] != ("word", "LEVEL") or toks[i + 1][0] != "str":
            raise ValueError("expected a level at token %d" % i)
        name = toks[i + 1][1]
        kind = KINDS[toks[i + 2][1]]
        i += 3
        items, description = "", ""
        if toks[i][0] == "{":
            i += 1
            while toks[i][0] != "}":
                if toks[i] == ("word", "NUMLOADITEMS"):
                    items = toks[i + 1][1]
                elif toks[i] == ("word", "DESCRIPTION"):
                    description = toks[i + 1][1]
                else:
                    raise ValueError("unknown field %r" % (toks[i],))
                i += 2
            i += 1
        out.append((name, kind, items, description))
    if toks[i][0] != "}" or i + 1 != len(toks):
        raise ValueError("unexpected end")
    return out


RUMBLE_EFFECT = re.compile(r'RumbleEffect\s+"([^"]*)"[^{]*\{\s*KeyFrames\s*\[\s*(\d+)\s*\]', re.I)
RUMBLE_FRAME = re.compile(r'KeyFrame\s+([\d.]+)\s*\{\s*Left\s+([\d.]+)\s+Right\s+([\d.]+)\s*\}', re.I)


def parse_rumble(text):
    """E, name, frame count; then F, time, left, right for each frame. From the text alone."""
    text = re.sub(r"//[^\n]*", "", text)
    out = []
    marks = list(RUMBLE_EFFECT.finditer(text))
    for i, m in enumerate(marks):
        end = marks[i + 1].start() if i + 1 < len(marks) else len(text)
        frames = RUMBLE_FRAME.findall(text, m.end(), end)
        if len(frames) != int(m.group(2)):
            raise ValueError("effect %s: %d frames, %s declared" % (m.group(1), len(frames), m.group(2)))
        out.append(("E", m.group(1), m.group(2)))
        out.extend(("F",) + f for f in frames)
    return out


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    image, out_dir = argv
    img, entries = jam._jams_on_image(image)
    entry = [e for e in entries if e.path.upper().endswith("/APPINIT.JAM")][0]
    archive = jam._load(img, entry)
    base = img.sector_offset(entry.sector)
    path, off, size = [f for f in archive.files if f[0].upper() == "LEVELS.TXT"][0]
    levels = parse(img._read(base + off, size).decode("latin-1"))
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "levels.tsv"), "w", encoding="latin-1", newline="\n") as f:
        for row in levels:
            f.write("\t".join(str(v) for v in row) + "\n")
    path, off, size = [f for f in archive.files if f[0].upper().endswith("RUMBLE.RMB")][0]
    with open(os.path.join(out_dir, "rumble.tsv"), "w", encoding="latin-1", newline="\n") as f:
        for row in parse_rumble(img._read(base + off, size).decode("latin-1")):
            f.write("\t".join(str(v) for v in row) + "\n")
    ini = [e for e in img.entries() if not e.is_dir and e.path.upper() == "/APP.INI"][0]
    with open(os.path.join(out_dir, "app.ini"), "wb") as f:
        f.write(img._read(img.sector_offset(ini.sector), ini.size))
    os.makedirs(os.path.join(out_dir, "JamFiles", "XBox"), exist_ok=True)
    target = os.path.join(out_dir, "JamFiles", "XBox", "AppInit.JAM")
    old = os.path.join(out_dir, "AppInit.JAM")
    if os.path.exists(old) and not os.path.exists(target):
        os.replace(old, target)
    if not os.path.exists(target) or os.path.getsize(target) != entry.size:
        with open(target, "wb") as f:
            for start in range(0, entry.size, 1 << 24):
                f.write(img._read(base + start, min(1 << 24, entry.size - start)))
    counts = {}
    for row in levels:
        counts[row[1]] = counts.get(row[1], 0) + 1
    print("%d entries; by kind %s; %s" % (len(levels), sorted(counts.items()), target))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
