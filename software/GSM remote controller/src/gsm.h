#include <regex.h>

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

void parse_command(String command, String value, bool reply_required) {
    // TODO: To be continued
    Serial.println(command);
    Serial.println(value);
    Serial.println(reply_required);
}

void read_sms_message(int sms_index) {
    String message = "";
    String sender = "";
    modem.readSMS(sms_index, sender, message);
    Serial.print("Message From: ");
    Serial.print(sender);
    Serial.println(" and the message is: ");
    Serial.println(message);

    // TODO: Check if sender is allowed before going further

    regex_t preg;
    const char *pattern = ";\\(+*\\)\\([[:alnum:]]*\\)=\\([[:alnum:]]*\\)";
    int rc;
    size_t nmatch = 4;
    regmatch_t pmatch[4];
    if(0 != (rc = regcomp(&preg, pattern, 0))) {
        Serial.println("Unable to compile regexp");
        regfree(&preg);
        return;
    }
    if (0 == (rc = regexec(&preg, message.c_str(), nmatch, pmatch, 0))) { // Regex match
        // if the message contains a "+", it means a reply is required
        bool bReplyRqst = (message.substring(pmatch[1].rm_eo, pmatch[1].rm_so)).length() == 1;
        String sCommand = message.substring(pmatch[2].rm_eo, pmatch[2].rm_so);
        String sValue = message.substring(pmatch[3].rm_eo, pmatch[3].rm_so);
        parse_command(sCommand, sValue, bReplyRqst);
    } else {  // Regex doesn't match
        Serial.println("Invalid command. Ignoring.");
    }
    regfree(&preg);
}

void parse_gsm_event_message() {
    String *pGsmEventData;
    xQueueReceive(qGsmEventData, &pGsmEventData, portMAX_DELAY);
    String sGsmEventData = *pGsmEventData;

    if(sGsmEventData.indexOf("CMTI:") > 0) {  // New message has been received
        int sms_index = modem.newMessageInterrupt(sGsmEventData);
        Serial.print("New SMS arrived at index: ");
        Serial.println(sms_index);
        read_sms_message(sms_index);
        modem.deleteSMS(sms_index);
    } else {
        Serial.println("Unkown GSM event: ");
        Serial.println(sGsmEventData);
        return;
    }
}

void setup_gsm_modem() {
    Serial.println("Initializing GSM module...");
    GsmSerial.begin(57600, SWSERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);
    delay(100);

    // Autobaud communication with modem 
    if (!modem.testAT(5000L)) // Timeout 5s to respond to AT
        Serial.println("Modem not responding to AT command. Autobauding failed.");
    
    // Queue of string pointers to get the data sent by the modem on RI events
    qGsmEventData = xQueueCreate(10, sizeof(String *));
    // Enable interrupt from SIM808 GSM module
    pinMode(GSM_INT_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(GSM_INT_PIN), ISR_GSM_RI, RISING);

    Serial.println("Initializing GSM module done");
}
