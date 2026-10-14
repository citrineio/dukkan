# Architecture and v0.3 decision log

## Actual v0.2 boundaries

```text
QML (Main.qml)
  ⇅ context property (StoreBackend)
C++ Qt backend
  ├── QtNetwork: Flathub AppStream public HTTP endpoint
  ├── asynchronous QProcess: `flatpak list`, `remote-ls`, `install`, `uninstall`
  └── local JSON cache of metadata
QML controller events ← optional SDL2 GameController bridge
```

There is **no Bazaar code** in this package. There is **no Flatpak SDK** in the build. All installations happen into the already-configured **user** Flatpak installation.

## Catalog trust

The four seed game identifiers are hard-coded in `data/games.json`. Application availability must also be confirmed by `flatpak remote-ls` on the actual target host (aarch64). HTTP API JSON is used for descriptions and art, not to authorize arbitrary installs. Metadata URLs for artwork are limited to HTTPS Flathub hosts.

No full catalog enumeration or dynamic public search is implemented. Search is a client-side filter of four curated games. Remote availability is determined by CLI, not by the HTTP API.

## UX / controller integration

The SDL2 bridge receives GameController events and emits QML actions only if Dukkan owns the active window. This helps prevent background controller events driving a hidden store window. A physical controller is not yet tested. The Qt keyboard path works regardless of SDL2 availability. Guide is intentionally not bound.

**Do not embed the storefront in the Quickshell menu** at this stage. A dedicated fullscreen Qt Quick window is more maintainable; Quickshell's role is system overlays. We can share theme tokens with the Zaman menu and Pegasus once an appearance schema exists.

## Important omissions

**Deferred by Kadhem on 14 October 2026:** the complete Flatpak transaction
download estimate and the multi-screenshot detail gallery are future-release
work. The current Sprint 2 closeout does not depend on either feature.

1. Dukkan shows the app ref download size reported by Flathub when available. This is not a full transaction estimate: required runtimes and extensions can add substantial downloads. A preflight that totals the complete Flatpak transaction remains open.
2. Flatpak transactions still use a CLI relay rather than a structured Flatpak API.
   Sprint 2 reports observed per-operation progress, or an indeterminate state
   when Flatpak does not provide a percentage.
3. No automatic catalog export or library sync into Pegasus. Curated installed-game
   launch through `zaman-sessiond` was added in Sprint 2.
4. No application-permission explanation or security warning screen beyond install confirmation.
5. No transactional retry queue or multiple concurrent installs.
6. No verified compatibility grading (graphics, controls, performance).
7. No dynamic theme packages.
8. Multiple screenshots per game are deferred. The current catalog selects only the first usable Flathub screenshot (or an icon fallback). A future revision should show a navigable gallery on the detail page, preserve the source aspect ratio, and handle missing/offline images without blocking installation.

## Recommended v0.3 sequence

- Validate QtWayland rendering, Sway fullscreen, and InputPlumber → SDL controller mapping on Dragon Q6A.
- Add Flatpak-specific sessiond launch targets, ensure graceful exit, focus, menu pause/guide behavior and return to Pegasus.
- Implement a proper public Flathub catalog search indexed by real aarch64 availability, local AppStream for offline browsing, and curation rules for non-game apps.
- Move installer operations into a worker/service using libflatpak APIs; provide structured progress, cancel, queued transactions, disk use, and runtime permissions.
- Introduce a shared Zaman theme token format for Quickshell/Pegasus/Dukkan.

## C++ developer notes

Qt6 required: Core, Gui, Quick, QuickControls2, Network. Optional: SDL2 development package. `qt_add_resources` embeds the QML and JSON into `qrc:/` to allow running from arbitrary paths. `cmake --build build -j2` limits memory usage on the SBC.

Build has not been executed by the author because Qt6 dev headers/tools are absent in the artifact-generation container. Treat the first compile/run on Zaman as an integration checkpoint; please share the first error if it fails.
