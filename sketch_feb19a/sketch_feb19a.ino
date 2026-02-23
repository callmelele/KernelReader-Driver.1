#include "USB.h"
#include "USBHIDMouse.h"

USBHIDMouse Mouse;

// Hardware Serial 2 pins (Connect these to your CH343 adapter)
// CH343 TX -> ESP32 RX (GPIO 16)
// CH343 RX -> ESP32 TX (GPIO 17)
#define RXD2 16
#define TXD2 17

void setup() {
    // COM5 (Native USB) - Used for flashing and debugging
    Serial.begin(115200); 

    // COM3 (The CH343 Adapter) - This is where the C++ data arrives
    Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

    Mouse.begin();
    USB.begin();
    
    // LED indicator to show it's powered on
    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);
}

void loop() {
    // Listen to Serial2 (COM3)
    if (Serial2.available() >= 3) {
        int8_t x = (int8_t)Serial2.read();
        int8_t y = (int8_t)Serial2.read();
        uint8_t click = (uint8_t)Serial2.read();

        // Perform move via the Native USB (COM5/HID)
        Mouse.move(x, y);

        if (click == 1) {
            Mouse.press(MOUSE_LEFT);
            Mouse.release(MOUSE_LEFT);
        }
    }
}