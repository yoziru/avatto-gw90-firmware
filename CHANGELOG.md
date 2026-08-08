# Changelog

## [Unreleased]

- Build: mise.toml + pyproject.toml for reproducible builds. (#3)
- CI: GitHub Actions workflow (PlatformIO, SHA-pinned deps). (#5)
- Chore: untrack build artifacts (bin/, webh/, .vscode/). (#6)
- Docs: AGENTS.md, ARCHITECTURE.md (renamed), rewritten README, CHANGELOG extracted.
- Translation: all Chinese → English (61 files). (#4)
- Web: embed i18n, remove CDN deps, fix console logging. (#1)
- Routes: auto-generate from webh/ directory. (#2)
- CSS: replace Bootstrap with Water.css + minimal custom stylesheet (65KB→10KB gzipped). (#18)
- Refactor: finish web.cpp split — events.cpp, common.cpp, delete web.cpp, fix CSS MIME. (#17)
- Test: expand native unit tests (log buffer, API dispatch). (#16)
- UI: JS-controlled dark mode with toggle (mobile + sidebar), remove `prefers-color-scheme`. (#20)
- UI: sidebar language selector + dark mode toggle. (#20)
- UI: fix mobile navbar layout — `display: flex` on container, clean up h2 positioning. (#20)
- UI: proper pill-shaped switch toggles, radio button fix, tab styling. (#20)
- UI: fix item-box height on desktop, temperature bar, table striping. (#20)
- UI: move toggles out of card-header to card-body (Ethernet, WireGuard). (#20)
- JS: simplify `isMobile()` (width only), auto-close sidebar on page nav. (#20)
- JS: sync both lang selectors, fix tab pane `.show` class. (#20)
- Build: OTA flash task, git-aware version string. (#20)
- Build: remove Bootstrap JS (dead code), remove unused `navbar-brand` CSS. (#20)
- Fix: remove duplicate sidebar `<li>` that broke the nav DOM. (#20)
- Fix: restore page-fragment inline scripts (file upload, GitHub OTA buttons) by executing injected `<script>` tags. (#20)
- Fix: apply saved dark theme before first paint to avoid light-mode flash. (#20)
- Build: make version script git-optional, gitignore generated `src/version.h`, add OTA auth to `flash:ota`. (#20)

## v1.0.7

- Fix ZHA USB mode serial connection failure (factory test timeout).

## v1.0.6

- Fix OTA display bugs on firmware update page.

## v1.0.5

- Initial fork from mercenaruss/uzg-firmware.
- CC2652P BSL flashing, factory test, AVATTO GW90-TI pin mapping, SVG fixes.
