/*
MIT License

Copyright (c) 2025 Richard Snakin

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

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
