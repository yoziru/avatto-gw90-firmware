#include <ArduinoJson.h>

void getReadableTime(String &readableTime, unsigned long beginTime);

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

#define STRINGIFY(s) STRINGIFY1(s)
#define STRINGIFY1(s) #s

float getCPUtemp();
void tempSensorLoop();

void zigbeeRouterRejoin();
void zigbeeEnableBSL();
void zigbeeRestart();

void ledPowerToggle();
void ledUSBToggle();

void getDeviceID(char *arr, size_t capacity);
void writeDefaultConfig(const char *path, JsonDocument& doc);

void resetSettings();

void setClock();
void setTimezone(const String &timezone);
