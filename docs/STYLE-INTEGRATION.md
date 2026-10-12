# Shared Zaman appearance (v0.2.3)

The current version of the modern Zaman Quickshell menu and Pegasus theme
were **not included** in the Dukkan source package. This release uses the
same *design direction*, not an asserted pixel-perfect copy of those files.

For a precise visual match, extract the current color, corner radius, and
motion values from the actual menu and Pegasus themes and publish them to:

    ~/.config/zaman/appearance.json

`data/appearance.example.json` defines the recognized format. Dukkan loads
this JSON as a **read-only** file, watches for filesystem changes, and falls
back safely to its own internal palette if the file is absent or invalid.

Semantic tokens:

- `background`: application canvas
- `rail`: navigation, artwork scrims, small badges
- `surface`: cards and activity containers
- `elevated`: selected cards, modals, artwork fallbacks
- `accent`: active section and action
- `accentStrong`: bright controller focus border, focus tab
- `installed`: installed-game status
- `text`: primary text
- `muted`: descriptions and secondary text
- `border`: unfocused outlines
- `shape.cornerRadius`: base card radius (4–36 pixels)
- `motion.focusMs`: focus transitions (0–500 ms)

No service, config file, package, or existing Zaman theme is created or
modified by Dukkan. An eventual Zaman Appearance Manager can own this file.
When Pegasus and Quickshell read from the same schema, changes are centralized.

## Controller behavior

- D-pad and stick select one visibly outlined tile.
- A opens details / selected confirmation action.
- B closes a modal, goes back from a details page, goes back from Library,
  or closes Dukkan on the Discover root page.
- X refreshes or requests stopping the current transaction.
- Y switches between Discover and Library.
- Start focuses search.
- Escape mirrors B; Ctrl+Q always exits Dukkan.

B on the Discover root calls Qt `close()` on Dukkan's own window
when no Flatpak transaction is active. While busy, it opens a safe-by-default
stop-request confirmation instead. On the
current standalone prototype, that does not explicitly call `zaman-sessiond`
or request focus on Pegasus. The normal effect when Pegasus remains open is
that the storefront disappears and the existing Sway session is revealed.

The Guide button remains reserved for Zaman's Quickshell system menu. Do not
bind the controller's Guide key in Dukkan.

## Important difference from a global library

QML components cannot be directly shared between a Quickshell process and
another independent QML process as runtime instances. Source components and
appearance tokens can be reused, with compatible Qt imports. A GTK frontend
is no longer involved in Dukkan.
