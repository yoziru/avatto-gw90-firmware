#include <Arduino.h>

#include "log.h"
#include "config.h"

LogConsoleType logConsole;
ZbLogConsoleType zbLogConsole;

// The console page polls this every `refreshLogs` ms (default 1 s) - returning
// a reused String instead of a fresh up-to-8KiB allocation per poll keeps that
// from being the biggest allocator on the device. The buffer is fully rebuilt
// each call and consumed synchronously by the sender, so reuse is safe.
template <typename Ring>
static String &ringDump(Ring &ring)
{
  static String buff;
  buff = "";

  if (ring.isEmpty()) {
    return buff;
  } else {
    for (typename Ring::index_t i = 0; i < ring.size(); i++) {
      buff += ring[i];
    }
    return buff;
  }
}

String &logPrint()
{
  return ringDump(logConsole);
}

String &logPrintZb()
{
  return ringDump(zbLogConsole);
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
  zbLogConsole.clear();
  Serial.println("================================\n");
}
