#include <Arduino.h>



void handleEvents();

// Web server init: static resources, page routing, API, file ops, firmware update
void initWebServer();                   

/*
// Page dynamic rendering

Purpose: Provides JSON data needed by the frontend (via HTTP Header)
Workflow:
    1. Frontend requests /api?action=0&page=X
    2. Backend calls corresponding handleXxx() function
    3. Generates JSON data → returned via respValuesArr Header
    4. Frontend fills HTML template with JS
*/
// Root page data
void handleRoot();
// Status page
void handleStatus();
// General page
void handleGeneral();
// Security page
void handleSecurity();
// WiFi page
void handleWifi();
// Ethernet config page
void handleEther();
// MQTT config page
void handleMqtt();
// WG config page
void handleWg();
// System tools page
void handleSysTools();
// About page
void handleAbout();

// API handler: API commands listed below
/*
enum CMD_t {
    CMD_ZB_ROUTER_RECON, // Router reconnect
    CMD_ZB_RST,          // Zigbee restart
    CMD_ZB_BSL,          // Enter Bootloader mode
    CMD_ESP_RES,         // ESP32 restart
    CMD_ADAP_LAN,        // Switch to LAN mode
    CMD_ADAP_USB,        // Switch to USB mode
    CMD_LED_PWR_TOG,     // Power LED toggle
    CMD_LED_USB_TOG,     // USB/Wi-Fi LED toggle
    CMD_CLEAR_LOG,       // Clear log
    CMD_ESP_UPD_URL,     // ESP32 online update
    CMD_ZB_CHK_REV,      // Check Zigbee version
    CMD_ZB_CHK_CON,      // Check Zigbee connection
    CMD_ZB_LED_TOG       // Zigbee LED toggle
};
*/
void handleApi();

// Save form data
/*
Purpose: Process form submission, write config to LittleFS
Workflow:
    1. Frontend POST /saveParams + form data
    2. Determine config type by pageId
    3. Read existing config → update fields → write back to file
    4. Call loadConfigXxx() to apply config
*/
void handleSaveParams();

// Event stream notification
/*
Purpose: Real-time progress push via Server-Sent Events (SSE)
Use cases:
    1. Firmware update progress (ZB_FW_downloading)
    2. Error notification (ZB_FW_err)
    3. ESP32 update progress (ESP_FW_prgs)
*/
void sendEvent(const char *event, const uint8_t evsz, const String data);

// Handle all client requests
void webServerHandleClient();
// Zigbee enter BSL mode
void handleZigbeeBSL();
// Zigbee restart
void handleZigbeeRestart();
// Zigbee serial config
void handleSerial();
// File upload
void handleSavefile();

// Static resource page loading
void sendGzip(const char* contentType, const uint8_t content[], uint16_t contentLen);

// Print log timestamp
void printLogTime();
void printLogMsg(String msg);

// Verify user permissions
bool checkAuth();

// Update progress handler
void progressFunc(unsigned int progress, unsigned int total);
// ESP32 update
void getEspUpdate(String esp_fw_url);
// Start ESP32 update
void runEspUpdateFirmware(uint8_t *data, size_t len);

#define UPD_FILE "https://github.com/AVATTO-smart/avatto-gw90-firmware/releases/latest/download/AVATTO-GW90-Ti.bin"