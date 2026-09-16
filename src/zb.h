#ifndef ZB_H_
#define ZB_H_

void clearS2Buffer();
void getZbVer();
void zbCheck();
void zbHealthCheck();
void zbLedToggle();

// True when the installed role was confirmed by a live coordinator response in
// the current boot. A cached role is display-only and must not authorize
// Zigbee flashing (see API_FLASH_ZB).
bool zbRoleDetectedThisBoot();

// Post-flash re-detection: the ZB firmware task requests it after a successful
// flash, loop() performs the bounded retries (Serial2 is owned by the task
// while flashing, so the UART reads must not happen there).
void zbRequestInfoRefresh();
void zbInfoRefreshLoop();

// Coordinator health metrics (for the web UI).
unsigned long zbGetLastRxMs();
unsigned long zbGetLastClientTxMs();
uint32_t zbGetAutoResetCount();

// Written by zb.cpp/health and the socket bridge (bridge.cpp).
extern volatile unsigned long zbLastRxMs;
extern volatile unsigned long zbClientTxMs;
int8_t checkFwHex(const char *tempFile);
int8_t runFlash(const char *hexFilePath,uint32_t binsize);
void zbInit();

bool zbSendCommand(uint8_t cmd, uint8_t *data, uint8_t len ,uint8_t timeout);
bool zbGetStatus(uint8_t *status, uint8_t statusLen, uint32_t timeout);
bool zbCheckLastCmd(void);
bool zbFlashStart(uint32_t addr,uint32_t flashSize);
bool zbEraseFlash(uint32_t start,uint32_t size);
bool zbSendSync(uint32_t timeout);

#endif
