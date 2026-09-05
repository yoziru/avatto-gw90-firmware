#ifndef CONFIG_H_
#define CONFIG_H_

#include <Arduino.h>
#include <CircularBuffer.hpp>
#include "version.h"

// ESP32 PINS TO CONTROL LAN8720
#define ETH_CLK_MODE_1                ETH_CLOCK_GPIO17_OUT       // 8720 clock pin: GPIO17 outputs clock
#define ETH_POWER_PIN_ALTERNATIVE_1   5                          // 8720 reset pin
#define ETH_POWER_PIN_1               -1                         // Not used
#define ETH_TYPE_1                    ETH_PHY_LAN8720            // 8720 PHY type
#define ETH_ADDR_1                    0                          // 8720 PHY address: floating, internal weak pull-down
#define ETH_MDC_PIN_1                 23                         // 8720 MDC pin: GPIO23
#define ETH_MDIO_PIN_1                18                         // 8720 MDIO pin: GPIO18
// ESP32 PINS TO CONTROL CC2652P
#define CC2652P_RST                   16                         // CC2652P reset pin
#define CC2652P_FLASH                 32                         // CC2652P chip select pin
#define CC2652P_RXD                   36                         // CC2652P serial RX pin
#define CC2652P_TXD                   4                          // CC2652P serial TX pin
#define BTN                           35                         // BTN button
#define MODE_SWITCH                   33                         // MODE SWITCH button


#define DEBOUNCE_TIME 70

#define BUFFER_SIZE 256

#define TCP_LISTEN_PORT 6638
#define FORMAT_LITTLEFS_IF_FAILED true

// CC2652 settings (FOR BSL VALIDATION!)
#define NEED_BSL_PIN 15  // CC2652 pin number (FOR BSL VALIDATION!)
#define NEED_BSL_LEVEL 0 // 0 = LOW, 1 = HIGH

constexpr int16_t overseerInterval = 5 * 1000; // check lan or wifi connection every 5sec
constexpr uint8_t overseerMaxRetry = 4;       // 5x4 = 20sec for PHY stability
constexpr uint8_t wifiFallbackMaxRetry = 6;   // 5x6 = 30sec more before last-resort AP
constexpr uint8_t LED_USB = 12;                // RED
constexpr uint8_t LED_PWR = 14;                // BLUE
constexpr uint8_t MAX_SOCKET_CLIENTS = 5;

enum class CoordinatorMode : uint8_t
{
  NETWORK = 0,           // Ethernet primary, WiFi STA fallback, AP last resort
  PASSTHROUGH = 1        // Serial passthrough to USB host
};

extern const char *coordMode;// coordMode node name
extern const char *configFileSystem;
extern const char *configFileWifi;
extern const char *configFileEther;
extern const char *configFileGeneral;
extern const char *configFileSecurity;
extern const char *configFileSerial;
extern const char *configFileMqtt;
extern const char *configFileWg;
extern const char *deviceModel;

IPAddress parse_ip_address(const char *str);
bool loadSystemVar();
bool loadConfigWifi();
bool loadConfigEther();
bool loadConfigGeneral();
bool loadConfigSecurity();
bool loadConfigSerial();
bool loadConfigMqtt();
bool loadConfigWg();

struct ConfigSettingsStruct
{
    char ssid[50];                     // WiFi SSID
    char password[50];                 // WiFi password
    char ipAddressWiFi[18];            // WiFi IP address
    char ipMaskWiFi[16];               // WiFi mask
    char ipGWWiFi[18];                 // WiFi gateway
    bool dhcpWiFi;                     // WiFi DHCP
    bool dhcp;                         // Ethernet DHCP
    bool connectedEther;               // Ethernet connected
    char ipAddress[18];                // Ethernet IP address
    char ipMask[16];                   // Ethernet mask
    char ipGW[18];                     // Ethernet gateway
    int serialSpeed;                   // Serial speed
    int socketPort;                    // Socket port
    bool disableWeb;                   // Web server enabled
    int refreshLogs;                   // Logs refresh interval
    char hostname[50];                 // Hostname
    bool connectedSocket[MAX_SOCKET_CLIENTS];  // Socket connected
    IPAddress connectedClientIp[MAX_SOCKET_CLIENTS];  // Socket client IP
    int connectedClients;              // Connected clients
    unsigned long socketTime;          // Socket time
    int tempOffset;                    // Temperature offset
    bool webAuth;                      // Web server authentication
    char webUser[50];                  // Web server user
    char webPass[50];                  // Web server password
    bool disableLedUSB;                // Disable LED USB
    bool disableLedPwr;                // Disable LED PWR
    bool disableLeds;                  // Disable LEDs
    CoordinatorMode coordinator_mode = CoordinatorMode::NETWORK;         // Coordinator mode
    bool keepWeb;                      // Keep web server
    bool apStarted;                    // AP started
    bool wifiFallbackActive;           // WiFi STA fallback in progress (NETWORK mode)
    bool wifiFallbackHadConnection;    // WiFi STA reached a connected state this fallback
    bool serversStarted;               // Web + socket servers already started once
    bool fwEnabled;                    // Firmware update enabled
    IPAddress fwIp;                    // Firmware update IP

    bool zbLedState;                   // ZB LED state
    bool zbFlashing;                   // ZB LED flashing
    char timeZone[50];                 // Time zone
};

struct MqttSettingsStruct
{
  bool enable;
  char server[50];
  IPAddress serverIP;
  int port;
  char user[50];
  char pass[50];
  char topic[50];
  int interval;
  bool discovery;
  unsigned long reconnectTime;
  unsigned long heartbeatTime;
};

struct WgSettingsStruct
{
  bool enable;
  bool init = 0;
  char localAddr[20];
  IPAddress localIP;
  char localPrivKey[45];
  char endAddr[45];
  char endPubKey[45];
  int endPort;
};

struct zbVerStruct
{
  uint32_t zbRev;
  uint8_t maintrel;
  uint8_t minorrel;
  uint8_t majorrel;
  uint8_t product;
  uint8_t transportrev;
  String chipID;
};

typedef CircularBuffer<char, 8024> LogConsoleType;
typedef CircularBuffer<char, 16384> ZbLogConsoleType;

extern ConfigSettingsStruct ConfigSettings;
extern MqttSettingsStruct MqttSettings;
extern WgSettingsStruct WgSettings;

#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(String(x))
#define DEBUG_PRINTLN(x) Serial.println(String(x))
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif
#endif
