#!/usr/bin/env python3
"""Run the actual flash routine against deterministic transport replies."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/zb.cpp").read_text()
# Compile the owning function unchanged; fake only hardware/file boundaries.
routine = source[source.index("int8_t runFlash("):source.index("int8_t checkFwHex(")]
ota = (ROOT / "src/web/ota.cpp").read_text()
task = ota[ota.index("bool startZbFlashTask()"):ota.index("void progressFunc(")]
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    harness = r"""
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using byte = uint8_t;
using std::min;
#define HEX 16
#define F(x) x
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
struct String : std::string {
    using std::string::string;
    String(std::string s) : std::string(s) {}
    String(unsigned n) : std::string(std::to_string(n)) {}
    String(int n) : std::string(std::to_string(n)) {}
    void trim() {}
    String substring(size_t a, size_t b) const { return substr(a, b - a); }
    char charAt(size_t n) const { return at(n); }
    size_t length() const { return size(); }
};
std::string firmware;
const char *tempFile = "test.hex";
struct File {
    size_t pos = 0;
    operator bool() const { return true; }
    bool available() { return pos < firmware.size(); }
    String readStringUntil(char delimiter) {
        size_t end = firmware.find(delimiter, pos);
        if (end == std::string::npos) end = firmware.size();
        String line(firmware.substr(pos, end - pos)); pos = end + 1; return line;
    }
    void close() {}
};
struct FS { File open(const char *, const char *) { return {}; } } LittleFS;
struct SerialStub { template<class... T> void printf(const char *, T...) {} void println(const char *) {} void updateBaudRate(unsigned) {} } Serial, Serial2;
struct { unsigned serialSpeed = 115200; std::atomic<bool> zbFlashing{false}; } ConfigSettings;
struct { void restart() {} } CCTool;
bool bslActive = true, zbRoleLiveThisBoot = true;
bool startOk = true, dataOk = true;
unsigned statusCalls = 0, failStatusAt = 0, dataCalls = 0, completeEvents = 0;
std::vector<byte> sent;
void printLogMsg(const String &) {}
void sendEvent(const char *tag, size_t, const String &value) {
    if (std::string(tag) == "ZB_FW_flashing" && value == "100") ++completeEvents;
}
bool zbFlashStart(uint32_t, uint32_t) { return startOk; }
bool zbCheckLastCmd() { return ++statusCalls < failStatusAt || failStatusAt == 0; }
bool zbSendCommand(uint8_t, byte *data, uint8_t size, uint8_t) {
    ++dataCalls;
    if (!dataOk) return false;
    sent.insert(sent.end(), data, data + size); return true;
}
void checkFwHexTask(void *);
bool createOk = true;
void (*queuedTask)(void *) = nullptr;
void *taskArg = nullptr;
constexpr int pdPASS = 1;
unsigned refreshes = 0, deletes = 0;
int validationResult = 0;
int xTaskCreatePinnedToCore(void (*callback)(void *), const char *, unsigned, void *arg, unsigned, void *, int) {
    if (!createOk) return 0;
    queuedTask = callback; taskArg = arg; return pdPASS;
}
void esp_task_wdt_reset() {}
int checkFwHex(const char *path) { assert(ConfigSettings.zbFlashing && path == tempFile); return validationResult; }
void zbRequestInfoRefresh() { ++refreshes; }
void vTaskDelete(void *) { ++deletes; }
"""
    checks = r"""
int main() {
    auto reset = [] {
        startOk = dataOk = true; statusCalls = failStatusAt = dataCalls = completeEvents = 0;
        sent.clear(); bslActive = zbRoleLiveThisBoot = true;
        firmware = ":0400000001020304F2\n:0400100005060708D2\n:00000001FF\n";
    };
    reset(); startOk = false;
    assert(runFlash("test.hex", 20) == -1 && dataCalls == 0 && completeEvents == 0);
    reset(); dataOk = false;
    assert(runFlash("test.hex", 20) == -1 && completeEvents == 0);
    reset(); failStatusAt = 2; // data ACK succeeds, final command status fails
    assert(runFlash("test.hex", 20) == -1 && completeEvents == 0);
    reset(); failStatusAt = 3; // final verification fails after all bytes were ACKed
    assert(runFlash("test.hex", 20) == -1 && completeEvents == 0);
    reset();
    assert(runFlash("test.hex", 20) == 0 && completeEvents == 1);
    assert(!bslActive && !zbRoleLiveThisBoot && sent.size() == 20);
    assert(sent[0] == 1 && sent[3] == 4 && sent[16] == 5 && sent[19] == 8);
    for (size_t i = 4; i < 16; ++i) assert(sent[i] == 0xFF);
    reset();
    assert(runFlash("test.hex", 21) == -1 && completeEvents == 0); // never fake total sent
    ConfigSettings.zbFlashing = true; createOk = false;
    assert(!startZbFlashTask() && !ConfigSettings.zbFlashing && !queuedTask);
    createOk = true;
    for (int result : {0, -1}) {
        ConfigSettings.zbFlashing = true; validationResult = result;
        const unsigned before = refreshes;
        assert(startZbFlashTask() && ConfigSettings.zbFlashing && queuedTask);
        queuedTask(taskArg);
        assert(!ConfigSettings.zbFlashing && refreshes == before + (result == 0));
    }
    assert(deletes == 2);
}
"""
    (work / "test.cpp").write_text(harness + routine + task + checks)
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-fsanitize=address,undefined",
                    str(work / "test.cpp"), "-o", str(work / "check")], check=True)
    subprocess.run([str(work / "check")], check=True)
print("Zigbee flash failure/progress regressions passed (ASan/UBSan)")
