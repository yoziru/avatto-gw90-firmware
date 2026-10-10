#include <Arduino.h>
#include <ArduinoJson.h>
#include <ETH.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <LittleFS.h>
#include <WiFi.h>

#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#include <miniz.h>

#include "config.h"
#include "encoding.h"
#include "web.h"
#include "log.h"
#include "etc.h"
#include "zb.h"
#include "zb_ver.h"
#include "zb_info.h"
#include "version.h"
#include "net.h"
#include "tls_client.h"

#include "webh/PAGE_ROOT.html.gz.h"
#include "webh/PAGE_GENERAL.html.gz.h"
#include "webh/PAGE_ETHERNET.html.gz.h"
#include "webh/PAGE_WIFI.html.gz.h"
#include "webh/PAGE_SERIAL.html.gz.h"
#include "webh/PAGE_SECURITY.html.gz.h"
#include "webh/PAGE_SYSTOOLS.html.gz.h"
#include "webh/PAGE_ABOUT.html.gz.h"
#include "webh/PAGE_MQTT.html.gz.h"
#include "webh/PAGE_WG.html.gz.h"

extern zbVerStruct zbVer;

namespace
{
constexpr uint32_t ZIP_LOCAL_FILE_HEADER = 0x04034B50;
constexpr uint16_t ZIP_DEFLATED = 8;
constexpr uint16_t ZIP_DATA_DESCRIPTOR = 0x0008;
constexpr uint16_t ZIP_ENCRYPTED = 0x0001;
const char *const downloadedFirmwareFile = "/config/fw.download";
const char *const extractedFirmwareFile = "/config/fw.hex.new";

uint16_t readLittleEndian16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t readLittleEndian32(const uint8_t *data)
{
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

struct ZipOutput
{
    File *file;
    uint32_t expectedSize;
    uint32_t expectedCrc;
    uint32_t size = 0;
    uint32_t crc = MZ_CRC32_INIT;
};

bool writeZipOutput(ZipOutput &output, const uint8_t *data, size_t length)
{
    if (length == 0 || length > output.expectedSize - output.size)
        return length == 0;

    if (output.file->write(data, length) != length)
        return false;

    output.crc = static_cast<uint32_t>(mz_crc32(output.crc, data, length));
    output.size += length;
    return true;
}

int writeZipOutputCallback(const void *data, int length, void *user)
{
    if (length < 0)
        return 0;
    return writeZipOutput(*static_cast<ZipOutput *>(user),
                          static_cast<const uint8_t *>(data),
                          static_cast<size_t>(length)) ? 1 : 0;
}

bool extractHexFromZip(const char *zipPath)
{
    File zip = LittleFS.open(zipPath, FILE_READ);
    if (!zip)
        return false;

    uint8_t header[30];
    if (zip.read(header, sizeof(header)) != sizeof(header) ||
        readLittleEndian32(header) != ZIP_LOCAL_FILE_HEADER)
    {
        return false;
    }

    const uint16_t flags = readLittleEndian16(header + 6);
    const uint16_t method = readLittleEndian16(header + 8);
    const uint32_t compressedSize = readLittleEndian32(header + 18);
    const uint32_t uncompressedSize = readLittleEndian32(header + 22);
    const uint16_t filenameLength = readLittleEndian16(header + 26);
    const uint16_t extraLength = readLittleEndian16(header + 28);

    if ((flags & (ZIP_ENCRYPTED | ZIP_DATA_DESCRIPTOR)) != 0 ||
        (method != 0 && method != ZIP_DEFLATED) ||
        compressedSize == 0 || uncompressedSize == 0 ||
        filenameLength < 4 || filenameLength >= 128)
    {
        return false;
    }

    char filename[128];
    if (zip.read(reinterpret_cast<uint8_t *>(filename), filenameLength) != filenameLength)
        return false;
    filename[filenameLength] = '\0';
    if (String(filename).endsWith(".hex") == false)
        return false;

    const uint32_t dataOffset = 30U + filenameLength + extraLength;
    if (!zip.seek(dataOffset) ||
        static_cast<uint64_t>(dataOffset) + compressedSize > zip.size())
    {
        return false;
    }

    LittleFS.remove(extractedFirmwareFile);
    File hex = LittleFS.open(extractedFirmwareFile, FILE_WRITE);
    if (!hex)
        return false;

    ZipOutput output{&hex, uncompressedSize, readLittleEndian32(header + 14)};
    bool ok = true;

    if (method == 0)
    {
        uint8_t buffer[1024];
        uint32_t remaining = compressedSize;
        while (remaining > 0 && ok)
        {
            const size_t wanted = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
            const size_t received = zip.read(buffer, wanted);
            if (received == 0)
            {
                ok = false;
                break;
            }
            ok = writeZipOutput(output, buffer, received);
            remaining -= received;
        }
    }
    else
    {
        uint8_t *compressed = static_cast<uint8_t *>(heap_caps_malloc(compressedSize, MALLOC_CAP_8BIT));
        if (!compressed || zip.read(compressed, compressedSize) != compressedSize)
        {
            if (compressed)
                heap_caps_free(compressed);
            hex.close();
            LittleFS.remove(extractedFirmwareFile);
            return false;
        }

        size_t inputSize = compressedSize;
        ok = tinfl_decompress_mem_to_callback(compressed, &inputSize,
                                               writeZipOutputCallback, &output, 0) == 1;
        heap_caps_free(compressed);
    }

    hex.close();
    if (!ok || output.size != uncompressedSize || output.crc != output.expectedCrc)
    {
        LittleFS.remove(extractedFirmwareFile);
        return false;
    }

    LittleFS.remove(tempFile);
    return LittleFS.rename(extractedFirmwareFile, tempFile);
}

bool prepareZigbeeFirmware(const char *downloadPath)
{
    File downloaded = LittleFS.open(downloadPath, FILE_READ);
    if (!downloaded)
        return false;

    uint8_t signature[4];
    const bool isZip = downloaded.read(signature, sizeof(signature)) == sizeof(signature) &&
                       readLittleEndian32(signature) == ZIP_LOCAL_FILE_HEADER;
    downloaded.close();

    if (isZip)
        return extractHexFromZip(downloadPath);

    LittleFS.remove(tempFile);
    return LittleFS.rename(downloadPath, tempFile);
}
}

void handleZigbeeBSL()
{
    if (checkAuth(true))
    {
        if (ConfigSettings.zbFlashing) {
            serverWeb.send(HTTP_CODE_CONFLICT, contTypeText, "Zigbee update in progress");
            return;
        }
        zigbeeEnableBSL();
        serverWeb.send(HTTP_CODE_OK, contTypeText, "");
    }
}

void handleZigbeeRestart()
{
    if (checkAuth(true))
    {
        if (ConfigSettings.zbFlashing) {
            serverWeb.send(HTTP_CODE_CONFLICT, contTypeText, "Zigbee update in progress");
            return;
        }
        zigbeeRestart();
        serverWeb.send(HTTP_CODE_OK, contTypeText, "");
    }
}

void handleApi()
{
    enum API_ACTION_t : uint8_t
    {
        API_GET_PAGE,
        API_GET_PARAM,
        API_STARTWIFISCAN,
        API_WIFISCANSTATUS,
        API_SEND_HEX,
        API_WIFICONNECTSTAT,
        API_CMD,
        API_GET_LOG,
        API_FLASH_ZB
    };
    const char *action = "action";
    const char *page = "page";
    const char *Authentication = "Authentication";
    const char *param = "param";
    const char *wrongArgs = "wrong args";
    const char *ok = "ok";

    const int requestedAction = serverWeb.arg(action).toInt();
    const bool mutation = requestedAction == API_CMD || requestedAction == API_FLASH_ZB ||
                          requestedAction == API_STARTWIFISCAN || requestedAction == API_WIFISCANSTATUS ||
                          requestedAction == API_SEND_HEX ||
                          (requestedAction == API_GET_PARAM && serverWeb.arg(param) == "zbRole");
    if (!checkAuth(mutation))
        return;
    if (ConfigSettings.webAuth)
    {
        serverWeb.sendHeader(Authentication, ok);
    }
    if (serverWeb.argName(0) != action)
    {
        DEBUG_PRINT(F("[handleApi] wrong arg 'action' "));
        DEBUG_PRINTLN(serverWeb.argName(0));
        serverWeb.send(500, contTypeText, wrongArgs);
    }
    else
    {
        const uint8_t actionId = serverWeb.arg(action).toInt();
        switch (actionId)
        {
        case API_FLASH_ZB:
        {
            Serial.println("\n\n========== api_update zigbee =========");
            if (ConfigSettings.zbFlashing) {
                serverWeb.send(HTTP_CODE_CONFLICT, contTypeText, "Zigbee update already in progress");
                break;
            }
            const char *fwurlArg = "fwurl";
            const char *fwRoleArg = "role";
            const char *confirmRoleSwitchArg = "confirmRoleSwitch";
            const char *tagZB_FW_info = "ZB_FW_info";
            const char *tagZB_FW_err = "ZB_FW_err";
            const char *tagZB_FW_download = "ZB_FW_downloading";
            const char *tagZB_FW_flash = "ZB_FW_flashing";

            if (serverWeb.hasArg(fwurlArg))
            {
                String fwUrl = serverWeb.arg(fwurlArg);
                fwUrl = String(urlDecode(fwUrl.c_str()).c_str());

                String requestedRole = serverWeb.hasArg(fwRoleArg) ? serverWeb.arg(fwRoleArg) : "";
                requestedRole.toLowerCase();
                String fwUrlLower = fwUrl;
                fwUrlLower.toLowerCase();
                String urlRole;
                if (fwUrlLower.indexOf("_router_") >= 0)
                    urlRole = "router";
                else if (fwUrlLower.indexOf("_coordinator_") >= 0)
                    urlRole = "coordinator";

                if (requestedRole.length() && requestedRole != "router" && requestedRole != "coordinator")
                {
                    serverWeb.send(HTTP_CODE_BAD_REQUEST, contTypeText, "invalid firmware role");
                    ConfigSettings.zbFlashing = 0;
                    break;
                }
                if (urlRole.length() && requestedRole.length() && urlRole != requestedRole)
                {
                    serverWeb.send(HTTP_CODE_BAD_REQUEST, contTypeText, "firmware URL role mismatch");
                    ConfigSettings.zbFlashing = 0;
                    break;
                }

                const String targetRole = requestedRole.length() ? requestedRole : urlRole;

                // Safety: a role from /config/zbInfo.json is last-known display
                // data and may be stale. Only a live detection in this boot may
                // authorize flashing, so if this boot has not confirmed the role
                // yet, query the coordinator now and refuse when it stays unknown.
                if (!zbRoleDetectedThisBoot())
                {
                    getZbVer();
                }
                const ZbRole installedZbRole = zbRoleDetectedThisBoot()
                                                   ? zbRoleFromDeviceState(zbVer.deviceState)
                                                   : ZbRole::UNKNOWN;
                String installedRole = zbRoleName(installedZbRole);
                installedRole.toLowerCase();
                if (!targetRole.length())
                {
                    serverWeb.send(HTTP_CODE_BAD_REQUEST, contTypeText, "firmware role is required");
                    ConfigSettings.zbFlashing = 0;
                    break;
                }
                if (installedZbRole == ZbRole::UNKNOWN)
                {
                    serverWeb.send(HTTP_CODE_CONFLICT, contTypeText, "installed firmware role could not be detected");
                    sendEvent(tagZB_FW_err, sizeof(tagZB_FW_err), "Installed firmware role could not be detected");
                    ConfigSettings.zbFlashing = 0;
                    break;
                }
                if (targetRole != installedRole && serverWeb.arg(confirmRoleSwitchArg) != "1")
                {
                    serverWeb.send(HTTP_CODE_CONFLICT, contTypeText, "firmware role switch requires confirmation");
                    sendEvent(tagZB_FW_err, sizeof(tagZB_FW_err), "Firmware role switch requires confirmation");
                    ConfigSettings.zbFlashing = 0;
                    break;
                }

                Serial.print("Firmware URL: ");
                Serial.println(fwUrl);

                bool wifiConnected = (WiFi.status() == WL_CONNECTED);
                bool ethConnected = ETH.linkUp();

                if (!wifiConnected && !ethConnected) {
                    Serial.println("No network connection!");
                    serverWeb.send(HTTP_CODE_SERVICE_UNAVAILABLE, contTypeText, "No network connection");
                    sendEvent(tagZB_FW_err, sizeof(tagZB_FW_err), "No network connection!");
                    ConfigSettings.zbFlashing = 0;
                    break;
                }

                ConfigSettings.zbFlashing = true;
                serverWeb.send(HTTP_CODE_OK, contTypeText, ok);

                uint8_t evWaitCount = 0;
                while (!eventsClient.connected() && evWaitCount < 20)
                {
                    webServerHandleClient();
                    delay(50);
                    evWaitCount++;
                }

                if (!eventsClient.connected()) {
                    Serial.println("Events client not ready, using polling mode");
                }

                HTTPClient http;
                NetworkClientSecure client;
                trustEspCertificateBundle(client);
                client.setTimeout(10000);

                http.begin(client, fwUrl);
                http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
                http.addHeader("Accept", "*/*");
                http.addHeader("Accept-Encoding", "identity");
                http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
                http.setTimeout(60000);

                Serial.println("Connecting to firmware server...");
                const int16_t httpCode = http.GET();

                int32_t fwSize = http.getSize();

                if (httpCode != HTTP_CODE_OK)
                {
                    Serial.print("HTTP Error: ");
                    Serial.println(httpCode);
                    sendEvent(tagZB_FW_err, sizeof(tagZB_FW_err), "HTTP Error: " + String(httpCode));
                    http.end();
                    ConfigSettings.zbFlashing = 0;
                    break;
                }

                Serial.println("Connection successful!");

                LittleFS.remove(downloadedFirmwareFile);

                File fwFile = LittleFS.open(downloadedFirmwareFile, "w");
                if (!fwFile)
                {
                    sendEvent(tagZB_FW_err, sizeof(tagZB_FW_err), "Failed to open file for writing");
                    ConfigSettings.zbFlashing = 0;
                    http.end();
                    break;
                }
                sendEvent(tagZB_FW_info, sizeof(tagZB_FW_info), "[start]");
                sendEvent(tagZB_FW_info, sizeof(tagZB_FW_info), "Downloading firmware...");

                WiFiClient *stream = http.getStreamPtr();
                uint8_t buff[1024];
                uint32_t downloaded = 0;
                unsigned long lastDataMs = millis();
                unsigned long lastProgressTime = 0;
                uint8_t percent = 0;
                // getSize() returns -1/0 when the server omits Content-Length;
                // then we download until the stream ends instead of trying to
                // compute percentages (which would divide by zero).
                const bool knownSize = (fwSize > 0);

                while (http.connected() || stream->available())
                {
                    size_t available = stream->available();

                    if (knownSize && downloaded >= (uint32_t)fwSize)
                        break;

                    if (available > 0)
                    {
                        size_t toRead = available;
                        if (toRead > sizeof(buff)) {
                            toRead = sizeof(buff);
                        }

                        int bytesRead = stream->read(buff, toRead);

                        if (bytesRead > 0)
                        {
                            if (fwFile.write(buff, bytesRead) != size_t(bytesRead)) {
                                fwFile.close();
                                LittleFS.remove(downloadedFirmwareFile);
                                ConfigSettings.zbFlashing = false;
                                http.end();
                                sendEvent(tagZB_FW_err, sizeof(tagZB_FW_err), "Firmware download write failed");
                                return;
                            }
                            downloaded += bytesRead;
                            lastDataMs = millis();

                            unsigned long now = millis();
                            if (knownSize && ((now - lastProgressTime > 500) || (downloaded % 10240 == 0))) {
                                percent = ((float)downloaded / fwSize) * 100;
                                sendEvent(tagZB_FW_download, sizeof(tagZB_FW_download), String(percent));
                                lastProgressTime = now;
                            }
                        }
                    }
                    else
                    {
                        if (millis() - lastDataMs > 200000) {
                            Serial.println("Download timeout!");
                            sendEvent(tagZB_FW_err, sizeof(tagZB_FW_err), "Download timeout!");
                            fwFile.close();
                            ConfigSettings.zbFlashing = 0;
                            http.end();
                            return;
                        }

                        delay(10);
                    }

                    static uint16_t loopCount = 0;
                    if (++loopCount % 10 == 0) {
                        webServerHandleClient();
                    }
                }
                fwFile.close();
                http.end();

                if (!knownSize || (int32_t)downloaded == fwSize)
                {
                    if (knownSize)
                    {
                        percent = ((float)downloaded / fwSize) * 100;
                        sendEvent(tagZB_FW_download, sizeof(tagZB_FW_download), String(percent));
                    }

                    sendEvent(tagZB_FW_info, sizeof(tagZB_FW_info), "Download complete!");
                    sendEvent(tagZB_FW_info, sizeof(tagZB_FW_info), String("Total size: ") + String(downloaded) + " bytes");
                    sendEvent(tagZB_FW_info, sizeof(tagZB_FW_info), "Preparing firmware...");

                    if (prepareZigbeeFirmware(downloadedFirmwareFile))
                    {
                        LittleFS.remove(downloadedFirmwareFile);
                        sendEvent(tagZB_FW_info, sizeof(tagZB_FW_info), "Validating and flashing...");
                        if (startZbFlashTask()) return; // task releases ownership
                    }
                    else
                    {
                        LittleFS.remove(downloadedFirmwareFile);
                        sendEvent(tagZB_FW_err, sizeof(tagZB_FW_err), "Invalid firmware archive");
                    }
                }
                else
                {
                    LittleFS.remove(downloadedFirmwareFile);
                    sendEvent(tagZB_FW_err, sizeof(tagZB_FW_err), "Incomplete firmware download");
                }
            }
            else
            {
                serverWeb.send(HTTP_CODE_BAD_REQUEST, contTypeText, "missing arg 1");
            }
            ConfigSettings.zbFlashing = 0;
        }
        break;
        case API_GET_LOG:
        {
            // logPrint*() returns a reused String (the console polls every
            // second); send it directly so it isn't copied into a new buffer.
            // src=zb serves the ZB frame ring, default is the event ring.
            serverWeb.send(HTTP_CODE_OK, contTypeText,
                           serverWeb.arg("src") == "zb" ? logPrintZb() : logPrint());
        }
        break;
        case API_CMD:
        {
            if (ConfigSettings.zbFlashing) {
                serverWeb.send(HTTP_CODE_CONFLICT, contTypeText, "Zigbee update in progress");
                break;
            }
            enum CMD_t : uint8_t
            {
                CMD_ZB_ROUTER_RECON = 0,
                CMD_ZB_RST = 1,
                CMD_ZB_BSL = 2,
                CMD_ESP_RES = 3,
                // 4, 5 were removed toggles (ZB frame log, temporary mode);
                // numbers are kept so existing data-cmd / API callers don't shift.
                CMD_LED_PWR_TOG = 6,
                CMD_LED_USB_TOG = 7,
                CMD_CLEAR_LOG = 8,
                CMD_ESP_UPD_URL = 9,
                CMD_ZB_CHK_REV = 10,
                CMD_ZB_CHK_CON = 11,
                CMD_ZB_LED_TOG = 12
            };
            String result = wrongArgs;
            const char *argCmd = "cmd";
            const char *argUrl = "url";
            if (serverWeb.hasArg(argCmd))
            {
                result = "ok";
                switch (serverWeb.arg(argCmd).toInt())
                {
                case CMD_CLEAR_LOG:
                    logClear();
                    break;
                case CMD_ZB_ROUTER_RECON:
                    zigbeeRouterRejoin();
                    break;
                case CMD_ZB_RST:
                    zigbeeRestart();
                    break;
                case CMD_ZB_BSL:
                    zigbeeEnableBSL();
                    break;
                case CMD_ESP_RES:
                    serverWeb.send(HTTP_CODE_OK, contTypeText, result);
                    ESP.restart();
                    break;
                case CMD_LED_PWR_TOG:
                    ledPowerToggle();
                    break;
                case CMD_LED_USB_TOG:
                    ledUSBToggle();
                    break;
                case CMD_ESP_UPD_URL:
                {
                    const bool force = serverWeb.arg("force") == "1";
                    if (serverWeb.hasArg(argUrl))
                        result = getEspUpdate(serverWeb.arg(argUrl), force);
                    else
                        result = getEspUpdate(UPD_FILE, force);
                }
                break;
                case CMD_ZB_CHK_REV:
                    getZbVer();
                    break;
                case CMD_ZB_CHK_CON:
                    zbCheck();
                    break;
                case CMD_ZB_LED_TOG:
                    zbLedToggle();
                    break;
                default:
                    break;
                }
                serverWeb.send(HTTP_CODE_OK, contTypeText, result);
            }
        }
        break;
        case API_WIFICONNECTSTAT:
        {
            String result;
            JsonDocument doc;
            const char *connected = "connected";
            const wl_status_t wifiStatus = WiFi.status();
            doc["status"] = static_cast<uint8_t>(wifiStatus);
            if (wifiStatus == WL_CONNECTED)
            {
                doc[connected] = true;
                doc["ip"] = WiFi.localIP().toString();
                doc["mask"] = WiFi.subnetMask().toString();
                doc["gw"] = WiFi.gatewayIP().toString();
                doc["ssid"] = WiFi.SSID();
            }
            else
            {
                doc[connected] = false;
                if (wifiLastDisconnectReason != 0)
                {
                    doc["reason"] = wifiLastDisconnectReason;
                    doc["reasonName"] = WiFi.disconnectReasonName(
                        static_cast<wifi_err_reason_t>(wifiLastDisconnectReason));
                }
            }
            serializeJson(doc, result);
            serverWeb.send(HTTP_CODE_OK, contTypeJson, result);
        }
        break;
        case API_GET_PARAM:
        {
            String resp = wrongArgs;
            if (serverWeb.hasArg(param))
            {
                if (serverWeb.arg(param) == "refreshLogs")
                {
                    resp = (String)ConfigSettings.refreshLogs;
                }
                else if (serverWeb.arg(param) == "coordMode")
                {
                    resp = String(static_cast<uint8_t>(ConfigSettings.coordinator_mode));
                }
                else if (serverWeb.arg(param) == "zbRev")
                {
                    char revBuf[16];
                    resp = zbRevDisplay(zbInfoDetectState, zbVer.zbRev, revBuf, sizeof(revBuf));
                }
                else if (serverWeb.arg(param) == "zbRole")
                {
                    // Cache is display-only: when a finished attempt left no
                    // role, trigger one live read here. While a detection is
                    // already pending/scheduled, report "checking" instead of
                    // issuing a duplicate query. Flashing requires its own
                    // live detection this boot (API_FLASH_ZB).
                    const bool roleKnown = zbInfoDetectState == ZbDetectState::KNOWN &&
                                           zbRoleFromDeviceState(zbVer.deviceState) != ZbRole::UNKNOWN;
                    if (zbInfoDetectState != ZbDetectState::DETECTING && !roleKnown)
                    {
                        getZbVer();
                    }
                    resp = zbRoleDisplay(zbInfoDetectState, zbVer.deviceState);
                }
                else if (serverWeb.arg(param) == "espVer")
                {
                    resp = VERSION;
                }
            }
            serverWeb.send(HTTP_CODE_OK, contTypeText, resp);
        }
        break;
        case API_STARTWIFISCAN:
            if (WiFi.getMode() == WIFI_OFF)
            {
                WiFi.mode(WIFI_STA);
            }
            WiFi.scanNetworks(true);
            serverWeb.send(HTTP_CODE_OK, contTypeTextHtml, ok);
            break;
        case API_WIFISCANSTATUS:
        {
            static uint8_t timeout = 0;
            JsonDocument doc;
            String result = "";
            int16_t scanRes = WiFi.scanComplete();
            const char *scanDone = "scanDone";
            doc[scanDone] = false;
            if (scanRes == -2)
            {
                WiFi.scanNetworks(true);
            }
            else if (scanRes > 0)
            {
                doc[scanDone] = true;
                JsonArray wifi = doc["wifi"].to<JsonArray>();
                for (int i = 0; i < scanRes; ++i)
                {
                    JsonObject wifi_0 = wifi.add<JsonObject>();
                    wifi_0["ssid"] = WiFi.SSID(i);
                    wifi_0["rssi"] = WiFi.RSSI(i);
                    wifi_0["channel"] = WiFi.channel(i);
                    wifi_0["secure"] = WiFi.encryptionType(i);
                }
                WiFi.scanDelete();
            }
            if (timeout < 10)
            {
                timeout++;
            }
            else
            {
                doc[scanDone] = true;
                WiFi.scanDelete();
            }
            if(doc[scanDone] == true)timeout = 0;
            serializeJson(doc, result);
            serverWeb.send(HTTP_CODE_OK, contTypeJson, result);
            break;
        }
        case API_GET_PAGE:
            if (!serverWeb.arg(page).length() > 0)
            {
                DEBUG_PRINTLN(F("[handleApi] wrong arg 'page'"));
                DEBUG_PRINTLN(serverWeb.argName(1));
                serverWeb.send(500, contTypeText, wrongArgs);
                return;
            }
            switch (serverWeb.arg(page).toInt())
            {
            case API_PAGE_ROOT:
                handleRoot();
                sendGzip(contTypeTextHtml, PAGE_ROOT_html_gz, PAGE_ROOT_html_gz_len);
                break;
            case API_PAGE_GENERAL:
                handleGeneral();
                sendGzip(contTypeTextHtml, PAGE_GENERAL_html_gz, PAGE_GENERAL_html_gz_len);
                break;
            case API_PAGE_ETHERNET:
                handleEther();
                sendGzip(contTypeTextHtml, PAGE_ETHERNET_html_gz, PAGE_ETHERNET_html_gz_len);
                break;
            case API_PAGE_WIFI:
                handleWifi();
                sendGzip(contTypeTextHtml, PAGE_WIFI_html_gz, PAGE_WIFI_html_gz_len);
                break;
            case API_PAGE_ZHA_Z2M:
                handleSerial();
                sendGzip(contTypeTextHtml, PAGE_SERIAL_html_gz, PAGE_SERIAL_html_gz_len);
                break;
            case API_PAGE_SECURITY:
                handleSecurity();
                sendGzip(contTypeTextHtml, PAGE_SECURITY_html_gz, PAGE_SECURITY_html_gz_len);
                break;
            case API_PAGE_SYSTOOLS:
                handleSysTools();
                sendGzip(contTypeTextHtml, PAGE_SYSTOOLS_html_gz, PAGE_SYSTOOLS_html_gz_len);
                break;
            case API_PAGE_ABOUT:
                handleAbout();
                sendGzip(contTypeTextHtml, PAGE_ABOUT_html_gz, PAGE_ABOUT_html_gz_len);
                break;
            case API_PAGE_MQTT:
                handleMqtt();
                sendGzip(contTypeTextHtml, PAGE_MQTT_html_gz, PAGE_MQTT_html_gz_len);
                break;
            case API_PAGE_WG:
                handleWg();
                sendGzip(contTypeTextHtml, PAGE_WG_html_gz, PAGE_WG_html_gz_len);
                break;
            default:
                break;
            }
            break;

        default:
            DEBUG_PRINTLN(F("[handleApi] switch (action) err"));
            break;
        }
    }
}
