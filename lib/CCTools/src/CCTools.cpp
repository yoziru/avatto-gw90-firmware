#include "CCTools.h"

CommandInterface::CommandInterface(Stream &serial) : _stream(serial) {}

bool CommandInterface::_sendSynch() {
    _stream.flush();
    _stream.write(0x55);
    _stream.write(0x55);
    return _wait_for_ack(2);
}

bool CommandInterface::_wait_for_ack(unsigned long timeout) {
    const unsigned long start = millis();
    while (millis() - start < timeout * 1000) {
        if (_stream.available()) {
            const int reply = _stream.read();
            if (reply == ACK_BYTE) return true;
            if (reply == NACK_BYTE) return false;
        }
        delay(1);
    }
    return false;
}

uint32_t CommandInterface::_cmdGetChipId() {
    _stream.write(3);
    _stream.write(0x28);
    _stream.write(0x28);
    byte version[4];
    if (!_wait_for_ack() || !_receivePacket(version, sizeof(version)) || !_checkLastCmd()) return 0;
    return (uint32_t(version[0]) << 8) | version[1];
}

bool CommandInterface::_checkLastCmd() {
    _stream.write(3);
    _stream.write(0x23);
    _stream.write(0x23);
    byte status;
    return _wait_for_ack() && _receivePacket(&status, 1) && status == COMMAND_RET_SUCCESS;
}

void CommandInterface::_sendAck() {
    _stream.write(uint8_t(0x00));
    _stream.write(ACK_BYTE);
}

void CommandInterface::_sendNAck() {
    _stream.write(uint8_t(0x00));
    _stream.write(NACK_BYTE);
}

bool CommandInterface::_cmdMemRead(uint32_t address, byte data[4]) {
    byte encoded[4];
    _encodeAddr(address, encoded);
    _stream.write(9);
    _stream.write(_calcChecks(0x2A, address, 2));
    _stream.write(0x2A);
    _stream.write(encoded, sizeof(encoded));
    _stream.write(1); // width: 4 bytes
    _stream.write(1); // one word
    return _wait_for_ack() && _receivePacket(data, 4) && _checkLastCmd();
}

bool CommandInterface::_receivePacket(byte *data, uint8_t length) {
    byte header[2];
    if (!data || length == 0 || _stream.readBytes(header, sizeof(header)) != sizeof(header) ||
        unsigned(header[0]) != unsigned(length) + 2 || _stream.readBytes(data, length) != length) {
        _sendNAck();
        return false;
    }
    byte checksum = 0;
    for (unsigned i = 0; i < length; ++i) checksum += data[i];
    if (checksum != header[1]) {
        _sendNAck();
        return false;
    }
    _sendAck();
    return true;
}

void CommandInterface::_encodeAddr(unsigned long addr, byte encoded[4]) {
    for (unsigned i = 0; i < 4; ++i) encoded[i] = addr >> (8 * (3 - i));
}

byte CommandInterface::_calcChecks(byte cmd, unsigned long addr, unsigned long size) {
    byte checksum = cmd;
    for (unsigned i = 0; i < 4; ++i) {
        checksum += addr >> (8 * i);
        checksum += size >> (8 * i);
    }
    return checksum;
}

CCTools::CCTools(Stream &serial, int resetPin, int bslPin)
    : CommandInterface(serial), _CC_RST_PIN(resetPin), _CC_BSL_PIN(bslPin) {}

bool CCTools::begin() {
    pinMode(_CC_RST_PIN, OUTPUT);
    digitalWrite(_CC_RST_PIN, HIGH);
    pinMode(_CC_BSL_PIN, OUTPUT);
    digitalWrite(_CC_BSL_PIN, HIGH);
    enterBSL();
    return _sendSynch();
}

void CCTools::enterBSL() {
    digitalWrite(_CC_RST_PIN, LOW);
    digitalWrite(_CC_BSL_PIN, LOW);
    delay(50);
    digitalWrite(_CC_RST_PIN, HIGH);
    delay(500);
    digitalWrite(_CC_BSL_PIN, HIGH);
    delay(500);
}

void CCTools::restart() {
    digitalWrite(_CC_RST_PIN, LOW);
    delay(50);
    digitalWrite(_CC_RST_PIN, HIGH);
    delay(500);
}

void CCTools::routerRejoin() {
    digitalWrite(_CC_BSL_PIN, LOW);
    delay(250);
    digitalWrite(_CC_BSL_PIN, HIGH);
    delay(500);
}

String CCTools::detectChipInfo() {
    const uint32_t chip = _cmdGetChipId();
    byte device[4], user[4];
    if (!chip || !_cmdMemRead(ICEPICK_DEVICE_ID, device) || !_cmdMemRead(FCFG_USER_ID, user)) return "";
    const uint32_t wafer = (((device[3] & 0x0F) << 16) | (device[2] << 8) | (device[1] & 0xF0)) >> 4;
    const uint32_t revision = (device[3] & 0xF0) >> 4;
    if (((user[1] >> 4) & PROTO_MASK_IEEE) != PROTO_MASK_IEEE) return "";
    if (chip == 0x1202 && wafer == 0xBB77 && revision == 1) return "CC2652P7";
    if (chip == 0x3202 && wafer == 0xBB41 && revision == 3) return "CC2652P2";
    return "Unknown ( " + String(chip, HEX) + " " + String(wafer, HEX) + " " + String(revision, HEX) + " )";
}
