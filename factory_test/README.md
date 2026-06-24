# AVATTO GW90 Factory Test Program Solution

## Overview

This factory test program is used for full functional testing of the AVATTO GW90-TI Zigbee gateway on the production line.  
The tests cover all hardware including ESP32-SOLO1 main controller, CC2652P Zigbee coordinator, LAN8720 Ethernet PHY, WiFi, LEDs, buttons, etc.

## Test Architecture

```
┌────────────────────┐      UART(115200)      ┌─────────────────┐
│   Test Host (PC)   │◄═══════════════════════►│  AVATTO GW90    │
│   (Python Script)  │      USB-TTL           │  (Factory FW)   │
└────────────────────┘                         └─────────────────┘
        │                                             │
        │  TCP (Optional)                              │  RJ45
        └──────────────────────────────────────────────┘
```

## Test Item List

| # | Test Item | Method | Pass Criteria | Duration |
|---|-----------|--------|---------------|----------|
| 1 | ESP32 Basic Boot | Serial output check | Boot info printed successfully | 1s |
| 2 | Flash R/W (16MB) | LittleFS write/read verify | Read matches write | 2s |
| 3 | ESP32 MAC Address | Read eFuse MAC | MAC not all-0/all-F | <1s |
| 4 | CPU Temperature Sensor | Read internal temperature | In 0°C ~ 80°C range | 1s |
| 5 | WiFi Scan | WiFi.scanNetworks() | At least 1 AP found | 5s |
| 6 | Ethernet PHY (LAN8720) | ETH.begin() + linkUp | Link up or DHCP IP obtained | 8s |
| 7 | CC2652P Communication | BSL handshake / SYS_VERSION | Version info returned | 3s |
| 8 | CC2652P Chip ID | CCTools.detectChipInfo() | Identified as CC2652P/P7 | 2s |
| 9 | Blue LED (PWR) | GPIO14 output + visual/photo sensor | LED on/off controllable | 2s |
| 10 | Red LED (USB) | GPIO12 output + visual/photo sensor | LED on/off controllable | 2s |
| 11 | Button (BTN) | GPIO35 input detect | Pressed=LOW, Released=HIGH | 3s |
| 12 | MODE_SWITCH | GPIO33 output + readback (if possible) | High/low toggleable | 1s |
| 13 | Zigbee LED Control | ZNP command to control CC2652P LED | Correct response | 2s |
| 14 | Serial Passthrough | Serial2 TX/RX loopback | Data matches | 1s |
| 15 | Factory Mark Write | NVS/LittleFS write mark | Mark written successfully | <1s |

**Estimated total test time per board**: ~30-35 seconds

## Build Method

```bash
# Build factory test firmware in PlatformIO
pio run -e factory-test-solo
```

## Host PC Usage

```bash
cd factory_test
pip install pyserial
python host_test.py --port COM3
```
