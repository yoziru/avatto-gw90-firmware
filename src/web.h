#ifndef WEB_H_
#define WEB_H_

#include <Arduino.h>
#include <WebServer.h>
#include <FS.h>
#include <NetworkClient.h>
#include "encoding.h"

extern WebServer serverWeb;
extern NetworkClient eventsClient;

extern const char *contTypeTextHtml;
extern const char *contTypeTextJs;
extern const char *contTypeTextCss;
extern const char *contTypeJson;
extern const char *contTypeText;
extern const char *respHeaderName;
extern const char *respTimeZonesName;
extern const char *checked;
extern const char *tempFile;

extern int totalLength;
extern int currentLength;

enum API_PAGE_t : uint8_t
{
    API_PAGE_ROOT,
    API_PAGE_GENERAL,
    API_PAGE_ETHERNET,
    API_PAGE_WIFI,
    API_PAGE_ZHA_Z2M,
    API_PAGE_SECURITY,
    API_PAGE_SYSTOOLS,
    API_PAGE_ABOUT,
    API_PAGE_MQTT,
    API_PAGE_WG
};

void handleEvents();
void initWebServer();
void webServerHandleClient();
void sendEvent(const char *event, const uint8_t evsz, const String &data);
void sendGzip(const char *contentType, const uint8_t content[], uint16_t contentLen);
bool checkAuth();
void printLogTime();
void printLogMsg(const String &msg);

void handleApi();
void handleZigbeeBSL();
void handleZigbeeRestart();

void handleRoot();
void handleStatus();
void handleGeneral();
void handleSecurity();
void handleWifi();
void handleEther();
void handleSerial();
void handleMqtt();
void handleWg();
void handleSysTools();
void handleAbout();

void handleSaveParams();
void handleSavefile();
void wifiCommitPending();

void checkFwHexTask(void *param);
void progressFunc(unsigned int progress, unsigned int total);
void getEspUpdate(String esp_fw_url);
void runEspUpdateFirmware(uint8_t *data, size_t len);

#define UPD_FILE "https://github.com/yoziru/avatto-gw90-firmware/releases/latest/download/AVATTO-GW90-Ti.bin"

#endif
