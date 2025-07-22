# ESP32 Dosimeter

**ESP32 Dosimeter** is a smart Geiger counter based on ESP32 with a CG_RadSens sensor and a 3.5" TFT display. It offers a web interface, sends Telegram alerts, and displays real-time measurements (radiation, voltage, Wi-Fi signal). Also includes OTA updates and SD logging.

---

## 📡 Features

- ☢️ Real-time radiation monitoring
- 📶 Wi-Fi auto-connect to known networks
- 🌐 Embedded web server with REST API
- 🤖 Telegram bot with alerts and remote control
- 📟 GUI on 480×320 SPI TFT (ILI9488)
- 💾 Logging and configuration on microSD
- 🔧 OTA firmware updates
- 🔍 File manager via web interface

---

## 🔌 Hardware Connections

### CG_RadSens (I²C)

| Signal | ESP32 GPIO |
|--------|------------|
| SDA    | GPIO 32    |
| SCL    | GPIO 33    |

### ILI9488 TFT Display (SPI)

| Signal | ESP32 GPIO |
|--------|------------|
| MOSI   | GPIO 23    |
| SCLK   | GPIO 18    |
| CS     | GPIO 5     |
| DC     | GPIO 2     |
| RST    | GPIO 4     |

### microSD Card (HSPI)

| Signal | ESP32 GPIO |
|--------|------------|
| MOSI   | GPIO 26    |
| MISO   | GPIO 27    |
| SCLK   | GPIO 25    |
| CS     | GPIO 19    |

---

## ⚙️ Requirements

- ESP32 board
- Arduino IDE or PlatformIO
- Libraries:
  - `TFT_eSPI`
  - `ArduinoJson`
  - `AsyncTelegram2`
  - `ESPAsyncWebServer`
  - `CG_RadSens`
  - `LittleFS`, `SD`, `Wire`

---

## 🔐 Configuration (`secret.h`)

```cpp
#define BOT_TOKEN "<your_telegram_bot_token>"
#define OWNER_CHAT_ID "<your_chat_id>"

#define WIFI_CREDENTIALS_INIT \
{\
    {"Wifi-1","password-1"},\
     ...
    {"Wifi-N","password-N"}\
}

```
# 🚀 Usage Guide

## Flash and Run

1. Connect all hardware modules as described in the wiring section
2. Create and configure `secret.h` with your Wi-Fi, Telegram bot, and thresholds
3. Compile and flash the firmware to the ESP32 using Arduino IDE or PlatformIO
4. Open Serial Monitor for logs during boot
5. After boot:
   - Connects to Wi-Fi (auto-detect)
   - Starts web server
   - Syncs time via NTP
   - Starts Telegram bot
   - Loads config and last state
6. Access the web interface via device IP
7. Control via Telegram or HTTP API

---

## 📲 Telegram Commands

| Command           | Description                     |
|-------------------|---------------------------------|
| `/start`          | Begin session                   |
| `/help`           | List available commands         |
| `/status`         | Show current readings           |
| `/subscribe`      | Enable Telegram alerts          |
| `/unsubscribe`    | Disable Telegram alerts         |
| `/keyboard`       | Show command buttons            |
| `/hide_keyboard`  | Hide buttons                    |

---

## 📡 REST API

| Endpoint                       | Description                          |
|--------------------------------|--------------------------------------|
| `/api_data`                    | Current sensor data                  |
| `/api_config`                  | Read current config                  |
| `/api_saveconfig`              | Save config changes                  |
| `/api_log?date=YYYY-MM-DD`     | Access daily logs                    |
| `/api_telegram_list`           | List Telegram subscribers            |
| `/api_telegram_delete_user`    | Remove Telegram user by chat_id      |
| `/api_diskusage`               | Internal FS usage                    |
| `/api_sddiskusage`             | SD card usage                        |

---

## 💾 SD Card Filesystem

```
/logs/log_YYYY-MM-DD.txt → Daily logs
/telegram_data/*.dat → Telegram user data
/config.json → Stored configuration
```

---

## 📟 On-screen Display

The TFT screen shows:

- ☢️ Radiation level (µSv/h, static & dynamic)
- ⚡ Battery voltage
- 📶 Wi-Fi signal strength
- 🌐 Local IP address
- ⏱️ Current time (via NTP)
- 📤 Telegram status (sent/failed)

---

## 🛠 Troubleshooting

- **Can't connect to Wi-Fi**: Check `WIFI_CREDENTIALS` in `secret.h`
- **No Telegram messages**: Verify `BOT_TOKEN`, `OWNER_CHAT_ID`, and network access
- **Empty screen**: Check display wiring (CS, DC, RST), and TFT_eSPI config
- **No SD logs**: Ensure SD card is FAT32 and properly wired

---

## 📘 License

MIT License or custom license — specify as needed.

