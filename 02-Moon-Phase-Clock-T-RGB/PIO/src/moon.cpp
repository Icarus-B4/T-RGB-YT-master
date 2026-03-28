#include "moon.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

bool fetchMoonData(MoonData &data) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    gmtime_r(&now, &timeinfo);

    if (timeinfo.tm_year < (2023 - 1900)) {
        Serial.println("[Moon] NTP sync required before fetching moon data.");
        return false;
    }

    char dateStr[25];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%dT%H:%M", &timeinfo);
    
    String url = "https://svs.gsfc.nasa.gov/api/dialamoon/";
    url += dateStr;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(15000); // 15 Sekunden warten ( NASA Server ist träge)
    
    Serial.print("[Moon] Fetching: ");
    Serial.println(url);
    
    int httpCode = http.GET();
    bool success = false;
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, payload);
        
        data.age = doc["age"].as<double>();
        data.illumination = doc["phase"].as<double>();
        data.phaseName = calculateMoonPhaseName(data.age, data.illumination);
        data.imageIndex = calculateMoonImageIndex(data.age, data.illumination);

        Serial.printf("[Moon] Data Sync: Age=%.2f, Phase=%.2f%%, Name=%s, Index=%d\n", 
                      data.age, data.illumination, data.phaseName.c_str(), data.imageIndex);
        success = true;
    } else {
        Serial.printf("[Moon] HTTP-Fehler: %d\n", httpCode);
        if (httpCode == -1) {
            Serial.println("[Moon] Grund: Verbindung fehlgeschlagen (Timeout/DNS/SSL).");
            Serial.printf("[Moon] WiFi-Status: %d (3=Verbunden)\n", WiFi.status());
            if (WiFi.status() == WL_CONNECTED) {
                Serial.print("[Moon] Lokale IP: ");
                Serial.println(WiFi.localIP());
            }
        } else if (httpCode == -11) {
            Serial.println("[Moon] Grund: Server-Timeout (NASA API ist langsam).");
        }
    }
    
    http.end();
    return success;
}

String calculateMoonPhaseName(double age, double illumination) {
    bool waxing = (age < 14.76);
    if (illumination < 2.0) return "Neumond";
    if (illumination > 98.0) return "Vollmond";
    if (illumination >= 2.0 && illumination < 48.0) return waxing ? "Zun. Sichel" : "Abn. Sichel";
    if (illumination >= 48.0 && illumination <= 52.0) return waxing ? "Erstes Viertel" : "Letztes Viertel";
    if (illumination > 52.0 && illumination <= 98.0) return waxing ? "Zun. Dreiviertel" : "Abn. Dreiviertel";
    return "Unbekannt";
}

int calculateMoonImageIndex(double age, double illumination) {
    bool waxing = (age < 14.76);
    double index = waxing ? (illumination / 100.0) * 15.0 : 30.0 - (illumination / 100.0) * 15.0;
    return (int)(index + 0.5) % 30;
}
