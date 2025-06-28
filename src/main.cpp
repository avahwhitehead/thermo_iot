#include <string.h>
#include <time.h>

#include <ArduinoJson.h>
#include <esp_sntp.h>
#include <M5UnitENV.h>
#include <M5Unified.h>
#include <PubSubClient.h>
#include <StreamUtils.h>
#include <WiFi.h>

#include "secrets.h"

#define NTP_SERVER1 "0.pool.ntp.org"
#define NTP_SERVER2 "1.pool.ntp.org"
#define NTP_SERVER3 "2.pool.ntp.org"

#ifdef IS_M5_ATOM_LITE
    #define SDA_PORT (uint8_t) 26
    #define SCL_PORT (uint8_t) 32
#endif

#ifdef IS_M5_STICK_C_PLUS2
    #define SDA_PORT (uint8_t) 32
    #define SCL_PORT (uint8_t) 33
#endif


String GetDatetimeString() {
    int year, month, day;
    int hours, minutes, seconds;

    if (M5.Rtc.isEnabled()) {
        auto date = M5.Rtc.getDate();
        auto time = M5.Rtc.getTime();

        year = date.year;
        month = date.month;
        day = date.date;

        hours = time.hours;
        minutes = time.minutes;
        seconds = time.seconds;
    } else {
        struct timespec today;
        
        clock_gettime(CLOCK_REALTIME, &today);

        time_t now_time = today.tv_sec;
        tm* local_time = localtime(&now_time);

        year = local_time -> tm_year + 1900;
        month = local_time -> tm_mon + 1;
        day = local_time -> tm_mday;

        hours = local_time -> tm_hour;
        minutes = local_time -> tm_min;
        seconds = local_time -> tm_sec;
    }

    // year
    auto dateString = String(year);

    // month
    dateString = dateString + "-";
    if (month < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(month);

    // day
    dateString = dateString + "-";
    if (day < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(day);

    // hour
    dateString = dateString + "T";
    if (hours < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(hours);

    // minute
    dateString = dateString + ":";
    if (minutes < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(minutes);

    // seconds
    dateString = dateString + ":";
    if (seconds < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(seconds);

    dateString = dateString + "Z";

    return dateString;
}

String GetHumanReadableDatetimeString() {
    int year, month, day;
    int hours, minutes, seconds;

    if (M5.Rtc.isEnabled()) {
        auto date = M5.Rtc.getDate();
        auto time = M5.Rtc.getTime();

        year = date.year;
        month = date.month;
        day = date.date;

        hours = time.hours;
        minutes = time.minutes;
        seconds = time.seconds;
    } else {
        struct timespec today;
        
        clock_gettime(CLOCK_REALTIME, &today);

        time_t now_time = today.tv_sec;
        tm* local_time = localtime(&now_time);

        year = local_time -> tm_year + 1900;
        month = local_time -> tm_mon + 1;
        day = local_time -> tm_mday;

        hours = local_time -> tm_hour;
        minutes = local_time -> tm_min;
        seconds = local_time -> tm_sec;
    }
    
    auto dateString = String();

    // day
    if (day < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(day);

    // month
    dateString = dateString + "/";
    if (month < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(month);

    // year
    dateString = dateString + "/" + String(year);


    // hour
    dateString = dateString + " ";
    if (hours < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(hours);

    // minute
    dateString = dateString + ":";
    if (minutes < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(minutes);

    // seconds
    dateString = dateString + ":";
    if (seconds < 10) {
        dateString = dateString + "0";
    }
    dateString = dateString + String(seconds);

    return dateString;
}

SCD4X scd4;
SHT4X sht4;
BMP280 bmp;

bool isSht4xInitialised = false;
bool isScd4xInitialised = false;
bool isBmp280Initialised = false;

bool TryInitialiseSht4x() {
    if (!sht4.begin(&Wire, SHT40_I2C_ADDR_44, SDA_PORT, SCL_PORT, 400000U)) {
        Serial.println("Couldn't find SHT4x sensor");
        
        return false;
    }
    
    Serial.println("Found SHT4x sensor");

    // You can have 3 different precisions, higher precision takes longer
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);

    return true;
}

bool TryInitialiseBmp280() {
    if (!bmp.begin(&Wire, BMP280_I2C_ADDR, SDA_PORT, SCL_PORT, 400000U)) {
        Serial.println("Couldn't find BMP280 sensor");
        
        return false;
    }
    
    Serial.println("Found BMP280 sensor");

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

    return true;
}

bool TryInitialiseScd4x() {
    if (!scd4.begin(&Wire, SCD4X_I2C_ADDR, 32, 33, 400000U)) {
        Serial.println("Couldn't find SCD4X");
        return false;
    }

    // stop potentially previously started measurement
    if (scd4.stopPeriodicMeasurement()) {
        Serial.print("Error trying to execute stopPeriodicMeasurement()");
        Serial.println();
    }

    // Start Measurement
    if (scd4.startPeriodicMeasurement()) {
        Serial.print("Error trying to execute startPeriodicMeasurement()");
        Serial.println();
    }

    Serial.println("Waiting for first measurement... (5 sec)");

    return true;
}

WiFiClient wifiClient;
PubSubClient mqttClient = PubSubClient(SECRET_MQTT_HOST, SECRET_MQTT_PORT, wifiClient);

bool isMqttConnected = false;

void setup() {
    auto cfg = M5.config();

    M5.begin(cfg);

    M5.Display.setTextSize(2);

    Serial.begin(115200);
    Serial.flush();

    Serial.println("Initialising...");

    Wire.begin();

    isSht4xInitialised = TryInitialiseSht4x();

    isBmp280Initialised = TryInitialiseBmp280();

    isScd4xInitialised = TryInitialiseScd4x();

    M5.Display.setRotation(1);
    M5.Display.clear();
    M5.Display.setCursor(0,0);
}

bool hasRtcSyncStarted = false;
bool hasRtcSynced = false;

void SendSensorPayloadToMqtt() {
    Serial.println();

    if (!mqttClient.connected()) {
        Serial.println("Refusing to send sensor data as MQTT client is disconnected");
        return;
    }

    if (!hasRtcSynced) {
        Serial.println("Refusing to send sensor data as Clock has not synced");
        return;
    }

    Serial.println("Attempting to send sensor data");

    auto datetimeString = GetDatetimeString();
    Serial.print("Timestamp: ");
    Serial.println(datetimeString);

    JsonDocument doc;
    doc["timestamp"] = datetimeString;

    doc["device"]["name"] = SECRET_MQTT_DEVICE_NAME;

    if (isSht4xInitialised) {
        doc["SHT4X"]["temperature"]["value"] = sht4.cTemp;
        doc["SHT4X"]["temperature"]["unit"] = "C";
    
        doc["SHT4X"]["humidity"]["value"] = sht4.humidity;
        doc["SHT4X"]["humidity"]["unit"] = "%";
    }
    
    if (isBmp280Initialised) {
        doc["BMP280"]["temperature"]["value"] = bmp.cTemp;
        doc["BMP280"]["temperature"]["unit"] = "C";
        
        doc["BMP280"]["pressure"]["value"] = bmp.pressure;
        doc["BMP280"]["pressure"]["unit"] = "Pa";
    }
    
    if (isScd4xInitialised) {
        doc["SCD4X"]["temperature"]["value"] = scd4.getTemperature();
        doc["SCD4X"]["temperature"]["unit"] = "C";
        
        doc["SCD4X"]["humidity"]["value"] = scd4.getHumidity();
        doc["SCD4X"]["humidity"]["unit"] = "%";
        
        doc["SCD4X"]["co2"]["value"] = scd4.getCO2();
        doc["SCD4X"]["co2"]["unit"] = "ppm";
    }

    if (!isSht4xInitialised && ! isBmp280Initialised && !isScd4xInitialised) {
        Serial.println("No sensor data to write. Skipping.");
        return;
    }

    if (!mqttClient.beginPublish(SECRET_MQTT_TOPIC, measureJson(doc), false)) {
        auto writeError = mqttClient.getWriteError();
        Serial.print("Failed to beginPublish sensor data with write error: ");
        Serial.println(writeError);
    }

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
    WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS);
}

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
        
        Serial.print("Unix time: ");
        Serial.println(t);

        if (M5.Rtc.isEnabled()) {
            M5.Rtc.setDateTime(gmtime(&t));
        } else {
            // Set the system time
            struct timespec stime;
            stime.tv_sec = t;
            
            if (clock_settime(CLOCK_REALTIME, &stime) == -1) {
                // Handle error
                Serial.println("Error setting time of day");
            }
        }
        
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
    Serial.print(SECRET_MQTT_HOST);
    Serial.print(":");
    Serial.print(SECRET_MQTT_PORT);
    Serial.println(")");

    if (wifiClient.connect(SECRET_MQTT_HOST, SECRET_MQTT_PORT)) {
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
    Serial.println(SECRET_MQTT_CLIENT_ID);
    Serial.print("Username: ");
    Serial.println(SECRET_MQTT_USER);

    mqttClient.connect(SECRET_MQTT_CLIENT_ID, SECRET_MQTT_USER, SECRET_MQTT_PASS);
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
    Serial.println();
    Serial.println("----------------");

    if (isSht4xInitialised) {
        Serial.print("Temp (SHT4X): ");
        Serial.print(sht4.cTemp);
        Serial.println("C");
        
        Serial.print("Humidity (SHT4X): ");
        Serial.print(sht4.humidity);
        Serial.println("% RH");
    }

    if (isBmp280Initialised) {
        Serial.print("Temp (BMP280): ");
        Serial.print(bmp.cTemp);
        Serial.println("C");

        Serial.print("Pressure: ");
        Serial.print(bmp.pressure);
        Serial.println("Pa");

        Serial.print("Approx altitude: ");
        Serial.print(bmp.altitude);
        Serial.println("m");
    }

    if (isScd4xInitialised) {
        Serial.print("Temp (SCD4X): ");
        Serial.print(scd4.getTemperature());
        Serial.println("C");
        
        Serial.print("C02: ");
        Serial.print(scd4.getCO2());
        Serial.println("ppm");
        
        Serial.print("Humidity: ");
        Serial.print(scd4.getHumidity());
        Serial.println("% RH");
    }

    Serial.println("----------------");
    Serial.println();
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
    if (isSht4xInitialised) {
        M5.Display.print(sht4.cTemp);
        M5.Display.print('C');
    } else {
        M5.Display.print("N/A   ");
        
    }
    
    M5.Display.print('-');

    if (isBmp280Initialised) {
        M5.Display.print(bmp.cTemp);
        M5.Display.println("C");
    } else {
        M5.Display.print("N/A   ");
    }

    M5.Display.setTextSize(1);
    M5.Display.println();

    // ========
    // Humidity
    // ========

    M5.Display.setTextSize(3);
    if (isSht4xInitialised) {
        M5.Display.print(sht4.humidity);
        M5.Display.println("% RH ");
    } else {
        M5.Display.println("N/A        ");
    }

    M5.Display.setTextSize(1);
    M5.Display.println();

    // ========
    // Pressure
    // ========

    M5.Display.setTextSize(2);

    if (isBmp280Initialised) {
        auto pressure = int(bmp.pressure);
        M5.Display.print(pressure);
        M5.Display.print("Pa ");
        if (pressure < 10) M5.Display.print(" ");
        if (pressure < 100) M5.Display.print(" ");
        if (pressure < 1000) M5.Display.print(" ");
        if (pressure < 10000) M5.Display.print(" ");
        if (pressure < 100000) M5.Display.print(" ");
        if (pressure < 1000000) M5.Display.print(" ");
    } else {
        M5.Display.print("N/A   Pa ");
    }

    if (isScd4xInitialised) {
        M5.Display.print(scd4.getCO2());
        M5.Display.print("ppm");
    } else {
        M5.Display.print("N/A ppm      ");
    }

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
    Serial.print("Loop: ");
    Serial.println(++loopCount);

    // Turn off when the power button is held
    M5.update();
    if (M5.BtnPWR.isPressed()) {
        Serial.println("Power off pressed...");
        delay(500);
        M5.Power.deepSleep();
        return;
    }

    // Update the sensors
    if (isBmp280Initialised) {
        //TODO: Handle case this is unplugged
        bmp.update();
    } else{
        isBmp280Initialised = TryInitialiseBmp280();
    }
    
    if (isSht4xInitialised) {
        //TODO: Handle case this is unplugged
        sht4.update();
    } else {
        isSht4xInitialised = TryInitialiseSht4x();
    }

    if (isScd4xInitialised) {
        scd4.update();
    } else {
        isScd4xInitialised = TryInitialiseScd4x();
    }

    WriteToSerial();

    WriteToDisplay();

    if (loopCount == 15) {
        SendSensorPayloadToMqtt();
        loopCount = 0;
    }

    delay(1000);
}
