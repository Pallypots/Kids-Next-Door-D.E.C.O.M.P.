#!/usr/bin/env python3
"""Read-only XDVDFS (Xbox DVD filesystem) reader.

Accepts a raw XISO, a full Redump-style disc image (game partition at 0x18300000) or any image where the
partition base is supplied explicitly. Never writes to the input. All reads are bounds-checked; malformed
structures raise XdvdfsError instead of producing partial results silently.

Format reference: https://xboxdevwiki.net/XDVDFS (verified against the owner's images, see docs/provenance.md).

  python -I tools/xdvdfs.py ls <image>                 list every file (path, sector, size)
  python -I tools/xdvdfs.py extract <image> <path> <out>  extract one file
  python -I tools/xdvdfs.py info <image>               volume descriptor
"""
import os
import struct
import sys

SECTOR = 2048
MAGIC = b"MICROSOFT*XBOX*MEDIA"
VD_SECTOR = 32
KNOWN_BASES = (0x0, 0x18300000, 0x0FD90000, 0x02080000)  # XISO, Redump XGD2, XGD3, XGD1 variants
ATTR_DIR = 0x10


class XdvdfsError(Exception):
    pass


class Entry:
    __slots__ = ("path", "sector", "size", "attrs")

    def __init__(self, path, sector, size, attrs):
        self.path, self.sector, self.size, self.attrs = path, sector, size, attrs

    @property
    def is_dir(self):
        return bool(self.attrs & ATTR_DIR)


class Image:
    def __init__(self, path, base=None):
        self.path = path
        self.f = open(path, "rb")
        self.length = os.fstat(self.f.fileno()).st_size
        try:
            self._open(base)
        except Exception:
            self.f.close()
            raise

    def _open(self, base):
        if base is None:
            base = self._detect_base()
        self.base = base
        vd = self._read(base + VD_SECTOR * SECTOR, SECTOR)
        if vd[:20] != MAGIC or vd[0x7EC:0x800] != MAGIC:
            raise XdvdfsError("volume descriptor magic missing at base 0x%X" % base)
        self.root_sector, self.root_size, self.filetime = struct.unpack_from("<IIQ", vd, 20)
        if self.root_size == 0 or self.root_size % 4:
            raise XdvdfsError("implausible root directory size %d" % self.root_size)

    def close(self):
        self.f.close()

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.close()

    def _read(self, off, n):
        if off < 0 or n < 0 or off + n > self.length:
            raise XdvdfsError("read 0x%X+0x%X outside image (0x%X bytes)" % (off, n, self.length))
        self.f.seek(off)
        d = self.f.read(n)
        if len(d) != n:
            raise XdvdfsError("short read at 0x%X" % off)
        return d

    def _detect_base(self):
        for b in KNOWN_BASES:
            off = b + VD_SECTOR * SECTOR
            if off + 20 <= self.length:
                self.f.seek(off)
                if self.f.read(20) == MAGIC:
                    return b
        raise XdvdfsError("no XDVDFS volume descriptor at any known partition base")

    def sector_offset(self, sector):
        return self.base + sector * SECTOR

    def _walk_dir(self, sector, size, prefix, out, depth=0):
        if depth > 64:
            raise XdvdfsError("directory nesting too deep (cycle?)")
        table = self._read(self.sector_offset(sector), size)
        seen = set()
        stack = [0]
        while stack:
            off = stack.pop()
            if off in seen:
                raise XdvdfsError("directory tree cycle at %s+0x%X" % (prefix or "/", off))
            seen.add(off)
            if off + 14 > len(table):
                raise XdvdfsError("dirent at 0x%X beyond table (%d bytes)" % (off, len(table)))
            left, right, sec, fsize, attrs, nlen = struct.unpack_from("<HHIIBB", table, off)
            if left == 0xFFFF and right == 0xFFFF:  # padding inside an empty directory
                continue
            if off + 14 + nlen > len(table):
                raise XdvdfsError("dirent name overruns table")
            name = table[off + 14:off + 14 + nlen].decode("latin-1")
            if not name or "/" in name or "\\" in name or name in (".", ".."):
                raise XdvdfsError("bad entry name %r" % name)
            path = prefix + "/" + name
            out.append(Entry(path, sec, fsize, attrs))
            if attrs & ATTR_DIR and fsize:
                self._walk_dir(sec, fsize, path, out, depth + 1)
            for child in (left, right):
                if child not in (0, 0xFFFF):
                    stack.append(child * 4)
        return out

    def entries(self):
        return sorted(self._walk_dir(self.root_sector, self.root_size, "", []), key=lambda e: e.path.lower())

    def find(self, path):
        want = "/" + path.strip("/").replace("\\", "/").lower()
        for e in self.entries():
            if e.path.lower() == want:
                return e
        raise XdvdfsError("not found: %s" % path)

    def read_file(self, entry):
        if entry.is_dir:
            raise XdvdfsError("%s is a directory" % entry.path)
        return self._read(self.sector_offset(entry.sector), entry.size)

    def extract_to(self, entry, out_path, chunk=1 << 22):
        if entry.is_dir:
            raise XdvdfsError("%s is a directory" % entry.path)
        start = self.sector_offset(entry.sector)
        self._read(start, 0) if entry.size == 0 else self._read(start + entry.size - 1, 1)  # bounds check
        os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
        with open(out_path, "wb") as o:
            pos = 0
            while pos < entry.size:
                n = min(chunk, entry.size - pos)
                o.write(self._read(start + pos, n))
                pos += n


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    cmd, img = argv[0], argv[1]
    with Image(img) as im:
        if cmd == "info":
            print("base=0x%X root_sector=0x%X root_size=0x%X filetime=0x%016X" % (im.base, im.root_sector, im.root_size, im.filetime))
        elif cmd == "ls":
            for e in im.entries():
                print("%s\t%s\t0x%X\t%d" % ("D" if e.is_dir else "F", e.path, e.sector, e.size))
        elif cmd == "extract":
            e = im.find(argv[2])
            im.extract_to(e, argv[3])
            print("extracted %s (%d bytes) -> %s" % (e.path, e.size, argv[3]))
        elif cmd == "extract-all":
            out = argv[2]
            for e in im.entries():
                if not e.is_dir:
                    im.extract_to(e, os.path.join(out, e.path.lstrip("/")))
        else:
            print(__doc__)
            return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
