#ifndef NETWORK_H_
#define NETWORK_H_

#include <WiFi.h>
#include <DNSServer.h>
#include <Ticker.h>
#include <WireGuard-ESP32.h>

extern volatile uint8_t btnFlag;
extern Ticker tmrNetworkOverseer;
extern IPAddress apIP;
extern DNSServer dnsServer;
extern WiFiServer server;
extern WireGuard wg;

void initLan();
void startSocketServer();
void wgBegin();
void startServers(bool usb = false);
void handleTmrNetworkOverseer();
void NetworkEvent(WiFiEvent_t event);
void startAP(const bool start);
void connectWifi();
void mDNS_start();

#endif
