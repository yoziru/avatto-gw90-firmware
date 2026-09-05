# AVATTO GW90-TI — ESP32 Zigbee Gateway Firmware

Firmware for the **AVATTO GW90-TI** Zigbee PoE gateway. Bridges a **TI CC2652P** coordinator to your network over Ethernet (PoE powered) with automatic WiFi fallback, compatible with **Home Assistant (ZHA)** and **Zigbee2MQTT**.

## Hardware

| Component | Detail |
|-----------|--------|
| MCU | ESP32-D0WDR2-V3 (dual-core Xtensa LX6 @ 240 MHz, 2 MB PSRAM) — verified via eFuse; older docs said "ESP32-SOLO1 single-core" which is incorrect |
| Zigbee | TI CC2652P (Zigbee 3.0) |
| Ethernet | LAN8720 PHY (RMII), **PoE powered** |
| Flash | 16 MB (Dual OTA: 2 × 6 MB app + 4 MB spiffs) |

Pin mapping in `src/config.h`, full technical reference in `ARCHITECTURE.md`.

## Quick Start

```bash
mise run setup            # Install deps (uv sync + build group)
mise run build            # Build prod (default)
mise run build:debug      # Debug build (-DDEBUG, profiler enabled)
mise run test             # Native unit tests (Unity)
mise run lint             # Static analysis via cppcheck (fails on high-severity defects)
mise run lint:full        # Stricter medium+ gate (skips platform packages)
export UPLOAD_PORT=/dev/cu.usbserial-XXXX
mise run flash            # Serial flash (first time)
mise run flash:ota        # OTA over network (needs OTA_DEVICE_IP, see below)
```

Outputs in `bin/`:
- `AVATTO-GW90-Ti.bin` — OTA firmware (for network flashing)
- `AVATTO-GW90-Ti_vX.Y.Z.full.bin` — Full image (bootloader + partitions + app)

## Network Flash (OTA)

```bash
export OTA_DEVICE_IP=192.168.1.18        # IP of the gateway on your LAN
mise run flash:ota                        # builds + uploads + waits for reboot
mise run flash:ota:debug                  # same, but builds + flashes the debug/profiler firmware
# Optional: pass web-auth credentials if enabled on the device
export OTA_USER=admin OTA_PASS=secret
```

Alternatively, once the device is on your network at `http://<ip>`:

1. **Web UI**: `http://<ip>/sys-tools` → upload `AVATTO-GW90-Ti.bin`
2. **API**: `curl "http://<ip>/api?cmd=10&url=http://your-server/AVATTO-GW90-Ti.bin"`

Only the non-full `.bin` (offset 0x10000) works for OTA.

## Network Logs

- **API**: `GET /api?action=7` returns the log ring (8 KB events; `&src=zb` for the 8 KB ZB frame ring)
- **SSE**: `GET /events` for real-time events (firmware progress)
- **Web UI**: web console reads logs via the API

No syslog/telnet/UDP log forwarding.

## Modes

| Mode | Value | Description |
|------|-------|-------------|
| **Network** | 0 | Zigbee over TCP socket (port 6638). Ethernet primary, automatic WiFi STA fallback when Ethernet drops, soft AP as last resort |
| **Serial** | 1 | CC2652P direct to USB host (serial passthrough) |

Configure at `http://<ip>/general`.

## Releases

Releases are cut from `main` via the GitHub **Release** workflow (`patch`/`minor`/`major` or `X.Y.Z`), which bumps `tools/version` + `manifest.json`, renames the `CHANGELOG.md` `[Unreleased]` section (if non-empty) to the released version, tags `vX.Y.Z`, builds prod, and attaches the firmware to a GitHub release whose notes come straight from that changelog section. Local equivalent: `scripts/release.sh <patch|minor|major|X.Y.Z>` (main branch only).

## Project Structure

| Path | Purpose |
|------|---------|
| `src/main.cpp` | Entry point, button/LED handling, loop scheduler |
| `src/network.cpp` / `net.h` | Ethernet, WiFi/AP, mDNS, overseer, WireGuard |
| `src/bridge.cpp` | TCP socket bridge (ZNP frames over LAN, port 6638) |
| `src/config.cpp` / `config.h` | Pins, config structs + JSON loaders |
| `src/web/` | Web server split: `common` (routes/auth), `api`, `pages`, `save`, `events` (SSE), `ota` |
| `src/mqtt.cpp` | MQTT client + HA auto-discovery |
| `src/etc.cpp` | Utilities: LEDs, ZB reset/BSL, NTP, temp sensor |
| `src/zb.cpp` | CC2652P management, BSL flashing, health check |
| `src/log.cpp` | Circular log buffer (8024 B) |
| `src/profiling.*` | Loop-latency/heap profiler (**debug build only**) |
| `src/{checksum,encoding,zb_ver,zones,config_utils,ha_discovery,wifi_save_logic,mqtt_cmd}.h` | Pure, unit-tested logic headers |
| `src/websrc/` | Web frontend source (HTML/CSS/JS) |
| `src/webh/` | Gzip-compressed web assets as C headers |
| `test/` | Native Unity test suites |

Full details: `ARCHITECTURE.md`.

## License & Attribution

This project is based on [AVATTO's official GW90-TI firmware](https://github.com/AVATTO-smart/avatto-gw90-firmware), with extensive modifications for stability, Home Assistant integration, and developer tooling. Changes relative to upstream are documented in `CHANGELOG.md`.

- Licensed under the [GNU General Public License v3.0](LICENSE), inherited from the upstream project — all modifications are distributed under the same license.
- **Not affiliated with or endorsed by AVATTO.** "AVATTO" and "GW90" are trademarks of their respective owners, used here solely to identify compatible hardware.
