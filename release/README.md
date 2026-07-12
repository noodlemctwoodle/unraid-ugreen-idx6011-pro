# Release payload

`mkpayload.sh` assembles the flash-payload `.txz` that the `.plg` downloads,
SHA256-verifies, and extracts to `/boot/config/plugins/ugreen-idx6011-pro/` on
install — so a plain install from the plugin URL stages everything.

```sh
bash release/mkpayload.sh 2026.07.12f              # default kver 6.18.38-Unraid
# -> release/ugreen-idx6011-pro-2026.07.12f.txz (+ .sha256)
```

The archive is built from `src/` (scripts), `prebuilt/` (binaries + kernel-bound
`modules/<kver>/*.ko` and `bzroot-wakefix`) and `images/` (icon). It intentionally
omits `settings.cfg` (the `.plg` seeds that only if absent, so user edits survive
updates).

Full cut-a-release steps — build artifacts, embed the SHA256 in the `.plg`, tag,
`gh release create`, upload — are in [`../docs/PLUGIN.md`](../docs/PLUGIN.md)
("Release process"). The built `.txz`/`.sha256` are release artifacts and are **not**
committed (see `.gitignore`); they live on the GitHub release.
