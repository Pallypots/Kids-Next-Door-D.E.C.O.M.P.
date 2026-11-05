"""Synthetic tests for tools/jam.py: valid archives of each class, and malformed input that must be refused."""
import os
import struct
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import jam  # noqa: E402

NAMES = [b"DATA", b"FILEA", b"FILEB", b"ROOT"]
EXTS = [b"AFA", b"AFD"]


def _word(token, name, dirs, files):
    if token == b"FSTR":
        return name | files << 12 | dirs << 21
    return name | dirs << 12 | files << 21


def build(token, files=((1, 0, 0x100, 0x10), (2, 1, 0x110, 0x20)), child=None):
    """Archive with one root file and one subdirectory DATA holding the remaining files."""
    body = b"".join(n.ljust(8, b"\0") for n in NAMES) + b"".join(e.ljust(4, b"\0") for e in EXTS)

    def file_entry(f):
        if token == b"JAM2":
            return struct.pack("<HB1xI", f[0], f[1], f[2])
        return struct.pack("<HB1xII", f[0], f[1], f[2], f[3])

    entry = jam.FILE_ENTRY[token]
    root_off = 0x20 + len(body)
    children = root_off + 4 + 8 + entry                     # root counts, one directory entry, one root file
    tail = file_entry(files[0])
    table = struct.pack("<HH", 1, 1) + struct.pack("<II", _word(token, 0, 0, len(files) - 1),
                                                   children if child is None else child) + tail
    table += b"".join(file_entry(f) for f in files[1:])
    size = root_off + len(table)
    head = token + struct.pack("<II", 0, size) + b"none".ljust(15, b"\0") + b"\0" + struct.pack("<HBx", len(NAMES), len(EXTS))
    return head + body + table


class Parsing(unittest.TestCase):
    def test_fsta(self):
        j = jam.Jam(build(b"FSTA"))
        self.assertEqual(j.info["name"], "none")
        self.assertEqual(j.files, [("FILEA.AFA", 0x100, 0x10), ("DATA\\FILEB.AFD", 0x110, 0x20)])
        self.assertEqual(j.dirs, ["DATA\\"])
        self.assertEqual(j.validate(0x200), [])

    def test_fstr_uses_the_other_bitfields(self):
        j = jam.Jam(build(b"FSTR"))
        self.assertEqual(j.files, [("FILEA.AFA", 0x100, 0x10), ("DATA\\FILEB.AFD", 0x110, 0x20)])

    def test_jam2_reads_size_from_the_record(self):
        sizes = {0x100: 0x10, 0x140: 0x20}
        def read_at(off, n):
            return struct.pack("<II", sizes[off], sizes[off])
        data = build(b"JAM2", files=((1, 0, 0x100, None), (2, 1, 0x140, None)))
        j = jam.Jam(data, read_at=read_at)
        self.assertEqual(j.files, [("FILEA.AFA", 0x100, 0x10), ("DATA\\FILEB.AFD", 0x140, 0x20)])
        self.assertEqual(j.validate(0x180), [])

    def test_jam2_size_unknown_without_a_reader_is_reported(self):
        j = jam.Jam(build(b"JAM2", files=((1, 0, 0x100, None), (2, 1, 0x140, None))))
        self.assertTrue(j.validate(0x180))


class Refusals(unittest.TestCase):
    def test_invalid_token(self):
        with self.assertRaises(jam.JamError):
            jam.Jam(b"NOPE" + build(b"FSTA")[4:])

    def test_jam1_is_not_silently_misread(self):
        with self.assertRaises(jam.JamError):
            jam.Jam(b"JAM1" + build(b"FSTA")[4:])

    def test_truncated_info_header(self):
        with self.assertRaises(jam.JamError):
            jam.parse_info(b"FSTA\0\0")

    def test_truncated_header_body(self):
        with self.assertRaises(jam.JamError):
            jam.Jam(build(b"FSTA")[:0x30])

    def test_header_larger_than_file(self):
        data = build(b"FSTA")
        with self.assertRaises(jam.JamError):
            jam.Jam(data, file_size=len(data) - 1)

    def test_child_table_outside_header(self):
        with self.assertRaises(jam.JamError):
            jam.Jam(build(b"FSTA", child=0x100000))

    def test_directory_cycle(self):  # the child points back at the root's own table
        root = 0x20 + 8 * len(NAMES) + 4 * len(EXTS) + 4
        with self.assertRaises(jam.JamError):
            jam.Jam(build(b"FSTA", child=root))

    def test_bad_name_index(self):
        with self.assertRaises(jam.JamError):
            jam.Jam(build(b"FSTA", files=((99, 0, 0x100, 0x10), (2, 1, 0x110, 0x20))))

    def test_bad_extension_index(self):
        with self.assertRaises(jam.JamError):
            jam.Jam(build(b"FSTA", files=((1, 99, 0x100, 0x10), (2, 1, 0x110, 0x20))))

    def test_jam2_read_past_file_propagates(self):
        def read_at(off, n):
            raise jam.JamError("past the file")
        with self.assertRaises(jam.JamError):
            jam.Jam(build(b"JAM2", files=((1, 0, 0x100, None), (2, 1, 0x140, None))), read_at=read_at)


class Validation(unittest.TestCase):
    def test_extent_past_file(self):
        self.assertTrue(jam.Jam(build(b"FSTA")).validate(0x120))

    def test_overlap(self):
        j = jam.Jam(build(b"FSTA", files=((1, 0, 0x100, 0x40), (2, 1, 0x110, 0x20))))
        self.assertTrue(any("overlaps" in p for p in j.validate(0x200)))

    def test_file_inside_header(self):
        j = jam.Jam(build(b"FSTA", files=((1, 0, 0x10, 0x10), (2, 1, 0x110, 0x20))))
        self.assertTrue(any("inside the header" in p for p in j.validate(0x200)))

    def test_jam2_misaligned_record(self):
        def read_at(off, n):
            return struct.pack("<II", 0x10, 0x10)
        j = jam.Jam(build(b"JAM2", files=((1, 0, 0x104, None), (2, 1, 0x140, None))), read_at=read_at)
        self.assertTrue(any("aligned" in p for p in j.validate(0x180)))


if __name__ == "__main__":
    unittest.main()
