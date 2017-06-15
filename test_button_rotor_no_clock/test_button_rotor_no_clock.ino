#include <ArduinoJson.h>
#include <Servo.h>
#include <DS1307RTC.h>
#include <EthernetV2_0.h>
#include <EthernetUdpV2_0.h>         // UDP library from: bjoern@cs.stanford.edu 12/30/2008
#include <SerLCD.h>
#include <SoftwareSerial.h>
#include <SparkFunDS1307RTC.h>
#include <SPI.h>
#include <Time.h>
#include <TimerOne.h>
#include <Wire.h>

#define PRINT_USA_DATE

#define MOMENTARY_PIN 2
#define SDCARD_CS 4
#define SERVO_PIN 9

// Button states for momentary switch
int buttonState, val;

// Interrupt for alerts
bool alert = 0;

// State of the servo (0 = off, 1 = on)
bool feeding = 0;

// Set network connection details
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 66);
EthernetServer server(3001);

// Create servo
Servo servo;

// Initialize LCD
serLCD lcd(5);

void displayTwoLineMessage(String line1, String line2) {
  lcd.selectLine(1);
  lcd.clearLine(1);
  lcd.print(line1);
  lcd.selectLine(2);
  lcd.clearLine(2);
  lcd.print(line2);
}

void clearLCD() {
  lcd.clearLine(1);
  lcd.clearLine(2);
}

// Manually initiate a feed cycle using portionSize as the delay time
void manualFeed(long portionSize) {
  if (portionSize > 5000) {
    portionSize = 5000;
  }
  alert = 1;

  // Convert portionSize into a time in seconds
  char buff[2];
  sprintf(buff, "%.5ld", portionSize);
  char withDot[3];
  withDot[0] = buff[1];
  withDot[1] = '.';
  withDot[2] = buff[2];
  withDot[3] = '\0';

  displayTwoLineMessage("MANUAL FEED", "T:" + String(withDot) + "s");

  servo.attach(SERVO_PIN);
  servo.write(2000);
  delay(portionSize);
  servo.detach();

  clearLCD();
  alert = 0;
}

void showStatus(IPAddress ip) {
  String address = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
  if (address != "0.0.0.0") {
    displayTwoLineMessage("Connected", address);
  } else {
    displayTwoLineMessage("Not Connected!", "");
  }
}

void setup()
{
  // Initialize serial connection
  Serial.begin(9600);

  // Pull the momentary switch high
  pinMode(MOMENTARY_PIN, INPUT);
  digitalWrite(MOMENTARY_PIN, HIGH);

  // Pull the ethernet shield SD card slot high
  pinMode(SDCARD_CS, OUTPUT);
  digitalWrite(SDCARD_CS, HIGH);

  // Initialize ethernet connection and start a server
  Ethernet.begin(mac, ip);
  Serial.println(Ethernet.localIP());
  server.begin();
}

void loop()
{

  val = digitalRead(MOMENTARY_PIN);

  if (val != buttonState) {
    if (val == LOW) {
      Serial.println("Button - low");
      Serial.println(String(val));
      manualFeed(2000);
    } else {
      Serial.println("Button - high");
      Serial.println(String(val));
    }
  }

  buttonState = val;

  EthernetClient client = server.available();
  if (client) {
    boolean currentLineIsBlank = true;
    String req_str = "";
    String data = "";
    StaticJsonBuffer<200> jsonBuffer;

    while (client.connected()) {
      while (client.available()) {
        // Read the client request into a string
        char c = client.read();
        req_str += c;
        Serial.write(c);

        if (c == '\n' && currentLineIsBlank && req_str.startsWith("POST")) {
          while (client.available()) {
            // Read the request data into a string
            char d = client.read();
            data += d;
            Serial.write(d);
          }
          Serial.write('\n');

          // Create a parsable JSON object
          JsonObject& root = jsonBuffer.parseObject(data);
          //const char* portionSize = root["portionSize"];
          int portionSize = root["portionSize"];

          // Manual feed with prespecified portion size (really a delay between start and stop of the auger)
          manualFeed(portionSize);

          // Send a response to the client
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: application/json");
          client.println();
          client.println("{\"result\":\"success\"}");
          client.stop();
        } else if (c == '\n' && currentLineIsBlank && !req_str.startsWith("POST")) {
          while (client.available()) {
            Serial.write(client.read());
          }
          client.println("HTTP/1.1 404 NOT FOUND");
          client.println("Content-Type: text/html");
          client.println();
          client.println("<HTML><BODY>Page not found</BODY></HTML>");
          client.stop();
        }
        else if (c == '\n') {
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
  }

  // Show the date and time unless there's an alert message
  if (alert == 0) {
    //showDateTime(lastSecond);
    showStatus(Ethernet.localIP());
  }
}

