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
#include <AsyncTelegram2.h>
#include <TFT_eWidget.h>

#define VERSION                 "1.0"
#define BUILD                   "00220"
#define SERIAL_OUT              0
#define CPU_FREQUENCY_240       1 // or 0 set CpuFrequencyMhz(240)
#define RAD_SENS_SDA            32 // SDA I²C DATA  GPIO 32
#define RAD_SENS_SCL            33 // SCL I²C CLOCK GPIO 33
#define RAD_I2C_ADDR            0x66
#define VCC_PIN                 ADC1_CHANNEL_7 // ADC1 - CH7 - GPIO 35
#define DEFAULT_VREF            1100
#define RADSENS_SENSITIVITY     105.0f
#define HOSTNAME                "esp32dosimeter"
#define TIMEZONE_OFFSET         3 // Set your time zone offset. For example: Europe/Moscow (UTC+3) -> offset = 3
#define TIMEZONE_OFFSET_SEC     (TIMEZONE_OFFSET * 3600)
#define NTP_SERVER              "time.google.com" // Set the closest available NTP server for time synchronization (pool.ntp.org)
#define CACHE_MAX_AGE           "max-age=86400"
#define RSTATIC_UPDATE          1000
#define RDYNAMIC_UPDATE         1000
#define VOLTAGE_UPDATE          5000
#define VOLTAGE_CORRECTION      1.0
#define WIFI_UPDATE             5000
#define SHOWBAR_UPDATE          1000
#define BOT_MTBS                2000
#define TELEGRAM_DATA           "/telegram"
#define LOGS_PATH               "/logs"
#define CONFIG_FILE             "/options.json"
#define LOGS_UPDATE             10000
#define SECURED_CLIENT_TIMEOUT  5000
#define TELEGRAM_ALERT_UPDATE   1000
#define RAD_THRESHOLD           0.30f
#define ALERT_PAUSE             300000
#define SD_SCLK                 25
#define SD_MISO                 27
#define SD_MOSI                 26
#define SD_CS                   19
#define SD_ROOT_DIR             "/"
#define X_TASK_DELAY            250
#define WIFI_RESTART_INTERVAL   10000
#define SERVER_SEND(URL, FILEPATH_GZ, MIME) \
	server.on(URL, HTTP_GET, [](AsyncWebServerRequest *request){ \
		AsyncWebServerResponse *response = request->beginResponse(LittleFS, FILEPATH_GZ, MIME); \
		response->addHeader("Cache-Control", "public, max-age=2592000"); \
		response->addHeader("Content-Encoding", "gzip"); \
		request->send(response); \
	})

MeterWidget templateMeter(
	int x, int y, 
	const char *title, 
	int zoneRed0, int zoneRed1, int zoneOrg0, int zoneOrg1, int zoneYell0, int zoneYell1, int zoneGrn0, int zoneGrn1,
	float diapason,
	const char *units,
	const char *val0, const char *val1, const char *val2, const char *val3, const char *val4
);
String getDateTimeStr(time_t t);
String getFileName(String username);
bool logText(const String &msg);
void deleteOldLogs(int days);
void drawBox(int x, int y, int w, int h, uint16_t fillCol, uint16_t borderCol, uint8_t borderWidth);
void showMessage(const char *msg, uint16_t color);
void showBar(void *pvParameters);
void connectWiFi();
void checkWiFi();
void meterHeader(int x, int y, const char *meterName);
void wifiUpdate(void *pvParameters);
void voltageUpdate(void *pvParameters);
void rDynamicUpdate(void *pvParameters);
void rStaticUpdate(void *pvParameters);
void APIGetLogByDate(AsyncWebServerRequest *request);
void APIVersion(AsyncWebServerRequest *request);
void APIdiskUsage(AsyncWebServerRequest *request);
void APITelegramDeleteUser(AsyncWebServerRequest *request);
void APITelegramList(AsyncWebServerRequest *request);
void APIFileList(AsyncWebServerRequest *request);
void APIData(AsyncWebServerRequest *request);
void APISDdiskUsage(AsyncWebServerRequest *request);
void APISDFileList(AsyncWebServerRequest *request);
void APIConfigJson(AsyncWebServerRequest *request);
void APISaveConfig(AsyncWebServerRequest *request);
void uploadMenu();
void scanFiles(const char *path);
void checkAsyncWebServer();
void onMessage(TBMessage &msg);
bool loadConfig(const char* filename = CONFIG_FILE);
bool saveConfig(const char* filename = CONFIG_FILE);
uint32_t readVcc_mV();

struct FileInfo {
    String name;
    size_t size;
    bool isDir;
    time_t modified;
};

struct BotCommand {
  const char* command;
  const char* description;
};