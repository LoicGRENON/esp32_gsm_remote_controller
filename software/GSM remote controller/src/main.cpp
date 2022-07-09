#include <Arduino.h>
#include "gsm.h"

void setup() {
    Serial.begin(115200);
    Serial.println("Initializing ...");

    setup_gsm_modem();

    Serial.println("Initializing done");
}

// TODO: TinyGSM is a blocking library so use FreeRTOS and tasks to use the second core of ESP32
void loop() {
    parse_gsm_event_message();
    delay(500);
}
