# Dashboard fonts

The panel renders text with a real TrueType font via `vendor/stb_truetype.h`
(`font_init()` / `text()` in `../draw.h`). The font is selectable from the
Screen settings page (`FONT` key in `settings.cfg`); the default is
**Roboto Condensed**.

These `.ttf` files ship in the plugin payload and install to
`/boot/config/plugins/ugreen-idx6011-pro/panel/fonts/`. At startup the panel
resolves `FONT=<name>` to `fonts/<name>.ttf`.

| Setting value      | Family              | File                   |
|--------------------|---------------------|------------------------|
| `RobotoCondensed`  | Roboto Condensed    | `RobotoCondensed.ttf`  |
| `Inter`            | Inter               | `Inter.ttf`            |
| `JetBrainsMono`    | JetBrains Mono      | `JetBrainsMono.ttf`    |
| `BarlowSemiCond`   | Barlow Semi Cond.   | `BarlowSemiCond.ttf`   |
| `Montserrat`       | Montserrat          | `Montserrat.ttf`       |

## Licensing

All five families are licensed under the **SIL Open Font License 1.1**. The
per-family licence text is included alongside each font (`*-OFL.txt`) and must
be kept with the fonts when redistributed.

## Adding a font

1. Drop `NewName.ttf` in this folder and its `NewName-OFL.txt` (or equivalent
   licence) next to it.
2. Add it to the packaging list in `release/mkpayload.sh` and to the Font
   `<select>` in `src/UgreenIDX6011ProLayout.page` (the Display > Theme tab).
3. Select it from the Display > Theme settings page. No code change is required
   — any `fonts/<value>.ttf` resolves automatically.
