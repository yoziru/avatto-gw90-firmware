#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <Ticker.h>
#include <ETH.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <WireGuard-ESP32.h>

#include "config.h"
#include "net.h"
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
  // No delay here - ETH.begin() starts the EMAC clock immediately

  if (ETH.begin(ETH_TYPE_1, ETH_ADDR_1, ETH_MDC_PIN_1, ETH_MDIO_PIN_1, ETH_POWER_PIN_ALTERNATIVE_1, ETH_CLK_MODE_1))
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
    // WireGuard-ESP32-Arduino (Tinkerforge fork) requires an explicit subnet,
    // local port and gateway. Match the legacy behavior: /32 host subnet,
    // port 51820, no gateway.
    if (!wg.begin(
            WgSettings.localIP,
            IPAddress(255, 255, 255, 255),
            51820,
            IPAddress(0, 0, 0, 0),
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
  if (ConfigSettings.serversStarted)
  {
    return;
  }
  ConfigSettings.serversStarted = true;
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
  case CoordinatorMode::NETWORK:
    if (ConfigSettings.connectedEther)
    {
      // Ethernet is the preferred uplink. If we failed over to WiFi STA,
      // tear it down again (less RF noise, ETH is back).
      if (ConfigSettings.wifiFallbackActive)
      {
        ConfigSettings.wifiFallbackActive = false;
        ConfigSettings.wifiFallbackHadConnection = false;
        Serial.println(F("[OVERSEER] ETH recovered, stopping WiFi fallback"));
        startAP(false);   // tear down AP + DNS if it was the last resort
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
      }
      startServers();
      tmrNetworkOverseer.stop();
    }
    else
    {
      if (tmrNetworkOverseer.counter() > overseerMaxRetry)
      {
        if (!ConfigSettings.wifiFallbackActive)
        {
          if ((strlen(ConfigSettings.ssid) >= 2) && (strlen(ConfigSettings.password) >= 8))
          {
            // Ethernet down: try WiFi STA as a secondary uplink before AP.
            ConfigSettings.wifiFallbackActive = true;
            Serial.println(F("[OVERSEER] ETH down, trying WiFi STA fallback"));
            connectWifi();
            // keep the overseer running so we notice when ETH comes back
          }
          else
          {
            // No WiFi credentials configured: no point attempting STA, go
            // straight to the last-resort AP config recovery.
            ConfigSettings.wifiFallbackActive = true;
            Serial.println(F("[OVERSEER] ETH down, no WiFi creds - starting AP"));
            startAP(true);
          }
        }
        else if (WiFi.isConnected())
        {
          ConfigSettings.wifiFallbackHadConnection = true;
          startServers();
          // keep the overseer running to catch ETH recovery
        }
        else if (!ConfigSettings.apStarted && !ConfigSettings.wifiFallbackHadConnection &&
                 tmrNetworkOverseer.counter() > overseerMaxRetry + wifiFallbackMaxRetry)
        {
          // WiFi STA never connected during this fallback: last-resort AP.
          Serial.println(F("[OVERSEER] WiFi fallback failed, starting AP"));
          startAP(true);
        }
        else if ((strlen(ConfigSettings.ssid) >= 2) && (strlen(ConfigSettings.password) >= 8))
        {
          // WiFi was up before but dropped, or still connecting: retry STA.
          connectWifi();
        }
        // else: AP is the active recovery channel - idle until ETH returns.
      }
    }
    break;
  case CoordinatorMode::PASSTHROUGH:
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

void NetworkEvent(arduino_event_id_t event)
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
    if (tmrNetworkOverseer.state() == STOPPED && ConfigSettings.coordinator_mode == CoordinatorMode::NETWORK)
    {
      Serial.println(F("[ETH_EVENT] Restarting overseer for LAN mode"));
      tmrNetworkOverseer.start();
    }
    break;
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
      WiFi.softAPdisconnect(true);    // off wifi
      dnsServer.stop();
      ConfigSettings.apStarted = false;
    }
  }
  else
  {
    if (!start)
      return;
    WiFi.mode(WIFI_AP_STA);
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
  if ((strlen(ConfigSettings.ssid) >= 2) && (strlen(ConfigSettings.password) >= 8))
  {
    DEBUG_PRINTLN(F("[connectWifi] Ok SSID & PASS"));
    connectWifiSta(ConfigSettings.ssid, ConfigSettings.password, ConfigSettings.dhcpWiFi,
                   ConfigSettings.ipAddressWiFi, ConfigSettings.ipMaskWiFi, ConfigSettings.ipGWWiFi);
  }
  else
  {
    if (!(ConfigSettings.coordinator_mode == CoordinatorMode::PASSTHROUGH && ConfigSettings.keepWeb))
    { // dont start ap in keepWeb
      DEBUG_PRINTLN(F("[connectWifi] NO SSID & PASS"));
      startAP(true);
      DEBUG_PRINTLN(F("[connectWifi] setupWifiAP"));
    }
  }
}

// Shared STA bring-up used by the overseer (connectWifi) and by the wifi save
// handler when applying/validating new credentials. Keeps the AP alive if it
// is running, applies a static IP when DHCP is off.
void connectWifiSta(const char *ssid, const char *pass, bool dhcp, const char *ip, const char *mask, const char *gw)
{
  WiFi.persistent(false);
  if (ConfigSettings.apStarted)
  {
    WiFi.mode(WIFI_AP_STA); // keep the AP alive while joining as STA
  }
  else
  {
    WiFi.mode(WIFI_STA);
  }
  delay(100);

  WiFi.begin(ssid, pass);
  WiFi.setSleep(false);
  DEBUG_PRINTLN(F("[connectWifiSta] WiFi.begin"));

  if (!dhcp)
  {
    IPAddress ip_address = parse_ip_address(ip);
    IPAddress gateway_address = parse_ip_address(gw);
    IPAddress netmask = parse_ip_address(mask);
    WiFi.config(ip_address, gateway_address, netmask);
    DEBUG_PRINTLN(F("[connectWifiSta] WiFi.config"));
  }
  else
  {
    DEBUG_PRINTLN(F("[connectWifiSta] Try DHCP"));
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
