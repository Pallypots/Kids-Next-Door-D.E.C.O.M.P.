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
            cls.symbols = dict(tomllib.load(f)["symbols"])

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
    CONTAINS = "?Contains@?$GmLinkedListOf@PAVFaUiBase@@@@UAEHPAVFaUiBase@@@Z"

    def test_positive_control(self):
        r = self.run_variant("pos_next", "void GmLinkedListIterator::Next(){ if (m_cur) m_cur = m_cur->next; }",
                             self.NEXT, 0x76EC0, 14)
        self.assertEqual(r[0], "MATCH", r)

    def test_wrong_constant_field(self):  # ->prev (+0x1C) instead of ->next (+0x18)
        r = self.run_variant("neg_const", "void GmLinkedListIterator::Next(){ if (m_cur) m_cur = m_cur->prev; }",
                             self.NEXT, 0x76EC0, 14)
        self.assertEqual(r[0], "MISMATCH", r)

    def test_reversed_condition(self):
        r = self.run_variant("neg_cond", "void* GmLinkedListIterator::Current(){ if (m_cur) return 0; return m_list; }",
                             self.CUR, 0x76F20, 12)
        self.assertNotEqual(r[0], "MATCH", r)

    def test_wrong_call_target(self):
        body = "template int GmLinkedListOf<FaUiBase*>::Contains(FaUiBase*);\n"
        good = self.run_variant("pos_contains", body, self.CONTAINS, 0x75FE0, 13)
        self.assertEqual(good[0], "MATCH", good)
        bad_map = dict(self.symbols)
        bad_map["?IsMemberOf@FaUiBase@@QAEHPAVGmSocket@@@Z"] = 0x76C80  # the sibling function's address
        bad = self.run_variant("neg_call", body, self.CONTAINS, 0x75FE0, 13, bad_map)
        self.assertEqual(bad[0], "MISMATCH", bad)

    def test_unmapped_call_is_unresolved(self):
        body = "template int GmLinkedListOf<FaUiBase*>::Contains(FaUiBase*);\n"
        r = self.run_variant("neg_unmapped", body, self.CONTAINS, 0x75FE0, 13, {})
        self.assertEqual(r[0], "UNRESOLVED", r)

    def test_removed_function_is_missing(self):
        r = self.run_variant("neg_missing", "void GmLinkedListIterator::Last(){ m_cur = m_list->tail; }",
                             self.NEXT, 0x76EC0, 14)
        self.assertEqual(r[0], "MISSING", r)

    def test_wrong_address(self):  # correct source checked at the neighbouring function's address
        r = self.run_variant("neg_addr", "void GmLinkedListIterator::Next(){ if (m_cur) m_cur = m_cur->next; }",
                             self.NEXT, 0x76ED0, 14)
        self.assertEqual(r[0], "MISMATCH", r)


if __name__ == "__main__":
    unittest.main()
