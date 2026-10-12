"""Shared appearance remains a read-only optional integration."""
import json
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]
QML = (ROOT / 'qml/Main.qml').read_text()
APP = (ROOT / 'src/appearanceprovider.cpp').read_text()
MAIN = (ROOT / 'src/main.cpp').read_text()
BUILD = (ROOT / 'CMakeLists.txt').read_text()

class ThemeContractTests(unittest.TestCase):
    def test_read_only_appearance_bridge(self):
        self.assertIn('appearance.tokens', QML)
        self.assertIn('setContextProperty(QStringLiteral("appearance"), &appearance)', MAIN)
        self.assertIn('src/appearanceprovider.cpp', BUILD)
        self.assertIn('/zaman/appearance.json', APP)
        self.assertNotIn('QIODevice::WriteOnly', APP)

    def test_theme_manifest(self):
        theme = json.loads((ROOT / 'data/appearance.example.json').read_text())
        for key in ('background','surface','accent','accentStrong','text','muted','border'):
            self.assertRegex(theme['colors'][key], r'^#[0-9a-fA-F]{6}$')

    def test_reusable_components_are_compiled(self):
        for name in ('GameArt', 'GameTile', 'ControlHint', 'Main'):
            self.assertIn('qml/' + name + '.qml', BUILD)
            self.assertTrue((ROOT / 'qml' / (name + '.qml')).is_file())

if __name__ == '__main__': unittest.main()
