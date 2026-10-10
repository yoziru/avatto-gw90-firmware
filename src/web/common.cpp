#include <Arduino.h>
#include <ETH.h>
#include <LittleFS.h>
#include <Update.h>
#include <WebServer.h>
#include <HTTPClient.h>

#include <esp_task_wdt.h>

#include "boot_guard.h"
#include "config.h"
#include "encoding.h"
#include "web.h"
#include "web_csrf.h"
#include "log.h"
#include "etc.h"
#include "zb.h"
#include "zones.h"
#include "version.h"

#include "webh/PAGE_LOADER.html.gz.h"
#include "webh/PAGE_LOGOUT.html.gz.h"

extern zbVerStruct zbVer;

WebServer serverWeb(80);

bool opened = false;
static bool zbUploadAuth = false;
File fwFile;
bool updateRejected = false;
bool updateResponded = false;

int totalLength;
int currentLength = 0;

const char *contTypeTextHtml = "text/html";
const char *contTypeTextJs = "text/javascript";
const char *contTypeTextCss = "text/css";
const char *checked = "true";
const char *respHeaderName = "respValuesArr";
const char *respTimeZonesName = "respTimeZones";
const char *contTypeJson = "application/json";
const char *contTypeText = "text/plain";

const char *tempFile = "/config/fw.hex";

#include "web_routes.h"

void webServerHandleClient()
{
    serverWeb.handleClient();
}

void sendGzip(const char *contentType, const uint8_t content[], uint16_t contentLen)
{
    serverWeb.sendHeader(F("Content-Encoding"), F("gzip"));
    serverWeb.send_P(HTTP_CODE_OK, contentType, (const char *)content, contentLen);
}

bool checkAuth(bool mutation)
{
    String source = serverWeb.header("Origin");
    if (source.length() == 0)
        source = serverWeb.header("Referer");
    String host = serverWeb.header("X-Forwarded-Host");
    if (host.length() == 0)
        host = serverWeb.hostHeader();
    if (!webRequestAllowed(source.c_str(), host.c_str(), mutation || serverWeb.method() != HTTP_GET,
                           serverWeb.header("X-Gateway-Request").c_str()))
    {
        printLogMsg("web: cross-origin request rejected: " + source);
        serverWeb.send(HTTP_CODE_FORBIDDEN, contTypeText, "request rejected; mutations require X-Gateway-Request: 1");
        return false;
    }

    if (!ConfigSettings.webAuth)
        return true;

    const char *www_realm = "Login Required";
    if (!serverWeb.authenticate(ConfigSettings.webUser, ConfigSettings.webPass))
    {
        serverWeb.requestAuthentication(DIGEST_AUTH, www_realm, "Authentication failed");
        return false;
    }
    return true;
}

template <typename Ring>
static void printTimeTo(Ring &ring)
{
    // Runs on every bridged frame and every log line - never allocate a String
    // here. A bounded snprintf into the stack is allocation-free. Wall clock
    // once NTP synced (same threshold as setTimezone), millis before that.
    char timeBuf[12];
    int tlen;
    const time_t nowSecs = time(nullptr);
    if (nowSecs >= 100000000)
    {
        struct tm timeinfo;
        localtime_r(&nowSecs, &timeinfo);
        tlen = snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
    else
    {
        tlen = snprintf(timeBuf, sizeof(timeBuf), "%lu", (unsigned long)millis());
    }
    ring.push('[');
    for (int j = 0; j < tlen; j++)
    {
        ring.push(timeBuf[j]);
    }
    ring.push(']');
}

void printLogTime()
{
    printTimeTo(logConsole);
}

void printLogTimeZb()
{
    printTimeTo(zbLogConsole);
}

void printLogMsg(const String &msg)
{
    printLogTime();
    logConsole.push(' ');
    logConsole.push('|');
    logConsole.push(' ');
    for (int j = 0; j < msg.length(); j++)
    {
        logConsole.push(msg[j]);
    }
    logConsole.push('\n');
}

static void sendUpdateResponse(int code, const char *message)
{
    if (updateResponded)
        return;
    updateResponded = true;
    serverWeb.send(code, contTypeText, message);
}

void initWebServer()
{
    serverWeb.onNotFound([]()
                         { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    // SPA shell: every page route serves the same loader; the JS router
    // fills #pageContent via the API.
    static const char *spaRoutes[] = {"/", "/general", "/security", "/wifi", "/ethernet", "/zha-z2m", "/about", "/sys-tools", "/mqtt", "/wg"};
    for (const char *route : spaRoutes)
    {
        serverWeb.on(route, []()
                     { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    }
    serverWeb.on("/saveParams", HTTP_POST, handleSaveParams);
    serverWeb.on("/cmdZigRST", handleZigbeeRestart);
    serverWeb.on("/cmdZigBSL", handleZigbeeBSL);
    serverWeb.on("/switch/firmware_update/toggle", handleZigbeeBSL);
    serverWeb.on("/api", handleApi);
    serverWeb.on("/status", handleStatus);
    serverWeb.on("/logout", []()
                 {
        serverWeb.sendHeader(F("Content-Encoding"), F("gzip"));
        serverWeb.send_P(401, contTypeTextHtml, (const char *)PAGE_LOGOUT_html_gz, PAGE_LOGOUT_html_gz_len); });

    serverWeb.on("/events", handleEvents);

    serverWeb.on(
        "/update", HTTP_POST, []()
        {
        if (updateResponded) {
            updateResponded = false;
            return;
        }
        serverWeb.sendHeader("Connection", "close");
        if (updateRejected)
        {
            sendUpdateResponse(HTTP_CODE_BAD_REQUEST, "unsigned firmware; add force=1 to install anyway");
            updateResponded = updateRejected = false;
            return;
        }
        if (!checkAuth(true)) return;
        sendUpdateResponse(HTTP_CODE_BAD_REQUEST, "no firmware uploaded");
        updateResponded = false; },
        []()
        {
            HTTPUpload &upload = serverWeb.upload();
            static long contentLength = 0;
            if (upload.status == UPLOAD_FILE_START)
            {
                updateResponded = false;
                updateRejected = false;
                if (!checkAuth())
                {
                    updateResponded = true;
                    return;
                }
                if (ConfigSettings.zbFlashing) {
                    sendUpdateResponse(HTTP_CODE_CONFLICT, "Zigbee update in progress");
                    return;
                }

                updateRejected = serverWeb.arg("force") != "1";
                if (updateRejected)
                {
                    DEBUG_PRINTLN("Unsigned firmware upload refused; add force=1 to confirm");
                    return;
                }

                DEBUG_PRINTLN("hostHeader: " + serverWeb.hostHeader());
                contentLength = serverWeb.header("Content-Length").toInt();
                DEBUG_PRINTLN("contentLength: " + String(contentLength));

                DEBUG_PRINTLN("Update ESP from file " + String(upload.filename.c_str()) + " size: " + String(upload.totalSize));
                DEBUG_PRINTLN("upload.currentSize " + String(upload.currentSize));
                if (!Update.begin(UPDATE_SIZE_UNKNOWN))
                {
                    Update.printError(Serial);
                    sendUpdateResponse(500, "could not start firmware upload");
                    return;
                }
                Update.onProgress(progressFunc);
            }
            else if (upload.status == UPLOAD_FILE_WRITE)
            {
                if (updateRejected || updateResponded)
                    return;
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
                {
                    Update.printError(Serial);
                    Update.abort();
                    sendUpdateResponse(500, "firmware upload write failed");
                }
            }
            else if (upload.status == UPLOAD_FILE_END)
            {
                if (updateRejected || updateResponded)
                    return;
                if (Update.end(true))
                {
                    bootGuardArm();
                    DEBUG_PRINTLN("Update success. Rebooting...");
                    sendUpdateResponse(HTTP_CODE_OK, "OK");
                    delay(2000);
                    ESP.restart();
                }
                else
                {
                    DEBUG_PRINTLN("Update error: ");
                    Update.printError(Serial);
                    sendUpdateResponse(500, "firmware verification failed");
                }
            }
            else if (upload.status == UPLOAD_FILE_ABORTED) {
                if (!updateRejected && !updateResponded) {
                    Update.abort();
                }
                updateResponded = updateRejected = false;
            }
        });

    serverWeb.on(
        "/updateZB", HTTP_POST, []()
        {
            if (!zbUploadAuth)
                return;
            zbUploadAuth = false;
            serverWeb.sendHeader("Connection", "close");
            serverWeb.send(HTTP_CODE_OK, contTypeText, "Upload OK. Try to flash...");
        },
        []()
        {
            HTTPUpload &upload = serverWeb.upload();
            if (upload.status == UPLOAD_FILE_START)
            {
                zbUploadAuth = checkAuth();
                if (!zbUploadAuth)
                    return;
                if (ConfigSettings.zbFlashing.exchange(true)) {
                    zbUploadAuth = false;
                    serverWeb.send(HTTP_CODE_CONFLICT, contTypeText, "Zigbee update already in progress");
                    return;
                }
                DEBUG_PRINTLN(String(millis()) + " Upload zigbee fw file: " + String(upload.filename.c_str()));
                printLogMsg("[ZB_FW] upload: " + String(upload.filename.c_str()));
            }
            if (!zbUploadAuth)
                return;
            if (opened == false)
            {
                opened = true;
                DEBUG_PRINTLN("Try to removed file " + String(tempFile));
                if (LittleFS.remove(tempFile))
                {
                    DEBUG_PRINTLN(F("Removed file - OK"));
                }
                else
                {
                    DEBUG_PRINTLN(F("Error while removing file"));
                }
                delay(250);
                fwFile = LittleFS.open(tempFile, FILE_WRITE);

                if (!fwFile)
                {
                    DEBUG_PRINTLN(F("- failed to open file for writing"));
                    ConfigSettings.zbFlashing = false;
                    opened = false;
                    zbUploadAuth = false;
                    serverWeb.send(500, contTypeText, "Could not create firmware file");
                    return;
                }
            }

            if (upload.status == UPLOAD_FILE_WRITE)
            {
                if (fwFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    fwFile.close();
                    LittleFS.remove(tempFile);
                    opened = zbUploadAuth = false;
                    ConfigSettings.zbFlashing = false;
                    serverWeb.send(500, contTypeText, "Zigbee firmware upload write failed");
                    return;
                }
            }
            else if (upload.status == UPLOAD_FILE_END)
            {
                delay(500);
                fwFile.close();
                delay(500);
                DEBUG_PRINTLN(String(millis()) + " UPLOAD_FILE_END");
                printLogMsg("[ZB_FW] upload finish! Size: " + String(upload.totalSize));
                Serial.println("[ZB_FW] File upload complete, total size: " + String(upload.totalSize) + " bytes");
                opened = false;

                DEBUG_PRINTLN("Total file size: " + String(upload.totalSize));

                Serial.println("[ZB_FW] Validating and flashing firmware...");
                if (!startZbFlashTask()) {
                    zbUploadAuth = false;
                    serverWeb.send(500, contTypeText, "Could not start firmware update task");
                }
            }
            else if (upload.status == UPLOAD_FILE_ABORTED) {
                fwFile.close();
                LittleFS.remove(tempFile);
                opened = zbUploadAuth = false;
                ConfigSettings.zbFlashing = false;
            }
        });

    const char *headerkeys[] = {"Content-Length", "Origin", "Referer", "X-Forwarded-Host", "X-Gateway-Request"};
    size_t headerkeyssize = sizeof(headerkeys) / sizeof(char *);
    registerStaticRoutes();
    serverWeb.collectHeaders(headerkeys, headerkeyssize);
    serverWeb.begin();
    DEBUG_PRINTLN(F("webserver setup done"));
}
