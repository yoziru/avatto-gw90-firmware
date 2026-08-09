#include <WiFi.h>
#include <ETH.h>

#include "config.h"
#include "factory.h"

#ifdef BUILD_FACTORY_TEST

const byte ZIGBEE_LED_ON[] = {0xFE, 0x02, 0x27, 0x0A, 0x01, 0x01, 0x2F};

#ifndef FACTORY_TEST_SSID
#define FACTORY_TEST_SSID "ezsmart_factory_test"
#endif
#ifndef FACTORY_TEST_PASS
#define FACTORY_TEST_PASS "ezsmart@2026"
#endif
#ifndef FACTORY_TEST_HOST
#define FACTORY_TEST_HOST "www.baidu.com"
#endif

TaskHandle_t xTestTaskHandle = NULL;

void factory_initLan()
{
  Serial.println(F("\n[LAN] ======== initLan() START ========"));
  Serial.print(F("[LAN] ETH_ADDR=")); Serial.print(ETH_ADDR_1);
  Serial.print(F(" PWR_PIN=")); Serial.print(ETH_POWER_PIN_1);
  Serial.print(F(" MDC=")); Serial.print(ETH_MDC_PIN_1);
  Serial.print(F(" MDIO=")); Serial.print(ETH_MDIO_PIN_1);
  Serial.print(F(" PWR_ALT=")); Serial.println(ETH_POWER_PIN_ALTERNATIVE_1);
  Serial.print(F("[LAN] DHCP=")); Serial.println(ConfigSettings.dhcp);

  // Hardware reset LAN8720 PHY via GPIO5 (same fix as initLan())
  pinMode(ETH_POWER_PIN_ALTERNATIVE_1, OUTPUT);
  digitalWrite(ETH_POWER_PIN_ALTERNATIVE_1, LOW);
  delay(200);
  digitalWrite(ETH_POWER_PIN_ALTERNATIVE_1, HIGH);
  // No delay — EMAC clock starts inside ETH.begin()

  if (ETH.begin(ETH_ADDR_1, ETH_POWER_PIN_1, ETH_MDC_PIN_1, ETH_MDIO_PIN_1, ETH_TYPE_1, ETH_CLK_MODE_1, ETH_POWER_PIN_ALTERNATIVE_1))
  {
    Serial.println(F("[LAN] ETH.begin() SUCCESS"));
    Serial.println(F("[LAN] Using DHCP"));
  }
  else
  {
    Serial.println(F("[LAN] ETH.begin() FAILED!"));
  }
  Serial.println(F("[LAN] ======== initLan() END ========\n"));
}


void testTaskFunction(void *pvParameters)
{
  Serial.println("[TEST TASK] Test task started!");

  // ========== Connecting to WiFi AP ==========
  Serial.println("\n[TEST TASK] ========== Connecting to WiFi AP ==========");
  
  const char* testSSID = FACTORY_TEST_SSID;
  const char* testPASS = FACTORY_TEST_PASS;
  // Turn off indicator LEDs
  digitalWrite(LED_PWR, 0);
  digitalWrite(LED_USB, 1);

  // Set WiFi mode to STA
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  Serial.print("[WIFI CONNECT] Connecting to SSID: ");
  Serial.println(testSSID);
  
  WiFi.begin(testSSID, testPASS);
  
  // Wait for connection, try up to 20 times
  uint8_t connectAttempts = 0;
  bool wifiConnected = false;
  
  while (connectAttempts < 20)
  {
    delay(500);
    connectAttempts++;
    Serial.print("[WIFI CONNECT] Attempt ");
    Serial.print(connectAttempts);
    Serial.print(" - Status: ");
    
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("CONNECTED");
      Serial.print("[WIFI CONNECT] Local IP: ");
      Serial.println(WiFi.localIP());
      
      // Get real-time RSSI after connection
      int32_t currentRSSI = WiFi.RSSI();
      Serial.print("[WIFI CONNECT] RSSI: ");
      Serial.print(currentRSSI);
      Serial.println(" dBm");
      
      // Assess connection quality based on signal strength
      if (currentRSSI >= -50)
      {
        Serial.println("[TEST TASK] Signal: EXCELLENT");
      }
      else if (currentRSSI >= -60)
      {
        Serial.println("[TEST TASK] Signal: GOOD");
      }
      else if (currentRSSI >= -70)
      {
        Serial.println("[TEST TASK] Signal: FAIR");
      }
      else
      {
        Serial.println("[TEST TASK] Signal: WEAK");
      }
      
      wifiConnected = true;
      break;
    }
    else
    {
      Serial.print("Connecting... (");
      Serial.print(WiFi.status());
      Serial.println(")");
    }
  }
  
  if (!wifiConnected)
  {
    Serial.println("[WIFI CONNECT] Failed to connect to AP!");
    Serial.print("[WIFI CONNECT] Final Status: ");
    Serial.println(WiFi.status());
  }
  
  // ========== WiFi network test ==========
  Serial.println("\n[TEST TASK] ========== WIFI NETWORK TEST START ==========");
  
  uint32_t wifiTestCounter = 0;
  uint32_t wifiSuccessCount = 0;
  uint32_t wifiFailCount = 0;
  
  // Run 10 WiFi tests (~50 seconds)
  while (wifiTestCounter < 3)
  {
    wifiTestCounter++;
    
    Serial.println("\n========================================");
    Serial.println("         WIFI TEST                      ");
    Serial.println("========================================");
    Serial.print("[WIFI TEST] Test Count: ");
    Serial.println(wifiTestCounter);
    
    bool wifiTestConnected = false;
    
    // 1. Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("[WIFI TEST] WiFi Status: CONNECTED");
      Serial.print("[WIFI TEST] SSID: ");
      Serial.println(WiFi.SSID());
      Serial.print("[WIFI TEST] Local IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("[WIFI TEST] Subnet Mask: ");
      Serial.println(WiFi.subnetMask());
      Serial.print("[WIFI TEST] Gateway: ");
      Serial.println(WiFi.gatewayIP());
      Serial.print("[WIFI TEST] DNS Server: ");
      Serial.println(WiFi.dnsIP());
      
      // Get real-time RSSI value
      int32_t rssi = WiFi.RSSI();
      Serial.print("[WIFI TEST] Signal Strength (RSSI): ");
      Serial.print(rssi);
      Serial.println(" dBm");
      
      wifiTestConnected = true;
      wifiSuccessCount++;
    }
    else
    {
      Serial.print("[WIFI TEST] WiFi Status: DISCONNECTED (");
      Serial.print(WiFi.status());
      Serial.println(")");
      
      // Attempt to reconnect
      Serial.println("[WIFI TEST] Attempting to reconnect...");
      WiFi.reconnect();
      delay(2000);
      
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.println("[WIFI TEST] Reconnect SUCCESS");
        Serial.print("[WIFI TEST] RSSI: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        wifiTestConnected = true;
        wifiSuccessCount++;
      }
      else
      {
        Serial.println("[WIFI TEST] Reconnect FAILED");
        wifiFailCount++;
      }
    }
    
    // 2. WiFi network connectivity test
    if (wifiTestConnected)
    {
      Serial.println("[WIFI TEST] Testing WiFi network connectivity...");
      
      WiFiClient wifiTestClient;
      const char* testHost = FACTORY_TEST_HOST;
      const int testPort = 80;
      
      unsigned long startTime = millis();
      int connectResult = wifiTestClient.connect(testHost, testPort);
      unsigned long connectTime = millis() - startTime;
      
      if (connectResult)
      {
        Serial.print("[WIFI TEST] DNS Resolution: SUCCESS (");
        Serial.print(wifiTestClient.remoteIP());
        Serial.print(") | Connect Time: ");
        Serial.print(connectTime);
        Serial.println(" ms");
        Serial.print("[WIFI TEST] Used Interface: WiFi (");
        Serial.print(WiFi.localIP());
        Serial.println(")");
        wifiTestClient.stop();
      }
      else
      {
        Serial.println("[WIFI TEST] DNS Resolution: FAILED");
        Serial.println("[WIFI TEST] Possible causes:");
        Serial.println("[WIFI TEST]   - DNS server not configured");
        Serial.println("[WIFI TEST]   - Gateway unreachable");
        Serial.println("[WIFI TEST]   - No Internet access");
      }
    }
    
    // Statistics
    Serial.println("\n[WIFI TEST] ========== Statistics ==========");
    Serial.print("[WIFI TEST]   Total Checks: ");
    Serial.println(wifiTestCounter);
    Serial.print("[WIFI TEST]   Success Count: ");
    Serial.println(wifiSuccessCount);
    Serial.print("[WIFI TEST]   Fail Count: ");
    Serial.println(wifiFailCount);
    
    if (wifiTestCounter > 0)
    {
      float wifiRate = (float)wifiSuccessCount / wifiTestCounter * 100.0f;
      Serial.print("[WIFI TEST]   Success Rate: ");
      Serial.print(wifiRate);
      Serial.println("%");
    }
    
    Serial.println("========================================\n");
  }

  // ========== LAN network test ==========
  Serial.println("\n[TEST TASK] ========== LAN NETWORK TEST START ==========");
  
  uint32_t lanTestCounter = 0;
  uint32_t lanSuccessCount = 0;
  uint32_t lanFailCount = 0;
  
  factory_initLan();
  delay(1000);
  // Run 10 LAN tests (~50 seconds)
  while (lanTestCounter < 3)
  {
    lanTestCounter++;
    
    Serial.println("\n========================================");
    Serial.println("         LAN TEST                       ");
    Serial.println("========================================");
    Serial.print("[LAN TEST] Test Count: ");
    Serial.println(lanTestCounter);
    
    bool lanConnected = false;
    
    // 1. Check physical connection
     if (ETH.linkUp())
    {
        Serial.println("[LAN TEST] Physical Link: UP");
        Serial.print("[LAN TEST] Link Speed: ");
        Serial.print(ETH.linkSpeed());
        Serial.println(" Mbps");
        
        // Check IP configuration
        if (ETH.localIP() != IPAddress(0, 0, 0, 0))
        {
            Serial.println("[LAN TEST] ETH IP Status: VALID");
            Serial.print("[LAN TEST] Local IP: ");
            Serial.println(ETH.localIP());
            Serial.print("[LAN TEST] Subnet Mask: ");
            Serial.println(ETH.subnetMask());
            Serial.print("[LAN TEST] Gateway: ");
            Serial.println(ETH.gatewayIP());
            lanConnected = true;
            lanSuccessCount++;
        }
        else
        {
            Serial.println("[LAN TEST] ETH IP Status: INVALID");
            lanFailCount++;
        }
    }
    else
    {
        Serial.println("[LAN TEST] Physical Link: DOWN");
        Serial.println("[LAN TEST] Check Ethernet cable connection!");
        lanFailCount++;
    }
    
    // 3. LAN network connectivity test
    if (lanConnected)
    {
      Serial.println("[LAN TEST] Testing LAN network connectivity...");
      
      WiFiClient lanTestClient;
      const char* testHost = FACTORY_TEST_HOST;
      const int testPort = 80;
      
      unsigned long startTime = millis();
      int connectResult = lanTestClient.connect(testHost, testPort);
      unsigned long connectTime = millis() - startTime;
      
      if (connectResult)
      {
        Serial.print("[LAN TEST] DNS Resolution: SUCCESS (");
        Serial.print(lanTestClient.remoteIP());
        Serial.print(") | Connect Time: ");
        Serial.print(connectTime);
        Serial.println(" ms");
        Serial.print("[LAN TEST] Used Interface: ETH (");
        Serial.print(ETH.localIP());
        Serial.println(")");
        lanTestClient.stop();
      }
      else
      {
        Serial.println("[LAN TEST] DNS Resolution: FAILED");
        Serial.println("[LAN TEST] Possible causes:");
        Serial.println("[LAN TEST]   - DNS server not configured");
        Serial.println("[LAN TEST]   - Gateway unreachable");
        Serial.println("[LAN TEST]   - No Internet access");
      }
    }
    
    // Statistics
    Serial.println("\n[LAN TEST] ========== Statistics ==========");
    Serial.print("[LAN TEST]   Total Checks: ");
    Serial.println(lanTestCounter);
    Serial.print("[LAN TEST]   Success Count: ");
    Serial.println(lanSuccessCount);
    Serial.print("[LAN TEST]   Fail Count: ");
    Serial.println(lanFailCount);
    
    if (lanTestCounter > 0)
    {
      float lanRate = (float)lanSuccessCount / lanTestCounter * 100.0f;
      Serial.print("[LAN TEST]   Success Rate: ");
      Serial.print(lanRate);
      Serial.println("%");
    }
    
    Serial.println("========================================\n");
  }
  
  // ========== Final summary report ==========
  Serial.println("\n########################################################");
  Serial.println("#              FACTORY TEST FINAL REPORT              #");
  Serial.println("########################################################");
  
  Serial.println("\n[WIFI SUMMARY]");
  Serial.print("  Total Tests: ");
  Serial.println(wifiTestCounter);
  Serial.print("  Success: ");
  Serial.println(wifiSuccessCount);
  Serial.print("  Failed: ");
  Serial.println(wifiFailCount);
  if (wifiTestCounter > 0)
  {
    float wifiRate = (float)wifiSuccessCount / wifiTestCounter * 100.0f;
    Serial.print("  Success Rate: ");
    Serial.print(wifiRate);
    Serial.println("%");
  }
  
  Serial.println("\n[LAN SUMMARY]");
  Serial.print("  Total Tests: ");
  Serial.println(lanTestCounter);
  Serial.print("  Success: ");
  Serial.println(lanSuccessCount);
  Serial.print("  Failed: ");
  Serial.println(lanFailCount);
  if (lanTestCounter > 0)
  {
    float lanRate = (float)lanSuccessCount / lanTestCounter * 100.0f;
    Serial.print("  Success Rate: ");
    Serial.print(lanRate);
    Serial.println("%");
  }
  
  Serial.println("\n########################################################");
  Serial.println("#                  TEST COMPLETED                       #");
  Serial.println("########################################################\n");
  

  // Turn on ZIGBEE indicator
  Serial2.write(ZIGBEE_LED_ON, sizeof(ZIGBEE_LED_ON));      // D11 on
  // Turn on WiFi indicator
  if(wifiSuccessCount)digitalWrite(LED_PWR, 1);             // D9 on
  // Turn on LAN indicator
  if(lanSuccessCount)digitalWrite(LED_USB, 0);              // D12 on 

  // Enter idle loop after test completes
  while (1)
  {
    if(!digitalRead(BTN))
    {
      digitalWrite(LED_USB, 1);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      digitalWrite(LED_USB, 0);
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

void factory_test(void)
{
  // Scan WiFi networks
  Serial.println("[FACTORY] Scanning WiFi networks...");

  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();  
  if (n < 0)
  {
      Serial.println("[FACTORY] WiFi scan FAILED!");
      Serial.print("[FACTORY] Error code: ");
      Serial.println(n);
      return;
  }
  else if (n == 0)
  {
      Serial.println("[FACTORY] No AP found");
      WiFi.scanDelete();
      return;
  }
    
  Serial.print("[FACTORY] Total APs found: ");
  Serial.println(n);

  bool foundTestAP = false;
  int32_t testAP_RSSI = -100;
    
    // Manually filter SSID
  for (int i = 0; i < n; i++)
  {
      String ssid = WiFi.SSID(i);
      int32_t rssi = WiFi.RSSI(i);
      
      Serial.print("[FACTORY] SSID[");
      Serial.print(i);
      Serial.print("]: ");
      Serial.print(ssid);
      Serial.print(" | RSSI: ");
      Serial.print(rssi);
      Serial.println(" dBm");
      
      if (ssid == FACTORY_TEST_SSID)
      {
          foundTestAP = true;
          if (rssi > testAP_RSSI)
          {
              testAP_RSSI = rssi;  // Pick the one with best signal
          }
          Serial.println("[FACTORY] >>> Found target AP!");
      }
  }

  // Free scan result memory
  WiFi.scanDelete();

  if (foundTestAP)
  {
    // Pass RSSI value as parameter to task (via pointer cast)
    xTaskCreate(
        testTaskFunction,
        "TestTask",
        4096,
        (void *)(intptr_t)testAP_RSSI,  // Pass RSSI value
        5,
        &xTestTaskHandle
    );
    Serial.println("[FACTORY] Test task created successfully");
    Serial.println("[FACTORY] Entering factory test mode...");
    
    while(1)
    {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
  else
  {
    Serial.println("[FACTORY] 'ez_factory_test' AP not found, continuing normal startup");
  }
}

#endif // BUILD_FACTORY_TEST












