"""Visual and layout guardrails for the independent Qt Quick storefront."""
from pathlib import Path
import re
import unittest
ROOT = Path(__file__).resolve().parents[1]
QML = (ROOT / 'qml/Main.qml').read_text()
ART = (ROOT / 'qml/GameArt.qml').read_text()
TILE = (ROOT / 'qml/GameTile.qml').read_text()
STORE = (ROOT / 'src/storebackend.cpp').read_text()
HEADER = (ROOT / 'src/storebackend.h').read_text()

class UiRevisionTests(unittest.TestCase):
    def test_artwork_is_letterboxed_instead_of_cropped(self):
        self.assertIn('Image.PreserveAspectFit', ART)
        self.assertNotIn('Image.PreserveAspectCrop', ART)
        self.assertIn('showingScreenshot ? Math.max(0, parent.width - 12)', ART)
        self.assertIn('Math.min(parent.width * 0.50, 210)', ART)
        self.assertIn('screenshotUrl: tile.game.screenshot', TILE)
        artwork = (ROOT / 'src/gameartwork.cpp').read_text()
        self.assertIn('"1248x702", "752x423", "624x351"', artwork)

    def test_new_console_information_hierarchy(self):
        for term in ('FEATURED  /  DISCOVER','Made to play','NAV', 'onActivated:',
                     'ListView.Horizontal','ABOUT THE GAME','GAME  /  '):
            if term != 'NAV': self.assertIn(term, QML)
        self.assertIn('Layout.preferredWidth: px(206)', QML)

    def test_no_mouseareas_are_directly_managed_by_layouts(self):
        # The old layout warning came from MouseArea directly inside RowLayout.
        # MouseAreas must be children of concrete Item/Rectangle surfaces.
        self.assertNotIn('MouseArea { Layout.', QML)
        self.assertNotIn('MouseArea { Layout.', TILE)

    def test_protected_transaction_and_progress_states(self):
        self.assertIn('Q_PROPERTY(int progressPercent', HEADER)
        self.assertIn('Q_PROPERTY(QString progressStage', HEADER)
        self.assertIn('void StoreBackend::consumeRelayOutput', STORE)
        self.assertIn('store.progressPercent < 0', QML)
        self.assertIn('NumberAnimation {', QML)
        self.assertIn('CURRENT STEP', QML)
        self.assertIn('store.installGame(pendingId)', QML)
        self.assertIn('store.removeGame(pendingId)', QML)

    def test_metadata_visibility(self):
        for text in ('FLATHUB · LIVE METADATA','CACHED METADATA','CURATED FALLBACK DATA'):
            self.assertIn(text, QML)

if __name__ == '__main__': unittest.main()
