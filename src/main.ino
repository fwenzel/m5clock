// src/main.cpp
#include <M5Unified.h>
#include <M5GFX.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <SPIFFS.h>
#include <cstdlib>
#include <esp_sleep.h>
#include "config.h"

// Update cadences
constexpr uint32_t kMinuteMs  = 60 * 1000;
constexpr uint32_t kHourMs    = 60 * kMinuteMs;
constexpr uint32_t kDayMs     = 24 * kHourMs;

// Layout
constexpr int W = 960, H = 540;
constexpr int CLOCK_H = 160;

// -------- globals --------
M5GFX display;             // M5GFX display for M5Paper
M5Canvas canvas(&display); // RAM canvas we draw into

String currentImagePath = "";


// Deep sleep until next full hour
void sleepUntilNextHour() {
  m5::rtc_datetime_t datetime = M5.Rtc.getDateTime();

  // Calculate seconds until next hour
  int secondsUntilNext = (60 - datetime.time.minutes) * 60 - datetime.time.seconds;

  Serial.printf("Sleeping for %d seconds until next hour (%02d:00)\n", secondsUntilNext, datetime.time.hours + 1);

  // Configure timer wake-up
  esp_sleep_enable_timer_wakeup(secondsUntilNext * 1000000ULL); // Convert to microseconds

  // Enter deep sleep
  esp_deep_sleep_start();
}

String getRandomImagePath(int hour) {
  // Format time as HHMM
  char timeStr[5];
  snprintf(timeStr, sizeof(timeStr), "%02d00", hour);

  // Search for files matching pattern quote_HHMM_*_credits.png
  String basePath = "/images/metadata/quote_" + String(timeStr) + "_";
  String suffix = "_credits.png";

  // Count available options for this time
  int optionCount = 0;
  for (int i = 0; i < 20; i++) { // Check up to 20 options
    String testPath = basePath + String(i) + suffix;
    if (SD.exists(testPath)) {
      optionCount++;
    } else {
      break; // No more options
    }
  }

  if (optionCount > 0) {
    // Randomly select one of the available options
    int selectedOption = random(optionCount);
    String selectedPath = basePath + String(selectedOption) + suffix;
    Serial.printf("Found %d options for time %s, selected option %d\n",
                  optionCount, timeStr, selectedOption);
    return selectedPath;
  }

  Serial.println("No images found for any time in the last 60 minutes");
  return "";
}

void draw() {
  // Get current time for image selection
  m5::rtc_datetime_t datetime = M5.Rtc.getDateTime();

  // Create full screen canvas
  canvas.createSprite(W, H);
  canvas.fillSprite(TFT_WHITE);

  // Load and display the selected image
  bool imageLoaded = false;
  if (currentImagePath.length() > 0 && SD.exists(currentImagePath)) {
    Serial.printf("Loading image: %s\n", currentImagePath.c_str());

    // Load PNG image from SD card using canvas
    // Try to load the image file
    File imageFile = SD.open(currentImagePath, FILE_READ);
    if (imageFile) {
      // Read the file into memory and draw it
      size_t fileSize = imageFile.size();
      uint8_t* buffer = (uint8_t*)malloc(fileSize);
      if (buffer) {
        imageFile.read(buffer, fileSize);
        imageFile.close();

        // Draw the PNG from memory
        if (canvas.drawPng(buffer, fileSize, 0, 0)) {
          Serial.println("Image loaded successfully");
          imageLoaded = true;
        } else {
          Serial.println("Failed to draw PNG from memory");
        }
        free(buffer);
      } else {
        Serial.println("Failed to allocate memory for image");
        imageFile.close();
      }
    } else {
      Serial.println("Failed to open image file");
    }
  } else {
    Serial.println("No image path or file not found, using white background");
  }

  // If no image was loaded, display time in big font in the middle
  if (!imageLoaded) {
    char hhmm[6];
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", datetime.time.hours, datetime.time.minutes);

    // Set up text for time display
    canvas.setTextColor(TFT_BLACK);
    canvas.setTextSize(8);  // Big font
    canvas.setTextDatum(middle_center);

    // Draw time in the middle of the screen
    canvas.drawString(hhmm, W/2, H/2);

    Serial.printf("Displaying time: %s (no image available)\n", hhmm);
  }

  // Draw date overlay in bottom-left corner
  char dateS[24];
  snprintf(dateS, sizeof(dateS), "%04d-%02d-%02d", datetime.date.year, datetime.date.month, datetime.date.date);

  // Set up text for overlay with good visibility
  canvas.setTextColor(TFT_BLACK); // Black text without background
  canvas.setTextSize(2);
  canvas.setTextDatum(bottom_left);

  // Draw date at bottom-left corner
  canvas.drawString(dateS, 20, H - 20);

  Serial.printf("Displayed image with date: %s\n", dateS);

  // Push canvas to display and update
  canvas.pushSprite(0, 0);
  display.display();
  canvas.deleteSprite();

  // Wait slightly to let the display update
  delay(2000);
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
  if (year < 2025 || month < 1 || month > 12 || day < 1 || day > 31 ||
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

  // Clear display
  display.begin();
  display.setRotation(1);
  display.clearDisplay(1);     // initial clear with update
  Serial.println("Display initialized");

  // Initialize SD card
  Serial.println("Initializing SD card...");
  if (!SD.begin(4)) {  // 4 is the CS pin for the SD card
    Serial.println("ERROR: SD card initialization failed!");
  } else {
    Serial.println("SD card initialized successfully");

    // Test if images directory exists
    if (SD.exists("/images/metadata")) {
      Serial.println("Images directory found");
    } else {
      Serial.println("WARNING: Images directory not found");
    }
  }

  // Check if we woke up from deep sleep
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Woke up from deep sleep");
  } else {
    // First boot - do initial setup
    Serial.println("First boot - initial setup");

    // Always sync time on boot, then wait 24 hours
    Serial.println("Syncing time on boot...");
    syncTimeOnce();
  }

  // Get current time and select new image path
  m5::rtc_datetime_t datetime = M5.Rtc.getDateTime();
  currentImagePath = getRandomImagePath(datetime.time.hours);

  // Draw the new image
  draw();

  // Sleep until next hour
  sleepUntilNextHour();
}

void loop() {
  // This function should never be reached in normal operation
  // The device will sleep after setup() and wake up every hour
  Serial.println("ERROR: Reached loop() - this should not happen");
  delay(1000);
}
