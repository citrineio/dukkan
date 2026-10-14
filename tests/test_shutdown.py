"""Guard the SDL shutdown path that previously ignored systemd's SIGTERM."""
from pathlib import Path
import unittest


class ShutdownTests(unittest.TestCase):
    def test_quit_event_is_handled_before_controller_focus_guard(self):
        source = (Path(__file__).resolve().parents[1] / "src/gamepadbridge.cpp").read_text()
        poll = source.split("void GamepadBridge::poll()", 1)[1]
        quit_event = poll.index("event.type == SDL_QUIT")
        quit_app = poll.index("QCoreApplication::quit()", quit_event)
        controller_event = poll.index("event.type == SDL_CONTROLLERDEVICEADDED")
        focus_guard = poll.index("!QGuiApplication::focusWindow()")
        self.assertLess(quit_event, quit_app)
        self.assertLess(quit_app, controller_event)
        self.assertLess(quit_app, focus_guard)
        self.assertIn("return;", poll[quit_app:controller_event])


if __name__ == "__main__":
    unittest.main()
