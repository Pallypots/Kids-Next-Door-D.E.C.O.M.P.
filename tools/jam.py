#!/usr/bin/env python3
"""Read-only parser for the game's JAM archives (FSTA/FSTR/JAM2 headers). Layout recovered from code: docs/formats.md.

  python -I tools/jam.py check <image>            parse every .JAM on a disc image and validate each one
  python -I tools/jam.py ls <image> <jam name>    list the files in one archive (matched on the end of its path)

All integers are little-endian. Every read is bounds-checked; malformed input raises JamError. File values are never
converted to host pointers. JAM1 uses another header class (0x00081020) and is rejected explicitly until a file using
it is seen. This reads directories and record prefixes only; it does not extract or interpret file contents.
"""
import os
import struct
import sys

HEADER_INFO_SIZE = 0x20
TOKENS = {b"JAM1": 1, b"JAM2": 2, b"FSTA": 3, b"FSTR": 4}
# Directory word: name index in the low 12 bits; (subdirectory mask, shift, file mask, shift) by header class.
# FSTA/FSTR selected at 0x0007F367, 0x0007F398, 0x0007F3C4 by the flag the FSTR constructor sets (0x0007D178).
# JAM2 at 0x000801F2-0x000801F8.
DIR_FIELDS = {
    b"FSTA": (0x001FF000, 12, 0xFFE00000, 21),
    b"FSTR": (0xFFE00000, 21, 0x001FF000, 12),
    b"JAM2": (0x001FF000, 12, 0xFFE00000, 21),
}
FILE_ENTRY = {b"FSTA": 12, b"FSTR": 12, b"JAM2": 8}  # 0x0007FB33; 0x00080844
JAM2_RECORD = 0x20  # JAM2 file offsets address a record: u32 size, u32 size, padding, then the data.
JAM2_ALIGN = 0x20
MAX_DEPTH = 64


class JamError(Exception):
    pass


def _take(buf, off, n, what):
    if off < 0 or off + n > len(buf):
        raise JamError("%s: range 0x%X+0x%X outside header of 0x%X bytes" % (what, off, n, len(buf)))
    return buf[off:off + n]


def _text(raw):
    return raw.split(b"\0", 1)[0].decode("latin-1")


def parse_info(raw):
    """The fixed 0x20-byte info header (AtIoJamInfoHeader, 0x0007FE80)."""
    if len(raw) < HEADER_INFO_SIZE:
        raise JamError("file shorter than the info header")
    token = bytes(raw[:4])
    if token not in TOKENS:
        raise JamError("invalid Jam token %r" % token)
    unk04, header_size = struct.unpack_from("<II", raw, 4)
    # Copied as 15 bytes (0x0007FF88-0x0007FFA9); the code then advances to +0x1C.
    name = bytes(raw[0x0C:0x1B])
    names, exts = struct.unpack_from("<HB", raw, 0x1C)
    return {"token": token, "unk04": unk04, "header_size": header_size, "name": _text(name), "names": names,
            "exts": exts}


class Jam:
    """Parsed archive directory.

    files: list of (path, offset of the data's first byte or record, size or None). For JAM2 the size lives in the
    file's record, not the directory: pass read_at(offset, n) to fill it in, otherwise size stays None.
    """

    def __init__(self, header, file_size=None, read_at=None):
        self.info = parse_info(header)
        self.token = self.info["token"]
        if self.token not in DIR_FIELDS:
            raise JamError("%r header class not implemented" % self.token)
        if self.info["header_size"] > len(header):
            raise JamError("header claims 0x%X bytes, 0x%X supplied" % (self.info["header_size"], len(header)))
        if file_size is not None and self.info["header_size"] > file_size:
            raise JamError("header larger than the file")
        self.buf = bytes(header[:self.info["header_size"]])
        self.fields = DIR_FIELDS[self.token]
        self.read_at = read_at
        names, exts = self.info["names"], self.info["exts"]
        self.names_off = HEADER_INFO_SIZE
        self.exts_off = self.names_off + 8 * names                          # 0x00082C9B
        root = self.names_off + 4 * (exts + 2 * names)                      # 0x0007F8B9-0x0007F8C0
        self.root_dirs, self.root_files = struct.unpack("<HH", _take(self.buf, root, 4, "root counts"))
        self.dirs = []
        self.files = []
        self._seen = set()
        self._walk(root + 4, self.root_dirs, self.root_files, "", 0)

    def _name(self, index):
        if index >= self.info["names"]:
            raise JamError("name index %d of %d" % (index, self.info["names"]))
        return _text(_take(self.buf, self.names_off + 8 * index, 8, "name"))

    def _ext(self, index):
        if index >= self.info["exts"]:
            raise JamError("extension index %d of %d" % (index, self.info["exts"]))
        return _text(_take(self.buf, self.exts_off + 4 * index, 4, "extension"))

    def _file(self, off):
        """One directory file entry: (name index, extension index, offset, size or None)."""
        if self.token == b"JAM2":
            name, ext, offset = struct.unpack("<HB1xI", _take(self.buf, off, 8, "file entry"))
            return name, ext, offset, None
        return struct.unpack("<HB1xII", _take(self.buf, off, 12, "file entry"))

    def _walk(self, off, ndirs, nfiles, prefix, depth):
        if depth > MAX_DEPTH:
            raise JamError("directory nesting deeper than %d" % MAX_DEPTH)
        if off in self._seen:
            raise JamError("directory table at 0x%X visited twice (cycle)" % off)
        self._seen.add(off)
        dmask, dshift, fmask, fshift = self.fields
        subdirs = []
        for i in range(ndirs):
            word, child = struct.unpack("<II", _take(self.buf, off + 8 * i, 8, "directory entry"))
            subdirs.append((self._name(word & 0xFFF), child, (word & dmask) >> dshift, (word & fmask) >> fshift))
        foff = off + 8 * ndirs
        for i in range(nfiles):
            name, ext, offset, size = self._file(foff + FILE_ENTRY[self.token] * i)
            leaf = self._name(name)
            if self.info["exts"]:
                leaf += "." + self._ext(ext)
            if size is None and self.read_at is not None:
                first, second = struct.unpack("<II", self.read_at(offset, 8))
                if first != second:  # Seen only as equal so far; the two fields may differ in a form not yet met.
                    raise JamError("%s: record sizes differ (0x%X, 0x%X)" % (prefix + leaf, first, second))
                size = first
            self.files.append((prefix + leaf, offset, size))
        for name, child, cdirs, cfiles in subdirs:                          # 0x0007F3E7-0x0007F3EC: base + child offset
            path = prefix + name + "\\"
            self.dirs.append(path)
            self._walk(child, cdirs, cfiles, path, depth + 1)

    def validate(self, file_size):
        """Facts that must hold if the layout is right. Returns a list of problems; empty means none were found."""
        problems = []
        last_end = self.info["header_size"]
        record = JAM2_RECORD if self.token == b"JAM2" else 0
        for path, off, size in sorted(self.files, key=lambda f: (f[1], f[2] or 0)):
            if size is None:
                problems.append("%s: size unknown (no reader supplied)" % path)
                continue
            if off < self.info["header_size"]:
                problems.append("%s starts inside the header (0x%X)" % (path, off))
            end = off + record + size
            if end > file_size:
                problems.append("%s ends past the file (0x%X > 0x%X)" % (path, end, file_size))
            if off < last_end and (size or record):
                problems.append("%s overlaps the previous file (0x%X < 0x%X)" % (path, off, last_end))
            if record:
                if off % JAM2_ALIGN:
                    problems.append("%s record is not 0x%X-aligned (0x%X)" % (path, JAM2_ALIGN, off))
                end = (end + JAM2_ALIGN - 1) & ~(JAM2_ALIGN - 1)
                if off >= last_end + JAM2_ALIGN and last_end != self.info["header_size"]:
                    problems.append("%s leaves an unexplained gap (0x%X after 0x%X)" % (path, off, last_end))
            last_end = max(last_end, end)
        if len({f[0] for f in self.files}) != len(self.files):
            problems.append("duplicate paths")
        return problems


def _jams_on_image(image):
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import xdvdfs
    img = xdvdfs.Image(image)
    return img, [e for e in img.entries() if not e.is_dir and e.path.upper().endswith(".JAM")]


def _load(img, entry):
    """Read only the archive's header and its files' record prefixes from the image."""
    if entry.size < HEADER_INFO_SIZE:
        raise JamError("%s: shorter than the info header" % entry.path)
    base = img.sector_offset(entry.sector)
    info = parse_info(img._read(base, HEADER_INFO_SIZE))
    if info["header_size"] > entry.size:
        raise JamError("%s: header larger than the file" % entry.path)

    def read_at(off, n):
        if off + n > entry.size:
            raise JamError("%s: read 0x%X+0x%X past the file" % (entry.path, off, n))
        return img._read(base + off, n)

    return Jam(img._read(base, info["header_size"]), entry.size, read_at)


def main(argv):
    if len(argv) == 2 and argv[0] == "check":
        img, entries = _jams_on_image(argv[1])
        bad = total_files = 0
        seen = {}
        for e in entries:
            try:
                jam = _load(img, e)
                problems = jam.validate(e.size)
            except JamError as err:
                print("%-52s PARSE ERROR: %s" % (e.path, err))
                bad += 1
                continue
            seen[jam.token] = seen.get(jam.token, 0) + 1
            total_files += len(jam.files)
            print("%-52s %-5s dirs %3d files %5d %s" % (e.path, jam.token.decode(), len(jam.dirs), len(jam.files),
                                                        "OK" if not problems else "BAD: " + problems[0]))
            bad += bool(problems)
        print("archives %d (%s), files %d, archives with problems %d" % (
            len(entries), ", ".join("%s %d" % (k.decode(), v) for k, v in sorted(seen.items())), total_files, bad))
        return 1 if bad else 0
    if len(argv) == 3 and argv[0] == "ls":
        img, entries = _jams_on_image(argv[1])
        for e in entries:
            if e.path.lower().endswith(argv[2].lower().replace("\\", "/")):
                for name, off, n in _load(img, e).files:
                    print("%10d %10d  %s" % (off, n, name))
                return 0
        print("not found: %s" % argv[2])
        return 2
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
