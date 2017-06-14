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

#define SERVO_MIDPOINT 1480

// Initialize encoder
ClickEncoder *encoder;
int16_t encLast, encValue;

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

void instantFeed(int portionSize) {
  Serial.println("Instant feed cycle");
  servo.write(portionSize);
  delay(2000);
  servo.write(SERVO_MIDPOINT);
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

  servo.attach(A0);
  // Initialize servo in a stopped state
  // (midpoint of 1000-2000 range); should
  // be 1500 but not quite calibrated
  servo.write(SERVO_MIDPOINT);
}

void loop()
{  
  static int8_t lastSecond = -1;

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
          instantFeed(portionSize);
          Serial.println(portionSize);

          // Send a response to the client
          client.println("HTTP/1.0 200 OK");
          client.println("Content-Type: application/json");
          client.println();
          client.println("{\"result\":\"success\"}");
          client.stop();
        }
        else if (c == '\n' && currentLineIsBlank && req_str.startsWith("POST")) {
          while (client.available()) {
            Serial.write(client.read());
          }
          client.println("HTTP/1.0 200 OK");
          client.println("Content-Type: text/html");
          client.println();
          client.println("<HTML><BODY>POST TEST OK!</BODY></HTML>");
          client.stop();
        } else if (c == '\n' && currentLineIsBlank && (!req_str.startsWith("GET") || !req_str.startsWith("POST"))) {
          while (client.available()) {
            Serial.write(client.read());
          }
          client.println("HTTP/1.0 404 NOT FOUND");
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

  // Read the time:
  int s = rtc.second();
  int m = rtc.minute();
  int h = rtc.hour();
  
  // Read the day/date:
  int dy = rtc.day();
  int da = rtc.date();
  int mo = rtc.month();
  int yr = rtc.year();
  
//  if (rtc.second() != lastSecond) {
//    lcd.selectLine(1);
//    lcd.clearLine(1);
//    lcd.print(String(rtc.hour()) + ":");
//    if(rtc.minute() < 10) { lcd.print(String("0")); }
//    lcd.print(String(rtc.minute()) + ":");
//    if(rtc.second() < 10) { lcd.print(String("0")); }
//    lcd.print(String(rtc.second()));
//    delay(1000);
//  }

  //lcd.selectLine(2);
  //lcd.clearLine(2);
  //lcd.print(String(rtc.month()) + "/" + String(rtc.day()) + "/" + String(rtc.year()));
  
  // Read encoder value
  encValue += encoder->getValue();
  if (encValue != encLast) {
    encLast = encValue;
    lcd.clear();
    lcd.selectLine(1);
    lcd.print("Encoder Value:");
    lcd.selectLine(2);
    lcd.print(encValue);
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
          servo.write(1000);
          feeding = 1;
        } else {
          servo.write(SERVO_MIDPOINT);
          feeding = 0;
        }
        break;
      }
  }

  // Read the state of the SQW pin and show it on the
  // pin 13 LED. (It should blink at 1Hz.)
  digitalWrite(SQW_OUTPUT_PIN, digitalRead(SQW_INPUT_PIN));
}

