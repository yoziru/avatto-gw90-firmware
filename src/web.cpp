#include <Arduino.h>
#include <ArduinoJson.h>
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
#include "webh/PAGE_LOGOUT.html.gz.h"

extern struct ConfigSettingsStruct ConfigSettings;
extern struct zbVerStruct zbVer;
extern struct MqttSettingsStruct MqttSettings;
extern struct WgSettingsStruct WgSettings;

WebServer serverWeb(80);
WiFiClient eventsClient;

bool wifiWebSetupInProgress = false;
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

void handleEvents()
{
    eventsClient = serverWeb.client();
    if (eventsClient)
    {
        eventsClient.println("HTTP/1.1 200 OK");
        eventsClient.println("Content-Type: text/event-stream;");
        eventsClient.println("Connection: close");
        eventsClient.println("Access-Control-Allow-Origin: *");
        eventsClient.println("Cache-Control: no-cache");
        eventsClient.println();
        eventsClient.flush();
    }
}

void sendEvent(const char *event, const uint8_t evsz, const String data)
{
    if (eventsClient)
    {
        char evnmArr[10 + evsz];
        sprintf(evnmArr, "event: %s\n", event);
        eventsClient.print(evnmArr);
        eventsClient.print(String("data: ") + data + "\n\n");
        eventsClient.flush();
    }
}

void initWebServer()
{
    serverWeb.on("/js/bootstrap.min.js", []()
                 { sendGzip(contTypeTextJs, bootstrap_min_js_gz, bootstrap_min_js_gz_len); });
    serverWeb.on("/js/masonry.js", []()
                 { sendGzip(contTypeTextJs, masonry_js_gz, masonry_js_gz_len); });
    serverWeb.on("/js/functions.js", []()
                 { sendGzip(contTypeTextJs, functions_js_gz, functions_js_gz_len); });
    serverWeb.on("/js/jquery-min.js", []()
                 { sendGzip(contTypeTextJs, jquery_min_js_gz, jquery_min_js_gz_len); });
    serverWeb.on("/js/i18next.min.js", []()
                 { sendGzip(contTypeTextJs, i18next_min_js_gz, i18next_min_js_gz_len); });
    serverWeb.on("/js/commonmark.min.js", []()
                 { sendGzip(contTypeTextJs, commonmark_min_js_gz, commonmark_min_js_gz_len); });
    serverWeb.on("/css/required.css", []()
                 { sendGzip(contTypeTextJs, required_css_gz, required_css_gz_len); });
    serverWeb.on("/favicon.ico", []()
                 { sendGzip("img/png", favicon_ico_gz, favicon_ico_gz_len); });

    serverWeb.on("/icon_left_dash.svg", []()
                 { sendGzip("image/svg+xml", icon_left_dash_svg_gz, icon_left_dash_svg_gz_len); });

    serverWeb.on("/AVATTO.svg", []()
                 { sendGzip("image/svg+xml", AVATTO_svg_gz, AVATTO_svg_gz_len); });
    serverWeb.on("/AVATTO_B.svg", []()
                 { sendGzip("image/svg+xml", AVATTO_B_svg_gz, AVATTO_B_svg_gz_len); });
    serverWeb.on("/icon_left_general.svg", []()
                 { sendGzip("image/svg+xml", icon_left_general_svg_gz, icon_left_general_svg_gz_len); });
    serverWeb.on("/icon_left_ethernet.svg", []()
                 { sendGzip("image/svg+xml", icon_left_ethernet_svg_gz, icon_left_ethernet_svg_gz_len); });
    serverWeb.on("/icon_left_wifi.svg", []()
                 { sendGzip("image/svg+xml", icon_left_wifi_svg_gz, icon_left_wifi_svg_gz_len); });
    serverWeb.on("/lan_connection.svg", []()
                 { sendGzip("image/svg+xml", lan_connection_svg_gz, lan_connection_svg_gz_len); });
    serverWeb.on("/realTime_control.svg", []()
                 { sendGzip("image/svg+xml", realTime_control_svg_gz, realTime_control_svg_gz_len); });
    serverWeb.on("/usb_connection.svg", []()
                 { sendGzip("image/svg+xml", usb_connection_svg_gz, usb_connection_svg_gz_len); });
    serverWeb.on("/wifi_connection.svg", []()
                 { sendGzip("image/svg+xml", wifi_connection_svg_gz, wifi_connection_svg_gz_len); });
    serverWeb.on("/workMode_selection.svg", []()
                 { sendGzip("image/svg+xml", workMode_selection_svg_gz, workMode_selection_svg_gz_len); });

    serverWeb.on("/icon_operating_mode.svg", []()
                 { sendGzip("image/svg+xml", icon_operating_mode_svg_gz, icon_operating_mode_svg_gz_len); });
    serverWeb.on("/Temp.svg", []()
                 { sendGzip("image/svg+xml", Temp_svg_gz, Temp_svg_gz_len); });
    serverWeb.on("/Time.svg", []()
                 { sendGzip("image/svg+xml", Time_svg_gz, Time_svg_gz_len); });


    serverWeb.on("/icon_header_wifi.svg", []()
                 { sendGzip("image/svg+xml", icon_header_wifi_svg_gz, icon_header_wifi_svg_gz_len); });
    serverWeb.on("/icon_header_ethernet.svg", []()
                 { sendGzip("image/svg+xml", icon_header_ethernet_svg_gz, icon_header_ethernet_svg_gz_len); });
    serverWeb.on("/icon_header_vpn.svg", []()
                 { sendGzip("image/svg+xml", icon_header_vpn_svg_gz, icon_header_vpn_svg_gz_len); });
    serverWeb.on("/icon_header_hardware.svg", []()
                 { sendGzip("image/svg+xml", icon_header_hardware_svg_gz, icon_header_hardware_svg_gz_len); });


    serverWeb.on("/icon_brand_info.svg", []()
                 { sendGzip("image/svg+xml", icon_brand_info_svg_gz, icon_brand_info_svg_gz_len); });
    serverWeb.on("/icon_hardware_info.svg", []()
                 { sendGzip("image/svg+xml", icon_hardware_info_svg_gz, icon_hardware_info_svg_gz_len); });
    serverWeb.on("/icon_mqtt_info.svg", []()
                 { sendGzip("image/svg+xml", icon_mqtt_info_svg_gz, icon_mqtt_info_svg_gz_len); });
    serverWeb.on("/icon_docment_info.svg", []()
                 { sendGzip("image/svg+xml", icon_docment_info_svg_gz, icon_docment_info_svg_gz_len); });
    serverWeb.on("/icon_scan_info.svg", []()
                 { sendGzip("image/svg+xml", icon_scan_info_svg_gz, icon_scan_info_svg_gz_len); });
    serverWeb.on("/icon_zigbee_info.svg", []()
                 { sendGzip("image/svg+xml", icon_zigbee_info_svg_gz, icon_zigbee_info_svg_gz_len); });
    serverWeb.on("/icon_web_info.svg", []()
                 { sendGzip("image/svg+xml", icon_web_info_svg_gz, icon_web_info_svg_gz_len); });
    serverWeb.on("/icon_purchase_info.svg", []()
                 { sendGzip("image/svg+xml", icon_purchase_info_svg_gz, icon_purchase_info_svg_gz_len); });
    serverWeb.on("/icon_service_info.svg", []()
                 { sendGzip("image/svg+xml", icon_service_info_svg_gz, icon_service_info_svg_gz_len); });

    serverWeb.on("/icon_left_zigbee.svg", []()
                 { sendGzip("image/svg+xml", icon_left_zigbee_svg_gz, icon_left_zigbee_svg_gz_len); });
    serverWeb.on("/icon_left_security.svg", []()
                 { sendGzip("image/svg+xml", icon_left_security_svg_gz, icon_left_security_svg_gz_len); });
    serverWeb.on("/icon_left_mqtt.svg", []()
                 { sendGzip("image/svg+xml", icon_left_mqtt_svg_gz, icon_left_mqtt_svg_gz_len); });
    serverWeb.on("/icon_left_vpn.svg", []()
                 { sendGzip("image/svg+xml", icon_left_vpn_svg_gz, icon_left_vpn_svg_gz_len); });
    serverWeb.on("/icon_left_tools.svg", []()
                 { sendGzip("image/svg+xml", icon_left_tools_svg_gz, icon_left_tools_svg_gz_len); });
    serverWeb.on("/icon_left_about.svg", []()
                 { sendGzip("image/svg+xml", icon_left_about_svg_gz, icon_left_about_svg_gz_len); });
    serverWeb.on("/icon_wifi_config.svg", []()
                 { sendGzip("image/svg+xml", icon_wifi_config_svg_gz, icon_wifi_config_svg_gz_len); });
    serverWeb.on("/icon_ethernet_config.svg", []()
                 { sendGzip("image/svg+xml", icon_ethernet_config_svg_gz, icon_ethernet_config_svg_gz_len); });
    serverWeb.on("/icon_serial_settings.svg", []()
                 { sendGzip("image/svg+xml", icon_serial_settings_svg_gz, icon_serial_settings_svg_gz_len); });
    serverWeb.on("/icon_config_generator.svg", []()
                 { sendGzip("image/svg+xml", icon_config_generator_svg_gz, icon_config_generator_svg_gz_len); });
    serverWeb.on("/icon_mqtt_settings.svg", []()
                 { sendGzip("image/svg+xml", icon_mqtt_settings_svg_gz, icon_mqtt_settings_svg_gz_len); });
    serverWeb.on("/icon_mqtt_status.svg", []()
                 { sendGzip("image/svg+xml", icon_mqtt_status_svg_gz, icon_mqtt_status_svg_gz_len); });
    serverWeb.on("/icon_vpn_configuration.svg", []()
                 { sendGzip("image/svg+xml", icon_vpn_configuration_svg_gz, icon_vpn_configuration_svg_gz_len); });
    serverWeb.on("/icon_firmware_update.svg", []()
                 { sendGzip("image/svg+xml", icon_firmware_update_svg_gz, icon_firmware_update_svg_gz_len); });
    serverWeb.on("/icon_system_control.svg", []()
                 { sendGzip("image/svg+xml", icon_system_control_svg_gz, icon_system_control_svg_gz_len); });
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
                xTaskCreate(checkFwHexTask,
                            "CheckFWHex",
                            8192,
                            (void *)tempFile,
                            3,
                            NULL);
            }
        });

    const char *headerkeys[] = {"Content-Length"};
    size_t headerkeyssize = sizeof(headerkeys) / sizeof(char *);
    registerStaticRoutes();
    serverWeb.collectHeaders(headerkeys, headerkeyssize);
    serverWeb.begin();
    DEBUG_PRINTLN(F("webserver setup done"));
}
