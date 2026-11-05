"""Qualification of the Xbox checker (tools/xcheck.py): positive and negative controls.

Part 1 (synthetic, public-CI safe): the comparison core on hand-built inputs.
Part 2 (local only): real MSVC objects compiled from deliberately broken variants of recovered source, compared
against the owner's reference XBE. Skipped automatically when the XBE or the compiler is absent.
"""
import os
import struct
import sys
import tomllib
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import xcheck  # noqa: E402

DIR32, REL32 = 0x0006, 0x0014


class CompareCore(unittest.TestCase):
    VA = 0x00100000

    def call_bytes(self, target):  # e8 rel32 ; c3
        return b"\xe8" + struct.pack("<i", target - (self.VA + 5)) + b"\xc3"

    def test_exact_match(self):
        self.assertEqual(xcheck.compare(b"\x8b\x41\x08\xc3", [], b"\x8b\x41\x08\xc3", self.VA, {}, 4)[0], "MATCH")

    def test_wrong_constant(self):
        self.assertEqual(xcheck.compare(b"\x8b\x41\x0c\xc3", [], b"\x8b\x41\x08\xc3", self.VA, {}, 4)[0], "MISMATCH")

    def test_reversed_condition(self):  # je vs jne
        self.assertEqual(xcheck.compare(b"\x74\x01\xc3\xc3", [], b"\x75\x01\xc3\xc3", self.VA, {}, 4)[0], "MISMATCH")

    def test_rel32_correct_symbol(self):
        cand = b"\xe8\0\0\0\0\xc3"  # MSVC REL32 field holds the addend (0)
        r = xcheck.compare(cand, [(1, REL32, "f")], self.call_bytes(0x00100100), self.VA, {"f": 0x00100100}, 6)
        self.assertEqual(r[0], "MATCH", r)

    def test_rel32_wrong_target(self):
        cand = b"\xe8\0\0\0\0\xc3"
        r = xcheck.compare(cand, [(1, REL32, "f")], self.call_bytes(0x00100100), self.VA, {"f": 0x00100200}, 6)
        self.assertEqual(r[0], "MISMATCH")

    def test_dir32_data_reference(self):
        orig = b"\xa1" + struct.pack("<I", 0x002B94A8) + b"\xc3"
        cand = b"\xa1\0\0\0\0\xc3"
        self.assertEqual(xcheck.compare(cand, [(1, DIR32, "g")], orig, self.VA, {"g": 0x002B94A8}, 6)[0], "MATCH")
        self.assertEqual(xcheck.compare(cand, [(1, DIR32, "g")], orig, self.VA, {"g": 0x002B94AC}, 6)[0], "MISMATCH")

    def test_dir32_addend_respected(self):
        orig = b"\xa1" + struct.pack("<I", 0x002B94AC) + b"\xc3"
        cand = b"\xa1" + struct.pack("<I", 4) + b"\xc3"  # g+4
        self.assertEqual(xcheck.compare(cand, [(1, DIR32, "g")], orig, self.VA, {"g": 0x002B94A8}, 6)[0], "MATCH")
        cand0 = b"\xa1\0\0\0\0\xc3"
        self.assertEqual(xcheck.compare(cand0, [(1, DIR32, "g")], orig, self.VA, {"g": 0x002B94A8}, 6)[0], "MISMATCH")

    def test_unknown_symbol_is_not_masked(self):
        cand = b"\xe8\0\0\0\0\xc3"
        r = xcheck.compare(cand, [(1, REL32, "nobody")], self.call_bytes(0x00100100), self.VA, {}, 6)
        self.assertEqual(r[0], "UNRESOLVED")

    def test_unsupported_relocation(self):
        r = xcheck.compare(b"\0" * 6, [(1, 0x000B, "s")], b"\0" * 6, self.VA, {"s": 0}, 6)
        self.assertEqual(r[0], "UNSUPPORTED")

    def test_truncated_or_padded_candidate(self):
        self.assertEqual(xcheck.compare(b"\xc3", [], b"\x90\xc3", self.VA, {}, 2)[0], "SIZE")
        self.assertEqual(xcheck.compare(b"\x90\xc3\xcc", [], b"\x90\xc3\xcc", self.VA, {}, 2)[0], "SIZE")


class VtableCore(unittest.TestCase):
    ORIG = struct.pack("<2I", 0x00100000, 0x00100010)
    RELOCS = [(0, DIR32, "a"), (4, DIR32, "b")]
    MAP = {"a": 0x00100000, "b": 0x00100010}

    def test_match(self):
        self.assertEqual(xcheck.compare_vtable(b"\0" * 8, self.RELOCS, self.ORIG, self.MAP, 2)[0], "MATCH")

    def test_swapped_slots(self):
        swapped = [(0, DIR32, "b"), (4, DIR32, "a")]
        self.assertEqual(xcheck.compare_vtable(b"\0" * 8, swapped, self.ORIG, self.MAP, 2)[0], "MISMATCH")

    def test_too_few_or_too_many_slots(self):
        self.assertEqual(xcheck.compare_vtable(b"\0" * 4, self.RELOCS[:1], self.ORIG, self.MAP, 2)[0], "SIZE")
        self.assertEqual(xcheck.compare_vtable(b"\0" * 12, self.RELOCS, self.ORIG, self.MAP, 2)[0], "SIZE")

    def test_slot_without_relocation(self):
        self.assertEqual(xcheck.compare_vtable(b"\0" * 8, self.RELOCS[:1], self.ORIG, self.MAP, 2)[0], "MISMATCH")

    def test_unknown_symbol_is_not_masked(self):
        self.assertEqual(xcheck.compare_vtable(b"\0" * 8, self.RELOCS, self.ORIG, {"a": 0x00100000}, 2)[0], "UNRESOLVED")

    def test_wrong_relocation_type(self):
        rel = [(0, REL32, "a"), (4, DIR32, "b")]
        self.assertEqual(xcheck.compare_vtable(b"\0" * 8, rel, self.ORIG, self.MAP, 2)[0], "UNSUPPORTED")


def build_coff(sections, symbols):
    """Minimal i386 COFF. sections: (name, raw bytes or None, memory size, relocs[(off, symidx, type)]).
    symbols: (name, value, section, class, aux bytes or None)."""
    nsym = sum(1 + (1 if s[4] else 0) for s in symbols)
    data_off = 20 + 40 * len(sections)
    blobs, headers, off = [], [], data_off
    for name, raw, size, relocs in sections:
        ptr = 0
        if raw is not None:
            ptr = off
            blobs.append(raw)
            off += len(raw)
        prel = off if relocs else 0
        for r in relocs:
            blobs.append(struct.pack("<IIH", *r))
            off += 10
        headers.append(name.encode().ljust(8, b"\0") + struct.pack("<IIIIIIHHI", 0, 0, size, ptr, prel, 0, len(relocs),
                                                                   0, 0x40000040))
    symtab = b""
    for name, value, sec, cls, aux in symbols:
        symtab += name.encode().ljust(8, b"\0") + struct.pack("<IhHBB", value, sec, 0, cls, 1 if aux else 0)
        if aux:
            symtab += aux.ljust(18, b"\0")
    head = struct.pack("<HHIIIHH", 0x14C, len(sections), 0, off, nsym, 0, 0)
    return head + b"".join(headers) + b"".join(blobs) + symtab + struct.pack("<I", 4)


class CoffParsing(unittest.TestCase):
    def test_bss_size_is_not_a_file_extent(self):  # regression: a large .bss was rejected as "outside file"
        c = xcheck.Coff(build_coff([(".bss", None, 0x1F400, [])], []))
        self.assertEqual(len(c.sections[0]["raw"]), 0x1F400)

    def test_section_data_outside_file_is_rejected(self):
        blob = bytearray(build_coff([(".text", b"\x90", 1, [])], []))
        struct.pack_into("<I", blob, 20 + 16, 0x1000)  # section size beyond the file, raw pointer unchanged
        with self.assertRaises(xcheck.CoffError):
            xcheck.Coff(bytes(blob))

    def test_weak_external_resolves_to_its_default(self):
        symbols = [("real", 0, 1, 2, None),                            # index 0: defined
                   ("alias", 0, 0, xcheck.IMAGE_SYM_CLASS_WEAK_EXTERNAL, struct.pack("<I", 0))]  # index 1 + aux
        c = xcheck.Coff(build_coff([(".rdata", b"\0" * 4, 4, [(0, 1, DIR32)])], symbols))
        self.assertEqual(c.reloc_symbol_name(1), "real")
        self.assertEqual(c.reloc_symbol_name(0), "real")


# ---------------------------------------------------------------- local, real objects
XBE_PATH = os.path.join(ROOT, "orig", "xiso", "default.xbe")


def _toolchain():
    with open(os.path.join(ROOT, "config", "toolchain.toml"), "rb") as f:
        return tomllib.load(f)["compiler"]["msvc14-host"]


def _available():
    try:
        return os.path.exists(XBE_PATH) and os.path.exists(os.path.join(_toolchain()["bin"], "cl.exe"))
    except Exception:  # noqa: BLE001
        return False


HEADER = '#include "gm/GmLinkedList.h"\n'


@unittest.skipUnless(_available(), "reference XBE or MSVC not available (local-only qualification)")
class RealObjects(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import verify
        import xbe
        cls.verify = verify
        cls.x = xbe.load(XBE_PATH)
        cls.comp = _toolchain()
        with open(os.path.join(ROOT, "config", "functions.toml"), "rb") as f:
            cls.symbols = verify.symbol_map(tomllib.load(f))

    def run_variant(self, name, body, symbol, va, size, symmap=None):
        # One directory per variant: MSVC resolves "quoted" includes relative to the source file first, so a shared
        # directory could shadow include/gm/GmLinkedList.h with a stale copy.
        obj = os.path.join(ROOT, "build", "qual", name, name + ".obj")
        ok, log = self.verify.compile_src(self.comp, None, obj, src_text=HEADER + body)
        self.assertTrue(ok, log)
        coff = xcheck.load(obj)
        return xcheck.check_function(coff, symbol, self.x, va, size, self.symbols if symmap is None else symmap)

    NEXT = "?Next@GmLinkedListIterator@@UAEXXZ"
    CUR = "?Current@GmLinkedListIterator@@UAEPAXXZ"
    CONTAINS = "?Contains@GmSocket@@UAEHPAVGmPlug@@@Z"
    CONTAINS_BODY = "int GmSocket::Contains(GmPlug* item){ return item->IsConnectedTo(this); }\n"

    def test_positive_control(self):
        r = self.run_variant("pos_next", "void GmLinkedListIterator::Next(){ if (m_cur) m_cur = m_cur->next; }",
                             self.NEXT, 0x76EC0, 14)
        self.assertEqual(r[0], "MATCH", r)

    def test_wrong_constant_field(self):  # ->prev (+0x1C) instead of ->next (+0x18)
        r = self.run_variant("neg_const", "void GmLinkedListIterator::Next(){ if (m_cur) m_cur = m_cur->prev; }",
                             self.NEXT, 0x76EC0, 14)
        self.assertEqual(r[0], "MISMATCH", r)

    def test_reversed_condition(self):
        r = self.run_variant("neg_cond", "void* GmLinkedListIterator::Current(){ if (m_cur) return 0; return m_socket; }",
                             self.CUR, 0x76F20, 12)
        self.assertNotEqual(r[0], "MATCH", r)

    def test_wrong_call_target(self):
        body = self.CONTAINS_BODY
        good = self.run_variant("pos_contains", body, self.CONTAINS, 0x75FE0, 13)
        self.assertEqual(good[0], "MATCH", good)
        bad_map = dict(self.symbols)
        bad_map["?IsConnectedTo@GmPlug@@QAEHPAVGmSocket@@@Z"] = 0x76C80  # the sibling function's address
        bad = self.run_variant("neg_call", body, self.CONTAINS, 0x75FE0, 13, bad_map)
        self.assertEqual(bad[0], "MISMATCH", bad)

    def test_unmapped_call_is_unresolved(self):
        body = self.CONTAINS_BODY
        r = self.run_variant("neg_unmapped", body, self.CONTAINS, 0x75FE0, 13, {})
        self.assertEqual(r[0], "UNRESOLVED", r)

    def test_removed_function_is_missing(self):
        r = self.run_variant("neg_missing", "void GmLinkedListIterator::Last(){ m_cur = List()->tail; }",
                             self.NEXT, 0x76EC0, 14)
        self.assertEqual(r[0], "MISSING", r)

    def test_wrong_address(self):  # correct source checked at the neighbouring function's address
        r = self.run_variant("neg_addr", "void GmLinkedListIterator::Next(){ if (m_cur) m_cur = m_cur->next; }",
                             self.NEXT, 0x76ED0, 14)
        self.assertEqual(r[0], "MISMATCH", r)

    # A vftable built from a class whose methods were declared in another order, at the real address.
    SWAPPED = """
class Swapped { public: Swapped(); virtual ~Swapped(); virtual int Contains(GmPlug*); virtual void Remove(GmPlug*); };
Swapped::Swapped() {}
Swapped::~Swapped() {}
int Swapped::Contains(GmPlug*) { return 0; }
void Swapped::Remove(GmPlug*) {}
"""

    def test_vtable_positive_control(self):
        obj = os.path.join(ROOT, "build", "qual", "vt_pos", "vt_pos.obj")
        ok, log = self.verify.compile_src(self.comp, "src/gm/GmLinkedList.cpp", obj)
        self.assertTrue(ok, log)
        r = xcheck.check_vtable(xcheck.load(obj), "??_7GmLinkedList@@6B@", self.x, 0x278568, 5, self.symbols)
        self.assertEqual(r[0], "MATCH", r)

    def test_vtable_wrong_address(self):
        obj = os.path.join(ROOT, "build", "qual", "vt_addr", "vt_addr.obj")
        ok, log = self.verify.compile_src(self.comp, "src/gm/GmLinkedList.cpp", obj)
        self.assertTrue(ok, log)
        r = xcheck.check_vtable(xcheck.load(obj), "??_7GmLinkedList@@6B@", self.x, 0x278418, 5, self.symbols)
        self.assertEqual(r[0], "MISMATCH", r)

    def test_vtable_wrong_slot_order(self):
        name = "vt_swap"
        obj = os.path.join(ROOT, "build", "qual", name, name + ".obj")
        ok, log = self.verify.compile_src(self.comp, None, obj, src_text=HEADER + self.SWAPPED)
        self.assertTrue(ok, log)
        symmap = dict(self.symbols)
        symmap["?Contains@Swapped@@UAEHPAVGmPlug@@@Z"] = 0x75FE0  # true address of Contains
        symmap["?Remove@Swapped@@UAEXPAVGmPlug@@@Z"] = 0x75FF0    # true address of Remove
        symmap["??_GSwapped@@UAEPAXI@Z"] = 0x77040                # true slot 0, so only the order can fail
        # Slots 1 and 2 are compared with 0x278568+4: the original Append and Remove, so Contains in slot 1 fails.
        coff = xcheck.load(obj)
        r = xcheck.check_vtable(coff, "??_7Swapped@@6B@", self.x, 0x278568, 3, symmap)
        self.assertEqual(r[0], "MISMATCH", r)


if __name__ == "__main__":
    unittest.main()
