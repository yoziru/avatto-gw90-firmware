#include <WiFi.h>

#include "config.h"
#include "net.h"
#include "log.h"
#include "mqtt.h"
#include "web.h"
#include "zb.h"
#include "bridge.h"

WiFiClient client[MAX_SOCKET_CLIENTS];

void socketClientConnected(int client)
{
  if (ConfigSettings.connectedSocket[client] != true)
  {
    DEBUG_PRINT(F("Connected client "));
    DEBUG_PRINTLN(client);
    if (ConfigSettings.connectedClients == 0)
    {
      ConfigSettings.socketTime = millis();
      DEBUG_PRINT(F("Socket time "));
      DEBUG_PRINTLN(ConfigSettings.socketTime);
      mqttPublishIo("socket", "ON");
    }
    ConfigSettings.connectedSocket[client] = true;
    ConfigSettings.connectedClientIp[client] = ::client[client].remoteIP();
    ConfigSettings.connectedClients++;
  }
}

void socketClientDisconnected(int client)
{
  if (ConfigSettings.connectedSocket[client] != false)
  {
    DEBUG_PRINT(F("Disconnected client "));
    DEBUG_PRINTLN(client);
    ConfigSettings.connectedSocket[client] = false;
    ConfigSettings.connectedClients--;
    if (ConfigSettings.connectedClients == 0)
    {
      ConfigSettings.socketTime = millis();
      DEBUG_PRINT(F("Socket time "));
      DEBUG_PRINTLN(ConfigSettings.socketTime);
      mqttPublishIo("socket", "OFF");
    }
  }
}

void printRecvSocket(size_t bytes_read, const uint8_t *net_buf)
{
  if (bytes_read == 0)
  {
    return;
  }
  // Hex dump into the web-console ring buffer. sprintf() is far too heavy for
  // the bridge hot path, so use a nibble lookup table instead.
  static const char hexDigits[] = "0123456789abcdef";
  printLogTime();
  logPush(' ');
  logPush('-');
  logPush('>');

  for (size_t i = 0; i < bytes_read; i++)
  {
    logPush(' ');
    logPush(hexDigits[net_buf[i] >> 4]);
    logPush(hexDigits[net_buf[i] & 0x0F]);
  }
  logPush('\n');
}

void printSendSocket(size_t bytes_read, const uint8_t *serial_buf)
{
  if (bytes_read == 0)
  {
    return;
  }
  static const char hexDigits[] = "0123456789abcdef";
  printLogTime();
  logPush(' ');
  logPush('<');
  logPush('-');
  for (size_t i = 0; i < bytes_read; i++)
  {
    logPush(' ');
    logPush(hexDigits[serial_buf[i] >> 4]);
    logPush(hexDigits[serial_buf[i] & 0x0F]);
  }
  logPush('\n');
}

void processSocketBridge()
{
  uint16_t net_bytes_read = 0;
  uint8_t net_buf[BUFFER_SIZE];

  if (server.hasClient())
  {
    for (byte i = 0; i < MAX_SOCKET_CLIENTS; i++)
    {
      if (!client[i] || !client[i].connected())
      {
        if (client[i])
        {
          client[i].stop();
        }
        if (ConfigSettings.fwEnabled)
        {
          WiFiClient TempClient2 = server.accept();
          if (TempClient2.remoteIP() == ConfigSettings.fwIp)
          {
            printLogMsg(String("[SOCK IP WHITELIST] Accepted connection from IP: ") + TempClient2.remoteIP().toString());
            client[i] = TempClient2;
            continue;
          }
          else
          {
            printLogMsg(String("[SOCK IP WHITELIST] Rejected connection from unknown IP: ") + TempClient2.remoteIP().toString());
          }
        }
        else
        {
          client[i] = server.accept();
          continue;
        }
      }
    }
    WiFiClient TempClient = server.accept();
    TempClient.stop();
  }

  for (byte cln = 0; cln < MAX_SOCKET_CLIENTS; cln++)
  {
    if (client[cln])
    {
      socketClientConnected(cln);
      while (client[cln].available() && net_bytes_read < BUFFER_SIZE)
      { // read from LAN
        net_buf[net_bytes_read++] = client[cln].read();
      } // send to Zigbee
      if (net_bytes_read > 0)
      {
        Serial2.write(net_buf, net_bytes_read);
        zbClientTxMs = millis(); // client -> coordinator traffic
      }
      // print to web console
      printRecvSocket(net_bytes_read, net_buf);
      net_bytes_read = 0;
    }
    else
    {
      socketClientDisconnected(cln);
    }
  }

  if (Serial2.available())
  {
    uint16_t serial_bytes_read = 0;
    uint8_t serial_buf[BUFFER_SIZE];
    zbLastRxMs = millis();
    while (Serial2.available() && serial_bytes_read < BUFFER_SIZE)
    { // read from Zigbee
      serial_buf[serial_bytes_read++] = Serial2.read();
    }
    // send to LAN
    for (byte cln = 0; cln < MAX_SOCKET_CLIENTS; cln++)
    {
      if (client[cln])
        client[cln].write(serial_buf, serial_bytes_read);
    }
    // print to web console
    printSendSocket(serial_bytes_read, serial_buf);
  }
}
