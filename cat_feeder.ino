#include <ArduinoJson.h>
#include <ServoTimer2.h>
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

#define SDCARD_CS 4

#define MOMENTARY 11

#define SQW_INPUT_PIN A4   // Input pin to read SQW
#define SQW_OUTPUT_PIN 13 // LED to indicate SQW's state

// Button states for momentary switch
int buttonState, val;

// Interrupt for alerts
bool alert = 0;

// Set a test value for the clock
static int8_t lastSecond = -1;

// State of the servo (0 = off, 1 = on)
bool feeding = 0;

// Set network connection details
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 66);
EthernetServer server(3001);

// Create servo
ServoTimer2 servo;

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
   
  servo.attach(A0);
  servo.write(2000);
  delay(portionSize);
  servo.detach();

  clearLCD();
  alert = 0;
}

// Display the date and time on the LCD
void showDateTime(int8_t lastSecond) {
  if (rtc.second() != lastSecond) {
    lcd.selectLine(1);
    lcd.clearLine(1);
    int adjustedHour = (rtc.hour() - 14 > 0 ? rtc.hour() - 14 : rtc.hour() + 10);
    if(adjustedHour < 10) { lcd.print(String("0")); }
    lcd.print(String(adjustedHour) + ":");
    if(rtc.minute() < 10) { lcd.print(String("0")); }
    lcd.print(String(rtc.minute()) + ":");
    if(rtc.second() < 10) { lcd.print(String("0")); }
    lcd.print(String(rtc.second()));
    delay(1000);
  }

  lcd.selectLine(2);
  lcd.clearLine(2);
  lcd.print(String(rtc.month()) + "/" + String(rtc.day()) + "/" + String(rtc.year()));
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
  pinMode(MOMENTARY, INPUT);
  digitalWrite(MOMENTARY, HIGH);

  // Pull the ethernet shield SD card slot high
  pinMode(SDCARD_CS, OUTPUT);
  digitalWrite(SDCARD_CS, HIGH);

  // Read the momentary switch
  buttonState = digitalRead(MOMENTARY);

  // Set up the clock
  pinMode(SQW_INPUT_PIN, INPUT_PULLUP);
  pinMode(SQW_OUTPUT_PIN, OUTPUT);
  digitalWrite(SQW_OUTPUT_PIN, digitalRead(SQW_INPUT_PIN));

  // Initialize clock library
  rtc.begin();
  rtc.writeSQW(SQW_SQUARE_1);
  rtc.autoTime();

  // Initialize ethernet connection and start a server
  Ethernet.begin(mac, ip);
  Serial.println(Ethernet.localIP());
  server.begin();
}

void loop()
{  

   val = digitalRead(MOMENTARY);
       
   if (val != buttonState) {
       if (val == LOW) {
           Serial.println("Button - low");
       } else {
           Serial.println("Button - high");
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
  
        if (c == '\n' && currentLineIsBlank && req_str.startsWith("GET")) {
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
        }
        else if (c == '\n' && currentLineIsBlank && req_str.startsWith("POST")) {
          while (client.available()) {
            Serial.write(client.read());
          }
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println();
          client.println("<HTML><BODY>POST TEST OK!</BODY></HTML>");
          client.stop();
        } else if (c == '\n' && currentLineIsBlank && (!req_str.startsWith("GET") || !req_str.startsWith("POST"))) {
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

  // Update RC data including seconds, minutes, etc.
  rtc.update();
  rtc.set24Hour();

  // Show the date and time unless there's an alert message
  if (alert == 0) {
    //showDateTime(lastSecond);
    showStatus(Ethernet.localIP());
  }

  // Read the state of the SQW pin and show it on the
  // pin 13 LED. (It should blink at 1Hz.)
  digitalWrite(SQW_OUTPUT_PIN, digitalRead(SQW_INPUT_PIN));
}

