#!/usr/bin/env python3
"""Writes the sound lists that lie loose on the disc, not in any archive, for tests/oracle/oracle_aod.cpp. The text cases come from the archives only, and an .AOD in an archive names some of these.

  python -I tests/oracle/aod_loose_cases.py <disc image> analysis-local/scratch/aod/loose

Real game data, so a git-ignored directory only. index.txt has the columns of the text cases' index: file, source, path as the game asks for it, size.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import xdvdfs

PREFIX = "/DataXbox/"
EXTENSIONS = (".aod", ".aob", ".aos", ".aoe")


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    image, out = argv
    os.makedirs(out, exist_ok=True)
    count = 0
    with xdvdfs.Image(image) as im, open(os.path.join(out, "index.txt"), "w", encoding="utf-8", newline="\n") as index:
        for e in im.entries():
            if e.is_dir or not e.path.lower().endswith(EXTENSIONS) or not e.path.startswith(PREFIX):
                continue
            name = "%04d.bin" % count
            data = im.read_file(e)
            with open(os.path.join(out, name), "wb") as f:
                f.write(data)
            index.write("%s\t%s\t%s\t%d\n" % (name, "(loose)", e.path[len(PREFIX):].replace("/", "\\"), len(data)))
            count += 1
    print("%d files" % count)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
