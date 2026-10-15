"""Offline/static contract tests. These do not compile or launch the UI."""
import json
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class PrototypeContractTests(unittest.TestCase):
    def setUp(self):
        self.backend = (ROOT / "src/storebackend.cpp").read_text()
        self.qml = (ROOT / "qml/Main.qml").read_text()

    def test_catalog_is_external_and_reloadable(self):
        loader = (ROOT / "src/gamecatalog.cpp").read_text()
        self.assertIn('/usr/local/share/zaman/catalog.json', loader)
        self.assertIn('ZAMAN_CATALOG_PATH', loader)
        self.assertIn('if (!loadCatalog()) return;', self.backend)
        self.assertFalse((ROOT / "data/games.json").exists())

    def test_actions_revalidate_catalog(self):
        self.assertIn('bool StoreBackend::authorize', self.backend)
        self.assertIn('if (authorize(id)) startTransaction', self.backend)
        self.assertIn('!authorize(id)', self.backend)
        relay = (ROOT / "tools/flatpak-transaction.py").read_text()
        self.assertIn('from zaman_catalog import require_enabled', relay)

    def test_flatpak_uses_user_installation_and_arch_filter(self):
        self.assertIn('QStringLiteral("--user")', self.backend)
        self.assertIn('QStringLiteral("--arch=aarch64")', self.backend)
        self.assertIn('QStringLiteral("--columns=ref:f,download-size")', self.backend)

    def test_transactions_do_not_spawn_a_shell_or_delete_data(self):
        self.assertNotIn("/bin/sh", self.backend)
        self.assertNotIn("bash -c", self.backend)
        self.assertNotIn("--delete-data", self.backend)
        self.assertIn("findGame(id)", self.backend)
        self.assertIn(':/tools/flatpak-transaction.py', self.backend)
        relay = (ROOT / "tools/flatpak-transaction.py").read_text()
        self.assertIn('--assumeyes', relay)

    def test_ui_requires_confirmation_and_checks_remote(self):
        self.assertIn('modal = "install"', self.qml)
        self.assertIn('modal = "remove"', self.qml)
        self.assertIn("store.availabilityChecked", self.qml)
        self.assertIn("store.installGame(pendingId)", self.qml)
        self.assertIn("store.removeGame(pendingId)", self.qml)

    def test_library_sync_is_delegated_without_system_modification(self):
        self.assertNotIn("sudo ", self.backend)
        self.assertNotIn("systemctl", self.backend)
        self.assertNotIn("metadata.pegasus.txt", self.backend)
        self.assertIn('QStringLiteral("library"), QStringLiteral("sync")', self.backend)

    def test_qml_resources_in_build_manifest(self):
        cmake = (ROOT / "CMakeLists.txt").read_text()
        self.assertIn("qml/Main.qml", cmake)
        self.assertNotIn("data/games.json", cmake)
        self.assertIn("src/gamecatalog.cpp", cmake)
        self.assertIn("Qt6::Network", cmake)


if __name__ == "__main__":
    unittest.main()
