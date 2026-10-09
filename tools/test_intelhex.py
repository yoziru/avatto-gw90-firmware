#!/usr/bin/env python3
"""Exercise the real HEX file parser on a host, without Arduino hardware."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    (work / "Arduino.h").write_text("""
#include <cstddef>
#include <cstdint>
inline void delay(unsigned) {}
""")
    (work / "esp_task_wdt.h").write_text("inline void esp_task_wdt_reset() {}\n")
    (work / "LittleFS.h").write_text("""
#pragma once
#include <fstream>
#include <memory>
struct File {
    std::shared_ptr<std::ifstream> input;
    operator bool() const { return input && input->is_open(); }
    void close() { if (input) input->close(); }
    size_t size() { auto pos = input->tellg(); input->seekg(0, std::ios::end); auto n = input->tellg(); input->seekg(pos); return n; }
    bool available() { return input->peek() != EOF; }
    int read() { return input->get(); }
    size_t position() { return input->tellg(); }
};
struct FileSystem {
    File open(const char *path, const char *) { return {std::make_shared<std::ifstream>(path)}; }
};
inline FileSystem LittleFS;
""")
    (work / "test.cpp").write_text(r"""
#include <cassert>
#include <fstream>
#include <string>
#include "IntelHex.h"
void noop() {}
void data(uint32_t, uint8_t, uint8_t *, size_t, size_t) {}
std::string rec(uint8_t n, uint16_t address, uint8_t type, uint8_t value = 0) {
    char buf[12];
    snprintf(buf, sizeof(buf), ":%02X%04X%02X", n, address, type);
    std::string result(buf);
    uint8_t sum = n + (address >> 8) + address + type;
    for (unsigned i = 0; i < n; ++i) {
        snprintf(buf, sizeof(buf), "%02X", value);
        result += buf; sum += value;
    }
    snprintf(buf, sizeof(buf), "%02X\n", uint8_t(-sum));
    return result + buf;
}
int main(int argc, char **argv) {
    const std::string start = ":0400000000000000FC\n";
    const std::string prefix = start + ":020000040005F5\n:047FD800C50FFEC50E\n";
    auto check = [&](std::string text, bool valid) {
        { std::ofstream file(argv[1]); file << text; }
        IntelHex hex(argv[1]);
        assert(hex.parse(noop, data, noop) == valid);
        assert(hex.fileParsed() == valid);
        if (valid) assert(hex.bslActive() && hex.bslPin() == 15 && !hex.bslLevel());
    };
    check(prefix + ":00000001FF\n", true); // CCFG ending exactly at record boundary
    std::string longRecord = rec(200, 0x7F80, 0);
    longRecord.replace(9 + 88 * 2, 8, "C50FFEC5");
    longRecord.replace(longRecord.size() - 3, 3, "A2\n");
    check(start + ":020000040005F5\n" + longRecord + ":00000001FF\n", true); // CCFG beyond old 100-byte buffer
    check(":020000040005F5\n:047FD800C50FFEC50E\n:00000001FF\n", false); // missing reset vectors
    check(prefix + ":0100000000\n:00000001FF\n", false); // corruption after valid CCFG
    check(prefix, false); // no EOF
    check(prefix + ":00000001FF\n" + rec(1, 0x7FE0, 0), false); // data after EOF
    check(prefix + rec(1, 0x7FD8, 0) + ":00000001FF\n", false); // overlap
    check(prefix + rec(255, 0x8000, 0) + ":00000001FF\n", false); // unsupported transport length
    check(prefix + ":020000040010EA\n" + rec(1, 0, 0) + ":00000001FF\n", false); // out of flash range
    check(prefix + std::string(600, '0') + "\n:00000001FF\n", false);
    { std::ofstream file(argv[1]); file << prefix << ":00000001FF\n"; }
    IntelHex reused(argv[1]);
    assert(reused.parse(noop, data, noop));
    assert(reused.parse(noop, data, noop)); // state reset for each parse
}
""")
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-fsanitize=address,undefined", "-g",
                    "-I" + directory, "-I" + str(ROOT / "lib/IntelHex/src"), str(work / "test.cpp"),
                    str(ROOT / "lib/IntelHex/src/IntelHex.cpp"), "-o", str(work / "check")], check=True)
    subprocess.run([str(work / "check"), str(work / "firmware.hex")], check=True)
print("HEX parser regressions passed (ASan/UBSan)")
