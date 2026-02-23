#include "USB.h"
#include "USBHIDMouse.h"

USBHIDMouse Mouse;

void setup() {
    // This is the bridge to the CH343 (COM3)
    Serial.begin(115200); 

    // Initialize the HID Mouse (COM5)
    Mouse.begin();
    USB.begin();
}

void loop() {
    // We listen to the hardware Serial (COM3)
    if (Serial.available() >= 3) {
        int8_t x = (int8_t)Serial.read();
        int8_t y = (int8_t)Serial.read();
        uint8_t click = (uint8_t)Serial.read();

        // Send move to the PC via the USB HID logic
        Mouse.move(x, y);

        if (click == 1) {
            Mouse.press(MOUSE_LEFT);
            delay(1); // Tiny delay to ensure the OS registers the click
            Mouse.release(MOUSE_LEFT);
        }
    }
}
