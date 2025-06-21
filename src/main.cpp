
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

#include "string"

#include "M5StickCPlus2.h"
#include "M5UnitENV.h"
#include "WiFi.h"

#include "secrets.h"

const char *ssid = SECRET_WIFI_SSID;
const char *password = SECRET_WIFI_PASS;

SHT4X sht4;
BMP280 bmp;

void ConnectToWifi() {
    WiFi.mode(WIFI_STA);

    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname("Thermo_iot");

    WiFi.begin(ssid, password);

    M5.Display.print("Waiting for connection");
    while (WiFi.status() != WL_CONNECTED) {
        M5.Display.print('.');
        delay(100);
    }
    M5.Display.println();

    M5.Display.println("Connected!");
    
    M5.Display.print("IP: ");
    M5.Display.println(WiFi.localIP());
    
    M5.Display.print("Signal strength: ");
    M5.Display.println(WiFi.RSSI());

    delay(1000);
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
    bmp.setSampling(BMP280::MODE_NORMAL,     /* Operating Mode. */
        BMP280::SAMPLING_X2,     /* Temp. oversampling */
        BMP280::SAMPLING_X16,    /* Pressure oversampling */
        BMP280::FILTER_X16,      /* Filtering. */
        BMP280::STANDBY_MS_500); /* Standby time. */

    M5.Display.setRotation(1);

    M5.Display.clear();
    M5.Display.setCursor(0,0);

    ConnectToWifi();

    M5.Display.clear();
    M5.Display.setCursor(0,0);
}

void DisplayBattery() {
    int batteryLevel = M5.Power.getBatteryLevel();

    int displayWidthPixels = M5.Display.width();
    int characterWidthPixels = M5.Display.fontWidth();

    int cursorPositionPixels = M5.Display.getCursorX();

    // Get the width of the number in characters
    // Plus one for %
    int text_width = 0;
    if (batteryLevel == 100) {
        text_width = characterWidthPixels * 4;
    } else if (batteryLevel >= 10) {
        text_width = characterWidthPixels * 3;
    } else {
        text_width = characterWidthPixels * 2;
    }

    M5.Display.setCursor(displayWidthPixels - cursorPositionPixels - text_width, 0);
    M5.Display.print(batteryLevel);
    M5.Display.println('%');
}

void DisplayStatusBar() {
    DisplayBattery();
}

void WriteToSerial() {
    // Turn off when the power button is held
    if (M5.BtnPWR.isPressed()) {
        Serial.println("Power off pressed...");
        delay(500);
        M5.Power.deepSleep();
        return;
    }

    // Read temp and humidity sensor
    Serial.println("-----SHT4X-----");
    Serial.print("Temperature: ");
    Serial.print(sht4.cTemp);
    Serial.println(" degrees C");
    Serial.print("Humidity: ");
    Serial.print(sht4.humidity);
    Serial.println("% rH");
    Serial.println("-------------\r\n");

    // Read Barometric temperature and pressor sensor
    Serial.println("-----BMP280-----");
    Serial.print(F("Temperature: "));
    Serial.print(bmp.cTemp);
    Serial.println(" degrees C");
    Serial.print(F("Pressure: "));
    Serial.print(bmp.pressure);
    Serial.println(" Pa");
    Serial.print(F("Approx altitude: "));
    Serial.print(bmp.altitude);
    Serial.println(" m");
    Serial.println("-------------\r\n");
}

void WriteToDisplay() {
    M5.Display.setCursor(0, 0);
    
    M5.Display.setTextSize(1);
    DisplayStatusBar();

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
    
    // ========
    // Altitude
    // ========

    // M5.Display.print(bmp.altitude);
}

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

    delay(1000);
}
