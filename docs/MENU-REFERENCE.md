# Reference from supplied Zaman menu archive

This design comes from `zaman-menu-direct-arrows/qml/shell.qml`, **not** a guess at the running menu's complete source. The archive itself references components it does not include.

- Panel stops: `#FE171D28`, `#FE12161E`, `#FE17191F` (near-opaque). Fullscreen Dukkan canvas uses opaque equivalents.
- Selected action: `#EAC28D`, optional gradient to `#CB986C`, with dark text `#29221C`.
- Main title: `#F0E5D6`; secondary: `#BBC6D3`.
- Muted blue accents: `#88A8C9`, `#9CAFC5`.
- Border: `#405D718C` (translucent slate, GTK/Qt colors handled separately).
- Panel radius: 24px at 1080p. Dukkan token radius 20px on large cards to account for different scale.
- Menu uses `uiScale = clamp(overlay.height / 1080, 0.65, 1.4)` and `px(...)` sizes; Dukkan now includes an equivalent helper.
- Menu footer uses semantic button faces `north`, `south`, `east`, `west`; Dukkan approximates these but can't import `MenuButtonDiamond` because that source is missing from the patch archive.
- Menu focus row and artwork require source files `MenuFocusRow.qml`, `MenuFocusBackground.qml`, `MenuButtonDiamond.qml`, and `ZamanMark.qml` for full consistency; archive includes none of them.
- Real-world focus transitions, compositor session handoff, and Pegasus visual parity remain pending runtime validation.
