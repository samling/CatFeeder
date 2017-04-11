#include <ClickEncoder.h>
#include <DS1307RTC.h>
#include <SerLCD.h>
#include <SoftwareSerial.h>
#include <Servo.h>
#include <SparkFunDS1307RTC.h>
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

void timerIsr() {
  encoder->service();
}

// Create servo
Servo servo;
int pos = 0;

// Initialize LCD
serLCD lcd(5);

void displayAccelerationStatus() {
  lcd.clear();
  lcd.print(encoder->getAccelerationEnabled() ? "on" : "off");
}

void setup()
{
  // Initialize serial connection
  Serial.begin(9600);

  // Set up the clock
  pinMode(SQW_INPUT_PIN, INPUT_PULLUP);
  pinMode(SQW_OUTPUT_PIN, OUTPUT);
  digitalWrite(SQW_OUTPUT_PIN, digitalRead(SQW_INPUT_PIN));

  // Set up the encoder
  encoder = new ClickEncoder(dpInEncoderA,dpInEncoderB,dpInEncoderPress);

  // Initialize clock library
  rtc.begin();

  // Initialize servo
  //myservo.attach(9);

  // Initialize timer for processing encoder interrupts
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);

  // Initialize the last state of the encoder as -1
  encLast = -1;
}

void loop()
{  
  static int8_t lastSecond = -1;

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
  
  if (rtc.second() != lastSecond) {
    lcd.selectLine(1);
    lcd.clearLine(1);
    lcd.print(String(rtc.hour()) + ":");
    if(rtc.minute() < 10) { lcd.print(String("0")); }
    lcd.print(String(rtc.minute()) + ":");
    if(rtc.second() < 10) { lcd.print(String("0")); }
    lcd.print(String(rtc.second()));
    delay(1000);
  }

  lcd.selectLine(2);
  lcd.clearLine(2);
  lcd.print(String(rtc.month()) + "/" + String(rtc.day()) + "/" + String(rtc.year()));
  
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

  ClickEncoder::Button b = encoder->getButton();
  if (b != ClickEncoder::Open){
    lcd.clear();
    lcd.selectLine(1);
    lcd.print("Button pressed");

    servo.attach(9);
    for (pos = 0; pos <=180; pos += 1) {
        servo.write(pos);
        delay(5);
    }
    
    #define VERBOSECASE(label) case label: lcd.selectLine(1); lcd.print(#label); break;
    switch (b) {
      VERBOSECASE(ClickEncoder::Pressed);
      VERBOSECASE(ClickEncoder::Held);
      VERBOSECASE(ClickEncoder::Released);
      VERBOSECASE(ClickEncoder::Clicked);
      case ClickEncoder::DoubleClicked:
        Serial.println("ClickEncoder::DoubleClicked");
        encoder->setAccelerationEnabled(!encoder->getAccelerationEnabled());
        Serial.print("Acceleration is:");
        Serial.println(encoder->getAccelerationEnabled() ? "enabled" : "disabled");
        break;
    }
  }

  // Read the state of the SQW pin and show it on the
  // pin 13 LED. (It should blink at 1Hz.)
  digitalWrite(SQW_OUTPUT_PIN, digitalRead(SQW_INPUT_PIN));
}

