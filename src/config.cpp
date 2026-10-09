#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

#include "config.h"
#include "config_utils.h"
#include "etc.h"
#include "log.h"

ConfigSettingsStruct ConfigSettings;
MqttSettingsStruct MqttSettings;
WgSettingsStruct WgSettings;

IPAddress parse_ip_address(const char *str)
{
  // fromString() rejects anything that is not strict dotted-decimal IPv4
  // (bad octet, wrong dot count, trailing garbage); on failure callers get
  // 0.0.0.0, same as before. The colon guard keeps the old contract that a
  // ':' is never valid - without it fromString()'s IPv6 fallback would turn
  // garbage into a :: address instead of 0.0.0.0.
  IPAddress result;
  if (str != nullptr && strchr(str, ':') == nullptr && result.fromString(str))
  {
    return result;
  }
  return IPAddress();
}

// Open a config file, deserialize it and handle the shared error path
// (log + remove the corrupt file so the next boot regenerates defaults).
static bool readJsonConfig(const char *path, JsonDocument &doc)
{
  File configFile = LittleFS.open(path, FILE_READ);
  if (!configFile)
  {
    DEBUG_PRINTLN(F("failed open."));
    return false;
  }
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();
  if (error)
  {
    DEBUG_PRINTLN(F("deserializeJson() failed: "));
    DEBUG_PRINTLN(error.f_str());
    LittleFS.remove(path);
    return false;
  }
  return true;
}

// Load WiFi configuration
bool loadConfigWifi()
{
  const char *ssid = "ssid";
  const char *pass = "pass";
  const char *dhcpWiFi = "dhcpWiFi";
  const char *ip = "ip";
  const char *mask = "mask";
  const char *gw = "gw";
  if (!LittleFS.exists(configFileWifi))
  {
    JsonDocument doc;
    doc[ssid] = "";
    doc[pass] = "";
    doc[dhcpWiFi] = 1;
    doc[ip] = "";
    doc[mask] = "";
    doc[gw] = "";
    writeDefaultConfig(configFileWifi, doc);
  }
  JsonDocument doc;
  if (!readJsonConfig(configFileWifi, doc))
  {
    return false;
  }

  ConfigSettings.dhcpWiFi = (int)doc[dhcpWiFi];
  strlcpy(ConfigSettings.ssid, doc[ssid] | "", sizeof(ConfigSettings.ssid));
  strlcpy(ConfigSettings.password, doc[pass] | "", sizeof(ConfigSettings.password));
  strlcpy(ConfigSettings.ipAddressWiFi, doc[ip] | "", sizeof(ConfigSettings.ipAddressWiFi));
  strlcpy(ConfigSettings.ipMaskWiFi, doc[mask] | "", sizeof(ConfigSettings.ipMaskWiFi));
  strlcpy(ConfigSettings.ipGWWiFi, doc[gw] | "", sizeof(ConfigSettings.ipGWWiFi));
  return true;
}
// Load Ethernet configuration
bool loadConfigEther()
{
  const char *dhcp = "dhcp";
  const char *ip = "ip";
  const char *mask = "mask";
  const char *gw = "gw";
  if (!LittleFS.exists(configFileEther))
  {
    JsonDocument doc;
    doc[dhcp] = 1;
    doc[ip] = "";
    doc[mask] = "";
    doc[gw] = "";
    writeDefaultConfig(configFileEther, doc);
  }

  JsonDocument doc;
  if (!readJsonConfig(configFileEther, doc))
  {
    return false;
  }

  ConfigSettings.dhcp = (int)doc[dhcp];
  strlcpy(ConfigSettings.ipAddress, doc[ip] | "", sizeof(ConfigSettings.ipAddress));
  strlcpy(ConfigSettings.ipMask, doc[mask] | "", sizeof(ConfigSettings.ipMask));
  strlcpy(ConfigSettings.ipGW, doc[gw] | "", sizeof(ConfigSettings.ipGW));
  return true;
}
// Load general configuration
bool loadConfigGeneral()
{
  const char *hostname = "hostname";
  const char *disableLeds = "disableLeds";
  const char *refreshLogs = "refreshLogs";
  const char *disableLedPwr = "disableLedPwr";
  const char *disableLedUSB = "disableLedUSB";
  const char *keepWeb = "keepWeb";
  const char *timeZoneName = "timeZoneName";
  if (!LittleFS.exists(configFileGeneral))
  {
    DEBUG_PRINTLN("RESET ConfigGeneral");
    JsonDocument doc;
    doc[hostname] = deviceModel;
    doc[disableLeds] = 0;
    doc[refreshLogs] = 1000;
    doc[disableLedPwr] = 0;
    doc[disableLedUSB] = 0;
    doc[coordMode] = 0;
    doc[keepWeb] = 0;
    writeDefaultConfig(configFileGeneral, doc);
  }

  JsonDocument doc;
  if (!readJsonConfig(configFileGeneral, doc))
  {
    return false;
  }

  if ((double)doc[refreshLogs] < 1000)
  {
    ConfigSettings.refreshLogs = 1000;
  }
  else
  {
    ConfigSettings.refreshLogs = (int)doc[refreshLogs];
  }
  DEBUG_PRINTLN(F("[loadConfigGeneral] 'doc[coordMode]' res is:"));
  DEBUG_PRINTLN(String((uint8_t)doc[coordMode]));
  strlcpy(ConfigSettings.hostname, doc[hostname] | "", sizeof(ConfigSettings.hostname));
  ConfigSettings.coordinator_mode = static_cast<CoordinatorMode>((uint8_t)doc[coordMode]);
  DEBUG_PRINTLN(F("[loadConfigGeneral] 'static_cast' res is:"));
  DEBUG_PRINTLN(String(static_cast<uint8_t>(ConfigSettings.coordinator_mode)));
  ConfigSettings.disableLedPwr = (uint8_t)doc[disableLedPwr];
  ConfigSettings.disableLedUSB = (uint8_t)doc[disableLedUSB];
  ConfigSettings.disableLeds = (uint8_t)doc[disableLeds];
  ConfigSettings.keepWeb = (uint8_t)doc[keepWeb];
  strlcpy(ConfigSettings.timeZone, doc[timeZoneName] | "", sizeof(ConfigSettings.timeZone));
  DEBUG_PRINTLN(F("[loadConfigGeneral] config load done"));
  return true;
}
// Load security configuration
bool loadConfigSecurity()
{
  const char *disableWeb = "disableWeb";
  const char *webAuth = "webAuth";
  const char *webUser = "webUser";
  const char *webPass = "webPass";
  const char *fwEnabled = "fwEnabled";
  const char *fwIp = "fwIp";
  if (!LittleFS.exists(configFileSecurity))
  {
    JsonDocument doc;
    doc[disableWeb] = 0;
    doc[webAuth] = 0;
    doc[webUser] = "admin";
    doc[webPass] = "";
    doc[fwEnabled] = 0;
    doc[fwIp] = "";
    writeDefaultConfig(configFileSecurity, doc);
  }

  JsonDocument doc;
  if (!readJsonConfig(configFileSecurity, doc))
  {
    return false;
  }

  ConfigSettings.disableWeb = (uint8_t)doc[disableWeb];
  ConfigSettings.webAuth = (uint8_t)doc[webAuth];
  strlcpy(ConfigSettings.webUser, doc[webUser] | "", sizeof(ConfigSettings.webUser));
  strlcpy(ConfigSettings.webPass, doc[webPass] | "", sizeof(ConfigSettings.webPass));
  ConfigSettings.fwEnabled = (uint8_t)doc[fwEnabled];
  ConfigSettings.fwIp = parse_ip_address(doc[fwIp] | "0.0.0.0");
  return true;
}

bool loadConfigSerial()
{
  const char *baud = "baud";
  const char *port = "port";
  if (!LittleFS.exists(configFileSerial))
  {
    JsonDocument doc;
    doc[baud] = 115200;
    doc[port] = 6638;
    writeDefaultConfig(configFileSerial, doc);
  }

  JsonDocument doc;
  if (!readJsonConfig(configFileSerial, doc))
  {
    return false;
  }

  ConfigSettings.serialSpeed = (int)doc[baud];
  ConfigSettings.socketPort = (int)doc[port];
  if (ConfigSettings.socketPort == 0)
  {
    ConfigSettings.socketPort = TCP_LISTEN_PORT;
  }
  return true;
}
// Load MQTT configuration
bool loadConfigMqtt()
{
  const char *enable = "enable";
  const char *server = "server";
  const char *port = "port";
  const char *user = "user";
  const char *pass = "pass";
  const char *topic = "topic";
  const char *interval = "interval";
  const char *discovery = "discovery";
  const char *tls = "tls";

  if (!LittleFS.exists(configFileMqtt))
  {
    char deviceIdArr[20];
    getDeviceID(deviceIdArr, sizeof(deviceIdArr));

    JsonDocument doc;
    doc[enable] = 0;
    doc[server] = "";
    doc[port] = 1883;
    doc[user] = "mqttuser";
    doc[pass] = "";
    doc[topic] = String(deviceIdArr);
    doc[interval] = 60;
    doc[discovery] = 0;
    doc[tls] = 0;
    writeDefaultConfig(configFileMqtt, doc);
  }

  JsonDocument doc;
  if (!readJsonConfig(configFileMqtt, doc))
  {
    return false;
  }

  MqttSettings.enable = (int)doc[enable];
  strlcpy(MqttSettings.server, doc[server] | "", sizeof(MqttSettings.server));
  MqttSettings.serverIP = parse_ip_address(MqttSettings.server);
  MqttSettings.port = (int)doc[port];
  strlcpy(MqttSettings.user, doc[user] | "", sizeof(MqttSettings.user));
  strlcpy(MqttSettings.pass, doc[pass] | "", sizeof(MqttSettings.pass));
  strlcpy(MqttSettings.topic, doc[topic] | "", sizeof(MqttSettings.topic));
  MqttSettings.interval = (int)doc[interval];
  MqttSettings.discovery = (int)doc[discovery];
  MqttSettings.tls = (int)doc[tls];
  return true;
}
// Load WG configuration
bool loadConfigWg()
{
  const char *enable = "enable";
  const char *localAddr = "localAddr";
  const char *localPrivKey = "localPrivKey";
  const char *endAddr = "endAddr";
  const char *endPubKey = "endPubKey";
  const char *endPort = "endPort";

  if (!LittleFS.exists(configFileWg))
  {
    JsonDocument doc;
    doc[enable] = 0;
    doc[localAddr] = "";
    doc[localPrivKey] = "";
    doc[endAddr] = "";
    doc[endPubKey] = "";
    doc[endPort] = 51820;
    writeDefaultConfig(configFileWg, doc);
  }

  JsonDocument doc;
  if (!readJsonConfig(configFileWg, doc))
  {
    return false;
  }

  WgSettings.enable = (int)doc[enable];

  strlcpy(WgSettings.localAddr, doc[localAddr] | "", sizeof(WgSettings.localAddr));
  WgSettings.localIP = parse_ip_address(WgSettings.localAddr);

  // The private key was historically persisted under "localIP". Keep reading
  // the old key so existing configs migrate silently; the UI writes the new name.
  const char *privKey = wgPrivateKey(doc);
  strlcpy(WgSettings.localPrivKey, privKey ? privKey : "", sizeof(WgSettings.localPrivKey));
  strlcpy(WgSettings.endAddr, doc[endAddr] | "", sizeof(WgSettings.endAddr));
  strlcpy(WgSettings.endPubKey, doc[endPubKey] | "", sizeof(WgSettings.endPubKey));
  WgSettings.endPort = doc[endPort] | 51820;
  return true;
}
