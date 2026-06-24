# AVATTO GW90 Zigbee Gateway Firmware — Technical Handover Document

> **Project Name**: avatto-gw90-firmware (PREGW90)  
> **Current Version**: v1.0.1  
> **Firmware Build Date**: 2024-03-26  
> **Base Repository**: mercenaruss/uzg-firmware (fork)  
> **Document Date**: 2026-02-24

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Hardware Platform](#2-hardware-platform)
3. [Development Environment & Toolchain](#3-development-environment--toolchain)
4. [Project Directory Structure](#4-project-directory-structure)
5. [Core Source Modules](#5-core-source-modules)
6. [Configuration System](#6-configuration-system)
7. [Flash Partition Table](#7-flash-partition-table)
8. [Third-Party Library Dependencies](#8-third-party-library-dependencies)
9. [Build Process & Automation Scripts](#9-build-process--automation-scripts)
10. [Web Frontend Build](#10-web-frontend-build)
11. [Firmware Features](#11-firmware-features)
12. [MQTT Integration](#12-mqtt-integration)
13. [WireGuard VPN Support](#13-wireguard-vpn-support)
14. [Zigbee Coordinator Management](#14-zigbee-coordinator-management)
15. [OTA & Firmware Updates](#15-ota--firmware-updates)
16. [Button & LED Interaction](#16-button--led-interaction)
17. [Network Connection Management](#17-network-connection-management)
18. [Debugging Methods](#18-debugging-methods)
19. [Known Issues & Precautions](#19-known-issues--precautions)
20. [Future Development Suggestions](#20-future-development-suggestions)

---

## 1. Project Overview

This project is the **AVATTO GW90-TI** Zigbee gateway's ESP32 firmware, forked and customized from the open-source project `mercenaruss/uzg-firmware`. The core function bridges a **CC2652P Zigbee coordinator** to the network (LAN/WiFi/USB) via the ESP32, making it usable by Home Assistant (ZHA/Zigbee2MQTT) and other smart home platforms.

**Main Features:**
- Three coordinator operating modes: LAN (Ethernet), WiFi, USB direct
- Embedded web management interface (network, serial, security, MQTT configuration, etc.)
- MQTT integration with Home Assistant auto-discovery
- WireGuard VPN tunnel support
- CC2652P Zigbee chip firmware flashing (BSL mode)
- mDNS service discovery
- TCP socket passthrough (Zigbee data bridging)

---

## 2. Hardware Platform

### 2.1 Main Components

| Component | Model | Description |
|-----------|-------|-------------|
| Main MCU | **ESP32-SOLO1** | Single-core Xtensa LX6, 160MHz |
| Zigbee Coordinator | **CC2652P** | TI, supports Zigbee 3.0 |
| Ethernet PHY | **LAN8720** | Connected via RMII interface |
| Flash | **16MB** | External SPI Flash |

### 2.2 GPIO Pin Assignment

| Pin | GPIO | Function | Description |
|-----|------|----------|-------------|
| CC2652P_RST | GPIO 16 | Zigbee reset | Output, active high |
| CC2652P_FLASH | GPIO 32 | Zigbee BSL mode | Output, used to enter flash mode |
| CC2652P_RXD | GPIO 36 | Zigbee serial RX | Serial2 receive |
| CC2652P_TXD | GPIO 4 | Zigbee serial TX | Serial2 transmit |
| BTN | GPIO 35 | User button | Input, falling-edge interrupt |
| MODE_SWITCH | GPIO 33 | USB/serial mode switch | Output, HIGH=USB mode |
| LED_USB | GPIO 12 | Red LED | USB mode indicator |
| LED_PWR | GPIO 14 | Blue LED | Power indicator |
| ETH_MDC | GPIO 23 | LAN8720 MDC | Ethernet management |
| ETH_MDIO | GPIO 18 | LAN8720 MDIO | Ethernet management |
| ETH_CLK | GPIO 17 | LAN8720 clock output | `ETH_CLOCK_GPIO17_OUT` |
| ETH_PWR_ALT | GPIO 5 | LAN8720 power control | Hardware reset LAN PHY |

### 2.3 CC2652P BSL Pin

- BSL verify pin: CC2652 Pin **15**
- BSL trigger level: **LOW** (0)

---

## 3. Development Environment & Toolchain

### 3.1 IDE & Framework

- **IDE**: PlatformIO (VSCode plugin)
- **Framework**: Arduino for ESP32
- **ESP32 Platform**:
  - **Production (Solo)**: `tasmota/platform-espressif32 @ 2023.07.00` — verified stable on ESP32-SOLO1
  - **Standard ESP32 dev board**: `espressif32 @ 6.4.0`

> ⚠️ Platform version selection is critical! Detailed version test history is documented in `platformio.ini`. Do not upgrade the platform version casually.

### 3.2 Environment Variables

| Environment | Description |
|-------------|-------------|
| `prod-solo` | **Default production build**, ESP32-SOLO1, 160MHz, 16MB Flash |
| `debug-solo` | Solo debug build, enables `-DDEBUG` macro |
| `prod` | Standard ESP32 dev board production build |
| `debug` | Standard ESP32 debug build |

### 3.3 Build Parameters

```
board_build.f_cpu = 160000000L      # CPU frequency 160MHz
board_upload.flash_size = 16MB       # Flash size
board_build.partitions = partitions.csv  # Custom partition table
monitor_speed = 115200               # Serial monitor baud rate
upload_speed = 460800                # Upload speed
```

### 3.4 Required Development Tools

- **Node.js + npm**: Web frontend build (gulp gzip packaging)
- **Python 3**: Build scripts (version management, firmware merging)
- **PlatformIO Core**: Compilation and upload

---

## 4. Project Directory Structure

```
PREGW90/
├── bin/                          # Build output (firmware binaries)
│   ├── AVATTO-GW90-Ti.bin        # OTA upgrade firmware
│   ├── AVATTO-GW90-Ti_v1.0.1.full.bin  # Full firmware (bootloader + partition table)
│   ├── bootloader_dio_40m.bin    # Bootloader
│   └── partitions.bin            # Partition table binary
│
├── lib/                          # Local libraries (not PIO-registered)
│   ├── CCTools/                  # CC2652P control library (BSL flash, reset, etc.)
│   ├── IntelHex/                 # Intel HEX format parser (firmware flashing)
│   └── WireGuard-ESP32/          # WireGuard VPN implementation
│
├── src/                          # Main source code
│   ├── main.cpp                  # Program entry (setup/loop, config loading, mode switching)
│   ├── config.h                  # Global config structs, pin definitions, constants
│   ├── version.h                 # Auto-generated version header
│   ├── web.cpp / web.h           # Web server, API, page routing
│   ├── mqtt.cpp / mqtt.h         # MQTT client, HA auto-discovery
│   ├── etc.cpp / etc.h           # Utility functions (LED, Zigbee control, config I/O)
│   ├── zb.cpp / zb.h             # Zigbee firmware management (version check, BSL flash)
│   ├── log.cpp / log.h           # Ring log buffer
│   ├── zones.h                   # Timezone data table
│   ├── webh/                     # Auto-generated — gzip-compressed C headers for web resources
│   └── websrc/                   # Web frontend source (HTML/CSS/JS/images)
│       ├── html/                 # HTML page templates
│       ├── css/                  # Stylesheets
│       ├── js/                   # JavaScript
│       ├── img/                  # Icons and images
│       └── gzipped/              # Intermediate gzip artifacts
│
├── tools/                        # Build tools and scripts
│   ├── build.py                  # Post-processing script (firmware merge, naming)
│   ├── version_increment_pre.py  # Pre-processing script (version auto-increment)
│   ├── merge_bin_esp.py          # Firmware merge tool
│   ├── version                   # Version number file (currently 1.0.1)
│   ├── esptool.py                # ESP flashing tool
│   ├── flash.sh                  # Flashing script
│   └── webfilesbuilder/          # Web frontend build tools
│       ├── build_html.py         # PIO pre-process: triggers npm/gulp build
│       ├── gulpfile.js           # Gulp task configuration
│       ├── gulp.js / gulp.meta.js
│       └── package.json          # npm dependencies
│
├── debug-ui/                     # Debug Web UI emulator
├── platformio.ini                # PlatformIO project configuration
├── partitions.csv                # Flash partition table
├── manifest.json                 # ESP Web Tools flash manifest
└── CHANGELOG.md                  # Change log
```

---

## 5. Core Source Modules

### 5.1 `main.cpp` (1506 lines)

**Program entry point**, contains:

| Function | Description |
|----------|-------------|
| `setup()` | Initialization: GPIO, Serial2(Zigbee), LittleFS, load all configs, LED setup, coordinator mode |
| `loop()` | Main loop: button handling, timer updates, web server, TCP socket passthrough, MQTT loop |
| `initLan()` | Initialize LAN8720 Ethernet (including hardware PHY reset) |
| `startServers()` | Start web server, socket server, mDNS, fetch Zigbee version |
| `setupCoordinatorMode()` | Select LAN/WiFi/USB mode based on config and start the corresponding network |
| `handleTmrNetworkOverseer()` | Periodic network check, start AP hotspot on timeout |
| `NetworkEvent()` | WiFi/ETH event callback handler |
| `connectWifi()` | WiFi STA connection logic |
| `startAP()` | Start AP hotspot (fallback or first-time configuration) |
| `toggleUsbMode()` | USB/non-USB mode toggle (save config and restart) |
| `loadConfig*()` | Configuration loading functions (WiFi/Ether/General/Security/Serial/MQTT/WG) |
| `mDNS_start()` | mDNS service registration (including ZHA zeroconf info) |

**TCP Socket Passthrough Logic** (in `loop()`):
1. Supports up to **5** simultaneous TCP client connections (`MAX_SOCKET_CLIENTS = 5`)
2. Read data from TCP client → write to Serial2 (CC2652P)
3. Read data from Serial2 → broadcast to all connected TCP clients
4. Supports IP whitelist filtering (`fwEnabled` / `fwIp`)

### 5.2 `config.h` (160 lines)

Global header file defining:

- **Hardware pin macros** (GPIO)
- **Coordinator mode enum** `COORDINATOR_MODE_t`: `LAN(0)`, `WIFI(1)`, `USB(2)`
- **Configuration structs**:
  - `ConfigSettingsStruct` — WiFi/Ethernet/Serial/Web/LED/firewall, etc.
  - `MqttSettingsStruct` — MQTT server/port/auth/topic/interval
  - `WgSettingsStruct` — WireGuard tunnel configuration
  - `zbVerStruct` — Zigbee firmware version info
- **Log buffer type**: `LogConsoleType` — `CircularBuffer<char, 8024>`
- **Debug macros**: `DEBUG_PRINT` / `DEBUG_PRINTLN`

### 5.3 `web.cpp` (1944 lines)

Web management interface, based on **ESP32 WebServer**:

**Page Routing**:
| URL | Function |
|-----|----------|
| `/` | Dashboard home |
| `/general` | General settings (hostname, coordinator mode, LED, timezone) |
| `/wifi` | WiFi configuration |
| `/ethernet` | Ethernet configuration |
| `/serial` | Serial configuration (baud rate, port number) |
| `/security` | Security settings (web auth, firewall) |
| `/mqtt` | MQTT settings |
| `/wg` | WireGuard VPN settings |
| `/about` | Device information |
| `/sys-tools` | System tools (reboot, firmware upgrade, etc.) |
| `/api` | JSON API endpoints |

**API** (`/api`): Provides device status and configuration data in JSON format, supports page asynchronous refresh.

**Web Resource Handling**: All HTML/JS/CSS/images are pre-compressed into gzip byte arrays, embedded at compile time, and served via `sendGzip()`.

**Security Features**:
- Optional HTTP Basic Auth (`checkAuth()`)
- IP whitelist firewall

### 5.4 `mqtt.cpp` (467 lines)

MQTT client based on **PubSubClient**:

| Function | Description |
|----------|-------------|
| `mqttConnectSetup()` | Initialize MQTT connection parameters |
| `mqttReconnect()` | Reconnect on disconnect (with LWT) |
| `mqttPublishState()` | Publish device state (uptime, temperature, IP, mode, Zigbee firmware version, etc.) |
| `mqttPublishDiscovery()` | Home Assistant MQTT auto-discovery (sensor/button/binary_sensor) |
| `mqttPublishIo()` | Publish I/O state (reboot/BSL/Socket) |
| `mqttCallback()` | Receive MQTT commands (`rst_esp`, `rst_zig`, `enbl_bsl`) |

**MQTT Topic Structure**:
```
{topic}/avty        — Online/offline (LWT)
{topic}/state       — Device state JSON
{topic}/io/{name}   — I/O control state
{topic}/cmd         — Command subscription
```

### 5.5 `etc.cpp` (373 lines)

Utility function module:

| Function | Description |
|----------|-------------|
| `zigbeeRouterRejoin()` | Zigbee router rejoin |
| `zigbeeEnableBSL()` | Enter CC2652P BSL flash mode |
| `zigbeeRestart()` | Restart CC2652P |
| `getCPUtemp()` | Read ESP32 CPU temperature (requires temporary WiFi activation) |
| `getDeviceID()` | Generate unique device ID |
| `writeDefaultConfig()` | Write default configuration to LittleFS |
| `resetSettings()` | Factory reset |
| `setClock()` / `setTimezone()` | NTP time sync and timezone setting |
| `ledPowerToggle()` / `ledUSBToggle()` | LED control |

**Configuration File Path Constants** (LittleFS filesystem):
```cpp
/config/system.json        — System variables (temperature offset)
/config/configWifi.json    — WiFi configuration
/config/configEther.json   — Ethernet configuration
/config/configGeneral.json — General settings
/config/configSecurity.json — Security settings
/config/configSerial.json  — Serial configuration
/config/configMqtt.json    — MQTT configuration
/config/configWg.json      — WireGuard configuration
```

### 5.6 `zb.cpp` (347 lines)

Zigbee coordinator management:

| Function | Description |
|----------|-------------|
| `getZbVer()` | Get CC2652P firmware version via SYS_VERSION command |
| `zbCheck()` | Check if Zigbee module is operational (LED blink test) |
| `zbInit()` | Initialize Zigbee communication |
| `checkFwHex()` | Validate uploaded HEX firmware file |
| `runFlash()` / `programFlashFromFile()` | Flash CC2652P firmware via BSL |
| `zbLedToggle()` | Toggle Zigbee module LED |

**Zigbee Communication Protocol**: Uses TI Z-Stack ZNP (Zigbee Network Processor) protocol, communicates with CC2652P via Serial2 (UART).

### 5.7 `log.cpp` / `log.h`

Ring log buffer (8024 bytes), used for real-time log display on the web console.

---

## 6. Configuration System

All configuration is stored in **LittleFS** filesystem under `/config/`, in JSON format.

### 6.1 Configuration File Field Descriptions

**`/config/configGeneral.json`**
```json
{
    "hostname": "AVATTO-GW90-TI",
    "disableLeds": false,
    "refreshLogs": 1000,
    "disableLedPwr": false,
    "disableLedUSB": false,
    "coordMode": 0,        // 0=LAN, 1=WiFi, 2=USB
    "prevCoordMode": 0,
    "keepWeb": false,       // Keep web accessible in USB mode
    "timeZoneName": ""
}
```

**`/config/configWifi.json`**
```json
{
    "enableWiFi": 0,
    "ssid": "",
    "pass": "",
    "dhcpWiFi": 1,
    "ip": "", "mask": "", "gw": ""
}
```

**`/config/configEther.json`**
```json
{
    "dhcp": 1,
    "ip": "", "mask": "", "gw": ""
}
```

**`/config/configSerial.json`**
```json
{
    "baud": 115200,
    "port": 6638
}
```

**`/config/configSecurity.json`**
```json
{
    "disableWeb": 0,
    "webAuth": 0,
    "webUser": "admin",
    "webPass": "",
    "fwEnabled": 0,
    "fwIp": ""
}
```

**`/config/configMqtt.json`**
```json
{
    "enable": 0,
    "server": "",
    "port": 1883,
    "user": "mqttuser",
    "pass": "",
    "topic": "{deviceId}",
    "interval": 60,
    "discovery": 0
}
```

**`/config/configWg.json`**
```json
{
    "enable": 0,
    "localAddr": "",
    "localIP": "",
    "endAddr": "",
    "endPubKey": "",
    "endPort": ""
}
```

### 6.2 Configuration Loading Flow (setup)

```
loadSystemVar()        → System variables (temperature offset calibration)
loadConfigSerial()     → Serial parameters
loadConfigWifi()       → WiFi
loadConfigEther()      → Ethernet
loadConfigGeneral()    → General settings
loadConfigSecurity()   → Security
loadConfigMqtt()       → MQTT
loadConfigWg()         → WireGuard
```

If any configuration fails to load, it tries to create a default config; repeated failures trigger `ESP.restart()`.

---

## 7. Flash Partition Table

**File**: `partitions.csv`

| Partition | Type | Subtype | Offset | Size | Description |
|-----------|------|---------|--------|------|-------------|
| nvs | data | nvs | 0x9000 | 20KB | Non-Volatile Storage |
| otadata | data | ota | 0xE000 | 8KB | OTA data |
| app0 | app | ota_0 | 0x10000 | **6MB** | Main application partition |
| app1 | app | ota_1 | 0x610000 | **6MB** | OTA backup partition |
| spiffs | data | spiffs | 0xC10000 | ~4MB | LittleFS filesystem |

> Total ~16MB Flash used. Supports OTA dual-partition switching for upgrades.

---

## 8. Third-Party Library Dependencies

### 8.1 PlatformIO lib_deps

| Library | Version | Purpose |
|---------|---------|---------|
| `bblanchon/ArduinoJson` | 6.21.3 | JSON serialization/deserialization |
| `rlogiacco/CircularBuffer` | ≥1.3.3 | Ring log buffer |
| `plerup/EspSoftwareSerial` | 8.1.0 | Software serial (backup) |
| `marian-craciunescu/ESP32Ping` | ≥1.7 | Ping network detection |
| `sstaub/Ticker` | ≥4.4.0 | Non-blocking timer |
| `knolleary/PubSubClient` | ^2.8 | MQTT client |
| `mathieucarbou/AsyncTCP` | 3.2.5 | Async TCP (dependency) |
| `Martin-Laclaustra/CronAlarms` | latest | Cron scheduled tasks |

### 8.2 Local Libraries (lib/)

| Library | Purpose |
|---------|---------|
| **CCTools** | CC2652P chip control (BSL entry, reset, router operations) |
| **IntelHex** | Intel HEX firmware file parsing (Zigbee firmware flashing) |
| **WireGuard-ESP32** | WireGuard VPN implementation for ESP32 platform |

---

## 9. Build Process & Automation Scripts

### 9.1 Build Commands

```bash
# Default production build (ESP32-SOLO1)
pio run -e prod-solo

# Debug build
pio run -e debug-solo

# Upload firmware
pio run -e prod-solo -t upload

# Serial monitor
pio device monitor
```

### 9.2 Automated Build Pipeline

PlatformIO executes the following pipeline via `extra_scripts` configuration:

#### Pre-build

1. **`tools/version_increment_pre.py`**
   - Reads version number from `tools/version`
   - Auto-increments patch version (unless `.version_no_increment` marker file exists)
   - Generates `src/version.h` (containing `VERSION` and `BUILD_TIMESTAMP` macros)

2. **`tools/webfilesbuilder/build_html.py`**
   - Checks `.no_web_update` marker file
   - Runs `npm install` + `npx gulp` to build web frontend
   - Compresses HTML/JS/CSS/images to gzip and converts to C header files in `src/webh/`

#### Post-build

3. **`tools/build.py`**
   - Copies `firmware.bin` to `bin/`
   - Copies `partitions.bin` and `bootloader_dio_40m.bin`
   - Calls `merge_bin_esp.py` to merge full firmware:
     - `bootloader @ 0x1000` + `partitions @ 0x8000` + `firmware @ 0x10000`
   - Generates named files:
     - `AVATTO-GW90-Ti_v{version}.full.bin` — full flash image
     - `AVATTO-GW90-Ti.bin` — OTA upgrade

### 9.3 Version Management

- Version number stored in `tools/version` text file
- Format: `major.minor.patch` (e.g. `1.0.1`)
- Auto-increments by +1 (patch) on each build, unless:
  - `tools/.version_no_increment` file exists (skip entirely)
  - `tools/.version_no_increment_update_date` file exists (don't increment but update timestamp)

### 9.4 Skipping Web Build

If only modifying C++ code, create an empty `.no_web_update` file under `tools/webfilesbuilder/` to skip the web build and speed up compilation.

---

## 10. Web Frontend Build

### 10.1 Tech Stack

- **HTML templates**: Located in `src/websrc/html/`
- **JavaScript**: jQuery + Bootstrap + custom `functions.js`
- **Internationalization**: i18next
- **Build tool**: Gulp (via `tools/webfilesbuilder/gulpfile.js`)

### 10.2 Build Pipeline

```
src/websrc/ (source)
    ↓ gulp (minify + gzip)
src/websrc/gzipped/ (intermediate)
    ↓ convert to C byte array
src/webh/*.gz.h (embedded at compile time)
```

### 10.3 Manual Web Resource Build

```bash
cd tools/webfilesbuilder
npm install
npx gulp
```

---

## 11. Firmware Features

### 11.1 Three Operating Modes

| Mode | `coordMode` | Description |
|------|-------------|-------------|
| **LAN** | 0 | Zigbee data passthrough via Ethernet TCP socket, default port 6638 |
| **WiFi** | 1 | Zigbee data passthrough via WiFi TCP socket |
| **USB** | 2 | GPIO33 pulled high, CC2652P communicates directly with host via USB |

### 11.2 Network Services

| Service | Port | Description |
|---------|------|-------------|
| Web management | TCP 80 | HTTP |
| TCP socket passthrough | TCP 6638 (default, configurable) | Zigbee data bridge |
| mDNS | UDP 5353 | Service discovery (`_avatto-gw90-ti._tcp`) |
| DNS (AP mode) | UDP 53 | Captive portal |

### 11.3 AP Hotspot Mode

- **SSID**: Device ID (e.g. `AVATTO-GW90-TI`)
- **IP**: 192.168.1.1
- **Trigger conditions**:
  - First boot with no configuration
  - Network connection timeout (5s × 4 retries = 20s)
  - No WiFi credentials

---

## 12. MQTT Integration

### 12.1 Features

- Periodic device state publishing (configurable interval, default 60s)
- Home Assistant MQTT auto-discovery (sensor/button/binary_sensor)
- Remote command control
- LWT (Last Will and Testament) messages (online/offline detection)

### 12.2 Remote Commands

Send JSON via `{topic}/cmd` topic:
```json
{"cmd": "rst_esp"}     // Restart ESP32
{"cmd": "rst_zig"}     // Restart Zigbee
{"cmd": "enbl_bsl"}    // Enter BSL mode
```

### 12.3 State Data

JSON published to `{topic}/state`:
```json
{
    "uptime": "0 d 01:23:45",
    "temperature": "42.5",
    "connections": 1,
    "ip": "192.168.1.100",
    "mode": "Zigbee-to-Ethernet",
    "zbfw": "20221226",
    "hostname": "AVATTO-GW90-TI"
}
```

---

## 13. WireGuard VPN Support

- Based on local library `WireGuard-ESP32`
- Automatically initializes WireGuard tunnel after network connection is established
- Configuration: local address/private key, endpoint address/public key/port

---

## 14. Zigbee Coordinator Management

### 14.1 CC2652P Firmware Version Detection

Uses the ZNP protocol's `SYS_VERSION` command to retrieve version info, including:
- `majorrel` / `minorrel` / `maintrel` / `zbRev` / `product` / `transportrev`

### 14.2 BSL Firmware Flashing

1. Upload `.hex` firmware file via web interface
2. File saved to LittleFS `/config/fw.hex`
3. Use CCTools library to put CC2652P into BSL mode
4. Execute flashing via serial port

### 14.3 LED Control

Control the LED on the CC2652P module via ZNP command:
- Command frame format: `FE 02 27 0A 01 {00|01} {checksum}`

---

## 15. OTA & Firmware Updates

### 15.1 Web OTA

Upload `.bin` file via web interface at `/sys-tools` for OTA upgrade.

### 15.2 Remote OTA

Firmware downloaded from GitHub Release:
```
https://github.com/AVATTO-smart/avatto-gw90-firmware/releases/latest/download/AVATTO-GW90-Ti.bin
```

### 15.3 ESP Web Tools Flashing

`manifest.json` supports flashing full firmware (bootloader + partition table + application) directly via browser Web Serial.

---

## 16. Button & LED Interaction

### 16.1 Physical Button (GPIO 35)

| Action | Effect |
|--------|--------|
| **Short press** | Toggle USB/non-USB mode (save and restart) |
| **Long press 2s** | Toggle LED global on/off |
| **Long press 4s** | Enter Zigbee BSL flash mode |
| **Hold > 2s at boot** | Factory reset |

### 16.2 LED Indicators

| LED | Color | GPIO | Indication |
|-----|-------|------|------------|
| LED_PWR | Blue | 14 | Power/operation indicator (can be disabled) |
| LED_USB | Red | 12 | USB mode indicator (only on in USB mode) |

---

## 17. Network Connection Management

### 17.1 Network Overseer Timer

- **Check interval**: 5 seconds
- **Max retries**: 4 times (i.e. 20s timeout)
- **Behavior**:
  - LAN mode: check Ethernet connection, start AP on timeout
  - WiFi mode: check WiFi connection, reconnect on timeout and start AP
  - USB mode: try WiFi for 10s, if failed try LAN, if both fail start AP

### 17.2 Ethernet Initialization

LAN8720 PHY initialization sequence:
1. GPIO5 LOW for 50ms → HIGH for 300ms (hardware reset PHY)
2. `ETH.begin()` initializes RMII interface
3. Configure static IP or use DHCP

### 17.3 mDNS Service

Registers `_avatto-gw90-ti._tcp` service with ZHA-compatible zeroconf metadata:
- `radio_type`: znp
- `baud_rate`: configured baud rate
- `data_flow_control`: software

---

## 18. Debugging Methods

### 18.1 Build Debug Version

```bash
pio run -e debug-solo
```
When the `-DDEBUG` macro is enabled, `DEBUG_PRINT` / `DEBUG_PRINTLN` macros output to Serial (115200 baud).

### 18.2 Serial Monitor

```bash
pio device monitor
```
Built-in `esp32_exception_decoder` filter can automatically decode exception stack traces.

### 18.3 Web Log

The web interface provides real-time log viewing (ring buffer 8024 bytes), including Zigbee communication data in HEX format.

### 18.4 Debug UI

The `debug-ui/` directory contains a standalone debug web interface, launchable via `start-debug-server.bat`, simulating device behavior with mock data.

### 18.5 Key Debug Log Prefixes

| Prefix | Module |
|--------|--------|
| `[LAN]` | Ethernet initialization |
| `[ETH_EVENT]` | Ethernet events (connected/disconnected/IP acquired) |
| `[NET_EVENT]` | Network event dispatch |
| `[OVERSEER]` | Network overseer timer |
| `[SRV]` | Service startup |
| `[MODE]` | Coordinator mode switch |
| `[LED]` | LED control state |
| `[ZBVER]` | Zigbee version info |
| `[ZB_FW_TASK]` | Zigbee firmware flashing task |
| `[SOCK IP WHITELIST]` | Socket connection IP filtering |

---

## 19. Known Issues & Precautions

### 19.1 Platform Version Compatibility

**Very important**: ESP32-SOLO1 platform version selection is critical. Tested results:
- ✅ `tasmota/platform-espressif32 @ 2023.07.00` — works reliably
- ❌ `2023.08.00` and later — HTTPS functionality broken
- ❌ `espressif32 @ 6.4.0` — does not support SOLO1 board

### 19.2 LAN8720 PHY Initialization

GPIO5 must be hardware-reset before `ETH.begin()` (LOW 50ms → HIGH 300ms), otherwise the PHY may not initialize correctly after `ESP.restart()`.

### 19.3 CPU Temperature Reading

The ESP32 temperature sensor requires the WiFi module to be active. `getCPUtemp()` temporarily enables then disables WiFi during the reading.

### 19.4 MQTT Buffer

PubSubClient's default buffer is small; `mqttPublishMsg()` uses `beginPublish/print/endPublish` pattern to support large messages.

### 19.5 Config File Corruption Handling

All `loadConfig*()` functions will **delete the corrupted file** and restart the device upon JSON deserialization failure. Default configuration is automatically created on next boot.

### 19.6 Version Number Storage

The `tools/version` file only stores the version string and is not auto-synced with Git commits. Manually edit this file to change the version number.

---

## Appendix A: Quick Start Guide

### First Compile

```bash
# 1. Install PlatformIO CLI (or VSCode plugin)
pip install platformio

# 2. Install Node.js (for web build)
# Download: https://nodejs.org/

# 3. Clone the project
git clone <repo_url>
cd PREGW90

# 4. Build
pio run -e prod-solo

# 5. Output files
# bin/AVATTO-GW90-Ti.bin          — OTA upgrade firmware
# bin/AVATTO-GW90-Ti_v*.full.bin  — Full flash firmware
```

### First Flash

```bash
# Via serial
pio run -e prod-solo -t upload

# Or manually with esptool
python tools/esptool.py --chip esp32 --baud 460800 write_flash 0x0 bin/AVATTO-GW90-Ti_v1.0.1.full.bin
```

### First-Time Device Configuration

1. Device creates AP hotspot on first boot (SSID: `AVATTO-GW90-TI`)
2. Connect to hotspot and browse to `192.168.1.1`
3. Configure network parameters (WiFi or Ethernet)
4. After reboot, access web management interface via the configured network

---

## Appendix B: File Line Counts

| File | Lines | Description |
|------|-------|-------------|
| `main.cpp` | 1506 | Program entry and core logic |
| `web.cpp` | 1944 | Web server (largest file) |
| `mqtt.cpp` | 467 | MQTT integration |
| `etc.cpp` | 373 | Utility functions |
| `zb.cpp` | 347 | Zigbee management |
| `config.h` | 160 | Configuration definitions |
| `zones.h` | 479 | Timezone data |

---

## Appendix C: Contacts & Resources

- **Original repository**: https://github.com/mercenaruss/uzg-firmware
- **OTA firmware download**: https://github.com/AVATTO-smart/avatto-gw90-firmware/releases
- **PlatformIO documentation**: https://docs.platformio.org/
- **CC2652P BSL documentation**: TI CC26xx Technical Reference Manual
- **Z-Stack ZNP interface**: TI Z-Stack ZNP Interface Specification

---
