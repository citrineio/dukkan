# Dukkan

**Dukkan** is an experimental, controller-first game storefront built for **Zaman OS**. It is a standalone **Qt 6 / Qt Quick (QML)** application that discovers a small curated selection of native Linux games from **Flathub** and installs them using the local **Flatpak CLI**.

> **Status: early prototype (v0.3.1).** This is source code for testing and development, **not** a production-ready game store. Fullscreen session integration, stable controller handling, general Flathub search, background transactions, and game launching through Zaman OS are not finished.

## Current functionality

- Discover and Library views; curated game list (four game identifiers in `data/games.json`).
- Game descriptions and artwork retrieved from Flathub, with a local metadata cache and fallback descriptions.
- Locally installed games recognized through Flatpak.
- AArch64 remote availability checked before installations.
- Flatpak installation and removal **with explicit confirmation**.
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
data/      Curated game catalog and example theme
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

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) and [`docs/CHANGELOG.md`](docs/CHANGELOG.md) for technical details.

## Scope and licensing

Dukkan is independent of Bazaar and **does not include Bazaar source**. Game titles, screenshots, and descriptions obtained at runtime from Flathub are third-party content subject to their respective licenses and terms. This repository does not bundle the game binaries.

Source code in this repository is offered under the [MIT License](LICENSE), copyright 2026 Kawn Electro. Dependencies retain their own licenses.
