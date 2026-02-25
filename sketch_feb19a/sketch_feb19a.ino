#include <WiFi.h>
#include <WiFiUdp.h>
#include "USB.h"
#include "USBHIDMouse.h"
#include "esp_wifi.h" // Needed for power-saving control

const char* ssid = "#Telia-88C6D8";
const char* password = "#M49Z)6c-sH%hsJ9";
WiFiUDP udp;
USBHIDMouse Mouse;

void setup() {
    Serial.begin(115200);
    
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(100); }

    // CRITICAL: Disable WiFi Power Saving to reduce latency/jitter
    esp_wifi_set_ps(WIFI_PS_NONE);

    udp.begin(4444);
    Mouse.begin();
    USB.begin();
    
    Serial.println(WiFi.localIP());
}

void handleInput(int8_t x, int8_t y, uint8_t click) {
    if (x != 0 || y != 0) {
        Mouse.move(x, y);
    }

    if (click == 1) {
        Mouse.press(MOUSE_LEFT);

        delayMicroseconds(500); 
        Mouse.release(MOUSE_LEFT);
    }
}

void loop() {
    // 1. UDP Handling
    int packetSize = udp.parsePacket();
    if (packetSize >= 3) {
        uint8_t buf[3];
        udp.read(buf, 3);
        handleInput((int8_t)buf[0], (int8_t)buf[1], buf[2]);
        while(udp.parsePacket() > 0) udp.flush(); // Clear backlog
    }

    // 2. Serial Handling (The Fix)
    if (Serial.available() >= 3) {
        int8_t x = Serial.read();
        int8_t y = Serial.read();
        uint8_t click = Serial.read();
        handleInput(x, y, click);
    }
}
