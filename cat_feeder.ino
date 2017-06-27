#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <Servo.h>
#include <SerLCD.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <Wire.h>

#define PRINT_USA_DATE

#define LCD_PIN D2
#define MOMENTARY_PIN D3
#define SERVO_PIN D1
#define FSR_PIN 0

// Button states for momentary switch
int buttonState, val;

// Interrupt for alerts
bool alert = 0;

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
const char* ssid = "ssid";
const char* password = "password";
WiFiServer server(8080);

// Create servo
Servo servo;

// Initialize LCD
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

  // Connect to wifi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Wifi connected");
  server.begin();
  Serial.println("");
  Serial.println("Server started");
  Serial.print(WiFi.localIP());

  // Start listening for UDP packets
  Serial.println("Starting UDP");
  udp.begin(udpPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());

  // Sync the clock via NTP
  Serial.println("");
  Serial.println("Synchronizing clock");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);

  // Pull the momentary switch high
  pinMode(MOMENTARY_PIN, INPUT);
  digitalWrite(MOMENTARY_PIN, HIGH);

  // Clear the LCD
  clearLCD();
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
      manualFeed(2000);
    }
  }
  buttonState = val;

  // Print the current time to the serial monitor
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      printTime();
    }
  }

  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }
  Serial.println("+---------------------+");
  Serial.println("|  Client connected   |");
  Serial.println("+---------------------+");

  // Wait until the client is available
  while(!client.available()) {
    delay(1);
  }

  // Read the request
  //String request = client.readString();
  String json = "";
  boolean httpBody = false;
  client.find("{");
  json = client.readStringUntil('}');
  Serial.println("{" + json + "}");

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

// Get the time from an NTP time server
time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
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
  Serial.println("No NTP Response");
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
  String YY = String(year());
  String MM = (month() < 10 ? "0" + String(month()) : String(month()));
  String DD = (day() < 10 ? "0" + String(day()) : String(day()));
  String hh = String(hour());
  String mm = (minute() < 10 ? "0" + String(minute()) : String(minute()));
  String ss = (second() < 10 ? "0" + String(second()) : String(second()));
  displayTwoLineMessage(DD + "/" + MM + "/" + YY, hh + ":" + mm + ":" + ss);

  // Detect changes in the FSR
  fsrReading = analogRead(FSR_PIN);
  //Serial.print("Pressure: ");
  //Serial.println(fsrReading);
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

