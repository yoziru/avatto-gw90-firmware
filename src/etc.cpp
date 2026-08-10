#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <FS.h>
#include <LittleFS.h>
#include <ETH.h>
#include <CCTools.h>

#include "config.h"
#include "web.h"
#include "log.h"
#include "etc.h"
#include "zones.h"

extern struct ConfigSettingsStruct ConfigSettings;

extern CCTools CCTool;

const char *coordMode = "coordMode";         // coordMode node name
const char *configFileSystem = "/config/system.json";
const char *configFileWifi = "/config/configWifi.json";
const char *configFileEther = "/config/configEther.json";
const char *configFileGeneral = "/config/configGeneral.json";
const char *configFileSecurity = "/config/configSecurity.json";
const char *configFileSerial = "/config/configSerial.json";
const char *configFileMqtt = "/config/configMqtt.json";
const char *configFileWg = "/config/configWg.json";
const char *deviceModel = "AVATTO-GW90-TI";

void getReadableTime(String &readableTime, unsigned long beginTime)
{
  unsigned long currentMillis;
  unsigned long seconds;
  unsigned long minutes;
  unsigned long hours;
  unsigned long days;
  currentMillis = millis() - beginTime;
  seconds = currentMillis / 1000;
  minutes = seconds / 60;
  hours = minutes / 60;
  days = hours / 24;
  currentMillis %= 1000;
  seconds %= 60;
  minutes %= 60;
  hours %= 24;

  readableTime = String(days) + " d ";

  if (hours < 10)
  {
    readableTime += "0";
  }
  readableTime += String(hours) + ":";

  if (minutes < 10)
  {
    readableTime += "0";
  }
  readableTime += String(minutes) + ":";

  if (seconds < 10)
  {
    readableTime += "0";
  }
  readableTime += String(seconds) + "";
}

float readtemp(bool clear)
{
  if (clear == true)
  {
    return (temprature_sens_read() - 32) / 1.8;
  }
  else
  {
    return (temprature_sens_read() - 32) / 1.8 - ConfigSettings.tempOffset;
  }
}

// The ESP32 TSENS shares the SAR ADC with the RF PHY. On ESP-IDF 5.x the radio
// stays in modem-sleep after esp_wifi_start(), so temprature_sens_read()
// returns the fixed 0x80 sentinel (53.3°C). A real scan forces the PHY up and
// clocks the ADC, giving a live reading.
//
// The read is performed asynchronously by a small state machine driven from
// loop() (tempSensorLoop). getCPUtemp() only ever returns the cached value, so
// a page load or MQTT publish never blocks the main loop for the ~1.3 s the
// radio power-up + scan takes (that would stall the web server, the Zigbee
// socket bridge and MQTT all at once). We refresh at most every
// kTempReadIntervalMs to limit 2.4 GHz RF noise near the Zigbee coordinator.
static const uint32_t kTempReadIntervalMs = 15000;
static const uint32_t kTempSettleMs = 1200;
static const uint8_t kTempSampleCount = 5;

static float cachedTemp = 0.0f;
static uint32_t lastTempReadMs = 0;

enum class TempReadStage : uint8_t {
    Idle,       // no read in progress
    WaitSettle, // radio powered on, waiting for the PHY/PLL to stabilize
    Sampling,   // collecting kTempSampleCount samples spaced ~20 ms apart
    TearDown    // radio powered down, finalizing the cached result
};
static TempReadStage tempStage = TempReadStage::Idle;
static uint32_t tempStageStartMs = 0;
static uint8_t tempSamples[kTempSampleCount] = {0};
static uint8_t tempSampleIdx = 0;
static bool tempPowered = false;

// Median of a 5-sample buffer (rejects outliers from the coarse TSENS).
static uint8_t median5(uint8_t s[5])
{
  for (int i = 0; i < 4; i++)
  {
    for (int j = i + 1; j < 5; j++)
    {
      if (s[j] < s[i])
      {
        uint8_t t = s[i];
        s[i] = s[j];
        s[j] = t;
      }
    }
  }
  return s[2];
}

// Synchronous read — only used by boot-time calibration (getCPUtemp(true)),
// where blocking is fine because setup() is still running and nothing else
// needs the loop.
static float readTempSync(bool clear)
{
  float result = 0.0f;
  bool wifiWasOn = (WiFi.getMode() == WIFI_MODE_STA || WiFi.getMode() == WIFI_MODE_APSTA);
  if (!wifiWasOn)
  {
    WiFi.mode(WIFI_STA);
    if (esp_wifi_start() == ESP_OK)
    {
      WiFi.setSleep(false);
      WiFi.scanNetworks(true);   // async scan forces the radio fully on
      delay(kTempSettleMs);
      uint8_t samples[kTempSampleCount] = {0};
      for (int i = 0; i < kTempSampleCount; i++)
      {
        samples[i] = temprature_sens_read();
        delay(20);
      }
      result = (median5(samples) - 32) / 1.8f;
      WiFi.scanDelete();
      esp_wifi_stop();
      WiFi.mode(WIFI_OFF);
    }
  }
  else
  {
    result = readtemp(clear);
  }
  return result;
}

// Kick off a background read if the cache is stale. Never blocks.
static void tempSensorRefresh()
{
  if (tempStage != TempReadStage::Idle)
  {
    return;
  }
  const uint32_t now = millis();
  if (cachedTemp != 0.0f && (now - lastTempReadMs) < kTempReadIntervalMs)
  {
    return;
  }
  tempStageStartMs = now;
  tempSampleIdx = 0;
  if (WiFi.getMode() == WIFI_MODE_STA || WiFi.getMode() == WIFI_MODE_APSTA)
  {
    // Radio already powered (STA fallback active): sample immediately.
    tempStage = TempReadStage::Sampling;
  }
  else
  {
    WiFi.mode(WIFI_STA);
    if (esp_wifi_start() != ESP_OK)
    {
      tempStage = TempReadStage::Idle;
      return;
    }
    WiFi.setSleep(false);
    WiFi.scanNetworks(true);
    tempPowered = true;
    tempStage = TempReadStage::WaitSettle;
  }
}

// Advance the async temp read. Call frequently from loop(); returns quickly.
void tempSensorLoop()
{
  const uint32_t now = millis();
  switch (tempStage)
  {
  case TempReadStage::Idle:
    tempSensorRefresh();
    break;
  case TempReadStage::WaitSettle:
    if (now - tempStageStartMs >= kTempSettleMs)
    {
      tempStageStartMs = now;
      tempStage = TempReadStage::Sampling;
    }
    break;
  case TempReadStage::Sampling:
    if (tempSampleIdx >= kTempSampleCount)
    {
      tempStage = TempReadStage::TearDown;
      break;
    }
    if (now - tempStageStartMs >= 20u * tempSampleIdx)
    {
      tempSamples[tempSampleIdx++] = temprature_sens_read();
    }
    break;
  case TempReadStage::TearDown:
    if (tempPowered)
    {
      WiFi.scanDelete();
      esp_wifi_stop();
      WiFi.mode(WIFI_OFF);
      tempPowered = false;
    }
    {
      uint8_t med = median5(tempSamples);
      cachedTemp = (med - 32) / 1.8f - ConfigSettings.tempOffset;
      lastTempReadMs = now;
      if (med == 0x80)
      {
        printLogMsg("CPUtemp: sensor not powered (0x80 sentinel)");
      }
      else
      {
        printLogMsg(String("CPUtemp raw=") + String(tempSamples[0], HEX) + ".." + String(tempSamples[4], HEX) +
                    " t=" + String(cachedTemp));
      }
    }
    tempStage = TempReadStage::Idle;
    break;
  }
}

float getCPUtemp(bool clear)
{
  DEBUG_PRINTLN(F("getCPUtemp"));
  if (clear)
  {
    // Boot-time calibration only — a synchronous blocking read is acceptable
    // here because setup() hasn't started serving yet.
    float raw = readTempSync(true);
    cachedTemp = raw;
    lastTempReadMs = millis();
    return raw;
  }
  // Normal path: never block the caller. Kick off a background refresh if the
  // cache is stale and return whatever we have.
  tempSensorRefresh();
  return cachedTemp;
}

void zigbeeRouterRejoin()
{
  printLogMsg("Router rejoin begin");
  DEBUG_PRINTLN(F("Router rejoin begin"));
  CCTool.routerRejoin();
  printLogMsg("Router in join mode!");
  DEBUG_PRINTLN(F("Router in join mode!"));
}

void zigbeeEnableBSL()
{
  printLogMsg("ZB enable BSL");
  DEBUG_PRINTLN(F("ZB enable BSL"));
  CCTool.enterBSL();
  printLogMsg("Now you can flash CC2652!");
  DEBUG_PRINTLN(F("Now you can flash CC2652!"));
}

void zigbeeRestart()
{
  printLogMsg("ZB RST begin");
  DEBUG_PRINTLN(F("ZB RST begin"));
  CCTool.restart();
  printLogMsg("ZB restart was done");
  DEBUG_PRINTLN(F("ZB restart was done"));
}

void ledPowerToggle()
{
  printLogMsg("BLUE LED has been toggled");
  DEBUG_PRINTLN(F("BLUE LED has been toggled"));
  digitalWrite(LED_PWR, !digitalRead(LED_PWR));
}

void ledUSBToggle()
{
  printLogMsg("RED LED has been toggled");
  DEBUG_PRINTLN(F("RED LED has been toggled"));
  digitalWrite(LED_USB, !digitalRead(LED_USB));
}

void getDeviceID(char *arr)
{
  uint64_t mac = ESP.getEfuseMac();
  uint8_t a = 0;
  uint8_t b = 0;
  a ^= mac >> 8 * 0;
  a ^= mac >> 8 * 1;
  a ^= mac >> 8 * 2;
  a ^= mac >> 8 * 3;
  b ^= mac >> 8 * 4;
  b ^= mac >> 8 * 5;
  b ^= mac >> 8 * 6;
  b ^= mac >> 8 * 7;

  char buf[20];

  sprintf(buf, "%02x%02x", a, b);

  for (uint8_t cnt = 0; cnt < strlen(buf); cnt++)
  {
    buf[cnt] = toupper(buf[cnt]);
  }

  sprintf(arr, "%s-%s", deviceModel, buf);
}

// void writeDefaultConfig(const char *path, String StringConfig)
// {
//   DEBUG_PRINTLN(path);
//   DEBUG_PRINTLN(F("failed open. try to write defaults"));
//   DEBUG_PRINTLN(StringConfig);

//   JsonDocument doc;
//   deserializeJson(doc, StringConfig);

//   File configFile = LittleFS.open(path, FILE_WRITE);
//   if (!configFile)
//   {
//     DEBUG_PRINTLN(F("failed write"));
//     //return false;
//   }
//   else
//   {
//     serializeJson(doc, configFile);
//   }
//   configFile.close();
// }

void writeDefaultConfig(const char *path, JsonDocument &doc)
{
  DEBUG_PRINTLN(path);
  DEBUG_PRINTLN(F("Write defaults"));

  File configFile = LittleFS.open(path, FILE_WRITE);
  if (!configFile)
  {
    DEBUG_PRINTLN(F("Failed Write"));
    // return false;
  }
  else
  {
    serializeJson(doc, configFile);
  }
  configFile.close();
}

void resetSettings()
{
  DEBUG_PRINTLN(F("[resetSettings] Start"));
  digitalWrite(LED_USB, 1);
  digitalWrite(LED_PWR, 0);
  for (uint8_t i = 0; i < 15; i++)
  {
    delay(200);
    digitalWrite(LED_USB, !digitalRead(LED_USB));
    digitalWrite(LED_PWR, !digitalRead(LED_PWR));
  }
  DEBUG_PRINTLN(F("[resetSettings] Led blinking done"));
  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED, "/lfs2", 10))
  {
    DEBUG_PRINTLN(F("Error with LITTLEFS"));
  }
  LittleFS.remove(configFileSerial);
  LittleFS.remove(configFileSecurity);
  LittleFS.remove(configFileGeneral);
  LittleFS.remove(configFileEther);
  LittleFS.remove(configFileWifi);
  LittleFS.remove(configFileSystem);
  LittleFS.remove(configFileWg);
  DEBUG_PRINTLN(F("[resetSettings] Config del done"));
  ESP.restart();
}

void setClock()
{
  configTime(0, 0, "pool.ntp.org", "time.google.com");

  DEBUG_PRINT(F("Waiting for NTP time sync: "));
  int startTryingTime = millis();
  DEBUG_PRINTLN(startTryingTime);
  startTryingTime = startTryingTime / 1000;
  time_t nowSecs = time(nullptr);
  DEBUG_PRINTLN(nowSecs);
  while ((nowSecs - startTryingTime) < 60)
  {
    delay(500);
    DEBUG_PRINT(F("."));
    yield();
    nowSecs = time(nullptr);
  }

  DEBUG_PRINTLN();
  struct tm timeinfo;
  localtime_r(&nowSecs, &timeinfo);
  DEBUG_PRINT(F("Current GMT time: "));
  DEBUG_PRINT(asctime(&timeinfo));

  char *zoneToFind = const_cast<char *>("Europe/Kiev");
  if (ConfigSettings.timeZone)
  {
    zoneToFind = ConfigSettings.timeZone;
  }
  const char *gmtOffset = getGmtOffsetForZone(zoneToFind);

  String timezone = "EET-2EEST,M3.5.0/3,M10.5.0/4";

  if (gmtOffset != nullptr)
  {
    DEBUG_PRINT(F("GMT Offset for "));
    DEBUG_PRINT(zoneToFind);
    DEBUG_PRINT(F(" is "));
    DEBUG_PRINTLN(gmtOffset);
    timezone = gmtOffset;
    setTimezone(timezone);
  }
  else
  {
    DEBUG_PRINT(F("GMT Offset for "));
    DEBUG_PRINT(zoneToFind);
    DEBUG_PRINTLN(F(" not found."));
  }
}

void setTimezone(String timezone)
{
  DEBUG_PRINTLN(F("Setting Timezone"));
  setenv("TZ", timezone.c_str(), 1); //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
  time_t nowSecs = time(nullptr);
  struct tm timeinfo;
  localtime_r(&nowSecs, &timeinfo);

  String timeNow = asctime(&timeinfo);
  timeNow.remove(timeNow.length() - 1);
  DEBUG_PRINT(F("Local time: "));
  DEBUG_PRINTLN(timeNow);
  printLogMsg("Local time: " + timeNow);
}
