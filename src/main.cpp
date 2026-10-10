#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <Update.h>
#include <Ticker.h>
#include <esp_wifi.h>
#include <ETH.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <CCTools.h>

#include "boot_guard.h"
#include "config.h"
#include "net.h"
#include "bridge.h"
#include "web.h"
#include "log.h"
#include "etc.h"
#include "mqtt.h"
#include "zb.h"
#include "zb_info.h"
#include "version.h"

#ifdef BUILD_DEBUG
#include "profiling.h"
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

SET_LOOP_TASK_STACK_SIZE(16384); // HTTPS OTA runs on loopTask, including TLS verification

#ifdef ETH_CLK_MODE
#undef ETH_CLK_MODE
#endif

volatile uint8_t btnFlag = 0;

void handleLongBtn();
void setupCoordinatorMode();

Ticker tmrBtnLongPress(handleLongBtn, 1000, 0, MILLIS);

extern CCTools CCTool;

#ifdef BUILD_FACTORY_TEST
#include "factory.h"
#endif

IRAM_ATTR bool debounce()
{
  volatile static unsigned long lastFire = 0;
  if (millis() - lastFire < DEBOUNCE_TIME)
  { // Debounce
    return 0;
  }
  lastFire = millis();
  return 1;
}

IRAM_ATTR void btnInterrupt()
{
  if (debounce())
    btnFlag = true;
}

void setLedsDisable(bool mode, bool setup)
{
  Serial.println("\n========== LED CONTROL ==========");
  Serial.print("[LED] Mode: ");
  Serial.println(mode ? "DISABLE ALL" : "ENABLE");
  Serial.print("[LED] Setup phase: ");
  Serial.println(setup ? "Yes" : "No");
  
  if (!setup)
  {
    Serial.println("[LED] Saving LED preferences to config...");
    const char *path = configFileGeneral;
    JsonDocument doc;
    File configFile = LittleFS.open(path, FILE_READ);
    deserializeJson(doc, configFile);
    configFile.close();
    doc["disableLeds"] = mode;
    doc["disableLedPwr"] = mode;
    doc["disableLedUSB"] = mode;
    configFile = LittleFS.open(path, FILE_WRITE);
    serializeJson(doc, configFile);
    configFile.close();
    ConfigSettings.disableLeds = mode;
    ConfigSettings.disableLedPwr = mode;
    ConfigSettings.disableLedUSB = mode;
  }
  
  if (mode)
  {
    Serial.println("[LED] Turning OFF all LEDs");
    digitalWrite(LED_USB, LOW);
    digitalWrite(LED_PWR, LOW);
    Serial.println("[LED] LED_USB (Red): OFF");
    Serial.println("[LED] LED_PWR (Blue): OFF");
  }
  else
  {
    // Blue Power LED
    if (!ConfigSettings.disableLedPwr)
    {
      digitalWrite(LED_PWR, HIGH);
      Serial.println("[LED] LED_PWR (Blue): ON");
    }
    else
    {
      digitalWrite(LED_PWR, LOW);
      Serial.println("[LED] LED_PWR (Blue): OFF (disabled in config)");
    }
    
    // Red USB LED - only on in SERIAL mode
    if (ConfigSettings.coordinator_mode == CoordinatorMode::PASSTHROUGH && !ConfigSettings.disableLedUSB)
    {
      digitalWrite(LED_USB, HIGH);
      Serial.println("[LED] LED_USB (Red): ON (serial mode)");
    }
    else
    {
      digitalWrite(LED_USB, LOW);
      if (ConfigSettings.coordinator_mode == CoordinatorMode::PASSTHROUGH) {
        Serial.println("[LED] LED_USB (Red): OFF (disabled in config)");
      } else {
        const char* modeNames[] = {"Network", "Serial"};
        Serial.print("[LED] LED_USB (Red): OFF (not in serial mode, current: ");
        Serial.print(modeNames[static_cast<size_t>(ConfigSettings.coordinator_mode)]);
        Serial.println(")");
      }
    }
  }
  Serial.println("================================\n");
}

void handleLongBtn()
{
  if (!digitalRead(BTN))
  { // long press
    DEBUG_PRINT(F("Long press "));
    DEBUG_PRINT(btnFlag);
    DEBUG_PRINTLN(F("s"));
    if (btnFlag >= 4)
    {
      printLogMsg("Long press 4sec - zigbeeEnableBSL");
      zigbeeEnableBSL();
      tmrBtnLongPress.stop();
      btnFlag = false;
    }
    else
      btnFlag = static_cast<uint8_t>(btnFlag + 1);
  }
  else
  { // stop long press
    if (btnFlag >= 2)
    {
      printLogMsg("Long press 2sec - setLedsDisable");
      setLedsDisable(!ConfigSettings.disableLeds, false);
    }
    tmrBtnLongPress.stop();
    btnFlag = false;
    printLogMsg("Stop long press");
  }
}

void toggleSerialMode()
{
  if (ConfigSettings.zbFlashing) return;
  Serial.println("\n========== MODE TOGGLE TRIGGERED ==========");
  
  const char* modeNames[] = {"Network", "Serial"};
  Serial.print("[MODE] Current Mode: ");
  Serial.print(modeNames[static_cast<size_t>(ConfigSettings.coordinator_mode)]);
  Serial.print(" (");
  Serial.print(static_cast<uint8_t>(ConfigSettings.coordinator_mode));
  Serial.println(")");
  
  if (ConfigSettings.coordinator_mode != CoordinatorMode::PASSTHROUGH)
  {
    Serial.print("[MODE] Switching from ");
    Serial.print(modeNames[static_cast<size_t>(ConfigSettings.coordinator_mode)]);
    Serial.println(" to Serial mode");
    ConfigSettings.coordinator_mode = CoordinatorMode::PASSTHROUGH;
  }
  else
  {
    Serial.println("[MODE] Switching from Serial to Network mode");
    ConfigSettings.coordinator_mode = CoordinatorMode::NETWORK;
  }
  
  Serial.print("[MODE] New Mode: ");
  Serial.print(modeNames[static_cast<size_t>(ConfigSettings.coordinator_mode)]);
  Serial.print(" (");
  Serial.print(static_cast<uint8_t>(ConfigSettings.coordinator_mode));
  Serial.println(")");
  
  // Save configuration
  const char *path = configFileGeneral;
  JsonDocument doc;
  File configFile = LittleFS.open(path, FILE_READ);
  deserializeJson(doc, configFile);
  configFile.close();
  doc[coordMode] = static_cast<uint8_t>(ConfigSettings.coordinator_mode);
  configFile = LittleFS.open(path, FILE_WRITE);
  serializeJson(doc, configFile);
  configFile.close();
  
  Serial.println("[MODE] Configuration saved");
  Serial.println("[MODE] Restarting device...");
  Serial.println("==========================================\n");
  
  digitalWrite(LED_USB, ConfigSettings.coordinator_mode == CoordinatorMode::PASSTHROUGH ? 1 : 0);
  ESP.restart();
}

void setupCoordinatorMode()
{
  Serial.println(F("\n======== setupCoordinatorMode() ========"));
  if (ConfigSettings.coordinator_mode > CoordinatorMode::PASSTHROUGH || ConfigSettings.coordinator_mode < CoordinatorMode::NETWORK)
  {
    Serial.println(F("[MODE] WRONG MODE DETECTED, set to NETWORK"));
    ConfigSettings.coordinator_mode = CoordinatorMode::NETWORK;
  }
  const char* modeNames[] = {"Network", "Serial"};
  Serial.print(F("[MODE] Mode: ")); Serial.print(modeNames[static_cast<size_t>(ConfigSettings.coordinator_mode)]);
  Serial.print(F(" (")); Serial.print(static_cast<uint8_t>(ConfigSettings.coordinator_mode)); Serial.println(F(")"));
  Serial.print(F("[MODE] keepWeb=")); Serial.println(ConfigSettings.keepWeb);
  Serial.print(F("[MODE] disableWeb=")); Serial.println(ConfigSettings.disableWeb);
  if (networkExpected())
  { // start network overseer
    Serial.println(F("[MODE] Registering WiFi event & starting overseer timer"));
    if (tmrNetworkOverseer.state() == STOPPED)
    {
      tmrNetworkOverseer.start();
      Serial.println(F("[MODE] tmrNetworkOverseer started"));
    }
    WiFi.onEvent(NetworkEvent);
    Serial.println(F("[MODE] NetworkEvent registered"));
  }
  else
  {
    Serial.println(F("[MODE] SERIAL mode without keepWeb - no network overseer"));
  }
  switch (ConfigSettings.coordinator_mode)
  {
  case CoordinatorMode::PASSTHROUGH:
    Serial.println(F("[MODE] -> SERIAL: MODE_SWITCH=HIGH"));
    digitalWrite(MODE_SWITCH, 1);
    break;

  case CoordinatorMode::NETWORK:
    Serial.println(F("[MODE] -> NETWORK: calling initLan()"));
    initLan();
    break;

  default:
    break;
  }
  if (ConfigSettings.coordinator_mode == CoordinatorMode::PASSTHROUGH && ConfigSettings.keepWeb)
  {
    Serial.println(F("[MODE] SERIAL+keepWeb: trying connectWifi()"));
    connectWifi();
  }
  Serial.println(F("======== setupCoordinatorMode() END ========\n"));
}

void setup()
{
  bootGuardBegin(); // enforce retry budget before any setup work can crash
  // PHY warm-restart workaround: ESP.restart() doesn't drop the 3.3V rail (esp. PoE),
  // so LAN8720 POR never fires. A 350ms delay at startup lets strapping pins settle.
  delay(350);

  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n========== DEVICE STARTING ==========");
  Serial.println("Firmware version: " VERSION);
  Serial.println("\n[DEBUG] Serial Monitor Ready");
  Serial.println("[DEBUG] Baud Rate: 115200");
  Serial.println("[DEBUG] Use Serial Monitor to view mode switching and system logs");
  Serial.println("[DEBUG] ==========================================\n");

  ConfigSettings.apStarted = false;
  ConfigSettings.serialSpeed = 115200;
  DEBUG_PRINTLN(F("Start"));
  pinMode(CC2652P_RST, OUTPUT);
  pinMode(CC2652P_FLASH, OUTPUT);
  digitalWrite(CC2652P_RST, 1);
  digitalWrite(CC2652P_FLASH, 1);
  pinMode(LED_PWR, OUTPUT);
  pinMode(LED_USB, OUTPUT);
  pinMode(BTN, INPUT);
  pinMode(MODE_SWITCH, OUTPUT);
  digitalWrite(MODE_SWITCH, 0); // enable zigbee serial
  digitalWrite(LED_PWR, 1);
  digitalWrite(LED_USB, 1);

// hard reset
#if !defined(BUILD_DEBUG) && !defined(BUILD_FACTORY_TEST)
  if (!digitalRead(BTN))
  {
    DEBUG_PRINTLN(F("[hard reset] Entering hard reset mode"));
    uint8_t counter = 0;
    while (!digitalRead(BTN))
    {
      if (counter >= 10)
      {
        resetSettings();
      }
      else
      {
        counter++;
        DEBUG_PRINTLN(counter);
        delay(200);
      }
    }
    DEBUG_PRINTLN(F("[hard reset] Btn up, exit"));
  }
#endif // !BUILD_DEBUG && !BUILD_FACTORY_TEST
  //--------------------

  // zig connection & leds testing
  Serial2.begin(115200, SERIAL_8N1, CC2652P_RXD, CC2652P_TXD); // start zigbee serial

  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED, "/lfs2", 10))
  {
    DEBUG_PRINTLN(F("Error with LITTLEFS"));
    return;
  }

  DEBUG_PRINTLN(F("LITTLEFS OK"));
  if (!LittleFS.exists("/config") && !LittleFS.mkdir("/config"))
  {
    DEBUG_PRINTLN(F("Error creating config dir"));
    ESP.restart();
  }

  if (loadZbInfo())
  {
    DEBUG_PRINTLN(F("Zigbee info cache load OK"));
  }

  if (!loadConfigSerial())
  {
    DEBUG_PRINTLN(F("Error load config serial"));
    ESP.restart();
  }
  else
  {
    DEBUG_PRINTLN(F("Config serial load OK"));
  }

  if ((!loadConfigWifi()) || (!loadConfigEther()) || (!loadConfigGeneral()) || (!loadConfigSecurity()) || (!loadConfigMqtt()) || (!loadConfigWg()))
  {
    DEBUG_PRINTLN(F("Error load config files"));
    ESP.restart();
  }
  else
  {
    DEBUG_PRINTLN(F("Config files load OK"));
  }

  zbInit();
#ifdef BUILD_FACTORY_TEST
  if (!digitalRead(BTN) && ConfigSettings.coordinator_mode != CoordinatorMode::PASSTHROUGH)
  {
    factory_test();
  }
#endif
  //-----------------
  attachInterrupt(digitalPinToInterrupt(BTN), btnInterrupt, FALLING);

  Serial.println("\n[INIT] Configuring LED initial state...");
  Serial.print("[INIT] disableLeds config: ");
  Serial.println(ConfigSettings.disableLeds ? "true" : "false");
  setLedsDisable(ConfigSettings.disableLeds, true);
  setupCoordinatorMode();
  ConfigSettings.connectedClients = 0;

  if (MqttSettings.enable)
  {
    mqttConnectSetup();
  }

  DEBUG_PRINTLN(millis());

  Serial2.updateBaudRate(ConfigSettings.serialSpeed); // set actual speed

  printf("[INIT] Serial speed: %d\n",ConfigSettings.serialSpeed);

  printLogMsg("Setup done");

  char deviceIdArr[20];
  getDeviceID(deviceIdArr, sizeof(deviceIdArr));

  DEBUG_PRINTLN(String(deviceIdArr));
  printLogMsg(String(deviceIdArr));

}


void loop(void)
{
#ifdef BUILD_DEBUG
  const uint64_t loopStartUs = profilingLoopBegin();
#endif

  if (btnFlag)
  {
    if (!digitalRead(BTN))
    { // pressed
      if (tmrBtnLongPress.state() == STOPPED)
      {
        tmrBtnLongPress.start();
      }
    }
    else
    {
      if (tmrBtnLongPress.state() == RUNNING)
      {
        btnFlag = false;
        tmrBtnLongPress.stop();
        toggleSerialMode();
      }
    }
  }

  tmrBtnLongPress.update();
  tmrNetworkOverseer.update();
  tmrZbHealth.update();
  bootGuardLoop();
  tempSensorLoop();     // non-blocking async temp read (no 1.3s stall in handlers)
  zbInfoRefreshLoop();  // post-flash version/role re-detection (bounded retries)
  wifiCommitPending(); // commit a staged wifi save once the STA link is up
  // Serve the web server whenever it has been started. handleClient() is a
  // no-op when disableWeb/serial-only means it never ran; the old
  // "only when no socket clients" gate deprioritized the UI behind the bridge
  // for no benefit and made disableWeb ineffective.
  webServerHandleClient();

  if (ConfigSettings.coordinator_mode != CoordinatorMode::PASSTHROUGH)
  {
    processSocketBridge();

    if (MqttSettings.enable)
    {
      mqttLoop();
    }
  }

  if (WiFi.getMode() == WIFI_MODE_AP || WiFi.getMode() == WIFI_MODE_APSTA)
  {
    dnsServer.processNextRequest();
  }

  delay(1); // yield to the RTOS scheduler; prevents a 100% CPU busy-spin

#ifdef BUILD_DEBUG
  profilingLoopTick(loopStartUs);
#endif
}



