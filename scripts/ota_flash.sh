#!/bin/sh
#
# Upload bin/AVATTO-GW90-Ti.bin to a gateway over the network (OTA) and wait
# for it to come back. Used by both `mise run flash:ota` (prod) and
# `mise run flash:ota:debug` (debug) - the build that produced the bin is
# chosen by the caller's mise task dependency.
#
# Env:
#   OTA_DEVICE_IP  (required) device IP, e.g. 192.168.1.18
#   OTA_USER       (optional) web-auth username; OTA_PASS for the password
#
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$ROOT_DIR"

BIN="bin/AVATTO-GW90-Ti.bin"
if [ ! -f "$BIN" ]; then
    echo "Firmware not found: $BIN (build it first)" >&2
    exit 1
fi

DEVICE_IP="${OTA_DEVICE_IP:?Set OTA_DEVICE_IP env var}"

echo "Uploading firmware to http://$DEVICE_IP/update ..."
if [ -n "${OTA_USER:-}" ]; then
    curl -sS -u "${OTA_USER}:${OTA_PASS:-}" \
        -F "firmware=@$BIN;filename=$(basename "$BIN")" \
        "http://$DEVICE_IP/update"
else
    curl -sS \
        -F "firmware=@$BIN;filename=$(basename "$BIN")" \
        "http://$DEVICE_IP/update"
fi
echo ""
echo "OTA triggered. Waiting for device to come back..."

for i in $(seq 1 60); do
    sleep 2
    STATUS=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 2 "http://$DEVICE_IP/" 2>/dev/null || true)
    if [ "$STATUS" = "200" ] || [ "$STATUS" = "302" ] || [ "$STATUS" = "401" ]; then
        echo "Device is back online! ($((i * 2))s)"
        break
    fi
    echo "  waiting... ($((i * 2))s)"
done

echo "Done."
