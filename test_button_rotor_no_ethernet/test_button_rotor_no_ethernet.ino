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

// Display the date and time on the LCD
void showDateTime(int8_t lastSecond) {
  if (rtc.second() != lastSecond) {
    lcd.selectLine(1);
    lcd.clearLine(1);
    int adjustedHour = (rtc.hour() - 14 > 0 ? rtc.hour() - 14 : rtc.hour() + 10);
    if (adjustedHour < 10) {
      lcd.print(String("0"));
    }
    lcd.print(String(adjustedHour) + ":");
    if (rtc.minute() < 10) {
      lcd.print(String("0"));
    }
    lcd.print(String(rtc.minute()) + ":");
    if (rtc.second() < 10) {
      lcd.print(String("0"));
    }
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

  // Pull the momentary switch high
  pinMode(MOMENTARY_PIN, INPUT);
  digitalWrite(MOMENTARY_PIN, HIGH);

  // Pull the ethernet shield SD card slot high
  pinMode(SDCARD_CS, OUTPUT);
  digitalWrite(SDCARD_CS, HIGH);

  // Set up the clock
  pinMode(SQW_INPUT_PIN, INPUT_PULLUP);
  pinMode(SQW_OUTPUT_PIN, OUTPUT);
  digitalWrite(SQW_OUTPUT_PIN, digitalRead(SQW_INPUT_PIN));

  // Initialize clock library
  rtc.begin();
  rtc.writeSQW(SQW_SQUARE_1);
  rtc.autoTime();

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

  // Update RC data including seconds, minutes, etc.
  rtc.update();
  rtc.set24Hour();

  // Read the state of the SQW pin and show it on the
  // pin 13 LED. (It should blink at 1Hz.)
  digitalWrite(SQW_OUTPUT_PIN, digitalRead(SQW_INPUT_PIN));
}

