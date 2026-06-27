// ============================================================
// MedChill AI — Phase 3: Complete Edge AI & Relay Inference
// Target: ESP32 DevKit V1 (WROOM-32 Stable Framework)
// Core Compatibility: Arduino ESP32 Core v3.0+ Fully Compliant
// ============================================================

#include <WiFi.h>
#include <DHT.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <FirebaseESP32.h>
#include <Adafruit_INA219.h>
#include "medchill_model_data.h"  // Local parameter links

// ── NETWORK CONFIGURATION CREDENTIALS ───────────────────────
#define WIFI_SSID         "Abc" 
#define WIFI_PASSWORD     "123456789" 

#define FIREBASE_HOST     "med-chill-ai-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH     "geHmbDr87vj5SQMRk0w0YK15WtTgwiZrh7940gY9"

// ── Pin Configuration Map ───────────────────────────────────
#define DHT_A_PIN         4       
#define DHT_B_PIN         15      
#define DHTTYPE           DHT22   

#define SDA_PIN           21      // Shared I2C Bus Row
#define SCL_PIN           22      

#define GREEN_LED         2       
#define RED_LED           12      
#define BUZZER_PIN        13      

#define SENSOR_LEFT_PIN   17      
#define SENSOR_RIGHT_PIN  16      

#define RELAY_A           25      // IN1 - Zone A Peltier Switch (Active-LOW)
#define RELAY_B           26      // IN2 - Zone B Peltier Switch (Active-LOW)

// ── Intervals & Parameters ──────────────────────────────────
#define ALARM_FREQUENCY   3000    
#define SAMPLE_INTERVAL   3000    // Read climate metrics every 3 seconds
#define CLOUD_INTERVAL    10000   // Sync telemetry to Firebase every 10 seconds

// Initialize Hardware Drivers
DHT dhtA(DHT_A_PIN, DHTTYPE);
DHT dhtB(DHT_B_PIN, DHTTYPE);
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
Adafruit_INA219 ina219;        

FirebaseData firebaseData;
FirebaseConfig fbConfig;
FirebaseAuth fbAuth;

// Non-blocking processing timers
unsigned long lastSensorRead = 0;
unsigned long lastAlarmToggle = 0;
unsigned long lastCloudSync = 0;
bool alarmState = false;

// Global metric registers
float tempA = 0.0, humA = 0.0;
float tempB = 0.0, humB = 0.0;

// Power Telemetry Storage Registers
float voltage_V = 0.0;
float current_mA = 0.0;
float power_mW = 0.0;

String relayA_State = "OFF";
String relayB_State = "OFF";

// Edge AI Rolling Memory Buffer 
float edgeInputBuffer[12];
int edgeBufferIdx = 0;
bool edgeBufferFull = false;
float predictedTempA = 0.0; // Stores the output of the offline predictive engine

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== MedChill AI: Phase 3 Native Edge AI Active ===");

  // Configure Local I/O Platform
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(SENSOR_LEFT_PIN, INPUT_PULLUP);
  pinMode(SENSOR_RIGHT_PIN, INPUT_PULLUP);
  pinMode(RELAY_A, OUTPUT);
  pinMode(RELAY_B, OUTPUT);

  // Active-LOW Safe State Boot (HIGH = Relay Switch Open/OFF)
  digitalWrite(RELAY_A, HIGH);
  digitalWrite(RELAY_B, HIGH);

  // Initialize Core Audio Framework
  ledcAttach(BUZZER_PIN, ALARM_FREQUENCY, 8);

  // Initialize Local I2C/Sensor Buses
  dhtA.begin();
  dhtB.begin();
  Wire.begin(SDA_PIN, SCL_PIN);
  oled.begin();

  if (!ina219.begin()) {
    Serial.println(">> WARNING: INA219 Power Chip Not Detected!");
  } else {
    Serial.println(">> INA219 Power Telemetry Engine Active.");
  }

  Serial.println(">> Embedded Edge Time-Series Forecast Engine Loaded.");

  // Visual Display Feedback
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(5, 20, "MedChill AI Monitor");
  oled.drawStr(5, 45, "Connecting Wi-Fi...");
  oled.sendBuffer();

  // Establish Wi-Fi Connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int networkAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && networkAttempts < 30) {
    delay(500);
    Serial.print(".");
    networkAttempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n>> Network Verified! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n>> Wi-Fi Handshake Timeout. Operating offline.");
  }

  // Link Firebase Database Core
  fbConfig.host = FIREBASE_HOST;
  fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&fbConfig, &fbAuth);
  Firebase.reconnectWiFi(true);

  Serial.println(">> Edge Inference Telemetry Node Stabilized.");
  Serial.println("------------------------------------------------");
}

void loop() {
  unsigned long currentMillis = millis();

  // ── 1. ENVIRONMENT, POWER, & EDGE FORECASTING MATRIX ──
  if (currentMillis - lastSensorRead >= SAMPLE_INTERVAL) {
    lastSensorRead = currentMillis;
    
    // Read Climate Inputs
    tempA = dhtA.readTemperature();
    humA  = dhtA.readHumidity();
    tempB = dhtB.readTemperature();
    humB  = dhtB.readHumidity();

    // Read Power Monitoring Metrics from INA219
    voltage_V = ina219.getBusVoltage_V();
    current_mA = ina219.getCurrent_mA();
    power_mW = ina219.getPower_mW();

    // Populate historical buffer for the Edge AI model
    if (!isnan(tempA)) {
      edgeInputBuffer[edgeBufferIdx % 12] = tempA;
      edgeBufferIdx++;
      if (edgeBufferIdx >= 12) {
        edgeBufferFull = true;
      }
    }
    
    // Execute Native Edge Predictive Inference once window is complete
    if (edgeBufferFull && !isnan(tempA)) {
      // Calculates the mathematical trend slope across the 12-slot data sequence
      float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
      for (int i = 0; i < 12; i++) {
        int chronologicalIdx = (edgeBufferIdx + i) % 12;
        float x = i;
        float y = edgeInputBuffer[chronologicalIdx];
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
      }
      float trendSlope = (12 * sumXY - sumX * sumY) / (12 * sumX2 - sumX * sumX);
      
      // Projects the trend slope 30 minutes (600 processing cycles) into the future
      predictedTempA = tempA + (trendSlope * 588.0f);
      
      // Boundary clamping to safeguard against sudden measurement glitches
      if (predictedTempA > 60.0f) predictedTempA = 60.0f;
      if (predictedTempA < 0.0f)  predictedTempA = 0.0f;
    } else {
      predictedTempA = tempA; // Fallback while rolling cache populates
    }
    
    // Active-LOW Core Threshold Control Loops (Turns cooling ON at >= 30.0°C)
    if (!isnan(tempA)) {
      if (tempA >= 30.0) { digitalWrite(RELAY_A, LOW);  relayA_State = "ON"; }  
      else               { digitalWrite(RELAY_A, HIGH); relayA_State = "OFF"; } 
    }
    if (!isnan(tempB)) {
      if (tempB >= 30.0) { digitalWrite(RELAY_B, LOW);  relayB_State = "ON"; }
      else               { digitalWrite(RELAY_B, HIGH); relayB_State = "OFF"; }
    }
  }

  // ── 2. EXACT FIREBASE CLOUD PACKET TREE SYNC (Every 10 seconds) ──
  if (currentMillis - lastCloudSync >= CLOUD_INTERVAL) {
    lastCloudSync = currentMillis;
    
    if (WiFi.status() == WL_CONNECTED) {
      Firebase.setFloat(firebaseData, "/power/voltage", voltage_V);
      Firebase.setFloat(firebaseData, "/power/current_mA", current_mA);
      Firebase.setFloat(firebaseData, "/power/power_mW", power_mW);
      
      int reed1 = digitalRead(SENSOR_LEFT_PIN);  
      int reed2 = digitalRead(SENSOR_RIGHT_PIN); 
      bool generalDoorBreach = ((reed1 == HIGH) || (reed2 == HIGH));
      
      Firebase.setBool(firebaseData, "/system/doorOpen", generalDoorBreach);
      Firebase.setString(firebaseData, "/system/lidStatus", generalDoorBreach ? "open" : "closed");
      Firebase.setInt(firebaseData, "/system/localUptime", (int)(currentMillis / 1000));
      Firebase.setBool(firebaseData, "/system/sleepMode", false);
      
      Firebase.setFloat(firebaseData, "/zoneA/temperature", tempA);
      Firebase.setFloat(firebaseData, "/zoneA/humidity", humA);
      Firebase.setInt(firebaseData, "/zoneA/pwm", (relayA_State == "ON") ? 255 : 0);
      Firebase.setFloat(firebaseData, "/zoneA/aiPredictedTemp", predictedTempA); 
      
      Firebase.setFloat(firebaseData, "/zoneB/temperature", tempB);
      Firebase.setFloat(firebaseData, "/zoneB/humidity", humB);
      Firebase.setInt(firebaseData, "/zoneB/pwm", (relayB_State == "ON") ? 255 : 0);
      
      Serial.println("[CLOUD SYNC] Telemetry tree + Edge AI prediction synced to Firebase.");
    }
  }

  // ── 3. LOCAL SECURITY & ALARM OVERRIDES ──
  int reed1 = digitalRead(SENSOR_LEFT_PIN);  
  int reed2 = digitalRead(SENSOR_RIGHT_PIN); 
  int leftDoorOpen  = (reed1 == HIGH) ? 0 : 1;
  int rightDoorOpen = (reed2 == HIGH) ? 1 : 0;
  bool generalDoorBreach = (leftDoorOpen || rightDoorOpen);

  if (generalDoorBreach) {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH); 
    if (currentMillis - lastAlarmToggle >= 250) {
      lastAlarmToggle = currentMillis;
      alarmState = !alarmState;
      ledcWrite(BUZZER_PIN, alarmState ? 127 : 0);
    }
  } 
  else if (isnan(tempA) || isnan(tempB)) {
    digitalWrite(GREEN_LED, LOW);
    ledcWrite(BUZZER_PIN, 127);
    if (currentMillis - lastAlarmToggle >= 500) {
      lastAlarmToggle = currentMillis;
      alarmState = !alarmState;
      digitalWrite(RED_LED, alarmState);
    }
  } 
  else {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    ledcWrite(BUZZER_PIN, 0);
    
    if (currentMillis - lastAlarmToggle >= SAMPLE_INTERVAL) {
      lastAlarmToggle = currentMillis;
      Serial.printf("[SYSTEM ACTIVE] Real A: %.1fC | AI Forecast: %.1fC | Power: %.1fW\n", 
                    tempA, predictedTempA, (power_mW / 1000.0));
    }
  }

  // ── 4. RENDER PHASE 3 DASHBOARD GUI DISPLAY FRAME ──
  oled.clearBuffer();
  oled.setFont(u8g2_font_5x7_tf);
  oled.drawStr(0, 7, "MedChill AI Monitor");
  oled.drawLine(0, 9, 128, 9);

  char bufA[32], bufB[32], aiBuf[32], powerBuf[32];
  
  if (isnan(tempA)) sprintf(bufA, "Zone A: SENSOR FAULT");
  else sprintf(bufA, "R1:%s  A:%.1fC H:%.0f%%", relayA_State.c_str(), tempA, humA);
  oled.drawStr(0, 20, bufA);

  if (isnan(tempB)) sprintf(bufB, "Zone B: SENSOR FAULT");
  else sprintf(bufB, "R2:%s  B:%.1fC H:%.0f%%", relayB_State.c_str(), tempB, humB);
  oled.drawStr(0, 32, bufB);

  if (!edgeBufferFull) {
    sprintf(aiBuf, "AI FORECAST -> CALIBRATING");
  } else {
    sprintf(aiBuf, "AI FORECAST -> %.1fC (30m)", predictedTempA);
  }
  oled.drawStr(0, 45, aiBuf);

  sprintf(powerBuf, "Load -> %.1fW | NET:ONLINE ★", (power_mW / 1000.0));
  oled.drawStr(0, 58, powerBuf);

  oled.sendBuffer();
}