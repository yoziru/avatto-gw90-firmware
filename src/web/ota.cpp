#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <LittleFS.h>
#include <Update.h>
#include <WiFi.h>

#include <esp_task_wdt.h>

#include "boot_guard.h"
#include "config.h"
#include "web.h"
#include "zb.h"
#include "tls_client.h"

void checkFwHexTask(void *param)
{
    const char *hexFilePath = static_cast<const char *>(param);
    Serial.println("\n[ZB_FW_TASK] Starting firmware validation task...");

    sendEvent("ZB_FW_info", sizeof("ZB_FW_info"), "Validating firmware...");

    esp_task_wdt_reset();

    if (0 == checkFwHex(hexFilePath)) {
        sendEvent("ZB_FW_info", sizeof("ZB_FW_info"), "Update done!");
    } else {
        sendEvent("ZB_FW_err", sizeof("ZB_FW_err"), "Update failed!");
    }

    Serial.println("[ZB_FW_TASK] Firmware update task completed");

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

void runEspUpdateFirmware(uint8_t *data, size_t len)
{
    Update.write(data, len);
    currentLength += len;

    if (currentLength != totalLength)
        return;
    Update.end(true);
    bootGuardArm();
    DEBUG_PRINTLN("Update success. Rebooting...");
    ESP.restart();
}

void getEspUpdate(String esp_fw_url)
{
    DEBUG_PRINTLN("getEspUpdate: " + esp_fw_url);
    printLogMsg("ESP OTA download start");
    HTTPClient clientWeb;
    NetworkClientSecure client;
    trustEspCertificateBundle(client);
    clientWeb.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    clientWeb.begin(client, esp_fw_url);
    clientWeb.addHeader("Content-Type", "application/octet-stream");

    int resp = clientWeb.GET();
    DEBUG_PRINTLN("Response: " + resp);
    if (resp == HTTP_CODE_OK)
    {
        totalLength = clientWeb.getSize();
        int len = totalLength;
        Update.begin(totalLength);
        Update.onProgress(progressFunc);
        DEBUG_PRINT("FW Size: ");

        DEBUG_PRINTLN(totalLength);
        uint8_t buff[128] = {0};
        WiFiClient *stream = clientWeb.getStreamPtr();
        DEBUG_PRINTLN("Updating firmware...");
        while (clientWeb.connected() && (len > 0 || len == -1))
        {
            size_t size = stream->available();
            if (size)
            {
                int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                runEspUpdateFirmware(buff, c);

                if (len > 0)
                {
                    len -= c;
                }
            }
        }
    }
    else
    {
        DEBUG_PRINTLN("Cannot download firmware file.");
        printLogMsg("ESP OTA download failed");
    }
    clientWeb.end();
}
