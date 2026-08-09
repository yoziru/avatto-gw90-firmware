#include <WiFi.h>

#include "config.h"
#include "network.h"
#include "log.h"
#include "mqtt.h"
#include "web.h"
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

void printRecvSocket(size_t bytes_read, uint8_t net_buf[BUFFER_SIZE])
{
  char output_sprintf[3];
  if (bytes_read > 0)
  {
    String tmpTime;
    String buff = "";
    unsigned long timeLog = millis();
    tmpTime = String(timeLog, DEC);
    logPush('[');
    for (int j = 0; j < tmpTime.length(); j++)
    {
      logPush(tmpTime[j]);
    }
    logPush(']');
    logPush(' ');
    logPush('-');
    logPush('>');

    for (int i = 0; i < bytes_read; i++)
    {
      sprintf(output_sprintf, "%02x", net_buf[i]);
      logPush(' ');
      logPush(output_sprintf[0]);
      logPush(output_sprintf[1]);
    }
    logPush('\n');
  }
}

void printSendSocket(size_t bytes_read, uint8_t serial_buf[BUFFER_SIZE])
{
  char output_sprintf[3];
  String tmpTime;
  String buff = "";
  unsigned long timeLog = millis();
  tmpTime = String(timeLog, DEC);
  logPush('[');
  for (int j = 0; j < tmpTime.length(); j++)
  {
    logPush(tmpTime[j]);
  }
  logPush(']');
  logPush(' ');
  logPush('<');
  logPush('-');
  for (int i = 0; i < bytes_read; i++)
  {
    // if (serial_buf[i] == 0x01)
    //{
    // }
    sprintf(output_sprintf, "%02x", serial_buf[i]);
    logPush(' ');
    logPush(output_sprintf[0]);
    logPush(output_sprintf[1]);
    // if (serial_buf[i] == 0x03)
    // {

    //}
  }
  logPush('\n');
}

void processSocketBridge()
{
  uint16_t net_bytes_read = 0;
  uint8_t net_buf[BUFFER_SIZE];
  uint16_t serial_bytes_read = 0;
  uint8_t serial_buf[BUFFER_SIZE];

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
          WiFiClient TempClient2 = server.available();
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
          client[i] = server.available();
          continue;
        }
      }
    }
    WiFiClient TempClient = server.available();
    TempClient.stop();
  }

  for (byte cln = 0; cln < MAX_SOCKET_CLIENTS; cln++)
  {
    if (client[cln])
    {
      socketClientConnected(cln);
      while (client[cln].available())
      { // read from LAN
        net_buf[net_bytes_read] = client[cln].read();
        if (net_bytes_read < BUFFER_SIZE - 1)
          net_bytes_read++;
      } // send to Zigbee
      Serial2.write(net_buf, net_bytes_read);
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
    while (Serial2.available())
    { // read from Zigbee
      serial_buf[serial_bytes_read] = Serial2.read();
      if (serial_bytes_read < BUFFER_SIZE - 1)
        serial_bytes_read++;
    }
    // send to LAN
    for (byte cln = 0; cln < MAX_SOCKET_CLIENTS; cln++)
    {
      if (client[cln])
        client[cln].write(serial_buf, serial_bytes_read);
    }
    // print to web console
    printSendSocket(serial_bytes_read, serial_buf);
    serial_bytes_read = 0;
  }
}
