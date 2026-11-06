"""tools/levels.py: its parse is the independent side of tests/native/test_gm_levels.cpp, so it needs checks of its own."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
import levels  # noqa: E402


class ParseTests(unittest.TestCase):
    def test_entries(self):
        text = ('// c\r\nLevels [3]\r\n{\r\n\tLevel "M" FMVCinema\r\n\tLevel "A" NormalLevel\r\n\t{\r\n\t\tNumLoadItems 7\r\n\t\tDescription "A: b"\r\n\t}\r\n'
                '\tLevel "B" HiddenLevel { NumLoadItems 8 }\r\n}\r\n')
        self.assertEqual(levels.parse(text), [("M", 3, "", ""), ("A", 0, 7, "A: b"), ("B", 1, 8, "")])

    def test_case_of_keywords(self):
        self.assertEqual(levels.parse('levels [1] { level "x" nonselectablelevel }'), [("x", 2, "", "")])

    def test_count_must_agree(self):
        with self.assertRaises(ValueError):
            levels.parse('Levels [1] { Level "A" NormalLevel Level "B" NormalLevel }')
        with self.assertRaises((ValueError, IndexError)):
            levels.parse('Levels [2] { Level "A" NormalLevel }')

    def test_unknown_field_rejected(self):
        with self.assertRaises(ValueError):
            levels.parse('Levels [1] { Level "A" NormalLevel { Other 1 } }')

    def test_unknown_kind_rejected(self):
        with self.assertRaises(KeyError):
            levels.parse('Levels [1] { Level "A" DesignerCinema }')


if __name__ == "__main__":
    unittest.main()
