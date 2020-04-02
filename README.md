# MiTemp-BLE-gw

ESP32 code for reading data from Xiaomi MiTemp LYWSDCGQ and LYWSD03MMC sensors. It is supossed to run on ESP32 using [arduino-esp32 SDK](https://github.com/espressif/arduino-esp32). It was tested with version [1.0.4](https://github.com/espressif/arduino-esp32/releases/tag/1.0.4) and attached patch to support multiple service data (needed for LYWSDCGQ sensor).

## What sensors are supported
- LYWSDCGQ - round one with LCD display powered by one AAA battery
- LYWSD03MMC - small square one with LCD display with great price / performance ratio

## How code works
Code consists of base BleAdvListener class that handle all needed for listening and extracting service data from BLE devices. On the top of that are classes for each sensor. Data from LYWSDCGQ sensor are extracted directly from ADV packets. Data from LYWSD03MMC sensor are received by doing BLE connection and requesting notification from sensor. All is prepared for very simple usage. Example code that reads data from both types of sensors at the same time and exporting it using simple HTTP api is located in [mitemp_ble_gw_esp32.cpp](/mitemp_ble_gw_esp32.cpp) file. After changing file extension it should be possible to compile it also in Arduino Studio (original code was developed in Sloeber IDE).

## Note to arduino-esp32 1.0.4 SDK
This version doesn't support multiple service data in included BLE library. You need to patch it using included [multiple_services.patch](/multiple_services.patch) file.
