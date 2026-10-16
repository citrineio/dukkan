> Historical prototype notes. Use the repository root README for current features and setup.

# Dukkan

See [Zaman managed storefront integration](../SESSION-INTEGRATION.md) for the paired source branches and device acceptance.

**Dukkan** is an experimental, controller-first game storefront built for **Zaman OS**. It is a standalone **Qt 6 / Qt Quick (QML)** application that discovers a small curated selection of native Linux games from **Flathub** and installs them using the local **Flatpak CLI**.

> **Status: early prototype (v0.3.2).** Sprint 3 adds Pegasus library sync
> requests after verified installs/removals. The paired Zaman theme refreshes
> Pegasus in place after a changed sync. General Flathub search and
> background transactions remain future work.

## Current functionality

- Discover and Library views; shared curated JSON catalog (`/usr/local/share/zaman/catalog.json`, supplied by zaman-sessiond).
- Game descriptions and artwork retrieved from Flathub, with a local metadata cache and fallback descriptions.
- Locally installed games recognized through Flatpak.
- AArch64 remote availability checked before installations.
- Flatpak installation and removal **with explicit confirmation**.
- Installed-game Play through `zaman-sessiond`, and a Pegasus Native Games sync
  request after a verified install or uninstall.
- Controller navigation via SDL2 when enabled, plus keyboard input and an on-screen search keyboard.
- Dark appearance based on Zaman's graphite, warm-gold, and muted-blue theme. Reads `~/.config/zaman/appearance.json` if present; does not modify it.

Only applications included in the curated catalog can be installed using this prototype. The search feature filters the curated catalog; it is **not** a full Flathub search engine.

## Dependencies

- CMake 3.21 or newer and a C++17 compiler
- Qt 6.4+ with **Core**, **Gui**, **Quick**, **QuickControls2**, and **Network** development components
- `flatpak` CLI and a Flathub remote (for installation and live catalog checks)
- Optional: SDL2 development headers/libraries (native gamepad input)
- A running Wayland session (tested manually with Sway on Zaman; other environments are not guaranteed)

On Debian 13, you can inspect the install impact first:

```bash
apt-get -s install qt6-base-dev qt6-declarative-dev libsdl2-dev
```

Do not install dependencies if they are already available.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
./build/dukkan
```

To disable native SDL2 controller handling:

```bash
cmake -S . -B build -DDUKKAN_USE_SDL2=OFF
```

Dukkan needs the active desktop session's `XDG_RUNTIME_DIR` and `WAYLAND_DISPLAY`. It is preferable to launch from inside the graphical session; launching over SSH may require importing those variables.

## Controls (prototype)

| Control | Typical behavior |
|---|---|
| D-pad | Navigate selections |
| A | Open / accept highlighted action |
| B | Back / cancel; exit from main page |
| Start | Search keyboard |
| X | Refresh / delete character (depending on context) |
| Y | Switch Discover and Library |
| Ctrl+Q | Quit from keyboard |

A Flatpak install/remove confirmation defaults to the **safe** action. The existing Zaman Guide button is not yet managed by Dukkan.

## Testing

```bash
python3 -m unittest discover -s tests -v
```

These Python tests are predominantly **static source/contract checks**. Passing them is not proof that the QML UI compiles, displays correctly, or works with a controller. Build and perform manual integration testing on a Zaman device.

## Repository layout

```text
data/      Example theme (the shared game catalog lives in zaman-sessiond)
qml/       Qt Quick frontend and reusable components
src/       Qt/C++ catalog, Flatpak subprocess and input backend
docs/      Architecture and design notes
tests/     Static regression checks
```

## Development roadmap

1. Integrate Dukkan as a **Storefront** managed by `zaman-sessiond`, alongside Steam; fix fullscreen, focus, Guide ownership and clean return to Pegasus.
2. Improve display scaling, focus styling, controller-first search, and transaction progress.
3. Introduce a full catalog with architecture/compatibility filtering and richer Flathub/AppStream metadata.
4. Move Flatpak transactions into a resilient service with reliable progress, cancellation and notifications.
5. Connect installed games to the Zaman library and launch path.

See [`docs/ARCHITECTURE.md`](../ARCHITECTURE.md) and [`docs/CHANGELOG.md`](../CHANGELOG.md) for technical details.

## Scope and licensing

Dukkan is independent of Bazaar and **does not include Bazaar source**. Game titles, screenshots, and descriptions obtained at runtime from Flathub are third-party content subject to their respective licenses and terms. This repository does not bundle the game binaries.

Source code in this repository is offered under the [MIT License](../../LICENSE), copyright 2026 Kawn Electro. Dependencies retain their own licenses.

## Shared catalog

Dukkan reads `/usr/local/share/zaman/catalog.json` on startup and X/F5 refresh.
`ZAMAN_CATALOG_PATH` can select an absolute development file. Install the shared
reader `zaman_catalog.py` alongside sessiond's helpers in `/usr/local/libexec/zaman`
for transactions; `ZAMAN_CATALOG_LIBEXEC` can select a development helper directory.
No embedded game list is used. Only enabled IDs are displayed or authorized.
Flathub still supplies live artwork and descriptions; optional catalog labels are
offline fallbacks. A malformed/missing catalog displays an error and denies new
actions. Existing gallery and live-progress behavior is retained.

Build/deployment, schema, editing commands and rollback are documented in
`zaman-sessiond/docs/shared-catalog.md` in the companion source checkout.
