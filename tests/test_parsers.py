"""Synthetic-fixture tests for tools/xdvdfs.py and tools/xbe.py. No game data required (safe for public CI).

Run: python -I -m unittest discover -s tests -v   (from the repository root)
"""
import os
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
import xbe  # noqa: E402
import xdvdfs  # noqa: E402


# ---------------------------------------------------------------- XDVDFS fixtures
def dirent(left, right, sector, size, attrs, name):
    e = struct.pack("<HHIIBB", left, right, sector, size, attrs, len(name)) + name.encode()
    return e + b"\xff" * ((-len(e)) % 4)


def make_xiso(base=0, tamper=None):
    S = 2048
    # root dir at sector 40: entries "a.txt" (left->"sub") and "sub" (dir at 41) containing "b.bin"
    e_a = dirent(0, 0, 0, 0, 0, "")  # placeholder for sizing
    sub = dirent(0, 0, 42, 5, 0x20, "b.bin")
    second = dirent(0, 0, 41, len(sub), 0x10, "sub")
    e_a = dirent(len(dirent(0, 0, 43, 3, 0x20, "a.txt")) // 4, 0, 43, 3, 0x20, "a.txt")
    root = e_a + second
    if tamper == "cycle":
        # Offset 0 means "no child", so a cycle must go through a non-zero offset: entry 2 points at itself.
        k = len(dirent(0, 0, 43, 3, 0x20, "a.txt")) // 4
        root = dirent(k, 0, 43, 3, 0x20, "a.txt") + dirent(k, 0, 43, 3, 0x20, "c.txt")
    vd = bytearray(S)
    vd[0:20] = xdvdfs.MAGIC
    struct.pack_into("<IIQ", vd, 20, 40, len(root), 0x1234)
    vd[0x7EC:0x800] = xdvdfs.MAGIC
    img = bytearray(base + S * 44)
    img[base + 32 * S:base + 33 * S] = vd
    img[base + 40 * S:base + 40 * S + len(root)] = root
    img[base + 41 * S:base + 41 * S + len(sub)] = sub
    img[base + 42 * S:base + 42 * S + 5] = b"hello"
    img[base + 43 * S:base + 43 * S + 3] = b"abc"
    if tamper == "truncate":
        img = img[:base + 42 * S + 2]
    return bytes(img)


class XdvdfsTests(unittest.TestCase):
    def _img(self, data):
        fd, p = tempfile.mkstemp(suffix=".iso")
        os.write(fd, data)
        os.close(fd)
        self.addCleanup(os.remove, p)
        return p

    def test_list_and_read(self):
        with xdvdfs.Image(self._img(make_xiso())) as im:
            self.assertEqual(im.base, 0)
            paths = [e.path for e in im.entries()]
            self.assertEqual(paths, ["/a.txt", "/sub", "/sub/b.bin"])
            self.assertEqual(im.read_file(im.find("sub/b.bin")), b"hello")
            self.assertEqual(im.read_file(im.find("A.TXT")), b"abc")  # case-insensitive lookup

    def test_redump_base_detected(self):
        # Use a small synthetic base from the known list would need 400 MB; test explicit base instead.
        with xdvdfs.Image(self._img(make_xiso(base=0x10000)), base=0x10000) as im:
            self.assertEqual(im.read_file(im.find("a.txt")), b"abc")

    def test_bad_magic(self):
        d = bytearray(make_xiso())
        d[32 * 2048] ^= 0xFF
        with self.assertRaises(xdvdfs.XdvdfsError):
            xdvdfs.Image(self._img(bytes(d)))

    def test_truncated_file_read_fails(self):
        with xdvdfs.Image(self._img(make_xiso(tamper="truncate"))) as im:
            with self.assertRaises(xdvdfs.XdvdfsError):
                im.read_file(im.find("sub/b.bin"))

    def test_cycle_detected(self):
        with xdvdfs.Image(self._img(make_xiso(tamper="cycle"))) as im:
            with self.assertRaises(xdvdfs.XdvdfsError):
                im.entries()


# ---------------------------------------------------------------- XBE fixtures
def make_xbe(variant="retail", ep=0x11010, digest_ok=True, rsize_over=False):
    base = 0x10000
    hdr = bytearray(0x1000)
    hdr[0:4] = b"XBEH"
    cert_va, sec_va, name_va, lib_va = base + 0x200, base + 0x400, base + 0x500, base + 0x600
    kthunk = 0x12000
    fields = [base, 0x1000, 0x3000, 0x178, 1000, cert_va, 2, sec_va, 0, ep ^ xbe.XOR_EP[variant], 0,
              0x10000, 0x100000, 0x1000, 0, 0, 0, 0, 0, 0, 0, kthunk ^ xbe.XOR_KT[variant], 0, 1, lib_va, 0, 0, 0, 0]
    struct.pack_into("<29I", hdr, 0x104, *fields)
    struct.pack_into("<III", hdr, 0x200, 0x1D0, 1000, 0x5454008A)
    hdr[0x20C:0x20C + 8] = "TEST".encode("utf-16-le")
    hdr[0x500:0x506] = b".text\0"
    hdr[0x506:0x50D] = b".rdata\0"
    hdr[0x600:0x608] = b"XAPILIB\0"
    struct.pack_into("<4H", hdr, 0x608, 1, 0, 5849, 1 | (2 << 13))
    text = bytes([0xC3]) * 0x20
    rdata = struct.pack("<II", 0x80000001, 0) + b"\0" * 0x18
    import hashlib
    secs = [(0x6, 0x11000, 0x1000, 0x1000, len(text), name_va, text),
            (0x2, 0x12000, 0x1000 if not rsize_over else 0x10, 0x2000, len(rdata), name_va + 6, rdata)]
    out = hdr + b"\0" * 0x2000
    out = bytearray(out)
    for i, (fl, va, vs, raw, rs, nva, body) in enumerate(secs):
        dg = hashlib.sha1(struct.pack("<I", rs) + body).digest()
        if not digest_ok and i == 0:
            dg = b"\0" * 20
        struct.pack_into("<9I", out, 0x400 + 0x38 * i, fl, va, vs, raw, rs, nva, 0, 0, 0)
        out[0x400 + 0x38 * i + 0x24:0x400 + 0x38 * i + 0x38] = dg
        out[raw:raw + rs] = body
    return bytes(out)


class XbeTests(unittest.TestCase):
    def test_retail_parse(self):
        x = xbe.Xbe(make_xbe())
        self.assertEqual(x.variant, "retail")
        self.assertEqual(x.entry, 0x11010)
        self.assertEqual([s.name for s in x.sections], [".text", ".rdata"])
        self.assertEqual(x.kernel_imports()[0]["ordinal"], 1)
        self.assertEqual(x.libraries[0]["version"], "1.0.5849")
        inv = x.inventory()
        self.assertTrue(all(s["digest_matches"] for s in inv["sections"]))
        self.assertEqual(inv["sections"][1]["zero_fill_range"], ["0x00012020", "0x00013000"])
        self.assertEqual(x.read_va(0x11000, 1), b"\xc3")

    def test_debug_key(self):
        self.assertEqual(xbe.Xbe(make_xbe("debug")).variant, "debug")

    def test_zero_fill_not_file_backed(self):
        x = xbe.Xbe(make_xbe())
        with self.assertRaises(xbe.XbeError):
            x.read_va(0x12020, 4)

    def test_entry_outside_code_rejected(self):
        with self.assertRaises(xbe.XbeError):
            xbe.Xbe(make_xbe(ep=0x12004))  # .rdata is not executable

    def test_bad_digest_reported(self):
        inv = xbe.Xbe(make_xbe(digest_ok=False)).inventory()
        self.assertFalse(inv["sections"][0]["digest_matches"])
        self.assertTrue(inv["sections"][1]["digest_matches"])

    def test_raw_bigger_than_virtual_rejected(self):
        with self.assertRaises(xbe.XbeError):
            xbe.Xbe(make_xbe(rsize_over=True))

    def test_bad_magic_and_truncation(self):
        d = bytearray(make_xbe())
        d[0] = 0
        with self.assertRaises(xbe.XbeError):
            xbe.Xbe(bytes(d))
        with self.assertRaises(xbe.XbeError):
            xbe.Xbe(make_xbe()[:0x300])


if __name__ == "__main__":
    unittest.main()
