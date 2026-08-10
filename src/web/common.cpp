#include <Arduino.h>
#include <ETH.h>
#include <LittleFS.h>
#include <Update.h>
#include <WebServer.h>
#include <HTTPClient.h>

#include <esp_task_wdt.h>

#include "config.h"
#include "encoding.h"
#include "web.h"
#include "log.h"
#include "etc.h"
#include "zb.h"
#include "zones.h"
#include "version.h"

#include "webh/PAGE_LOADER.html.gz.h"
#include "webh/PAGE_LOGOUT.html.gz.h"

extern struct ConfigSettingsStruct ConfigSettings;
extern struct zbVerStruct zbVer;
extern struct MqttSettingsStruct MqttSettings;
extern struct WgSettingsStruct WgSettings;

WebServer serverWeb(80);

bool opened = false;
File fwFile;

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

String urlDecode(String input)
{
    std::string s = input.c_str();
    return String(urlDecode(s).c_str());
}

bool checkAuth()
{
    if (!ConfigSettings.webAuth)
        return true;
    const char *www_realm = "Login Required";
    if (!serverWeb.authenticate(ConfigSettings.webUser, ConfigSettings.webPass))
    {
        serverWeb.requestAuthentication(DIGEST_AUTH, www_realm, "Authentication failed");
        return false;
    }
    else
    {
        return true;
    }
}

void printLogTime()
{
    String tmpTime;
    unsigned long timeLog = millis();
    tmpTime = String(timeLog, DEC);
    logPush('[');
    for (int j = 0; j < tmpTime.length(); j++)
    {
        logPush(tmpTime[j]);
    }
    logPush(']');
}

void printLogMsg(String msg)
{
    printLogTime();
    logPush(' ');
    logPush('|');
    logPush(' ');
    for (int j = 0; j < msg.length(); j++)
    {
        logPush(msg[j]);
    }
    logPush('\n');
}

void initWebServer()
{
    serverWeb.onNotFound([]()
                         { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/general", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/security", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/wifi", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/ethernet", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/zha-z2m", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/about", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/sys-tools", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/mqtt", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/wg", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/saveParams", HTTP_POST, handleSaveParams);
    serverWeb.on("/cmdZigRST", handleZigbeeRestart);
    serverWeb.on("/cmdZigBSL", handleZigbeeBSL);
    serverWeb.on("/saveFile", handleSavefile);
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
        serverWeb.sendHeader("Connection", "close");
        serverWeb.send(HTTP_CODE_OK, contTypeText, (Update.hasError()) ? "FAIL" : "OK"); },
        []()
        {
            HTTPUpload &upload = serverWeb.upload();
            static long contentLength = 0;
            if (upload.status == UPLOAD_FILE_START)
            {
                if (!checkAuth())
                    return;

                DEBUG_PRINTLN("hostHeader: " + serverWeb.hostHeader());
                contentLength = serverWeb.header("Content-Length").toInt();
                DEBUG_PRINTLN("contentLength: " + String(contentLength));

                DEBUG_PRINTLN("Update ESP from file " + String(upload.filename.c_str()) + " size: " + String(upload.totalSize));
                DEBUG_PRINTLN("upload.currentSize " + String(upload.currentSize));
                if (!Update.begin(contentLength))
                {
                    Update.printError(Serial);
                }
                Update.onProgress(progressFunc);
            }
            else if (upload.status == UPLOAD_FILE_WRITE)
            {
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
                {
                    Update.printError(Serial);
                }
            }
            else if (upload.status == UPLOAD_FILE_END)
            {
                if (Update.end(true))
                {
                    DEBUG_PRINTLN("Update success. Rebooting...");
                    serverWeb.send(HTTP_CODE_OK, contTypeText, "OK");
                    delay(2000);
                    ESP.restart();
                }
                else
                {
                    DEBUG_PRINTLN("Update error: ");
                    Update.printError(Serial);
                    serverWeb.send(HTTP_CODE_OK, contTypeText, "FAIL");
                }
            }
        });

    serverWeb.on(
        "/updateZB", HTTP_POST, []()
        {
            serverWeb.sendHeader("Connection", "close");
            serverWeb.send(HTTP_CODE_OK, contTypeText, "Upload OK. Try to flash...");
        },
        []()
        {
            HTTPUpload &upload = serverWeb.upload();
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
                    return;
                }
            }

            if (upload.status == UPLOAD_FILE_START)
            {
                if (!checkAuth())
                    return;
                DEBUG_PRINTLN(String(millis()) + " Upload zigbee fw file: " + String(upload.filename.c_str()));
                printLogMsg("[ZB_FW] upload: " + String(upload.filename.c_str()));
            }
            else if (upload.status == UPLOAD_FILE_WRITE)
            {
                fwFile.write(upload.buf, sizeof(upload.buf));
            }
            else if (upload.status == UPLOAD_FILE_END)
            {
                serverWeb.send(HTTP_CODE_OK, contTypeText, "OK");
                delay(500);
                fwFile.close();
                delay(500);
                DEBUG_PRINTLN(String(millis()) + " UPLOAD_FILE_END");
                printLogMsg("[ZB_FW] upload finish! Size: " + String(upload.totalSize));
                Serial.println("[ZB_FW] File upload complete, total size: " + String(upload.totalSize) + " bytes");
                opened = false;

                DEBUG_PRINTLN("Total file size: " + String(upload.totalSize));

                Serial.println("[ZB_FW] Validating and flashing firmware...");
                xTaskCreatePinnedToCore(checkFwHexTask,
                            "CheckFWHex",
                            8192,
                            (void *)tempFile,
                            3,
                            NULL,
                            0);
            }
        });

    const char *headerkeys[] = {"Content-Length"};
    size_t headerkeyssize = sizeof(headerkeys) / sizeof(char *);
    registerStaticRoutes();
    serverWeb.collectHeaders(headerkeys, headerkeyssize);
    serverWeb.begin();
    DEBUG_PRINTLN(F("webserver setup done"));
}
