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

3. **Build and upload:**
   ```bash
   pio run -t upload
   ```

## Features

- Displays current time and date
- Syncs time via HTTP from worldclockapi.com
- Updates display every minute
- Periodic full screen refresh to prevent ghosting
- Low power consumption with e-paper display

## Security Note

The `src/config.h` file contains your WiFi credentials and is excluded from version control. Never commit this file to a repository.
