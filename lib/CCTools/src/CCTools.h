#ifndef CCTools_DETECT_H
#define CCTools_DETECT_H

#include <Arduino.h>

class CommandInterface {
protected:
    Stream &_stream;
    static const uint8_t ACK_BYTE = 0xCC;
    static const uint8_t NACK_BYTE = 0x33;
    static const uint8_t COMMAND_RET_SUCCESS = 0x40;
    static const uint32_t ICEPICK_DEVICE_ID = 0x50001318;
    static const uint32_t FCFG_USER_ID = 0x50001294;
    static const uint32_t PROTO_MASK_IEEE = 0x04;

    CommandInterface(Stream &serial);
    bool _sendSynch();
    bool _wait_for_ack(unsigned long timeout = 1);
    uint32_t _cmdGetChipId();
    bool _receivePacket(byte *data, uint8_t length);
    bool _checkLastCmd();
    void _sendAck();
    void _sendNAck();
    void _encodeAddr(unsigned long addr, byte encodedAddr[4]);
    byte _calcChecks(byte cmd, unsigned long addr, unsigned long size);
    bool _cmdMemRead(uint32_t address, byte data[4]);
};

class CCTools : public CommandInterface {
private:
    int _CC_RST_PIN, _CC_BSL_PIN;
public:
    CCTools(Stream &serial, int CC_RST_PIN, int CC_BSL_PIN);
    bool begin();
    String detectChipInfo();
    void enterBSL();
    void restart();
    void routerRejoin();
};

#endif
