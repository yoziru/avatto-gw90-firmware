# Security Policy

This is community firmware that users flash onto their own AVATTO GW90-TI gateway. It sits on your LAN next to the devices it controls and is not hardened like a vendor product. This document describes what it does and does not protect.

## Supported versions

Only the latest release is supported. There are no long-term-support or backport branches; fixes land in the next `vX.Y.Z` release cut from `main`.

Updates are delivered in three ways, none of them signed:

- **OTA from GitHub** — the About page downloads the latest `AVATTO-GW90-Ti.bin` from this repository's GitHub releases.
- **Manual web upload** — the `sys-tools` page or `POST /update` with a local `.bin`.
- **Serial flash** — the full image (`bin/AVATTO-GW90-Ti_vX.Y.Z.full.bin`) via esptool or the ESP Web Tools flow in `manifest.json`.

Images are not signature-checked before boot; see the threat model below.

## Reporting a vulnerability

Please report vulnerabilities privately through GitHub Security Advisories: open the repository's **Security** tab → **Report a vulnerability**
(<https://github.com/yoziru/avatto-gw90-firmware/security/advisories/new>). Do not open a public issue for a security problem. Include the firmware version, the affected page or endpoint, and a minimal reproduction if you have one. This is a hobby project maintained on a best-effort basis.

## Threat model

**Web UI — plain HTTP, optional digest auth.** The UI is served over plain HTTP on port 80 (`WebServer serverWeb(80)`). Authentication is HTTP Digest (`checkAuth()` in `src/web/common.cpp`), but it is **off by default** (`webAuth: 0` in `/config/configSecurity.json`) and the transport is unencrypted. Anyone on the same network can read the traffic, and an active attacker can tamper with it. Digest avoids sending the password in cleartext, but on plain HTTP it is not protection against an on-path attacker. The API, status, event-stream, Zigbee-command and firmware endpoints call `checkAuth()`; the settings-form handler (`/saveParams`) currently does not, so enabling auth alone is not a complete access control.

**Firmware downloads — HTTPS without certificate validation.** Remote OTA fetches release binaries over HTTPS, but the TLS client is configured with `setInsecure()` (`src/web/ota.cpp`, `src/web/api.cpp`), so the server certificate is not verified. This protects against passive eavesdropping only: an attacker who can redirect the traffic could serve a malicious image, and flashed images are not signature-checked afterwards.

**MQTT — optional, plaintext, no TLS.** MQTT is off by default and connects over plain TCP (default port 1883) using `WiFiClient` and PubSubClient. Credentials and payloads are sent in the clear; MQTT TLS is not currently supported.

**Credential storage — unencrypted.** Wi-Fi password, web username/password, MQTT credentials and the WireGuard private key are stored in plain JSON files in LittleFS (`/config/configWifi.json`, `configSecurity.json`, `configMqtt.json`, `configWg.json`). There is no flash encryption, so anyone who can read the flash gets all of them. Do not reuse passwords.

**Physical access means full compromise.** There is no secure boot, no flash encryption, no per-device keys and no fusing performed here. Anyone with physical access and a serial adapter can read the flash, replace the firmware, or enter the CC2652P bootloader. The firmware cannot defend against that.

**Zigbee bridge — unauthenticated by design.** The coordinator is exposed as a raw TCP socket on port 6638 (configurable, `TCP_LISTEN_PORT`) with no authentication: any client that can reach the port can talk to the Zigbee network. This matches the trust model ZHA and Zigbee2MQTT assume on a LAN. An optional single-IP allowlist can restrict who may connect, but it is not an authentication mechanism.

**WireGuard — for remote access.** The built-in WireGuard client (off by default) lets you reach the device from outside without exposing it: only your WireGuard peer can connect, and tunnel traffic is authenticated and encrypted.

## Recommendations

- Never expose the device to the internet (no port forwarding, no DMZ, no cloud proxy).
- For remote access, use the built-in WireGuard client or a VPN/reverse proxy with TLS.
- If you enable web auth, use a strong, unique password, and remember that everyone on the LAN still sees the traffic.
- Run MQTT only on a trusted or segmented network, or through a tunnel; do not send credentials you care about over plaintext.
- Treat the LAN as trusted: segment IoT devices and block unsolicited inbound connections to the device.
- Keep the firmware current; only the latest release is supported.

## Out of scope / accepted risks

- No secure boot, flash encryption, measured boot or firmware signing — this is a community, user-flashed build with no per-device provisioning or key material.
- No protection against an attacker with physical or network-level access (see above).
- The Zigbee TCP port and MQTT are intentionally unauthenticated/plaintext by default, matching the ecosystem's LAN trust model.
- Only the stock build is in scope; modified builds, the factory-test environment and third-party forks are not.
