"""Controller-state regressions for the v0.3 console layout (static, not runtime)."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
QML = (ROOT / 'qml/Main.qml').read_text()

class ControllerFlowTests(unittest.TestCase):
    def test_fullscreen_without_global_compositor_changes(self):
        self.assertIn('visibility: Window.FullScreen', QML)
        self.assertIn('sequence: "F11"', QML)
        self.assertNotIn('swaymsg', QML)

    def test_card_selection_then_details_action(self):
        self.assertIn('else if (action === "select") openDetails()', QML)
        self.assertIn('else requestGameAction()', QML)
        self.assertIn('function requestGameAction()', QML)
        self.assertIn('store.availabilityChecked', QML)

    def test_dialog_is_safe_by_default_and_b_dismisses(self):
        self.assertIn('property int modalChoice: 0', QML)
        self.assertIn('if (modalChoice === 1)', QML)
        self.assertIn('if (action === "left" || action === "up") modalChoice = 0', QML)
        self.assertIn('else if (action === "right" || action === "down") modalChoice = 1', QML)
        self.assertIn('else if (action === "back") dismissModal()', QML)
        self.assertIn('else if (modal === "cancel") store.cancelJob(pendingJobId)', QML)
        self.assertIn('"Keep running"', QML)

    def test_b_from_discover_leaves_background_transactions_running(self):
        self.assertIn('function leaveStore()', QML)
        leave = QML.split('function leaveStore()', 1)[1].split('function changeSection', 1)[0]
        self.assertNotIn('requestCancel', leave)
        self.assertIn('root.close()', leave)
        self.assertIn('else leaveStore()', QML)
        self.assertIn('root.close()', QML)
        self.assertIn('sequence: "Ctrl+Q"', QML)

    def test_controller_keyboard_and_navigation_rail(self):
        for code in ('function openKeyboard()', 'function closeKeyboard()',
                     'function keyActivate(label)', 'function navigateShelf(direction)',
                     'focusArea = "nav"', '"SPACE","CLEAR","DONE"'):
            self.assertIn(code, QML)
        self.assertIn('["Q","W","E","R","T","Y","U","I","O","P"]', QML)
        self.assertIn('function cycleKeyboardView()', QML)
        self.assertIn('else if (action === "library") cycleKeyboardView()', QML)
        self.assertIn('["!","@","#","$","%","^","&","*","(",")"]', QML)
        self.assertIn('focusArea = "hero"', QML)
        self.assertIn('"START"; label: "Search"', QML)
        self.assertIn('"Y"; label: "Library"', QML)

    def test_controller_guide_not_stolen(self):
        pad = (ROOT / 'src/gamepadbridge.cpp').read_text()
        self.assertNotIn('SDL_CONTROLLER_BUTTON_GUIDE', pad)

if __name__ == '__main__': unittest.main()
