/*
Arduino-MAX30100 oximetry / heart rate integrated sensor library
Copyright (C) 2016  OXullo Intersecans <x@brainrapers.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"

#define REPORTING_PERIOD_MS 15000  // Collect data for 15 seconds before sending

// WiFi Credentials
const char* WIFI_SSID = " YOUR SSID";
const char* WIFI_PASSWORD = "SSID PASSWORD";

// Firebase Realtime Database URL & API Key
const char* FIREBASE_HOST = "https://matra-sakhi-y2mnw0-default-rtdb.firebaseio.com";  
const char* FIREBASE_API_KEY = "AIzaSyCAnPpxwI3FrimhzgkbxvFpTXZ8WLEilnk";  

PulseOximeter pox;
uint32_t tsLastReport = 0;
float sumHeartRate = 0, sumSpO2 = 0;
int sampleCount = 0;
bool dataSent = false;

// Callback for beat detection
void onBeatDetected() {
    Serial.println("Beat!");
}

void setup() {
    Serial.begin(115200);
    Wire.begin();

    // Connect to WiFi
    connectWiFi();

    Serial.print("Initializing pulse oximeter.. ");
    if (!pox.begin()) {
        Serial.println("FAILED");
        while (1);
    }
    Serial.println("SUCCESS");

    pox.setOnBeatDetectedCallback(onBeatDetected);
}

void loop() {
    pox.update();
    delay(10);

    if (!dataSent) {
        float heartRate = pox.getHeartRate();
        float SpO2 = pox.getSpO2();
        
        if (heartRate > 0 && SpO2 > 0) {
            sumHeartRate += heartRate;
            sumSpO2 += SpO2;
            sampleCount++;
        }
    }
    
    if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
        if (!dataSent && sampleCount > 0) {
            float avgHeartRate = sumHeartRate / sampleCount;
            float avgSpO2 = sumSpO2 / sampleCount;

            Serial.printf("\nAvg Heart Rate: %.2f bpm / Avg SpO2: %.2f%%\n", avgHeartRate, avgSpO2);

            sendDataToFirebase("/sensor/heartRate", avgHeartRate);
            sendDataToFirebase("/sensor/spo2", avgSpO2);

            dataSent = true;
        } else if (dataSent) {
            Serial.println("Resetting readings for next cycle...");
            sumHeartRate = 0;
            sumSpO2 = 0;
            sampleCount = 0;
            dataSent = false;
            tsLastReport = millis();
        }
    }

    checkWiFiReconnect();
}

void sendDataToFirebase(String path, float value) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = String(FIREBASE_HOST) + path + ".json?auth=" + FIREBASE_API_KEY;
        String jsonPayload = "{\"value\": " + String(value) + ", \"timestamp\": \"" + getTimeStamp() + "\"}";

        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        int httpResponseCode = http.PUT(jsonPayload);

        if (httpResponseCode > 0) {
            Serial.printf("Data sent to %s\n", path.c_str());
        } else {
            Serial.printf("Failed to send data. HTTP Response: %d\n", httpResponseCode);
        }
        http.end();
    } else {
        Serial.println("No WiFi Connection.");
    }
}

String getTimeStamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to get time");
        return "1970-01-01 00:00:00";
    }

    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d", 
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, 
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    return String(buffer);
}

void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi...");

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
    } else {
        Serial.println("\nWiFi Connection Failed!");
    }
}

void checkWiFiReconnect() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi Disconnected! Reconnecting...");
        WiFi.disconnect();
        WiFi.reconnect();
    }
}
