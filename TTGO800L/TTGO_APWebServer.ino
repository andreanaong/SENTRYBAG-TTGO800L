/*********
  Rui Santos
  Complete project details at http://randomnerdtutorials.com  
*********/

// Load Wi-Fi library
#include <WiFi.h>

// Replace with your network credentials
const char *ssid = "thesis";
const char *password = "thesispw";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

String mobileNumbers[3] = {"0", "0", "0"};

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

#include <Wire.h>
#include <TinyGsmClient.h>

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

void setup() {
  // Set console baud rate
  SerialMon.begin(115200);
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

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  server.begin();
}

void loop(){
  WiFiClient client = server.available();

  if (client) {
    Serial.println("New Client.");
    String currentLine = "";

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        header += c;
        if (c == '\n') {
          if (currentLine.length() == 0) {
            if (header.indexOf("GET /save") >= 0) {
              saveMobileNumbers(header);
            }

            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // Display the HTML web page with IP address and mobile number input fields
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<style>html { font-family: monospace; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".inputField { font-size: 20px; margin: 10px;}</style></head>");

            client.println("<body><h1>ESP32 Web Server</h1>");
            client.println("<p>IP Address: " + WiFi.softAPIP().toString() + "</p>");
            client.println("<form action=\"/save\">");
            client.println("Mobile Number 1: <input type=\"text\" name=\"mobileNumber0\" class=\"inputField\" value=\"" + mobileNumbers[0] + "\"><br>");
            client.println("Mobile Number 2: <input type=\"text\" name=\"mobileNumber1\" class=\"inputField\" value=\"" + mobileNumbers[1] + "\"><br>");
            client.println("Mobile Number 3: <input type=\"text\" name=\"mobileNumber2\" class=\"inputField\" value=\"" + mobileNumbers[2] + "\"><br>");
            client.println("<input type=\"submit\" value=\"Save\">");
            client.println("</form>");
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
}

void saveMobileNumbers(String request) {
  for (int i = 0; i < 3; i++) {
    String paramName = "mobileNumber" + String(i);
    int paramIndex = request.indexOf(paramName);
    if (paramIndex >= 0) {
      int valueIndex = request.indexOf("=", paramIndex) + 1;
      int endIndex = request.indexOf("&", valueIndex);
      if (endIndex == -1) {
        endIndex = request.length();
      }
      mobileNumbers[i] = request.substring(valueIndex, endIndex);
    }
  }

  Serial.println("Mobile numbers saved:");
  Serial.println("Mobile Number 1: " + mobileNumbers[0]);
  Serial.println("Mobile Number 2: " + mobileNumbers[1]);
  Serial.println("Mobile Number 3: " + mobileNumbers[2]);
}