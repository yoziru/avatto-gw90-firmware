# Security Policy

This is community firmware that users flash onto their own AVATTO GW90-TI gateway. It sits on your LAN next to the devices it controls and is not hardened like a vendor product. This document describes what it does and does not protect.

## Supported versions

Only the latest release is supported. There are no long-term-support or backport branches; fixes land in the next `vX.Y.Z` release cut from `main`.

Updates are delivered in three ways:

- **OTA from GitHub** — the About page downloads `AVATTO-GW90-Ti.bin` from this repository's releases. Official ESP32 releases ship a detached ECDSA P-256/SHA-256 signature (`AVATTO-GW90-Ti.bin.sig`) that remote OTA verifies before the boot partition is switched. Zigbee (CC2652P) firmware is an upstream binary and is not signed.
- **Manual web upload** — `POST /update` with a local `.bin`. A browser upload cannot be verified, so it requires explicit confirmation (`force=1`, prompted in the web UI).
- **Serial flash** — the full image (`bin/AVATTO-GW90-Ti_vX.Y.Z.full.bin`) via esptool or the ESP Web Tools flow in `manifest.json`. Flashed directly; no signature check.

## Reporting a vulnerability

Please report vulnerabilities privately through GitHub Security Advisories: open the repository's **Security** tab → **Report a vulnerability**
(<https://github.com/yoziru/avatto-gw90-firmware/security/advisories/new>). Do not open a public issue for a security problem. Include the firmware version, the affected page or endpoint, and a minimal reproduction if you have one. This is a hobby project maintained on a best-effort basis.

## Threat model

**Web UI — plain HTTP, optional digest auth.** The UI is served over plain HTTP on port 80 (`WebServer serverWeb(80)`). Authentication is HTTP Digest (`checkAuth()` in `src/web/common.cpp`), but it is **off by default** (`webAuth: 0` in `/config/configSecurity.json`) and the transport is unencrypted: anyone on the same network can read the traffic, and an active attacker can tamper with it. All state-changing routes go through `checkAuth()`, including `/saveParams`, and cross-site browser requests are rejected when `Origin`/`Referer` does not match the request `Host` (`X-Forwarded-Host` is honored for reverse proxies). Digest on plain HTTP is still not protection against an on-path attacker.

**Firmware downloads — validated TLS, signed ESP32 releases.** Remote OTA fetches release binaries over HTTPS with the server certificate validated against the ESP32's built-in CA bundle (`setCACertBundle` in `src/web/ota.cpp` and `src/web/api.cpp`) — no `setInsecure()`. Unless `force=1`, ESP32 remote OTA also downloads `<url>.sig` and verifies the streamed image's ECDSA P-256/SHA-256 signature against the embedded public key before switching the boot partition; a missing or invalid signature aborts with an error suggesting `force=1`, leaving the running firmware in place. Zigbee (CC2652P) firmware is fetched over the same validated TLS but is not signed. Manual `POST /update` uploads cannot be verified, so they require `force=1`; that is consent, not a security boundary — web auth is the real access control.

**MQTT — optional, off by default, optional TLS.** MQTT is off by default and connects over plain TCP (default port 1883) unless the MQTT page's TLS switch is on, in which case the broker's certificate chain is validated against the CA bundle. The broker field takes an IP address only, so the hostname is not verified, and self-signed or private-CA certificates are not supported. The port is not changed automatically — set the broker's TLS port (usually 8883) yourself.

**Credential storage — unencrypted.** Wi-Fi password, web username/password, MQTT credentials and the WireGuard private key are stored in plain JSON files in LittleFS (`/config/configWifi.json`, `configSecurity.json`, `configMqtt.json`, `configWg.json`). There is no flash encryption, so anyone who can read the flash gets all of them. Do not reuse passwords.

**Physical access means full compromise.** There is no secure boot, no flash encryption, no per-device keys and no fusing performed here. The release signature protects OTA downloads, not the boot process. Anyone with physical access and a serial adapter can read the flash, replace the firmware, or enter the CC2652P bootloader; the firmware cannot defend against that.

**Zigbee bridge — unauthenticated by design.** The coordinator is exposed as a raw TCP socket on port 6638 (configurable, `TCP_LISTEN_PORT`) with no authentication: any client that can reach the port can talk to the Zigbee network. This matches the trust model ZHA and Zigbee2MQTT assume on a LAN. An optional single-IP allowlist can restrict who may connect, but it is not an authentication mechanism.

**WireGuard — for remote access.** The built-in WireGuard client (off by default) lets you reach the device from outside without exposing it: only your WireGuard peer can connect, and tunnel traffic is authenticated and encrypted.

## Recommendations

- Never expose the device to the internet (no port forwarding, no DMZ, no cloud proxy).
- For remote access, use the built-in WireGuard client or a VPN/reverse proxy with TLS.
- If you enable web auth, use a strong, unique password, and remember that everyone on the LAN still sees the traffic.
- Enable MQTT TLS where the broker presents a publicly trusted certificate; otherwise keep MQTT on a trusted or segmented network or behind a tunnel.
- Treat the LAN as trusted: segment IoT devices and block unsolicited inbound connections to the device.
- Keep the firmware current; only the latest release is supported.

## Out of scope / accepted risks

- No secure boot, flash encryption, measured boot or per-device key material. ESP32 OTA downloads are signed, but serial flashing bypasses that check.
- No protection against an attacker with physical or network-level access (see above).
- The Zigbee TCP port is intentionally unauthenticated, and MQTT is plaintext unless TLS is enabled, matching the ecosystem's LAN trust model.
- Only the stock build is in scope; modified builds, the factory-test environment and third-party forks are not.
