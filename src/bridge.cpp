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

// Hex dump of a bridge byte run into the web-console frame ring. sprintf() is
// far too heavy for the bridge hot path, so use a nibble lookup table instead.
static void logFrameHex(const char *arrow, const uint8_t *buf, size_t len)
{
  static const char hexDigits[] = "0123456789abcdef";
  printLogTimeZb();
  for (const char *a = arrow; *a; a++)
  {
    zbLogConsole.push(*a);
  }
  for (size_t i = 0; i < len; i++)
  {
    zbLogConsole.push(' ');
    zbLogConsole.push(hexDigits[buf[i] >> 4]);
    zbLogConsole.push(hexDigits[buf[i] & 0x0F]);
  }
  zbLogConsole.push('\n');
}

void printRecvSocket(size_t bytes_read, const uint8_t *net_buf)
{
  if (bytes_read == 0)
  {
    return;
  }
  logFrameHex(" ->", net_buf, bytes_read);
}

// Serial->net frames are logged through a tiny ZNP framer: the coordinator's
// bytes arrive in arbitrary read() chunks, so dumping per-chunk produced
// headless tails and glued lines the browser dissector couldn't decode. Here
// bytes accumulate until a whole frame (SOF + len + cmd + data + FCS) is
// present and that exact frame becomes one log line. Partial data flushes raw
// after 50 ms of silence.
static uint8_t zbAcc[512];
static size_t zbAccLen = 0;
static uint32_t zbAccStart = 0;

static void zbFramerFlush()
{
  if (zbAccLen)
  {
    logFrameHex(" <-", zbAcc, zbAccLen);
    zbAccLen = 0;
  }
}

static void zbFramerFeed(const uint8_t *buf, size_t len)
{
  for (size_t i = 0; i < len; i++)
  {
    if (zbAccLen < sizeof(zbAcc))
    {
      zbAcc[zbAccLen++] = buf[i];
    }
    else
    {
      zbFramerFlush(); // overlong garbage, start over
    }
    if (zbAccLen == 1 && zbAcc[0] != 0xFE)
    {
      zbAccLen = 0; // not an SOF, drop
    }
  }
  while (zbAccLen >= 2)
  {
    const size_t need = 5 + zbAcc[1]; // SOF + len + c0 + c1 + data[len] + FCS
    if (zbAccLen < need)
    {
      break; // wait for the rest of the frame
    }
    logFrameHex(" <-", zbAcc, need);
    zbAccLen -= need;
    memmove(zbAcc, zbAcc + need, zbAccLen);
  }
}

void printSendSocket(size_t bytes_read, const uint8_t *serial_buf)
{
  if (bytes_read == 0)
  {
    return;
  }
  if (!zbAccLen)
  {
    zbAccStart = millis();
  }
  zbFramerFeed(serial_buf, bytes_read);
}

void processSocketBridge()
{
  if (ConfigSettings.zbFlashing) return;
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
  if (zbAccLen && millis() - zbAccStart > 50)
  {
    zbFramerFlush(); // stalled partial frame
  }
}
