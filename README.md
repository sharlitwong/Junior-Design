# ESP32-S3-DevKitC-1 / WROOM-2 PlatformIO Setup & RGB Sanity Check

**Description:**
This repository provides the PlatformIO configuration and example code for flashing an **RGB LED** on the **ESP32-S3-DevKitC-1-N32R16V / WROOM-2-N32R16V** board, including correct flash, PSRAM, and OPI settings.

Website documentation from espressif: [here](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html)

---

## Features

* Configured for **32 MB flash / 16 MB PSRAM (OPI memory)**
* Correct `platformio.ini` for this board
* Example RGB blinking code
* Sanity check integration for board health

---

## Requirements

* [PlatformIO](https://platformio.org/) installed
* USB 2.0 cable (data + power)
* ESP32-S3-DevKitC-1 / WROOM-2 module

---

## Setup Instructions

1. Clone this repo:

   ```bash
   git clone https://github.com/opsec-bot/ESP32-S3-DevKitC-1-N32R16V.git
   cd ESP32-S3-DevKitC-1-N32R16V
   ```

2. **Copy the custom board definition:**

   ```text
   Copy /boards/esp32s3_n32r16v.json
   to
   C:\Users\%USER%\.platformio\platforms\espressif32\boards\
   ```

   This ensures PlatformIO recognizes the WROOM-2 module with correct flash and PSRAM configuration.

3. Connect your board via USB.

4. Install dependencies:
   PlatformIO will auto-install **Adafruit NeoPixel**.

5. Flash the board:

   ```bash
   pio run -t upload
   ```

6. Monitor the output:

   ```bash
   pio device monitor
   ```

   The **sanity check** should pass, and RGB LEDs will blink correctly.
