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
  page renderer advances `y` by your return value.
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
(right-aligned secondary value, slot 0 upper / 1 lower),
`item_head(y, dot, name, name_sc, right, right_sc, col)` (list-item header row).

Change any of these and **every** card updates — that's the point.

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
live in `prefs.h` (`cfg_layout_<page>`), and match the original hardcoded layout.
Pages are enabled/disabled and reordered from the **web layout editor** (writes
these strings) — see the settings `.page` files.

## Build + verify

```bash
docker exec i915build bash /tmp/pb2/build.sh          # single-TU compile (gcc -O2 -w)
/usr/local/bin/panel_dash --shot /tmp/shots           # render every page to PNG, no panel needed
```

`--shot` renders offscreen with live stats — use it to eyeball a change without the
physical panel. Always `--shot` a layout change (default + a variant) before you ship.
