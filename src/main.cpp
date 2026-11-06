// ECSE3038 - Week 8, Lecture 1, starter.
// Done for you: Wi-Fi (Wokwi-GUEST), the LCD, showLines(), and a loop that
// calls pollWall() every 3 seconds.
// Your job tonight: fill in pollWall() so the board asks the class wall for
// YOUR v2 message, with your secret key in the X-API-Key header.
// Before you build: copy include/secrets.h.example to include/secrets.h
// and put in your v2 name and key.
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#if __has_include("secrets.h")
#include "secrets.h"   // WALL_NAME and WALL_KEY (and a real board's Wi-Fi)
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define WIFI_CHANNEL 6
#endif
#ifndef WIFI_CHANNEL
#define WIFI_CHANNEL 0
#endif
#ifndef WALL_NAME
#define WALL_NAME ""
#endif
#ifndef WALL_KEY
#define WALL_KEY ""
#endif

// The class wall. http, not https, and no / at the end.
const char *WALL_URL = "http://135.232.203.144/wall";

const unsigned long WIFI_TIMEOUT_MS = 15000;
const unsigned long POLL_EVERY_MS = 3000;   // ask every 3 s
const uint16_t HTTP_TIMEOUT_MS = 5000;      // give up on one request after 5 s

LiquidCrystal_I2C lcd(0x27, 16, 2);         // I2C address 0x27, 16 columns, 2 rows
unsigned long lastPoll = 0;
bool stopped = false;                       // true after a 401 or 403
String shown1, shown2;                      // what the LCD shows now

// Print two lines, each padded with spaces to 16 so old letters are wiped.
void showLines(const String &line1, const String &line2) {
  if (line1 == shown1 && line2 == shown2) {
    return;                                 // nothing new: no flicker
  }
  shown1 = line1;
  shown2 = line2;
  char row[17];
  snprintf(row, sizeof(row), "%-16.16s", line1.c_str());
  lcd.setCursor(0, 0);
  lcd.print(row);
  snprintf(row, sizeof(row), "%-16.16s", line2.c_str());
  lcd.setCursor(0, 1);
  lcd.print(row);
}

bool connectWiFi() {
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);
  showLines("Connecting to", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_TIMEOUT_MS) {
      Serial.println();
      Serial.println("No Wi-Fi after 15 s. Trying again on the next poll.");
      showLines("No Wi-Fi", "Trying again");
      return false;
    }
    delay(250);
    Serial.print(".");
  }

  Serial.println(" connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  showLines("Wi-Fi OK", WiFi.localIP().toString());
  return true;
}

// A wrong key does not get better by asking again. Say why, then stop asking.
void stopPolling(const char *line1, const char *line2, const String &detail) {
  stopped = true;
  showLines(line1, line2);
  Serial.print("Stopped: ");
  Serial.print(line1);
  Serial.print(" ");
  Serial.println(line2);
  Serial.print("Server said: ");
  Serial.println(detail);
  Serial.println("Fix include/secrets.h, build again, restart the simulator.");
}

void pollWall() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi lost. Reconnecting.");
    connectWiFi();
    return;
  }

  // TODO 1: create an HTTPClient and begin it on
  //         WALL_URL + "/v2/messages/" + WALL_NAME
  //         (Tutorial 8 did the same with your /settings path)
  HTTPClient client;
  String endpoint = String(WALL_URL) + "/v2/messages/" + WALL_NAME;
  client.begin(endpoint);
  // TODO 2: add the header X-API-Key with WALL_KEY as its value
  client.addHeader("X-API-Key", WALL_KEY);
  // TODO 3: send the GET, keep the status, read the body, then http.end()
  //         Print the status to Serial.
  int status_code = client.GET();
  String body = client.getString();
  client.end();

  // TODO 4: if the status is HTTP 200, read line_1 and line_2 with
  //         ArduinoJson and call showLines(line1, line2)
  if (status_code ==200){
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);

    if(error){
      Serial.print("deserializeJscon() failed");
      Serial.println(error.c_str());
      return;
    }
    const char* line_1 = doc["line_1"];
    const char* line_2 = doc["line_2"];

    showLines(line_1, line_2);

  }
  // TODO 5 (19:45): 404 -> showLines("No message yet", WALL_NAME)
  //                 401 or 403 -> stopPolling(...) with the reason
  //                 below 0 (no answer) -> print it and let the next poll retry

  if(status_code ==404){
    showLines("No message yet", WALL_NAME);
  } else if (status_code==401){
    stopPolling("got 401", "check wall_key", body);
  } else if (status_code ==403) {
    stopPolling("got 403", "check wall_name", body);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Hello from the ESP32");
  lcd.init();
  lcd.backlight();

  if (strlen(WALL_NAME) == 0 || strlen(WALL_KEY) == 0) {
    stopPolling("No secrets.h", "Copy the example", "WALL_NAME or WALL_KEY is missing");
    return;
  }
  connectWiFi();
}

void loop() {
  if (stopped || millis() - lastPoll < POLL_EVERY_MS) {
    return;
  }
  lastPoll = millis();
  pollWall();
}
