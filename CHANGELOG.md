# Changelog

## [Unreleased]

- Feature: the status page now has a **Zigbee connection** card showing the live link between the coordinator and its client (Z2M, ZHA, or the USB host in Serial mode): connection state (Connected / Not connected / USB passthrough), client IP, connected-for duration, coordinator status, and auto-reset count. The card auto-refreshes every 5 s via `/status`; it works for both Zigbee2MQTT and ZHA (both use the TCP socket) and degrades gracefully in Serial mode. The "Connection Duration" row moved here from Device Status and is renamed "Connected for".
- Fix: values sent through the `respValuesArr` HTTP header (status-page `data-replace` fields) are ASCII-only — an em dash placeholder rendered as mojibake (`â€"`) in the browser. Use an ASCII `-` for not-applicable values instead. Also replaced the remaining em dashes across the source with ASCII hyphens for consistency.

- Build: stop auto-incrementing the patch version on every build. `tools/gen_version.py` now reads `tools/version` read-only (only the Release workflow bumps it) and appends `-dirty` when the working tree is dirty; version logic consolidated into `tools/version_util.py` shared by `gen_version.py` and `build.py`. Removes `version_increment_pre.py`, `version_increment_post.py`, and the `.version_no_increment` marker logic. Version reverted to `1.0.7`.

- Fix: the on-board temperature read was frozen at exactly 53.3°C — the ESP32 TSENS returns the fixed `0x80` sentinel whenever the RF PHY (shared SAR ADC) isn't actually powered. On ESP-IDF 5.x `esp_wifi_start()` alone leaves the radio in modem-sleep, so now the read forces the radio fully on (scan + `setSleep(false)`), takes a median of 5 samples, then tears it back down. Results are cached for 15 s so the 60 s MQTT heartbeat and page reloads don't power the radio every call (2.4 GHz noise near the Zigbee coordinator).
- Fix: the temp read is now **non-blocking** — it runs as a state machine in `loop()` (`tempSensorLoop`) instead of `delay(1200)` inside the web/MQTT handlers, which previously froze the web server, Zigbee socket bridge and MQTT together for ~1.3 s on every cold read. Page loads stay ~40 ms even on a cold cache; boot-time calibration keeps a synchronous read (nothing else runs during setup).
- Fix: restore the on-board temperature **auto-calibration** (offset against a ~30 °C reference, stored in `/config/system.json` on first boot) so the reported die temp reads sensibly.
- Network: STA now negotiates **802.11 b/g/n** (was pinned to 802.11b-only, capping fallback throughput at 11 Mbps and rejecting b/g/n-only APs).
- UI: the main status page now reports the WiFi radio state honestly — green **Connected** when STA is the active uplink, blue **Asleep** when configured but idle (Ethernet primary, radio off), grey **Not configured** when no credentials are saved. `wifiEnabled` reflects the actual radio power state. The Operating-mode diagram tints the WiFi icon/line/dot blue when it's asleep, and the stale "Switch" label reads "WiFi".
- UI: fixed the Wi-Fi page IP-settings panel — the live-IP box no longer shows a blank "—" when WiFi is asleep (it shows an explanatory note instead), and the DHCP toggle now cleanly swaps between the live values and the static form.
- UI: the "Your Network" row's checkmark and info button are now vertically centered and evenly spaced (the info button's water.css margin was adding a stray right gap), and expanded rows/panels no longer show a hand cursor over non-interactive areas.
- Fix: join a new Wi-Fi network via **commit-on-connect** — the config is staged in RAM and only written to `/config/configWifi.json` once the STA link to that SSID actually succeeds. A wrong password or unreachable AP can never become the saved network (previously any `saveParams` post persisted immediately, so a failed join looked "saved" after refresh). Tweakable IP settings on the already-saved network and "Forget" still persist right away. Adds `wifiSaveMode`/`wifiShouldCommit` pure logic (unit-tested) and a shared `connectWifiSta()` used by both the overseer and the save handler.
- Feature: the saved network is no longer listed under "Other Networks" on the Wi-Fi page (it has its own "Your Network" section with a checkmark).
- UI: the Wi-Fi page card now shows a proper Wi-Fi fan icon instead of the scanning-loop icon; the "Forget This Network" action renders as a red-outlined delete button (added `.btn-outline-danger`).
- Cleanup: remove the deprecated `adapterModeUSB`/`adapterModeLAN` API commands and the dead "Enable Network/Serial Mode" buttons in System Tools (mode is configured on the General page); drop dead `emergencyWifi`/`enableWiFi`/`disableEmerg` config keys and stale comments; de-duplicate the `temprature_sens_read` declaration.
- Feature: collapse the three coordinator modes (LAN/WiFi/USB) into two — **Network** (0) and **Serial** (1). Network mode uses Ethernet as the primary uplink with automatic WiFi-STA fallback when Ethernet drops (and back when it returns, turning WiFi off again to reduce RF noise); a soft AP remains the last-resort config recovery when both fail. The old standalone WiFi mode is gone — WiFi STA is now just the fallback in Network mode. Serial mode (was USB) is unchanged GPIO33 passthrough with the optional Keep Network & Web channel.
- Feature: report the active uplink ("Ethernet" / "WiFi (fallback)" / "Access Point" / "None") as `netUplink` on the root status page.
- Refactor: remove `CoordinatorMode::WIFI` and the now-redundant `prevCoordinator_mode`; mode is a binary toggle (Network ↔ Serial) on the button and the General page.
- Fix: guard `startServers()` so web/socket handlers are registered once per boot (repeated overseer reconnects previously re-registered duplicate `.on()` handlers).

## v1.0.2 — 2026-07-15

- Build: migrate to `pioarduino/platform-espressif32 55.03.38` (Arduino Core 3.3.8 + ESP-IDF 5.5.4, vanilla espressif core) and `board = esp32dev`. Fixes the misidentified hardware: the AVATTO GW90-TI uses a dual-core **ESP32-D0WDR2-V3** (2 cores + 2MB PSRAM), not the single-core ESP32-SOLO1 that README/AGENTS.md assumed. This invalidates the old "SOLO1 requires tasmota 2023.07.00" platform constraint; HTTPS (`WiFiClientSecure`) and modern deps now work. Renames envs `prod-solo`/`debug-solo`/`factory-test-solo` → `prod`/`debug`/`factory-test`. Runs both cores at 240 MHz (was 160 MHz).
- Deps: replace vendored `lib/WireGuard-ESP32` with the maintained Tinkerforge fork (`Tinkerforge/WireGuard-ESP32-Arduino` via git `lib_deps`) — esp_netif/IDF 5.x compatible with the LAN8720 Ethernet fix built in. Adapt `wgBegin()` to the fork's explicit subnet/port/gateway API (`/32`, port 51820, no gateway).
- Migrate: `WiFiClientSecure` → `NetworkClientSecure`, `WiFiClient` → `NetworkClient`, `ETH.begin()` → Core 3.x signature, `SYSTEM_EVENT_*` → `ARDUINO_EVENT_*` event names, `server.available()` → `server.accept()`, `xTaskCreate` → `xTaskCreatePinnedToCore` (pins flash/test tasks to core 0), `WiFiEvent_t` → `arduino_event_id_t`, `#include <esp_chip_info.h>`/`<esp_mac.h>`. Rename `src/network.h` → `src/net.h` (avoids shadowing the framework `Network` library).
- Fix: `wifiMac` read `00:00:00:00:00:00` with WiFi off (Ethernet mode) on Core 3.x — `WiFi.macAddress()` now requires an initialized STA netif. Read the burned-in base MAC via `esp_base_mac_addr_get()` (eFuse) instead.
- Fix: CPU temp display was skewed by a bogus auto-calibration (`loadSystemVar` stored `tempOffset = rawTemp - 30`, but a sealed PoE gateway runs ~53°C, so it subtracted ~23–33°C). Stop auto-calibrating (store 0) and clamp any stale offset outside ±10°C to 0. Raw sensor reads the true ~53°C die temp.
- Note: the D0WDR2-V3's embedded 2MB PSRAM is deliberately NOT enabled — it routes through GPIO16/17, which conflict with `CC2652P_RST` (GPIO16). Enabling PSRAM would break the Zigbee reset line.
- Build: cppcheck gate now runs with `--skip-packages` — the GCC 14.2 toolchain `type_traits` header breaks cppcheck 2.21 parsing (unrelated to project code).
- Fix: add `delay(1)` yield to `loop()` — PR #29 removed `CronAlarms`, whose `Cron.delay()` was the only yield, causing a 100% CPU busy-spin. (#33)
- Fix: auto-recover a hung CC2652P coordinator via a fully-passive `zbHealthCheck()` (observes client↔coordinator traffic; resets only when a client is active with zero coordinator output for 3 min ×3); exposes Coordinator status + auto-reset count in the Device Status card. (#34)
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
