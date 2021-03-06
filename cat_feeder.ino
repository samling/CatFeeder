#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <Servo.h>
#include <SerLCD.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include "FeedTimer.h"

#define FSR_PIN 0
//#define SERVO_PIN D1
#define LCD_PIN D2
#define MOMENTARY_PIN D3

// Stepper motor
#define stp D1
#define dir D4
#define MS1 D5
#define MS2 D6
#define EN  D7
char user_input;
int x;
int y;
int state;

// Button states for momentary switch
int buttonState;
int val;

// Interrupt for alerts
bool alert = 0;

// Feed timers
FeedTimer timer1;
FeedTimer timer2;

// FSR
int fsrReading;

// NTP
unsigned int udpPort = 2390;
IPAddress ntpIP;
const char* ntpServerName = "time.google.com";
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];
WiFiUDP udp;
time_t getNtpTime();
time_t prevDisplay = 0;
const int timeZone = -7;

// Internet
const unsigned long HTTP_TIMEOUT = 10000;
const char* ssid = "2Bros1Cat-MikroTik";
const char* password = "0liverWorldwide";
WiFiServer server(8080);

// LCD
serLCD lcd(LCD_PIN);

// ----------------------------//
//            Setup            //
//-----------------------------//

void setup()
{
  // Initialize serial connection
  Serial.begin(9600);
  delay(500);
  lcd.setBrightness(30);

  // Set sane feeding time defaults
  timer1.setTime(1, 7, 0);
  timer2.setTime(1, 16, 0);

  // Show defaults
  Serial.println("");
  Serial.println("Timers:");
  Serial.println("Timer1: " + (timer1.hour() < 10 ? "0" + String(timer1.hour()) : String(timer1.hour())) + ":" + (timer1.minute() < 10 ? "0" + String(timer1.minute()) : String(timer1.minute())) + ", Portion Size: " + String(timer1.portionSize())); 
  Serial.println("Timer2: " + (timer2.hour() < 10 ? "0" + String(timer2.hour()) : String(timer2.hour())) + ":" + (timer2.minute() < 10 ? "0" + String(timer2.minute()) : String(timer2.minute())) + ", Portion Size: " + String(timer2.portionSize())); 

  // Connect to wifi
  Serial.println("");
  Serial.print("Connecting to wifi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Wifi connected!");
  server.begin();
  Serial.println("");
  Serial.println("Server started");
  Serial.print(WiFi.localIP());

  // Start listening for UDP packets
  Serial.println("");
  Serial.println("Starting UDP");
  udp.begin(udpPort);
  Serial.print("Local UDP port: ");
  Serial.println(udp.localPort());

  // Sync the clock via NTP
  Serial.println("");
  Serial.println("Synchronizing clock");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
  Serial.println("Time set to: " + (hour() < 10 ? "0" + String(hour()) : String(hour())) + ":" + (minute() < 10 ? "0" + String(minute()) : String(minute())) + ":" + (second() < 10 ? "0" + String(second()) : String(second())));
  Serial.println("Date set to: " + (day() < 10 ? "0" + String(day()) : String(day())) + "/" + (month() < 10 ? "0" + String(month()) : String(month())) + "/" + (year() < 10 ? "0" + String(year()) : String(year())));

  // Pull the momentary switch high
  pinMode(MOMENTARY_PIN, INPUT);
  digitalWrite(MOMENTARY_PIN, HIGH);

  // Set output mode on stepper motor
  pinMode(stp, OUTPUT);
  pinMode(dir, OUTPUT);
  pinMode(MS1, OUTPUT);
  pinMode(MS2, OUTPUT);
  pinMode(EN, OUTPUT);
  resetEDPins(); //Set step, direction, microstep and enable pins to default states

  // Clear the LCD
  clearLCD();

  // Startup complete
  Serial.println("");
  Serial.println("Startup complete!");
}

// ----------------------------//
//          Main Loop          //
//-----------------------------//

void loop()
{
  // Read the momentary push button
  val = digitalRead(MOMENTARY_PIN);
  if (val != buttonState) {
    if (val == LOW) {
      feed("manualFeed", 2);
    }
  }
  buttonState = val;

  // Print the current time to the LCD
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { // Update the display only if time has changed
      prevDisplay = now();
      printTime();
    }
  }

  // Check if the current time matches a feed time
  checkFeedTimes();

  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }
  Serial.println("+---------------------+");
  Serial.println("|   Client connected  |");
  Serial.println("+---------------------+");

  // Wait until the client is available
  while(!client.available()) {
    delay(1);
  }

  // Read the request
  String data = "";
  String json = "";
  boolean httpBody = false;
  client.find("{");
  data = client.readStringUntil('}');
  json = "{" + data + "}";
  Serial.println(json);

  // Parse the JSON data
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);
  String action = root["action"];
  int portionSize = root["portionSize"];

  // Do something depending on the request type
  if (action == "manualFeed") {
    // Issue the manual feed command
    feed(action, portionSize);
  } else if (action == "setTime") {
    int timer = root["timer"];
    int h = root["h"];
    int m = root["m"];

    if (timer == 1) {
      timer1.setTime(portionSize, h, m);
    } else if (timer == 2) {
      timer2.setTime(portionSize, h, m);
    }
  }

  // Send response
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println();
  client.println("{\"result\":\"success\"}");

  // Clear the buffer
  client.flush();
  Serial.println("+---------------------+");
  Serial.println("| Client disconnected |");
  Serial.println("+---------------------+");
  Serial.println("");
}

// ----------------------------//
//       Helper Functions      //
//-----------------------------//

//Default microstep mode function
void StepForwardDefault(long milli)
{
  digitalWrite(dir, HIGH); //Pull direction pin low to move "forward"
  for(x= 1; x<milli; x++)  //Loop the forward stepping enough times for motion to be visible
  {
    digitalWrite(stp,HIGH); //Trigger one step forward
    delay(1);
    digitalWrite(stp,LOW); //Pull step pin low so it can be triggered again
    delay(1);
  }
}

//Reset Easy Driver pins to default states
void resetEDPins()
{
  digitalWrite(stp, LOW);
  digitalWrite(dir, HIGH);
  digitalWrite(MS1, LOW);
  digitalWrite(MS2, LOW);
  digitalWrite(EN, LOW);
}

// Get the time from an NTP time server
time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmitting NTP Request...");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.println("NTP Server: " + String(ntpServerName));
  Serial.print("NTP Server IP: ");
  Serial.println(ntpServerIP);
  Serial.print("");
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Received NTP Response");
      udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("ERROR: No NTP Response");
  return 0; // return 0 if unable to get the time
}

// Send an NTP packet
unsigned long sendNTPpacket(IPAddress& address) {
  Serial.println("Sending NTP packet...");
  // Set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize NTP request values
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum (type of clock)
  packetBuffer[2] = 6;          // Polling interval
  packetBuffer[3] = 0xEC;       // Peer clock precision
  // 8 bytes of zero for root delay and root dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  udp.beginPacket(address, 123);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

// Print the time to the LCD
void printTime()
{
  // digital clock display of the time
  String YY = String(year()).substring(2);
  String MM = (month() < 10 ? "0" + String(month()) : String(month()));
  String DD = (day() < 10 ? "0" + String(day()) : String(day()));
  String hh = (hour() < 10 ? "0" + String(hour()) : String(hour()));
  String mm = (minute() < 10 ? "0" + String(minute()) : String(minute()));
  String ss = (second() < 10 ? "0" + String(second()) : String(second()));
  displayTwoLineMessage(DD + "/" + MM + "/" + YY + " 1)" + (timer1.hour() < 10 ? "0" + String(timer1.hour()) : String(timer1.hour())) + ":" + (timer1.minute() < 10 ? "0" + String(timer1.minute()) : String(timer1.minute())), hh + ":" + mm + ":" + ss + " 2)" + (timer2.hour() < 10 ? "0" + String(timer2.hour()) : String(timer2.hour())) + ":" + (timer2.minute() < 10 ? "0" + String(timer2.minute()) : String(timer2.minute())));

  // Detect changes in the FSR
//  fsrReading = analogRead(FSR_PIN);
//  Serial.print("Pressure: ");
//  Serial.println(fsrReading);
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

// Check the time against our preset feed times and trigger a feed cycle if they match
void checkFeedTimes() {
  if (second() == 0 && (timer1.hour() == hour() && timer1.minute() == minute())) {
    feed("scheduledFeed", timer1.portionSize());
    delay(1000);
  } else if (second() == 0 && (timer2.hour() == hour() && timer2.minute() == minute())) {
    feed("scheduledFeed", timer2.portionSize());
    delay(1000);
  }
}

// Initiate a feed cycle using portionSize as the delay time
void feed(String action, long portionSize) {
  // Limit duration to 5s maximum
  if (portionSize > 5) {
    portionSize = 5;
  }

  // Tailor the display to the action
  String displayAction;
  if (action == "manualFeed") {
    displayAction = "MANUAL FEED";
  } else if (action = "scheduledFeed") {
    displayAction = "SCHEDULED FEED";
  }
  // Pause the clock to show the alert
  alert = 1;
  
  // Convert portionSize into a time in seconds
  char buff[3];
  sprintf(buff, "%d", portionSize);
  char withDot[3];
  withDot[0] = buff[0];
  withDot[1] = '.';
  withDot[2] = buff[1];
  withDot[3] = '\0';

  // Display a notification on the LCD
  //displayTwoLineMessage(displayAction, "T:" + String(withDot) + "s" + " @ " + (hour() < 10 ? "0" + String(hour()) : String(hour())) + ":" + (minute() < 10 ? "0" + String(minute()) : String(minute())));
  displayTwoLineMessage(displayAction, "T:" + String(portionSize) + "s" + " @ " + (hour() < 10 ? "0" + String(hour()) : String(hour())) + ":" + (minute() < 10 ? "0" + String(minute()) : String(minute())));

  // Spin the auger
  StepForwardDefault(portionSize*500);
  //servo.attach(SERVO_PIN);
  //servo.write(1000); // Going in the clockwise direction
  //delay(portionSize);
  //servo.detach();

  clearLCD();
  alert = 0;
}

