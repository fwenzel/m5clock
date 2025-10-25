# M5Clock

A simple digital clock for M5Paper using M5Unified and M5GFX libraries.

## Setup

1. **Configure WiFi credentials:**
   ```bash
   cp src/config.h.example src/config.h
   ```
   
2. **Edit `src/config.h` with your WiFi credentials:**
   ```cpp
   const char* WIFI_SSID = "YourWiFiNetworkName";
   const char* WIFI_PASS = "YourWiFiPassword";
   ```

3. **Prepare SD card with images:**
   - Insert an SD card into the M5Paper
   - Copy all PNG files from `img_gen/images/metadata/` to the SD card
   - Maintain the directory structure: `/images/metadata/quote_*.png`
   - Images should be named like `quote_HHMM_N_credits.png` where:
     - `HHMM` is 24-hour time format (e.g., "0508" for 5:08 AM)
     - `N` is zero-indexed option number (0, 1, 2, etc.)

4. **Build and upload:**
   ```bash
   pio run -t upload
   ```

## Features

- Displays full-screen PNG images based on current time
- Randomly selects from available images for each time period
- Falls back to nearest previous time if no images exist for current time
- Shows large time display when no images are available (white background)
- Shows date overlay in bottom-left corner
- Syncs time via HTTP from worldclockapi.com
- Updates display every minute
- Low power consumption with e-paper display

## Security Note

The `src/config.h` file contains your WiFi credentials and is excluded from version control. Never commit this file to a repository.
