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


class RumbleParseTests(unittest.TestCase):
    TEXT = 'RumbleEffects[ 2 ]\n{\n RumbleEffect "A" // c\n {\n KeyFrames[ 1 ]\n {\n KeyFrame 0.5 { Left 0.25 Right 0.75 }\n }\n }\n RumbleEffect "B"\n {\n KeyFrames[ 0 ]\n {\n }\n }\n}\n'

    def test_effects(self):
        self.assertEqual(levels.parse_rumble(self.TEXT), [("E", "A", "1"), ("F", "0.5", "0.25", "0.75"), ("E", "B", "0")])

    def test_count_must_agree(self):
        with self.assertRaises(ValueError):
            levels.parse_rumble(self.TEXT.replace("KeyFrames[ 1 ]", "KeyFrames[ 2 ]"))


if __name__ == "__main__":
    unittest.main()
