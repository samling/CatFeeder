#include <ArduinoJson.h>
#include <Servo.h>
#include <DS1307RTC.h>
#include <EthernetV2_0.h>
#include <EthernetUdpV2_0.h>         // UDP library from: bjoern@cs.stanford.edu 12/30/2008
#include <SerLCD.h>
#include <SoftwareSerial.h>
#include <RTClib.h>
#include <SPI.h>
#include <Time.h>
#include <TimerOne.h>
#include <Wire.h>

#define PRINT_USA_DATE

#define MOMENTARY_PIN 2
#define SDCARD_CS 4
#define SERVO_PIN 9

#define SQW_INPUT_PIN A4   // Input pin to read SQW
#define SQW_OUTPUT_PIN 13 // LED to indicate SQW's state

// Button states for momentary switch
int buttonState, val;

// Interrupt for alerts
bool alert = 0;

// Create clock
RTC_DS1307 rtc;

// Define the days of the week
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// Set network connection details
byte mac[] = { 0xFE, 0xED, 0xFE, 0xED, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 66);
EthernetServer server(3001);

// Create servo
Servo servo;

// Initialize LCD
serLCD lcd(5);

void setup()
{
  // Initialize serial connection
  Serial.begin(9600);

  // Check for the DS1307
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  // Pull the momentary switch high
  pinMode(MOMENTARY_PIN, INPUT);
  digitalWrite(MOMENTARY_PIN, HIGH);

  // Set up the clock
  pinMode(SQW_INPUT_PIN, INPUT_PULLUP);
  pinMode(SQW_OUTPUT_PIN, OUTPUT);
  digitalWrite(SQW_OUTPUT_PIN, digitalRead(SQW_INPUT_PIN));

  // Pull the ethernet shield SD card slot high
  pinMode(SDCARD_CS, OUTPUT);
  digitalWrite(SDCARD_CS, HIGH);

  // Initialize clock library
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Initialize ethernet connection and start a server
  Ethernet.begin(mac, ip);
  Serial.println(Ethernet.localIP());
  server.begin();
}

void loop()
{
  // Listen for and handle JSON requests
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

        // If the request is a POST request, parse the JSON data and
        // react accordingly
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
          
          // Send a response to the client
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: application/json");
          client.println();
          client.println("{\"result\":\"success\"}");
          client.stop();

          // Manual feed with prespecified portion size (really a delay between start and stop of the auger)
          manualFeed(portionSize);
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
    //printDateTime();
    showStatus(Ethernet.localIP());
  }

  // Read the momentary push button
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
}

// Write to both lines of the LCD
void displayTwoLineMessage(String line1, String line2) {
  lcd.selectLine(1);
  lcd.clearLine(1);
  lcd.print(line1);
  lcd.selectLine(2);
  lcd.clearLine(2);
  lcd.print(line2);
}

// Clear the LCD screen
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
  servo.write(-2000); // Going in the clockwise direction
  delay(portionSize);
  servo.detach();

  clearLCD();
  alert = 0;
}

// Print the date and time to the serial monitor
void printDateTime() {
  DateTime now = rtc.now();
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

  Serial.print(" since midnight 1/1/1970 = ");
  Serial.print(now.unixtime());
  Serial.print("s = ");
  Serial.print(now.unixtime() / 86400L);
  Serial.println("d");;
  delay(1000);
}

// Display the date and time on the LCD
//void showDateTime(int8_t lastSecond) {
//  DateTime now = rtc.now();
//  if (now.second() != lastSecond) {
//    lcd.selectLine(1);
//    lcd.clearLine(1);
//    int adjustedHour = (now.hour() - 14 > 0 ? now.hour() - 14 : now.hour() + 10);
//    if (adjustedHour < 10) {
//      lcd.print(String("0"));
//    }
//    lcd.print(String(adjustedHour) + ":");
//    if (now.minute() < 10) {
//      lcd.print(String("0"));
//    }
//    lcd.print(String(now.minute()) + ":");
//    if (now.second() < 10) {
//      lcd.print(String("0"));
//    }
//    lcd.print(String(now.second()));
//    delay(1000);
//  }
//
//  lcd.selectLine(2);
//  lcd.clearLine(2);
//  lcd.print(String(now.month()) + "/" + String(now.day()) + "/" + String(now.year()));
//}

// Print the IP address (if connected) to the LCD screen
void showStatus(IPAddress ip) {
  String address = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
  if (address != "0.0.0.0") {
    displayTwoLineMessage("Connected", address);
  } else {
    displayTwoLineMessage("ERROR:", "Not connected");
  }
}

