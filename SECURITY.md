# Security Policy

## Reporting a vulnerability

Please report security issues **privately** — do not open a public issue, pull
request, or discussion for a suspected vulnerability.

Use GitHub's private reporting: on this repository open the **Security** tab →
**Report a vulnerability** (Security advisories → *Report a vulnerability*). That
opens a private channel with the maintainer and can lead to a coordinated fix and
a published advisory.

This is a solo, community-run project, so handling is best-effort — expect an
initial acknowledgement within about a week. Please allow a reasonable window to
release a fix before any public disclosure.

When reporting, please include:
- the affected version (see below) and your Unraid / kernel version,
- what the issue is and its security impact,
- steps to reproduce or a proof of concept, if you have one.

## Supported versions

This project ships rolling, date-versioned releases. Only the **latest release**
receives security fixes — please reproduce on the latest release before reporting.

| Version | Supported |
|---------|-----------|
| Latest release (`YYYY.MM.DD`) | :white_check_mark: |
| Any older release | :x: |

## Scope

**In scope** — report here:
- `panel_dash`, the C dashboard daemon (runs as root; parses `/proc`, `/sys`,
  emhttp ini files, the docker socket, and raw I2C frames),
- the plugin install / remove and boot provisioning (the `.plg` logic, the
  self-registered EFI boot entry, the display + touch module overlay),
- the LED monitor scripts,
- the integrity of the install chain (the SHA256-verified payload downloaded from
  the GitHub release).

**Out of scope** — please report to the upstream project instead:
- vendored third-party libraries — [nothings/stb](https://github.com/nothings/stb),
- the bundled LED CLI — [klein0r/ugreen_leds_controller](https://github.com/klein0r/ugreen_leds_controller),
- the Linux kernel / Intel i915 / i2c-tools,
- UGREEN hardware or firmware, and Unraid itself.

## Threat model (context)

The dashboard runs as **root on a single-user NAS**, launched by the plugin with
fixed arguments. It exposes no network service and takes no untrusted remote
input, so the primary concerns are memory-safety bugs in `panel_dash` and the
integrity of the download → verify → install chain. The C code is scanned with
CodeQL on every push and pull request.
