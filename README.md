# Dukkan

A controller-operated native Linux game storefront for the
[Zaman retro console](https://zaman.kawnelectro.com/), by
[Kawn Electro](https://kawnelectro.com/).

Dukkan lets players discover, install, launch and remove a curated selection of
games without leaving the console interface. The current prototype uses Flathub
and user Flatpak installations, with architecture checks for Zaman's ARM64 system.

**Status:** early prototype, version 0.3.2. The repository contains the current
Sprint 4 source. It is not a payment platform or an unrestricted Flathub browser.

## Current features

- Discover and Library views, with search across the curated catalog.
- Game descriptions, local title artwork, screenshots and a full-screen gallery.
- Installation/removal confirmations, live transaction progress and cancellation.
- Installed-game launch through Zaman Sessiond.
- Pegasus Native Games synchronization after a verified install or removal.
- Terminal transaction notifications through Zaman Notifications.
- Controller and keyboard navigation, with an on-screen search keyboard.
- A shared JSON catalog and read-only Zaman appearance settings.

Install success requires a verified terminal result. Closing Dukkan does not turn
its current transaction into an independent background download service. General
Flathub search, payments and publisher self-service uploads are not implemented.

## Bring your game to Zaman

We welcome developers and publishers interested in making their games available
on the Zaman retro console.

Contact **Kadhem at [kadhem.alabdulmuhsin@kawnelectro.com](mailto:kadhem.alabdulmuhsin@kawnelectro.com)**,
or [open a game-submission issue](https://github.com/citrineio/dukkan/issues/new?template=game-submission.yml).
Include a game/project link, your contact, Linux/ARM64 availability, controller
support, distribution rights/license, and an existing Flatpak/Flathub app ID if any.

The current distribution path is a curated ARM64 Flatpak catalog. Games without
that package format are still welcome as integration enquiries; submission does
not promise compatibility, storefront inclusion or a commercial publishing deal.
Do not post private keys, unpublished builds or confidential agreement details in
public issues. Email is available for private discussions.

## Build and run

Build requirements are CMake 3.21+, C++17 and Qt 6.4+ with Core, Gui, Quick,
QuickControls2 and Network. SDL2 is optional for native gamepad input. Runtime
installation requires Flatpak and a configured Flathub remote. Zaman integration
requires the matching [session daemon](https://github.com/citrineio/zaman-sessiond).
No dependency installation is performed by these commands.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/dukkan
```

Use `-DDUKKAN_USE_SDL2=OFF` when configuring to disable the optional SDL2 bridge.
Launch within the graphical session so Wayland environment variables are present.
For managed launch and return to Pegasus, follow
[session integration](docs/SESSION-INTEGRATION.md).

## Catalog and artwork

Dukkan reads `/usr/local/share/zaman/catalog.json`, supplied by zaman-sessiond.
`ZAMAN_CATALOG_PATH` may select an absolute development file. Only enabled catalog
IDs are displayed or authorized; invalid catalog data denies new actions.
Flathub supplies descriptions/screenshots, cached locally for subsequent use.
Screenshot bytes still need the network or the image cache.

Local title-art overrides use
`$XDG_DATA_HOME/zaman/game-artwork/<app-id>/logo.png`. The default data root is
`~/.local/share`. See the [gallery notes](docs/SPRINT3.5-GALLERY.md).
The shared catalog schema and editing commands live in
[zaman-sessiond/docs/shared-catalog.md](https://github.com/citrineio/zaman-sessiond/blob/main/docs/shared-catalog.md).

## Controls

| Input | Action |
| --- | --- |
| D-pad / arrows | Navigate |
| A / Enter | Select or confirm the highlighted action |
| B / Escape | Back; leave the catalog from the main page |
| Start | Search keyboard |
| X / F5 | Refresh; X deletes a character during text entry |
| Y | Switch Discover/Library; contextual keyboard/gallery action |
| Shoulders / Page Up and Page Down | Previous/next screenshot in the gallery |
| Ctrl+Q | Quit |

The displayed legend describes the current screen. Destructive confirmations
default to the safe choice. Guide ownership belongs to Zaman's session stack.

## Tests and contributing

```sh
python3 -m unittest discover -s tests -v
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Python checks include source contracts; the C++/QML checks require the existing Qt
build environment. Passing source checks alone does not establish controller or
console compatibility. Report the app ID, architecture, display/controller setup
and reproduction steps with a bug report.

Technical notes: [architecture](docs/ARCHITECTURE.md),
[design system](docs/DESIGN-SYSTEM.md), [changes](docs/CHANGELOG.md), and
[Sprint 4 publication](docs/sprint4-publication.md).

## License

Dukkan's code is [MIT licensed](LICENSE), copyright 2026 Kawn Electro. Game names,
screenshots, descriptions and binaries remain subject to their respective owners' rights and
licenses. This repository does not bundle game binaries or ROMs.
