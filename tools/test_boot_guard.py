#!/usr/bin/env python3
"""Execute the real boot guard with fake NVS and boot-partition operations."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = "\n".join(line for line in (ROOT / "src/boot_guard.cpp").read_text().splitlines()
                   if not line.startswith("#include"))
harness = r"""
#include <cassert>
#include <cstdint>
#include <string>
#include <map>
#include "boot_guard_logic.h"
struct String : std::string {
    using std::string::string;
    String(std::string s) : std::string(s) {}
    String(int n) : std::string(std::to_string(n)) {}
};
std::map<std::string, unsigned> nvs;
struct Preferences {
    bool begin(const char *, bool) { return true; }
    void end() {}
    bool getBool(const char *k, bool fallback) { return nvs.count(k) ? nvs[k] : fallback; }
    uint8_t getUChar(const char *k, uint8_t fallback) { return nvs.count(k) ? nvs[k] : fallback; }
    void putBool(const char *k, bool v) { nvs[k] = v; }
    void putUChar(const char *k, uint8_t v) { nvs[k] = v; }
};
unsigned long millis() { return 1; }
void delay(unsigned) {}
void printLogMsg(const String &) {}
struct Reboot {};
struct { void restart() { throw Reboot{}; } } ESP;
struct esp_partition_t { const char *label; } running{"app1"}, fallback{"app0"};
const esp_partition_t *esp_ota_get_running_partition() { return &running; }
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *) { return &fallback; }
int bootResult = 0, switched = 0;
constexpr int ESP_OK = 0, WL_CONNECTED = 3;
int esp_ota_set_boot_partition(const esp_partition_t *partition) {
    assert(partition == &fallback); ++switched; return bootResult;
}
enum class CoordinatorMode { NETWORK, PASSTHROUGH };
struct { bool connectedEther = true, apStarted = false, zbFlashing = false; CoordinatorMode coordinator_mode = CoordinatorMode::NETWORK; } ConfigSettings;
struct { int status() { return WL_CONNECTED; } } WiFi;
bool networkExpected() { return true; }
unsigned zbGetLastClientTxMs() { return 0; }
unsigned zbGetLastRxMs() { return 0; }
"""
checks = r"""
int main() {
    bootGuardArm();
    for (unsigned i = 1; i <= 3; ++i) { bootGuardBegin(); assert(nvs["tries"] == i && switched == 0); }
    bool rebooted = false;
    try { bootGuardBegin(); } catch (Reboot &) { rebooted = true; }
    assert(rebooted && switched == 1 && !nvs["armed"]); // never call loop: setup-only crashes
    bootGuardArm(); nvs["tries"] = 255; rebooted = false;
    try { bootGuardBegin(); } catch (Reboot &) { rebooted = true; }
    assert(rebooted && switched == 2); // exhausted count cannot wrap to zero
    bootGuardArm(); nvs["tries"] = 3; bootResult = -1;
    bootGuardBegin();
    assert(switched == 3 && nvs["rb_failed"] && !nvs["armed"]); // invalid fallback never reboots
    bootGuardBegin(); assert(switched == 3); // normal/disarmed boot is untouched
}
"""
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    (work / "test.cpp").write_text(harness + source + checks)
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-I" + str(ROOT / "src"),
                    str(work / "test.cpp"), "-o", str(work / "check")], check=True)
    subprocess.run([str(work / "check")], check=True)
print("Boot guard startup/rollback regressions passed")
