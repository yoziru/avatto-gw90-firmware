#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <LittleFS.h>
#include <Update.h>
#include <WiFi.h>

#include <esp_task_wdt.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

#include "boot_guard.h"
#include "config.h"
#include "web.h"
#include "zb.h"
#include "fw_pubkey.h"
#include "fw_verify.h"
#include "tls_client.h"
#include "bridge.h"

namespace
{
constexpr size_t FW_SHA256_LEN = 32;
constexpr size_t FW_SIG_URL_MAX = 256;
const char *const fwUnsignedError = "unsigned firmware; add force=1 to install anyway";
const char *const fwBadSignatureError = "invalid firmware signature; add force=1 to install anyway";
const char *const fwIncompleteError = "firmware download incomplete";

mbedtls_sha256_context fwSha256;
bool fwHashActive = false;
const char *fwRejectError = nullptr;
char fwSigUrlBuf[FW_SIG_URL_MAX];

void fwHashAbort()
{
    if (fwHashActive)
    {
        mbedtls_sha256_free(&fwSha256);
        fwHashActive = false;
    }
}

bool verifyFirmwareSignature()
{
    uint8_t digest[FW_SHA256_LEN] = {0};
    mbedtls_sha256_finish(&fwSha256, digest);
    fwHashAbort();

    HTTPClient clientSig;
    NetworkClientSecure client;
    NetworkClient plainClient;
    trustEspCertificateBundle(client);
    client.setTimeout(10000);
    clientSig.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    NetworkClient &transport = String(fwSigUrlBuf).startsWith("https://") ? static_cast<NetworkClient &>(client) : plainClient;
    if (!clientSig.begin(transport, fwSigUrlBuf)) {
        fwRejectError = fwUnsignedError;
        return false;
    }
    clientSig.setTimeout(10000);
    if (clientSig.GET() != HTTP_CODE_OK)
    {
        DEBUG_PRINTLN("Firmware signature download failed");
        clientSig.end();
        fwRejectError = fwUnsignedError;
        return false;
    }

    const String signature = clientSig.getString();
    clientSig.end();
    if (signature.isEmpty())
    {
        DEBUG_PRINTLN("Firmware signature is empty");
        fwRejectError = fwUnsignedError;
        return false;
    }

    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    int ret = mbedtls_pk_parse_public_key(&pk, reinterpret_cast<const unsigned char *>(FW_SIGNING_PUBKEY_PEM), sizeof(FW_SIGNING_PUBKEY_PEM));
    if (ret == 0)
    {
        ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, digest, sizeof(digest), reinterpret_cast<const unsigned char *>(signature.c_str()), signature.length());
    }
    mbedtls_pk_free(&pk);
    if (ret != 0)
    {
        DEBUG_PRINTLN("Firmware signature verification failed: " + String(ret));
        fwRejectError = fwBadSignatureError;
        return false;
    }
    printLogMsg("ESP OTA signature verified");
    return true;
}
} // namespace

bool startZbFlashTask()
{
    if (xTaskCreatePinnedToCore(checkFwHexTask, "CheckFWHex", 8192, const_cast<char *>(tempFile), 3, NULL, 0) == pdPASS)
        return true;
    ConfigSettings.zbFlashing = false;
    sendEvent("ZB_FW_err", sizeof("ZB_FW_err"), "Could not start firmware update task");
    return false;
}

void checkFwHexTask(void *param)
{
    const char *hexFilePath = static_cast<const char *>(param);
    Serial.println("\n[ZB_FW_TASK] Starting firmware validation task...");

    sendEvent("ZB_FW_info", sizeof("ZB_FW_info"), "Validating firmware...");

    esp_task_wdt_reset();

    if (0 == checkFwHex(hexFilePath)) {
        // The installed revision/role may have changed: ask loop() to re-detect
        // once the coordinator has rebooted. Serial2 is free again here - the
        // re-detection itself must not run from this task.
        zbRequestInfoRefresh();
        sendEvent("ZB_FW_info", sizeof("ZB_FW_info"), "Update done!");
    } else {
        sendEvent("ZB_FW_err", sizeof("ZB_FW_err"), "Update failed!");
    }

    Serial.println("[ZB_FW_TASK] Firmware update task completed");

    ConfigSettings.zbFlashing = false;
    vTaskDelete(NULL);
}

void progressFunc(unsigned int progress, unsigned int total)
{
    const char *tagESP_FW_progress = "ESP_FW_prgs";
    const uint8_t eventLen = 11;

    float percent = ((float)progress / total) * 100.0;

    sendEvent(tagESP_FW_progress, eventLen, String(percent));

#ifdef DEBUG
    if (int(percent) % 5 == 0)
    {
        DEBUG_PRINTLN("Update ESP32 progress: " + String(progress) + " of " + String(total) + " | " + String(percent) + "%");
    }
#endif
};

static bool runEspUpdateFirmware(uint8_t *data, size_t len)
{
    if (Update.write(data, len) != len) return false;
    if (fwHashActive)
    {
        if (mbedtls_sha256_update(&fwSha256, data, len) != 0) return false;
    }
    currentLength += len;
    return true;
}

String getEspUpdate(String esp_fw_url, bool force)
{
    if (!esp_fw_url.startsWith("http://") && !esp_fw_url.startsWith("https://"))
        return "firmware URL must use HTTP or HTTPS";
    DEBUG_PRINTLN("getEspUpdate: " + esp_fw_url);
    printLogMsg("ESP OTA download start");

    fwRejectError = nullptr;
    fwHashActive = false;
    if (!force)
    {
        if (!fwSigUrl(esp_fw_url.c_str(), fwSigUrlBuf, sizeof(fwSigUrlBuf)))
        {
            printLogMsg("ESP OTA rejected: firmware URL too long");
            return "firmware URL too long";
        }
        mbedtls_sha256_init(&fwSha256);
        mbedtls_sha256_starts(&fwSha256, 0);
        fwHashActive = true;
    }

    HTTPClient clientWeb;
    NetworkClientSecure client;
    NetworkClient plainClient;
    trustEspCertificateBundle(client);
    clientWeb.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    NetworkClient &transport = esp_fw_url.startsWith("https://") ? static_cast<NetworkClient &>(client) : plainClient;
    if (!clientWeb.begin(transport, esp_fw_url)) {
        fwHashAbort();
        return "invalid firmware URL";
    }
    clientWeb.setTimeout(10000);
    // ponytail: request close-delimited HTTP/1.0 bodies; reject chunked replies
    // rather than feed HTTP framing to Update. Add chunk decoding if needed.
    clientWeb.useHTTP10(true);
    const char *responseHeaders[] = {"Transfer-Encoding"};
    clientWeb.collectHeaders(responseHeaders, 1);
    clientWeb.addHeader("Content-Type", "application/octet-stream");

    int resp = clientWeb.GET();
    DEBUG_PRINTLN("Response: " + String(resp));
    if (resp != HTTP_CODE_OK)
    {
        DEBUG_PRINTLN("Cannot download firmware file.");
        printLogMsg("ESP OTA download failed");
        clientWeb.end();
        fwHashAbort();
        return "download failed";
    }

    totalLength = clientWeb.getSize();
    currentLength = 0;
    if (clientWeb.header("Transfer-Encoding").length() || totalLength == 0 ||
        !Update.begin(totalLength > 0 ? size_t(totalLength) : UPDATE_SIZE_UNKNOWN)) {
        clientWeb.end();
        fwHashAbort();
        return "unsupported, empty, or oversized firmware response";
    }
    Update.onProgress(progressFunc);
    DEBUG_PRINT("FW Size: ");

    DEBUG_PRINTLN(totalLength);
    uint8_t buff[128] = {0};
    WiFiClient *stream = clientWeb.getStreamPtr();
    DEBUG_PRINTLN("Updating firmware...");
    int len = totalLength;
    unsigned long lastDataMs = millis();
    bool transferOk = true;
    while ((clientWeb.connected() || stream->available()) && (len > 0 || len == -1))
    {
        size_t size = stream->available();
        if (size)
        {
            size_t toRead = min(size, sizeof(buff));
            if (len > 0) toRead = min(toRead, size_t(len));
            int c = stream->readBytes(buff, toRead);
            if (c > 0) {
                if (!runEspUpdateFirmware(buff, c)) { transferOk = false; break; }
                lastDataMs = millis();
            }

            if (len > 0)
            {
                len -= c;
            }
        }
        if (millis() - lastDataMs > 15000) { transferOk = false; break; }
        processSocketBridge();
        delay(1);
    }
    clientWeb.end();

    if (!transferOk || currentLength == 0 || (totalLength > 0 && currentLength != totalLength))
    {
        Update.abort();
        fwHashAbort();
        printLogMsg("ESP OTA download incomplete");
        return fwIncompleteError;
    }
    if (!force && !verifyFirmwareSignature())
    {
        Update.abort();
        printLogMsg("ESP OTA rejected: " + String(fwRejectError));
        return fwRejectError;
    }
    if (!Update.end(true))
    {
        printLogMsg("ESP OTA finalize failed");
        return "update finalize failed";
    }
    bootGuardArm();
    DEBUG_PRINTLN("Update success. Rebooting...");
    esp_restart();
    return "reboot failed";
}
