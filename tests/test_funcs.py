"""Tests for tools/funcs.py: MSVC exception-metadata parsing (synthetic) and inventory facts on the real XBE (local)."""
import json
import os
import struct
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import funcs  # noqa: E402

MAGIC = 0x19930520


def memory(words):
    def u32(va):
        return words[va]
    return u32


def funcinfo(base, unwind, tries=()):
    """Lay out a FuncInfo at `base`: unwind = [action...], tries = [[handler...]...]."""
    m = {}
    umap = base + 0x100
    tmap = base + 0x200
    hmap = base + 0x300
    for off, v in enumerate((MAGIC, len(unwind), umap, len(tries), tmap)):
        m[base + 4 * off] = v
    for k, action in enumerate(unwind):
        m[umap + 8 * k] = k - 1
        m[umap + 8 * k + 4] = action
    for k, handlers in enumerate(tries):
        entry = tmap + 20 * k
        h = hmap + 0x40 * k
        m[entry + 12] = len(handlers)
        m[entry + 16] = h
        for c, addr in enumerate(handlers):
            m[h + 16 * c + 12] = addr
    return m


class FuncInfo(unittest.TestCase):
    def test_unwind_actions_and_catch_handlers(self):
        u, c = funcs.parse_funcinfo(memory(funcinfo(0x1000, [0x500, 0x510], [[0x520, 0x530]])), 0x1000)
        self.assertEqual(u, {0x500, 0x510})
        self.assertEqual(c, {0x520, 0x530})

    def test_null_actions_are_not_functions(self):  # a state with nothing to destroy has action 0
        u, _ = funcs.parse_funcinfo(memory(funcinfo(0x1000, [0, 0x510])), 0x1000)
        self.assertEqual(u, {0x510})

    def test_bad_magic(self):
        m = funcinfo(0x1000, [0x500])
        m[0x1000] = 0x12345678
        with self.assertRaises(ValueError):
            funcs.parse_funcinfo(memory(m), 0x1000)

    def test_implausible_counts(self):
        m = funcinfo(0x1000, [0x500])
        m[0x1004] = 0x7FFFFFFF
        with self.assertRaises(ValueError):
            funcs.parse_funcinfo(memory(m), 0x1000)

    def test_thunk_scan_requires_the_magic(self):
        good = b"\xB8" + struct.pack("<I", 0x1000) + b"\xE9\0\0\0\0"
        wrong_target = b"\xB8" + struct.pack("<I", 0x2000) + b"\xE9\0\0\0\0"
        unmapped = b"\xB8" + struct.pack("<I", 0x3000) + b"\xE9\0\0\0\0"
        not_a_jump = b"\xB8" + struct.pack("<I", 0x1000) + b"\x90\0\0\0\0"
        data = wrong_target + unmapped + not_a_jump + good
        hits = funcs.find_eh_thunks(data, 0x100, memory({0x1000: MAGIC, 0x2000: 0}))
        self.assertEqual(hits, [(0x100 + 30, 0x1000)])


class Kind(unittest.TestCase):
    def test_direct_evidence_wins(self):
        self.assertEqual(funcs.function_kind(["call", "eh_unwind"]), "source")
        self.assertEqual(funcs.function_kind(["eh_unwind", "ptr"]), "eh_unwind")
        self.assertEqual(funcs.function_kind(["eh_catch", "eh_unwind"]), "eh_catch")
        self.assertEqual(funcs.function_kind(["gap"]), "source")


INV = os.path.join(ROOT, "analysis-local", "inv", "functions.json")


@unittest.skipUnless(os.path.exists(INV), "local inventory not generated")
class RealInventory(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(INV) as f:
            cls.by_va = {int(r["va"], 16): r for r in json.load(f)}

    def test_tail_call_is_not_absorbed(self):  # regression: 0x00013380 ends with `jmp 0x0001F020`
        self.assertEqual(int(self.by_va[0x13380]["max_end"], 16), 0x133A2)
        self.assertIn(0x1F020, self.by_va)

    def test_known_unwind_funclet(self):  # FuncInfo 0x002A279C of ~GmLinkedList's frame lists 0x001F8890
        self.assertEqual(self.by_va[0x1F8890]["kind"], "eh_unwind")

    def test_known_handler_thunk(self):
        self.assertEqual(self.by_va[0x1F8898]["kind"], "eh_thunk")

    def test_recovered_functions_stay_source(self):
        for va in (0x76DD0, 0x77060, 0x76E40, 0x85375):
            self.assertEqual(self.by_va[va]["kind"], "source", hex(va))


if __name__ == "__main__":
    unittest.main()
