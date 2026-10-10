#!/usr/bin/env python3
"""Execute recovery, STA configuration, and temperature ownership together."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
network = (ROOT / "src/network.cpp").read_text()
overseer = network[network.index("void handleTmrNetworkOverseer()"):network.index("void NetworkEvent(")]
sta = network[network.index("void connectWifiSta("):network.index("void mDNS_start()")]
temperature = (ROOT / "src/etc.cpp").read_text()
temperature = temperature[temperature.index("static float rawTempToCelsius("):temperature.index("void zigbeeRouterRejoin()")]
harness = r"""
#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "wifi_save_logic.h"
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define F(x) x
constexpr int WIFI_OFF = 0, WIFI_STA = 1, WIFI_MODE_STA = 1, WIFI_AP_STA = 3, WIFI_MODE_APSTA = 3;
constexpr int WL_CONNECTED = 3, ESP_OK = 0;
constexpr unsigned INADDR_NONE = 0xFFFFFFFF, overseerInterval = 5000, overseerMaxRetry = 4, wifiFallbackMaxRetry = 6;
enum class CoordinatorMode { NETWORK, PASSTHROUGH };
struct {
    CoordinatorMode coordinator_mode = CoordinatorMode::NETWORK;
    bool connectedEther = false, wifiFallbackActive = false, apStarted = false;
    const char *ssid = "A", *password = "";
} ConfigSettings;
struct { template<class T> void println(T) {} template<class T> void print(T) {} } Serial;
struct { unsigned counter() { return 20; } void stop() {} } tmrNetworkOverseer;
unsigned long now = 100000, wifiFallbackLastOnlineMs = 0;
unsigned connects = 0, apStarts = 0, radioStops = 0, relinquishes = 0;
std::vector<std::string> order;
unsigned long millis() { return now; }
void delay(unsigned ms) { now += ms; }
void printLogMsg(const char *) {}
void initLan() {}
void startServers(bool = false) {}
void startAP(bool start) { ConfigSettings.apStarted = start; if (start) ++apStarts; }
void connectWifi() { ++connects; }
bool networkExpected() { return true; }
uint16_t wifiLastDisconnectReason = 0;
struct IPAddress { unsigned value; IPAddress(unsigned n = 0) : value(n) {} };
IPAddress parse_ip_address(const char *) { return IPAddress(1); }
struct {
    bool connected = false, dhcp = true, configOk = true;
    int modeValue = WIFI_OFF;
    bool isConnected() { return connected; }
    int status() { return connected ? WL_CONNECTED : 0; }
    void disconnect() { connected = false; }
    void persistent(bool) {}
    void setAutoReconnect(bool) {}
    void setSleep(bool) {}
    int getMode() { return modeValue; }
    void mode(int mode) { modeValue = mode; }
    void scanNetworks(bool) {}
    void scanDelete() {}
    bool config(IPAddress ip, IPAddress, IPAddress) {
        dhcp = ip.value == INADDR_NONE; order.push_back("config"); return configOk;
    }
    void begin(const char *, const char *) { order.push_back("begin"); }
} WiFi;
int esp_wifi_start() { return ESP_OK; }
void esp_wifi_stop() { ++radioStops; }
uint8_t temprature_sens_read() { return 160; }
"""
checks = r"""
int main() {
    handleTmrNetworkOverseer(); assert(ConfigSettings.wifiFallbackActive && connects == 1); // saved open SSID
    WiFi.connected = true; handleTmrNetworkOverseer();
    now += 1000; WiFi.connected = false; handleTmrNetworkOverseer(); assert(!ConfigSettings.apStarted);
    now += 30000; handleTmrNetworkOverseer(); assert(ConfigSettings.apStarted && apStarts == 1);
    WiFi.connected = true; handleTmrNetworkOverseer(); assert(!ConfigSettings.apStarted);
    connectWifiSta("A", "", false, "1", "1", "1"); assert(!WiFi.dhcp);
    order.clear(); connectWifiSta("A", "", true, "", "", "");
    assert(WiFi.dhcp && order == std::vector<std::string>({"config", "begin"}));
    order.clear(); WiFi.configOk = false; connectWifiSta("A", "", true, "", "", "");
    assert(order == std::vector<std::string>({"config"})); WiFi.configOk = true;
    WiFi.mode(WIFI_OFF); getCPUtemp(); assert(tempPowered); // temperature temporarily owns radio
    connectWifiSta("A", "", true, "", "", ""); assert(!tempPowered);
    for (unsigned i = 0; i < 30; ++i) { now += 100; tempSensorLoop(); }
    assert(radioStops == 0 && WiFi.getMode() == WIFI_STA && tempValid); // join owns it at teardown
}
"""
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    (work / "test.cpp").write_text(harness + temperature + overseer + sta + checks)
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-fsanitize=address,undefined",
                    "-I" + str(ROOT / "src"), str(work / "test.cpp"), "-o", str(work / "check")], check=True)
    subprocess.run([str(work / "check")], check=True)
print("Network fallback/DHCP/radio ownership regressions passed (ASan/UBSan)")
