#ifndef NET_H_
#define NET_H_

#include <WiFi.h>
#include <DNSServer.h>
#include <Ticker.h>
#include <WireGuard-ESP32.h>

extern volatile uint8_t btnFlag;
extern Ticker tmrNetworkOverseer;
extern Ticker tmrZbHealth;
extern IPAddress apIP;
extern DNSServer dnsServer;
extern WiFiServer server;
extern WireGuard wg;
extern volatile uint16_t wifiLastDisconnectReason;

void initLan();
void startSocketServer();
void wgBegin();
void startServers(bool usb = false);
void handleTmrNetworkOverseer();
void NetworkEvent(arduino_event_t *event);
void startAP(const bool start);
void connectWifi();
void connectWifiSta(const char *ssid, const char *pass, bool dhcp, const char *ip, const char *mask, const char *gw);
void mDNS_start();

#endif
