#!/usr/bin/env python3
"""Execute startup timezone and real log timestamp code at the clock boundary."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
clock = (ROOT / 'src/etc.cpp').read_text().split('bool clockTimezoneReady = false;', 1)[1]
log = (ROOT / 'src/web/common.cpp').read_text()
timestamp = log[log.index('template <typename Ring>'):log.index('void printLogTime()')]
startup = (ROOT / 'src/main.cpp').read_text().split('void setup()\n', 1)[1]
assert startup.index('applyClockTimezone();') < startup.index('zbInit();')
harness = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include "zones.h"
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define F(x) x
struct String : std::string {
    using std::string::string;
    String(std::string value) : std::string(value) {}
    size_t length() const { return size(); }
    void remove(size_t offset) { erase(offset); }
};
struct { char timeZone[50] = "Etc/GMT-2"; } ConfigSettings;
bool clockTimezoneReady = false;
time_t fakeNow = 1700000000;
time_t fakeTime(time_t *) { return fakeNow; }
#define time fakeTime
unsigned long millis() { return 123; }
unsigned ntpStarts = 0;
void configTime(int, int, const char *, const char *) {
    ++ntpStarts; setenv("TZ", "UTC0", 1); tzset();
}
void printLogMsg(const String &) {}
void applyClockTimezone();
void setTimezone(const String &);
struct Ring : std::string { void push(char value) { push_back(value); } };
'''
checks = r'''
int main() {
    setenv("TZ", "UTC0", 1); tzset();
    Ring early; printTimeTo(early); assert(early == "[123ms]");
    applyClockTimezone(); // before any network/NTP callback
    Ring local; printTimeTo(local); assert(local == "[00:13:20]" && ntpStarts == 0);
    setClock(); // configTime resets TZ; the configured zone must be restored
    Ring later; printTimeTo(later); assert(later == local && ntpStarts == 1);
    fakeNow = 0;
    Ring unsynced; printTimeTo(unsynced); assert(unsynced == early);
}
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(harness + clock + timestamp + checks)
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-fsanitize=address,undefined',
                    '-I' + str(ROOT / 'src'), str(work / 'check.cpp'), '-o', str(work / 'check')], check=True)
    subprocess.run([str(work / 'check')], check=True)
print('Startup/local timezone and log timestamp regressions passed (ASan/UBSan)')
