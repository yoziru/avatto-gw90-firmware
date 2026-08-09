# AGENTS.md — AI Agent Guide for avatto-gw90-firmware

## Project Overview

ESP32-SOLO1 firmware for the AVATTO GW90-TI Zigbee Gateway. Bridges a TI CC2652P Zigbee coordinator to the network (Ethernet/WiFi/USB) for use with Home Assistant (ZHA/Zigbee2MQTT).

## Build System

| Tool | Details |
|------|---------|
| **Platform** | `tasmota/platform-espressif32 @ 2023.07.00` (Solo1) / `espressif32 @ 6.4.0` (dev board) |
| **Framework** | Arduino for ESP32 |
| **Default env** | `prod-solo` |
| **Flash size** | 16MB |
| **Partitions** | Dual OTA (2×6MB app partitions + 4MB spiffs) |

### CRITICAL: Platform Version Constraint

The ESP32-SOLO1 board requires `tasmota/platform-espressif32 @ 2023.07.00`. Versions after `2023.08.00` break HTTPS (`WiFiClientSecure`). The `espressif32 @ 6.4.0` platform does not support the `esp32-solo1` board. This is documented with extensive trial logs in `platformio.ini:12-22`.

## Build & Flash Commands

```bash
mise run build           # Build prod-solo (default)
mise run build:debug     # Build debug-solo
mise run flash           # Serial flash (set UPLOAD_PORT env var)
mise run flash:debug     # Debug flash (set UPLOAD_PORT env var)
mise run flash:ota       # OTA flash over network (set OTA_DEVICE_IP; optional OTA_USER/OTA_PASS)
mise run test            # Native unit tests (Unity)
mise run lint            # Static analysis via pio check/cppcheck (fails on high severity)
mise run lint:full       # cppcheck medium+ gate (skips platform packages)
mise run monitor         # Serial monitor (set MONITOR_PORT env var)
mise run clean           # Clean artifacts
mise run setup           # uv sync --group build
```

Set `UPLOAD_PORT=/dev/cu.usbserial-XXXX` before flashing.

### Network Flash (No Serial)

1. **Web UI**: Browse to `http://<device-ip>/sys-tools`, upload `bin/AVATTO-GW90-Ti.bin`
2. **API**: `curl "http://<ip>/api?cmd=10&url=http://your-server/AVATTO-GW90-Ti.bin"`

Only the non-full `.bin` at offset 0x10000 is used for OTA. The `.full.bin` is for serial flashing only.

### Network Logs

- **API**: `GET /api?action=9` returns the circular log buffer (8KB)
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
├── network.h/cpp   Network: initLan, WiFi/AP, mDNS, overseer, WireGuard
├── bridge.h/cpp    TCP socket bridge: accept/forward ZNP frames over LAN
├── factory.h/cpp   Factory test mode (BUILD_FACTORY_TEST only)
├── ip_utils.h      Pure IPv4 parser (unit-testable)
├── web.cpp         Web server (2145 lines), API, OTA, SSE events
├── web.h           Web server declarations, UPD_FILE URL
├── mqtt.cpp        MQTT client, HA auto-discovery
├── etc.cpp         Utilities: LED control, ZB reset/BSL, config, NTP
├── zb.cpp          CC2652P management: version, BSL flashing, ZNP protocol
├── log.cpp         Circular buffer log (8024 bytes)
├── zones.h         Timezone data table
├── websrc/         Frontend source (HTML/CSS/JS) — processed by gulp
└── webh/           Auto-generated gzip-compressed C headers from websrc/

tools/
├── build.py                 Post-build: merge bootloader + partitions + firmware
├── version_increment_pre.py Pre-build: auto-increment version in tools/version
├── webfilesbuilder/         Gulp-based web frontend build (npm install + npx gulp)
├── flash.sh                 Manual serial flash script
└── esptool.py               ESP flasher
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
| LAN | 0 | Zigbee over Ethernet TCP socket (default port 6638) |
| WiFi | 1 | Zigbee over WiFi TCP socket |
| USB | 2 | GPIO33 HIGH, CC2652P direct to USB host |

## Pin Mapping (ESP32-SOLO1)

| Function | GPIO | Notes |
|----------|------|-------|
| CC2652P_RST | 16 | Active HIGH |
| CC2652P_FLASH | 32 | BSL mode entry |
| CC2652P_RXD | 36 | Serial2 RX |
| CC2652P_TXD | 4 | Serial2 TX |
| BTN | 35 | Short=USB toggle, Long2s=LED, Long4s=BSL |
| MODE_SWITCH | 33 | HIGH=USB passthrough |
| LED_USB | 12 | Red |
| LED_PWR | 14 | Blue |
| ETH_MDC/MDIO/CLK | 23/18/17 | LAN8720 RMII |

## API Endpoints

| Route | Method | Description |
|-------|--------|-------------|
| `/api?action=8&cmd=N` | GET | Execute command (cmd 0-12) |
| `/api?action=9` | GET | Get log buffer |
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

1. **Manual builds**: `mise run build` auto-increments the patch version in `tools/version`; output in `bin/`: OTA binary + full flash binary.
2. **Automated releases**: trigger the `Release` workflow on GitHub (Actions → Release → Run workflow) with `patch`/`minor`/`major` (or explicit `X.Y.Z`). It bumps `tools/version` + `manifest.json`, tags `vX.Y.Z`, builds `prod-solo` with the version pinned, and creates a GitHub release with the firmware attached. Local script: `scripts/release.sh <patch|minor|major|X.Y.Z>` (main branch only).
3. Update `manifest.json` for ESP Web Tools — done automatically by the release workflow.

## Web Frontend

- **Source**: `src/websrc/` (HTML, JS, CSS, images)
- **Build**: `npm install && npx gulp` in `tools/webfilesbuilder/`
- **Output**: gzip-compressed C header files in `src/webh/`
- **To skip**: create `tools/webfilesbuilder/.no_web_update` (saves build time)
- **Tech**: jQuery, Bootstrap, i18next, Gulp

## Changelog

Keep `CHANGELOG.md` up to date with every meaningful change. Follow the existing format:
- `[Unreleased]` for pending work
- Versioned sections for releases (`## vX.Y.Z — YYYY-MM-DD`)
- Tag entries with PR numbers where applicable: `(#N)`

## Known Issues

- **HTTPS on Solo1**: Only works with `tasmota/platform-espressif32 @ 2023.07.00`. Newer versions break `WiFiClientSecure`.
- **LAN8720 PHY**: GPIO5 must be toggled LOW→HIGH before `ETH.begin()` or it fails after `ESP.restart()`.
- **No tests**: No test framework exists in the project.
- **CPU temp**: Requires WiFi to be active; `getCPUtemp()` temporarily enables WiFi to read.
- **Version auto-increment**: Patch version bumps every build. Create `tools/.version_no_increment` to skip.
