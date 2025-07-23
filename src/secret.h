#pragma once
#include <Arduino.h>

struct WifiCredential {
  const char* ssid;
  const char* pass;
};

#define BOT_TOKEN "XXXXXXXXXXXXXXXXXXXXX" // Telegram BOT Token (Get from Botfather)
#define OWNER_CHAT_ID "XXXXXXXXX" // Owner chat id

#define WIFI_CREDENTIALS_INIT \
{\
    {"Wifi-1","password-1"},\
     ...
    {"Wifi-N","password-N"}\
}
const WifiCredential WIFI_CREDENTIALS[] PROGMEM = WIFI_CREDENTIALS_INIT;
#define WIFI_CREDENTIALS_COUNT (sizeof(WIFI_CREDENTIALS)/sizeof(WIFI_CREDENTIALS[0]))