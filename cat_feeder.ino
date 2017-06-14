#include <ArduinoJson.h>
#include <ServoTimer2.h>
#include <ClickEncoder.h>
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

#define dpInEncoderA 2
#define dpInEncoderB 3
#define dpInEncoderPress 4

#define SQW_INPUT_PIN A4   // Input pin to read SQW
#define SQW_OUTPUT_PIN 13 // LED to indicate SQW's state

// Initialize encoder
ClickEncoder *encoder;
int16_t encLast, encValue;

// Interrupt for alerts
bool alert = 0;

// Set a test value for the clock
static int8_t lastSecond = -1;

// State of the servo (0 = off, 1 = on)
bool feeding = 0;

// Encoder needs to run timer interrupt service routine once every millisecond
void timerIsr() {
  encoder->service();
}

// Set network connection details
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 66);
EthernetServer server(3001);

// Create servo
ServoTimer2 servo;

// Initialize LCD
serLCD lcd(5);

// Show if the encoder is accelerated (faster turning -> higher increase) or not
void displayAccelerationStatus() {
  lcd.clear();
  lcd.print(encoder->getAccelerationEnabled() ? "on" : "off");
}

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

void setup()
{
  // Initialize serial connection
  Serial.begin(9600);

  // Set up the encoder
  encoder = new ClickEncoder(dpInEncoderA, dpInEncoderB, dpInEncoderPress);
  
  // Initialize timer for processing encoder interrupts and attach the timer interrupt service routine
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);

  // Initialize the last state of the encoder as -1
  encLast = -1;
  
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
  
//  // Read encoder value
//  encValue += encoder->getValue();
//  if (encValue != encLast) {
//    encLast = encValue;
//    lcd.clear();
//    lcd.selectLine(1);
//    lcd.print("Encoder Value:");
//    lcd.selectLine(2);
//    lcd.print(encValue);
//  }

  // Show the date and time unless there's an alert message
  if (alert == 0) {
    showDateTime(lastSecond);
  }

  // Process encoder button actions
  ClickEncoder::Button b = encoder->getButton();
  if (b != ClickEncoder::Open) {
    #define VERBOSECASE(label) case label: lcd.selectLine(1); lcd.print(#label); break;

    switch (b) {
      VERBOSECASE(ClickEncoder::Pressed);
      VERBOSECASE(ClickEncoder::Held);
      VERBOSECASE(ClickEncoder::Released);
      case ClickEncoder::DoubleClicked:
        Serial.println("ClickEncoder::DoubleClicked");
        encoder->setAccelerationEnabled(!encoder->getAccelerationEnabled());
        Serial.print("Acceleration is:");
        Serial.println(encoder->getAccelerationEnabled() ? "enabled" : "disabled");
        break;
      case ClickEncoder::Clicked:
        if (feeding == 0) {
          alert = 1;
          
          displayTwoLineMessage("CONTINUOUS", "FEED");
          
          servo.attach(A0);
          servo.write(2000);
          
          feeding = 1;
        } else {
          alert = 0;
                    
          clearLCD();
          
          servo.detach();
          
          feeding = 0;
        }
        break;
      }
  }

  // Read the state of the SQW pin and show it on the
  // pin 13 LED. (It should blink at 1Hz.)
  digitalWrite(SQW_OUTPUT_PIN, digitalRead(SQW_INPUT_PIN));
}

