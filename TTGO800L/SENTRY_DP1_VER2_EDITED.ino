// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

// Load libraries
#include <Wire.h>
#include <WiFi.h>
#include <TinyGsmClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

Preferences preferences;

const char apn[] = "internet.globe.com.ph";

// Replace with your network credentials
const char *ssid = "thesis";
const char *password = "thesispw";

// Set web server port number to 80
WiFiServer server(80);

// HTTP VARIABLES
String header;
String firstName = "";
String lastName = "";
String userName = "";
String mobileNumbers[4] = {"63", "63", "63", "63"};
String savedMobileNumbers[4] = {"63", "63", "63", "63"}; // Saved numbers to text

// GPS ADDRESS VARIABLES
const size_t bufferSize = 4096;
char buffer[bufferSize];
const char* apiToken = "128816616938358395746x22401";
String gpsAddress = "";
String lac, cid;
String cellapiToken = "pk.9a72fb54d9a5c44a9c2c7e43057afd92";
static const uint32_t GPSBaud = 9600;
unsigned long previousMillis = 0;
const long interval = 30 * 60 * 1000; // 30 minutes in milliseconds - TRIAL FOR 1 minute
// ====================================================================
String location = "";
String smsMessage;
String newContact = "\nSent from Personal Safety by SENTRY BAG: You're receiving this message because you have been added as an emergency contact for " + userName + ".\nPlease call or text " + userName + " directly for updates."; 
// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;
int connect = 0;
bool Request;

long debouncing_time = 50; //Debouncing Time in Milliseconds
volatile unsigned long last_micros;

// TTGO T-Call pins
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define I2C_SDA              21
#define I2C_SCL              22

#define BUTTON_PIN           12  

// Set serial for debug console 
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

void setup() {
  // Set console baud rate
  SerialMon.begin(9600);
  //===================================================================
  
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
  attachInterrupt(BUTTON_PIN, isr, RISING);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Set GSM module baud rate and UART pins
  SerialAT.begin(4800, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  // Restart SIM800 module, it takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  modem.restart();
  // use modem.init() if you don't need the complete restart

  SerialMon.println("Connecting to network...");
  while (connect == 0){
    if (!modem.gprsConnect(apn)) {
      SerialMon.println("Failed to connect to network");
      modem.gprsConnect(apn);
      connect = 0;
    } else {
      SerialMon.println("Connected to network");
      connect += 1;
    }
  }

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  server.begin();
  loadSettings();
  Serial.println("Loaded settings successfully.");
  //===================================================================
}

// Custom splitString function
void splitString(String input, char delimiter, String output[], int outputSize) {
  int partCount = 0;
  int startIdx = 0;
  for (int i = 0; i < input.length() && partCount < outputSize; i++) {
    if (input.charAt(i) == delimiter) {
      output[partCount++] = input.substring(startIdx, i);
      startIdx = i + 1;
    }
  }
  if (startIdx < input.length() && partCount < outputSize) {
    output[partCount++] = input.substring(startIdx);
  }
}

void isr() {
 Request = true;
}

void loop() {
  if (Request){
   Serial.println("Interrupt Request Received!");
   
  if((long)(micros() - last_micros) >= debouncing_time * 1000) {
    Serial.println("The button is pressed");
    getAddress();
    //SerialMon.println(smsMessage);
    delay(10000);
    sendToAllSMS(true);
    last_micros = micros();
  }

  Request = false;
 }
  //WEB SERVER
  WiFiClient client = server.available();

  if (client) {
    handleClient(client);
  } else {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis; // Save the current time for the next interval

      // Send SMS every 30 minutes
      Serial.println("Sending 30 minutes delay message");
      getAddress();
      delay(10000);
      sendToAllSMS(false);
    }    
  }
}
   
void saveMobileNumbers(String request, WiFiClient &client) {
  Serial.println("Saving mobile numbers...");
  String params[4];

  splitString(request, '&', params, 4);
  
  for (int i = 0; i < 4; i++) {
    int equalsIndex = params[i].indexOf('=');
    if (equalsIndex != -1) {
      String encodedValue = params[i].substring(equalsIndex + 1);

      // Trim the HTTP request headers if they exist
      int httpIndex = encodedValue.indexOf("HTTP/");
      if (httpIndex != -1) {
        encodedValue = encodedValue.substring(0, httpIndex);
      }
      
      // Trim leading spaces
      encodedValue.trim();

      Serial.print("Trimmed input: ");
      Serial.println(encodedValue);

      // =====================================================
      if (encodedValue.length() != 12) {
        encodedValue = "63";
      }
      // =====================================================

      mobileNumbers[i] = encodedValue;
    }
  }

  // Send response to client
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();

  client.println("<!DOCTYPE html><html>");
  client.println("<script>alert('Mobile numbers saved successfully!'); window.location.replace('/');</script>");
  client.println("</html>");
  client.stop();

  for (int i = 0; i < 4; i++) {
    if (savedMobileNumbers[i] != mobileNumbers[i]){
      Serial.print("Checking mobile number for SMS: ");
      Serial.println(mobileNumbers[i]);

      Serial.println("Sending SMS to: " + mobileNumbers[i]);

      // =====================================================
      // Check if the mobile number is not "63" before sending SMS
      if (mobileNumbers[i] != "63") {
        Serial.println("Sending SMS to: " + mobileNumbers[i]);
        // Uncomment this line when you are ready to send SMS
        sendOneSMS(mobileNumbers[i]);
      } else {
        Serial.println("Mobile number is '63', no SMS will be sent.");
      }
      // =====================================================
      
      savedMobileNumbers[i] = mobileNumbers[i];
    }
  }

  saveSettings();

  // Print only the mobile numbers in the Serial Monitor
  for (int i = 0; i < 4; i++) {
    Serial.print("Mobile Number ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(mobileNumbers[i]);
  }
}

// =======================================================================
String urldecode(String str) {
  str.replace("+", " ");
  
  String encodedChars[] = {"%21", "%22", "%23", "%24", "%25", "%26", "%27", "%28", "%29", "%2A", "%2B", "%2C", "%2D", "%2E", "%2F", "%3A", "%3B", "%3C", "%3D", "%3E", "%3F", "%40", "%5B", "%5C", "%5D", "%5E", "%5F", "%60", "%7B", "%7C", "%7D", "%7E"};
  String decodedChars[] = {"!", "\"", "#", "$", "%", "&", "'", "(", ")", "*", "+", ",", "-", ".", "/", ":", ";", "<", "=", ">", "?", "@", "[", "\\", "]", "^", "_", "`", "{", "|", "}", "~"};

  for (int i = 0; i < sizeof(encodedChars) / sizeof(encodedChars[0]); i++) {
    str.replace(encodedChars[i], decodedChars[i]);
  }

  return str;
}
// =======================================================================

void saveUserName(String request, WiFiClient &client) {
  int firstNameIndex = request.indexOf("FirstName=");
  int lastNameIndex = request.indexOf("LastName=");
  
  if (firstNameIndex != -1 && lastNameIndex != -1) {
    int firstNameEndIndex = request.indexOf("&", firstNameIndex);
    int lastNameEndIndex = request.indexOf(" HTTP/", lastNameIndex);
    
    if (firstNameEndIndex != -1 && lastNameEndIndex != -1) {
      // firstName = request.substring(firstNameIndex + 10, firstNameEndIndex);
      // lastName = request.substring(lastNameIndex + 9, lastNameEndIndex);
      
      firstName = urldecode(request.substring(firstNameIndex + 10, firstNameEndIndex));
      lastName = urldecode(request.substring(lastNameIndex + 9, lastNameEndIndex));
      
    
      // Combine first and last names into userName
      userName = firstName + " " + lastName;

      //Print debug information
      Serial.println("Received First Name: " + firstName);
      Serial.println("Received Last Name: " + lastName);

      // Print the updated userName in the Serial Monitor
      Serial.print("User Name: ");
      Serial.println(userName);
      saveSettings();

      client.println("<script>alert('User name saved successfully!'); window.location.replace('/');</script>");
      
      newContact = "\nSent from Personal Safety by SENTRY BAG: You're receiving this message because you have been added as an emergency contact for " + userName + ".\nPlease call or text " + userName + " directly for updates.";

    } else {
      Serial.println("Error: End index not found for first or last name.");
    }
  } else {
    Serial.println("Error: First or last name index not found in the request.");
  }
}

void getAddress() {
  String response;
  String coordinates = "";
  SerialAT.println("AT+CREG=2");  // Set the device to report location information
  SerialAT.println("AT+CREG?");   // Get the location information
  delay(5000);

  // Read the response from SIM800
  while (SerialAT.available()) {
    String line = SerialAT.readStringUntil('\n');
    if (line.startsWith("+CREG: 2,1")) { // only run if successful
      SerialMon.println(line);
      // Extract the values from the line
      int comma1Pos = line.indexOf(',');
      int comma2Pos = line.indexOf(',', comma1Pos + 1);
      int comma3Pos = line.indexOf(',', comma2Pos + 1);
      int comma4Pos = line.indexOf(',', comma3Pos + 1);

      // Extract Cell ID (CID), LAC, MNC, and MCC
      lac = line.substring(comma2Pos + 1, comma3Pos);
      cid = line.substring(comma3Pos + 1, comma4Pos);

      // Convert the values to integers
      lac = hexStringToDecimal(lac);  // Convert hexadecimal LAC to decimal
      cid = hexStringToDecimal(cid);
    }
  }
  //SerialMon.println("Making HTTPS request to Unwired Labs API...");

  if (!modem.init()) {
    SerialMon.println("GSM module initialization failed.");
    return;
  }
  TinyGsmClient client(modem);
  // Make an HTTP POST request to Unwired Labs geolocation API using client.connect()
  if (client.connect("ap1.unwiredlabs.com", 80)) {
    SerialMon.println("Connected to Unwired Labs API server");

    // Replace with your actual Cell ID, LAC, MCC, and MNC

    String url = "/v2/process";


    String payload = "{"
                     "\"token\":\""
                     + cellapiToken + "\","
                                  "\"cells\":[{"
                                  "\"radio\":\"umts\","
                                  "\"mcc\":\"515\","
                                  "\"mnc\":\"02\","
                                  "\"lac\":\""
                     + lac + "\","
                             "\"cid\":\""
                     + cid + "\""
                             "}],"
                             "\"address\":2"
                             "}";

    // Print payload for debugging
    SerialMon.println("Payload: " + payload);

    client.print("POST " + url + " HTTP/1.1\r\n");
    client.print("Host: ap1.unwiredlabs.com\r\n");
    client.print("Content-Type: application/json\r\n");
    client.print("Content-Length: " + String(payload.length()) + "\r\n");
    client.print("Connection: close\r\n\r\n");
    client.print(payload);

    delay(1000);

    String readString;

    // Skip HTTP headers
    char endOfHeaders[] = "\r\n\r\n";
    if (!client.find(endOfHeaders)) {
      SerialMon.println(F("Invalid response"));
    }

    while (client.available()) {
      char c = client.read();

      // Skip non-JSON characters until '{' is encountered
      if (c == '{') {
        response = c;  // Include the opening curly brace
        break;
      }
    }

    // Continue reading the JSON content
    while (client.available()) {
      char c = client.read();
      response += c;

      // Break the loop when the end of the JSON content is reached
      if (c == '}') {
        break;
      }
    }

    // Print the cleaned response
    SerialMon.println("Response: " + response);

    client.stop();

    // Parse JSON response
    //StaticJsonDocument<192> doc;
    StaticJsonDocument<bufferSize> doc;
    deserializeJson(doc, response);

    //Extract latitude and longitude
    double latitude = doc["lat"];
    double longitude = doc["lon"];
    const char* address = doc["address"];
    double accuracy = doc["accuracy"];
    accuracy = accuracy/1000;

    coordinates = String(latitude, 8) + "," + String(longitude,8);

    gpsAddress = "\nCoordinates: " + String(coordinates) + "\nLocation: " + address + "\nAccuracy: " + String(accuracy, 3) + " km";

    SerialMon.println(gpsAddress);
    smsMessage = "Sent from SENTRY BAG: You're receiving this message because you're an emergency contact for " + userName + ".\nSharing their real-time location with you: " + gpsAddress ;
    //SerialMon.println(smsMessage);
    client.stop();
  } else {
    SerialMon.println("Failed to connect to OpenCellId server");
  }
}

void sendToAllSMS(bool sendEmergencyMessage) {
  if (sendEmergencyMessage == true){
    // send location + emergency message
    smsMessage = "EMERGENCY ALERT!\n" + smsMessage;
  } 
  for (int i = 0; i < 4; i++){
    String numToText = "+" + savedMobileNumbers[i];
    if (numToText != "+63"){
      SerialMon.print("Sending SMS to: ");
      SerialMon.println(numToText);

      if (modem.sendSMS(numToText, smsMessage)) {
        // SerialMon.println(smsMessage);
        SerialMon.println("Successful sending SMS message");
      } else {
        SerialMon.println("SMS failed to send");
      }
    } 
    delay(5000); // Add a delay to avoid sending multiple messages quickly
  }
}

void sendOneSMS(String numberToText) {
  String numToText = "+" + numberToText;
  if (numToText != "+63"){
    if (modem.sendSMS(numToText, newContact)) {
      // SerialMon.println(smsMessage);
      SerialMon.println("Successful sending SMS message");
    } else {
      SerialMon.println("SMS failed to send");
    }
  }
  delay(2000); // Add a delay to avoid sending multiple messages quickly
}


long hexStringToDecimal(String hexString) {
  long decimalValue = 0;
  int hexLength = hexString.length();

  for (int i = 0; i < hexLength; i++) {
    char c = hexString.charAt(i);
    int digitValue;

    if (c >= '0' && c <= '9') {
      digitValue = c - '0';
    } else if (c >= 'A' && c <= 'F') {
      digitValue = 10 + (c - 'A');
    } else if (c >= 'a' && c <= 'f') {
      digitValue = 10 + (c - 'a');
    } else {
      // Handle invalid characters if necessary
      continue;
    }

    decimalValue = (decimalValue * 16) + digitValue;
  }
  return decimalValue;
}

void saveSettings() {
  preferences.begin("myapp", false); // Open Preferences with my-app namespace. RW-mode (second parameter) is false by default
  preferences.putString("firstName", firstName);
  preferences.putString("lastName", lastName);
  preferences.putString("userName", userName);

  preferences.putString("0", savedMobileNumbers[0]);
  preferences.putString("1", savedMobileNumbers[1]);
  preferences.putString("2", savedMobileNumbers[2]);
  preferences.putString("3", savedMobileNumbers[3]);

  preferences.end(); // Close the Preferences

  Serial.println("Saved USERNAME PREFERENCES");
  Serial.println("Saved MOBILE NUMBERS PREFERENCES");
}

void loadSettings() {
  preferences.begin("myapp", true); // Open Preferences with my-app namespace. Read-only mode set to true  
  firstName = preferences.getString("firstName", "");
  lastName = preferences.getString("lastName", "");
  userName = preferences.getString("userName", "");

  savedMobileNumbers[0] = preferences.getString("0", "63");
  savedMobileNumbers[1] = preferences.getString("1", "63");
  savedMobileNumbers[2] = preferences.getString("2", "63");
  savedMobileNumbers[3] = preferences.getString("3", "63");
    
  preferences.end(); // Close the Preferences

  Serial.println("Loaded username: " + userName);

  for (int i = 0; i < 4; i++) {
    Serial.print("Preferences Mobile Numbers ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(savedMobileNumbers[i]);
  }
}

void handleClient(WiFiClient& client){
  Serial.println("New Client.");
  String currentLine = "";

  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      header += c;
      if (c == '\n') {
        if (currentLine.length() == 0) {
          if (header.indexOf("GET /saveMobile") >= 0) {
            saveMobileNumbers(header, client);
          } else if(header.indexOf("GET /saveName") >= 0) {
            saveUserName(header, client);
          }

          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println("Connection: close");
          client.println();

          // Display the HTML web page with IP address and mobile number input fields
          client.println("<!DOCTYPE html>");
          client.println("<html lang=\"en\">");
          client.println("<head>");
          client.println("    <meta charset=\"UTF-8\">");
          client.println("    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
          client.println("    <title>SENTRY Bag</title>");
          client.println("</head>");
          client.println("<style>");
          client.println("*{");
          client.println("    margin: 0;");
          client.println("    padding: 0;");
          client.println("    font-family: century gothic;");
          client.println("    box-sizing: border-box;");
          client.println("    outline: none;");
          client.println("    border: none;");
          client.println("    text-decoration: none;");
          client.println("}");
          client.println("");
          client.println("body{");
          client.println("    background-size: cover;");
          client.println("}");
          client.println(".container{");
          client.println("    min-height: 100vh;");
          client.println("    display: flex;");
          client.println("    align-items: center;");
          client.println("    justify-content: center;");
          client.println("    padding: 20px;");
          client.println("    padding-bottom: 60px;");
          client.println("}");
          client.println("");
          client.println(".container .content{");
          client.println("    text-align: center;");
          client.println("}");
          client.println("");
          client.println("h1{");
          client.println("    font-size: 50px;");
          client.println("    color: #BF0606;");
          client.println("}");
          client.println("");
          client.println("h3{");
          client.println("    font-size: 20px;");
          client.println("    color: black;");
          client.println("}");
          client.println("");            
          client.println(".btn, .mess{");
          client.println("    display: inline-block;");
          client.println("    padding: 30px 40px;");
          client.println("    font-size: 16px;");
          client.println("    background: #BF0606;");
          client.println("    color: white;");
          client.println("    margin: 0 5px;");
          client.println("    border-radius: 20px;");
          client.println("    cursor: pointer;");
          client.println("    width: 90%;");
          client.println("}");
          client.println("");
          client.println(".btn:hover, .mess:hover{");
          client.println("    background: forestgreen;");
          client.println("    color: white;");
          client.println("}");
          client.println("");
          client.println(".rfid{");
          client.println("    display: inline-block;");
          client.println("    padding: 30px 40px;");
          client.println("    font-size: 16px;");
          client.println("    background: #BF0606;");
          client.println("    color: white;");
          client.println("    margin: 0 5px;");
          client.println("    border-radius: 20px;");
          client.println("    cursor: pointer;");
          client.println("    width: 90%;");
          client.println("}");
          client.println("");
          client.println("");
          client.println(".switch{");
          client.println("    position: relative;");
          client.println("    margin-left: 25%;");
          client.println("    display: inline-block;");
          client.println("    width: 250px;");
          client.println("    height: 60px;");
          client.println("}");
          client.println("");
          client.println(".slider{");
          client.println("    position: absolute;");
          client.println("    cursor: pointer;");
          client.println("    top: 0;");
          client.println("    left: 0;");
          client.println("    right: 0;");
          client.println("    bottom: 0;");
          client.println("    background-color: gray;");
          client.println("    transition: .4s;");
          client.println("    border-radius: 50px;");
          client.println("    width: 110px;");
          client.println("}");
          client.println("");
          client.println(".switch input{display: none}");
          client.println("");
          client.println(".slider:before {");
          client.println("    position: absolute;");
          client.println("    content: \"\";");
          client.println("    height: 50px;");
          client.println("    width: 50px;");
          client.println("    left: 5px;");
          client.println("    bottom: 5px;");
          client.println("    background-color: white;");
          client.println("    transition: 0.4s;");
          client.println("    border-radius: 50px;");
          client.println("}");
          client.println("");
          client.println("input:checked + .slider{");
          client.println("    background-color: forestgreen;");
          client.println("}");
          client.println("");
          client.println("input:checked + .slider:before{");
          client.println("    transform: translateX(50px);");
          client.println("}");
          client.println("");
          client.println(".message-box {");
          client.println("        color: black; ");
          client.println("        font-size: 16px; ");
          client.println("    }");

          client.println(".message-box textarea{");
          client.println("    min-width: 300px;");
          client.println("    max-width: 300px;");
          client.println("    min-height: 150px;");
          client.println("    max-height: 150px;");
          client.println("    padding: 10px 10px 0 20px;");
          client.println("    border: 4px solid #BF0606;");
          client.println("    border-radius: 10px; ");
          client.println("}");

          client.println(".contact textarea{");
          client.println("    min-width: 300px;");
          client.println("    max-width: 300px;");
          client.println("    min-height: 50px;");
          client.println("    max-height: 50px;");
          client.println("    padding: 10px 10px 10px 20px;");
          client.println("    border: 4px solid #BF0606;");
          client.println("    border-radius: 10px; ");
          client.println("}");

          client.println(".inputField{");
          client.println("    position: absolute;");
          client.println("    border: 4px solid #BF0606;");
          client.println("    min-width: 300px;");
          client.println("    max-width: 300px;");
          client.println("    min-height: 50px;");
          client.println("    max-height: 50px;");
          client.println("    padding: 10px 10px 10px 20px;");
          client.println("    border: 4px solid #BF0606;");
          client.println("    border-radius: 10px; ");
          client.println("}");

          client.println(".submit{");
          client.println("    display: inline-block;");
          client.println("    padding: 30px 40px;");
          client.println("    font-size: 16px;");
          client.println("    background: #BF0606;");
          client.println("    color: white;");
          client.println("    margin: 0 5px;");
          client.println("    border-radius: 20px;");
          client.println("    cursor: pointer;");
          client.println("    width: 90%;");
          client.println("}");

          client.println(".submit:hover{");
          client.println("    background: forestgreen;");
          client.println("    color: white;");
          client.println("}");

          client.println(".contacts .container {");
          client.println("    min-height: 100vh;");
          client.println("    display: flex;");
          client.println("    align-items: center;");
          client.println("    justify-content: center;");
          client.println("    padding: 20px;");
          client.println("    padding-bottom: 60px;");
          client.println("}");

          client.println(".contacts .content {");
          client.println("    text-align: center;");
          client.println("}");

          client.println(".contact-form {");
          client.println("    display: grid;");
          client.println("    align-items: center;");
          client.println("    margin-bottom: 15px;");
          client.println("}");

          client.println(".contact-form label {");
          client.println("    text-align: left;");
          client.println("    white-space: nowrap;");
          client.println("}");

          client.println(".inputField {");
          client.println("    width: 60%;");
          client.println("    box-sizing: border-box;");
          client.println("    position: static; ");
          client.println(" }");
          client.println("</style>");
          client.println("");
          client.println("<body>");
          // ===============================================================================================
          client.println("<header>");            
          client.println("    <!--Landing page-->");
          client.println("    <div class = \"container\">");
          client.println("        <div class = \"content\">");
          client.println("            <h1>SENTRY Bag</h1>");
          client.println("            <br><br>");
          client.println("            <a href=\"#userName\" class=\"btn\"><b>Username</b></a> <br><br><br>");
          client.println("            <a href=\"#contacts\" class=\"btn\"><b>Emergency Contact</b></a> <br><br><br>");
          client.println("            <a class=\"rfid\"><b>Anti-theft OFF/ON</b><br><br>");
          client.println("            <label class=\"switch\">");
          client.println("                <input type=\"checkbox\" checked>");
          client.println("                    <span class=\"slider\"></span></a>");
          client.println("            </label>");
          client.println("        </div>");
          client.println("    </div>");
          client.println("</header>");
          // ===============================================================================================
          // For adding USERNAME
          client.println("<section id=\"userName\" class=\"message\">");
          client.println("    <div class=\"container\">");
          client.println("        <div class=\"content\">");
          client.println("            <form action=\"saveName\" class=\"message-box\">");
          client.println("                <input type=\"text\" placeholder=\"First Name\" name=\"FirstName\" class=\"inputField\" value=\"" + firstName + "\" required><br>");
          client.println("                <input type=\"text\" placeholder=\"Last Name\" name=\"LastName\" class=\"inputField\" value=\"" + lastName + "\" required><br>");
          client.println("                <input type=\"submit\" value=\"Save Name\" class=\"mess\">");
          client.println("            </form>");
          client.println("        </div>");
          client.println("    </div>");
          client.println("</section>");
          // ===============================================================================================
          // CONTACT NUMBERS
          client.println("<section id=\"contacts\" class=\"contacts\">");
          client.println("    <div class=\"container\">");
          client.println("        <div class=\"content\">");
          client.println("            <form action=\"saveMobile\" class=\"contact-form-container\">");
          client.println("                <h1>Contact List</h1><br><br>");
          client.println("                <div class=\"contact-form\">");

          // Loop through mobileNumbers array
          for (int i = 0; i < 4; i++) {
            // Generate input field for each mobile number
            client.println("                   <label for=\"mobileNumber" + String(i + 1) + "\">Mobile Number " + String(i + 1) + ":</label>");
            client.print("                     <input type=\"text\" id=\"mobileNumber" + String(i + 1) + "\" name=\"mobileNumber" + String(i + 1) + "\" class=\"inputField\" value=\"" + savedMobileNumbers[i] + "\" maxlength=\"12\" ><br>");
          }

          client.println("                    </div>");

          // Save Mobile button
          client.println("                      <input class=\"submit\" type=\"submit\" value=\"Save Mobile\">");

          client.println("            </form>");
          client.println("        </div>");
          client.println("    </div>");
          client.println("</section>");
          // ===============================================================================================
    
          // ===============================================================================================
          client.println("</body></html>");
          client.println();

          // Break out of the while loop
          break;
        } else {
          currentLine = "";
        }
      } else if (c != '\r') {
        currentLine += c;
      }
    }
  }

  header = "";
  client.stop();
  Serial.println("Client disconnected.");
  Serial.println("");
}
