# Changelog

All notable changes to this project are documented here. Entries are grouped by kind: **Features**, **Fixes**, **Improvements** (perf / refactoring / tests), **Chores** (build / CI / deps / cleanup / docs).

## [Unreleased]

### Features

- **Firmware updates moved to the About page.** Opening About checks GitHub for both the gateway (ESP32) and the Zigbee coordinator/router firmware: it shows what's installed, what's latest, and only offers an update (with release notes) when a newer version actually exists. Manual file upload stays as the fallback when GitHub is unreachable.
- **Readable Zigbee console.** Zigbee traffic now appears as short one-liners instead of raw hex, e.g. `AF_INCOMING_MSG from 0xA494 · cluster 0x0B04 · LQI 61 · ZCL Report`, and version reads show as `SYS_VERSION v2.7.1`. Turn off "Human-readable frames" to see the raw bytes again.
- **Logs that don't drown each other.** Zigbee frames now go to their own history buffer instead of sharing the event log, so busy mesh traffic can no longer push device events out — with real clock timestamps once NTP has synced. The filter buttons pick what you're looking at: **ESP32** (device events, incl. Zigbee chip/version reports) or **Zigbee ZNP Frames** (traffic).
- **More useful device events.** The ESP32 log now records Ethernet/WiFi up-down, MQTT connect results and OTA downloads.
- **Simpler Tools page.** The old tabs are gone: Logs on top, Module Control below. System Variables moved to General; the file browser and Current Control card were removed along with their API endpoints (nothing but them used those). Every save is confirmed with a log line.

### Improvements

- Wi-Fi joins that can't succeed now fail fast: a join to a wrong-password or out-of-range network gives up after ~45 s, stops the retry loop, and keeps the previously saved network. Repeated connect failures no longer flood the log with "WiFi down" lines.
- Wi-Fi page polish: the Join Other Network form shows a clear message when the network name or password is missing, password fields no longer trigger the browser's strong-password/save prompts, expanded rows use the full width, and the join form only appears after a scan finishes (a stalled scan times out with tap-to-retry). The saved-network row opens its IP settings on click anywhere, and the password field and Connect button share one height with even padding.
- Timezone table moved from DRAM to flash (~3.7 KiB freed); MQTT command dispatch extracted into pure `mqtt_cmd.h` with 9 native tests.
- Static web routes generated as a table + loop; `loadSystemVar()` temp-calibration block deduplicated into one helper.

### Chores

- Web UI slim-down (no visual changes): dropped the gulp toolchain (`gulpfile.js`, `package.json`, `npm install` step — `build_html.py` now stages/gzips with stdlib only, wiping stale headers so deleted assets stop shipping); removed `commonmark.min.js` (release notes render as plain text), 14 unreferenced images, and the dead `/js/{jquery,bootstrap,masonry}` routes; SPA shell routes collapsed into a table loop.

- Deps: `knolleary/PubSubClient` (unmaintained) → `hmueller01/PubSubClient3@3.3.0` (drop-in, verified live against a broker); WireGuard git dep pinned to a commit; dropped the bogus `npm@6` webfiles dependency.
- Cleanup: removed dead webfiles heritage (`gulp.js`, `gulp.meta.js`, `buildWeb.bat`), unused gulp requires, hand-rolled IPv4 parser (core `fromString()` instead) and `String` urlDecode wrapper, `haDiscoveryEntityCount()` helper, bubble sort in `merge_bin_esp`, unreachable CCTools package cases; deduped the 4 gulp gzip/emit task pairs.

## v1.1.1 — 2026-08-25

- Build: bump pioarduino platform 55.03.38 → 55.03.311 (Arduino core 3.3.11, ESP-IDF 5.5.5); drop the redundant `pip` package from the Python build env.

## v1.1.0 — 2026-08-11

*All fork work since branching from AVATTO's v1.0.7.*

### Features

- Collapse the three coordinator modes (LAN/WiFi/USB) into two — **Network** (0) and **Serial** (1). Network mode uses Ethernet as the primary uplink with automatic WiFi-STA fallback when Ethernet drops (and back when it returns, turning WiFi off again to reduce RF noise); a soft AP remains the last-resort config recovery. The old standalone WiFi mode is gone; Serial mode is unchanged GPIO33 passthrough with the optional Keep Network & Web channel. (#57, #59)
- Report the active uplink ("Ethernet" / "WiFi (fallback)" / "Access Point" / "None") as `netUplink` on the root status page. (#58)
- Status-page **Zigbee connection** card showing the live link between the coordinator and its client (Z2M, ZHA, or USB host in Serial mode): connection state, client IP, connected-for duration, coordinator status, auto-reset count; auto-refreshes every 5 s via `/status`. (#41)
- Lightweight loop/heap profiler, **gated behind the `debug` build** (`BUILD_DEBUG`). `profilingLoopBegin()/profilingLoopTick()` bracket each `loop()` iteration tracking min/EMA/max; 60 s snapshots log loop latency + heap + fragmentation, exposed via the status API (`loopMinUs/loopAvgUs/loopMaxUs`, `espHeapMinFree`, `espHeapLargest`, `espHeapFrag`). Stats math is pure `src/profiling.h` (unit-tested).
- Wi-Fi joins are now **commit-on-connect**: config is staged in RAM and only written to `/config/configWifi.json` once the STA link actually succeeds — a wrong password or unreachable AP can never become the saved network. Tweakable IP settings on the saved network and "Forget" still persist immediately. Adds pure `wifiShouldCommit()` logic (unit-tested) and shared `connectWifiSta()`. (#53)
- Wi-Fi page: saved network gets its own "Your Network" section with checkmark instead of hiding under "Other Networks". (#54)
- Web UI overhaul (#20): JS-controlled dark mode with toggle, sidebar language selector, proper pill-shaped switch toggles/radio/tab styling, mobile navbar fixes; embedded i18n without CDN deps (#1); routes auto-generated from `webh/` (#2).

### Fixes

- Security: `/events` (SSE) now requires web auth like every other protected endpoint — unauthenticated clients can no longer observe firmware-flash progress or occupy the single global SSE slot.
- C++ audit — buffer safety & correctness:
  - socket bridge read LAN/Serial2 bytes after bounds checking — reads now bounded upfront (`bridge.cpp`)
  - `logPrint()` skipped the newest log char (`size()-1` off-by-one)
  - MQTT callback VLA sized off network-supplied length → fixed buffer + `deserializeJson()` error check; HA discovery `payload_press` emitted invalid JSON → buttons actually work now
  - ZNP status reads had no bound against caller buffers → `zbGetResponseData`/`zbGetStatus` clamp payloads
  - `runFlash()` overflowed its 240-byte send buffer on oversized Intel HEX records → rejects file
  - `saveParams` checked `baud` for the `port` field; System Tools leaked/truncated `configGeneral.json` handles
  - `tempOffset` load precedence no-op + no sanity clamp → proper default + ±10 °C re-calibration
  - WireGuard private key persisted under misleading `localIP` key → `localPrivKey` (loader keeps fallback)
  - timezone default "Europe/Kiev" never applied (always-truthy array check); dropped `const_cast` on string literal
  - Zigbee-firmware download divided by zero without `Content-Length`; timeout was total-download instead of no-data
  - `GET /api?action=5` returned nothing on missing file (now 404); SSE client `sprintf` into undersized VLA; `UPD_FILE` pointed at old org
- Platform/Core-3.x migration fallout: `wifiMac` read all-zeros with WiFi off (now via `esp_base_mac_addr_get()`); CPU temp skewed by bogus auto-calibration (store 0, clamp stale offsets); missing `delay(1)` yield caused 100% busy-spin after `CronAlarms` removal. (#67, #68, #33)
- Auto-recover a hung CC2652P coordinator via fully-passive `zbHealthCheck()` (resets only when a client is active with zero coordinator output for 3 min ×3); exposes status + auto-reset count in Device Status. (#34)
- Temperature read was frozen at exactly 53.3 °C — TSENS returns a fixed sentinel when the RF PHY is down; reads now force the radio on, take a median of 5 samples, cache 15 s. Also restored the ~30 °C auto-calibration persisted to `/config/system.json`, and made reads **non-blocking** (state machine in `loop()`) so cold reads no longer freeze web/bridge/MQTT for ~1.3 s. (#46–#48)
- `disableWeb` now actually disables the web server; loop serves web unconditionally instead of deprioritizing it behind socket clients. (#32)
- PASSTHROUGH-mode overseer re-ran `ETH.begin()` every 5 s tick, flapping the LAN8720 link — brings Ethernet up once now. (#33)
- `setClock()` blocked the whole loop up to 60 s waiting for NTP on every link-up — applies timezone immediately, syncs in background. (#34)
- `connectWifi()` static attempt counter persisted across callers and re-armed indefinitely → 20 s timestamp throttle. (#35)
- Debug logging leaked into prod firmware (`#define DEBUG` unconditional); broken `#if BUILD_ENV_NAME != debug` guard replaced with `-DBUILD_DEBUG`; ISR-written `btnFlag` now `volatile`; socket arrays sized to `MAX_SOCKET_CLIENTS`; TCP port unified at 6638. (#26)
- Off-by-one buffer overflow in `mqttCallback` (`jjson[length+1]`). (#24)
- Static-analysis defects: device-ID XOR init, `%02x` buffer overflow (`char[2]` → `char[3]`). (#20)
- UI: duplicate sidebar `<li>` broke nav DOM; page-fragment inline scripts (upload/GitHub OTA buttons) execute again after injection; saved dark theme applies before first paint (no light-mode flash). (#20)
- Values in `respValuesArr` headers are ASCII-only (em-dash placeholder rendered as mojibake). (#42)
- `parseIpv4()` rejected/accepted wrong inputs (`1.2.3`, `256.x`) — fixed during pure-header extraction. (#75)

### Improvements

- Performance: temperature read is **demand-driven** — radio powers up only when a consumer asks (status page/MQTT), so idle gateway has no 30 ms loop spikes or 2.4 GHz noise near the coordinator; boot calibration caches the offset-adjusted value; console poll reuses a module-level String instead of allocating up to 8 KiB per fetch.
- Performance: cut heap allocation churn on hot paths — `getRootData()` reuses a module-level `JsonDocument`, `/status` body reuses a reserved `String`, per-frame `printLogTime()` no longer allocates.
- Refactor: split `main.cpp` (2053 → 484 lines) into `network.cpp`/`bridge.cpp`/`factory.cpp`; finished the `web.cpp` split (`events.cpp`, `common.cpp`). (#74, #17)
- Refactor/DRY: extracted `readJsonConfig()`, `rawTempToCelsius()`, `zbLedCommand()`; HA discovery monster-loop became a data table; bridge hex-dump uses a nibble table instead of per-byte `sprintf`.
- Testability: extracted pure headers with Unity suites — `ip_utils.h` (59→63), `checksum.h`+`zones.h` (+15), `zb_ver.h` (63→67), `config_utils.h`+`ha_discovery.h` (74→88), discovery-message builder + WiFi throttle (88→95 tests); expanded native coverage of log buffer/API dispatch. (#16, #28)
- STA negotiates 802.11 b/g/n (was b-only, capping fallback at 11 Mbps and rejecting n-only APs). (#49)
- UI: honest WiFi radio state on status page (green Connected / blue Asleep / grey Not configured; diagram tints accordingly); Wi-Fi IP-settings panel shows explanatory note instead of blank "—" when asleep; card layout/alignment polish; fan icon; red-outlined Forget button. (#50–#52, #55)
- UI: About page cleanup — marketing/link cards and footer logos removed; "Brand Information" renamed **Device**, hardware info merged into one System Information card; About API serves full device info via `getRootData()`. Device-information card moved to About, Zigbee-connection card moved under Operating-mode diagram. (#37, #38)
- CSS: replaced Bootstrap with Water.css + minimal custom stylesheet (65 KB → 10 KB gzipped). (#18)
- C++ modernization: `-std=gnu++17`, scoped `enum class CoordinatorMode`, `constexpr` constants/config helpers, include guards everywhere. (#22)

### Chores

- Build: migrated to `pioarduino/platform-espressif32 55.03.38` (Arduino Core 3.3.8 + ESP-IDF 5.5.4) — correct dual-core ESP32-D0WDR2-V3 target (was misassumed SOLO1), HTTPS works, envs renamed `prod`/`debug`/`factory-test`, both cores at 240 MHz. (#64)
- Build: replaced vendored WireGuard lib with maintained Tinkerforge fork (esp_netif/IDF 5.x compatible); adapted `wgBegin()` to its API. (#65)
- Build: full Core-3.x API migration (`NetworkClientSecure`, event names, `accept()`, pinned tasks). (#66)
- Build: stopped auto-incrementing patch version per build — `tools/version` is read-only (Release workflow bumps), `-dirty` suffix on dirty trees, logic consolidated in `tools/version_util.py`. (#44)
- Build: factory-test code gated behind `-DBUILD_FACTORY_TEST`; credentials injectable via `FACTORY_TEST_SSID/PASS/HOST` flags. (#77, #78)
- Build/deps: ArduinoJson 6 → 7.4.3 migration; removed unused libs (AsyncTCP, EspSoftwareSerial, ESP32Ping); pinned remaining `lib_deps` to exact versions. (#23, #24, #91, #93–#96)
- Build: mise.toml + pyproject.toml reproducible setup; OTA flash task; git-aware version string. (#3, #20)
- CI: GitHub Actions workflow (PlatformIO, SHA-pinned deps); `Release` workflow (version bump/tag/firmware attach); `pio check` lint gate with `--skip-packages` (GCC 14.2 toolchain header breaks cppcheck parsing); debug build job + artifact upload. (#5, #27, #70, #76, #82, #94)
- Cleanup: dead code removal across audit (`programFlashFromFile`, `zbSendAck`, factory BSL/Type-C passthrough, deprecated `adapterModeUSB/LAN` commands + their UI buttons, dead `emergencyWifi`/`enableWiFi` config keys, unused includes, commented-out cruft); deleted stale `tools/` scripts, `debug-ui/`, dead web JS. (#25, #28, #56, #79, #81)
- Chore: untracked build artifacts (`bin/`, `webh/`, `.vscode/`). (#6)
- Docs: rewrote README/ARCHITECTURE/AGENTS.md + factory-test README around current codebase and contributor workflows; CHANGELOG extracted; AGENTS.md added.
- i18n: translated all remaining Chinese content to English (61 files). (#4)

## v1.0.7

Upstream release (AVATTO):

- Fix ZHA USB mode serial connection failure (factory test timeout).

## v1.0.6

Upstream release (AVATTO):

- Fix OTA display bugs on firmware update page.

## v1.0.5

Upstream release (AVATTO):

- Initial fork from mercenaruss/uzg-firmware.
- CC2652P BSL flashing, factory test, AVATTO GW90-TI pin mapping, SVG fixes.
