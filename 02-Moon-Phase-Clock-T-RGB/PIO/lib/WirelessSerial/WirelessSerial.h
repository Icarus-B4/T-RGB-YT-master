#ifndef WIRELESS_SERIAL_H
#define WIRELESS_SERIAL_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <vector>

class WirelessSerialClass {
public:
    WirelessSerialClass();
    void begin(const char* mdnsName = "moonclock");
    void update();
    
    size_t printf(const char * format, ...);
    size_t println(const String &s);
    size_t println(const char *s);
    size_t println();
    size_t print(const String &s);
    size_t print(const char *s);

private:
    AsyncWebServer server;
    WebSocketsServer webSocket;
    String logBuffer;
    void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
    SemaphoreHandle_t wsMutex;
    std::vector<String> logHistory;
    void addToHistory(const String &s);
    static const int MAX_HISTORY = 30;
};

extern WirelessSerialClass WirelessSerial;

#endif
