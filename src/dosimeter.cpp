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

#include <Arduino.h>
#include <ArduinoJson.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <TFT_eSPI.h>
#include <TFT_eWidget.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <SPI.h>
#include <SD.h>
#include <vector>
#include <algorithm>
#include <Wire.h>
#include <CG_RadSens.h>
#include <time.h>
#include <AsyncTelegram2.h>
#include "esp32/clk.h"
#include "secret.h"
#include "dosimeter.h"
#include "fonts/NotoSansBold15.h"

/*
------------ Rad Sens ------------
	VCC             3V3
	GND      GND    GND
	SDA I²C DATA    GPIO 32
	SCL I²C CLOCK   GPIO 33
------------ ILI9488 -------------
	VCC	  3V3
	GND	  GND
	MOSI  GPIO 23
	SCLK  GPIO 18
	CS    GPIO 5
	DC    GPIO 2
	RST   GPIO 4
	LED   3V3
------------ MICRO SD ------------
	SCLK  GPIO 25
	MISO  GPIO 27
	MOSI  GPIO 26
	CS    GPIO 19
------------ VCC_PIN -------------
		+---------- 3V3
		|
	[R1]  100 kΩ
		|
		+---------> ADC1 - CH7 - GPIO 35
		|
	[R2]  100 kΩ
		|
		GND
*/

esp_adc_cal_characteristics_t adc_chars;
std::vector<FileInfo> fileList;
CG_RadSens radSens(RAD_I2C_ADDR);
WiFiClientSecure securedClient;
AsyncTelegram2 bot(securedClient);
AsyncWebServer server(80);
SPIClass spiSD(HSPI);
TFT_eSPI tft = TFT_eSPI();
fs::FS* microSDfs = nullptr;
char SSID[128];

IPAddress   myIP;
MeterWidget volts = MeterWidget(&tft);
MeterWidget rssi = MeterWidget(&tft);
MeterWidget radiationStatic = MeterWidget(&tft);
MeterWidget radiationDynamic = MeterWidget(&tft);

bool wifiUpdaterRun     = false;
bool voltageUpdaterRun  = false;
bool rStaticUpdaterRun  = false;
bool rDynamicUpdaterRun = false;
bool showBarRun         = false;

float rStatic  = 0.0;
float rDynamic = 0.0;
float rssi_val = 0.0;
float voltage  = 0.0;

TaskHandle_t  wifiUpdateHandle;
TaskHandle_t  voltageUpdateHandle;
TaskHandle_t  rDynamicUpdateHandle;
TaskHandle_t  rStaticUpdateHandle;
TaskHandle_t  showBarHandle;

String ownerChatId      = OWNER_CHAT_ID;
String NTPserver        = NTP_SERVER;
float RadAlertThold     = RAD_THRESHOLD;
float voltageCorrection = VOLTAGE_CORRECTION;

ReplyKeyboard myReplyKbd;

uint64_t totalBytes;

/*######################################################################################*/

void APISaveConfig(AsyncWebServerRequest *request) {
    if (request->hasParam("ownerChatId")) {
      ownerChatId = request->getParam("ownerChatId")->value();
    } else {
		logText("Error: failed option 'ownerChatId'");
		request->send(200, "application/json", "{\"status\":\"ERROR\"}");
		return;
	}
    if (request->hasParam("NTPserver") && NTPserver != request->getParam("NTPserver")->value()) {
    	String prevNTPserver = NTPserver;
		NTPserver = request->getParam("NTPserver")->value();
		configTime(0, 0, nullptr);
		configTime(TIMEZONE_OFFSET_SEC, 0, NTPserver.c_str());
		struct tm timeinfo;
		if (getLocalTime(&timeinfo)) {
			char msg[128]; char buf[64];
			strftime(buf, sizeof(buf), "%F %T", &timeinfo);
  			snprintf(msg, sizeof(msg), "Setting new ntp-server: %s, time: %s", NTPserver.c_str(), buf);
			logText(msg);
		} else {
			NTPserver = prevNTPserver;
			logText("Error: failed NTPserver!");
			configTime(TIMEZONE_OFFSET_SEC, 0, NTPserver.c_str());
			request->send(200, "application/json", "{\"status\":\"ERROR\"}");
			return;
		}
    } else if (!request->hasParam("NTPserver")) {
		logText("Error: failed option 'NTPserver'");
		request->send(200, "application/json", "{\"status\":\"ERROR\"}");
		return;
	}
    if (request->hasParam("RadAlertThold")) {
      RadAlertThold = request->getParam("RadAlertThold")->value().toFloat();
    } else {
		logText("Error: failed option 'RadAlertThold'");
		request->send(200, "application/json", "{\"status\":\"ERROR\"}");
		return;
	}
    if (request->hasParam("voltageCorrection")) {
      voltageCorrection = request->getParam("voltageCorrection")->value().toFloat();
    } else {
		logText("Error: failed option 'voltageCorrection'");
		request->send(200, "application/json", "{\"status\":\"ERROR\"}");
		return;
	}
	if(saveConfig()) {
    	request->send(200, "application/json", "{\"status\":\"OK\"}");
	} else {
		request->send(200, "application/json", "{\"status\":\"ERROR\"}");
	}
}

/*######################################################################################*/

void APIConfigJson(AsyncWebServerRequest *request) {
	StaticJsonDocument<512> doc;
	doc["ownerChatId"] = ownerChatId;
	doc["NTPserver"] = NTPserver;
	doc["RadAlertThold"] = RadAlertThold;
	doc["voltageCorrection"] = voltageCorrection;
	String json;
	serializeJsonPretty(doc, json);
	request->send(200, "application/json", json);
}

/*######################################################################################*/

bool saveConfig(const char* filename) {
	StaticJsonDocument<512> doc;

	doc["ownerChatId"] = ownerChatId;
	doc["NTPserver"] = NTPserver;
	doc["RadAlertThold"] = RadAlertThold;
	doc["voltageCorrection"] = voltageCorrection;

	microSDfs->remove(filename);

	File file = microSDfs->open(filename, FILE_WRITE);
	if (!file) {
		logText("[microSD] Error: failed to open config file for writing");
		return false;
	}

	if (serializeJsonPretty(doc, file) == 0) {
		logText("[microSD] Error: failed to write JSON");
		file.close();
		return false;
	}

	file.close();
	logText("[microSD] Config saved to SD");
	return true;
}

/*######################################################################################*/

bool loadConfig(const char* filename) {
	File file = microSDfs->open(filename, FILE_READ);
	if (!file) {
		logText("[microSD] Error: Failed to open config file on SD");
		return false;
	}

	size_t size = file.size();
	if (size == 0 || size > 1024) {
		logText("[microSD] Error: Config file size is invalid");
		file.close();
		return false;
	}

	StaticJsonDocument<512> doc;
	DeserializationError error = deserializeJson(doc, file);
	file.close();

	if (error) {
		logText("[microSD] Error: JSON parse: " + String(error.c_str()));
		return false;
	}

	if (doc.containsKey("ownerChatId")) ownerChatId = doc["ownerChatId"].as<String>();
	if (doc.containsKey("NTPserver")) NTPserver = doc["NTPserver"].as<String>();
	if (doc.containsKey("RadAlertThold")) RadAlertThold = doc["RadAlertThold"].as<float>();
	if (doc.containsKey("voltageCorrection")) voltageCorrection = doc["voltageCorrection"].as<float>();

	logText("[microSD] Config loaded from SD");
	return true;
}

/*######################################################################################*/

String getDateTimeStr(time_t t) {
	struct tm *tm_info = localtime(&t);
	char buffer[20];
	strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
	return String(buffer);
}

/*######################################################################################*/

bool logText(const String &msg) {
	time_t now = time(nullptr);
	String datetime = getDateTimeStr(now);
	String filename = LOGS_PATH "/log_" + datetime.substring(0, 10) + ".txt";

	fs::File fLog = microSDfs->open(filename, FILE_APPEND);
	if(fLog) {
		fLog.println(datetime + ": " + msg);
		fLog.close();
		fileList.clear();
		scanFiles(SD_ROOT_DIR);
		return true;
	}
	return false;
}

/*######################################################################################*/

void deleteOldLogs(int days) {
	fs::File root = microSDfs->open(LOGS_PATH);
	time_t now = time(nullptr);

	while (fs::File file = root.openNextFile()) {
		if (!file.isDirectory()) {
			String name = file.name();
			if (name.startsWith("log_") && name.endsWith(".txt")) {
				struct tm tm;
				memset(&tm, 0, sizeof(tm));               // log_0000-00-00
				String datePart = name.substring(4, 14);  // YYYY-MM-DD
				if (strptime(datePart.c_str(), "%Y-%m-%d", &tm)) {
					time_t fileTime = mktime(&tm);
					if (now - fileTime > days * 86400) {
						String name4delete = String(LOGS_PATH) + "/" + name;
						file.close();
						if (microSDfs->exists(name4delete)) {
							if(microSDfs->remove(name4delete)) {
								logText(String("Old log file deleted: '") + name4delete + "'");
							} else {
								logText(String("Error removing '") + name4delete + "'");
							}
						} else {
							logText(String("Not found '") + name4delete + "'");
						}
					}
				}
			}
		}
	}
}

/*######################################################################################*/

String getFileName(String username) {
	String safe = TELEGRAM_DATA;
	safe += "/";
	username.toUpperCase();
	for (int i = 0; i < username.length(); i++) {
		char c = username[i];
		if (isalnum(c) || c == '_' || c == '-') {
			safe += c;
		}
	}
	if (safe.length() == 0) {
		safe = "00000000";
	}
	return safe + ".dat";
}

/*######################################################################################*/

uint32_t readVcc_mV() {
    uint32_t raw = adc1_get_raw(VCC_PIN);
    uint32_t pin_mV = esp_adc_cal_raw_to_voltage(raw, &adc_chars);
    return ((float)pin_mV * 2.0f * voltageCorrection);
}

/*######################################################################################*/

void drawBox(int x, int y, int w, int h, uint16_t fillCol, uint16_t borderCol, uint8_t borderWidth = 1) {
	tft.fillRect(x, y, w, h, borderCol);
	if (borderWidth > 0) {
		tft.fillRect(
			x + borderWidth,
			y + borderWidth,
			w - 2 * borderWidth,
			h - 2 * borderWidth,
			fillCol
		);
	}
}

/*######################################################################################*/

void showMessage(const char *msg, uint16_t color) {
	drawBox(5, 295, 471, 24, TFT_LIGHTGREY, TFT_GREY);
	tft.setTextColor(color, TFT_LIGHTGREY, true);
	tft.loadFont(NotoSansBold15);
	tft.setCursor(9, 300);
	tft.print(msg);
	tft.unloadFont();
}

/*######################################################################################*/

void showBar(void *pvParameters) {
	unsigned long last = 0;
	bool firstTime = true;
	while(true) {
		unsigned long now = millis();
		if( 
			now - last >= SHOWBAR_UPDATE &&
			!wifiUpdaterRun &&
			!voltageUpdaterRun &&
			!rStaticUpdaterRun &&
			!rDynamicUpdaterRun &&
			!showBarRun
		) {
			showBarRun = true;
			time_t tNow = time(nullptr);
			if(tNow > 86400) {
				if(firstTime) {
					char ipStr[64];
					snprintf(ipStr, sizeof(ipStr), "IP: %u.%03u.%03u.%03u", 
						myIP[0], myIP[1], myIP[2], myIP[3]
					);
					showMessage(ipStr, TFT_GREY);
					firstTime = false;
				}
				if(!firstTime) {
					char timeStr[9];
					struct tm *tm_struct = localtime(&tNow);
					strftime(timeStr, sizeof(timeStr), "%H:%M:%S", tm_struct);
					tft.setTextColor(TFT_GREY, TFT_LIGHTGREY, true);
					tft.loadFont(NotoSansBold15);
					tft.setCursor(408, 300);
					tft.print(timeStr);
					tft.unloadFont();
				}
				last = now;
			}
			showBarRun = false;
		}
	}
	vTaskDelete(NULL);
}

/*######################################################################################*/

void checkWiFi() {
	static unsigned long lastRestartAttempt = 0;
	static unsigned int counter = 0;
	unsigned long now = millis();
	if (now - lastRestartAttempt > WIFI_RESTART_INTERVAL) {
		if(WiFi.status() != WL_CONNECTED) {
			if(counter > 18) {
				logText("[WiFi]: " + String(SSID) + " Still disconnected after 18 attempts. Rebooting...");
				ESP.restart();
			}
			counter += 1;
		} else {
			counter = 0;
		}
		lastRestartAttempt = now;
	}
}

/*######################################################################################*/

void connectWiFi() {
	showMessage("Select network...", TFT_GREY);
	char msg[256];
	WiFi.mode(WIFI_STA);
	WiFi.setHostname(HOSTNAME);
	WiFi.setSleep(false);
	WiFi.setAutoReconnect(true);
	int n = WiFi.scanNetworks(false, false);

	int bestCred = -1;
  	int bestRSSI = -127;

	for (int i = 0; i < n; i++) {
		String ssidFound = WiFi.SSID(i);
		for (size_t c = 0; c < WIFI_CREDENTIALS_COUNT; c++) {
			if (ssidFound.equals(WIFI_CREDENTIALS[c].ssid)) {
				int rssiFound = WiFi.RSSI(i);
				if (rssiFound > bestRSSI) {
					bestRSSI = rssiFound;
					bestCred = c;
				}
			}
		}
	}

	if (bestCred >= 0) {
		snprintf(SSID, sizeof(SSID), "%s", WIFI_CREDENTIALS[bestCred].ssid);
		snprintf(SSID, sizeof(SSID), "%s", strupr(SSID));
		snprintf(msg, sizeof(msg), "Connecting to %s...", SSID);
		showMessage(msg, TFT_GREY);
		WiFi.begin(WIFI_CREDENTIALS[bestCred].ssid, WIFI_CREDENTIALS[bestCred].pass);
	} else {
		showMessage("No known networks in range", TFT_RED);
		while(true) {delay(1000);}
	}

	while (WiFi.status() != WL_CONNECTED) {
		delay(250);
#if SERIAL_OUT == 1
		Serial.print(".");
#endif
	}
	snprintf(msg, sizeof(msg), "Connected to %s", SSID);
	showMessage(msg, TFT_GREY);
	delay(1000);
	myIP = WiFi.localIP();
	showMessage("Setting time...", TFT_GREY);
	configTime(TIMEZONE_OFFSET_SEC, 0, NTPserver.c_str());
	delay(500);
}

/*######################################################################################*/

void meterHeader(int x, int y, const char *meterName) {
	tft.fillRect(x, y, 239, 20, TFT_GREY);
	tft.setTextColor(TFT_WHITE, TFT_GREY, true);
	tft.loadFont(NotoSansBold15);
	tft.setCursor(x + 4, y + 4);
	tft.print(meterName);
	tft.unloadFont();
}

/*######################################################################################*/

void wifiUpdate(void *pvParameters) {
	unsigned long last = 0;
	while (true) {
		unsigned long now = millis();
		if (
			now - last >= WIFI_UPDATE &&
			!showBarRun &&
			!wifiUpdaterRun &&
			!voltageUpdaterRun &&
			!rStaticUpdaterRun &&
			!rDynamicUpdaterRun &&
			WiFi.status() == WL_CONNECTED
		) {
			wifiUpdaterRun = true;
			rssi_val = WiFi.RSSI();
			rssi.updateNeedle(-rssi_val, rssi_val, 0, "%d");
			last = now;
			wifiUpdaterRun = false;
		}
	}
	vTaskDelete(NULL);
}

/*######################################################################################*/

void voltageUpdate(void *pvParameters) {
	unsigned long last = 0;
	while (true) {
		unsigned long now = millis();
		if (
			now - last >= VOLTAGE_UPDATE &&
			!showBarRun &&
			!wifiUpdaterRun &&
			!voltageUpdaterRun &&
			!rStaticUpdaterRun &&
			!rDynamicUpdaterRun
		) {
			voltageUpdaterRun = true;
			uint32_t vcc = readVcc_mV();
			voltage = vcc / 1000.0;
			volts.updateNeedle(voltage - 3.0, voltage, 0, "%.2f");
			last = now;
			voltageUpdaterRun = false;
		}
	}
	vTaskDelete(NULL);
}

/*######################################################################################*/

void rDynamicUpdate(void *pvParameters) {
	unsigned long last = 0;
	while (true) {
		unsigned long now = millis();
		if (
			now - last >= RDYNAMIC_UPDATE &&
			!showBarRun &&
			!wifiUpdaterRun &&
			!voltageUpdaterRun &&
			!rStaticUpdaterRun &&
			!rDynamicUpdaterRun
		) {
			rDynamicUpdaterRun = true;
			rDynamic = radSens.getRadIntensyDynamic() / 100.;
#if SERIAL_OUT == 1
			Serial.printf("Dynamic: %.2f uSv/h\n", rDynamic);
#endif
			radiationDynamic.updateNeedle(rDynamic, rDynamic, 0, "%.2f");
			last = now;
			rDynamicUpdaterRun = false;
		}
	}
	vTaskDelete(NULL);
}

/*######################################################################################*/

void rStaticUpdate(void *pvParameters) {
	unsigned long last = 0;
	while (true) {
		unsigned long now = millis();
		if (
			now - last >= RSTATIC_UPDATE &&
			!showBarRun &&
			!wifiUpdaterRun &&
			!voltageUpdaterRun &&
			!rStaticUpdaterRun &&
			!rDynamicUpdaterRun
		) {
			rStaticUpdaterRun = true;
			rStatic = radSens.getRadIntensyStatic() / 100.;
#if SERIAL_OUT == 1
			Serial.printf("Static: %.2f uSv/h\n", rStatic);
#endif
			radiationStatic.updateNeedle(rStatic, rStatic, 0, "%.2f");
			last = now;
			rStaticUpdaterRun = false;
		}
	}
	vTaskDelete(NULL);
}

/*######################################################################################*/

void APIGetLogByDate(AsyncWebServerRequest *request) {
    if (!microSDfs) {
        request->send(500, "text/plain", "Filesystem not initialized");
        return;
    }

    if (!request->hasParam("date")) {
        request->send(400, "text/plain", "Missing 'date' parameter");
        return;
    }

    String dateStr = request->getParam("date")->value();
    String fileName = String(LOGS_PATH) + "/log_" + dateStr + ".txt";

    File file = microSDfs->open(fileName, "r");
    if (!file || !file.available()) {
        request->send(404, "text/plain", "Log file not found or unreadable");
        return;
    }

    AsyncWebServerResponse *response = request->beginResponse("text/plain", file.size(),
        [file](uint8_t *buffer, size_t maxLen, size_t total) mutable -> size_t {
            return file.read(buffer, maxLen);
        });

    request->send(response);
}

/*######################################################################################*/

void APIVersion(AsyncWebServerRequest *request) {
    char buffer[64];
    snprintf(
		buffer, sizeof(buffer), "{\"version\":\"%s\",\"build\":\"%s\"}",
        VERSION, BUILD
    );
#if SERIAL_OUT == 1
    Serial.print(F("Version: "));
    Serial.println(buffer);
#endif

    request->send(200, "application/json", buffer);
    return;
}

/*######################################################################################*/

void APIdiskUsage(AsyncWebServerRequest *request) {
    char buffer[64];
    snprintf(
		buffer, sizeof(buffer), "{\"total\":\"%ld\",\"used\":\"%ld\"}",
        LittleFS.totalBytes(), LittleFS.usedBytes()
    );
#if SERIAL_OUT == 1
    Serial.print(F("diskUsage: "));
    Serial.println(buffer);
#endif
    request->send(200, "application/json", buffer);
    return;
}

/*######################################################################################*/

void telegramAlert() {
	static unsigned long last = 0;
	static unsigned long lastMsg = 0;
	static bool firstTime = true;
	unsigned long now = millis();

	if((now - last) > TELEGRAM_ALERT_UPDATE) {
		if((rDynamic > RadAlertThold || rStatic > RadAlertThold) && ((now - lastMsg) > ALERT_PAUSE || firstTime)) {
			fs::File root = microSDfs->open(TELEGRAM_DATA);
			if (!root || !root.isDirectory()) {
				logText("Error: [telegramAlert] failed to open " TELEGRAM_DATA);
				return;
			}

			char msg[256];
			snprintf(msg, sizeof(msg), "<code>⚠️ Radiation Alert! Threshold exceeded.\n"
				" • Static level:   %.2fµSv/h\n"
				" • Dynamic level:  %.2fµSv/h\n"
				" • Time:           %s</code>", 
				rStatic, rDynamic, getDateTimeStr(time(nullptr)).c_str()
			);
			fs::File file = root.openNextFile();
			while (file) {
				String fileName = file.name();
				int pos =  fileName.indexOf(".dat");
				String chatId = fileName.substring(0, pos);
				int64_t msg_chatId = strtoll(chatId.c_str(), nullptr, 10);
				String userName = file.readStringUntil('\n');
				bot.sendTo(msg_chatId, msg);
				logText("[AsyncTelegram2] telegramAlert sent messaage to '@" + userName + "' chatId:" + chatId);
				delay(1000);
				file = root.openNextFile();
			}
			lastMsg = now;
			firstTime = false;
		}
		last = now;
	}
}

/*######################################################################################*/

void APITelegramDeleteUser(AsyncWebServerRequest *request) {
	if (!request->hasParam("chat_id")) {
		request->send(200, "text/plain", "Incorrect arguments\r\n");
        return;
    }

	String fileName = getFileName(request->getParam("chat_id")->value());
	if(microSDfs->exists(fileName.c_str())) {
		microSDfs->remove(fileName.c_str());
		logText(String("User has deleted, file: '") + fileName + "'");
		request->send(200, "application/json", String("{\"result\":\"ok\"}"));
	} else {
		logText(String("Error deleting user, file: '") + fileName + "'");
		request->send(200, "application/json", String("{\"result\":\"error\"}"));
	}
	fileList.clear();
	scanFiles(SD_ROOT_DIR);
}

/*######################################################################################*/

void APITelegramList(AsyncWebServerRequest *request) {
	fs::File root = microSDfs->open(TELEGRAM_DATA);
	if (!root || !root.isDirectory()) {
		request->send(200, "text/plain", "Error: failed to open TELEGRAM_DATA\r\n");
        return;
    }
	StaticJsonDocument<2048> doc;
    JsonArray array = doc.to<JsonArray>();

	fs::File file = root.openNextFile();
	while (file) {
		JsonObject entry = array.createNestedObject();
		entry["userName"] = file.readStringUntil('\n');
		String fileName = file.name();
		int pos =  fileName.indexOf(".dat");
		entry["chatId"] = fileName.substring(0, pos);
		file = root.openNextFile();
	}

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

/*######################################################################################*/

void APIFileList(AsyncWebServerRequest *request) {
    if (!request->hasParam("dir")) {
		request->send(200, "text/plain", "Incorrect arguments\r\n");
        return;
    }

    String path = request->getParam("dir")->value();
    if (path == "") {
		request->send(200, "text/plain", "Empty path\r\n");
        return;
    }

    fs::File root = LittleFS.open(path);
    if (!root || !root.isDirectory()) {
		request->send(200, "text/plain", "Error: failed to open directory\r\n");
        return;
    }

    StaticJsonDocument<2048> doc;
    JsonArray array = doc.to<JsonArray>();

    fs::File file = root.openNextFile();
    while (file) {
        JsonObject entry = array.createNestedObject();
        entry["type"] = file.isDirectory() ? "directory" : "file";
        entry["name"] = file.name();
        if (!file.isDirectory()) {
            entry["size"] = file.size();
        }
        file = root.openNextFile();
    }

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

/*######################################################################################*/

void APIData(AsyncWebServerRequest *request) {
	char response[255];

	time_t tNow = time(nullptr);
	snprintf(response, sizeof(response), 
		"{\"ssid\":\"%s\",\"rDynamic\":\"%.2f\",\"rStatic\":\"%.2f\",\"voltage\":\"%.2f\",\"wifi\":\"%.1f\",\"time\":\"%lu\"}",
		SSID, rDynamic, rStatic, voltage, rssi_val, tNow
	);

	request->send(200, "application/json", response);
}

/*######################################################################################*/

void uploadMenu() {

	BotCommand commands[] = {
		{"/help", "Get help using the bot"},
		{"/start", "Start interacting with the bot"},
		{"/status", "View current bot status"},
		{"/subscribe", "Subscribe to rad alerts"},
		{"/unsubscribe", "Unsubscribe from rad alerts"},
		{"/keyboard", "Show keyboard"},
		{"/hide_keyboard", "Hide keyboard"}
	};
	for (auto &cmd : commands) {
		bot.setMyCommands(cmd.command, cmd.description);
	}

}

/*######################################################################################*/

MeterWidget templateMeter(
	int x, int y, 
	const char *title, 
	int zoneRed0, int zoneRed1, int zoneOrg0, int zoneOrg1, int zoneYell0, int zoneYell1, int zoneGrn0, int zoneGrn1,
	float diapason,
	const char *units,
	const char *val0, const char *val1, const char *val2, const char *val3, const char *val4
) {
	MeterWidget meter(&tft);
	meterHeader(x, y, title);
	meter.setZones(zoneRed0, zoneRed1, zoneOrg0, zoneOrg1, zoneYell0, zoneYell1, zoneGrn0, zoneGrn1);
	meter.analogMeter(x, y + 20, diapason, units, val0, val1, val2, val3, val4);
	return meter;
}

/*######################################################################################*/

void scanFiles(const char *path) {

    File dir = microSDfs->open(path);
    if (!dir || !dir.isDirectory()) {
        logText("Failed to open directory: " + String(path));
        return;
    }

    File entry = dir.openNextFile();
    while (entry) {
        FileInfo info;
        info.name = String(path);
        if (!info.name.endsWith("/")) info.name += "/";
        info.name += String(entry.name());
        info.size = entry.size();
        info.isDir = entry.isDirectory();
        info.modified = entry.getLastWrite();

        fileList.push_back(info);

        if (entry.isDirectory()) {
            scanFiles(info.name.c_str());
        }
        entry.close();
        entry = dir.openNextFile();
    }
}

/*######################################################################################*/

void APISDdiskUsage(AsyncWebServerRequest *request) {

	uint64_t usedBytes = 0;
	for (const auto& f : fileList) {
		if (!f.isDir) {
			usedBytes += f.size;
		}
	}

	DynamicJsonDocument doc(256);
	doc["total"] = totalBytes;
	doc["used"] = usedBytes;

	String json;
	serializeJsonPretty(doc, json);
	request->send(200, "application/json", json);
}

/*######################################################################################*/

void APISDFileList(AsyncWebServerRequest *request) {
    if (!request->hasParam("dir")) {
        request->send(200, "text/plain", "Incorrect arguments\r\n");
        return;
    }

    String pathStr = request->getParam("dir")->value();
    if (pathStr == "") {
        request->send(200, "text/plain", "Empty path\r\n");
        return;
    }

    if (!pathStr.endsWith("/")) pathStr += "/";

    std::vector<FileInfo> filtered;

    for (auto &f : fileList) {
        if (f.name.startsWith(pathStr)) {
            String relativePath = f.name.substring(pathStr.length());
            if (relativePath.length() == 0 || relativePath.indexOf('/') != -1) {
                continue;
            }
            filtered.push_back(f);
        }
    }

    std::sort(filtered.begin(), filtered.end(), [](const FileInfo &a, const FileInfo &b) {
        return a.name < b.name;
    });

    const size_t capacity = JSON_ARRAY_SIZE(50) + 50 * JSON_OBJECT_SIZE(4);
    DynamicJsonDocument doc(capacity);
    JsonArray arr = doc.to<JsonArray>();

    for (const auto &f : filtered) {
        String relativePath = f.name.substring(pathStr.length());

        JsonObject obj = arr.createNestedObject();
        obj["type"]     = f.isDir ? "directory" : "file";
        obj["name"]     = relativePath;
        obj["size"]     = f.size;
        obj["modified"] = getDateTimeStr(f.modified);
    }

    String result;
    serializeJsonPretty(doc, result);
    request->send(200, "application/json", result);
}

/*######################################################################################*/

void setup(void) {

#ifdef CPU_FREQUENCY_240
	#if CPU_FREQUENCY_240 == 1
		setCpuFrequencyMhz(240);
	#endif
#endif

	tft.init();
	tft.setRotation(1);
	Serial.begin(115200);

	volts = templateMeter(0, 148, "VOLTAGE", 0, 100, 10, 60, 20, 50, 30, 40, 1.0, "V", "3.0", "", "3.5", "", "4.0");
	rssi = templateMeter(241, 148, "WIFI", 80, 100, 70, 80, 60, 70, 0, 30, 100, "dBm", "0", "-25", "-50", "-75", "-100");
	radiationStatic = templateMeter(0, 0, "RADIATION:STATIC", 90, 100, 30, 90, 15, 30, 0, 15, 1.0, "uSv/h", "0", "", "0.5", "", "1");
	radiationDynamic = templateMeter(241, 0, "RADIATION:DYNAMIC", 90, 100, 30, 90, 15, 30, 0, 15, 1.0, "uSv/h", "0", "", "0.5", "", "1");
	rssi.updateNeedle(50, -50, 0, "%d");
	volts.updateNeedle(0.3, 3.3, 0, "%.2f");
	radiationStatic.updateNeedle(0.1, 0.1, 0, "%.2f");
	radiationDynamic.updateNeedle(0.1, 0.1, 0, "%.2f");

	adc1_config_width(ADC_WIDTH_BIT_12);
  	adc1_config_channel_atten(VCC_PIN, ADC_ATTEN_DB_11);

	esp_adc_cal_value_t val = esp_adc_cal_characterize(
        ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12,
        DEFAULT_VREF, &adc_chars
	);

#if SERIAL_OUT == 1
  	Serial.printf("ADC calibrated using %s\n",
        val == ESP_ADC_CAL_VAL_EFUSE_VREF ? "eFuse Vref" :
        val == ESP_ADC_CAL_VAL_EFUSE_TP   ? "Two‑Point" : "DEFAULT 1.1 V"
	);
#endif

	Wire.begin(RAD_SENS_SDA, RAD_SENS_SCL);

#if SERIAL_OUT == 1
	for (byte addr = 1; addr < 127; addr++) {
		Wire.beginTransmission(addr);
		if (Wire.endTransmission() == 0) {
			Serial.print("Found device at 0x");
			Serial.println(addr, HEX);
			delay(5);
		}
	}
#endif

	radSens.init();

#if SERIAL_OUT == 1
	uint8_t sensorChipId = radSens.getChipId();
	Serial.print("Chip id: 0x");
	Serial.println(sensorChipId, HEX);

	float sensitivity = radSens.getSensitivity();
	Serial.print("getSensitivity(): ");
	Serial.println(sensitivity);
#endif

	radSens.setSensitivity(RADSENS_SENSITIVITY);

	spiSD.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
	if (!SD.begin(SD_CS, spiSD)) {
		showMessage("Card Mount Failed...", TFT_RED);
		delay(2000);
  	}

	uint8_t cardType = SD.cardType();
  	if (cardType == CARD_NONE) {
		showMessage("No SD card attached...", TFT_RED);
		delay(2000);
	}

	totalBytes = SD.totalBytes();

	microSDfs = &SD;
	fileList.clear();
	scanFiles(SD_ROOT_DIR);

	connectWiFi();

	meterHeader(241, 148, SSID);

	time_t now = time(nullptr);
	while(now < 86400) {
		now = time(nullptr);
		delay(200);
	}

	LittleFS.begin(true);

	char message[255];
	snprintf(message, sizeof(message),
		"@esp32dosimeter started, build:%s, CPU frequency %dMhz", BUILD, getCpuFrequencyMhz()
	);
	while(!logText(String(message))) {
		delay(100);
		logText(String(message));
	}

	esp_reset_reason_t reason = esp_reset_reason();
	switch(reason) {
		case ESP_RST_UNKNOWN:    logText("Reset reason: UNKNOWN"); break; 
		case ESP_RST_POWERON:    logText("Reset reason: POWER IS ON"); break;
		case ESP_RST_EXT:        logText("Reset reason: External RESET"); break;
		case ESP_RST_SW:         logText("Reset reason: Software RESET"); break;
		case ESP_RST_PANIC:      logText("Reset reason: Crash (PANIC)"); break;
		case ESP_RST_INT_WDT:    logText("Reset reason: WDT: interrupt"); break;
		case ESP_RST_TASK_WDT:   logText("Reset reason: WDT: task stuck"); break;
		case ESP_RST_WDT:        logText("Reset reason: WDT: General"); break;
		case ESP_RST_DEEPSLEEP:  logText("Reset reason: Waking up from sleep");
		case ESP_RST_BROWNOUT:   logText("Reset reason: Power failure");
		case ESP_RST_SDIO:       logText("Reset reason: Reset from SDIO");
		default:                 logText("Reset reason: Unknown cause");
  	}

	if(!loadConfig()) {
		logText("Create config: " CONFIG_FILE);
		saveConfig();
	}

	server.on("/api_data", HTTP_GET, APIData);
	server.on("/api_dirlist", HTTP_GET, APIFileList);
	server.on("/api_sddirlist", APISDFileList);
	server.on("/api_diskusage", HTTP_GET, APIdiskUsage);
	server.on("/api_sddiskusage", HTTP_GET, APISDdiskUsage);
	server.on("/api_version", HTTP_GET, APIVersion);
	server.on("/api_telegram_list", HTTP_GET, APITelegramList);
	server.on("/api_telegram_delete_user", APITelegramDeleteUser);
	server.on("/api_log", HTTP_GET, APIGetLogByDate);
	server.on("/api_config", HTTP_GET, APIConfigJson);
	server.on("/api_saveconfig", HTTP_GET, APISaveConfig);

	SERVER_SEND("/assets/js/jquery-3.7.1.min.js", "/assets/js/jquery-3.7.1.min.js.gz", "application/javascript");
	SERVER_SEND("/assets/js/raphael.min.js", "/assets/js/raphael.min.js.gz", "application/javascript");
	SERVER_SEND("/assets/bootstrap/css/bootstrap.min.css", "/assets/bootstrap/css/bootstrap.min.css.gz", 
		"text/css");
	SERVER_SEND("/assets/bootstrap/js/bootstrap.min.js", "/assets/bootstrap/js/bootstrap.min.js.gz", 
		"application/javascript");

	server.on("/assets/fonts", HTTP_GET, [](AsyncWebServerRequest *request){
		String path = request->url();
		bool gzipped = false;
		String mimeType;
		if (path.endsWith(".svg")) {
			gzipped = true;
			path += ".gz";
			mimeType = "image/svg+xml";
		} else if(path.endsWith(".ttf")) {
			gzipped = true;
			path += ".gz";
			mimeType = "font/ttf";
		} else if(path.endsWith(".eot")) {
			gzipped = true;
			path += ".gz";
			mimeType = "application/vnd.ms-fontobject";
		} else if(path.endsWith(".woff")) {
			mimeType = "font/woff";
		} else if(path.endsWith(".woff2")) {
			mimeType = "font/woff2";
		} else if(path.endsWith(".css")) {
			gzipped = true;
			path += ".gz";
			mimeType = "text/css";
		}
		if (LittleFS.exists(path)) {
			AsyncWebServerResponse *response = request->beginResponse(LittleFS, path, mimeType);
			response->addHeader("Cache-Control", "public, max-age=2592000");
			if(gzipped) response->addHeader("Content-Encoding", "gzip");
			request->send(response);
			return;
		}
  		request->send(404, "text/plain", "Not found");
	});
	
	server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl(CACHE_MAX_AGE);

	server.begin();

	securedClient.setInsecure();
	securedClient.setTimeout(SECURED_CLIENT_TIMEOUT);

	if (!SD.exists(TELEGRAM_DATA)) {
		SD.mkdir(TELEGRAM_DATA);
		logText("[microSD] folder created: " TELEGRAM_DATA);
	} else {
		logText("[microSD] skipped (already exists): " TELEGRAM_DATA);
	}
	if (!SD.exists(LOGS_PATH)) {
		SD.mkdir(LOGS_PATH);
		logText("[microSD] folder created: " LOGS_PATH);
	} else {
		logText("[microSD] skipped (already exists): " LOGS_PATH);
	}

	delay(500);

	xTaskCreatePinnedToCore(voltageUpdate, "voltageUpdate", 2048, NULL, 1, &voltageUpdateHandle, 1);
	delay(X_TASK_DELAY);
	xTaskCreatePinnedToCore(rDynamicUpdate, "rDynamicUpdate", 2048, NULL, 1, &rDynamicUpdateHandle, 1);
	delay(X_TASK_DELAY);
	xTaskCreatePinnedToCore(rStaticUpdate, "rStaticUpdate", 2048, NULL, 1, &rStaticUpdateHandle, 1);
	delay(X_TASK_DELAY);
	xTaskCreatePinnedToCore(showBar, "showBar", 2048, NULL, 1, &showBarHandle, 1);
	delay(X_TASK_DELAY);
	xTaskCreatePinnedToCore(wifiUpdate, "wifiUpdate", 2048, NULL, 1, &wifiUpdateHandle, 1);

	ArduinoOTA.setHostname(HOSTNAME);
	ArduinoOTA.begin();

	bot.setUpdateTime(BOT_MTBS);
	bot.setTelegramToken(BOT_TOKEN);
	uploadMenu();
	if(bot.begin()) {
		logText(String("[AsyncTelegram2] bot '@") + bot.getBotName() + "' up and ready");
		myReplyKbd.addButton("💡Help");
		myReplyKbd.addButton("📊Status");
		myReplyKbd.addRow();
		myReplyKbd.addButton("➕Subscribe");
		myReplyKbd.addButton("➖Unsubscribe");
		myReplyKbd.addRow();
		myReplyKbd.addButton("🚫Hide keyboard");
		myReplyKbd.enableResize();
		int64_t msg_chatId = strtoll(ownerChatId.c_str(), nullptr, 10);
		bot.sendTo(msg_chatId, "I'm  up and ready! 😎");
	} else {
		logText("[AsyncTelegram2] begin error...");
	}

}

/*######################################################################################*/

void loop() {

	static unsigned long lastCheckTimeLogs = 0;
	static bool logsFirst = true;
	static bool logHasWorked = false;
	TBMessage msg;
	unsigned long now = millis();

	ArduinoOTA.handle();
	telegramAlert();
	checkWiFi();

	if ((now - lastCheckTimeLogs > LOGS_UPDATE) || logsFirst) {
		struct tm timeinfo;
		getLocalTime(&timeinfo);
		if((timeinfo.tm_min == 0 && timeinfo.tm_sec >= 1 && !logHasWorked) || logsFirst) {
			if(rStatic > 0 && rDynamic > 0 && voltage > 0 && rssi_val < -1) {
				String message = "Status: RADIATION:STATIC: " + String(rStatic) + "µSv/h, RADIATION:DYNAMIC: " +
					String(rDynamic) + "µSv/h, VOLTAGE: " + String(voltage) + "V, " + 
					SSID + ": " + String(rssi_val) + "dBm";
				logText(message);
				deleteOldLogs(10);
				logsFirst = false;
				logHasWorked = true;
			}
		} else if(logHasWorked && timeinfo.tm_min != 0) {
			logHasWorked = false;
		}
		lastCheckTimeLogs = now;
	}

	if (bot.getNewMessage(msg)) {
		String chat_id = String(msg.chatId);
		String userName = msg.sender.username;
		if (msg.text.equalsIgnoreCase("/help") || 
			msg.text.endsWith("Help")) {
			String sMsg = String("/help -- Get help using the bot\n") +
						"/start -- Start interacting with the bot\n" +
						"/subscribe -- Subscribe to rad alerts\n" +
						"/unsubscribe -- Unsubscribe from rad alerts\n" +
						"/status -- View bot status\n" +
						"/keyboard -- Show keyboard\n" +
						"/hide_keyboard -- Hide keyboard";
			bot.sendMessage(msg, sMsg);
			logText("[@" +userName + "]: '" + msg.text +"', sent help/start message, chat_id=" + chat_id);
		} else if (msg.text.equalsIgnoreCase("/start") || msg.text.equalsIgnoreCase("/keyboard")) {
			bot.sendMessage(msg, "Use the keyboard 🤖", myReplyKbd);
			logText("[@" + userName + "]: '" + msg.text +"' sent /keyboard show/ message, chat_id=" + chat_id);
		} else if (msg.text.equalsIgnoreCase("/hide_keyboard") || msg.text.endsWith("Hide keyboard")) {
			bot.removeReplyKeyboard(msg, "Keyboard closed 🤖");
			logText("[@" + userName + "]: '" + msg.text +"' sent /keyboard removed/ message, chat_id=" + chat_id);
		} else if (msg.text.equalsIgnoreCase("/status") || msg.text.endsWith("Status")) {
			String sMsg = 
					"<code>☢️ RADIATION:\n ├──── static:    " + String(rStatic) + 
					"µSv/h,\n └─── dynamic:    " + String(rDynamic) + "µSv/h,\n" +
					"⚡ VOLTAGE:      " + String(voltage) + "V\n" + 
					"📶 WIFI:         " + String(rssi_val) + "dBm\n" +
					"📡 SSID:         " + String(SSID) + "\n" +
					"🌐 IP:           " + WiFi.localIP().toString() + "\n"
					"🔖 Version:      " + String(VERSION) + ":" + BUILD + "</code>";
			bot.sendMessage(msg, sMsg);
			logText("[@" + userName + "]: '" + msg.text +"' sent status message, chat_id=" + chat_id);
		} else if (msg.text.equalsIgnoreCase("/subscribe") || msg.text.endsWith("Subscribe")) {
			String fileName = getFileName(chat_id);
			File chatFile = microSDfs->open(fileName, "r");
			if (!chatFile) {
				chatFile = microSDfs->open(fileName, "w");
				chatFile.print(userName);
				chatFile.close();
				bot.sendMessage(msg, "You're subscribed to receive radiation alerts 🤖");
				logText("[@" + userName + "]: '" + msg.text +"' user has been subscribed, chat_id=" + chat_id);
			} else {
				bot.sendMessage(msg, "You are already subscribed to receive radiation alerts 🤖");
				logText("[@" + userName + "]: '" + msg.text +"' user is already subscribed, (" +
					chatFile.name() + ") chat_id=" + chat_id);
				chatFile.close();
			}
		} else if (msg.text.equalsIgnoreCase("/unsubscribe") || msg.text.endsWith("Unsubscribe")) {
			String fileName = getFileName(chat_id);
			if(microSDfs->exists(fileName.c_str())) {
				microSDfs->remove(fileName.c_str());
			}
			bot.sendMessage(msg, "Your alert subscription has been canceled 🤖");
			logText("[@" + userName + "]: '" + msg.text +"' subscription has been canceled, chat_id=" + chat_id);
			fileList.clear();
			scanFiles(SD_ROOT_DIR);			
		} else {
			bot.sendMessage(msg, "❓ Unknown command");
			logText("[@" + userName + "]: '" + msg.text +"' unknown command, chat_id=" + chat_id);
		}
	}
}
