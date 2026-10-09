#!/usr/bin/env python3
"""Exercise the real bootloader packet reader and chip detection without a UART."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    (work / "Arduino.h").write_text(r"""
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
using byte = uint8_t;
constexpr int HIGH = 1, LOW = 0, OUTPUT = 1, HEX = 16;
struct String : std::string {
    using std::string::string;
    String(std::string s) : std::string(s) {}
    String(uint32_t n, int) { char s[12]; snprintf(s, sizeof(s), "%x", n); assign(s); }
};
inline unsigned long clockMs = 0;
inline unsigned long millis() { return clockMs; }
inline void delay(unsigned ms) { clockMs += ms; }
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
class Stream {
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual size_t write(uint8_t) = 0;
    size_t write(const char *) { return 0; }
    virtual void flush() {}
    virtual size_t readBytes(uint8_t *out, size_t n) {
        size_t i = 0; while (i < n && available()) out[i++] = read(); return i;
    }
    size_t write(const uint8_t *bytes, size_t n) { for (size_t i = 0; i < n; ++i) write(bytes[i]); return n; }
};
""")
    (work / "test.cpp").write_text(r"""
#include <cassert>
#include <vector>
#include "CCTools.h"
struct UART : Stream {
    std::vector<byte> input, output;
    size_t pos = 0;
    int available() override { return input.size() - pos; }
    int read() override { return available() ? input[pos++] : -1; }
    size_t write(byte value) override { output.push_back(value); return 1; }
    void reply(std::initializer_list<byte> payload) {
        byte checksum = 0; for (byte b : payload) checksum += b;
        input.push_back(0xCC); input.push_back(payload.size() + 2); input.push_back(checksum);
        input.insert(input.end(), payload);
        input.insert(input.end(), {0xCC, 3, 0x40, 0x40}); // command status
    }
};
struct Probe : CCTools {
    using CCTools::CCTools;
    using CommandInterface::_receivePacket;
};
int main() {
    for (std::vector<byte> bad : std::vector<std::vector<byte>>{{}, {6}, {0, 0}, {1, 0}, {2, 0}, {5, 0}, {6, 0, 0}, {6, 1, 0, 0, 0, 0}}) {
        UART uart; uart.input = bad; Probe tool(uart, 16, 32);
        struct { byte data[4]; byte guard = 0xAA; } buffer;
        assert(!tool._receivePacket(buffer.data, 4));
        assert(buffer.guard == 0xAA && uart.output.back() == 0x33);
    }
    UART good; good.input = {6, 10, 1, 2, 3, 4}; Probe tool(good, 16, 32); byte data[4];
    assert(tool._receivePacket(data, 4) && data[3] == 4 && good.output.back() == 0xCC);
    UART missing; CCTools absent(missing, 16, 32); assert(absent.detectChipInfo().empty());
    UART truncated; truncated.input = {0xCC, 6, 0, 0}; CCTools partial(truncated, 16, 32);
    assert(partial.detectChipInfo().empty());
    UART p2;
    p2.reply({0x32, 0x02, 0, 0});
    p2.reply({0, 0x10, 0xB4, 0x3B});
    p2.reply({0, 0x40, 0, 0}); // IEEE enabled, BLE disabled: catches precedence bug
    CCTools chip(p2, 16, 32); assert(chip.detectChipInfo() == "CC2652P2");
    assert(p2.pos == p2.input.size());
}
""")
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-fsanitize=address,undefined", "-g",
                    "-I" + directory, "-I" + str(ROOT / "lib/CCTools/src"), str(work / "test.cpp"),
                    str(ROOT / "lib/CCTools/src/CCTools.cpp"), "-o", str(work / "check")], check=True)
    subprocess.run([str(work / "check")], check=True)
print("CCTools packet/detection regressions passed (ASan/UBSan)")
