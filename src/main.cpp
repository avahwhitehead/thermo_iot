
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


SHT4X sht4;
BMP280 bmp;

WiFiClient wifiClient;
PubSubClient mqttClient = PubSubClient(mqttHost, mqttPort, wifiClient);

void ConnectToWifi() {
    WiFi.mode(WIFI_STA);

    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname("Thermo_iot");

    WiFi.begin(ssid, password);

    M5.Display.print("Waiting for connection");
    while (WiFi.status() != WL_CONNECTED) {
        M5.Display.print('.');
        delay(250);
    }
    M5.Display.println();

    M5.Display.println("Connected!");

    M5.Display.print("IP: ");
    M5.Display.println(WiFi.localIP());

    M5.Display.print("Signal strength: ");
    M5.Display.println(WiFi.RSSI());
}

void ConnectToMqtt() {
    delay(500);

    Serial.print("Attempting to connect to WiFi client (");
    Serial.print(mqttHost);
    Serial.print(":");
    Serial.print(mqttPort);
    Serial.println(")");

    if (wifiClient.connect(mqttHost, mqttPort)) {
        Serial.println("Connected to WiFi client");
    } else {
        Serial.println("Failed to connect to WiFi client");
    }

    delay(500);

    Serial.println("Attempting to connect to MQTT");
    Serial.print("Client id: ");
    Serial.println(mqttClientId);
    Serial.print("Username: ");
    Serial.println(mqttUsername);

    if (mqttClient.connect(mqttClientId, mqttUsername, mqttPassword)) {
        Serial.println("Connected to MQTT");
    } else {
        Serial.println("Failed to connect to MQTT");
        Serial.print("rc=");
        Serial.println(mqttClient.state());
    }
}

void PerformNtpSync() {
    if (!M5.Rtc.isEnabled()) {
        Serial.println("RTC not found");
        return;
    }
    
    Serial.println("Found RTC");
   
    Serial.println("Attempting NTP synchronisation");

    configTzTime("UTC", NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);

    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED)
    {
      M5.delay(500);
    }

    Serial.println("NTP Connected");

    // Advance one second.
    time_t t = time(nullptr)+1;
    // Synchronization in seconds
    while (t > time(nullptr));
    M5.Rtc.setDateTime(gmtime(&t));

    auto datetimeString = GetDatetimeString();
    Serial.print("Timestamp: ");
    Serial.println(datetimeString);
}

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
        BMP280::MODE_NORMAL,     /* Operating Mode. */
        BMP280::SAMPLING_X2,     /* Temp. oversampling */
        BMP280::SAMPLING_X16,    /* Pressure oversampling */
        BMP280::FILTER_X16,      /* Filtering. */
        BMP280::STANDBY_MS_500); /* Standby time. */

    M5.Display.setRotation(1);

    M5.Display.clear();
    M5.Display.setCursor(0,0);

    ConnectToWifi();

    PerformNtpSync();

    ConnectToMqtt();

    delay(1000);

    M5.Display.clear();
    M5.Display.setCursor(0,0);
}

void SendSensorPayloadToMqtt() {
    Serial.println();
    Serial.println("Attempting to send sensor data");

    if (!mqttClient.connected()) {
        Serial.println("Refusing to send sensor data as MQTT client is disconnected");
        return;
    }

    auto datetimeString = GetDatetimeString();
    Serial.print("Timestamp: ");
    Serial.println(datetimeString);

    JsonDocument doc;
    doc["timestamp"] = datetimeString;

    //SHT4X
    doc["SHT4X"]["temperature"]["value"] = sht4.cTemp;
    doc["SHT4X"]["temperature"]["unit"] = "C";

    doc["SHT4X"]["humidity"]["value"] = sht4.humidity;
    doc["SHT4X"]["humidity"]["unit"] = "%";

    doc["BMP280"]["temperature"]["value"] = bmp.cTemp;
    doc["BMP280"]["temperature"]["unit"] = "C";

    doc["BMP280"]["pressue"]["value"] = bmp.pressure;
    doc["BMP280"]["pressue"]["unit"] = "Pa";

    mqttClient.beginPublish(mqttTopic, measureJson(doc), false);
    BufferingPrint bufferedClient(mqttClient, 32);
    serializeJson(doc, bufferedClient);
    bufferedClient.flush();
    mqttClient.endPublish();
}

void DisplayBattery() {
    int batteryLevel = M5.Power.getBatteryLevel();

    int displayWidthPixels = M5.Display.width();
    int characterWidthPixels = M5.Display.fontWidth();

    // Offset by 3 for the maximum battery length
    // Offset by 1 for the percent sign
    // Offset by 3 for the charging indicator
    auto newCursorX = displayWidthPixels - (7 * characterWidthPixels);
    M5.Display.setCursor(newCursorX, 0);

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
    if (M5.Power.isCharging()) {
        M5.Display.print("(C)");
    } else {
        M5.Display.print("   ");
    }
    
    M5.Display.println();
}

void DisplayWiFi() {
    M5.Display.print(WiFi.localIP());
    M5.Display.print(" (");
    M5.Display.print(WiFi.RSSI());
    M5.Display.print(')');
}

void DisplayStatusBar() {
    DisplayWiFi();

    DisplayBattery();
}

void DisplayLowerStatusBar() {
    M5.Display.print("WiFi Client: ");
    if (wifiClient.connected()) {
        M5.Display.print("Connected");
    } else {
        M5.Display.print("Failed   ");
    }

    M5.Display.print("   ");
    M5.Display.print("MQTT Client: ");
    M5.Display.print(mqttClient.state());
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

    Serial.print(F("Pressure: "));
    Serial.print(bmp.pressure);
    Serial.println(" Pa");

    Serial.print(F("Approx altitude: "));
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
    M5.Display.println("Temperature (SHT4 - BMP)");
    M5.Display.println();

    // ========
    // Humidity
    // ========

    M5.Display.setTextSize(3);
    M5.Display.print(sht4.humidity);
    M5.Display.println("%");

    M5.Display.setTextSize(1);
    M5.Display.println("Humidity");
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
    M5.Display.println("Pressure");

    M5.Display.setTextSize(1);
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
