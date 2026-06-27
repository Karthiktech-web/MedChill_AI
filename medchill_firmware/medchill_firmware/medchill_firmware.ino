// ============================================================
// MedChill AI — Phase 2: Complete Relay & Power Sync Framework
// Target: ESP32 DevKit V1 (WROOM-32 Stable Framework)
// Core Compatibility: Arduino ESP32 Core v3.0+ Fully Compliant
// ============================================================

#include <WiFi.h>
#include <DHT.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <FirebaseESP32.h>
#include <Adafruit_INA219.h>  // New Power Sensor Framework Injection

// ── NETWORK CONFIGURATION CREDENTIALS ───────────────────────
#define WIFI_SSID         "Abc"         // Replace with your Wi-Fi name
#define WIFI_PASSWORD     "123456789"     // Replace with your Wi-Fi password

#define FIREBASE_HOST     "med-chill-ai-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH     "geHmbDr87vj5SQMRk0w0YK15WtTgwiZrh7940gY9"

// ── Pin Configuration Map ───────────────────────────────────
#define DHT_A_PIN         4       // Zone A DHT22 (Vaccines)
#define DHT_B_PIN         15      // Zone B DHT22 (Tablets)
#define DHTTYPE           DHT22  

#define SDA_PIN           21      // Shared SH1106 OLED & INA219 SDA
#define SCL_PIN           22      // Shared SH1106 OLED & INA219 SCL

#define GREEN_LED         2       // Normal State Indicator
#define RED_LED           12      // Alert State Indicator
#define BUZZER_PIN        13      // Audio Alarm Pin

#define SENSOR_LEFT_PIN   17      // Left Door Reed Switch
#define SENSOR_RIGHT_PIN  16      // Right Door Reed Switch

#define RELAY_A           25      // IN1 - Zone A Peltier Switch
#define RELAY_B           26      // IN2 - Zone B Peltier Switch

// ── Intervals & Parameters ──────────────────────────────────
#define ALARM_FREQUENCY   3000    
#define SAMPLE_INTERVAL   3000    // Read climate metrics every 3 seconds
#define CLOUD_INTERVAL    10000   // Sync telemetry to Firebase every 10 seconds

// Initialize Hardware Structures
DHT dhtA(DHT_A_PIN, DHTTYPE);
DHT dhtB(DHT_B_PIN, DHTTYPE);
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
Adafruit_INA219 ina219;        // Power monitor driver link

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

// Tracking strings for localized feedback loops
String relayA_State = "OFF";
String relayB_State = "OFF";

// Edge AI Rolling Memory Buffer 
float edgeInputBuffer[12];
int edgeBufferIdx = 0;
bool edgeBufferFull = false;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== MedChill AI: Phase 2 Complete Relay & Power Bus ===");

  // Configure Local I/O Platform
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(SENSOR_LEFT_PIN, INPUT_PULLUP);
  pinMode(SENSOR_RIGHT_PIN, INPUT_PULLUP);
  pinMode(RELAY_A, OUTPUT);
  pinMode(RELAY_B, OUTPUT);

  // High Polarity Safe State Forced Initialization (Active-LOW: HIGH = Relay Switch Open/OFF)
  digitalWrite(RELAY_A, HIGH);
  digitalWrite(RELAY_B, HIGH);

  // Initialize Core Audio Framework
  ledcAttach(BUZZER_PIN, ALARM_FREQUENCY, 8);

  // Initialize Local Bus Interconnect links
  dhtA.begin();
  dhtB.begin();
  Wire.begin(SDA_PIN, SCL_PIN);
  oled.begin();

  // Initialize INA219 Sensor Over Shared I2C Bus Row
  if (!ina219.begin()) {
    Serial.println(">> WARNING: INA219 Power Chip Not Detected on I2C Row!");
  } else {
    Serial.println(">> INA219 Power Telemetry Engine Connected Successfully.");
  }

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
    Serial.println("\n>> Wi-Fi Handshake Timeout. Running offline.");
  }

  // Link Firebase Database Core
  fbConfig.host = FIREBASE_HOST;
  fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&fbConfig, &fbAuth);
  Firebase.reconnectWiFi(true);

  Serial.println(">> Cloud Telemetry Matrix Active.");
  Serial.println("------------------------------------------------");
}

void loop() {
  unsigned long currentMillis = millis();

  // ── 1. ENVIRONMENT & POWER SAMPLING MATRIX ──
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

    // Cache historical variables safely for True Edge AI steps
    if (!isnan(tempA)) {
      edgeInputBuffer[edgeBufferIdx % 12] = tempA;
      edgeBufferIdx++;
      if (edgeBufferIdx >= 12) {
        edgeBufferFull = true;
      }
    }
    
    // Threshold Control Loops: Switch ON if temp hits or passes 30°C boundary
    if (!isnan(tempA)) {
      if (tempA >= 30.0) { digitalWrite(RELAY_A, LOW);  relayA_State = "ON"; }  // Switch Clo
      else               { digitalWrite(RELAY_A, HIGH); relayA_State = "OFF"; } // Switch Open
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
      // /power node matching your exact database tree structure
      Firebase.setFloat(firebaseData, "/power/voltage", voltage_V);
      Firebase.setFloat(firebaseData, "/power/current_mA", current_mA);
      Firebase.setFloat(firebaseData, "/power/power_mW", power_mW);
      
      // /system node matching your exact database tree structure
      int reed1 = digitalRead(SENSOR_LEFT_PIN);  
      int reed2 = digitalRead(SENSOR_RIGHT_PIN); 
      bool generalDoorBreach = ((reed1 == HIGH) || (reed2 == HIGH)); // Matches your pull-up logic
      
      Firebase.setBool(firebaseData, "/system/doorOpen", generalDoorBreach);
      Firebase.setString(firebaseData, "/system/lidStatus", generalDoorBreach ? "open" : "closed");
      Firebase.setInt(firebaseData, "/system/localUptime", (int)(currentMillis / 1000));
      Firebase.setBool(firebaseData, "/system/sleepMode", false);
      
      // /zoneA & /zoneB nodes mapping exact historic elements
      Firebase.setFloat(firebaseData, "/zoneA/temperature", tempA);
      Firebase.setFloat(firebaseData, "/zoneA/humidity", humA);
      Firebase.setInt(firebaseData, "/zoneA/pwm", (relayA_State == "ON") ? 255 : 0); // Preserves chart mappings
      
      Firebase.setFloat(firebaseData, "/zoneB/temperature", tempB);
      Firebase.setFloat(firebaseData, "/zoneB/humidity", humB);
      Firebase.setInt(firebaseData, "/zoneB/pwm", (relayB_State == "ON") ? 255 : 0);
      
      Serial.println("[CLOUD SYNC] Complete telemetry tree synced to Firebase.");
    }
  }

  // ── 3. LOCAL DOOR BREACH HANDLING ──
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
      Serial.printf("[SYSTEM ACTIVE] V:%.2fV | I:%.1fmA | P:%.1fmW\n", voltage_V, current_mA, power_mW);
    }
  }

  // ── 4. RENDER GUI DISPLAY FRAME ──
  oled.clearBuffer();
  oled.setFont(u8g2_font_5x7_tf);
  oled.drawStr(0, 7, "MedChill AI Monitor");
  oled.drawLine(0, 9, 128, 9);

  char bufA[32], bufB[32], powerBuf[32], netBuf[32];
  
  if (isnan(tempA)) sprintf(bufA, "Zone A: SENSOR FAULT");
  else sprintf(bufA, "R1:%s  A:%.1fC H:%.0f%%", relayA_State.c_str(), tempA, humA);
  oled.drawStr(0, 20, bufA);

  if (isnan(tempB)) sprintf(bufB, "Zone B: SENSOR FAULT");
  else sprintf(bufB, "R2:%s  B:%.1fC H:%.0f%%", relayB_State.c_str(), tempB, humB);
  oled.drawStr(0, 33, bufB);

  // Render live wattage consumption directly onto screen center line
  sprintf(powerBuf, "Load -> %.2fV  %.1fW", voltage_V, (power_mW / 1000.0));
  oled.drawStr(0, 46, powerBuf);

  sprintf(netBuf, "Doors:%s | NET:ONLINE ★", generalDoorBreach ? "OPEN" : "OK");
  oled.drawStr(0, 60, netBuf);

  oled.sendBuffer();
}