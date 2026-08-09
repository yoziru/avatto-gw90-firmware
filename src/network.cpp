#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <Ticker.h>
#include <esp_wifi.h>
#include <ETH.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <WireGuard-ESP32.h>

#include "config.h"
#include "network.h"
#include "web.h"
#include "log.h"
#include "etc.h"
#include "zb.h"

IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
WiFiServer server(TCP_LISTEN_PORT, MAX_SOCKET_CLIENTS);

WireGuard wg;

Ticker tmrNetworkOverseer(handleTmrNetworkOverseer, overseerInterval, 0, MILLIS);
Ticker tmrZbHealth(zbHealthCheck, 30000, 0, MILLIS);

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

void startServers(bool usb)
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
  tmrZbHealth.start();
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
