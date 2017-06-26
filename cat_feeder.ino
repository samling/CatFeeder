#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <Servo.h>
#include <SerLCD.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <Time.h>
#include <Wire.h>

#define PRINT_USA_DATE

#define LCD_PIN D2
#define MOMENTARY_PIN D3
#define SERVO_PIN D1

// Button states for momentary switch
int buttonState, val;

// Interrupt for alerts
bool alert = 0;

// Connect to wifi
const char* ssid = "ssid";
const char* password = "password";
WiFiServer server(8080);

// Create servo
Servo servo;

// Initialize LCD
serLCD lcd(LCD_PIN);

void setup()
{
  // Initialize serial connection
  Serial.begin(9600);
  delay(500);
  lcd.setBrightness(30);

  // Connect to wifi
  WiFi.begin("2Bros1Cat", "0liverWorldwide");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  server.begin();
  Serial.println("");
  Serial.println("Server started");
  Serial.print(WiFi.localIP());
  
  // Pull the momentary switch high
  pinMode(MOMENTARY_PIN, INPUT);
  digitalWrite(MOMENTARY_PIN, HIGH);
}

void loop()
{ 
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

  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  Serial.println("Client connected");

  String request = client.readStringUntil('\r');
  Serial.println(request);
  client.flush();

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("");
  client.println("<!DOCTYPE HTML>");
  client.println("<html>OK</html>");
  delay(1);

  Serial.println("Client disconnected");
  Serial.println("");
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
  sprintf(buff, "%.5Lu", portionSize);
  char withDot[3];
  withDot[0] = buff[1];
  withDot[1] = '.';
  withDot[2] = buff[2];
  withDot[3] = '\0';

  displayTwoLineMessage("MANUAL FEED", "T:" + String(portionSize) + "s");

  servo.attach(SERVO_PIN);
  servo.write(1000); // Going in the clockwise direction
  delay(portionSize);
  servo.detach();

  clearLCD();
  alert = 0;
}
//
//// Print the date and time to the serial monitor
//void printDateTime() {
//  DateTime now = rtc.now();
//  Serial.print(now.year(), DEC);
//  Serial.print('/');
//  Serial.print(now.month(), DEC);
//  Serial.print('/');
//  Serial.print(now.day(), DEC);
//  Serial.print(" (");
//  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
//  Serial.print(") ");
//  Serial.print(now.hour(), DEC);
//  Serial.print(':');
//  Serial.print(now.minute(), DEC);
//  Serial.print(':');
//  Serial.print(now.second(), DEC);
//  Serial.println();
//
//  Serial.print(" since midnight 1/1/1970 = ");
//  Serial.print(now.unixtime());
//  Serial.print("s = ");
//  Serial.print(now.unixtime() / 86400L);
//  Serial.println("d");;
//  delay(1000);
//}

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

