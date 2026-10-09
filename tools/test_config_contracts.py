#!/usr/bin/env python3
"""Run factory reset and MQTT reconnect against filesystem/broker boundaries."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
etc = (ROOT / 'src/etc.cpp').read_text()
mqtt = (ROOT / 'src/mqtt.cpp').read_text()
constants = '\n'.join(line for line in etc.splitlines() if line.startswith('const char *configFile'))
reset = etc[etc.index('void resetSettings()'):etc.index('void setClock()')]
reconnect = mqtt[mqtt.index('void mqttReconnect()'):mqtt.index('void mqttOnConnect()')]
harness = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>
using byte = uint8_t;
using boolean = bool;
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define FORMAT_LITTLEFS_IF_FAILED true
constexpr int LED_USB = 12, LED_PWR = 14;
struct String : std::string {
    using std::string::string;
    String(std::string s) : std::string(s) {}
    String(int n) : std::string(std::to_string(n)) {}
    String operator+(int n) const { return std::string(*this) + std::to_string(n); }
};
void digitalWrite(int, int) {}
int digitalRead(int) { return 0; }
void delay(unsigned) {}
unsigned millis() { return 100; }
void printLogMsg(const String &) {}
std::set<std::string> files = {"/config/configSerial.json", "/config/configSecurity.json",
    "/config/configGeneral.json", "/config/configEther.json", "/config/configWifi.json",
    "/config/configWg.json", "/config/configMqtt.json", "/unrelated.hex"};
struct {
    bool begin(bool, const char *, unsigned) { return true; }
    void remove(const char *path) { files.erase(path); }
} LittleFS;
bool rebooted = false, retainedOffline = false;
struct { void restart() { rebooted = true; } } ESP;
struct {
    const char *topic = "gateway", *user = "test", *pass = "secret";
    bool tls = false; unsigned interval = 60, reconnectTime = 0;
} MqttSettings;
void getDeviceID(char *out, size_t size) { assert(size >= 20); strcpy(out, "AVATTO-GW90-TI-0001"); }
void mqttOnConnect() {}
struct {
    bool connect(const char *, const char *, const char *, const char *topic, byte, bool retained, const char *message) {
        retainedOffline = retained && std::string(message) == "offline" && std::string(topic) == "gateway/avty";
        return true;
    }
    int state() { return 0; }
} clientPubSub;
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    (work / 'test.cpp').write_text(harness + constants + '\n' + reset + reconnect + r'''
int main() {
    resetSettings(); assert(rebooted && files == std::set<std::string>{"/unrelated.hex"});
    mqttReconnect(); assert(retainedOffline);
}
''')
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', str(work / 'test.cpp'), '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True)
print('Factory reset/MQTT availability contracts passed')
