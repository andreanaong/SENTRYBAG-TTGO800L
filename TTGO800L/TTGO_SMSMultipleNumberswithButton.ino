// SIM card PIN (leave empty if not defined)
const char simPIN[] = "";

// Your phone number to send SMS: + (plus sign) and country code, for the Philippines +63, followed by phone number
// SMS_TARGET Example for the Philippines +639XXXXXXXXX
const String SMS_TARGET[2] = {"+639XXXXXXXXX", "+639XXXXXXXXX"};

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

#include <Wire.h>
#include <TinyGsmClient.h>
#include <SoftwareSerial.h>
#include <TinyGPS.h>

#define rxPin 3  
#define txPin 4   
SoftwareSerial gpsSerial(rxPin, txPin);
TinyGPS gps;

float storedLatitude = 0.0;
float storedLongitude = 0.0;

// TTGO T-Call pins
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define I2C_SDA              21
#define I2C_SCL              22

// ETO YUNG PIN NUNG BUTTON bale pag clinick yung button dun lang siya mag sesend
#define BUTTON_PIN           12  // pwedeng ibang pin to so palitan mo nalang dipende sa pin na pagsasalpakan mo

// Set serial for debug console (to Serial Monitor, default speed 4800)
#define SerialMon Serial
// Set serial for AT commands (to SIM800 module)
#define SerialAT Serial1

// Define the serial console for debug prints, if needed
//#define DUMP_AT_COMMANDS

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

bool setPowerBoostKeepOn(int en) {
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  if (en) {
    Wire.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    Wire.write(0x35); // 0x37 is default reg value
  }
  return Wire.endTransmission() == 0;
}

void sendSMS() {
  for (int i = 0; i < 2; i++){
    String smsMessage = "ESP32 Trial SMS test for Multiple numbers W/ button";
    if (modem.sendSMS(SMS_TARGET[i], smsMessage)) {
      SerialMon.println(smsMessage);
    } else {
      SerialMon.println("SMS failed to send");
    }
    // Add a delay to avoid sending multiple messages quickly
    delay(5000);
    digitalWrite(13, HIGH);
    delay(500);
    digitalWrite(13, LOW);
  }
}

void readGPS() {
  while (gpsSerial.available() > 0) {
    if (gps.encode(gpsSerial.read())) {
      float latitude, longitude;
      unsigned long fix_age;

      gps.f_get_position(&latitude, &longitude, &fix_age);

      if (fix_age == TinyGPS::GPS_INVALID_AGE) {
        Serial.println("Invalid GPS data");
      } else {
        Serial.print("Latitude: ");
        Serial.println(latitude, 6);
        Serial.print("Longitude: ");
        Serial.println(longitude, 6);
        storedLatitude = latitude;
        storedLongitude = longitude;
      }
    }
  }
}

void setup() {
  // Set console baud rate
  SerialMon.begin(9600);
  gpsSerial.begin(9600);
  SerialMon.println("Initializing...");
  delay(1000);

  // Keep power when running from battery
  Wire.begin(I2C_SDA, I2C_SCL);
  bool isOk = setPowerBoostKeepOn(1);
  SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));

  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Assuming the button is connected to GND when pressed
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  //LED FOR TESTING
  pinMode(13, OUTPUT);

  // Set GSM module baud rate and UART pins
  SerialAT.begin(4800, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  // Restart SIM800 module, it takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  modem.restart();
  // use modem.init() if you don't need the complete restart
}


// BALE NILAGAY KO SA ISANG FUNCTION YUNG SEND SMS PARA MADALI HANAPIN AT MAGETS
// SO AYUN TRY TRY MO NALANG TO IF GAGANA THEN CHAT KA LANG IF MAY PROBLEM
void loop() {
  // Check if the button is pressed
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(100); // Debounce delay
    while (digitalRead(BUTTON_PIN) == LOW) {} // Wait for button release
    SerialMon.println("Button Clicked");
    // Unlock your SIM card with a PIN if needed
    if (strlen(simPIN) && modem.getSimStatus() != 3) {
      modem.simUnlock(simPIN);
      SerialMon.println("Successful PIN Unlock");
    }
    
    // Call the function to send the SMS to multiple numbers
      sendSMS();
      readGPS();
      delay(1000); 
  }

  // Your other loop logic here
  delay(1);
}
