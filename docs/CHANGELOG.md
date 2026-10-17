# Changelog

This repository begins with the experimental **v0.3.1** code snapshot.

## v0.4.0 — Sprint 5 background transactions

- Independent user service with a durable single-worker queue, request IDs,
  cancellation, crash/reboot reconciliation and verified install/remove results.
- Closing or returning Home leaves accepted work running; the UI reconnects.
- Ordered terminal notifications use durable ACK/retry delivery through Zaman Menu.
  Progress remains a live view and is not appended to notification history.
- Preserve the shared catalog, game/transaction lifetime lock and Pegasus refresh;
  uninstall retains application saves.
- Add Activity for queue/results and transaction-notification navigation. Normal
  launches stay on Discover; handled request nonces survive UI restarts.
- Embed approved main branding once; paired menu sources include the supplied
  Dukkan sign, vector-backed Steam emblem and empty-notification rendering fix.

## v0.3.2 — Sprint 3 Pegasus library sync candidate

- Request `zamanctl library sync` after a confirmed Flatpak transaction reaches
  its exact installed-ref postcondition.
- Report a library-sync failure separately from transaction success; the CLI
  repair command is safe to retry.
- With the paired Zaman Pegasus theme watcher, installed-game changes appear
  without a manual game-list reload or display-session restart.

## v0.3.1 — Initial public source snapshot (prototype)

- Independent Qt Quick storefront using Flathub metadata and user Flatpak installations.
- Four-game curated AArch64 catalog, game details, search keyboard, installation and removal confirmation.
- Redesigned TV/console layout with Zaman-inspired styling and reusable QML components.
- Hotfix for QML `font.letterSpacing` startup issue.
- Improved shutdown cleanup for in-flight Flatpak subprocesses.

**Known limitations:** no production-ready session management, incomplete controller/UI polish, no generalized catalog search, and no persistent download service. See `docs/ARCHITECTURE.md`.
