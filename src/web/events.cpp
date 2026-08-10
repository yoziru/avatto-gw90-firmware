#include <Arduino.h>
#include <WebServer.h>
#include <NetworkClient.h>
#include "web.h"

NetworkClient eventsClient;

void handleEvents()
{
    eventsClient = serverWeb.client();
    if (eventsClient)
    {
        eventsClient.println("HTTP/1.1 200 OK");
        eventsClient.println("Content-Type: text/event-stream;");
        eventsClient.println("Connection: close");
        eventsClient.println("Access-Control-Allow-Origin: *");
        eventsClient.println("Cache-Control: no-cache");
        eventsClient.println();
        eventsClient.flush();
    }
}

void sendEvent(const char *event, const uint8_t evsz, const String data)
{
    if (eventsClient)
    {
        char evnmArr[10 + evsz];
        sprintf(evnmArr, "event: %s\n", event);
        eventsClient.print(evnmArr);
        eventsClient.print(String("data: ") + data + "\n\n");
        eventsClient.flush();
    }
}
