# Changelog

## [Unreleased]

- Docs: AGENTS.md, ARCHITECTURE.md (renamed), rewritten README, CHANGELOG extracted.
- Translation: all Chinese → English (61 files). (#4)
- Web: embed i18n, remove CDN deps, fix console logging. (#1)
- Routes: auto-generate from webh/ directory. (#2)

## 2026-06-24

- Build: mise.toml + pyproject.toml for reproducible builds. (#3)
- CI: GitHub Actions workflow (PlatformIO, SHA-pinned deps). (#5)
- Chore: untrack build artifacts (bin/, webh/, .vscode/). (#6)

## v1.0.7 — 2026-05-12

- Fix ZHA USB mode serial connection failure (factory test timeout).

## v1.0.6 — 2026-04-28

- Fix OTA display bugs on firmware update page.

## v1.0.5 — 2026-04-15

- Initial fork from mercenaruss/uzg-firmware.
- CC2652P BSL flashing, factory test, AVATTO GW90-TI pin mapping, SVG fixes.
