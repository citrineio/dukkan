# Sprint 3.5: game gallery and identity

This changes Dukkan's detail page only. The existing Play/Install and confirmed
Uninstall flows remain in the information column, together with the game name
and title artwork (application icon fallback).

## Controls

| Input | Detail page | Fullscreen image |
| --- | --- | --- |
| L1 / R1 | Previous / next image, wrapping | Previous / next image, wrapping |
| Y | Open current image fullscreen | No store action |
| B | Return to catalog | Return to the same detail image/action |
| Left / right | Choose Play/Install or Uninstall | No store action |
| Up / down | Scroll description | No store action |
| A | Activate selected store action | No store action |
| Page Up / Page Down | Previous / next image | Previous / next image |
| Space / Escape | Open image / return to catalog | No action / return to details |

Mouse users can select thumbnails, click the main image to enlarge, and use
visible Previous/Next/Back controls. No automatic slideshow or video playback.
Guide remains owned by the existing session stack.

## Artwork

Flathub's current `screenshots[].sizes[]` schema is supported, including numeric
or string dimensions. One entry per screenshot exposes detail, thumbnail and
full-resolution URLs plus a caption. Older dimension-key cache entries still
work. Unsafe hosts, duplicate images and empty entries are excluded. There is a
24-image limit, asynchronous loading and bounded decoding. Missing/failed images
show a placeholder while other images remain navigable; zero images disable the
viewer and a single image disables Previous/Next.

Title art is discovered locally, in this order:

1. `$XDG_DATA_HOME/zaman/game-artwork/<app-id>/logo.png` (default data root:
   `~/.local/share`). This also permits title art before installation.
2. Known title images within the user's installed Flatpak
   `app/<app-id>/aarch64/stable/active/files/` tree (SuperTux, SuperTuxKart, Wesnoth).
3. Exported PNG application icons, largest available standard size first.
   Dukkan can also use the Flathub icon when no installed icon is present.

Both Dukkan and the accompanying sessiond change use this title-art convention.
Artwork is read, not extracted by launching the game. No third-party logo scraper,
new artwork service, or generated game logos are introduced. PNG files keep this
independent of optional Qt SVG plugins. Press F5 in Dukkan and run
`zamanctl library sync` after adding/replacing an override.

Source paths were checked against the upstream SuperTux v0.7.0 data and Flathub
install prefix, SuperTuxKart CMake install rules and distributed `logo_slim.png`,
and Wesnoth's SCons data install rules. Missing package paths fall back to icons;
this is not a claim of device inspection.

## Validation

Local source/contract tests, executable QML state-function tests, and the Flatpak
transaction relay tests run without changing the device. CI compiles Qt/SDL,
tests the real C++ metadata parser, and loads the whole QML application with an
isolated fake backend. The latter exercises fullscreen action isolation, B return,
and captures 1080p, fullscreen, and 720p layouts.

Device acceptance still requires a real controller and the installed Qt stack:

1. Open SuperTuxKart details; verify distinct screenshots, thumbnails and count.
2. L1/R1 wrap without changing Play/Uninstall selection or description position.
3. Y opens fullscreen; L1/R1 still work. A/X/Y/Start must not launch, uninstall,
   refresh or leave the page. B returns to details; B again returns to catalog.
4. Check title-art/icon fallback, a missing image, one image and an offline reopen.
   Metadata is cached; screenshot bytes still require network or Qt's image cache.
5. With the sessiond companion applied, run `zamanctl library sync` and check
   the native entries in the **existing** Pegasus theme. Its full card components
   are not present in these repositories; the sync supplies `logo`, `boxFront`
   and `tile` roles to cover normal theme choices.

Build/test with the existing Qt development environment:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

The runtime QML test requires the same Quick/Controls/Layouts QML modules as the
application. Nothing is installed or activated by these commands.
