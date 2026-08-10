# AVATTO GW90-PoE-Ti — ESP32 Zigbee Gateway Firmware

Firmware for the **AVATTO GW90-PoE-Ti** Zigbee/Thread PoE dongle. Bridges a **TI CC2652P** coordinator to your network over Ethernet (PoE powered), WiFi, or USB, compatible with **Home Assistant (ZHA)** and **Zigbee2MQTT**.

## Hardware

| Component | Detail |
|-----------|--------|
| MCU | ESP32-D0WDR2-V3 (dual-core Xtensa LX6, 240 MHz, 2 MB PSRAM) — verified via eFuse; earlier docs said "ESP32-SOLO1 single-core 160 MHz", which is incorrect |
| Zigbee | TI CC2652P (Zigbee 3.0 / Thread) |
| Ethernet | LAN8720 PHY (RMII), **PoE powered** |
| Flash | 16 MB (Dual OTA: 2 × 6 MB app + 4 MB spiffs) |

Pin mapping in `src/config.h`, architecture docs in `ARCHITECTURE.md`.

## Quick Start

```bash
mise run setup        # Install deps (uv sync + build group)
mise run build        # Build prod (default)
mise run build:debug  # Debug build (-DDEBUG)
mise run test         # Run native unit tests (Unity)
mise run lint         # Static analysis via cppcheck (fails on high-severity defects)
mise run lint:full    # Stricter medium+ gate (skips platform packages)
export UPLOAD_PORT=/dev/cu.usbserial-XXXX
mise run flash        # Serial flash (first time)
mise run flash:ota    # OTA over network (needs OTA_DEVICE_IP, see below)
```

Outputs in `bin/`:
- `AVATTO-GW90-Ti.bin` — OTA firmware (for network flashing)
- `AVATTO-GW90-Ti_vX.Y.Z.full.bin` — Full image (bootloader + partitions + app)

## Network Flash (OTA)

```bash
export OTA_DEVICE_IP=192.168.1.18        # IP of the gateway on your LAN
mise run flash:ota                        # builds + uploads + waits for reboot
# Optional: pass web-auth credentials if enabled on the device
export OTA_USER=admin OTA_PASS=secret
```

Alternatively, once the device is on your network at `http://<ip>`:

1. **Web UI**: `http://<ip>/sys-tools` → upload `AVATTO-GW90-Ti.bin`
2. **API**: `curl "http://<ip>/api?cmd=10&url=http://your-server/AVATTO-GW90-Ti.bin"`

Only the non-full `.bin` (offset 0x10000) works for OTA.

## Network Logs

- **API**: `GET /api?action=9` returns the circular log buffer (8 KB)
- **SSE**: `GET /events` for real-time events (firmware progress)
- **Web UI**: web console reads logs via the API

No syslog/telnet/UDP log forwarding.

## Modes

| Mode | Value | Description |
|------|-------|-------------|
| **Network** | 0 | Zigbee over TCP socket (port 6638). Ethernet primary, automatic WiFi STA fallback when Ethernet drops, soft AP as last resort |
| **Serial** | 1 | CC2652P direct to USB host (serial passthrough) |

Configure at `http://<ip>/general`.

## Project Structure

| Path | Purpose |
|------|---------|
| `src/main.cpp` | Entry point, TCP bridge, network init |
| `src/web.cpp` | Web server, API, OTA, SSE |
| `src/mqtt.cpp` | MQTT client + HA auto-discovery |
| `src/etc.cpp` | Utilities (LED, config, NTP, ZB control) |
| `src/zb.cpp` | CC2652P management and BSL flashing |
| `src/log.cpp` | Circular log buffer |
| `src/websrc/` | Web frontend source (HTML/JS/CSS) |
| `src/webh/` | Gzip-compressed web assets as C headers |
