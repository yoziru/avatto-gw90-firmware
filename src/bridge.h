#ifndef BRIDGE_H_
#define BRIDGE_H_

#include <WiFi.h>

void processSocketBridge();
void socketClientConnected(int client);
void socketClientDisconnected(int client);
void printRecvSocket(size_t bytes_read, uint8_t net_buf[BUFFER_SIZE]);
void printSendSocket(size_t bytes_read, uint8_t serial_buf[BUFFER_SIZE]);

#endif
