//main.cpp
#include <Arduino.h>
#include "serial_menu.h"

void setup() {
    Serial.begin(115200);
    //init display , buttons, 50
    bt::menuInit();
}

void loop() {
    bt::menuLoop();
}
