# Dukkan v0.3 — frontend design decisions

Design target: 1920×1080 television at sofa distance, with fluid height-based
scaling on other 16:9 displays.

## Hierarchy

1. Navigation rail, subordinate to game content, 206 design px wide.
2. Feature panel, approx. 424 design px high: prominent 51px game name,
   genre/installation status, brief summary, gold details action.
3. Horizontal shelf, four curated games, with one focused gold tile and
   restrained game cover artwork. Icons do not fill hero and card images.
4. Quiet footer controls; only one primary action at a time.
5. Game details separated into two balanced columns, with a scrollable text
   body and a single install/remove action near the bottom.

## Tokens

- Main background `#0C1018`, dark rail `#12161E` and card `#171D28`.
- Warm focus/accent `#EAC28D`, text `#F0E5D6`.
- Secondary text `#BBC6D3`, cool/installed `#88A8C9`.
- 12–23px corner radii, no broad blue bloom or large white app cards.
- Focus animates scale and fill in approx. 210ms, based on the existing
  Dukkan fallback; exact motion easing of the menu's missing component is
  not asserted.

## Accessibility and input

- 20–51px primary typography at 1080p, no tiny multi-paragraph descriptions.
- Controller focus is an explicit finite state machine; hover does not decide
  the controller selection. The user can always escape confirmation with B.
- Search has a controller-driven keyboard and supports normal key presses too.
- Controls remain at the bottom, with keyboard shortcuts as a fallback.

## Engineering rule

Qt `RowLayout`/`ColumnLayout` own child geometry. Anchors only control children
of regular `Item` / `Rectangle`, never children directly laid out by those
layout managers. Plain `Row` children use local `y` rather than vertical anchors
so that positioners do not conflict with anchors either.
