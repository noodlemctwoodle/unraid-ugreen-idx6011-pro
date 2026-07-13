# Dashboard modules — contributor guide

The front-panel dashboard (`src/panel/`) is **config-driven**. Every page is an
ordered list of **modules** (widgets) read from `settings.cfg`; each module draws a
**card** using the shared **style shim** so the whole dashboard has one visual
language. This guide is how you add or change a module.

## Architecture

```
src/panel/
  panel_dash.c            single translation unit — #includes everything in order
  pageframe.h             page table, header/footer, body_top/page_end
  modules/
    module.h              framework: modfn typedef + modinfo_t registry struct
    cardstyle.h           THE STYLE SHIM — card helpers + geometry/scale constants
    mod_<name>.h          one file per module; draws itself, returns its height
    registry.h            id -> module table + render_modules(layout, st)
  pages/<page>.h          thin: page_<x>() calls render_modules(cfg_layout_<x>, st)
```

It is **one translation unit**: the modules are not compiled separately, they are
`#include`d into `panel_dash.c` in a fixed order (framework → shim → `mod_*` →
registry → pages). Keep each file small and single-purpose — no monoliths.

## A module

A module is one function:

```c
static int mod_cpu(int y, stats_t *st, int variant){
    char v[32];
    snprintf(v, sizeof v, "%.0f%%", st->cpu);
    int h = metric_card(y, "CPU", st->cpu, v, variant == 1);   /* bar / ring */
    if (st->temp_c > 0){ snprintf(v, sizeof v, "%dC", st->temp_c);
        card_sub(y, 0, v, st->temp_c > 85 ? UN_BAD : UN_DIM); }
    return h;                       /* height consumed (incl. the gap) */
}
```

Rules:
- Draw starting at `y`; **return the total height you consumed** (card + gap). The
  page renderer advances `y` by your return value. This return value is also what
  makes scrolling work: **touch/scroll/navigation are handled globally** by the main
  loop (swipe = page nav, drag = up/down scroll) — a module never handles input, it
  just draws and returns its height, and the page scrolls automatically.
- **Never lay a card out by hand.** Compose the shim helpers below so your card
  matches every other card. If you need a new card shape, add it to `cardstyle.h`
  (so it's reusable + consistent), don't inline it.
- `variant` selects a display style (0-based). Ignore it if you have one style.
- Read live values from `stats_t *st` (see `stats.h`). Colours come from the
  runtime palette (`UN_TEXT`, `UN_DIM`, `UN_OK/WARN/BAD`, …) in `draw.h`.

## The style shim (`cardstyle.h`)

Geometry / scale constants — use these, never magic numbers:
`C_X0` (left inset), `C_R` (right edge), `C_W` (content width), `C_GAP`,
`C_VAL` (primary value scale), `C_SUB`, `C_TAG`, `C_BODY`.

Card helpers (each returns the advance = card height + `C_GAP`):

| helper | card |
|---|---|
| `metric_card(y, title, pct, value, ring)` | title + big value + **bar** (ring=0) or **ring** (ring=1) |
| `spark_card(y, title, value, hist, hcnt, hpos, col)` | title + value + 62px sparkline |
| `value_card(y, h, title, value, col)` | title + one text line |
| `bar_card(y, h, title, value, pct, col)` | title + right value + a bar |

Accents (call after the card helper, they draw onto the card you just made):
`card_tag(y, s, col)` (small top-right tag), `card_sub(y, slot, s, col)`
(right-aligned **short** value — temp/power — slot 0 upper / 1 lower),
`metric_detail(y, s, col, ring)` (a **long** used/total line, placed safely for the
bar/ring variant — see below), `item_head(y, dot, name, name_sc, right, right_sc, col)`
(list-item header row).

Change any of these and **every** card updates — that's the point.

## Colours

Colour is part of the shim too, so a new module is coloured like every other one
**for free**. Two layers:

**Palette** (`draw.h`) — the named theme colours, themeable at runtime from
Settings ▸ Screen: `UN_TEXT`, `UN_DIM` (labels), `UN_OK` / `UN_WARN` / `UN_BAD`
(status), `UN_ORANGE` / `UN_ORANGE_M` (accents), `UN_RED`, `UN_GREY_*`. Use these
for plain text/labels; never a literal hex.

**Semantic colour helpers** (`cardstyle.h`) — map a *state* to a palette colour in
one place, so thresholds are identical everywhere:

| helper | maps |
|---|---|
| `col_load(pct)` | usage/load % → accent / warn (>75) / bad (>90) |
| `col_temp(c)` | CPU/board temp → ok / warn (>70) / bad (>85) / dim (n/a) |
| `col_disktemp(c)` | disk temp → dim / warn (>45) / bad (>55) |
| `col_state(up)` | link/service up → ok / dim (text) |
| `col_dot(up)` | presence dot → ok / grey |
| `col_health(h)` | 0/1/2 → ok / warn / bad |

**Rule: a module must never hardcode a colour decision** (no `temp > 85 ? UN_BAD :
…` inline). Call a `col_*` helper, or add a new one here — then it's consistent and
re-themeable everywhere. `metric_card`/`bar_card` already colour their bar/ring via
`col_load`.

## Text & fonts

`text(x, y, scale, colour, str)` and `text_w(scale, str)` (in `draw.h`) render
with a real TrueType font via `vendor/stb_truetype.h`. The user picks the face
on the Screen settings page (`FONT` key → `panel/fonts/<name>.ttf`, default
**Roboto Condensed**); if no TTF loads it falls back to the built-in
`stb_easy_font` vector font. Notes for module authors:

- `scale` is unchanged from the old renderer — it maps to a pixel height, so the
  existing per-card y-offsets keep working across fonts.
- Because it's a real font, **full glyph coverage is available** — you can use
  `°`, `–`, `→`, `×`, etc. directly in UTF-8 string literals (the old vector
  font only knew ASCII 32–126, which is why placeholders use plain ASCII).
- `text_w()` is font-accurate, so right-aligned/centered text and `trunc_fit`
  work for every face — don't hardcode pixel widths for strings.
- Glyphs are cached per (codepoint, size); nothing to manage. To add a
  selectable font see `src/panel/fonts/README.md`.

## Layout & avoiding overlap

The panel is only **258 px** wide, so text overlap is the easiest way to make a
card look broken. The rule that keeps it from happening: **every text helper clips
to a zone, so overlap is impossible if you compose helpers instead of calling
`text()` by hand.** A metric card has these zones:

```
┌───────────────────────────────────────┐
│ TITLE (left)               tag (right) │  card_tag clips right of the title
│  BIG VALUE          card_sub (right)   │  bar mode: value left, subs right
│  (or a RING, left)  metric_detail zone │  ring mode: ring left, detail right
│  metric_detail row (bar mode)          │  bar mode: detail on its OWN row
│  ▓▓▓▓▓▓▓ bar ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓   │
└───────────────────────────────────────┘
```

Guarantees baked into the shim:
- `card_tag` / `card_sub` **truncate** to their right-hand zone — they can't run
  back over the title or the big value.
- `metric_detail` places a *long* used/total line where it can never collide:
  **right of the ring** (ring mode) or on **its own row** below the value (bar
  mode), truncating either way. Use it for anything like `"1.8 / 62.3 GB"`; use
  `card_sub` only for short values (`"39C"`, `"7.9 W"`).
- `value_card`, `bar_card`, and the `item_head` name all `trunc_fit` too.

So: **never call `text()` for content that could be wide** — reach for a helper
that owns a zone (or add one that `trunc_fit`s). And **always `--shot` a layout
change and eyeball it**, watching ring-variant details and long strings in
particular (a right-aligned long string over a ring gauge was the classic trap).

## Registering a module

Add it to the table in `registry.h` (id, label for the web editor, fn, variant
count + names) and `#include` its file in `panel_dash.c` (after `cardstyle.h`,
before `registry.h`):

```c
{ "cpu", "CPU", mod_cpu, 2, { "bar", "ring" } },
```

## Layout config

Each page is an ordered `id[:variant],...` string in `settings.cfg`, e.g.

```
LAYOUT_OVERVIEW=host,cpu:ring,mem,net,storage,uptime
```

`render_modules()` parses it, looks each id up in the registry, resolves the
variant name to an index, and calls the module. Unknown ids are skipped. Defaults
live in `prefs.h` (`cfg_layout_<page>`).

Every page is config-driven: `LAYOUT_OVERVIEW`, `LAYOUT_HARDWARE`, `LAYOUT_NETWORK`,
`LAYOUT_DISKS`, `LAYOUT_DOCKER`, `LAYOUT_HOME`. **Home is a blank canvas** — its
default is only a starter; put any modules you like on it. Pages are
enabled/disabled and reordered from the **web layout editor** (writes these
strings) — see the settings `.page` files.

## Build + verify

```bash
docker exec i915build bash /tmp/pb2/build.sh          # single-TU compile (gcc -O2 -w)
/usr/local/bin/panel_dash --shot /tmp/shots           # render every page to PNG, no panel needed
```

`--shot` renders offscreen with live stats — use it to eyeball a change without the
physical panel. Always `--shot` a layout change (default + a variant) before you ship.
