#ifndef BRIDGE_H_
#define BRIDGE_H_

#include <WiFi.h>

void processSocketBridge();
void socketClientConnected(int client);
void socketClientDisconnected(int client);
void printRecvSocket(size_t bytes_read, const uint8_t *net_buf);
void printSendSocket(size_t bytes_read, const uint8_t *serial_buf);

#endif
