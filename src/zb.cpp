#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>
#include <ETH.h>
#include <HTTPClient.h>
#include <esp_task_wdt.h>

#include <IntelHex.h>
#include <CCTools.h>

#include "config.h"
#include "checksum.h"
#include "web.h"
#include "log.h"
#include "etc.h"
#include "zb.h"
#include "zb_ver.h"

zbVerStruct zbVer;

volatile unsigned long zbLastRxMs = 0;   // last time bytes were received from the coordinator
volatile unsigned long zbClientTxMs = 0; // last time a TCP client sent bytes to the coordinator
volatile uint32_t zbAutoResets = 0;      // count of automatic coordinator resets by zbHealthCheck

unsigned long zbGetLastRxMs()
{
    return zbLastRxMs;
}

unsigned long zbGetLastClientTxMs()
{
    return zbClientTxMs;
}

uint32_t zbGetAutoResetCount()
{
    return zbAutoResets;
}

const byte cmdLed0 = 0x27;
const byte cmdLed1 = 0x0A;
const byte cmdLedIndex = 0x01; // for led 1
const byte cmdLedStateOff = 0x00;
const byte cmdLedStateOn = 0x01;
const byte cmdFrameStart = 0xFE;
const byte cmdLedLen = 0x02;

const byte zigLed1Off[] = {cmdFrameStart, cmdLedLen, cmdLed0, cmdLed1, cmdLedIndex, cmdLedStateOff, 0x2E}; // resp FE 01 67 0A 00 6C
const byte zigLed1On[] = {cmdFrameStart, cmdLedLen, cmdLed0, cmdLed1, cmdLedIndex, cmdLedStateOn, 0x2F};
const byte cmdLedResp[] = {0xFE, 0x01, 0x67, 0x0A, 0x00, 0x6C};

CCTools CCTool(Serial2, CC2652P_RST, CC2652P_FLASH);

bool bslActive;

void clearS2Buffer()
{
    while (Serial2.available())
    { // clear buffer
        Serial2.read();
    }
}


// Protocol version | Product ID | Version | 20250321
//   02        01      02 07 01     D1 FE 34 01   00     74(checksum)
static void getZbRole()
{
    const byte cmdZdoExtNwkInfo[] = {0xFE, 0x00, 0x25, 0x50, 0x75};
    const uint8_t responseLen = 24;

    for (uint8_t i = 0; i < 3; i++)
    {
        clearS2Buffer();
        Serial2.write(cmdZdoExtNwkInfo, sizeof(cmdZdoExtNwkInfo));
        Serial2.flush();
        const unsigned long deadline = millis() + 500;
        while (Serial2.available() < responseLen + 5 && millis() < deadline)
        {
            delay(1);
        }

        if (Serial2.available() < responseLen + 5 || Serial2.read() != 0xFE || Serial2.read() != responseLen || Serial2.read() != 0x65 || Serial2.read() != 0x50)
        {
            clearS2Buffer();
            continue;
        }

        byte response[responseLen];
        byte expectedChecksum = responseLen ^ 0x65 ^ 0x50;
        for (uint8_t j = 0; j < responseLen; j++)
        {
            response[j] = Serial2.read();
            expectedChecksum ^= response[j];
        }
        const byte responseChecksum = Serial2.read();
        if (response[0] != 0 || responseChecksum != expectedChecksum)
        {
            clearS2Buffer();
            continue;
        }
        zbVer.deviceState = response[2];
        printLogMsg(String("[ZBROLE] ") + zbRoleName(zbRoleFromDeviceState(zbVer.deviceState)) + " (state " + zbVer.deviceState + ")");
        return;
    }
}

void getZbVer()
{
    zbVer.zbRev = 0;
    zbVer.deviceState = 0xFF;
    const byte cmdFrameStart = 0xFE;
    const byte zero = 0x00;
    const byte cmd1 = 0x21;
    const byte cmd2 = 0x02;
    const byte cmdSysVersion[] = {cmdFrameStart, zero, cmd1, cmd2, 0x23};
    bool versionRead = false;


    for (uint8_t i = 0; i < 6; i++)
    {
        if (Serial2.read() != cmdFrameStart || Serial2.read() != 0x0a || Serial2.read() != 0x61 || Serial2.read() != cmd2)
        {                    // check for packet start
            clearS2Buffer(); // skip
            Serial2.write(cmdSysVersion, sizeof(cmdSysVersion));
            Serial2.flush();
            delay(100);
        }
        else
        {
            const uint8_t zbVerLen = 11;
            byte zbVerBuf[zbVerLen];
            for (uint8_t j = 0; j < zbVerLen; j++)
            {
                zbVerBuf[j] = Serial2.read();
            }
            const ZbVerInfo info = decodeZbVer(zbVerBuf);
            zbVer.zbRev = info.zbRev;
            zbVer.maintrel = info.maintrel;
            zbVer.minorrel = info.minorrel;
            zbVer.majorrel = info.majorrel;
            zbVer.product = info.product;
            zbVer.transportrev = info.transportrev;

            printLogMsg(String("[ZBVER]") + " Rev: " + zbVer.zbRev + " Maintrel: " + zbVer.maintrel + " Minorrel: " + zbVer.minorrel + " Majorrel: " + zbVer.majorrel + " Transportrev: " + zbVer.transportrev + " Product: " + zbVer.product);
            clearS2Buffer();
            versionRead = true;
            break;
        }
    }

    if (versionRead)
    {
        getZbRole();
    }
}

// Send a Zigbee LED on/off command and wait for the coordinator's ack frame.
// Returns true once a complete matching response (FE 67 0A ...) is observed.
bool zbLedCommand(const byte *cmd, size_t cmdLen)
{
    clearS2Buffer();
    Serial2.write(cmd, cmdLen);
    Serial2.flush();
    delay(400);
    for (uint8_t i = 0; i < 5; i++)
    {
        if (Serial2.read() != 0xFE)
        {                   // check for packet start
            Serial2.read(); // skip
            continue;
        }
        bool frameOk = true;
        for (uint8_t j = 1; j < 4; j++)
        {
            if (Serial2.read() != cmdLedResp[j])
            { // check if resp ok
                frameOk = false;
                break;
            }
        }
        if (frameOk)
        {
            return true;
        }
    }
    return false;
}

void zbCheck()
{
    bool respOk = false;
    for (uint8_t i = 0; i < 12; i++)
    { // wait for zigbee start
        respOk = zbLedCommand(zigLed1On, sizeof(zigLed1On));
        digitalWrite(LED_USB, !digitalRead(LED_USB)); // blue led flashing mean wait for zigbee resp
        if (respOk)
            break;
    }
    delay(500);
    if (!respOk)
    {
        digitalWrite(LED_PWR, 1);
        digitalWrite(LED_USB, 1);
        for (uint8_t i = 0; i < 5; i++)
        { // indicate wrong resp
            digitalWrite(LED_PWR, !digitalRead(LED_PWR));
            digitalWrite(LED_USB, !digitalRead(LED_USB));
            delay(1000);
        }
        printLogMsg("[ZBCHK] Wrong answer");
        printLogMsg("[ZBVER] Unknown");
        zbVer.zbRev = 0;
        zbVer.deviceState = 0xFF;
    }
    else
    {
        Serial2.write(zigLed1Off, sizeof(zigLed1Off));
        Serial2.flush();
        delay(250);
        clearS2Buffer();
        printLogMsg("[ZBCHK] Connection OK");
    }
    digitalWrite(LED_PWR, 1);
    digitalWrite(LED_USB, 0);
}

// Periodic coordinator health check.
//
// Fully passive: it never injects frames into Serial2 and never consumes bytes
// that belong to the ZNP bridge. It simply observes whether the coordinator is
// producing output. If a TCP client (ZHA/Zigbee2MQTT) has been actively sending
// frames to the coordinator but the coordinator has produced NOTHING for a long
// window, the coordinator is almost certainly hung, so we reset it. This keeps
// ZHA/Z2M from silently losing the Zigbee network.
//
// IMPORTANT: this must stay observation-only. Probing the coordinator with
// BSL-mode commands (zbGetStatus/zbSendCommand) on the shared Serial2 line would
// corrupt live ZNP frames and break connectivity - do not reintroduce that.
void zbHealthCheck()
{
    if (ConfigSettings.coordinator_mode == CoordinatorMode::PASSTHROUGH)
    {
        return; // Serial passthrough: nothing to monitor
    }

    const unsigned long now = millis();
    const unsigned long coordinatorSilenceMs = now - zbLastRxMs;
    const unsigned long clientIdleMs = now - zbClientTxMs;

    // Only suspect a hang when a client is (or recently was) actively talking
    // to the coordinator. A quiet-but-healthy network must never trigger a reset.
    const unsigned long silentWindowMs = 180000; // 3 min with zero coordinator output
    const unsigned long clientActiveWindowMs = 120000; // client sent frames within 2 min

    if (coordinatorSilenceMs < silentWindowMs || clientIdleMs > clientActiveWindowMs)
    {
        return;
    }

    static uint8_t missCount = 0;
    missCount++;
    printLogMsg(String("[ZBH] Coordinator silent ") + String(coordinatorSilenceMs / 1000) + "s while client active (" + String(missCount) + "/3)");
    if (missCount >= 3)
    {
        printLogMsg("[ZBH] Coordinator hung, resetting...");
        missCount = 0;
        zbAutoResets = static_cast<uint32_t>(zbAutoResets + 1);
        zigbeeRestart();
    }
}

void zbLedToggle()
{
    if (ConfigSettings.zbLedState == 0)
    {
        printLogMsg("[ZB] LED toggle ON");
        if (zbLedCommand(zigLed1On, sizeof(zigLed1On)))
        {
            printLogMsg("[ZB] LED toggle OK");
            ConfigSettings.zbLedState = true;
        }
    }
    else
    {
        printLogMsg("[ZB] LED toggle OFF");
        if (zbLedCommand(zigLed1Off, sizeof(zigLed1Off)))
        {
            printLogMsg("[ZB] LED toggle OK");
            ConfigSettings.zbLedState = false;
        }
    }
}

void preParse()
{
    DEBUG_PRINTLN(String(millis()) + " Starting the parsing process");
}

void postParse()
{
    DEBUG_PRINTLN(String(millis()) + " Parsing complete");
}

void parseCallback(uint32_t address, uint8_t len, uint8_t *data, size_t currentPosition, size_t _totalSize)
{
    // IntelHex parse hook. Progress is reported by checkFwHex() / runFlash();
    // this callback is intentionally a no-op.
    (void)address;
    (void)len;
    (void)data;
    (void)currentPosition;
    (void)_totalSize;
}

int8_t runFlash(const char *hexFilePath,uint32_t binsize)
{
    //const char *tempFile = "/config/fw.hex";
    File fwFile = LittleFS.open(tempFile, "r");
    if (!fwFile)
    {
        DEBUG_PRINTLN(F("Failed to open fw.hex file"));
        printLogMsg("[ZB_FLASH] Failed to open fw.hex file");
        return -1;
    }

    DEBUG_PRINTLN(F("Starting to send fw.hex data..."));
    printLogMsg("[ZB_FLASH] Starting to send fw.hex data...");

    uint32_t zbFlashTotalSize = binsize;
    uint32_t zbFlashTotalSent  = 0;

    if (zbFlashTotalSize  == 0)
    {
        printLogMsg("[ZB_FLASH] Failed to get total size");
        fwFile.close();
        return -1;
    }
    zbFlashStart(0,zbFlashTotalSize);

    size_t packetCount = 0;

    // Buffer for 240 bytes of data
    byte dataBuffer[240] = {0};
    size_t totalBytes = 0;
    uint8_t lineCount = 0;
    
    // Address tracking variables
    uint32_t extendedAddr = 0;
    uint32_t currentWriteAddr = 0;  // Current write address in buffer
    uint32_t nextExpectAddr = 0;    // Expected address for next record
    bool firstRecord = true;
    uint8_t i=0;

    // Progress tracking variables
    size_t lastProgressSize = 0;
    const size_t progressInterval = zbFlashTotalSize / 100;  // Report progress every ~1%

    Serial.printf("fw flash size: %ld\n",zbFlashTotalSize);
    while (fwFile.available())
    {
        String line = fwFile.readStringUntil('\n');
        line.trim();
        
        if (line.length() > 0 && line[0] == ':')
        {
            // Skip end-of-file record
            if (line == ":00000001FF")
            {
                DEBUG_PRINTLN("HEX file end record found");
                break;
            }
            
            // Parse record type
            uint8_t recordType = strtol(line.substring(7, 9).c_str(), nullptr, 16);
            // Handle extended linear address record (0x04)
            if (recordType == 0x04)
            {
                extendedAddr = strtol(line.substring(9, 13).c_str(), nullptr, 16) << 16;
                continue;
            }
            // Handle extended segment address record (0x02)
            if (recordType == 0x02)
            {
                extendedAddr = strtol(line.substring(9, 13).c_str(), nullptr, 16) << 4;
                continue;
            }
            // Only process data records (type 00)
            if (recordType != 0x00)
            {
                continue;
            }
            // Parse current line address and data length
            uint16_t lineAddr = strtol(line.substring(3, 7).c_str(), nullptr, 16);
            uint8_t lineDataLen = strtol(line.substring(1, 3).c_str(), nullptr, 16);
            // A single record bigger than the send buffer would overflow
            // dataBuffer[] below - refuse the file instead of corrupting RAM.
            if (lineDataLen > sizeof(dataBuffer))
            {
                printLogMsg("[ZB_FLASH] HEX record exceeds buffer size, aborting");
                goto flash_quit;
            }
            uint32_t fullAddr = extendedAddr + lineAddr;
            // Initialize on first record
            if (firstRecord)
            {
                currentWriteAddr = fullAddr;
                nextExpectAddr = fullAddr;
                firstRecord = false;
                DEBUG_PRINTLN(String("[HEX] Start address: 0x") + String(fullAddr, HEX));
            }
            // Check if address is contiguous, fill gaps if not
            if (fullAddr != nextExpectAddr)
            {
                // Calculate gap size
                uint32_t gapSize = fullAddr - nextExpectAddr;
                
                DEBUG_PRINT(String("[HEX] Gap detected: 0x") + String(nextExpectAddr, HEX));
                DEBUG_PRINT(" -> 0x" + String(fullAddr, HEX));
                DEBUG_PRINTLN(" (" + String(gapSize) + " bytes)");
                
                while (gapSize > 0)
                {
                    // Calculate fill size for this iteration (remaining buffer space)
                    uint32_t fillSize = gapSize;
                    if (fillSize > (240 - totalBytes))
                    {
                        fillSize = 240 - totalBytes;
                    }
                    // Fill with 0xFF
                    for (uint32_t i = 0; i < fillSize; i++)
                    {
                        dataBuffer[totalBytes + i] = 0xFF;
                    }
                    totalBytes += fillSize;
                    gapSize -= fillSize;
                    nextExpectAddr += fillSize;
                    
                    // If buffer is full, send current data
                    if (totalBytes >= 240)
                    {
                        packetCount++;
                        zbFlashTotalSent += totalBytes;
                        
                        DEBUG_PRINT("Packet #");
                        DEBUG_PRINT(packetCount);
                        DEBUG_PRINT(" | Addr: 0x");
                        DEBUG_PRINT(String(currentWriteAddr, HEX));
                        DEBUG_PRINT(" | ");
                        DEBUG_PRINT(totalBytes);
                        DEBUG_PRINTLN(" bytes [GAP FILL]");
                        i=0;              
                        while(!zbCheckLastCmd())
                        {
                            i++;
                            if(i>10)goto flash_quit;
                        }
                        zbSendCommand(0x24, dataBuffer, totalBytes, 3);
                        
                        memset(dataBuffer, 0, sizeof(dataBuffer));
                        totalBytes = 0;
                        lineCount = 0;
                        currentWriteAddr = nextExpectAddr;
                        
                        if (zbFlashTotalSent - lastProgressSize >= progressInterval)
                        {
                            uint8_t percent = ((float)zbFlashTotalSent / zbFlashTotalSize) * 100;
                            sendEvent("ZB_FW_flashing", sizeof("ZB_FW_flashing"), String(percent));
                            lastProgressSize = zbFlashTotalSent;
                        }
                    }
                }
            }
            
            // Check if buffer can hold current line data
            if (totalBytes + lineDataLen > 240)
            {
                // Buffer is full, send current data first
                packetCount++;
                zbFlashTotalSent += totalBytes;
                
                DEBUG_PRINT("Packet #");
                DEBUG_PRINT(packetCount);
                DEBUG_PRINT(" | Addr: 0x");
                DEBUG_PRINT(String(currentWriteAddr, HEX));
                DEBUG_PRINT(" | ");
                DEBUG_PRINT(totalBytes);
                DEBUG_PRINT(" bytes (");
                DEBUG_PRINT(lineCount);
                DEBUG_PRINTLN(" lines)");
                
                i=0;              
                while(!zbCheckLastCmd())
                {
                    i++;
                    if(i>10)goto flash_quit;
                }
                zbSendCommand(0x24, dataBuffer, totalBytes, 3);
                
                // Clear buffer, update start address
                memset(dataBuffer, 0, sizeof(dataBuffer));
                totalBytes = 0;
                lineCount = 0;
                currentWriteAddr = fullAddr;
                nextExpectAddr = fullAddr;
                

                if (zbFlashTotalSent - lastProgressSize >= progressInterval)
                {
                    uint8_t percent = ((float)zbFlashTotalSent / zbFlashTotalSize) * 100;
                    sendEvent("ZB_FW_flashing", sizeof("ZB_FW_flashing"), String(percent));
                    lastProgressSize = zbFlashTotalSent;
                }
            }
            
            // Parse current line data and write to buffer
            for (uint8_t i = 0; i < lineDataLen; i++)
            {
                char hexByte[3] = {0};
                hexByte[0] = line.charAt(9 + i * 2);
                hexByte[1] = line.charAt(9 + i * 2 + 1);
                dataBuffer[totalBytes + i] = strtol(hexByte, nullptr, 16);
            }
            
            totalBytes += lineDataLen;
            lineCount++;
            nextExpectAddr = fullAddr + lineDataLen; // Update expected next address
        }
    }
    
    // Send remaining data in last frame
    if (totalBytes > 0)
    {
        packetCount++;
        zbFlashTotalSent += totalBytes;
        
        DEBUG_PRINT("Packet #");
        DEBUG_PRINT(packetCount);
        DEBUG_PRINT(" | Addr: 0x");
        DEBUG_PRINT(String(currentWriteAddr, HEX));
        DEBUG_PRINT(" | ");
        DEBUG_PRINT(totalBytes);
        DEBUG_PRINT(" bytes (");
        DEBUG_PRINT(lineCount);
        DEBUG_PRINTLN(" lines) [LAST]");
        
        i=0;              
        while(!zbCheckLastCmd())
        {
            i++;
            if(i>10)goto flash_quit;
        }
        zbSendCommand(0x24, dataBuffer, totalBytes, 3);

        // Report final progress
        zbFlashTotalSent = zbFlashTotalSize;

        // Send flash complete event
        sendEvent("ZB_FW_flashing", sizeof("ZB_FW_flashing"), "100");
    }
    i=0;              
    while(!zbCheckLastCmd())
    {
        i++;
        if(i>10)break;
    }
flash_quit:
    fwFile.close();
    Serial2.updateBaudRate(ConfigSettings.serialSpeed); 
    CCTool.restart();         // Restart
    DEBUG_PRINTLN(F("fw.hex data send complete"));
    Serial.printf("total send: %ld",zbFlashTotalSent);
    printLogMsg(String("[ZB_FLASH] fw.hex data send complete | Total: ") + String(zbFlashTotalSent) + " bytes");
    if(zbFlashTotalSent == zbFlashTotalSize)return 0;
    return -1;
}

int8_t checkFwHex(const char *tempFile) // check Zigbee FW file using IntelHEX, than check BSL pin.
{
    IntelHex zb_hex(tempFile);

    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    Serial.printf("[ZB_FLASH] Free heap before verify: %u bytes\n", (unsigned int)free_heap);

    if (!zb_hex.parse(preParse, parseCallback, postParse))
    {
        String msg = ("Failed to parse the zb_hex HEX file. Corrupted file");
        DEBUG_PRINTLN(msg);
        printLogMsg(msg);
    }

    if (!zb_hex.fileParsed())
    {
        String msg = ("File not good");
        DEBUG_PRINTLN(msg);
        printLogMsg(msg);
    }

    u_int8_t local_chip_id = ALL_CHIP_ID;
    if (zbVer.chipID == "CC2652P7")
        local_chip_id = P7_CHIP_ID;

    if (zb_hex.bslActive())
    {
        String msg = "BSL (" + String(zb_hex.bslAddr() ? "P7 and R7 chips" : "All chips") + ") pin " + String(zb_hex.bslPin()) + " level " + String(zb_hex.bslLevel() ? "HIGH" : "LOW");
        DEBUG_PRINTLN(msg);
        printLogMsg(msg);

        if (zb_hex.bslAddr() == local_chip_id && zb_hex.bslPin() == NEED_BSL_PIN && zb_hex.bslLevel() == NEED_BSL_LEVEL) // All series DIO 15 LOW
        {
            zb_hex.setFileValidated(true);
            String msg = ("BSL config OK");
            DEBUG_PRINTLN(msg);
            printLogMsg(msg);
        }
        else
        {
            String msg = ("BSL config incorect. Wrong chip, pin or level.");
            DEBUG_PRINTLN(msg);
            printLogMsg(msg);
        }
    }
    else
    {
        String msg = ("BSL config error. Range not found or CCFG incorrect.");
        DEBUG_PRINTLN(msg);
        printLogMsg(msg);
    }

    if (!zb_hex.fileValidated())
    {
        String msg = ("Zigbee FW file INVALID - ERROR");
        DEBUG_PRINTLN(msg);
        printLogMsg(msg);
        return -1;
    }
    else
    {
        String msg = ("Zigbee FW file VALID - OK");
        DEBUG_PRINTLN(msg);
        printLogMsg(msg);
        sendEvent("ZB_FW_info", sizeof("ZB_FW_info"), "Validation complete!");

        return runFlash(tempFile,zb_hex.getBinSize());
    }
    return -1;
}

void zbInit()
{
    if (CCTool.begin())
    {
        String zb_chip = CCTool.detectChipInfo();
        DEBUG_PRINTLN(zb_chip);
        printLogMsg(String("[ZBCHIP] ") + zb_chip);
        zbVer.chipID = zb_chip;
        CCTool.restart();
    }
    else
    {
        String msg = "No connection with Zigbee";
        printLogMsg(String("[ZBCHIP] ") + msg);
        DEBUG_PRINTLN(msg);
    }
}

/************************************************************************** */

bool zbWaitForAck(uint32_t timeout)
{
    uint32_t startMillis = millis();
    while (millis() - startMillis < timeout * 1000)
    {
        if (Serial2.available() >= 1)
        {
            uint8_t received = Serial2.read();
            if (received == 0xcc)
            {
                return true;
            }
            else if (received == 0x33)
            {
                return false;
            }
        }
    }
    return false;
}

bool zbSendCmdResponse(bool ack)
{
    uint8_t pData[2] = {0};
    pData[0] = 0x00;
    pData[1] = (ack) ? 0xCC : 0x33;

    if(Serial2.write(pData,2)!=2)return false;

    //Serial.println("zbSendCommand ack: " + String(ack ? "ACK" : "NACK"));

    return true;
}

bool zbSendCommand(uint8_t cmd, uint8_t *data, uint8_t len,uint8_t timeout)
{
    if(len > 252)return false;

    byte cmd_buf[255] = {0}; // 3 header bytes + max 252 payload

    cmd_buf[0] = len + 3;
    cmd_buf[1] = zbGenChksum(cmd,data,len);
    cmd_buf[2] = cmd;

    for (unsigned int i = 0; i < len; i++)
    {
        cmd_buf[i + 3] = data[i];
    }

    Serial2.write(cmd_buf, len + 3);

    return zbWaitForAck(1);
}

// Read a ZNP response frame into `data` (capacity `dataLen`). Returns 1 on a
// checksum-valid frame, 0 on timeout/corruption. The payload size announced in
// the frame header is clamped to the caller's buffer to avoid stack corruption
// from a malformed coordinator response.
static uint32_t zbGetResponseData(uint8_t *data, uint8_t dataLen, uint8_t timeout)
{
    unsigned int i = 0;
    byte checksum = 0;

    if (data == NULL || dataLen < 2)
        return 0;

    uint32_t numPayloadBytes = 0;
    unsigned long startMillis = millis();

    while (millis() - startMillis < timeout * 1000)
    {
        if (Serial2.available() >= 1)
        {
            data[i++] = Serial2.read();
        }
        if (i == 2)
            break;
    }

    if (i < 2)
        return 0;

    numPayloadBytes = data[0] - 2;
    if (numPayloadBytes > dataLen)
    {
        numPayloadBytes = dataLen; // clamp to the caller's buffer
    }
    checksum = data[1];

    i = 0;
    while (millis() - startMillis < timeout * 1000)
    {
        if (Serial2.available() >= 1)
        {
            data[i++] = Serial2.read();
        }
        if (i == numPayloadBytes)
            break;
    }

    if (i < numPayloadBytes)
        return 0;

    if (checksum != zbGenChksum(0, data, numPayloadBytes))
    {
        zbSendCmdResponse(false);
        return 0;
    }
    zbSendCmdResponse(true);
    return 1;
}

bool zbGetStatus(uint8_t *status, uint8_t statusLen, uint32_t timeout)
{
    if (status == NULL)
        return false;

    if (zbSendCommand(0x23, 0, 0, timeout))
    {
        if (zbGetResponseData(status, statusLen, timeout))
        {
            return true;
        }
    }
    return false;
}

bool zbCheckLastCmd(void)
{
    uint8_t stat[10] = {0};
    if (zbGetStatus(stat, sizeof(stat), 1))
    {
        return (stat[0] == 0x40);
    }
    return false;
 }

 bool zbFlashStart(uint32_t addr,uint32_t flashSize)
 {
    uint8_t buf[8]={0};
    uint32_t size =0;

    size = flashSize; 

    buf[0] = (uint8_t)(addr >> 24) ;
    buf[1] = (uint8_t)(addr >> 16);
    buf[2] = (uint8_t)(addr >> 8);
    buf[3] = (uint8_t)(addr >> 0);

    buf[4] = (uint8_t)(size >> 24);
    buf[5] = (uint8_t)(size >> 16);
    buf[6] = (uint8_t)(size >> 8);
    buf[7] = (uint8_t)(size >> 0);

    Serial.printf("update baud\n");
    Serial2.updateBaudRate(1000000);         // Increase baud rate
    Serial.printf("erase flash\n");
    zbEraseFlash(0,size);

    Serial.printf("send start\n");
    if(zbSendCommand(0x21,buf,8,2))
    {
        if(zbGetStatus(buf, sizeof(buf), 1))
        {
            //Serial.printf("flash read status: %02X\n",buf[0]);
            return (buf[0] == 0x40);
        }
    }
    return false;
 }

bool zbSendSync(uint32_t timeout)
{
    uint8_t buf[2]={0x55,0x55};
    Serial2.write(buf,2);
    return zbWaitForAck(timeout);
}


bool zbEraseFlash(uint32_t start,uint32_t size)
{
    if(!bslActive)
    {
        CCTool.enterBSL();
        bslActive = 1;
    }
    if(zbSendSync(3)) 
    {
        return zbSendCommand(0x2c,0,0,5);
    }
    return false;
}
