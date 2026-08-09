#include <WiFi.h>
// #include <WiFiClient.h>
// #include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <Update.h>
#include <Ticker.h>
#include <esp_wifi.h>
#include <ETH.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <CCTools.h>
#include <WireGuard-ESP32.h>

#include "config.h"
#include "web.h"
#include "log.h"
#include "etc.h"
#include "mqtt.h"
#include "zb.h"
#include "version.h"



#include <freertos/FreeRTOS.h>
#include <freertos/task.h>




#ifdef ETH_CLK_MODE
#undef ETH_CLK_MODE
#endif

#define BUFFER_SIZE 256

// volatile bool btnFlag = false;
volatile uint8_t btnFlag = 0;
bool updWeb = false;

void mDNS_start();
void connectWifi();
void handleLongBtn();
void handleTmrNetworkOverseer();
void setupCoordinatorMode();
void startAP(const bool start);

Ticker tmrBtnLongPress(handleLongBtn, 1000, 0, MILLIS);
Ticker tmrNetworkOverseer(handleTmrNetworkOverseer, overseerInterval, 0, MILLIS);

IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
WiFiServer server(TCP_LISTEN_PORT, MAX_SOCKET_CLIENTS);

static WireGuard wg;

extern CCTools CCTool;
// MDNSResponder MDNS; don't need?


const byte ZIGBEE_LED_ON[] = {0xFE, 0x02, 0x27, 0x0A, 0x01, 0x01, 0x2F};
const byte ZIGBEE_LED_OFF[] = {0xFE, 0x02, 0x27, 0x0A, 0x01, 0x00, 0x2E};
const byte cmdSysVersion[] = {0Xfe, 0, 0X21, 0X02, 0x23};

#ifdef BUILD_FACTORY_TEST
void factory_test(void);
#endif


void initLan()
{
  Serial.println(F("\n[LAN] ======== initLan() START ========"));
  Serial.print(F("[LAN] ETH_ADDR=")); Serial.print(ETH_ADDR_1);
  Serial.print(F(" PWR_PIN=")); Serial.print(ETH_POWER_PIN_1);
  Serial.print(F(" MDC=")); Serial.print(ETH_MDC_PIN_1);
  Serial.print(F(" MDIO=")); Serial.print(ETH_MDIO_PIN_1);
  Serial.print(F(" PWR_ALT=")); Serial.println(ETH_POWER_PIN_ALTERNATIVE_1);
  Serial.print(F("[LAN] DHCP=")); Serial.println(ConfigSettings.dhcp);
  if (!ConfigSettings.dhcp) {
    Serial.print(F("[LAN] Static IP=")); Serial.println(ConfigSettings.ipAddress);
    Serial.print(F("[LAN] Static GW=")); Serial.println(ConfigSettings.ipGW);
    Serial.print(F("[LAN] Static Mask=")); Serial.println(ConfigSettings.ipMask);
  }

  // Hardware reset LAN8720 PHY via GPIO5 to ensure clean state after ESP.restart().
  // CRITICAL: PHY reset must be released immediately before ETH.begin() so the
  // 50MHz EMAC clock is already running when the PHY comes out of reset.
  // A clockless PHY (current code had 300ms gap) enters an undefined state.
  pinMode(ETH_POWER_PIN_ALTERNATIVE_1, OUTPUT);
  digitalWrite(ETH_POWER_PIN_ALTERNATIVE_1, LOW);
  delay(200);
  digitalWrite(ETH_POWER_PIN_ALTERNATIVE_1, HIGH);
  // No delay here — ETH.begin() starts the EMAC clock immediately

  if (ETH.begin(ETH_ADDR_1, ETH_POWER_PIN_1, ETH_MDC_PIN_1, ETH_MDIO_PIN_1, ETH_TYPE_1, ETH_CLK_MODE_1, ETH_POWER_PIN_ALTERNATIVE_1))
  {
    Serial.println(F("[LAN] ETH.begin() SUCCESS"));
    if (!ConfigSettings.dhcp)
    {
      Serial.println(F("[LAN] Configuring static IP..."));
      ETH.config(parse_ip_address(ConfigSettings.ipAddress), parse_ip_address(ConfigSettings.ipGW), parse_ip_address(ConfigSettings.ipMask));
      Serial.println(F("[LAN] Static IP configured"));
    }
    else
    {
      Serial.println(F("[LAN] Using DHCP"));
    }
  }
  else
  {
    Serial.println(F("[LAN] ETH.begin() FAILED!"));
  }
  Serial.println(F("[LAN] ======== initLan() END ========\n"));
}

void startSocketServer()
{
  server.begin(ConfigSettings.socketPort);
  server.setNoDelay(true);
}

void wgBegin()
{
  if (!wg.is_initialized())
  {
    //printLogMsg(String("Initializing WireGuard interface..."));
    if (!wg.begin(
            WgSettings.localIP,
            WgSettings.localPrivKey,
            WgSettings.endAddr,
            WgSettings.endPubKey,
            WgSettings.endPort))
    {
      printLogMsg(String("Failed to initialize WG"));
      WgSettings.init = false;
    }
    else
    {
      printLogMsg(String("WG was initialized"));
      WgSettings.init = true;
    }
  }
}

void startServers(bool usb = false)
{
  Serial.print(F("[SRV] startServers() called, usb=")); Serial.println(usb);
  Serial.print(F("[SRV] connectedEther=")); Serial.println(ConfigSettings.connectedEther);
  Serial.print(F("[SRV] WiFi.isConnected=")); Serial.println(WiFi.isConnected());
  Serial.print(F("[SRV] apStarted=")); Serial.println(ConfigSettings.apStarted);
  initWebServer();
  if (!usb)
    startSocketServer();
  startAP(false);
  mDNS_start();
  getZbVer();
  if (WgSettings.enable)
  {
    wgBegin();
  }
}

void handleTmrNetworkOverseer()
{
  switch (ConfigSettings.coordinator_mode)
  {
  case CoordinatorMode::WIFI:
    DEBUG_PRINT(F("WiFi.status = "));
    DEBUG_PRINTLN(WiFi.status());
    if (WiFi.isConnected())
    {
      DEBUG_PRINTLN(F("WIFI CONNECTED"));
      startServers();
      tmrNetworkOverseer.stop();
    }
    else
    {
      if (tmrNetworkOverseer.counter() > overseerMaxRetry)
      {
        DEBUG_PRINTLN(F("WIFI counter overflow"));
        startAP(true);
        connectWifi();
      }
    }
    break;
  case CoordinatorMode::LAN:
    Serial.print(F("[OVERSEER] LAN check: connectedEther="));
    Serial.print(ConfigSettings.connectedEther);
    Serial.print(F(" counter="));
    Serial.println(tmrNetworkOverseer.counter());
    if (ConfigSettings.connectedEther)
    {
      DEBUG_PRINTLN(F("LAN CONNECTED"));
      Serial.println(F("[OVERSEER] LAN connected! Starting servers..."));
      startServers();
      tmrNetworkOverseer.stop();
    }
    else
    {
      if (tmrNetworkOverseer.counter() > overseerMaxRetry)
      {
        DEBUG_PRINTLN(F("LAN counter overflow"));
        Serial.print(F("[OVERSEER] LAN timeout! counter="));
        Serial.print(tmrNetworkOverseer.counter());
        Serial.print(F(" > maxRetry="));
        Serial.println(overseerMaxRetry);
        Serial.println(F("[OVERSEER] Starting AP as fallback..."));
        startAP(true);
      }
    }
    break;
  case CoordinatorMode::USB:
    if (tmrNetworkOverseer.counter() > 3)
    { // 10 seconds for wifi connect
      if (WiFi.isConnected())
      {
        tmrNetworkOverseer.stop();
        startServers(true);
      }
      else
      {
        initLan();
        if (tmrNetworkOverseer.counter() > 6)
        { // 3sec for lan
          if (ConfigSettings.connectedEther)
          {
            tmrNetworkOverseer.stop();
            startServers(true);
          }
          else
          {                            // no network interfaces
            tmrNetworkOverseer.stop(); // stop timer
            startAP(true);
          }
        }
      }
    }
    break;

  default:
    break;
  }
}

void NetworkEvent(WiFiEvent_t event)
{
  Serial.print(F("[NET_EVENT] Event ID: "));
  Serial.println(event);
  switch (event)
  {
  case ARDUINO_EVENT_ETH_START:
    Serial.println(F("[ETH_EVENT] ETH Started"));
    Serial.print(F("[ETH_EVENT] Setting hostname: ")); Serial.println(ConfigSettings.hostname);
    ETH.setHostname(ConfigSettings.hostname);
    break;
  case ARDUINO_EVENT_ETH_CONNECTED:
    Serial.println(F("[ETH_EVENT] ETH Connected (link up)"));
    break;
  case ARDUINO_EVENT_ETH_GOT_IP:
    Serial.println(F("[ETH_EVENT] ======== ETH GOT IP ========"));
    Serial.print(F("[ETH_EVENT] MAC: ")); Serial.println(ETH.macAddress());
    Serial.print(F("[ETH_EVENT] IPv4: ")); Serial.println(ETH.localIP());
    Serial.print(F("[ETH_EVENT] Subnet: ")); Serial.println(ETH.subnetMask());
    Serial.print(F("[ETH_EVENT] Gateway: ")); Serial.println(ETH.gatewayIP());
    if (ETH.fullDuplex())
    {
      Serial.println(F("[ETH_EVENT] FULL_DUPLEX"));
    }
    Serial.print(F("[ETH_EVENT] Speed: ")); Serial.print(ETH.linkSpeed()); Serial.println(F("Mbps"));
    ConfigSettings.connectedEther = true;
    Serial.println(F("[ETH_EVENT] connectedEther = true"));
    Serial.println(F("[ETH_EVENT] ========================"));
    setClock();
    break;
  case ARDUINO_EVENT_ETH_DISCONNECTED: // 21:  //SYSTEM_EVENT_ETH_DISCONNECTED:
    Serial.println(F("[ETH_EVENT] ETH Disconnected"));
    Serial.print(F("[ETH_EVENT] coordinator_mode=")); Serial.println(static_cast<uint8_t>(ConfigSettings.coordinator_mode));
    ConfigSettings.connectedEther = false;
    if (tmrNetworkOverseer.state() == STOPPED && ConfigSettings.coordinator_mode == CoordinatorMode::LAN)
    {
      Serial.println(F("[ETH_EVENT] Restarting overseer for LAN mode"));
      tmrNetworkOverseer.start();
    }
    break;
  case SYSTEM_EVENT_ETH_STOP:
  case ARDUINO_EVENT_ETH_STOP:
    Serial.println(F("[ETH_EVENT] ETH Stopped"));
    ConfigSettings.connectedEther = false;
    if (tmrNetworkOverseer.state() == STOPPED)
    {
      Serial.println(F("[ETH_EVENT] Restarting overseer after ETH stop"));
      tmrNetworkOverseer.start();
    }
    break;
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    DEBUG_PRINTLN(F("WiFi"));
    DEBUG_PRINT(F("IPv4: "));
    DEBUG_PRINT(WiFi.localIP().toString());
    DEBUG_PRINT(F(", "));
    DEBUG_PRINT(WiFi.subnetMask().toString());
    DEBUG_PRINT(F(", "));
    DEBUG_PRINTLN(WiFi.gatewayIP().toString());
    setClock();
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: // SYSTEM_EVENT_STA_DISCONNECTED:
    DEBUG_PRINTLN(F("WIFI STA DISCONNECTED"));
    if (tmrNetworkOverseer.state() == STOPPED)
    {
      tmrNetworkOverseer.start();
    }
    break;
  default:
    break;
  }
}

// Start or stop AP
void startAP(const bool start)
{
  Serial.print("[startAP] Called with start=");
  Serial.println(start);
  if (ConfigSettings.apStarted)
  {
    Serial.println("[startAP] AP already started");
    if (!start)
    {
      if (ConfigSettings.coordinator_mode != CoordinatorMode::WIFI)
      {
        WiFi.softAPdisconnect(true);    // off wifi
      }
      else
      {
        WiFi.mode(WIFI_STA);
      }
      dnsServer.stop();
      ConfigSettings.apStarted = false;
    }
  }
  else
  {
    if (!start)
      return;
    WiFi.mode(WIFI_AP_STA); // WIFI_AP_STA for possible wifi scan in wifi mode
    WiFi.disconnect();
    // String AP_NameString;
    // getDeviceID(AP_NameString);

    // char AP_NameChar[AP_NameString.length() + 1];
    // memset(AP_NameChar, 0, AP_NameString.length() + 1);

    // for (int i = 0; i < AP_NameString.length(); i++){
    //   AP_NameChar[i] = AP_NameString.charAt(i);
    // }

    Serial.println("[startAP] Starting AP mode...");
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    char apSsid[32];  // Increased to 32 to safely hold SSID + null terminator
    getDeviceID(apSsid);
    Serial.print("[startAP] SSID: ");
    Serial.println(apSsid);
    bool apResult = WiFi.softAP(apSsid); //, WIFIPASS);
    Serial.print("[startAP] softAP result: ");
    Serial.println(apResult ? "SUCCESS" : "FAILED");
    if (apResult) {
      Serial.print("[startAP] AP IP: ");
      Serial.println(WiFi.softAPIP());
    }
    // if DNSServer is started with "*" for domain name, it will reply with
    // provided IP to all DNS request
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", apIP);
    WiFi.setSleep(false);
    // ConfigSettings.wifiAPenblTime = millis();
    startServers();
    ConfigSettings.apStarted = true;
  }
}

void connectWifi()
{
  static uint8_t timeout = 0;
  if (WiFi.status() == WL_IDLE_STATUS && timeout < 20)
  { // connection in progress
    DEBUG_PRINTLN(F("[connectWifi] WL_IDLE_STATUS"));
    timeout++;
    return;
  }
  else
  {
    timeout = 0;
    DEBUG_PRINTLN(F("[connectWifi] timeout"));
  }
  WiFi.persistent(false);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B);
  if ((strlen(ConfigSettings.ssid) >= 2) && (strlen(ConfigSettings.password) >= 8))
  {
    DEBUG_PRINTLN(F("[connectWifi] Ok SSID & PASS"));
    if (ConfigSettings.apStarted)
    {
      // DEBUG_PRINTLN(F("[connectWifi] WiFi.mode(WIFI_AP_STA)"));
      // WiFi.mode(WIFI_AP_STA);
    }
    else
    {
      DEBUG_PRINTLN(F("[connectWifi] WiFi.mode(WIFI_STA)"));
      WiFi.mode(WIFI_STA);
    }
    delay(100);

    WiFi.begin(ConfigSettings.ssid, ConfigSettings.password);
    WiFi.setSleep(false);
    DEBUG_PRINTLN(F("[connectWifi] WiFi.begin"));

    if (!ConfigSettings.dhcpWiFi)
    {
      IPAddress ip_address = parse_ip_address(ConfigSettings.ipAddressWiFi);
      IPAddress gateway_address = parse_ip_address(ConfigSettings.ipGWWiFi);
      IPAddress netmask = parse_ip_address(ConfigSettings.ipMaskWiFi);
      WiFi.config(ip_address, gateway_address, netmask);
      DEBUG_PRINTLN(F("[connectWifi] WiFi.config"));
    }
    else
    {
      DEBUG_PRINTLN(F("[connectWifi] Try DHCP"));
    }
  }
  else
  {
    if (!(ConfigSettings.coordinator_mode == CoordinatorMode::USB && ConfigSettings.keepWeb))
    { // dont start ap in keepWeb
      DEBUG_PRINTLN(F("[connectWifi] NO SSID & PASS"));
      startAP(true);
      DEBUG_PRINTLN(F("[connectWifi] setupWifiAP"));
    }
  }
}

void mDNS_start()
{
  const char *host = "_avatto-gw90-ti";
  const char *http = "_http";
  const char *tcp = "_tcp";
  if (!MDNS.begin(ConfigSettings.hostname))
  {
    printLogMsg("Error setting up MDNS responder!");
  }
  else
  {
    printLogMsg("mDNS responder started");
    MDNS.addService(http, tcp, 80); // web
    //--zeroconf zha--
    MDNS.addService(host, tcp, ConfigSettings.socketPort);
    MDNS.addServiceTxt(host, tcp, "version", "1.0");
    MDNS.addServiceTxt(host, tcp, "radio_type", "znp");
    MDNS.addServiceTxt(host, tcp, "baud_rate", String(ConfigSettings.serialSpeed));
    MDNS.addServiceTxt(host, tcp, "data_flow_control", "software");
  }
}

IRAM_ATTR bool debounce()
{
  volatile static unsigned long lastFire = 0;
  if (millis() - lastFire < DEBOUNCE_TIME)
  { // Debounce
    return 0;
  }
  lastFire = millis();
  return 1;
}

IRAM_ATTR void btnInterrupt()
{
  if (debounce())
    btnFlag = true;
}

void setLedsDisable(bool mode, bool setup)
{
  Serial.println("\n========== LED CONTROL ==========");
  Serial.print("[LED] Mode: ");
  Serial.println(mode ? "DISABLE ALL" : "ENABLE");
  Serial.print("[LED] Setup phase: ");
  Serial.println(setup ? "Yes" : "No");
  
  if (!setup)
  {
    Serial.println("[LED] Saving LED preferences to config...");
    const char *path = configFileGeneral;
    JsonDocument doc;
    File configFile = LittleFS.open(path, FILE_READ);
    deserializeJson(doc, configFile);
    configFile.close();
    doc["disableLeds"] = mode;
    doc["disableLedPwr"] = mode;
    doc["disableLedUSB"] = mode;
    configFile = LittleFS.open(path, FILE_WRITE);
    serializeJson(doc, configFile);
    configFile.close();
    ConfigSettings.disableLeds = mode;
    ConfigSettings.disableLedPwr = mode;
    ConfigSettings.disableLedUSB = mode;
  }
  
  if (mode)
  {
    Serial.println("[LED] Turning OFF all LEDs");
    digitalWrite(LED_USB, LOW);
    digitalWrite(LED_PWR, LOW);
    Serial.println("[LED] LED_USB (Red): OFF");
    Serial.println("[LED] LED_PWR (Blue): OFF");
  }
  else
  {
    // Blue Power LED
    if (!ConfigSettings.disableLedPwr)
    {
      digitalWrite(LED_PWR, HIGH);
      Serial.println("[LED] LED_PWR (Blue): ON");
    }
    else
    {
      digitalWrite(LED_PWR, LOW);
      Serial.println("[LED] LED_PWR (Blue): OFF (disabled in config)");
    }
    
    // Red USB LED - only on in USB mode
    if (ConfigSettings.coordinator_mode == CoordinatorMode::USB && !ConfigSettings.disableLedUSB)
    {
      digitalWrite(LED_USB, HIGH);
      Serial.println("[LED] LED_USB (Red): ON (USB mode)");
    }
    else
    {
      digitalWrite(LED_USB, LOW);
      if (ConfigSettings.coordinator_mode == CoordinatorMode::USB) {
        Serial.println("[LED] LED_USB (Red): OFF (disabled in config)");
      } else {
        const char* modeNames[] = {"LAN", "WiFi", "USB"};
        Serial.print("[LED] LED_USB (Red): OFF (not in USB mode, current: ");
        Serial.print(modeNames[static_cast<size_t>(ConfigSettings.coordinator_mode)]);
        Serial.println(")");
      }
    }
  }
  Serial.println("================================\n");
}

void handleLongBtn()
{
  if (!digitalRead(BTN))
  { // long press
    DEBUG_PRINT(F("Long press "));
    DEBUG_PRINT(btnFlag);
    DEBUG_PRINTLN(F("s"));
    if (btnFlag >= 4)
    {
      printLogMsg("Long press 4sec - zigbeeEnableBSL");
      zigbeeEnableBSL();
      tmrBtnLongPress.stop();
      btnFlag = false;
    }
    else
      btnFlag++;
  }
  else
  { // stop long press
    if (btnFlag >= 2)
    {
      printLogMsg("Long press 2sec - setLedsDisable");
      setLedsDisable(!ConfigSettings.disableLeds, false);
    }
    tmrBtnLongPress.stop();
    btnFlag = false;
    printLogMsg("Stop long press");
  }
}

void toggleUsbMode()
{
  Serial.println("\n========== MODE TOGGLE TRIGGERED ==========");
  
  const char* modeNames[] = {"LAN", "WiFi", "USB"};
  Serial.print("[MODE] Current Mode: ");
  Serial.print(modeNames[static_cast<size_t>(ConfigSettings.coordinator_mode)]);
  Serial.print(" (");
  Serial.print(static_cast<uint8_t>(ConfigSettings.coordinator_mode));
  Serial.println(")");
  
  if (ConfigSettings.coordinator_mode != CoordinatorMode::USB)
  {
    Serial.print("[MODE] Switching from ");
    Serial.print(modeNames[static_cast<size_t>(ConfigSettings.coordinator_mode)]);
    Serial.println(" to USB mode");
    
    ConfigSettings.prevCoordinator_mode = ConfigSettings.coordinator_mode; // remember current state
    ConfigSettings.coordinator_mode = CoordinatorMode::USB;                // toggle
    
    Serial.print("[MODE] Previous mode saved: ");
    Serial.println(modeNames[static_cast<size_t>(ConfigSettings.prevCoordinator_mode)]);
  }
  else
  {
    Serial.print("[MODE] Switching from USB to ");
    Serial.print(modeNames[static_cast<size_t>(ConfigSettings.prevCoordinator_mode)]);
    Serial.println(" mode");
    
    ConfigSettings.coordinator_mode = ConfigSettings.prevCoordinator_mode;
  }
  
  Serial.print("[MODE] New Mode: ");
  Serial.print(modeNames[static_cast<size_t>(ConfigSettings.coordinator_mode)]);
  Serial.print(" (");
  Serial.print(static_cast<uint8_t>(ConfigSettings.coordinator_mode));
  Serial.println(")");
  
  // Save configuration
  const char *path = configFileGeneral;
  JsonDocument doc;
  File configFile = LittleFS.open(path, FILE_READ);
  deserializeJson(doc, configFile);
  configFile.close();
  doc[prevCoordMode] = static_cast<uint8_t>(ConfigSettings.prevCoordinator_mode);
  doc[coordMode] = static_cast<uint8_t>(ConfigSettings.coordinator_mode);
  configFile = LittleFS.open(path, FILE_WRITE);
  serializeJson(doc, configFile);
  configFile.close();
  
  Serial.println("[MODE] Configuration saved");
  Serial.println("[MODE] Restarting device...");
  Serial.println("==========================================\n");
  
  digitalWrite(LED_USB, ConfigSettings.coordinator_mode == CoordinatorMode::USB ? 1 : 0);
  ESP.restart();
}

void setupCoordinatorMode()
{
  Serial.println(F("\n======== setupCoordinatorMode() ========"));
  if (ConfigSettings.coordinator_mode > CoordinatorMode::USB || ConfigSettings.coordinator_mode < CoordinatorMode::LAN)
  {
    Serial.println(F("[MODE] WRONG MODE DETECTED, set to LAN"));
    ConfigSettings.coordinator_mode = CoordinatorMode::LAN;
  }
  const char* modeNames[] = {"LAN", "WiFi", "USB"};
  Serial.print(F("[MODE] Mode: ")); Serial.print(modeNames[static_cast<size_t>(ConfigSettings.coordinator_mode)]);
  Serial.print(F(" (")); Serial.print(static_cast<uint8_t>(ConfigSettings.coordinator_mode)); Serial.println(F(")"));
  Serial.print(F("[MODE] keepWeb=")); Serial.println(ConfigSettings.keepWeb);
  Serial.print(F("[MODE] disableWeb=")); Serial.println(ConfigSettings.disableWeb);
  if (ConfigSettings.coordinator_mode != CoordinatorMode::USB || ConfigSettings.keepWeb)
  { // start network overseer
    Serial.println(F("[MODE] Registering WiFi event & starting overseer timer"));
    if (tmrNetworkOverseer.state() == STOPPED)
    {
      tmrNetworkOverseer.start();
      Serial.println(F("[MODE] tmrNetworkOverseer started"));
    }
    WiFi.onEvent(NetworkEvent);
    Serial.println(F("[MODE] NetworkEvent registered"));
  }
  else
  {
    Serial.println(F("[MODE] USB mode without keepWeb - no network overseer"));
  }
  switch (ConfigSettings.coordinator_mode)
  {
  case CoordinatorMode::USB:
    Serial.println(F("[MODE] -> USB: MODE_SWITCH=HIGH"));
    digitalWrite(MODE_SWITCH, 1);
    break;

  case CoordinatorMode::WIFI:
    Serial.println(F("[MODE] -> WIFI: calling connectWifi()"));
    connectWifi();
    break;

  case CoordinatorMode::LAN:
    Serial.println(F("[MODE] -> LAN: calling initLan()"));
    initLan();
    break;

  default:
    break;
  }
  if (!ConfigSettings.disableWeb && (ConfigSettings.coordinator_mode != CoordinatorMode::USB || ConfigSettings.keepWeb))
  {
    updWeb = true;
    Serial.println(F("[MODE] Web server enabled (updWeb=true)"));
  }
  if (ConfigSettings.coordinator_mode == CoordinatorMode::USB && ConfigSettings.keepWeb)
  {
    Serial.println(F("[MODE] USB+keepWeb: trying connectWifi()"));
    connectWifi();
  }
  Serial.println(F("======== setupCoordinatorMode() END ========\n"));
}

void setup()
{
  // PHY warm-restart workaround: ESP.restart() doesn't drop the 3.3V rail (esp. PoE),
  // so LAN8720 POR never fires. A 350ms delay at startup lets strapping pins settle.
  delay(350);

  Serial.begin(115200); // todo ifdef DEBUG
  delay(500);
  Serial.println("\n\n========== DEVICE STARTING ==========");
  Serial.println("Firmware version: " VERSION);
  Serial.println("\n[DEBUG] Serial Monitor Ready");
  Serial.println("[DEBUG] Baud Rate: 115200");
  Serial.println("[DEBUG] Use Serial Monitor to view mode switching and system logs");
  Serial.println("[DEBUG] ==========================================\n");

  ConfigSettings.apStarted = false;
  ConfigSettings.serialSpeed = 115200;
  DEBUG_PRINTLN(F("Start"));
  pinMode(CC2652P_RST, OUTPUT);
  pinMode(CC2652P_FLASH, OUTPUT);
  digitalWrite(CC2652P_RST, 1);
  digitalWrite(CC2652P_FLASH, 1);
  pinMode(LED_PWR, OUTPUT);
  pinMode(LED_USB, OUTPUT);
  pinMode(BTN, INPUT);
  pinMode(MODE_SWITCH, OUTPUT);
  digitalWrite(MODE_SWITCH, 0); // enable zigbee serial
  digitalWrite(LED_PWR, 1);
  digitalWrite(LED_USB, 1);

// hard reset
#if !defined(BUILD_DEBUG) && !defined(BUILD_FACTORY_TEST)
  if (!digitalRead(BTN))
  {
    DEBUG_PRINTLN(F("[hard reset] Entering hard reset mode"));
    uint8_t counter = 0;
    while (!digitalRead(BTN))
    {
      if (counter >= 10)
      {
        resetSettings();
      }
      else
      {
        counter++;
        DEBUG_PRINTLN(counter);
        delay(200);
      }
    }
    DEBUG_PRINTLN(F("[hard reset] Btn up, exit"));
  }
#endif // !BUILD_DEBUG && !BUILD_FACTORY_TEST
  //--------------------

  // zig connection & leds testing
  Serial2.begin(115200, SERIAL_8N1, CC2652P_RXD, CC2652P_TXD); // start zigbee serial

  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED, "/lfs2", 10))
  {
    DEBUG_PRINTLN(F("Error with LITTLEFS"));
    return;
  }

  DEBUG_PRINTLN(F("LITTLEFS OK"));
  if (!loadSystemVar())
  {
    DEBUG_PRINTLN(F("Error load system vars"));
    const char *path = "/config";

    if (LittleFS.mkdir(path))
    {
      DEBUG_PRINTLN(F("Config dir created"));
      delay(500);
      ESP.restart();
    }
    else
    {
      DEBUG_PRINTLN(F("mkdir failed"));
    }
  }
  else
  {
    DEBUG_PRINTLN(F("System vars load OK"));
  }

  if (!loadConfigSerial())
  {
    DEBUG_PRINTLN(F("Error load config serial"));
    ESP.restart();
  }
  else
  {
    DEBUG_PRINTLN(F("Config serial load OK"));
  }

  if ((!loadConfigWifi()) || (!loadConfigEther()) || (!loadConfigGeneral()) || (!loadConfigSecurity()) || (!loadConfigMqtt()) || (!loadConfigWg()))
  {
    DEBUG_PRINTLN(F("Error load config files"));
    ESP.restart();
  }
  else
  {
    DEBUG_PRINTLN(F("Config files load OK"));
  }

  zbInit();
#ifdef BUILD_FACTORY_TEST
  if (!digitalRead(BTN) && ConfigSettings.coordinator_mode != CoordinatorMode::USB)
  {
    factory_test();
  }
#endif
  //-----------------
  attachInterrupt(digitalPinToInterrupt(BTN), btnInterrupt, FALLING);

  Serial.println("\n[INIT] Configuring LED initial state...");
  Serial.print("[INIT] disableLeds config: ");
  Serial.println(ConfigSettings.disableLeds ? "true" : "false");
  setLedsDisable(ConfigSettings.disableLeds, true);
  setupCoordinatorMode();
  ConfigSettings.connectedClients = 0;

  if (MqttSettings.enable)
  {
    mqttConnectSetup();
  }

  DEBUG_PRINTLN(millis());

  Serial2.updateBaudRate(ConfigSettings.serialSpeed); // set actual speed

  printf("[INIT] Serial speed: %d\n",ConfigSettings.serialSpeed);

  printLogMsg("Setup done");

  char deviceIdArr[20];
  getDeviceID(deviceIdArr);

  DEBUG_PRINTLN(String(deviceIdArr));
  printLogMsg(String(deviceIdArr));

}

WiFiClient client[MAX_SOCKET_CLIENTS];

void socketClientConnected(int client)
{
  if (ConfigSettings.connectedSocket[client] != true)
  {
    DEBUG_PRINT(F("Connected client "));
    DEBUG_PRINTLN(client);
    if (ConfigSettings.connectedClients == 0)
    {
      ConfigSettings.socketTime = millis();
      DEBUG_PRINT(F("Socket time "));
      DEBUG_PRINTLN(ConfigSettings.socketTime);
      mqttPublishIo("socket", "ON");
    }
    ConfigSettings.connectedSocket[client] = true;
    ConfigSettings.connectedClients++;
  }
}

void socketClientDisconnected(int client)
{
  if (ConfigSettings.connectedSocket[client] != false)
  {
    DEBUG_PRINT(F("Disconnected client "));
    DEBUG_PRINTLN(client);
    ConfigSettings.connectedSocket[client] = false;
    ConfigSettings.connectedClients--;
    if (ConfigSettings.connectedClients == 0)
    {
      ConfigSettings.socketTime = millis();
      DEBUG_PRINT(F("Socket time "));
      DEBUG_PRINTLN(ConfigSettings.socketTime);
      mqttPublishIo("socket", "OFF");
    }
  }
}

void printRecvSocket(size_t bytes_read, uint8_t net_buf[BUFFER_SIZE])
{
  char output_sprintf[3];
  if (bytes_read > 0)
  {
    String tmpTime;
    String buff = "";
    unsigned long timeLog = millis();
    tmpTime = String(timeLog, DEC);
    logPush('[');
    for (int j = 0; j < tmpTime.length(); j++)
    {
      logPush(tmpTime[j]);
    }
    logPush(']');
    logPush(' ');
    logPush('-');
    logPush('>');

    for (int i = 0; i < bytes_read; i++)
    {
      sprintf(output_sprintf, "%02x", net_buf[i]);
      logPush(' ');
      logPush(output_sprintf[0]);
      logPush(output_sprintf[1]);
    }
    logPush('\n');
  }
}

void printSendSocket(size_t bytes_read, uint8_t serial_buf[BUFFER_SIZE])
{
  char output_sprintf[3];
  String tmpTime;
  String buff = "";
  unsigned long timeLog = millis();
  tmpTime = String(timeLog, DEC);
  logPush('[');
  for (int j = 0; j < tmpTime.length(); j++)
  {
    logPush(tmpTime[j]);
  }
  logPush(']');
  logPush(' ');
  logPush('<');
  logPush('-');
  for (int i = 0; i < bytes_read; i++)
  {
    // if (serial_buf[i] == 0x01)
    //{
    // }
    sprintf(output_sprintf, "%02x", serial_buf[i]);
    logPush(' ');
    logPush(output_sprintf[0]);
    logPush(output_sprintf[1]);
    // if (serial_buf[i] == 0x03)
    // {

    //}
  }
  logPush('\n');
}

void loop(void)
{
  if (btnFlag)
  {
    if (!digitalRead(BTN))
    { // pressed
      if (tmrBtnLongPress.state() == STOPPED)
      {
        tmrBtnLongPress.start();
      }
    }
    else
    {
      if (tmrBtnLongPress.state() == RUNNING)
      {
        btnFlag = false;
        tmrBtnLongPress.stop();
        toggleUsbMode();
      }
    }
  }

  tmrBtnLongPress.update();
  tmrNetworkOverseer.update();
  if (updWeb)
  {
    webServerHandleClient();
  }
  else
  {
    if (ConfigSettings.connectedClients == 0)
    {
      webServerHandleClient();
    }
  }

  if (ConfigSettings.coordinator_mode != CoordinatorMode::USB)
  {
    uint16_t net_bytes_read = 0;
    uint8_t net_buf[BUFFER_SIZE];
    uint16_t serial_bytes_read = 0;
    uint8_t serial_buf[BUFFER_SIZE];

    if (server.hasClient())
    {
      for (byte i = 0; i < MAX_SOCKET_CLIENTS; i++)
      {
        if (!client[i] || !client[i].connected())
        {
          if (client[i])
          {
            client[i].stop();
          }
          if (ConfigSettings.fwEnabled)
          {
            WiFiClient TempClient2 = server.available();
            if (TempClient2.remoteIP() == ConfigSettings.fwIp)
            {
              printLogMsg(String("[SOCK IP WHITELIST] Accepted connection from IP: ") + TempClient2.remoteIP().toString());
              client[i] = TempClient2;
              continue;
            }
            else
            {
              printLogMsg(String("[SOCK IP WHITELIST] Rejected connection from unknown IP: ") + TempClient2.remoteIP().toString());
            }
          }
          else
          {
            client[i] = server.available();
            continue;
          }
        }
      }
      WiFiClient TempClient = server.available();
      TempClient.stop();
    }

    for (byte cln = 0; cln < MAX_SOCKET_CLIENTS; cln++)
    {
      if (client[cln])
      {
        socketClientConnected(cln);
        while (client[cln].available())
        { // read from LAN
          net_buf[net_bytes_read] = client[cln].read();
          if (net_bytes_read < BUFFER_SIZE - 1)
            net_bytes_read++;
        } // send to Zigbee
        Serial2.write(net_buf, net_bytes_read);
        // print to web console
        printRecvSocket(net_bytes_read, net_buf);
        net_bytes_read = 0;
      }
      else
      {
        socketClientDisconnected(cln);
      }
    }

    if (Serial2.available())
    {
      while (Serial2.available())
      { // read from Zigbee
        serial_buf[serial_bytes_read] = Serial2.read();
        if (serial_bytes_read < BUFFER_SIZE - 1)
          serial_bytes_read++;
      }
      // send to LAN
      for (byte cln = 0; cln < MAX_SOCKET_CLIENTS; cln++)
      {
        if (client[cln])
          client[cln].write(serial_buf, serial_bytes_read);
      }
      // print to web console
      printSendSocket(serial_bytes_read, serial_buf);
      serial_bytes_read = 0;
    }

    if (MqttSettings.enable)
    {
      mqttLoop();
    }
  }

  if (WiFi.getMode() == WIFI_MODE_AP || WiFi.getMode() == WIFI_MODE_APSTA)
  {
    dnsServer.processNextRequest();
  }
}






/***************************************************************** */
#ifdef BUILD_FACTORY_TEST

#ifndef FACTORY_TEST_SSID
#define FACTORY_TEST_SSID "ezsmart_factory_test"
#endif
#ifndef FACTORY_TEST_PASS
#define FACTORY_TEST_PASS "ezsmart@2026"
#endif
#ifndef FACTORY_TEST_HOST
#define FACTORY_TEST_HOST "www.baidu.com"
#endif

TaskHandle_t xTestTaskHandle = NULL;

void factory_initLan()
{
  Serial.println(F("\n[LAN] ======== initLan() START ========"));
  Serial.print(F("[LAN] ETH_ADDR=")); Serial.print(ETH_ADDR_1);
  Serial.print(F(" PWR_PIN=")); Serial.print(ETH_POWER_PIN_1);
  Serial.print(F(" MDC=")); Serial.print(ETH_MDC_PIN_1);
  Serial.print(F(" MDIO=")); Serial.print(ETH_MDIO_PIN_1);
  Serial.print(F(" PWR_ALT=")); Serial.println(ETH_POWER_PIN_ALTERNATIVE_1);
  Serial.print(F("[LAN] DHCP=")); Serial.println(ConfigSettings.dhcp);

  // Hardware reset LAN8720 PHY via GPIO5 (same fix as initLan())
  pinMode(ETH_POWER_PIN_ALTERNATIVE_1, OUTPUT);
  digitalWrite(ETH_POWER_PIN_ALTERNATIVE_1, LOW);
  delay(200);
  digitalWrite(ETH_POWER_PIN_ALTERNATIVE_1, HIGH);
  // No delay — EMAC clock starts inside ETH.begin()

  if (ETH.begin(ETH_ADDR_1, ETH_POWER_PIN_1, ETH_MDC_PIN_1, ETH_MDIO_PIN_1, ETH_TYPE_1, ETH_CLK_MODE_1, ETH_POWER_PIN_ALTERNATIVE_1))
  {
    Serial.println(F("[LAN] ETH.begin() SUCCESS"));
    Serial.println(F("[LAN] Using DHCP"));
  }
  else
  {
    Serial.println(F("[LAN] ETH.begin() FAILED!"));
  }
  Serial.println(F("[LAN] ======== initLan() END ========\n"));
}


void testTaskFunction(void *pvParameters)
{
  Serial.println("[TEST TASK] Test task started!");

  // ========== Connecting to WiFi AP ==========
  Serial.println("\n[TEST TASK] ========== Connecting to WiFi AP ==========");
  
  const char* testSSID = FACTORY_TEST_SSID;
  const char* testPASS = FACTORY_TEST_PASS;
  // Turn off indicator LEDs
  digitalWrite(LED_PWR, 0);
  digitalWrite(LED_USB, 1);

  // Set WiFi mode to STA
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  Serial.print("[WIFI CONNECT] Connecting to SSID: ");
  Serial.println(testSSID);
  
  WiFi.begin(testSSID, testPASS);
  
  // Wait for connection, try up to 20 times
  uint8_t connectAttempts = 0;
  bool wifiConnected = false;
  
  while (connectAttempts < 20)
  {
    delay(500);
    connectAttempts++;
    Serial.print("[WIFI CONNECT] Attempt ");
    Serial.print(connectAttempts);
    Serial.print(" - Status: ");
    
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("CONNECTED");
      Serial.print("[WIFI CONNECT] Local IP: ");
      Serial.println(WiFi.localIP());
      
      // Get real-time RSSI after connection
      int32_t currentRSSI = WiFi.RSSI();
      Serial.print("[WIFI CONNECT] RSSI: ");
      Serial.print(currentRSSI);
      Serial.println(" dBm");
      
      // Assess connection quality based on signal strength
      if (currentRSSI >= -50)
      {
        Serial.println("[TEST TASK] Signal: EXCELLENT");
      }
      else if (currentRSSI >= -60)
      {
        Serial.println("[TEST TASK] Signal: GOOD");
      }
      else if (currentRSSI >= -70)
      {
        Serial.println("[TEST TASK] Signal: FAIR");
      }
      else
      {
        Serial.println("[TEST TASK] Signal: WEAK");
      }
      
      wifiConnected = true;
      break;
    }
    else
    {
      Serial.print("Connecting... (");
      Serial.print(WiFi.status());
      Serial.println(")");
    }
  }
  
  if (!wifiConnected)
  {
    Serial.println("[WIFI CONNECT] Failed to connect to AP!");
    Serial.print("[WIFI CONNECT] Final Status: ");
    Serial.println(WiFi.status());
  }
  
  // ========== WiFi network test ==========
  Serial.println("\n[TEST TASK] ========== WIFI NETWORK TEST START ==========");
  
  uint32_t wifiTestCounter = 0;
  uint32_t wifiSuccessCount = 0;
  uint32_t wifiFailCount = 0;
  
  // Run 10 WiFi tests (~50 seconds)
  while (wifiTestCounter < 3)
  {
    wifiTestCounter++;
    
    Serial.println("\n========================================");
    Serial.println("         WIFI TEST                      ");
    Serial.println("========================================");
    Serial.print("[WIFI TEST] Test Count: ");
    Serial.println(wifiTestCounter);
    
    bool wifiTestConnected = false;
    
    // 1. Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("[WIFI TEST] WiFi Status: CONNECTED");
      Serial.print("[WIFI TEST] SSID: ");
      Serial.println(WiFi.SSID());
      Serial.print("[WIFI TEST] Local IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("[WIFI TEST] Subnet Mask: ");
      Serial.println(WiFi.subnetMask());
      Serial.print("[WIFI TEST] Gateway: ");
      Serial.println(WiFi.gatewayIP());
      Serial.print("[WIFI TEST] DNS Server: ");
      Serial.println(WiFi.dnsIP());
      
      // Get real-time RSSI value
      int32_t rssi = WiFi.RSSI();
      Serial.print("[WIFI TEST] Signal Strength (RSSI): ");
      Serial.print(rssi);
      Serial.println(" dBm");
      
      wifiTestConnected = true;
      wifiSuccessCount++;
    }
    else
    {
      Serial.print("[WIFI TEST] WiFi Status: DISCONNECTED (");
      Serial.print(WiFi.status());
      Serial.println(")");
      
      // Attempt to reconnect
      Serial.println("[WIFI TEST] Attempting to reconnect...");
      WiFi.reconnect();
      delay(2000);
      
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.println("[WIFI TEST] Reconnect SUCCESS");
        Serial.print("[WIFI TEST] RSSI: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        wifiTestConnected = true;
        wifiSuccessCount++;
      }
      else
      {
        Serial.println("[WIFI TEST] Reconnect FAILED");
        wifiFailCount++;
      }
    }
    
    // 2. WiFi network connectivity test
    if (wifiTestConnected)
    {
      Serial.println("[WIFI TEST] Testing WiFi network connectivity...");
      
      WiFiClient wifiTestClient;
      const char* testHost = FACTORY_TEST_HOST;
      const int testPort = 80;
      
      unsigned long startTime = millis();
      int connectResult = wifiTestClient.connect(testHost, testPort);
      unsigned long connectTime = millis() - startTime;
      
      if (connectResult)
      {
        Serial.print("[WIFI TEST] DNS Resolution: SUCCESS (");
        Serial.print(wifiTestClient.remoteIP());
        Serial.print(") | Connect Time: ");
        Serial.print(connectTime);
        Serial.println(" ms");
        Serial.print("[WIFI TEST] Used Interface: WiFi (");
        Serial.print(WiFi.localIP());
        Serial.println(")");
        wifiTestClient.stop();
      }
      else
      {
        Serial.println("[WIFI TEST] DNS Resolution: FAILED");
        Serial.println("[WIFI TEST] Possible causes:");
        Serial.println("[WIFI TEST]   - DNS server not configured");
        Serial.println("[WIFI TEST]   - Gateway unreachable");
        Serial.println("[WIFI TEST]   - No Internet access");
      }
    }
    
    // Statistics
    Serial.println("\n[WIFI TEST] ========== Statistics ==========");
    Serial.print("[WIFI TEST]   Total Checks: ");
    Serial.println(wifiTestCounter);
    Serial.print("[WIFI TEST]   Success Count: ");
    Serial.println(wifiSuccessCount);
    Serial.print("[WIFI TEST]   Fail Count: ");
    Serial.println(wifiFailCount);
    
    if (wifiTestCounter > 0)
    {
      float wifiRate = (float)wifiSuccessCount / wifiTestCounter * 100.0f;
      Serial.print("[WIFI TEST]   Success Rate: ");
      Serial.print(wifiRate);
      Serial.println("%");
    }
    
    Serial.println("========================================\n");
  }

  // ========== LAN network test ==========
  Serial.println("\n[TEST TASK] ========== LAN NETWORK TEST START ==========");
  
  uint32_t lanTestCounter = 0;
  uint32_t lanSuccessCount = 0;
  uint32_t lanFailCount = 0;
  
  factory_initLan();
  delay(1000);
  // Run 10 LAN tests (~50 seconds)
  while (lanTestCounter < 3)
  {
    lanTestCounter++;
    
    Serial.println("\n========================================");
    Serial.println("         LAN TEST                       ");
    Serial.println("========================================");
    Serial.print("[LAN TEST] Test Count: ");
    Serial.println(lanTestCounter);
    
    bool lanConnected = false;
    
    // 1. Check physical connection
     if (ETH.linkUp())
    {
        Serial.println("[LAN TEST] Physical Link: UP");
        Serial.print("[LAN TEST] Link Speed: ");
        Serial.print(ETH.linkSpeed());
        Serial.println(" Mbps");
        
        // Check IP configuration
        if (ETH.localIP() != IPAddress(0, 0, 0, 0))
        {
            Serial.println("[LAN TEST] ETH IP Status: VALID");
            Serial.print("[LAN TEST] Local IP: ");
            Serial.println(ETH.localIP());
            Serial.print("[LAN TEST] Subnet Mask: ");
            Serial.println(ETH.subnetMask());
            Serial.print("[LAN TEST] Gateway: ");
            Serial.println(ETH.gatewayIP());
            lanConnected = true;
            lanSuccessCount++;
        }
        else
        {
            Serial.println("[LAN TEST] ETH IP Status: INVALID");
            lanFailCount++;
        }
    }
    else
    {
        Serial.println("[LAN TEST] Physical Link: DOWN");
        Serial.println("[LAN TEST] Check Ethernet cable connection!");
        lanFailCount++;
    }
    
    // 3. LAN network connectivity test
    if (lanConnected)
    {
      Serial.println("[LAN TEST] Testing LAN network connectivity...");
      
      WiFiClient lanTestClient;
      const char* testHost = FACTORY_TEST_HOST;
      const int testPort = 80;
      
      unsigned long startTime = millis();
      int connectResult = lanTestClient.connect(testHost, testPort);
      unsigned long connectTime = millis() - startTime;
      
      if (connectResult)
      {
        Serial.print("[LAN TEST] DNS Resolution: SUCCESS (");
        Serial.print(lanTestClient.remoteIP());
        Serial.print(") | Connect Time: ");
        Serial.print(connectTime);
        Serial.println(" ms");
        Serial.print("[LAN TEST] Used Interface: ETH (");
        Serial.print(ETH.localIP());
        Serial.println(")");
        lanTestClient.stop();
      }
      else
      {
        Serial.println("[LAN TEST] DNS Resolution: FAILED");
        Serial.println("[LAN TEST] Possible causes:");
        Serial.println("[LAN TEST]   - DNS server not configured");
        Serial.println("[LAN TEST]   - Gateway unreachable");
        Serial.println("[LAN TEST]   - No Internet access");
      }
    }
    
    // Statistics
    Serial.println("\n[LAN TEST] ========== Statistics ==========");
    Serial.print("[LAN TEST]   Total Checks: ");
    Serial.println(lanTestCounter);
    Serial.print("[LAN TEST]   Success Count: ");
    Serial.println(lanSuccessCount);
    Serial.print("[LAN TEST]   Fail Count: ");
    Serial.println(lanFailCount);
    
    if (lanTestCounter > 0)
    {
      float lanRate = (float)lanSuccessCount / lanTestCounter * 100.0f;
      Serial.print("[LAN TEST]   Success Rate: ");
      Serial.print(lanRate);
      Serial.println("%");
    }
    
    Serial.println("========================================\n");
  }
  
  // ========== Final summary report ==========
  Serial.println("\n########################################################");
  Serial.println("#              FACTORY TEST FINAL REPORT              #");
  Serial.println("########################################################");
  
  Serial.println("\n[WIFI SUMMARY]");
  Serial.print("  Total Tests: ");
  Serial.println(wifiTestCounter);
  Serial.print("  Success: ");
  Serial.println(wifiSuccessCount);
  Serial.print("  Failed: ");
  Serial.println(wifiFailCount);
  if (wifiTestCounter > 0)
  {
    float wifiRate = (float)wifiSuccessCount / wifiTestCounter * 100.0f;
    Serial.print("  Success Rate: ");
    Serial.print(wifiRate);
    Serial.println("%");
  }
  
  Serial.println("\n[LAN SUMMARY]");
  Serial.print("  Total Tests: ");
  Serial.println(lanTestCounter);
  Serial.print("  Success: ");
  Serial.println(lanSuccessCount);
  Serial.print("  Failed: ");
  Serial.println(lanFailCount);
  if (lanTestCounter > 0)
  {
    float lanRate = (float)lanSuccessCount / lanTestCounter * 100.0f;
    Serial.print("  Success Rate: ");
    Serial.print(lanRate);
    Serial.println("%");
  }
  
  Serial.println("\n########################################################");
  Serial.println("#                  TEST COMPLETED                       #");
  Serial.println("########################################################\n");
  

  // Turn on ZIGBEE indicator
  Serial2.write(ZIGBEE_LED_ON, sizeof(ZIGBEE_LED_ON));      // D11 on
  // Turn on WiFi indicator
  if(wifiSuccessCount)digitalWrite(LED_PWR, 1);             // D9 on
  // Turn on LAN indicator
  if(lanSuccessCount)digitalWrite(LED_USB, 0);              // D12 on 

  // Enter idle loop after test completes
  while (1)
  {
    if(!digitalRead(BTN))
    {
      digitalWrite(LED_USB, 1);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      digitalWrite(LED_USB, 0);
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

void factory_test(void)
{
  // Scan WiFi networks
  Serial.println("[FACTORY] Scanning WiFi networks...");

  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();  
  if (n < 0)
  {
      Serial.println("[FACTORY] WiFi scan FAILED!");
      Serial.print("[FACTORY] Error code: ");
      Serial.println(n);
      return;
  }
  else if (n == 0)
  {
      Serial.println("[FACTORY] No AP found");
      WiFi.scanDelete();
      return;
  }
    
  Serial.print("[FACTORY] Total APs found: ");
  Serial.println(n);

  bool foundTestAP = false;
  int32_t testAP_RSSI = -100;
    
    // Manually filter SSID
  for (int i = 0; i < n; i++)
  {
      String ssid = WiFi.SSID(i);
      int32_t rssi = WiFi.RSSI(i);
      
      Serial.print("[FACTORY] SSID[");
      Serial.print(i);
      Serial.print("]: ");
      Serial.print(ssid);
      Serial.print(" | RSSI: ");
      Serial.print(rssi);
      Serial.println(" dBm");
      
      if (ssid == FACTORY_TEST_SSID)
      {
          foundTestAP = true;
          if (rssi > testAP_RSSI)
          {
              testAP_RSSI = rssi;  // Pick the one with best signal
          }
          Serial.println("[FACTORY] >>> Found target AP!");
      }
  }

  // Free scan result memory
  WiFi.scanDelete();

  if (foundTestAP)
  {
    // Pass RSSI value as parameter to task (via pointer cast)
    xTaskCreate(
        testTaskFunction,
        "TestTask",
        4096,
        (void *)(intptr_t)testAP_RSSI,  // Pass RSSI value
        5,
        &xTestTaskHandle
    );
    Serial.println("[FACTORY] Test task created successfully");
    Serial.println("[FACTORY] Entering factory test mode...");
    
    while(1)
    {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
  else
  {
    Serial.println("[FACTORY] 'ez_factory_test' AP not found, continuing normal startup");
  }
}

#endif // BUILD_FACTORY_TEST












