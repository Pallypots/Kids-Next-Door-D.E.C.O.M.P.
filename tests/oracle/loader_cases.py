#!/usr/bin/env python3
"""Writes each distinct .XKA and .XGG of the disc once, and the one loose .AGD the game names, for tests/oracle/oracle_loader.cpp. Also counts every extension the database loaders can reach, in archives and loose.

  python -I tests/oracle/loader_cases.py <disc image> analysis-local/scratch/loader/bin

Real game data, so a git-ignored directory only. index.txt has the columns of the text cases' index: file, source, path as the game asks for it, size; a row for every place a file lies, the file written once for each content.
"""
import collections
import hashlib
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import jam as jamtool

KEEP = (".XKA", ".XGG", ".XFF")
COUNT = (".AGG", ".AKA", ".AGV", ".AGP", ".AKM", ".AKW", ".AGO", ".AGH", ".AFF", ".XGG", ".XKA", ".XFF", ".AGD")


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    image, out = argv
    os.makedirs(out, exist_ok=True)
    img, entries = jamtool._jams_on_image(image)
    seen = set()
    counts = collections.Counter()
    distinct = collections.Counter()
    total = 0
    with open(os.path.join(out, "index.txt"), "w", encoding="utf-8", newline="\n") as index:
        for e in img.entries():
            if e.is_dir:
                continue
            ext = os.path.splitext(e.path)[1].upper()
            if ext in COUNT:
                counts["loose " + ext] += 1
            if e.path.upper().endswith("LOADER.AGD") and "DIRTYDSK" in e.path.upper():
                data = img.read_file(e)
                with open(os.path.join(out, "dirtydsk_loader.agd"), "wb") as f:
                    f.write(data)
                index.write("%s\t%s\t%s\t%d\n" % ("dirtydsk_loader.agd", "(loose)", e.path, len(data)))
        for e in entries:
            j = jamtool._load(img, e)
            base = img.sector_offset(e.sector)
            record = jamtool.JAM2_RECORD if j.token == b"JAM2" else 0
            for path, off, size in j.files:
                ext = os.path.splitext(path)[1].upper()
                if ext in COUNT:
                    counts["archive " + ext] += 1
                if ext not in KEEP:
                    continue
                data = img._read(base + off + record, size)
                digest = hashlib.sha256(data).hexdigest()[:16]
                name = digest + ext.lower()
                if digest not in seen:
                    seen.add(digest)
                    distinct[ext] += 1
                    with open(os.path.join(out, name), "wb") as f:
                        f.write(data)
                    total += size
                # Every place a file is, so that an .AGD finds what it names: one content may lie under several paths.
                index.write("%s\t%s\t%s\t%d\n" % (name, e.path, path, size))
    for k in sorted(counts):
        print(k, counts[k])
    print("distinct kept:", dict(distinct), total, "bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
