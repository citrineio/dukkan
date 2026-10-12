"""Static checks for failures identified on a real Zaman OS test."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
QML = (ROOT / "qml/Main.qml").read_text()
CPP = (ROOT / "src/storebackend.cpp").read_text()

class StartupRegressionTests(unittest.TestCase):
    def test_qml_text_letter_spacing_is_font_property(self):
        # Main.qml:338 used `letterSpacing: 1.2` directly on a Text item.
        self.assertNotRegex(QML, r"(?<!font\.)\bletterSpacing\s*:")
        self.assertIn("font.letterSpacing: 1.2", QML)

    def test_all_flatpak_child_processes_cleaned_up(self):
        self.assertIn("findChildren<QProcess *>()", CPP)
        self.assertIn("QObject::disconnect(process, nullptr, this, nullptr)", CPP)
        self.assertIn("process->terminate()", CPP)
        self.assertIn("process->kill()", CPP)
        self.assertIn("process->waitForFinished(1200)", CPP)

if __name__ == "__main__":
    unittest.main()
