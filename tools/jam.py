#!/usr/bin/env python3
"""Read-only parser for the game's JAM archives (FSTA/FSTR/JAM2 headers). Layout recovered from code: docs/formats.md.

  python -I tools/jam.py check <image>            parse every .JAM on a disc image and validate each one
  python -I tools/jam.py ls <image> <jam name>    list the files in one archive (matched on the end of its path)
  python -I tools/jam.py cases <image> <out dir>  write oracle inputs (git-ignored directory only: real game data)
  python -I tools/jam.py textcases <image> <out dir>  each distinct text file once, for the tokenizer oracle (git-ignored directory only)
  python -I tools/jam.py e2e <image> <out dir> [--all]  extract archives and a manifest of CRC-32s (git-ignored directory only);
                                                  by default small archives and the first files of each; --all means everything

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


def perturbations(path):
    """Paths that are near a real one and exercise lookup failures and quirks."""
    leaf = path.rsplit("\\", 1)[-1]
    parent = path[:-len(leaf)]
    stem, _, ext = leaf.partition(".")
    out = [path.lower(), parent + stem, parent + stem + ".ZZZ", parent + "ZZZZZZ." + ext, "ZZZZZZ\\" + path, leaf,
           parent + stem + "." + ext + ".X", path + "\\", "\\" + path, parent + "\\" + leaf]
    if parent:
        first = parent.split("\\")[0]
        out += [first + "\\" + first + "\\" + leaf, parent.rstrip("\\"), parent, parent.lower()]
    return out


def swap_to_fstr(header):
    """An FSTR-form copy of a real FSTA header: the token and every directory word's two count fields. FSTR does not occur
    on this disc, so this is the only way to run that code on realistic data; it is derived, not a real file."""
    jam = Jam(header)
    if jam.token != b"FSTA":
        raise JamError("expected FSTA")
    out = bytearray(header)
    out[:4] = b"FSTR"

    def walk(off, ndirs, nfiles):
        children = []
        for i in range(ndirs):
            at = off + 8 * i
            word, child = struct.unpack_from("<II", out, at)
            d, f = (word >> 12) & 0x1FF, word >> 21
            if d > 0x1FF or f > 0x1FF:
                raise JamError("counts do not fit the swapped fields")
            struct.pack_into("<I", out, at, (word & 0xFFF) | f << 12 | d << 21)
            children.append((child, d, f))
        for child, d, f in children:
            walk(child, d, f)

    root = 0x20 + 4 * (jam.info["exts"] + 2 * jam.info["names"])
    walk(root + 4, jam.root_dirs, jam.root_files)
    return bytes(out)


def write_case(out_dir, name, data, jam):
    with open(os.path.join(out_dir, name + ".bin"), "wb") as f:
        f.write(data)
    with open(os.path.join(out_dir, name + ".txt"), "w", encoding="latin-1", newline="\n") as f:
        for path, off, size in jam.files:
            f.write("F\t%s\t%d\t%d\n" % (path, off, size if size is not None else 0))
        for d in jam.dirs:
            f.write("D\t%s\t0\t0\n" % d.rstrip("\\"))
        seen = {p[0] for p in jam.files} | {d.rstrip("\\") for d in jam.dirs}
        for path, _, _ in jam.files[:400]:
            for q in perturbations(path):
                if q and q not in seen and len(q) < 200:
                    seen.add(q)
                    f.write("?\t%s\t0\t0\n" % q)


def synthetic_archive(token):
    """Hand-built archive reaching what the disc cannot: an empty name, a file with no extension, a full-length label, a
    deep tree for the failed-component behaviour, and (JAM2) records whose two size fields differ. No game data."""
    names = [b"", b"ART", b"FILEA", b"FILEB", b"LEVEL", b"ROOT", b"SUB"]
    exts = [b"", b"AFA", b"AFD"]
    swapped = token == b"FSTR"
    jam2 = token == b"JAM2"

    def word(name, dirs, files):
        return name | files << 12 | dirs << 21 if swapped else name | dirs << 12 | files << 21

    def file_entry(name, ext, off, size):
        if jam2:
            return struct.pack("<HB1xI", name, ext, off)
        return struct.pack("<HB1xII", name, ext, off, size)

    head = token + struct.pack("<II", 0x11223344, 0) + b"ABCDEFGHIJKLMNO" + b"\0" + struct.pack("<HBx", len(names), len(exts))
    table = b"".join(n.ljust(8, b"\0") for n in names) + b"".join(e.ljust(4, b"\0") for e in exts)
    entry = FILE_ENTRY[token]
    root = 0x20 + len(table)
    # root: dirs ART, ROOT; files FILEA.AFA, FILEB (no extension)
    root_table = root + 4
    art = root_table + 2 * 8 + 2 * entry      # dirs LEVEL, SUB; file FILEB.AFD
    roots = art + 2 * 8 + entry               # dir LEVEL (a second one); file FILEA
    level = roots + 8 + entry                 # file FILEA.AFD
    sub = level + entry                       # dir LEVEL (a third); no files
    level3 = sub + 8                          # file ROOT.AFA
    end = level3 + entry
    offset = (end + 0x1F) & ~0x1F
    # Five data slots, 0x40 apart.
    slots = [offset + 0x40 * k for k in range(6)]
    body = struct.pack("<HH", 2, 2)
    body += struct.pack("<II", word(1, 2, 1), art) + struct.pack("<II", word(5, 1, 1), roots)
    body += file_entry(2, 1, slots[0], 0x11) + file_entry(3, 0, slots[1], 0x12)
    body += struct.pack("<II", word(4, 0, 1), level) + struct.pack("<II", word(6, 1, 0), sub)
    body += file_entry(3, 2, slots[2], 0x13)
    body += struct.pack("<II", word(4, 0, 1), level) + file_entry(2, 0, slots[3], 0x14)
    body += file_entry(2, 2, slots[4], 0x15)
    body += struct.pack("<II", word(4, 0, 1), level3)
    body += file_entry(5, 1, slots[5], 0x16)
    data = bytearray(head + table + body)
    struct.pack_into("<I", data, 8, len(data))
    if jam2:
        data.extend(b"\0" * (slots[-1] + 0x40 - len(data)))
        for k, off in enumerate(slots):
            struct.pack_into("<II", data, off, 0x1000 + k, 0x20 + k)  # first field != second field
    return bytes(data)


SYNTHETIC_QUERIES = [
    "FILEA.AFA", "FILEB", "FILEA", "FILEB.AFA", "filea.afa", "ART", "ROOT", "LEVEL", "SUB", "FILEA.ZZZ", "NOPE", "NOPE.AFA",
    "ART\\FILEB.AFD", "ART\\LEVEL", "ART\\LEVEL\\FILEA.AFD", "ART\\SUB", "ART\\SUB\\LEVEL", "ART\\SUB\\LEVEL\\ROOT.AFA",
    "ROOT\\FILEA", "ROOT\\LEVEL", "ROOT\\LEVEL\\FILEA.AFD", "art\\sub\\level\\root.afa",
    # Failed middle components: names that exist but are not children here, so the scan continues from where it stopped.
    "FILEA\\ART", "FILEA\\ROOT", "ROOT\\ART", "SUB\\ROOT", "LEVEL\\ART", "FILEA\\FILEB\\ROOT", "ART\\ROOT",
    "ART\\FILEA\\SUB", "ART\\FILEA\\LEVEL", "ART\\ART\\LEVEL", "SUB\\ART\\LEVEL\\FILEA.AFD", "FILEA\\ROOT\\FILEA",
    "ROOT\\ART\\FILEB.AFD", "FILEA\\ROOT\\LEVEL\\FILEA.AFD", "ART\\NOPE", "NOPE\\ART", "NOPE\\FILEA.AFA",
    # Odd separators and empty components.
    "\\ART", "ART\\", "ART\\\\LEVEL", "\\", "\\\\", "\\FILEA.AFA", "ART\\\\FILEB.AFD", ".", ".AFA", "FILEA.", "ART\\.AFD",
]


def write_synthetic_cases(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    for token, name in ((b"FSTA", "synth_fsta"), (b"FSTR", "synth_fstr"), (b"JAM2", "synth_jam2")):
        with open(os.path.join(out_dir, name + ".bin"), "wb") as f:
            f.write(synthetic_archive(token))
        with open(os.path.join(out_dir, name + ".txt"), "w", encoding="latin-1", newline="\n") as f:
            for q in SYNTHETIC_QUERIES:
                f.write("?\t%s\t0\t0\n" % q)


def write_cases(image, out_dir, whole_file_limit=1 << 22):
    """Header of every FSTA archive (whole file for small JAM2, whose records live in the data), plus a derived FSTR."""
    write_synthetic_cases(out_dir)
    img, entries = _jams_on_image(image)
    fstr_done = 0
    for e in entries:
        base = img.sector_offset(e.sector)
        jam = _load(img, e)
        name = e.path.rsplit("/", 1)[-1][:-4]
        if jam.token == b"JAM2":
            if e.size <= whole_file_limit:
                write_case(out_dir, name, img._read(base, e.size), jam)
            continue
        write_case(out_dir, name, jam.buf, jam)
        if fstr_done < 4 and len(jam.files) > 0:
            write_case(out_dir, "fstr_" + name, swap_to_fstr(jam.buf), Jam(swap_to_fstr(jam.buf)))
            fstr_done += 1


def write_e2e(image, out_dir, limit=1 << 25, per_archive=400):
    """Extract small archives and slice their files with Python alone, so the manifest owes nothing to the recovered code."""
    import zlib
    os.makedirs(out_dir, exist_ok=True)
    img, entries = _jams_on_image(image)
    with open(os.path.join(out_dir, "manifest.txt"), "w", encoding="latin-1", newline="\n") as m:
        for e in entries:
            if e.size > limit:
                continue
            jam = _load(img, e)
            name = e.path.rsplit("/", 1)[-1]
            data = img._read(img.sector_offset(e.sector), e.size)
            with open(os.path.join(out_dir, name), "wb") as f:
                f.write(data)
            m.write("A\t%s\n" % name)
            record = JAM2_RECORD if jam.token == b"JAM2" else 0
            for path, off, size in jam.files[:per_archive]:
                body = data[off + record:off + record + size]
                m.write("F\t%s\t%d\t%d\n" % (path, size, zlib.crc32(body) & 0xFFFFFFFF))


def write_text_cases(image, out_dir, binary_samples=60):
    """Oracle inputs for tests/oracle/oracle_tok.cpp: each distinct text file once, and some binary files (git-ignored directory only)."""
    import hashlib
    img, entries = _jams_on_image(image)
    os.makedirs(out_dir, exist_ok=True)
    seen, index, binary = set(), [], 0
    total = 0
    for e in entries:
        j = _load(img, e)
        base = img.sector_offset(e.sector)
        for path, off, size in j.files:
            if size > 1 << 22:
                continue
            data = img._read(base + off + (JAM2_RECORD if j.token == b"JAM2" else 0), size)
            digest = hashlib.sha256(data).hexdigest()[:16]
            if digest in seen:
                continue
            seen.add(digest)
            head = data[:512]
            text = bool(head) and not any(c < 9 or 13 < c < 32 for c in head)
            name = None
            if text:
                name = digest + ".bin"
            elif binary < binary_samples and len(seen) % 40 == 0:
                name = "b_" + digest + ".bin"
                binary += 1
            if name:
                with open(os.path.join(out_dir, name), "wb") as f:
                    f.write(data)
                index.append("%s	%s	%s	%d" % (name, e.path, path, size))
                total += size
    with open(os.path.join(out_dir, "index.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(index) + "\n")
    print("%d files (%d binary samples), %d bytes, in %s" % (len(index), binary, total, out_dir))


def main(argv):
    if len(argv) >= 3 and argv[0] == "e2e":
        if len(argv) == 4 and argv[3] == "--all":
            write_e2e(argv[1], argv[2], limit=1 << 40, per_archive=1 << 30)
        else:
            write_e2e(argv[1], argv[2])
        return 0
    if len(argv) == 3 and argv[0] == "textcases":
        write_text_cases(argv[1], argv[2])
        return 0
    if len(argv) == 3 and argv[0] == "cases":
        write_cases(argv[1], argv[2])
        return 0
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
