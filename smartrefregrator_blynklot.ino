#define BLYNK_TEMPLATE_ID "enter your templete id"
#define BLYNK_TEMPLATE_NAME "smart refrigerator"
#define BLYNK_AUTH_TOKEN "enter your token"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WebServer.h>

// WiFi
char ssid[] = "enter your wifi name ";
char pass[] = "enter your wifi password";

// Pins
#define DHTPIN 4
#define DHTTYPE DHT22
#define RELAY_PIN 5
#define MQ135_PIN 34

Adafruit_SSD1306 display(128, 64, &Wire, -1);
DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;
WebServer server(80);

// Modes
bool manualMode = false;
int manualState = LOW;

// Storage Mode
int fridgeMode = 0;
float tempHigh = 30;
float tempLow = 25;

// Gas
int gasValue = 0;

// Data
float lastTemp = 0;
float lastHum = 0;

// Food spoil
bool foodSpoiled = false;
bool alertSent = false;

// ================= ITEM =================
String getItemName() {
  switch(fridgeMode) {
    case 0: return "Medicine";
    case 1: return "Fruits";
    case 2: return "Vegetables";
    case 3: return "Dairy";
    case 4: return "Meat";
    case 5: return "Others";
    default: return "Unknown";
  }
}

// ================= WEB =================
void handleRoot() {
  String webpage = "<html><head><meta http-equiv='refresh' content='3'></head><body>";
  webpage += "<h1>SMART REFRIGERATOR</h1>";

  webpage += "Temp: " + String(lastTemp) + "<br>";
  webpage += "Humidity: " + String(lastHum) + "<br>";
  webpage += "Gas: " + String(gasValue) + "<br>";
  webpage += "Food: " + String(foodSpoiled ? "SPOILED" : "FRESH") + "<br>";

  webpage += "Item: " + getItemName() + "<br>";
  webpage += "Mode: " + String(manualMode ? "MANUAL" : "AUTO") + "<br>";

  server.send(200, "text/html", webpage);
}

// ================= DISPLAY =================
void updateDisplay() {

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.println("SMART FRIDGE");
  display.println("----------------");

  display.print("Temp: "); display.println(lastTemp);
  display.print("Hum : "); display.println(lastHum);
  display.print("Gas : "); display.println(gasValue);

  display.print("Food: ");
  display.println(foodSpoiled ? "SPOILED!" : "FRESH");

  display.print("Item: ");
  display.println(getItemName());

  display.print("Mode: ");
  display.println(manualMode ? "MANUAL" : "AUTO");

  display.display();
}

// ================= SENSOR =================
void sendData() {

  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();

  if (!isnan(temp) && !isnan(hum)) {
    lastTemp = round(temp * 10) / 10.0;
    lastHum  = round(hum * 10) / 10.0;
  }

  int total = 0;
  for(int i = 0; i < 10; i++) {
    total += analogRead(MQ135_PIN);
    delay(5);
  }
  gasValue = total / 10;

  // ===== DEBUG =====
  Serial.print("Gas: ");
  Serial.print(gasValue);
  Serial.print("  Hum: ");
  Serial.println(lastHum);

  // ===== SMART SPOIL LOGIC =====
  if (gasValue > 1500) {
    foodSpoiled = true;
  }
  else if (gasValue > 900 || (gasValue > 700 && lastHum > 70)) {
    foodSpoiled = true;
  }
  else {
    foodSpoiled = false;
  }

  // ===== BLYNK =====
  Blynk.virtualWrite(V0, lastTemp);
  Blynk.virtualWrite(V1, lastHum);
  Blynk.virtualWrite(V7, gasValue);
  Blynk.virtualWrite(V8, foodSpoiled ? "SPOILED" : "FRESH");

  // ===== NOTIFICATION =====
  if (foodSpoiled && !alertSent) {
    Blynk.logEvent("food_alert", "⚠ Food Spoiled Detected!");
    alertSent = true;
  }
  if (!foodSpoiled) alertSent = false;

  // ===== RELAY =====
  if (manualMode) {
    digitalWrite(RELAY_PIN, manualState);
  }
  else {
    if (lastTemp > tempHigh)
      digitalWrite(RELAY_PIN, HIGH);
    else if (lastTemp < tempLow)
      digitalWrite(RELAY_PIN, LOW);
  }

  updateDisplay();
}

// ================= BLYNK =================
BLYNK_WRITE(V2) {
  manualMode = param.asInt();
  updateDisplay();
}

BLYNK_WRITE(V5) {
  manualState = param.asInt();
  updateDisplay();
}

BLYNK_WRITE(V3) {

  fridgeMode = param.asInt();

  switch (fridgeMode) {
    case 0: tempHigh = 10; tempLow = 7; break;
    case 1: tempHigh = 12; tempLow = 8; break;
    case 2: tempHigh = 14; tempLow = 10; break;
    case 3: tempHigh = 10; tempLow = 6; break;
    case 4: tempHigh = 8; tempLow = 4; break;
    case 5: tempHigh = 28; tempLow = 19; break;
  }

  updateDisplay();
}

// ================= SETUP =================
void setup() {

  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  pinMode(MQ135_PIN, INPUT);

  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true);
  }

  display.clearDisplay();
  display.setCursor(0, 10);
  display.println("Starting...");
  display.display();

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  server.on("/", handleRoot);
  server.begin();

  timer.setInterval(3000L, sendData);
}

// ================= LOOP =================
void loop() {
  Blynk.run();
  timer.run();
  server.handleClient();
}