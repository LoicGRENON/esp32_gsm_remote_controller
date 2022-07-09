#include <Arduino.h>

// Define modem type
#define TINY_GSM_MODEM_SIM808
// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG Serial
// Include modified version of TinyGSM library which supports SMS reading
// from https://github.com/satyamkr80/TinyGSM
// see PR #665: https://github.com/vshymanskyy/TinyGSM/pull/665
#include <TinyGSM.h>
#include <SoftwareSerial.h>

// Define serial pins for modem
#define GSM_RX_PIN 16
#define GSM_TX_PIN 17
// Define interrupt pin to use for modem RI pin
#define GSM_INT_PIN 18

SoftwareSerial GsmSerial;
TinyGsm modem(GsmSerial);

QueueHandle_t qGsmEventData;
String sGsmEventDataISR;

void IRAM_ATTR ISR_GSM_RI(){
    sGsmEventDataISR = "";
    while(GsmSerial.available()) {
        char c = GsmSerial.read();
        sGsmEventDataISR += c;
    }

    // Send the string pointer to queue for further use
    String * pGsmEventData = &sGsmEventDataISR;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendToBackFromISR(qGsmEventData, &pGsmEventData, &xHigherPriorityTaskWoken);
}

void read_sms_message(int sms_index) {
    // TODO: add a readSMS() declaration to get the senderID at once (one AT+CMGR command)
    // Currently both readSMS and getSenderID send AT+CMGR commands which is slow
    // => pass these variables as reference of the function
    String SMS = modem.readSMS(sms_index);
    String ID = modem.getSenderID(sms_index);
    Serial.print("Message From : ");
    Serial.println(ID);
    Serial.println(" and the message is ");
    Serial.println(SMS);
}

void parse_gsm_event_message() {
    String *pGsmEventData;
    xQueueReceive(qGsmEventData, &pGsmEventData, portMAX_DELAY);
    String sGsmEventData = *pGsmEventData;

    if(sGsmEventData.indexOf("CMTI:") > 0) {  // New message has been received
        Serial.print("New SMS arrived at index :- ");
        //Serial.println(sGsmEventData);
        // TODO: remove the need to use newMessageInterrupt()
        int sms_index = modem.newMessageInterrupt(sGsmEventData);
        Serial.println(sms_index);
        read_sms_message(sms_index);
    } else {
        Serial.println("false alarm");
        Serial.println(sGsmEventData);
        return;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Initializing Serial...");

    // TODO: Try using higher baudrate
    // Maybe check using TinyGsmAutoBaud() - But not in production code !
    GsmSerial.begin(9600, SWSERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);
    Serial.println("Initializing GSM module...");
    delay(100);

    // Autobaud communication with modem
    // TODO: wait for OK ?
    GsmSerial.write("AT\r");
    delay(1000);
    
    // Queue of string pointers to get the data sent by the modem on RI events
    qGsmEventData = xQueueCreate(10, sizeof(String *));
    // Enable interrupt from SIM808 GSM module
    pinMode(GSM_INT_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(GSM_INT_PIN), ISR_GSM_RI, RISING);

    Serial.println("Initializing done");
}

// TODO: TinyGSM is a blocking library so use FreeRTOS and tasks to use the second core of ESP32
void loop() {
    parse_gsm_event_message();
    delay(500);
}
