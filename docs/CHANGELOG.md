# Changelog

This repository begins with the experimental **v0.3.1** code snapshot.

## v0.3.1 — Initial public source snapshot (prototype)

- Independent Qt Quick storefront using Flathub metadata and user Flatpak installations.
- Four-game curated AArch64 catalog, game details, search keyboard, installation and removal confirmation.
- Redesigned TV/console layout with Zaman-inspired styling and reusable QML components.
- Hotfix for QML `font.letterSpacing` startup issue.
- Improved shutdown cleanup for in-flight Flatpak subprocesses.

**Known limitations:** no production-ready session management, incomplete controller/UI polish, no generalized catalog search, and no persistent download service. See `docs/ARCHITECTURE.md`.
