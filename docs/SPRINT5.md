# Sprint 5 — background transactions

The storefront is now a client. `tools/transactions.py serve` is an independent
user service; it is not a child of the Dukkan window and does not share its unit.
Python 3's standard library, SQLite, existing Flatpak and existing session helpers
are sufficient. There are no new package dependencies.

## State and API

Socket: `$XDG_RUNTIME_DIR/dukkan-transactions/control.sock`, private to the user.
Requests/replies are newline-delimited JSON. Requests are bounded to 8192 bytes.
Only same-UID peers are accepted. No API field contains executable commands.

Actions: `snapshot`, `submit` (`app`, `operation`, UUID-hex `request`), `get`,
`cancel`, `retry` (new `request`), and `navigate` (all use stable UUID-hex `id`).
The CLI exposes the same actions. Operations are `install` and `uninstall`.

Example diagnostics (read-only):

```sh
python3 tools/transactions.py snapshot
python3 tools/transactions.py get TRANSACTION_UUID_HEX
```

The SQLite database is `$XDG_STATE_HOME/dukkan/transactions.sqlite3`, defaulting
to `~/.local/state/dukkan/transactions.sqlite3`. WAL and FULL synchronization
commit acceptance before replying. Keep the database and its WAL/SHM together;
never delete or copy a live database piecemeal. It holds app, operation, phase,
progress, result, retry ancestry, cancellation and delivery/library-publication
state. One modifying worker runs at a time, with at most 32 pending jobs.
Snapshots include pending work plus 64 recent terminal jobs; durable request IDs
and older jobs remain queryable. An explicitly navigated older job is included.

Phases: queued → running → succeeded/failed/cancelled/interrupted. A blocked
worker returns to waiting. Cancellation is a persisted request; a verified commit
that won the cancellation race is reported as success. Retry creates a new job,
retaining the old result. Repeated request IDs and duplicate pending operations
do not enqueue another transaction; conflicting queued operations are rejected.

## Recovery and lifecycle audit

Before creating a child, its running phase is committed. After a worker failure
or service restart, started work is reconciled under the existing transaction
lock against the exact user `app/ID/aarch64/stable` ref. A satisfied postcondition
is recovered as success. Otherwise the result is interrupted and requires an
explicit retry. Unstarted queued jobs continue normally. Do not infer cancellation
or success solely from process exit codes or a vanished process.

The accepted managed-game helper and the transaction relay use the **same global
exclusive lock**, `$XDG_RUNTIME_DIR/zaman-installed-game/transaction.lock`.
The worker preserves this conservative policy; it does not introduce per-app
locks or enable parallel runtime modification during managed gameplay. The relay
also checks sessiond's installed-game phase (including CleanupFailed), emulator
state, and running Flatpak instances while holding the lock. Uncertain lifecycle
inspection fails closed. Flatpak receives an inherited lock FD, preserving the
lock if its relay dies before Flatpak exits. External applications that bypass
Zaman are not coordinated by its lifecycle protocol; Flatpak's own locking still
applies. The `flatpak ps` check is an additional snapshot, not an atomic lock for
unmanaged launches.

Installed helpers on the inspected device match the published sessiond sources:

| Helper | SHA-256 |
| --- | --- |
| zaman_catalog.py | 4da01672a5f3e9dd2008f5d4658b19a4410255f0c4c4cadc8ff76cf3d57305cd |
| flatpak-game-session.py | 8e107210d8977245fb8b30543f5f3c10a71a5c8ebbf30457814ead3824cd700b |
| pegasus-library-sync.py | b6a24a947b75d73b6d39f8c727269d98435b8d48502a0a7b2d04bc13490a1530 |

No sessiond, Guide/input-helper, PS2 launcher/profile or Pegasus theme code changes
are required. The worker delegates library publication to the accepted
`/usr/local/bin/zamanctl library sync` path. Publication is separately retried
under the transaction lock and does not turn a verified install into a failure.
The Activity view exposes publication errors. Uninstall omits `--delete-data`.

## Notifications and navigation

Terminal result and its ordered outbox sequence commit together. The worker
delivers keys `transaction:DATABASE_UUID:SEQUENCE` in order and requires the menu
server's matching `acceptedEvent` acknowledgement. The notification server saves
the record and a per-database sequence watermark atomically before acknowledging.
Lost acknowledgements, history clear, expiry, dismissal and server restart do not
produce another terminal record. Routine progress never enters history.

Successful installs use `state=installed` and muted green. Removal is neutral;
failure/interruption uses error; cancellation is neutral. Existing DND and close
behavior remain. Receipt watermarks support 64 database identities; one normal
installation uses one identity indefinitely. Corrupt history blocks durable ACKs
instead of silently claiming delivery. The guarantee assumes intact queue/history
storage; manually deleting/restoring either database is not normal recovery.

Typed targets are `dukkan-transaction:UUID_HEX`. Selecting one or an Activity row
uses the existing menu lease-release handoff, then sessiond's managed Dukkan launch
and a validated worker navigation request. Gameplay is never interrupted by this
route; return to the library first. No shell strings, external URLs, new global
controller shortcuts, or Steam Guide overrides are introduced.

## UI and branding

Activity is reachable from the Dukkan rail (and Ctrl+T). It shows queue, progress,
result and publication state; A opens the game, X confirms cancellation, Y confirms
retry when applicable, and B returns. Closing the window leaves accepted jobs
running. The menu has a compact Activity entry/list; its notifications link to the
same transaction. Ordinary launches open Discover. A private, atomic client
navigation marker remembers recently handled request nonces, preventing a retained
worker target from reopening Activity after the client restarts. Fresh clicks use
new nonces and still open their requested transaction. Resuming an existing window
preserves its current view.

The approved full shopfront artwork appears once in the Dukkan sidebar. The menu
uses the owner's supplied Arabic sign PNG, unchanged and filtered at menu sizes.
Steam uses the official emblem Bézier geometry through the existing Qt Quick
Canvas API, repainted at the displayed size with oversampling; no SVG image plugin
is required. Storefront labels stay aligned and focus colors remain contextual.
Notifications displays live queued/running activity without percentage-history
entries, and a proper empty message when both visible history and live work are
empty. Clearing history does not cancel a job.

## Validation boundaries

Offline Python state/recovery/relay tests and actual-source JavaScript navigation
tests are supplied. Qt compilation/CTest, real Unix-socket/private D-Bus tests,
systemd lifecycle and hardware acceptance are owner-run. The authoring environment
does not provide Qt development tools/GIO and disallows Unix listeners. A skipped
socket test is not a device pass.
