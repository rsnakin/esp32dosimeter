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
|------------------|---------------------------------|
| `/start`          | Begin session                   |
| `/help`           | List available commands         |
| `/status`         | Show current readings           |
| `/subscribe`      | Enable Telegram alerts          |
| `/unsubscribe`    | Disable Telegram alerts         |
| `/keyboard`       | Show command buttons            |
| `/hide_keyboard`  | Hide buttons                    |

---

## 📡 REST API

| Endpoint                        | Description                          |
|----------------------------------|--------------------------------------|
| `/api_data`                     | Current sensor data                  |
| `/api_config`                   | Read current config                  |
| `/api_saveconfig`              | Save config changes                  |
| `/api_log?date=YYYY-MM-DD`     | Access daily logs                    |
| `/api_telegram_list`           | List Telegram subscribers            |
| `/api_telegram_delete_user`    | Remove Telegram user by chat_id      |
| `/api_diskusage`               | Internal FS usage                    |
| `/api_sddiskusage`             | SD card usage                        |

---

## 💾 SD Card Filesystem

```
/logs/log_YYYY-MM-DD.txt     → Daily logs  
/telegram_data/*.dat         → Telegram user data  
/config.json                 → Stored configuration  
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
