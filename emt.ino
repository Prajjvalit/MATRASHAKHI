#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// 🔹 WiFi Credentials
const char* WIFI_SSID = " YOUR SSID ";
const char* WIFI_PASSWORD = " SSID PASSWORD";

// 🔹 Firebase Configuration
const char* FIREBASE_HOST = "https://matra-sakhi-y2mnw0-default-rtdb.firebaseio.com";
const char* FIREBASE_API_KEY = "AIzaSyCAnPpxwI3FrimhzgkbxvFpTXZ8WLEilnk";

// 🔹 Sensor Pins
#define ECG_PIN 34       // AD8232 ECG Output Pin
#define ONE_WIRE_BUS 15  // DS18B20 Temperature Sensor Pin

// 🔹 Sensor Objects
Adafruit_MPU6050 mpu;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

// 🔹 Variables for Fall Detection
bool inFreeFall = false;
bool fallDetected = false;
bool lastFallState = false;
unsigned long fallTime = 0;
unsigned long fallResetTime = 0;
unsigned long lastECGTime = 0;
unsigned long lastTempTime = 0;

void setup() {
    Serial.begin(115200);
    Wire.begin();

    // ✅ Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n✅ WiFi Connected!");

    // ✅ Initialize MPU6050 (Fall Detection)
    if (!mpu.begin()) {
        Serial.println("❌ MPU6050 not found! Check wiring.");
        while (1);
    }
    Serial.println("✅ MPU6050 Initialized!");

    // ✅ Initialize DS18B20 (Temperature Sensor)
    ds18b20.begin();
    Serial.println("✅ DS18B20 Temperature Sensor Initialized!");

    Serial.println("✅ System Ready - Monitoring ECG, Fall Detection & Temperature...");
}

void loop() {
    detectFall();  // 🔹 Fall Detection

    // 🔹 Read ECG every 50ms (Non-blocking)
    if (millis() - lastECGTime >= 50) {
        lastECGTime = millis();
        readECG();
    }

    // 🔹 Read Temperature every 5 seconds
    if (millis() - lastTempTime >= 5000) {
        lastTempTime = millis();
        readTemperature();
    }

    // 🔹 Send fall detection status only if it changes
    if (fallDetected != lastFallState) {
        lastFallState = fallDetected;
        updateFallStatus();
    }
}

// 🔹 Reliable Fall Detection Function
void detectFall() {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    float accMagnitude = sqrt(a.acceleration.x * a.acceleration.x +
                              a.acceleration.y * a.acceleration.y +
                              a.acceleration.z * a.acceleration.z);

    float gyroMagnitude = sqrt(g.gyro.x * g.gyro.x +
                               g.gyro.y * g.gyro.y +
                               g.gyro.z * g.gyro.z);

    // Step 1: Detect Free Fall (Low Acceleration < 1.5)
    if (accMagnitude < 1.5) {  
        inFreeFall = true;
        fallTime = millis();
    }

    // Step 2: Detect Impact (High Acceleration > 15.0 & Gyroscope > 1.0)
    if (inFreeFall && accMagnitude > 15.0 || gyroMagnitude > 1.0) {  
        fallDetected = true;
        fallResetTime = millis();
        inFreeFall = false;
        Serial.println("🚨 FALL DETECTED! 🚨");

        // ✅ Send Fall Detection Status to Firebase
        updateFallStatus();
    }

    // Step 3: Reset Fall Detection after 10 seconds
    if (fallDetected && millis() - fallResetTime > 10000) {  
        Serial.println("✅ Resetting fall detection...");
        fallDetected = false;
        updateFallStatus();
    }
}

// 🔹 ECG Data Function
void readECG() {
    int ecgValue = analogRead(ECG_PIN);
    unsigned long timestamp = millis();  // Capture time for data logging
    
    Serial.print("ECG: ");
    Serial.print(ecgValue);
    Serial.println(fallDetected ? "  🚨 FALL DETECTED!" : "  No Fall");

    // ✅ Send ECG Data to Firebase
    String jsonPayload = "{\"value\": " + String(ecgValue) + ", \"timestamp\": " + String(timestamp) + "}";
    sendDataToFirebase("/sensor/ecg", jsonPayload);
}

// 🔹 Temperature Reading Function
void readTemperature() {
    ds18b20.requestTemperatures();  // Request temperature readings
    float temperature = ds18b20.getTempCByIndex(0);

    // Handle sensor errors
    if (temperature == -127.00) {
        Serial.println("❌ DS18B20 Error: Sensor not detected!");
        return;
    }

    Serial.print("🌡 Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");

    // ✅ Send Temperature Data to Firebase
    String jsonPayload = "{\"temperature\": " + String(temperature) + "}";
    sendDataToFirebase("/sensor/temperature", jsonPayload);
}

// 🔹 Update Fall Detection Status
void updateFallStatus() {
    String fallStatus = fallDetected ? "true" : "false";
    sendDataToFirebase("/sensor/fallDetected", "\"" + fallStatus + "\"");  
}

// 🔹 Function to Send Data to Firebase
void sendDataToFirebase(String path, String value) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = String(FIREBASE_HOST) + path + ".json?auth=" + FIREBASE_API_KEY;

        http.begin(url);
        http.addHeader("Content-Type", "application/json");

        int httpResponseCode = http.PUT(value);

        if (httpResponseCode > 0) {
            Serial.print("✅ Data sent successfully to ");
            Serial.println(path);
        } else {
            Serial.print("❌ Failed to send data. HTTP Response: ");
            Serial.println(httpResponseCode);
        }

        http.end();
    } else {
        Serial.println("❌ No WiFi Connection.");
    }
}