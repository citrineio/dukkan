# Sprint 3.5 follow-up: one live progress surface

Applies on top of the accepted Sprint 3.5 gallery candidate, not the older main
branch. No sessiond/menu change or package installation is needed.

- Flatpak's Unicode/ASCII terminal bar, duplicate percent and trailing transfer
  statistics are removed from the phase label. Real numeric and OSC progress
  still drive the UI; split reads and step resets remain supported.
- The catalog and details use `TransactionProgress.qml` and `ProgressTrack.qml`.
  The catalog surface is hidden while details is open. The detail surface replaces
  the disabled "Transaction in progress" action row, shows the active game's
  name and phase, and follows the same live percent as the catalog.
- Unknown progress remains indeterminate; displayed percent is explicitly the
  current Flatpak step. No timer-based invented completion values.
- X/Stop uses the existing confirmation dialog. Browsing another game's details
  still shows the actual active transaction name, and store actions remain
  blocked while a transaction runs. Returning to the catalog preserves progress.

Validation: Python tests exercise real terminal-style bar rows, partial UTF-8,
OSC progress and postcondition verification. The Qt runtime test changes percent
in a fake backend, checks only one visible track in either page, and exercises
Stop confirmation and indeterminate state. The Qt tests must be compiled/run on
the device or CI; the authoring workspace does not contain Qt.

Close Dukkan before rebuilding its currently selected candidate binary. After
building, relaunch it through `zamanctl storefront launch dukkan`; no launcher
configuration change is required for this follow-up.
