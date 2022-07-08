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

volatile int sms_index = 0;

void IRAM_ATTR ISR_GSM_RI(){
  String msg = "";

  while(GsmSerial.available()) {
    char c = GsmSerial.read();
    msg += c;   
  }

  // TODO: Move the code below outside ISR ?
  // Or use ets_printf instead of Serial.println as least
  if(msg.indexOf("CMTI:") > 0) {  // New message has been received
    Serial.print("New SMS arrived at index :- ");
    sms_index = modem.newMessageInterrupt(msg);
    Serial.println(sms_index);
    msg = "";
  } else {
    Serial.println("false alarm");
    Serial.println(msg);
    return;
  }
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
  
  // Enable interrupt from SIM808 GSM module
  pinMode(GSM_INT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(GSM_INT_PIN), ISR_GSM_RI, HIGH);  // TODO: use RISING instead of HIGH ? Check SIM808 datasheet how long RI stays high

  Serial.println("Initializing done");
}

// TODO: TinyGSM is a blocking library so use FreeRTOS and tasks to use the second core of ESP32
void loop() {
  Serial.println("Enter the SMS index to Read ");
  while(Serial.available() == 0){

  }
  int i = Serial.parseInt();
  Serial.print("Reading message at :- ");
  Serial.println(i);
  read_sms_message(i);

  // if interrupt occurs, Fetch the SMS from indexed MSSG number.   
  if(sms_index > 0){
    read_sms_message(sms_index);
    sms_index=0;
  }
}
