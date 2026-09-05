# AVATTO GW90-TI Zigbee Gateway — Architecture

> Maintained at `yoziru/avatto-gw90-firmware` · fork of `mercenaruss/uzg-firmware` · version lives in `tools/version` (releases are tagged `vX.Y.Z`)
>
> This document is a **map for contributors**: how the firmware is put together and where to go to make changes. If you are *deploying* a gateway — flashing, configuring, updating, troubleshooting — start with the `README`.

## Table of Contents

1. [Overview](#1-overview)
2. [Hardware Platform](#2-hardware-platform)
3. [Directory Structure](#3-directory-structure)
4. [Core Source Modules](#4-core-source-modules)
5. [Coordinator Modes](#5-coordinator-modes)
6. [Configuration System](#6-configuration-system)
7. [Flash Partition Table](#7-flash-partition-table)
8. [Dependencies](#8-dependencies)
9. [Build Process & Versioning](#9-build-process--versioning)
10. [Web Frontend Build](#10-web-frontend-build)
11. [Network Management](#11-network-management)
12. [On-board Temperature Sensor](#12-on-board-temperature-sensor)
13. [Zigbee Coordinator Management](#13-zigbee-coordinator-management)
14. [MQTT & Home Assistant Discovery](#14-mqtt--home-assistant-discovery)
15. [WireGuard](#15-wireguard)
16. [OTA & Releases](#16-ota--releases)
17. [Profiler (Debug Build)](#17-profiler-debug-build)
18. [Testing](#18-testing)
19. [API Endpoints](#19-api-endpoints)
20. [Known Issues & Precautions](#20-known-issues--precautions)

---

## Common contributor tasks → where to look

| I want to… | Start here |
|---|---|
| Build / test / lint / flash a device | `mise run <task>` (§9.3) |
| Add or change a configuration option | struct + loader in `config.h`/`config.cpp`, the save handler in `web/save.cpp`, and the page in `web/pages.cpp` |
| Change the web UI | `src/websrc/` → gulp → `src/webh/` (§10) |
| Add an API endpoint | `web/api.cpp` (action enum + dispatch, §19) |
| Change how ZNP frames are bridged | `bridge.cpp` |
| Tune the coordinator health check | `zb.cpp` → `zbHealthCheck()` |
| Add a unit test | keep the logic in a pure `*.h`, add a suite in `test/` (§18) |
| Debug the loop / memory usage | debug build + profiler (§17), `mise run flash:ota:debug` |
| Cut a release | `Release` workflow / `scripts/release.sh` (§16) |

---

## 1. Overview

The AVATTO GW90-TI is a PoE-powered Zigbee gateway. An **ESP32-D0WDR2-V3** bridges a **TI CC2652P** Zigbee coordinator to the LAN over Ethernet (primary) with automatic WiFi-STA fallback, so Home Assistant can use it with **ZHA** or **Zigbee2MQTT** over a plain TCP socket (port 6638). The ESP32 is a transparent serial↔socket bridge: the Zigbee network itself lives in the client application.

**Main features:**

- Two coordinator modes: **Network** (Ethernet → WiFi STA fallback → soft-AP recovery) and **Serial** (USB passthrough)
- Embedded web UI for network/serial/security/MQTT/WireGuard configuration
- MQTT integration with Home Assistant auto-discovery
- CC2652P firmware flashing (BSL) with Intel HEX validation
- WireGuard VPN tunnel support
- mDNS/ZHA zeroconf service discovery
- Passive coordinator health monitoring with auto-reset
- Lightweight profiler in the debug build

---

## 2. Hardware Platform

> The MCU was verified via eFuse (`espModel`/`espCores` in the web status): **dual-core ESP32-D0WDR2-V3**. Older docs said "ESP32-SOLO1 single-core" — that is wrong and drove an entire platform-pinning saga based on a chip that isn't present.

| Component | Model | Notes |
|-----------|-------|-------|
| MCU | ESP32-D0WDR2-V3 | Dual-core Xtensa LX6 @ 240 MHz, 2 MB embedded PSRAM (see Known Issues) |
| Zigbee | TI CC2652P | Zigbee 3.0 coordinator, ZNP protocol on Serial2 |
| Ethernet | LAN8720 PHY | RMII, PoE powered |
| Flash | 16 MB | Dual OTA: 2 × 6 MB app + 4 MB LittleFS |

### 2.1 GPIO Pin Assignment (from `src/config.h`)

| Pin | GPIO | Function |
|-----|------|----------|
| CC2652P_RST | 16 | Zigbee reset, active high |
| CC2652P_FLASH | 32 | Zigbee BSL entry |
| CC2652P_RXD | 36 | Serial2 RX |
| CC2652P_TXD | 4 | Serial2 TX |
| BTN | 35 | Short = mode toggle, long 2 s = LEDs, long 4 s = BSL |
| MODE_SWITCH | 33 | HIGH = serial passthrough |
| LED_USB | 12 | Red |
| LED_PWR | 14 | Blue |
| ETH_MDC / MDIO / CLK | 23 / 18 / 17 | LAN8720 RMII |
| ETH_PWR_ALT | 5 | LAN8720 hardware reset |

### 2.2 CC2652P BSL

- BSL verify pin: **15**, trigger level: **LOW** (`NEED_BSL_PIN` / `NEED_BSL_LEVEL` in `config.h`)

---

## 3. Directory Structure

```
├── src/
│   ├── main.cpp            # setup()/loop(): boot, button/LED, coordinator mode, loop scheduler
│   ├── config.h/.cpp       # Pins, config structs, config loaders, parse_ip_address
│   ├── net.h/network.cpp   # initLan, WiFi/AP, mDNS, overseer, WireGuard, connectWifi
│   ├── bridge.cpp/h        # TCP socket bridge: ZNP frames over LAN (port 6638)
│   ├── web.h               # Web handler declarations, API page enum, UPD_FILE
│   ├── web/                # Split web server implementation
│   │   ├── common.cpp      # WebServer instance, route registration, auth, sendGzip, log
│   │   ├── api.cpp         # /api dispatch (actions 0-10), ZB firmware download
│   │   ├── pages.cpp       # getRootData() + per-page handlers (root/status/general/...)
│   │   ├── save.cpp        # /saveParams, /saveFile, wifi commit-on-connect
│   │   ├── events.cpp      # /events SSE endpoint
│   │   └── ota.cpp         # ESP OTA (web/URL) + ZB checkFwHexTask
│   ├── mqtt.cpp/h          # MQTT client + HA auto-discovery
│   ├── mqtt_cmd.h          # MQTT command dispatch (pure)
│   ├── etc.cpp/h           # LEDs, ZB reset/BSL, device ID, config write, NTP, temp sensor
│   ├── zb.cpp/h            # CC2652P: version, BSL flashing, ZNP commands, health check
│   ├── log.cpp/h           # Circular log buffer (8024 B)
│   ├── factory.cpp/h       # Factory test (BUILD_FACTORY_TEST only)
│   ├── profiling.cpp/h     # Loop-latency/heap profiler (BUILD_DEBUG only)
│   ├── checksum.h          # ZNP checksum (pure)
│   ├── encoding.h          # URL decode, hex→bin (pure)
│   ├── zb_ver.h            # ZNP version-response decode (pure)
│   ├── zones.h             # Timezone POSIX-rule table (pure)
│   ├── config_utils.h      # temp-offset clamp, WG private-key lookup (pure)
│   ├── ha_discovery.h      # HA discovery entity table + message builder (pure)
│   ├── wifi_save_logic.h   # Commit-on-connect + connect-throttle logic (pure)
│   ├── web_routes.h        # Auto-generated static asset routes
│   ├── webh/               # Auto-generated gzip C headers from websrc/
│   └── websrc/             # Frontend source (HTML/JS/CSS) - gulp build
├── lib/                    # Local libs (not PIO-registered)
│   ├── CCTools/            # CC2652P control (BSL, reset, chip detect)
│   └── IntelHex/           # Intel HEX parser for ZB flashing
├── tools/
│   ├── build.py            # Post-build: merge bootloader+partitions+firmware
│   ├── gen_version.py      # Pre-build: generate src/version.h from tools/version
│   ├── version_util.py     # Shared version/git-describe helpers
│   └── webfilesbuilder/    # Gulp frontend build
├── scripts/
│   └── ota_flash.sh        # Shared OTA upload (used by flash:ota / flash:ota:debug)
├── test/                   # Native Unity test suites
├── factory_test/           # Standalone factory-test firmware + Python host
├── platformio.ini
├── partitions.csv
├── manifest.json           # ESP Web Tools manifest
└── *.md                    # README / AGENTS / CHANGELOG / ARCHITECTURE
```

---

## 4. Core Source Modules

### 4.1 `main.cpp` — entry point and loop scheduler

`setup()`: PHY warm-restart delay → GPIO → hard-reset check → Serial2 (Zigbee) → LittleFS → load all configs → `zbInit()` → button interrupt → `setLedsDisable` → `setupCoordinatorMode()` → MQTT setup → set the configured serial baud.

`loop()` is a **strict serial scheduler** — everything runs in one iteration, so the loop cycle time *is* the gateway's end-to-end latency:

1. Button/timer handling (Ticker: `tmrBtnLongPress`, `tmrNetworkOverseer`, `tmrZbHealth`)
2. `tempSensorLoop()` — non-blocking temp state machine
3. `wifiCommitPending()` — commit a staged WiFi save once the link is up
4. `webServerHandleClient()`
5. `processSocketBridge()` + `mqttLoop()` (Network mode)
6. DNS (AP mode)
7. `delay(1)` yield (prevents a 100% busy-spin)

### 4.2 `network.cpp` / `net.h`

| Function | Purpose |
|----------|---------|
| `initLan()` | LAN8720 bring-up (hardware PHY reset on GPIO5, `ETH.begin`, DHCP/static) |
| `startServers()` | Start web + socket servers (once per boot), AP, mDNS, ZB version probe |
| `handleTmrNetworkOverseer()` | 5 s uplink check; Ethernet → WiFi STA fallback → last-resort AP |
| `NetworkEvent()` | ETH/WiFi event callback |
| `connectWifi()` / `connectWifiSta()` | STA bring-up with a 20 s connect throttle |
| `startAP()` | Soft-AP with captive-portal DNS (SSID = device ID, 192.168.1.1) |
| `wgBegin()` | WireGuard tunnel bring-up |
| `mDNS_start()` | mDNS + ZHA zeroconf (`_avatto-gw90-ti._tcp`, radio_type=znp) |

`startServers()` skips `initWebServer()` when `disableWeb` is set — the setting now actually disables the web server.

### 4.3 `bridge.cpp` — TCP socket bridge

Up to `MAX_SOCKET_CLIENTS` (5) `WiFiClient`s. Each loop iteration:

- client → Serial2: bounded read into a 256 B buffer, written to the coordinator
- Serial2 → all clients: bounded read, broadcast
- Per-frame hex dump to the web-console log (`printRecvSocket`/`printSendSocket`, allocation-free)
- Optional IP whitelist when `fwEnabled`

`zbLastRxMs`/`zbClientTxMs` are shared with `zb.cpp` for the health check.

### 4.4 `config.cpp` / `config.h`

`ConfigSettingsStruct`, `MqttSettingsStruct`, `WgSettingsStruct`, `zbVerStruct` globals. All loaders use the shared `readJsonConfig()` helper (open → deserialize → on error log + delete the file so defaults regenerate next boot). Pure helpers live in `config_utils.h` (temp-offset clamp, WG private-key key migration).

### 4.5 `web/` — web server, API, pages

`common.cpp` owns the `WebServer serverWeb(80)` instance and registers all routes (page routes, `/api`, `/status`, `/saveParams`, `/saveFile`, `/update`, `/updateZB`, `/events`, `/logout`) plus the ~50 auto-generated static asset routes (`web_routes.h`).

`pages.cpp` `getRootData()` builds the shared device-status JSON into a **reused module-level `JsonDocument`** (steady allocation, no per-poll churn). `/status` serializes it to a reused String.

`api.cpp` dispatches `/api?action=N` (see [API Endpoints](#19-api-endpoints)); includes the CC2652P firmware download (HTTPS via `NetworkClientSecure`, progress over SSE).

`events.cpp` is a single-slot SSE endpoint (`/events`), now **auth-gated** like every other protected endpoint. `sendEvent()` writes via `printf`.

`save.cpp` handles `/saveParams` (per page) and `/saveFile`. WiFi changes follow **commit-on-connect**: a *new* network is staged in RAM and only persisted after the STA link to that SSID succeeds (`wifi_save_logic.h`); tweaks to the already-saved network persist immediately.

`ota.cpp` ESP OTA (web upload + URL fetch) and the CC2652P `checkFwHexTask`.

### 4.6 `mqtt.cpp`

PubSubClient (maintained v3 fork) on a WiFiClient. Topics:

```
{topic}/avty   — LWT online/offline
{topic}/state  — periodic JSON state
{topic}/io/{n} — button/binary-sensor state
{topic}/cmd    — commands: rst_esp, rst_zig, enbl_bsl
```

HA auto-discovery is generated from the **pure entity table** in `ha_discovery.h` (buttons, socket binary sensor, sensors for uptime/IP/temp/hostname/connections/mode/zbfw) — payloads are validated by unit tests.

### 4.7 `etc.cpp` — utilities

LEDs, `zigbeeRouterRejoin`/`zigbeeEnableBSL`/`zigbeeRestart`, `getDeviceID`, `writeDefaultConfig`, `resetSettings`, `setClock`/`setTimezone` (non-blocking NTP), and the on-board temperature state machine (see [§12](#12-on-board-temperature-sensor)).

### 4.8 `zb.cpp` — Zigbee coordinator

`getZbVer()` (ZNP SYS_VERSION), `zbCheck()`/`zbLedToggle()` (shared `zbLedCommand`), `zbInit()` (chip detect via CCTools), `checkFwHex()`/`runFlash()` (Intel HEX validation + BSL flash), and the ZNP framing helpers (`zbSendCommand`, `zbGetStatus` — buffer-bounded, `zbCheckLastCmd`, `zbFlashStart`, `zbEraseFlash`, `zbSendSync`).

`zbHealthCheck()` (30 s Ticker) is **fully passive**: it only observes client↔coordinator traffic and resets the coordinator when a client is active but the coordinator is silent for 3 min (×3). It never injects frames onto the shared Serial2 line.

### 4.9 `log.cpp`

`CircularBuffer<char, 8024>` ring buffer. `printLogMsg()` (web console) uses allocation-free `printLogTime()`. `logPrint()` returns a reused String for the 1 s console poll. `logClear()`.

### 4.10 `profiling.cpp` / `profiling.h` (debug only)

Loop-cycle latency (min/EMA/max) + heap fragmentation, gated behind `BUILD_DEBUG`. See [§17](#17-profiler-debug-build).

---

## 5. Coordinator Modes

| Mode | Value | Behavior |
|------|-------|----------|
| **Network** | 0 | Zigbee over TCP socket (6638). Ethernet primary → auto WiFi STA fallback → soft AP last resort. |
| **Serial** | 1 | GPIO33 HIGH, CC2652P direct to USB host (serial passthrough). "Keep Network & Web" keeps one channel alive. |

Network mode is the default. The button short-press toggles between them (persisted + restart). `CoordinatorMode` is a scoped enum in `config.h`.

---

## 6. Configuration System

All config lives in **LittleFS** under `/config/` as JSON. Each subsystem has a loader + a `writeDefaultConfig()` fallback.

| File | Content |
|------|---------|
| `/config/system.json` | `tempOffset` calibration (see §12) |
| `/config/configWifi.json` | `ssid`, `pass`, `dhcpWiFi`, `ip`, `mask`, `gw` |
| `/config/configEther.json` | `dhcp`, `ip`, `mask`, `gw` |
| `/config/configGeneral.json` | `hostname`, `disableLeds`, `refreshLogs`, `disableLedPwr`, `disableLedUSB`, `coordMode`, `keepWeb`, `timeZoneName` |
| `/config/configSecurity.json` | `disableWeb`, `webAuth`, `webUser`, `webPass`, `fwEnabled`, `fwIp` |
| `/config/configSerial.json` | `baud`, `port` |
| `/config/configMqtt.json` | `enable`, `server`, `port`, `user`, `pass`, `topic`, `interval`, `discovery` |
| `/config/configWg.json` | `enable`, `localAddr`, `localPrivKey`, `endAddr`, `endPubKey`, `endPort` |

> The WireGuard private key is stored under `localPrivKey`; the loader still accepts the legacy `localIP` key so existing configs migrate silently.

Load order (setup): system → serial → wifi → ether → general → security → mqtt → wg. A corrupt file is deleted and defaults regenerate on the next boot.

---

## 7. Flash Partition Table

| Partition | Type | Offset | Size |
|-----------|------|--------|------|
| nvs | data | 0x9000 | 20 KB |
| otadata | data | 0xe000 | 8 KB |
| app0 | app (ota_0) | 0x10000 | 6 MB |
| app1 | app (ota_1) | 0x610000 | 6 MB |
| spiffs | data (LittleFS) | 0xc10000 | ~4 MB |

---

## 8. Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| bblanchon/ArduinoJson | 7.4.3 | JSON |
| rlogiacco/CircularBuffer | 1.4.0 | Log ring buffer |
| sstaub/Ticker | 4.4.0 | Timers (button, overseer, health) |
| hmueller01/PubSubClient3 | 3.3.0 | MQTT (maintained PubSubClient continuation) |
| Tinkerforge/WireGuard-ESP32-Arduino | git, pinned — see `platformio.ini` | WireGuard (esp_netif/IDF 5.x) |
| *local* `lib/CCTools` | — | CC2652P control |
| *local* `lib/IntelHex` | — | HEX parsing |
| *test only* throwtheswitch/Unity | 2.6.1 | Native unit tests |

Platform: `pioarduino/platform-espressif32` (pinned in `platformio.ini`; vanilla espressif core — HTTPS works out of the box).

---

## 9. Build Process & Versioning

### 9.1 Environments (`platformio.ini`)

| Env | Notes |
|-----|-------|
| `prod` | Default. `-std=gnu++17`, 240 MHz, 16 MB flash, dual-OTA partitions |
| `debug` | `-DDEBUG` + `-DBUILD_DEBUG` (enables DEBUG_PRINT + profiler) |
| `factory-test` | `-DBUILD_FACTORY_TEST` |
| `native` | Unity test suite on the host |

### 9.2 Pipeline (`extra_scripts`)

- **pre** `tools/gen_version.py` — reads `tools/version` (constant; builds never modify it), appends `-dirty` when the tree is dirty, writes `src/version.h`
- **pre** `tools/webfilesbuilder/build_html.py` + `gen_routes.py` — gulp frontend build + static route generation
- **post** `tools/build.py` — merges bootloader + partitions + app into `bin/AVATTO-GW90-Ti_vX.Y.Z.full.bin`, copies `bin/AVATTO-GW90-Ti.bin` (OTA)

Skip the web rebuild with an empty `tools/webfilesbuilder/.no_web_update` file.

### 9.3 Mise tasks

```bash
mise run build            # prod
mise run build:debug      # debug (profiler enabled)
mise run test             # native unit tests
mise run lint             # pio check (cppcheck), high-severity gate
mise run lint:full        # medium+ gate
mise run flash            # serial (UPLOAD_PORT)
mise run flash:debug      # debug over serial
mise run flash:ota        # prod OTA (OTA_DEVICE_IP)
mise run flash:ota:debug  # debug OTA
mise run monitor          # serial monitor (MONITOR_PORT)
```

`scripts/ota_flash.sh` is the shared OTA uploader used by both flash tasks.

---

## 10. Web Frontend Build

- Source: `src/websrc/` (HTML, JS, CSS, images) — jQuery, Bootstrap-derived minimal CSS, i18next
- Build: `npm install && npx gulp` in `tools/webfilesbuilder/`
- Output: gzip-compressed C headers in `src/webh/`, routes in `src/web_routes.h` (auto-generated)
- Pages are served gzip-encoded; the `[Unreleased]`-era UI is a single-page shell that fetches JSON data per page

---

## 11. Network Management

- **Overseer** (`tmrNetworkOverseer`, 5 s): in Network mode, Ethernet is preferred. If Ethernet drops, WiFi STA is brought up (if credentials exist) as a fallback; when Ethernet returns, WiFi is torn down again (less RF noise). If both fail, a soft AP starts for config recovery.
- **PASSTHROUGH mode**: WiFi for ~10 s, then `initLan()` is attempted **once** (re-running `ETH.begin()` every tick used to flap the LAN8720 PHY), then AP as last resort.
- **Ethernet init**: GPIO5 must be toggled LOW→HIGH immediately before `ETH.begin()` (the EMAC clock must be running when the PHY comes out of reset) or the PHY fails after `ESP.restart()`.
- **mDNS**: `_avatto-gw90-ti._tcp` with `radio_type=znp`, `baud_rate`, `data_flow_control=software`.

---

## 12. On-board Temperature Sensor

The ESP32 TSENS shares the SAR ADC with the RF PHY. On IDF 5.x the radio stays in modem-sleep after `esp_wifi_start()`, so `temprature_sens_read()` returns the fixed `0x80` sentinel unless the PHY is actually powered. A real read therefore powers the radio on, triggers a scan, samples, then powers it back down.

Design (all in `etc.cpp`):

- **Demand-driven**: the radio only powers up when a consumer asks — the status/home page (`getRootData()` → `getCPUtemp()`) or MQTT state publish. With neither active, the radio stays down: no loop spikes and no 2.4 GHz noise near the coordinator. A `tempRequested` flag gates the state machine.
- **Non-blocking**: `tempSensorLoop()` runs as a state machine from `loop()` (Idle → WaitSettle ~1.2 s → 5 samples ~20 ms apart → TearDown). `getCPUtemp()` never blocks; it returns the cache and requests a refresh.
- **Calibration**: on first boot the offset needed to report ~30 °C is stored in `/config/system.json` (raw die temp on this sealed PoE gateway is ~71 °C). A stale offset outside ±10 °C is clamped to 0 and recalibrated; `tempSetCalibrated()` caches the offset-adjusted value so the first page view is correct.
- Reads are capped at one per 15 s; per-read logging is `DEBUG`-only.

---

## 13. Zigbee Coordinator Management

- **Protocol**: TI Z-Stack ZNP over Serial2 (default 115 200 baud; raised to 1 000 000 during BSL flash).
- **Version**: `getZbVer()` decodes the 11-byte SYS_VERSION response (`zb_ver.h`).
- **Health**: `zbHealthCheck()` passively resets a hung coordinator (client active + 3 min silence ×3) and counts `zbAutoResets`.
- **BSL flashing**: HEX file validated with `IntelHex` (chip/BSL-pin/level checks against `NEED_BSL_PIN`/`NEED_BSL_LEVEL`), then `runFlash()` streams it in 240 B packets with gap-fill and progress events.

---

## 14. MQTT & Home Assistant Discovery

- Commands on `{topic}/cmd`: `{"cmd":"rst_esp"}` / `"rst_zig"` / `"enbl_bsl"`.
- State JSON on `{topic}/state`: uptime, temperature, connections, IP, mode, `zbfw`, hostname.
- HA discovery payloads are built by `buildHaDiscoveryMessage()` from the `ha_discovery.h` entity table (retained, topic-per-entity, `stat_t`/`cmd_t`/`avty_t`/`dev`). Every button `payload_press` is valid JSON carrying `cmd` — enforced by unit tests.

---

## 15. WireGuard

`wgBegin()` uses the Tinkerforge fork's explicit API (`/32` host subnet, local port 51820, no gateway) after the network is up. Config in `/config/configWg.json`.

---

## 16. OTA & Releases

- **Web**: `POST /update` (multipart `AVATTO-GW90-Ti.bin`) or URL fetch (`/api?cmd=10`).
- **ZB**: `POST /updateZB` or `/api?action=10&fwurl=...`.
- **Release flow** (`scripts/release.sh` + the GitHub `Release` workflow):
  1. Bump `tools/version` + `manifest.json`, commit + tag `vX.Y.Z` (workflow_dispatch with `patch`/`minor`/`major`/`X.Y.Z`)
  2. Build `prod` with the pinned version
  3. Attach `bin/*.bin` to the GitHub release

---

## 17. Profiler (Debug Build)

Gated behind `-DBUILD_DEBUG` — **production firmware ships zero instrumentation**.

- `profilingLoopBegin()` / `profilingLoopTick()` bracket each `loop()` iteration; the loop is a strict serial scheduler, so the cycle time is the gateway's end-to-end latency.
- Tracks min / EMA / max over a rolling 60 s window; every minute a `PROF` line is logged (loop latency + free/min/largest heap + fragmentation %).
- The status API exposes `loopMinUs`/`loopAvgUs`/`loopMaxUs`, `espHeapMinFree`, `espHeapLargest`, `espHeapFrag`.
- Stats math is pure `profiling.h` (unit-tested).

Observed on a live PoE gateway: loop min ~1.26 ms, typical ~2 ms, ~30 ms periodic spikes from the temp-sensor radio cycle, heap ~187 K free / ~43 % fragmentation (stable).

---

## 18. Testing

`mise run test` runs the Unity suite on the host (`native` env). Only pure logic is testable (no hardware). The suite covers:

- `encoding` (urlDecode, hex2bin) · `checksum` (ZNP) · `zb_ver` (version decode) · `zones` (TZ table)
- `config_utils` (temp clamp, WG key fallback) · `ha_discovery` (entity table + message/topic shape + payload JSON validity)
- `wifi_save_logic` (commit-on-connect modes, connect throttle) · `profiling` (stats math) · `log` (ring buffer) · `api_dispatch` (API numbering) · `mqtt_cmd` (command dispatch)

`mise run lint` runs cppcheck with `--skip-packages` (cppcheck 2.21 cannot parse the GCC 14.2 toolchain's `type_traits`).

---

## 19. API Endpoints

| Route | Description |
|-------|-------------|
| `GET /api?action=0&page=N` | Page data (JSON headers) |
| `GET /api?action=1&param=X` | Single param (`refreshLogs`, `coordMode`, `zbRev`, `espVer`) |
| `GET /api?action=2` | Start WiFi scan |
| `GET /api?action=3` | WiFi scan status/results |
| `GET /api?action=4` | `/config` file list |
| `GET /api?action=5&filename=X` | Read a config file |
| `GET /api?action=7` | WiFi connect status |
| `GET /api?action=8&cmd=N` | Execute command (see below) |
| `GET /api?action=9` | Log buffer |
| `GET /api?action=10&fwurl=X` | Download + flash CC2652P firmware from URL |
| `POST /saveParams` | Save config (pageId) |
| `POST /saveFile` | Write a config file |
| `POST /update` | ESP32 OTA upload |
| `POST /updateZB` | CC2652P upload |
| `GET /events` | SSE event stream (auth-gated) |
| `GET /status` | Full device status JSON |
| `GET /logout` | Logout |

**Commands** (`cmd`): 0 router rejoin · 1 ZB restart · 2 ZB BSL · 3 ESP restart · 6 LED power toggle · 7 LED USB toggle · 8 clear log · 9 ESP OTA from URL · 10 ZB check rev · 11 ZB check conn · 12 ZB LED toggle.

---

## 20. Known Issues & Precautions

- **LAN8720 PHY**: GPIO5 must be toggled LOW→HIGH before `ETH.begin()` or the PHY fails after `ESP.restart()`.
- **cppcheck vs GCC 14.2**: `pio check` requires `--skip-packages`.
- **PSRAM intentionally disabled**: the D0WDR2-V3's 2 MB PSRAM routes through GPIO16/17, which conflict with `CC2652P_RST` (GPIO16). Enabling PSRAM would break the Zigbee reset line.
- **`/events` auth**: `EventSource` can't send custom headers; browsers replay cached HTTP-auth credentials for the origin, so an already-logged-in session works, and unauthenticated clients get a 401 + the frontend's polling fallback.
- **Temp reads are demand-driven**: the reported temperature may be the cached boot value on the very first page view after a long idle, and only refreshes while a page or MQTT is active.
- **Version is constant**: `tools/version` is only bumped by the release workflow; dirty-tree builds carry a `-dirty` suffix.
