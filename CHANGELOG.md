# Changelog

## [Unreleased]

- Test: extract CC2652P ZNP version-response decode into pure `src/zb_ver.h` (`decodeZbVer`), used by `getZbVer()`; add Unity suite (4 tests). 63 → 67 tests.
- Refactor: split `main.cpp` (2053 → 484 lines) into `network.cpp` (initLan, WiFi/AP, mDNS, overseer, WireGuard), `bridge.cpp` (TCP socket bridge), `factory.cpp` (BUILD_FACTORY_TEST only); globals/`BUFFER_SIZE` moved into `config.h`.
- Refactor: extract config loading into `config.cpp` + pure `ip_utils.h` `parseIpv4()` (4 new Unity tests; fixed `1.2.3` and `256.x` parse bugs). 59 → 63 tests.
- CI: add `Release` workflow — bump `tools/version` + `manifest.json` via `patch`/`minor`/`major` (or explicit `X.Y.Z`), tag `vX.Y.Z`, build pinned `prod-solo`, attach firmware to GitHub release. Local equivalent: `scripts/release.sh`.
- Build: gate all factory-test code behind `-DBUILD_FACTORY_TEST` (new `factory-test-solo` env); prod `prod-solo` and `debug-solo` builds now exclude it. Factory test triggers on BTN pressed (LOW) instead of released, and hard-reset mode is skipped in factory builds.
- Build: make factory-test credentials injectable via `FACTORY_TEST_SSID`/`FACTORY_TEST_PASS`/`FACTORY_TEST_HOST` build flags with defaults preserved.
- Cleanup: remove dead factory BSL/Type-C passthrough code (`typecReveivePktProcess`, `zigbeeReveivePktProcess`, `bsl_flag`, unused RX buffers) and commented-out `cmd2zigbee`/`clearS2Buffer`.
- Test: extract `zbGenChksum` → `src/checksum.h` and `getGmtOffsetForZone` → `src/zones.h` as header-only pure functions; add Unity suites (15 new tests, 59 total). (#28)
- Cleanup: remove dead `hexToDec` (no callers). (#28)
- CI: add `pio check` (cppcheck) lint gate, `debug-solo` build job, and firmware artifact upload (`bin/*.bin`) to the workflow. (#27)
- Fix: gate `DEBUG_PRINT*` behind the `-DDEBUG` build flag only — unconditional `#define DEBUG` in `config.h` leaked full debug logging into prod firmware. (#26)
- Fix: `#if BUILD_ENV_NAME != debug` preprocessor guard never worked (compared a string literal); replaced with a real `-DBUILD_DEBUG` flag for debug envs. (#26)
- Fix: `btnFlag` written from an ISR was not `volatile`; now `volatile uint8_t`. (#26)
- Fix: socket arrays sized `[10]` vs `MAX_SOCKET_CLIENTS` (5) — unified; `TCP_LISTEN_PORT` 9999 → 6638 to match config/docs default. (#26)
- Security: remove committed factory-test credentials (`factory-test-ap.txt`, `factory-test-process.txt`, `bin/factory-test-ap.txt`); document the inline AP-scan test flow in `factory_test/README.md` instead. (#25)
- Build: wire `version_increment_post.py` so patch version auto-increments every build; stop committing `tools/.version_no_increment` (now gitignored). (#25)
- Build: fix `build.py` version filename trailing-newline bug (`AVATTO-GW90-Ti_v1.0.7<newline>.full.bin`). (#25)
- Cleanup: delete dead `tools/` scripts (`commit.sh`, `make_git_hook.sh`, `flash.sh`, `esptool.py`, `debug_build.py`); stale `debug-ui/`; dead web JS (`bootstrap.min.js`, `jquery-min.js`, `masonry.js`) + inert `data-masonry` attributes/CSS. (#25)
- Deps: migrate ArduinoJson 6.x → 7.4.3 (`DynamicJsonDocument`/`StaticJsonDocument` → auto-sized `JsonDocument`, `createNested*` → `to<JsonArray>()`/`add<JsonObject>()`). (#24)
- Fix: off-by-one buffer overflow in `mqttCallback` (`jjson[length+1]` → `jjson[length]`). (#24)
- Deps: remove unused `ArduinoJson.h` includes from `zb.cpp`/`web/common.cpp`; self-contained `etc.h`. (#24)
- Build: suppress cppcheck preprocessor errors from ArduinoJson 7 macros. (#24)
- Deps: remove unused libs — AsyncTCP, EspSoftwareSerial, ESP32Ping (no code references; Serial2 is hardware UART). (#23)
- Deps: pin all remaining `lib_deps` to exact versions for reproducible builds (CircularBuffer 1.4.0, Ticker 4.4.0, PubSubClient 2.8.0, CronAlarms 0.1.0, Unity 2.6.1). (#23)
- C++: enable `-std=gnu++17` (override framework `gnu++11` via `build_unflags`). (#22)
- C++: modernize `encoding.h` — `constexpr`/`noexcept` helpers, `std::string_view` inputs, drop `strtol` hex parsing. (#22)
- C++: convert `COORDINATOR_MODE_t` unscoped enum to scoped `enum class CoordinatorMode`; explicit `uint8_t` casts at JSON/serial boundaries. (#22)
- C++: `constexpr` config constants; add missing include guards to `log.h`, `mqtt.h`, `zb.h`, `web.h`. (#22)
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
- Build: enable `pio check` (cppcheck) with `lint`/`lint:full` mise tasks. (#20)
- Fix: static-analysis defects — init device-ID XOR bytes, `%02x` buffer overflow (`char[2]` → `char[3]`). (#20)

## v1.0.7

- Fix ZHA USB mode serial connection failure (factory test timeout).

## v1.0.6

- Fix OTA display bugs on firmware update page.

## v1.0.5

- Initial fork from mercenaruss/uzg-firmware.
- CC2652P BSL flashing, factory test, AVATTO GW90-TI pin mapping, SVG fixes.
