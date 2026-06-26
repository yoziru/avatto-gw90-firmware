# Changelog

## [Unreleased]

- Build: mise.toml + pyproject.toml for reproducible builds. (#3)
- CI: GitHub Actions workflow (PlatformIO, SHA-pinned deps). (#5)
- Chore: untrack build artifacts (bin/, webh/, .vscode/). (#6)
- Docs: AGENTS.md, ARCHITECTURE.md (renamed), rewritten README, CHANGELOG extracted.
- Translation: all Chinese → English (61 files). (#4)
- Web: embed i18n, remove CDN deps, fix console logging. (#1)
- Routes: auto-generate from webh/ directory. (#2)
- Web: replace i18next with custom MicroI18n. (#10)
- Web: replace device PNGs with inline SVG diagram. (#12)
- Test: add unit test infrastructure with Unity. (#13)
- Refactor: split web.cpp into modular files under src/web/. (#11)
- Fix: LAN8720 PHY init failure after ESP.restart() on PoE. (#9)
- Web: remove remaining device PNGs, replace with inline SVG. (#15)

## v1.0.7

- Fix ZHA USB mode serial connection failure (factory test timeout).

## v1.0.6

- Fix OTA display bugs on firmware update page.

## v1.0.5

- Initial fork from mercenaruss/uzg-firmware.
- CC2652P BSL flashing, factory test, AVATTO GW90-TI pin mapping, SVG fixes.
