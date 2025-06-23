/* TODO:
 * Set up grafana
 * Rename the metric on indexdb to something other than "mymetric"
 * Update doc coment
*/

/**
 * @file ENV_IV.ino
 * @author SeanKwok (shaoxiang@m5stack.com)
 * @brief 
 * @version 0.1
 * @date 2024-01-30
 *
 *
 * @Hardwares: M5Core + Unit ENV_IV
 * @Platform Version: Arduino M5Stack Board Manager v2.1.0
 * @Dependent Library:
 * M5UnitENV: https://github.com/m5stack/M5Unit-ENV
 */

#include <string>

#include <ArduinoJson.h>
#include <esp_sntp.h>
#include <M5StickCPlus2.h>
#include <M5UnitENV.h>
#include <PubSubClient.h>
#include <StreamUtils.h>
#include <WiFi.h>

#include "secrets.h"

#define NTP_SERVER1 "0.pool.ntp.org"
#define NTP_SERVER2 "1.pool.ntp.org"
#define NTP_SERVER3 "2.pool.ntp.org"

const char *ssid = SECRET_WIFI_SSID;
const char *password = SECRET_WIFI_PASS;

const char *mqttHost = SECRET_MQTT_HOST;
const int mqttPort = SECRET_MQTT_PORT;
const char *mqttTopic = SECRET_MQTT_TOPIC;

const char *mqttClientId = SECRET_MQTT_CLIENT_ID;

const char *mqttUsername = SECRET_MQTT_USER;
const char *mqttPassword = SECRET_MQTT_PASS;

String GetDatetimeString() {
    auto date = M5.Rtc.getDate();
    auto time = M5.Rtc.getTime();

    // year
    auto dateString = String(date.year);

    // month
    dateString = dateString + "-";
    if (date.month < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(date.month);

    // day
    dateString = dateString + "-";
    if (date.date < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(date.date);

    // hour
    dateString = dateString + "T";
    if (time.hours < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(time.hours);

    // minute
    dateString = dateString + ":";
    if (time.minutes < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(time.minutes);

    // seconds
    dateString = dateString + ":";
    if (time.seconds < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(time.seconds);

    dateString = dateString + "Z";

    return dateString;
}

String GetHumanReadableDatetimeString() {
    auto date = M5.Rtc.getDate();
    auto time = M5.Rtc.getTime();
    
    auto dateString = String();

    // day
    if (date.date < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(date.date);

    // month
    dateString = dateString + "/";
    if (date.month < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(date.month);

    // year
    dateString = dateString + "/" + String(date.year);


    // hour
    dateString = dateString + " ";
    if (time.hours < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(time.hours);

    // minute
    dateString = dateString + ":";
    if (time.minutes < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(time.minutes);

    // seconds
    dateString = dateString + ":";
    if (time.seconds < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(time.seconds);

    return dateString;
}


SHT4X sht4;
BMP280 bmp;

WiFiClient wifiClient;
PubSubClient mqttClient = PubSubClient(mqttHost, mqttPort, wifiClient);

bool isMqttConnected = false;

void setup() {
    auto cfg = M5.config();

    M5.begin(cfg);

    M5.Display.setTextSize(2);

    Serial.begin(115200);
    Serial.flush();

    Serial.println("Initialising...");

    Wire.begin();

    if (!sht4.begin(&Wire, SHT40_I2C_ADDR_44, 32, 33, 400000U)) {
        Serial.println("Couldn't find SHT4x");
        M5.Display.println("Couldn't find SHT4x");
        delay(2000);
        M5.Power.powerOff();
        return;
    }

    Serial.println("Found SHT4x sensor");

    // You can have 3 different precisions, higher precision takes longer
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);

    if (!bmp.begin(&Wire, BMP280_I2C_ADDR, 32, 33, 400000U)) {
        Serial.println("Couldn't find BMP280");
        M5.Display.println("Couldn't find BMP280");
        delay(2000);
        M5.Power.powerOff();
        return;
    }

    /* Default settings from datasheet. */
    bmp.setSampling(
        // Operating Mode.
        BMP280::MODE_NORMAL,
        // Temp. oversampling.
        BMP280::SAMPLING_X2,
        // Pressure oversampling.
        BMP280::SAMPLING_X16,
        // Filtering.
        BMP280::FILTER_X16,
        // Standby time.
        BMP280::STANDBY_MS_500
    );

    M5.Display.setRotation(1);

    M5.Display.clear();
    M5.Display.setCursor(0,0);
}

void SendSensorPayloadToMqtt() {
    Serial.println();

    if (!mqttClient.connected()) {
        Serial.println("Refusing to send sensor data as MQTT client is disconnected");
        return;
    }

    Serial.println("Attempting to send sensor data");

    auto datetimeString = GetDatetimeString();
    Serial.print("Timestamp: ");
    Serial.println(datetimeString);

    JsonDocument doc;
    doc["timestamp"] = datetimeString;

    doc["device"]["name"] = SECRET_MQTT_DEVICE_NAME;

    //SHT4X
    doc["SHT4X"]["temperature"]["value"] = sht4.cTemp;
    doc["SHT4X"]["temperature"]["unit"] = "C";

    doc["SHT4X"]["humidity"]["value"] = sht4.humidity;
    doc["SHT4X"]["humidity"]["unit"] = "%";

    doc["BMP280"]["temperature"]["value"] = bmp.cTemp;
    doc["BMP280"]["temperature"]["unit"] = "C";

    doc["BMP280"]["pressure"]["value"] = bmp.pressure;
    doc["BMP280"]["pressure"]["unit"] = "Pa";

    mqttClient.beginPublish(mqttTopic, measureJson(doc), false);
    BufferingPrint bufferedClient(mqttClient, 32);
    serializeJson(doc, bufferedClient);
    bufferedClient.flush();
    
    if (mqttClient.endPublish()) {
        Serial.println("Sensor data sent successfully.");
    } else {
        auto writeError = mqttClient.getWriteError();
        Serial.print("Failed to send sensor data with write error: ");
        Serial.println(writeError);
    }
}

int batteryDisplayLength = 7;
void DisplayBattery() {
    int batteryLevel = M5.Power.getBatteryLevel();

    bool isCharging = false;
    if (!M5.Power.charge_unknown) {
        isCharging = M5.Power.isCharging();
    }

    if (!isCharging) {
        M5.Display.print("   ");
    }

    // Pad the battery level with leading space
    // To 3 characters
    if (batteryLevel < 100) {
        if (batteryLevel >= 10) {
            M5.Display.print(" ");
        } else {
            M5.Display.print("  ");
        }
    }

    // Display the battery level
    M5.Display.print(batteryLevel);
    M5.Display.print('%');
    
    //Display charging
    if (isCharging) {
        M5.Display.print("(C)");
    }
}

bool isWifiConnected = false;

const int wifiDisplayLength = 22;
void UpdateAndDisplayWiFiStatus() {
    auto status = WiFi.status();

    if (status == WL_CONNECTED) {
        auto localIp = WiFi.localIP();
        auto rssi = WiFi.RSSI();

        if (!isWifiConnected) {
            Serial.println("WiFi connected.");
            Serial.print("Local IP: ");
            Serial.println(localIp);
            Serial.print("RSSI: ");
            Serial.println(rssi);

            isWifiConnected = true;
        }

        // Print 22 characters to fully clear the old IP and RSSI
        auto oldCursorX = M5.Display.getCursorX();
        auto oldCursorY = M5.Display.getCursorY();
        M5.Display.print("                      ");
        M5.Display.setCursor(oldCursorX, oldCursorY);

        M5.Display.print(localIp);
        M5.Display.print(" (");
        M5.Display.print(rssi);
        M5.Display.print(')');
        
        int characterWidthPixels = M5.Display.fontWidth();
        M5.Display.setCursor(oldCursorX + (wifiDisplayLength * characterWidthPixels), oldCursorY);
        return;
    }

    // WiFi is not connected - flag it
    isWifiConnected = false;
    
    M5.Display.print("WiFi: ");
    if (status == WL_NO_SHIELD) {
        Serial.println("No WiFi Shield");
        M5.Display.print("No Shield       ");
    } else if (status == WL_IDLE_STATUS) {
        Serial.println("WiFi Idle");
        M5.Display.print("Idle            ");
    } else if (status == WL_NO_SSID_AVAIL) {
        Serial.println("WiFi No SSID");
        M5.Display.print("No SSID         ");
    } else if (status == WL_SCAN_COMPLETED) {
        Serial.println("WiFi Scan Complete");
        M5.Display.print("Scan Complete   ");
    } else if (status == WL_CONNECT_FAILED) {
        Serial.println("WiFi Connection Failed");
        M5.Display.print("Connect Failed  ");
    } else if (status == WL_CONNECTION_LOST) {
        Serial.println("WiFi Connection Lost");
        M5.Display.print("Connection Lost ");
    } else if (status == WL_DISCONNECTED) {
        Serial.println("WiFi Disconnected");
        M5.Display.print("Disconnected    ");
    }

    Serial.println("Attempting to connect to WiFi");

    WiFi.mode(WIFI_STA);
    WiFi.setHostname("Thermo_iot");
    
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.begin(ssid, password);
}

bool hasRtcSyncStarted = false;
bool hasRtcSynced = false;

int timestampDisplayCharacters = 20;
void UpdateAndDisplayTime() {
    if (hasRtcSynced) {
        auto timestamp = GetHumanReadableDatetimeString();
        M5.Display.print(timestamp);
        M5.Display.print(' ');
        return;
    }
    
    if (!isWifiConnected) {
        Serial.println("NTP Sync skipped: No WiFi");

        auto timestamp = GetHumanReadableDatetimeString();
        M5.Display.print(timestamp);
        M5.Display.print('?');

        return;
    }

    auto status = sntp_get_sync_status();

    Serial.print("NTP sync status: ");
    Serial.println(status);

    // Is the sync ongoing?
    if (hasRtcSyncStarted || status == SNTP_SYNC_STATUS_IN_PROGRESS) {
        auto timestamp = GetHumanReadableDatetimeString();
        M5.Display.print(timestamp);
        M5.Display.print('*');
        return;
    }

    // Is this the first check after the sync has completed?
    if (status == SNTP_SYNC_STATUS_COMPLETED) {
        hasRtcSynced = true;

        // Update the clock
        time_t t = time(nullptr) + 1;
        while (t > time(nullptr));
        M5.Rtc.setDateTime(gmtime(&t));
        
        Serial.println("NTP sync completed");
        
        auto timestamp = GetHumanReadableDatetimeString();
        Serial.print("Current Time: ");
        Serial.println(timestamp);
        
        M5.Display.print(timestamp);
        // Print a space to clear the syncing '*'
        M5.Display.print(' ');
        
        return;
    }
    
    // Start the sync
    Serial.println("Starting NTP synchronisation");
    
    configTzTime("UTC", NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);

    auto timestamp = GetHumanReadableDatetimeString();
    M5.Display.print(timestamp);
    M5.Display.print('*');
}

void DisplayStatusBar() {
    int displayWidthPixels = M5.Display.width();
    int characterWidthPixels = M5.Display.fontWidth();
    int centralSpace = (displayWidthPixels / characterWidthPixels) - (timestampDisplayCharacters + batteryDisplayLength);
    
    UpdateAndDisplayTime();

    for (int i = 0; i < centralSpace; i++) { 
        M5.Display.print(' ');
    }

    DisplayBattery();

    M5.Display.println();
    
    UpdateAndDisplayWiFiStatus();

    M5.Display.println();
}

bool isWifiClientConnected = false;

const int wifiClientDisplayLength = 16;
void UpdateAndDisplayWiFiClientStatus() {
    M5.Display.print("WiFi: ");

    isWifiClientConnected = wifiClient.connected();

    if (isWifiClientConnected) {
        M5.Display.print("Connected!");
        return;
    }
    
    if (!isWifiConnected) {
        M5.Display.print("Waiting...");
        Serial.println("Refusing to connect to WiFi client; WiFi not connected.");
        return;
    }

    Serial.print("Attempting to connect to WiFi client (");
    Serial.print(mqttHost);
    Serial.print(":");
    Serial.print(mqttPort);
    Serial.println(")");

    if (wifiClient.connect(mqttHost, mqttPort)) {
        Serial.println("Connected to WiFi client");
        M5.Display.print("Connected!");
        
        isWifiClientConnected = true;
    } else {
        Serial.println("Failed to connect to WiFi client");
        M5.Display.print("Failed    ");

        isWifiClientConnected = false;
    }
}

bool isMqttClientConnected = false;

const int mqttClientDisplayLength = 20;
void UpdateAndDisplayMqttClientStatus() {
    M5.Display.print("MQTT: ");
    
    auto state = mqttClient.state();
    
    if (state == MQTT_CONNECTED) {
        isMqttClientConnected = true;
        M5.Display.print("Connected!  ");
        return;
    }
    
    if (!isWifiClientConnected) {
        M5.Display.print("Waiting...  ");
        Serial.println("Refusing to connect to MQTT client; WiFi Client not connected.");
        return;
    }
    
    if (!hasRtcSynced) {
        M5.Display.print("Waiting...  ");
        Serial.println("Refusing to connect to MQTT client; RTC not synced.");
        return;
    }

    if (state == MQTT_CONNECTION_TIMEOUT) {
        Serial.println("MQTT state: MQTT_CONNECTION_TIMEOUT");
        M5.Display.print("Timeout     ");
    }
    if (state == MQTT_CONNECTION_LOST) {
        Serial.println("MQTT state: MQTT_CONNECTION_LOST");
        M5.Display.print("Conn Lost   ");
    }
    if (state == MQTT_CONNECT_FAILED) {
        Serial.println("MQTT state: MQTT_CONNECT_FAILED");
        M5.Display.print("Failed      ");
    }
    if (state == MQTT_DISCONNECTED) {
        Serial.println("MQTT state: MQTT_DISCONNECTED");
        M5.Display.print("Disconnected");
    }
    if (state == MQTT_CONNECT_BAD_PROTOCOL) {
        Serial.println("MQTT state: MQTT_CONNECT_BAD_PROTOCOL");
        M5.Display.print("Bad protocol");
    }
    if (state == MQTT_CONNECT_BAD_CLIENT_ID) {
        Serial.println("MQTT state: MQTT_CONNECT_BAD_CLIENT_ID");
        M5.Display.print("Bad clientId");
    }
    if (state == MQTT_CONNECT_UNAVAILABLE) {
        Serial.println("MQTT state: MQTT_CONNECT_UNAVAILABLE");
        M5.Display.print("Unavailable ");
    }
    if (state == MQTT_CONNECT_BAD_CREDENTIALS) {
        Serial.println("MQTT state: MQTT_CONNECT_BAD_CREDENTIALS");
        M5.Display.print("Bad login   ");
    }
    if (state == MQTT_CONNECT_UNAUTHORIZED) {
        Serial.println("MQTT state: MQTT_CONNECT_UNAUTHORIZED");
        M5.Display.print("Unauthorized");
    }

    Serial.println("Attempting to connect to MQTT");
    Serial.print("Client id: ");
    Serial.println(mqttClientId);
    Serial.print("Username: ");
    Serial.println(mqttUsername);

    mqttClient.connect(mqttClientId, mqttUsername, mqttPassword);
}

void DisplayLowerStatusBar() {
    M5.Display.println();

    int displayWidthPixels = M5.Display.width();
    int characterWidthPixels = M5.Display.fontWidth();
    int centralSpace = (displayWidthPixels / characterWidthPixels) - (mqttClientDisplayLength + wifiClientDisplayLength);
    
    UpdateAndDisplayWiFiClientStatus();

    for (int i = 0; i < centralSpace; i++) { 
        M5.Display.print(' ');
    }
    
    UpdateAndDisplayMqttClientStatus();
}

void WriteToSerial() {
    Serial.println("----------------");

    Serial.print("Temp (SHT40): ");
    Serial.print(sht4.cTemp);
    Serial.println("C");

    Serial.print("Temp (BMP280): ");
    Serial.print(bmp.cTemp);
    Serial.println("C");

    Serial.print("Humidity: ");
    Serial.print(sht4.humidity);
    Serial.println("% rH");

    Serial.print("Pressure: ");
    Serial.print(bmp.pressure);
    Serial.println(" Pa");

    Serial.print("Approx altitude: ");
    Serial.print(bmp.altitude);
    Serial.println(" m");

    Serial.println("----------------");
}

void WriteToDisplay() {
    M5.Display.setCursor(0, 0);

    M5.Display.setTextSize(1);
    DisplayStatusBar();
    M5.Display.println();

    // ========
    // Temperature
    // ========

    M5.Display.setTextSize(3);
    M5.Display.print(sht4.cTemp);
    M5.Display.print('C');
    M5.Display.print('-');
    M5.Display.print(bmp.cTemp);
    M5.Display.println("C");

    M5.Display.setTextSize(1);
    M5.Display.println();

    // ========
    // Humidity
    // ========

    M5.Display.setTextSize(3);
    M5.Display.print(sht4.humidity);
    M5.Display.println("% RH");

    M5.Display.setTextSize(1);
    M5.Display.println();

    // ========
    // Pressure
    // ========

    M5.Display.setTextSize(2);
    M5.Display.print(bmp.pressure / 101325);
    M5.Display.print("atm");

    M5.Display.print(" | ");

    M5.Display.print(int(bmp.pressure));
    M5.Display.println("Pa");

    M5.Display.setTextSize(1);
    
    M5.Display.println();

    DisplayLowerStatusBar();

    // ========
    // Altitude
    // ========

    // M5.Display.print(bmp.altitude);
}

unsigned int loopCount = 0;

void loop() {
    // Turn off when the power button is held
    M5.update();
    if (M5.BtnPWR.isPressed()) {
        Serial.println("Power off pressed...");
        delay(500);
        M5.Power.deepSleep();
        return;
    }

    // Update the sensors
    bmp.update();
    sht4.update();

    WriteToSerial();

    WriteToDisplay();

    if (++loopCount >= 10) {
        SendSensorPayloadToMqtt();
        loopCount = 0;
    }

    delay(1000);
}
