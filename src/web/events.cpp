#include <Arduino.h>
#include <WebServer.h>
#include <NetworkClient.h>
#include "config.h"
#include "web.h"

NetworkClient eventsClient;

void handleEvents()
{
    // An unauthenticated client must not be able to observe firmware-flash
    // progress, nor take over the single global SSE slot. EventSource can't
    // send custom headers, but browsers replay cached HTTP-auth credentials
    // for the origin, so an already-logged-in session keeps working; anyone
    // else gets a 401 and the frontend falls back to polling mode.
    if (!checkAuth())
    {
        return; // rejection already sent by checkAuth()
    }
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

void sendEvent(const char *event, const uint8_t evsz, const String &data)
{
    if (eventsClient)
    {
        (void)evsz; // sizeof(event) hint, not needed for printf
        eventsClient.printf("event: %s\n", event);
        eventsClient.printf("data: %s\n\n", data.c_str());
        eventsClient.flush();
    }
}
