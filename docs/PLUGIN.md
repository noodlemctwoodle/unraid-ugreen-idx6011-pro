# Plugin packaging — how `ugreen-idx6011-pro.plg` conforms to Unraid

The authoritative schema is Unraid's own plugin manager, `/usr/local/sbin/plugin`
(957-line PHP; its header doc is the spec). Everything below was verified against
that source on Unraid 7.3.2 and exercised with a full real-manager lifecycle test
(`install` → attribute queries → `remove` → reinstall) on 2026-07-12.

## Schema facts that matter (from the parser itself)

| Fact | Consequence for us |
|---|---|
| `<PLUGIN>` attrs: `name` MANDATORY, `version` MANDATORY (`YYYY.MM.DD[a-z]` convention), `author`, `pluginURL` (required for update checks), plus webGUI-recognized `icon`, `support`, `min`/`max`, `launch`. Unknown attrs are queryable via `plugin <attr> <file>`. | We set name/author/version/min/icon/support/pluginURL. |
| `<FILE>` elements run in document order. No `Method` attr ⇒ treated as `Method="install"`. `Method` can list several methods space-separated. | Install logic lives in a single `<FILE Run="/bin/bash"><INLINE>`. |
| **Removal executes only `<FILE ... Method="remove">` elements.** A `<REMOVE>` element is NOT part of the schema — it is silently ignored. | Our uninstall is a `Method="remove"` FILE (fixed 2026.07.12a; the earlier `<REMOVE>` block did nothing). |
| `<FILE Name=... ><URL>...<MD5|SHA256>` downloads and hash-verifies payloads; files already present with matching hash are not re-downloaded. | Used for the future public txz payload (see Release process). |
| **At every boot** `rc.local` runs `plugin install` on each `/boot/config/plugins/*.plg`. **Any error moves the plg to `/boot/config/plugins-error` — permanently disabling it.** | Boot INLINE is hardened: every call guarded with `[ -f ... ] &&`, script ends `true`, and `start-panel.sh` itself always `exit 0`. |
| `plugin update` requires `pluginURL` + `version`; it fetches the remote plg and compares version strings. | Version bumps must be monotonic (`2026.07.12` → `2026.07.12a` → …). |
| Convention: webGUI files under `/usr/local/emhttp/plugins/<name>/`, persistent files under `/boot/config/plugins/<name>/` (flash writes minimized). | We follow both; RAM-side dir is created on demand and removed by `Method="remove"`. |

## What the plugin does at boot (verified)

`<FILE Run="/bin/bash"><INLINE>` →
1. Migrates away any legacy `/boot/config/go` block (idempotent `sed`).
2. `start.sh` (LED daemon — skipped gracefully if its payload isn't staged).
3. `start-panel.sh`:
   - **DMI gate**: `sys_vendor == UGREEN && product_name == "iDX6011 Pro"`, else exit 0.
   - `assert-grub.sh`: re-adds the UNRAID menuentry to the NVMe ESP if a UGOS update
     wiped it (backs up first), asserts `default`/`timeout`, puts the `debian`
     EFI entry first in BootOrder. Never edits the vendor menuentry.
   - Stages `panel/overlay/$(uname -r)/bzroot-wakefix` → `/boot/bzroot-wakefix`
     (kernel upgrades: panel dark for one boot + an Unraid notification, never a
     broken boot).
   - Loads the touch stack: stock `mfd_core` + per-kernel `intel-lpss(-pci)` +
     `i2c-designware-core/platform` from flash + stock `i2c-dev`.
   - If `eDP-1` is `connected` (i.e. we booted via the NVMe path): kills any old
     daemon **first**, copies the new binary, starts `panel_dash` with
     `settings.cfg` values. Otherwise posts a warning notification and exits 0.

`<FILE Run="/bin/bash" Method="remove">` → stops both daemons, removes the RAM-side
webGUI dir. **Deliberately leaves** `/boot/bzroot-wakefix`, the grub entry, and the
flash payload: the grub UNRAID entry references `bzroot-wakefix`, so deleting it
would break the default boot. Full manual uninstall = `front-panel-blueprint.md`.

## Flash payload layout

```
/boot/config/plugins/ugreen-idx6011-pro/
├── start.sh / stop.sh / monitor.sh / ugreen_leds_cli…   # LED feature (existing)
├── start-panel.sh / stop-panel.sh / assert-grub.sh      # LCD feature
└── panel/
    ├── panel_dash                    # daemon binary
    ├── settings.cfg                   # BRIGHTNESS / INTERVAL / ROTATE
    ├── wallpaper.png                  # optional, user-supplied
    ├── modules/<kver>/*.ko            # touch stack, per kernel version
    └── overlay/<kver>/bzroot-wakefix  # display-module overlay, per kernel version
```

## Release process (for public distribution)

1. Build artifacts for the target Unraid release: `boot/build-overlay.sh` (overlay +
   touch modules) and `src/panel/build.sh` (daemon).
2. Pack the payload: `tar cJf ugreen-idx6011-pro-<ver>.txz` of the flash layout above
   (root:root ownership).
3. Attach the txz to a GitHub release; compute `md5sum`.
4. In the plg, add before the install INLINE:
   ```xml
   <FILE Name="&plgdir;/&name;-&version;.txz">
     <URL>https://github.com/noodlemctwoodle/unraid-ugreen-idx6011-pro/releases/download/&version;/&name;-&version;.txz</URL>
     <MD5>…</MD5>
   </FILE>
   ```
   and have the INLINE untar it over `&plgdir;` before starting.
5. Bump `version`, push the plg to `main` (that raw URL is the `pluginURL` update
   check target). Users get it via Plugins → Install Plugin → plg URL.
6. Per-Unraid-release maintenance: rebuild step 1, add the new `<kver>` dirs to the
   payload, release. (Users on unsupported kernels degrade to
   "panel dark + notification", nothing breaks.)

## Lifecycle test record (2026-07-12, Unraid 7.3.2)

```
plugin install /tmp/ugreen-idx6011-pro.plg   → exit 0, symlink in /var/log/plugins,
                                               plg copied to flash, daemon up
plugin version …                             → 2026.07.12a
plugin support …                             → github issues URL
plugin remove ugreen-idx6011-pro.plg         → Method="remove" ran, daemon stopped,
                                               symlink removed, plg → plugins-removed
reinstall                                    → daemon up again
```

## Settings `.page` skeleton (for the webGUI page, Unraid 7.x-verified pattern)

File: `/usr/local/emhttp/plugins/ugreen-idx6011-pro/UgreenIDX6011Pro.page`
(+ `launch="Settings/UgreenIDX6011Pro"` on the PLUGIN element; the route is
MenuPath/PageBasename). Canonical Dynamix pattern — POST to `/update.php` with
hidden `#file` (writes fields to the cfg on flash) and `#command` (restart script);
CSRF is auto-injected for POST forms; never trigger actions via GET.

```
Menu="Utilities"
Title="UGREEN iDX6011 Pro"
Icon="hdd-o"
Markdown="true"
---
<?PHP
$plugin = 'ugreen-idx6011-pro';
$cfg = parse_plugin_cfg($plugin);
?>
<form markdown="1" method="POST" action="/update.php" target="progressFrame">
<input type="hidden" name="#file" value="<?=$plugin?>/panel/settings.cfg">
<input type="hidden" name="#command" value="/boot/config/plugins/<?=$plugin?>/start-panel.sh">

_(LCD brightness %)_:
: <input type="number" name="BRIGHTNESS" min="5" max="100" value="<?=htmlspecialchars($cfg['BRIGHTNESS'] ?? '75')?>">

_(Auto-rotate pages every N seconds, 0 = off)_:
: <input type="number" name="ROTATE" min="0" max="300" value="<?=htmlspecialchars($cfg['ROTATE'] ?? '0')?>">

&nbsp;
: <span class="buttons-spaced"><input type="submit" value="_(Apply)_"><input type="button" value="_(Done)_" onclick="done()"></span>
</form>
```

7.2+ notes: `markdown="1"` required on the form for the `_(label)_:` definition-list
syntax; buttons wrapped in `<span class="buttons-spaced">`; `htmlspecialchars()` on
all echoed values; PHP 8.3-clean (no uninitialized vars, no `strftime`).

## Public-release conformance checklist (research-verified, distilled)

1. `.plg` basename == `name` entity == `name` attr == both plugin dir names.
2. `version` zero-padded `YYYY.MM.DD[a-z]`, no dashes — comparisons are **strcmp**
   (lexicographic), so padding is load-bearing.
3. `pluginURL` = raw default-branch URL of the plg itself, byte-identical in any CA
   wrapper. `support` = forum thread (CA requires one).
4. Removal ONLY via `<FILE Method="remove" Run=...>` (no Name attr). `<REMOVE>`
   elements and `Method="update"` do not exist in the schema.
5. `plugin update` runs the NEW plg's install and never the old plg's remove →
   install scripts must stop running daemons before starting (ours do).
6. Payload txz: `name-version-x86_64-1.txz`, root:root, `install/slack-desc`,
   installed via `Run="upgradepkg --install-new --reinstall"` on a `Name`d FILE
   under `&plgdir;` with `<SHA256>` (preferred over MD5; mismatch aborts install).
   Pre-install FILE purges txz files not matching `&version;`.
7. `Name`+`INLINE` FILEs without a hash are never overwritten → correct mechanism
   for user-editable configs (our `settings.cfg` uses it).
8. Escaping inside INLINE: `&amp;` for `&`, `&#124;` for `|`, `&lt;` for `<`.
9. Test matrix before shipping: install → reboot-restore → version-bump update
   (no duplicate daemons) → remove (everything gone) → `install ... forced`
   idempotency → corrupted-SHA256 abort lands the plg in `plugins-error`.
10. CA submission extras: public repo + OSI license, `ca_profile.xml`, wrapper XML
    with PNG icon ≥256×256 at a stable raw URL, GitHub 2FA, never rename the repo.

Status here: items 1-5, 7, 8 conformant and lifecycle-tested on the box (2026-07-12);
6, 9(update/corrupt cases), 10 + the .page above are the remaining public-release work.
