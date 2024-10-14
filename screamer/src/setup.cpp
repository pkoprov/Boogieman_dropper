#include <Arduino.h>
#include <WiFi.h>
#include "setup.h"
#include "clsScreamer.h"

extern clsScreamer screamer;

const char* ssid = "Koprov 2.4_IoT";
const char* password = "7323uewe";

void setupDevice() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    screamer.initializeMQTT();
}
