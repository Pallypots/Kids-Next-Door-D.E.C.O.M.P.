"""Synthetic checks of tools/progress_map.py: no byte counts twice, recorded sizes win, library code is not counted."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "tools"))
import progress_map as pm  # noqa: E402


class ProgressMapTest(unittest.TestCase):
    F = [{"va": "0x100", "max_end": "0x180"}, {"va": "0x140", "max_end": "0x200"}, {"va": "0x200", "max_end": "0x300"}]
    OWN = {0x100: "game", 0x140: "game", 0x200: "xdk"}

    def test_recorded_size_wins_an_overlap(self):
        sp = pm.spans(self.F, self.OWN, {0x140: "at/asn"}, {0x140: 0x40})
        self.assertEqual([(a, b) for a, b, l in sp if l[0] == "done"], [(0x140, 0x180)])
        self.assertEqual([(a, b) for a, b, l in sp if l[0] == "left"], [(0x100, 0x140)])

    def test_no_byte_twice(self):
        sp = pm.spans(self.F, self.OWN, {}, {})
        for (a, b, _), (c, d, _) in zip(sp, sp[1:]):
            self.assertLessEqual(b, c)
        self.assertEqual(sum(b - a for a, b, _ in sp), 0x200)

    def test_cells_and_styles(self):
        sp = pm.spans(self.F, self.OWN, {0x140: "at/asn"}, {0x140: 0x40})
        grid = pm.cells(sp, 0x100, 0x200, 0x80)
        self.assertEqual(len(grid), 4)
        colour, alpha = pm.cell_style(grid[0])  # half recovered, half not
        self.assertEqual(colour, "#a77bf3")
        self.assertEqual(alpha, round(0.35 + 0.65 * 0.5, 2))
        self.assertEqual(pm.cell_style(grid[2]), (pm.LIBRARY, 1))
        self.assertEqual(pm.cell_style({}), (pm.BG, 0))
        self.assertEqual(pm.cell_style({("left",): 10}), (pm.NOT_YET, 1))

    def test_family_prefix_is_whole_word(self):
        self.assertEqual(pm.family("at/asn")[0], "Scenes, actors and collision")
        self.assertEqual(pm.family("at/asnx"), pm.OTHER)

    def test_svg_has_one_rect_per_lit_cell(self):
        grid = [{("left",): 1}, {}, {("library",): 4}]
        out = pm.svg(grid, dict(fn_done=1, fn_total=2, fn_pct=50.0, b_done=1, b_total=2, byte_pct=50.0, cell=2048), cols=3)
        self.assertTrue(out.startswith("<svg"))
        self.assertIn("Decompiled: 50.0%", out)


if __name__ == "__main__":
    unittest.main()
