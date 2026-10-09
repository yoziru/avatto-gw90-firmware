#include "IntelHex.h"
#include "IntelHexRecord.h"
#include <esp_task_wdt.h>

IntelHex::IntelHex(const char *filename) : _filename(filename) {}

IntelHex::~IntelHex() { close(); }

bool IntelHex::open() {
    _file = LittleFS.open(_filename, "r");
    return _file;
}

void IntelHex::close() { _file.close(); }

bool IntelHex::parse(void (*preCallback)(), void (*parseCallback)(uint32_t, uint8_t, uint8_t *, size_t, size_t), void (*postCallback)()) {
    _file_parsed = _file_validated = _file_last_line = _bsl_valid = false;
    _offset_high = _bin_max_address = 0;
    _bsl_level = _bsl_pin = _bsl_addr = 0;
    if (!open()) return false;
    _totalSize = _file.size();
    preCallback();
    bool valid = true;
    while (valid && _file.available()) {
        valid = _munchLine(parseCallback);
        delay(1);
    }
    _file_parsed = valid && _file_last_line && _bin_max_address != 0;
    postCallback();
    close();
    return _file_parsed;
}

bool IntelHex::_munchLine(void (*parseCallback)(uint32_t, uint8_t, uint8_t *, size_t, size_t)) {
    // Intel HEX permits at most 521 characters per record. Bound input before
    // allocating a String so a corrupt upload cannot exhaust the heap.
    char line[522];
    size_t length = 0;
    while (_file.available()) {
        const char c = _file.read();
        if (c == '\n') break;
        if (c == '\r') {
            if (_file.available() && _file.read() != '\n') return false;
            break;
        }
        if (length == sizeof(line) - 1) return false;
        line[length++] = c;
    }
    if (length == 0) return true;
    IntelHexRecord record;
    if (_file_last_line || !decodeIntelHexRecord(line, length, record)) return false;
    if (record.type == 1) {
        _file_last_line = true;
    } else if (record.type == 2 || record.type == 4) {
        _offset_high = uint32_t((record.data[0] << 8) | record.data[1]) << (record.type == 2 ? 4 : 16);
    } else if (record.type == 0) {
        // The flashing transport currently sends whole records in 240-byte
        // packets. Reject larger records before erase, not midway through it.
        if (record.length > 240) return false;
        const uint64_t address = uint64_t(_offset_high) + record.address;
        const uint64_t end = address + record.length;
        if ((_bin_max_address == 0 && address != 0) || address < _bin_max_address || end > 0xB0000) return false;
        _bin_max_address = end;
        if (!_bsl_valid) _bsl_valid = _checkBSLconfig(address, record.length, record.data);
        parseCallback(address, record.length, record.data, _file.position(), _totalSize);
    }
    esp_task_wdt_reset();
    return true;
}

bool IntelHex::_checkBSLconfig(uint32_t address, uint8_t len, uint8_t *data) {
    for (size_t i = 0; i < ELEMENTCOUNT(CCFG_ADDRESS); ++i) {
        if (address > CCFG_ADDRESS[i] || uint64_t(address) + len < uint64_t(CCFG_ADDRESS[i]) + 4) continue;
        const uint8_t *config = data + (CCFG_ADDRESS[i] - address);
        if (config[0] != BL_ENABLE || config[3] != BOOTLOADER_ENABLE ||
            (config[2] != BL_LEVEL_LOW && config[2] != BL_LEVEL_HIGH) || config[1] == 0) continue;
        _bsl_level = config[2] == BL_LEVEL_LOW ? 1 : 2;
        _bsl_pin = config[1];
        _bsl_addr = i;
        return true;
    }
    return false;
}
