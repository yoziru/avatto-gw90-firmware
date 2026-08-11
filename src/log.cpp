#include <Arduino.h>

#include "log.h"
#include "config.h"

LogConsoleType logConsole;

void logPush(char c)
{
  logConsole.push(c);
}

// The console page polls this every `refreshLogs` ms (default 1 s) - returning
// a reused String instead of a fresh up-to-8KiB allocation per poll keeps that
// from being the biggest allocator on the device. The buffer is fully rebuilt
// each call and consumed synchronously by the sender, so reuse is safe.
String &logPrint()
{
  static String buff;
  buff = "";

  if (logConsole.isEmpty()) {
    return buff;
  } else {
    for (decltype(logConsole)::index_t i = 0; i < logConsole.size(); i++) {
      buff += logConsole[i];
    }
    return buff;
  }
}

void logClear()
{
  Serial.println("\n========== LOG CLEAR ==========");
  Serial.print("[LOG] Current log buffer size: ");
  Serial.println(logConsole.size());
  
  if (!logConsole.isEmpty()) {
    logConsole.clear();
    Serial.println("[LOG] Log buffer cleared successfully");
  } else {
    Serial.println("[LOG] Log buffer already empty");
  }
  Serial.println("================================\n");
}
