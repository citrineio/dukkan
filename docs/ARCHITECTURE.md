# Current architecture — Sprint 5 / v0.4.0

Dukkan is a Qt Quick storefront client with a separate Python user service owning
accepted Flatpak operations. The source is independent of Bazaar. It uses the
existing user Flatpak installation, shared Zaman catalog and lifecycle helpers.

## Client

`Main.qml` provides Discover, Library, game details, gallery, confirmations and
Activity. `StoreBackend` reads metadata through Qt Network, performs local discovery
with asynchronous QProcesses, and exchanges bounded newline-delimited JSON with
`tools/transactions.py` over a private Unix socket. The UI does not own a modifying
Flatpak process. SDL2 optionally provides gamepad actions for the active window.

Ordinary launches open Discover. Explicit transaction links open Activity. Recently
handled navigation nonces are atomically persisted in a private client marker so
an old service snapshot does not reopen Activity on a new launch.

## Worker and durable state

The independent `dukkan-transactions.service` owns a SQLite queue, stable job and
request IDs, one active worker, recovery, terminal-result delivery and separate
library-publication retry. Closing the UI leaves accepted work running. The
service reconciles uncertain work against actual Flatpak state rather than trusting
exit status alone. Uninstall preserves application data.

The relay and managed-game helper retain the same exclusive transaction/lifetime
lock. Session lifecycle inspection fails closed. This sprint does not enable
parallel runtime modification during managed gameplay or change Guide ownership.

## Integration boundaries

- The shared catalog authorizes enabled application IDs. HTTP metadata provides
  descriptions and artwork; it does not authorize arbitrary packages.
- Sessiond owns installed-game/storefront lifecycle and delegates Pegasus sync to
  the accepted library helper. Dukkan does not rewrite Pegasus configuration.
- Zaman Menu owns the notification server, durable receipt watermark, history,
  DND, toast presentation, live activity and validated transaction handoff.
- Successful results use verified postconditions; progress is an observed current
  Flatpak step and may remain indeterminate. The displayed app size is not a full
  runtime-inclusive transaction estimate.

The complete protocol, storage paths, recovery and validation boundaries are in
[SPRINT5.md](SPRINT5.md). The earlier decision log is retained as
[historical v0.3 architecture](history/architecture-v0.3.md); it is not the current
feature inventory. Public catalog search, payments, publisher self-service,
compatibility grading and structured libflatpak transactions remain future work.
