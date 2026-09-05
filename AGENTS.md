# AGENTS.md — AI Agent Guide for avatto-gw90-firmware

## Project Overview

ESP32 firmware for the AVATTO GW90-TI Zigbee Gateway. Bridges a TI CC2652P Zigbee coordinator to the network (Ethernet/WiFi/USB) for use with Home Assistant (ZHA/Zigbee2MQTT).

> **Hardware correction:** The AVATTO GW90-TI is built on a **dual-core ESP32-D0WDR2-V3** (2 cores, 2 MB PSRAM), verified via the chip's eFuse (`espModel`/`espCores` in the web status). Older docs said "ESP32-SOLO1 single-core" — that was wrong and drove an entire platform-pinning saga based on a single-core chip that isn't present.

## Build System

| Tool | Details |
|------|---------|
| **Platform** | `pioarduino/platform-espressif32` (pinned in `platformio.ini`; vanilla espressif Arduino core, not the tasmota fork) |
| **Framework** | Arduino for ESP32 |
| **Default env** | `prod` |
| **Flash size** | 16MB |
| **Partitions** | Dual OTA (2×6MB app partitions + 4MB spiffs) |

> **Why this platform:** it ships the vanilla espressif Arduino Core, so `NetworkClientSecure`/HTTPS and `HTTPClient` work out of the box, and the underlying IDF provides the `esp_netif` APIs the Tinkerforge WireGuard fork needs. The tasmota fork strips TLS (`CONFIG_MBEDTLS_TLS_DISABLED=y`), so it cannot do HTTPS firmware downloads.

## Build & Flash Commands

```bash
mise run build           # Build prod (default)
mise run build:debug     # Build debug
mise run flash           # Serial flash (set UPLOAD_PORT env var)
mise run flash:debug     # Debug flash (set UPLOAD_PORT env var)
mise run flash:ota       # OTA flash over network (set OTA_DEVICE_IP; optional OTA_USER/OTA_PASS)
mise run flash:ota:debug # OTA flash of the debug/profiler build (same env vars)
mise run test            # Native unit tests (Unity)
mise run lint            # Static analysis via pio check/cppcheck (fails on high severity)
mise run lint:full       # cppcheck medium+ gate (skips platform packages)
mise run monitor         # Serial monitor (set MONITOR_PORT env var)
mise run clean           # Clean artifacts
mise run setup           # uv sync --group build
```

> **Note:** `pio check` needs `--skip-packages` — cppcheck 2.21 cannot parse the GCC 14.2 toolchain's `type_traits` header (a tool limitation, not a project defect). Both `mise run lint` and the CI lint job already pass `--skip-packages`.

Set `UPLOAD_PORT=/dev/cu.usbserial-XXXX` before flashing.

### Network Flash (No Serial)

1. **Web UI**: Browse to `http://<device-ip>/sys-tools`, upload `bin/AVATTO-GW90-Ti.bin`
2. **API**: `curl "http://<ip>/api?cmd=10&url=http://your-server/AVATTO-GW90-Ti.bin"`

Only the non-full `.bin` at offset 0x10000 is used for OTA. The `.full.bin` is for serial flashing only.

### Network Logs

- **API**: `GET /api?action=7` returns the log ring (8KB events; `&src=zb` for the 8KB ZB frame ring). The web console fetches both and merges by timestamp
- **SSE**: `/events` endpoint for real-time server-sent events (used for firmware progress)
- **Web UI**: The web console page reads logs via the API
- **Serial**: `pio device monitor` at 115200 baud

There is no syslog/telnet/UDP log forwarding. The log buffer is a `CircularBuffer<char, 8024>` in `log.cpp`.

## Project Structure

```
src/
├── main.cpp        Entry point: setup(), loop(), button/LED handling
├── config.h        Pin definitions, config structs, config loaders + parse_ip_address decls
├── config.cpp      ConfigSettings/MqttSettings/WgSettings globals + loadConfig*() + parse_ip_address()
├── net.h/network.cpp  Network: initLan, WiFi/AP, mDNS, overseer, WireGuard
├── bridge.h/cpp    TCP socket bridge: accept/forward ZNP frames over LAN
├── factory.h/cpp   Factory test mode (BUILD_FACTORY_TEST only)
├── web.h           Web server declarations, API page enum, UPD_FILE URL
├── web/            Web server split: common (routes/auth), api, pages, save, events (SSE), ota
├── mqtt.cpp        MQTT client, HA auto-discovery
├── mqtt_cmd.h      MQTT command dispatch (pure, unit-testable)
├── etc.cpp         Utilities: LED control, ZB reset/BSL, config, NTP, temp sensor
├── zb.cpp          CC2652P management: version, BSL flashing, ZNP protocol, health check
├── log.cpp         Circular buffer log (8024 bytes)
├── profiling.h/cpp Loop-latency/heap profiler (BUILD_DEBUG only)
├── zones.h         Timezone data table
├── checksum.h      ZNP checksum (pure)
├── encoding.h      URL decode, hex→bin (pure)
├── zb_ver.h        ZNP version-response decode (pure)
├── config_utils.h  temp-offset clamp, WG private-key lookup (pure)
├── ha_discovery.h  HA discovery entity table + message builder (pure)
├── wifi_save_logic.h  WiFi commit-on-connect + connect throttle (pure)
├── websrc/         Frontend source (HTML/CSS/JS) — staged+gzip'd by build_html.py
└── webh/           Auto-generated gzip-compressed C headers from websrc/

tools/
├── build.py                 Post-build: merge bootloader + partitions + firmware
├── gen_version.py           Pre-build: generate src/version.h from tools/version (read-only; -dirty when tree is dirty)
├── version_util.py          Shared version read + git describe helper (used by gen_version.py and build.py)
└── webfilesbuilder/         Python-only web frontend build (no npm/node)

scripts/
└── ota_flash.sh             Shared OTA uploader (used by flash:ota / flash:ota:debug)
```

## Code Conventions

- **Language**: C++ (Arduino framework)
- **Formatting**: Arduino-style (opening brace on same line for functions)
- **Naming**: CamelCase for functions, PascalCase for structs, UPPER_CASE for macros
- **Headers**: Include guards with `#ifndef CONFIG_H_` style
- **Debug prints**: Use `DEBUG_PRINT("msg")` / `DEBUG_PRINTLN("msg")` macros (gated by `-DDEBUG` flag)
- **Config storage**: JSON files in LittleFS `/config/` directory. Each subsystem has its own config file and `loadConfig*()` function
- **Logging**: `printLogMsg()` writes to the circular buffer (accessible via web API)
- **Serial2**: Used for CC2652P Zigbee communication at configured baud rate (default 115200)
- **No standard library pattern**: Raw Arduino, no classes for most modules — free functions in `.cpp` files

## Key Configuration Files (LittleFS)

| File | Content |
|------|---------|
| `/config/system.json` | Temp offset calibration |
| `/config/configWifi.json` | WiFi credentials |
| `/config/configEther.json` | Ethernet DHCP/static IP |
| `/config/configGeneral.json` | Hostname, coordinator mode, LED settings |
| `/config/configSecurity.json` | Web auth, IP whitelist |
| `/config/configSerial.json` | Baud rate, TCP port (default 6638) |
| `/config/configMqtt.json` | MQTT broker settings |
| `/config/configWg.json` | WireGuard tunnel config |

## Coordinator Modes

| Mode | Value | Behavior |
|------|-------|----------|
| Network | 0 | Zigbee over TCP socket (port 6638). Ethernet primary → automatic WiFi STA fallback → AP last resort |
| Serial | 1 | GPIO33 HIGH, CC2652P direct to USB host (serial passthrough) |

Network mode is the default. If Ethernet drops, the overseer automatically brings up the configured WiFi STA as a secondary uplink; when Ethernet returns it switches back and turns WiFi off (less RF noise). If both fail, a soft AP starts for config recovery. Serial mode's "Keep Network & Web" checkbox keeps one channel alive (WiFi → Ethernet → AP).

## Pin Mapping (ESP32-D0WDR2-V3)

| Function | GPIO | Notes |
|----------|------|-------|
| CC2652P_RST | 16 | Active HIGH |
| CC2652P_FLASH | 32 | BSL mode entry |
| CC2652P_RXD | 36 | Serial2 RX |
| CC2652P_TXD | 4 | Serial2 TX |
| BTN | 35 | Short=Serial toggle, Long2s=LED, Long4s=BSL |
| MODE_SWITCH | 33 | HIGH=serial passthrough |
| LED_USB | 12 | Red |
| LED_PWR | 14 | Blue |
| ETH_MDC/MDIO/CLK | 23/18/17 | LAN8720 RMII |

## API Endpoints

| Route | Method | Description |
|-------|--------|-------------|
| `/api?action=6&cmd=N` | GET | Execute command (cmd 0-12) |
| `/api?action=7` | GET | Get log buffer |
| `/update` | POST | ESP32 OTA firmware upload |
| `/updateZB` | POST | CC2652P firmware upload |
| `/events` | GET | SSE event stream |
| `/saveParams` | POST | Save config form |
| `/api?action=0&page=X` | GET | Get page data as JSON headers |

## OTA Architecture

- **Dual partition**: `app0` (0x10000, 6MB), `app1` (0x610000, 6MB)
- **Web upload**: `POST /update` with multipart firmware
- **Remote OTA**: `CMD_ESP_UPD_URL` (cmd=10) fetches from URL (default GitHub release)
- **OTA binary**: `bin/AVATTO-GW90-Ti.bin` (firmware only, ~6MB max)
- **Full flash binary**: `bin/AVATTO-GW90-Ti_v{ver}.full.bin` (bootloader + partitions + firmware)

## Release Flow

1. **Manual builds**: `mise run build` reads `tools/version` as-is (never modifies it); output in `bin/`: OTA binary + full flash binary.
2. **Automated releases**: trigger the `Release` workflow on GitHub (Actions → Release → Run workflow) with `patch`/`minor`/`major` (or explicit `X.Y.Z`). It bumps `tools/version` + `manifest.json`, renames a non-empty `CHANGELOG.md` `[Unreleased]` section to the released version, tags `vX.Y.Z`, builds `prod` with the version pinned, and creates a GitHub release whose notes are that changelog section (falls back to generated notes if empty). Local script: `scripts/release.sh <patch|minor|major|X.Y.Z>` (main branch only).
3. Update `manifest.json` for ESP Web Tools — done automatically by the release workflow.

## Web Frontend

- **Source**: `src/websrc/` (HTML, JS, CSS, images)
- **Build**: automatic PlatformIO pre-step — `tools/webfilesbuilder/build_html.py` stages `src/websrc/` and gzips each file into a header (pure stdlib, no npm/node)
- **Output**: gzip-compressed C header files in `src/webh/`
- **To skip**: create `tools/webfilesbuilder/.no_web_update` (saves build time)
- **Tech**: hand-rolled JS (`functions.js`), `water.css` + custom `style.css` (dark mode via `data-theme`)

## Changelog

Keep `CHANGELOG.md` up to date with every meaningful change. Follow the existing format:
- `[Unreleased]` for pending work
- Versioned sections for releases (`## vX.Y.Z — YYYY-MM-DD`)
- Tag entries with PR numbers where applicable: `(#N)`

Write entries for users, not developers. The changelog doubles as the GitHub release notes, so `[Unreleased]` should tell people what changed for them. Implementation detail belongs in Improvements/Chores.

## Known Issues

- **LAN8720 PHY**: GPIO5 must be toggled LOW→HIGH before `ETH.begin()` or it fails after `ESP.restart()`.
- **cppcheck vs GCC 14.2**: `pio check` without `--skip-packages` fails parsing the toolchain's `type_traits` (cppcheck 2.21 limitation). Always use `--skip-packages`.
- **Native tests**: `mise run test` runs the Unity suite (native env). Only pure logic is testable (no hardware deps).
- **CPU temp**: The TSENS shares the SAR ADC with the RF PHY, so a real read requires powering the radio. Reads are **demand-driven** (only when the status page or MQTT asks) and **non-blocking** (state machine in `loop()`, `tempSensorLoop`); the reported value is a cache calibrated against ~30 °C (`/config/system.json`).
- **Version is constant**: `tools/gen_version.py` reads `tools/version` as-is (no auto-increment). Only the release workflow bumps it; builds from a dirty working tree get a `-dirty` suffix (`tools/version_util.py`'s `describe()`), so they're distinguishable from clean CI/release builds.
