// src/main.cpp
#include <M5Unified.h>
#include <M5GFX.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// Update cadences
constexpr uint32_t kMinuteMs  = 60 * 1000;
constexpr uint32_t kHourMs    = 60 * kMinuteMs;
constexpr int      kPartialsBeforeFull = 15;

// Layout
constexpr int W = 960, H = 540;
constexpr int CLOCK_H = 160;

// -------- globals --------
M5GFX display;             // M5GFX display for M5Paper
M5Canvas canvas(&display); // RAM canvas we draw into

uint32_t lastMinute  = 0;
int partials = 0;


void draw(bool full) {
  // Create only the region we intend to push to minimize RAM and time
  int ch = full ? H : CLOCK_H;
  canvas.createSprite(W, ch);
  canvas.fillSprite(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK);
  canvas.setTextDatum(middle_center);

  // Time from RTC
  m5::rtc_datetime_t datetime = M5.Rtc.getDateTime();

  char hhmm[6];  snprintf(hhmm, sizeof(hhmm), "%02d:%02d", datetime.time.hours, datetime.time.minutes);
  char dateS[24];snprintf(dateS, sizeof(dateS), "%04d-%02d-%02d", datetime.date.year, datetime.date.month, datetime.date.date);
  
  Serial.printf("Displaying time: %s %s\n", hhmm, dateS);

  // Big clock
  canvas.setTextSize(5);
  canvas.drawString(hhmm, W/2, 40);
  canvas.setTextSize(2);
  canvas.drawString(dateS, W/2, 120);


  // Push to EPD: full refresh or partial for the top band
  if (full) {
    canvas.pushSprite(0, 0);
    display.display();                // full-screen update
    partials = 0;
  } else {
    // Only update the clock band to reduce ghosting and flicker
    canvas.pushSprite(0, 0);
    display.display(0, 0, W, CLOCK_H); // partial region update
    partials++;
  }
  canvas.deleteSprite();
}

bool syncTimeViaHTTP() {
  Serial.println("Syncing time via HTTP...");
  
  WiFiClient client;
  HTTPClient http;
  
  if (!http.begin(client, "http://worldclockapi.com/api/json/pst/now")) {
    Serial.println("ERROR: Failed to begin HTTP client");
    return false;
  }
  
  http.setTimeout(10000);
  int code = http.GET();
  
  if (code != 200) {
    Serial.printf("ERROR: HTTP request failed with code %d\n", code);
    http.end();
    return false;
  }
  
  JsonDocument doc;
  auto err = deserializeJson(doc, http.getStream());
  http.end();
  
  if (err) {
    Serial.printf("ERROR: JSON parsing failed: %s\n", err.c_str());
    return false;
  }
  
  String datetimeStr = doc["currentDateTime"].as<String>();
  
  if (datetimeStr.length() == 0) {
    Serial.println("ERROR: No datetime field in response");
    return false;
  }
  
  // Parse the datetime string (format: "2024-12-18T14:30:45.123456-08:00")
  int year = datetimeStr.substring(0, 4).toInt();
  int month = datetimeStr.substring(5, 7).toInt();
  int day = datetimeStr.substring(8, 10).toInt();
  int hour = datetimeStr.substring(11, 13).toInt();
  int minute = datetimeStr.substring(14, 16).toInt();
  int second = datetimeStr.substring(17, 19).toInt();
  
  // Validate the parsed values
  if (year < 2020 || year > 2030 || month < 1 || month > 12 || day < 1 || day > 31 ||
      hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
    Serial.println("ERROR: Invalid parsed time values");
    return false;
  }
  
  // Set the RTC
  m5::rtc_datetime_t datetime;
  datetime.time.hours = hour;
  datetime.time.minutes = minute;
  datetime.time.seconds = second;
  datetime.date.year = year;
  datetime.date.month = month;
  datetime.date.date = day;
  datetime.date.weekDay = 0;
  
  M5.Rtc.setDateTime(&datetime);
  
  Serial.printf("Time synced: %04d-%02d-%02d %02d:%02d:%02d\n", 
                year, month, day, hour, minute, second);
  return true;
}


void syncTimeOnce() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) delay(100);
  
  if (WiFi.status() == WL_CONNECTED) {
    syncTimeViaHTTP();
  } else {
    Serial.println("ERROR: WiFi connection failed");
  }
  WiFi.disconnect(true);
}

void setup() {
  Serial.begin(115200);
  Serial.println("M5Paper Clock Starting...");
  
  auto cfg = M5.config();
  cfg.output_power = true;     // enable EPD power rails
  M5.begin(cfg);

  display.begin();
  display.setRotation(1);
  display.clearDisplay(1);     // initial clear with update

  Serial.println("Display initialized");
  
  Serial.println("Syncing time...");
  syncTimeOnce();
  

  draw(true);                  // initial full draw
  lastMinute  = millis();
  
  Serial.println("Setup complete");
}

void loop() {
  uint32_t now = millis();

  if (now - lastMinute >= kMinuteMs) {
    bool doFull = (partials >= kPartialsBeforeFull);
    draw(doFull);
    lastMinute = now;
  }


  // Light idle to keep CPU cool. For battery, switch to deep sleep with RTC alarm.
  delay(50);
}
