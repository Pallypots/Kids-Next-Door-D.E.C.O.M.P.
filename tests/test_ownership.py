"""tools/ownership.py: the classification rules, on functions made up here."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
import ownership  # noqa: E402


def fn(va, section=".text", kind="source", slots=None, size=0x10):
    f = {"va": "0x%08X" % va, "section": section, "kind": kind, "max_end": "0x%08X" % (va + size)}
    if slots:
        f["vtable_slots"] = slots
    return f


def run(functions, strings=None, recovered=(), library=(), hints=None):
    return ownership.classify(functions, strings or {}, set(recovered), set(library), hints or {})


class ClassifyTests(unittest.TestCase):
    def test_each_kind_of_evidence(self):
        r = run([fn(0x100, section="DSOUND"), fn(0x200), fn(0x300, slots=["FaThing[1]@+0"]), fn(0x400), fn(0x500), fn(0x600)],
                strings={"0x00000400": "crt"}, recovered=[0x200], library=[0x500], hints={0x600: "xdk"})
        self.assertEqual(r["0x00000100"][:2], ("xdk", "direct"))
        self.assertEqual(r["0x00000200"][:2], ("game", "direct"))
        self.assertEqual(r["0x00000300"][:2], ("game", "direct"))
        self.assertEqual(r["0x00000400"][:2], ("crt", "direct"))
        self.assertEqual(r["0x00000500"][:2], ("crt", "direct"))
        self.assertEqual(r["0x00000600"][:2], ("xdk", "hint"))

    def test_foreign_rtti_class_is_not_evidence(self):
        self.assertEqual(run([fn(0x100, slots=["type_info[0]@+0"])])["0x00000100"][:2], ("unknown", "none"))

    def test_only_source_functions_are_classified(self):
        self.assertNotIn("0x00000100", run([fn(0x100, kind="eh_unwind")]))

    def test_gap_between_agreeing_neighbours(self):
        r = run([fn(0x100), fn(0x200), fn(0x300)], recovered=[0x100, 0x300])
        self.assertEqual(r["0x00000200"][:2], ("game", "inferred"))

    def test_gap_between_disagreeing_neighbours_stays_unknown(self):
        r = run([fn(0x100), fn(0x200), fn(0x300)], strings={"0x00000100": "game", "0x00000300": "crt"})
        self.assertEqual(r["0x00000200"][:2], ("unknown", "none"))

    def test_edges_stay_unknown(self):
        r = run([fn(0x100), fn(0x200), fn(0x300)], recovered=[0x200])
        self.assertEqual(r["0x00000100"][1], "none")
        self.assertEqual(r["0x00000300"][1], "none")

    def test_conflict_is_unknown_and_breaks_a_run(self):
        r = run([fn(0x100), fn(0x200), fn(0x300), fn(0x400)], strings={"0x00000100": "game", "0x00000300": "crt", "0x00000400": "game"}, recovered=[0x300])
        self.assertEqual(r["0x00000300"][:2], ("unknown", "conflict"))
        self.assertEqual(r["0x00000200"][1], "none")

    def test_hint_gives_way_to_direct_evidence(self):
        self.assertEqual(run([fn(0x100)], recovered=[0x100], hints={0x100: "xdk"})["0x00000100"][:2], ("game", "direct"))

    def test_run_does_not_cross_sections(self):
        r = run([fn(0x100), fn(0x200, section="OTHER"), fn(0x300)], recovered=[0x100, 0x300])
        self.assertEqual(r["0x00000200"][:2], ("unknown", "none"))

    def test_string_rules(self):
        self.assertEqual(ownership.tag_for_string("GetStringByNameCallback"), "game")
        self.assertEqual(ownership.tag_for_string("Design\\Registry\\"), "game")
        self.assertEqual(ownership.tag_for_string("XBox GamePad Controller"), "game")
        self.assertIsNone(ownership.tag_for_string("Callback at the start"))
        self.assertIsNone(ownership.tag_for_string("Error"))

    def test_bytes_are_unique(self):
        self.assertEqual(ownership.unique_bytes([(0, 10), (5, 15), (20, 30), (22, 25)]), 25)


if __name__ == "__main__":
    unittest.main()
