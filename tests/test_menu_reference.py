"""Zaman palette and theme semantics from the supplied patch archive."""
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
QML = (ROOT / 'qml/Main.qml').read_text()
TILE = (ROOT / 'qml/GameTile.qml').read_text()
THEME = (ROOT / 'src/appearanceprovider.cpp').read_text()
REFERENCE = json.loads((ROOT / 'data/appearance.example.json').read_text())

class MenuReferenceTests(unittest.TestCase):
    def test_qt_controls_palette_collision_is_avoided(self):
        self.assertNotIn('id: palette', QML)
        self.assertNotIn('palette.', QML)
        self.assertIn('id: zamanTheme', QML)
        self.assertIn('appearance.tokens || ({})', QML)

    def test_menu_colors_present(self):
        for key, color in [('accent','#EAC28D'), ('text','#F0E5D6'),
                           ('muted','#BBC6D3'),('surface','#171D28')]:
            self.assertEqual(REFERENCE['colors'][key], color)
            self.assertIn(color, THEME)

    def test_single_active_catalog_focus(self):
        self.assertIn('root.focusArea === "catalog" && !root.detailsOpen', QML)
        self.assertIn('chosen: root.selectedIndex === index', QML)
        self.assertIn('border.color: chosen ? theme.accent', TILE)
        self.assertIn('focusArea = "dialog"', QML)
        self.assertIn('focusArea = "keyboard"', QML)

if __name__ == '__main__': unittest.main()
