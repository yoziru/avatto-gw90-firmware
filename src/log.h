#ifndef LOG_H_
#define LOG_H_

#include "config.h"

extern LogConsoleType logConsole;
extern ZbLogConsoleType zbLogConsole;

// Two rings: events (printLogMsg / printLogTime) and ZB frame hex dumps from
// the bridge. Split so frame spam can never evict event lines.
void logClear();
String &logPrint();    // event ring dump
String &logPrintZb();  // frame ring dump

#endif
