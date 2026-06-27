// ============================================================
// MedChill AI — Phase 2: Wi-Fi Access & Firebase Connection
// Target: ESP32 DevKit V1 (WROOM-32 Stable Framework)
// Core Compatibility: Arduino ESP32 Core v3.0+ Fully Compliant
// ============================================================

#include <WiFi.h>
#include <DHT.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <FirebaseESP32.h>

// ── NETWORK CONFIGURATION CREDENTIALS ───────────────────────
#define WIFI_SSID         "YOUR_WIFI_NAME"         // Replace with your Wi-Fi router network name
#define WIFI_PASSWORD     "YOUR_WIFI_PASSWORD"     // Replace with your Wi-Fi password

#define FIREBASE_HOST     "med-chill-ai-default-rtdb.asia-southeast1.firebasedatabase.app" // Your database endpoint
#define FIREBASE_AUTH     "geHmbDr87vj5SQMRk0w0YK15WtTgwiZrh7940gY9" // Your database token secret

// ── Pin Configuration Map ───────────────────────────────────
#define DHT_A_PIN         4       // Zone A DHT22 (Vaccines)
#define DHT_B_PIN         15      // Zone B DHT22 (Tablets)
#define DHTTYPE           DHT22  

#define SDA_PIN           21      // SH1106 OLED SDA
#define SCL_PIN           22      // SH1106 OLED SCL

#define GREEN_LED         2       // Normal State Indicator
#define RED_LED           12      // Alert State Indicator
#define BUZZER_PIN        13      // Audio Alarm Pin

#define SENSOR_LEFT_PIN   17      // Left Door Reed Switch
#define SENSOR_RIGHT_PIN  16      // Right Door Reed Switch

// ── Intervals & Parameters ──────────────────────────────────
#define ALARM_FREQUENCY   3000    // 3 kHz buzzer frequency
#define SAMPLE_INTERVAL   3000    // Read climate metrics every 3 seconds
#define CLOUD_INTERVAL    10000   // Sync telemetry to Firebase every 10 seconds

DHT dhtA(DHT_A_PIN, DHTTYPE);
DHT dhtB(DHT_B_PIN, DHTTYPE);
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// Firebase Core Engine Interfaces
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

// Edge AI Rolling Memory Buffer 
float edgeInputBuffer[12];        // Holds sequence data points
int edgeBufferIdx = 0;            // Array index pointer
bool edgeBufferFull = false;      // Armed when 12 slots are populated

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== MedChill AI: Phase 2 Cloud Initialization ===");

  // Configure I/O Map
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(SENSOR_LEFT_PIN, INPUT_PULLUP);
  pinMode(SENSOR_RIGHT_PIN, INPUT_PULLUP);

  // Initialize core audio architecture drivers (Core v3.0+ compliant)
  ledcAttach(BUZZER_PIN, ALARM_FREQUENCY, 8);

  // Start localized hardware assets
  dhtA.begin();
  dhtB.begin();
  Wire.begin(SDA_PIN, SCL_PIN);
  oled.begin();

  // Render initial display status
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(5, 20, "MedChill AI Monitor");
  oled.drawStr(5, 45, "Connecting Wi-Fi...");
  oled.sendBuffer();

  // ── ESTABLISH WIRELESS HANDSHAKE ──
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi Network");
  int networkAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && networkAttempts < 30) {
    delay(500);
    Serial.print(".");
    networkAttempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n>> Wireless Link Active! IP Address: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n>> Wi-Fi Timeout. Operating in offline failsafe mode.");
  }

  // ── INITIALIZE FIREBASE ENGINE ──
  fbConfig.host = FIREBASE_HOST;
  fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&fbConfig, &fbAuth);
  Firebase.reconnectWiFi(true);

  Serial.println(">> Cloud Synchronization Engine Online.");
  Serial.println("------------------------------------------------");
}

void loop() {
  unsigned long currentMillis = millis();

  // ── 1. CLIMATE SAMPLING ──
  if (currentMillis - lastSensorRead >= SAMPLE_INTERVAL) {
    lastSensorRead = currentMillis;
    tempA = dhtA.readTemperature();
    humA  = dhtA.readHumidity();
    tempB = dhtB.readTemperature();
    humB  = dhtB.readHumidity();

    if (!isnan(tempA)) {
      edgeInputBuffer[edgeBufferIdx % 12] = tempA;
      edgeBufferIdx++;
      if (edgeBufferIdx >= 12) {
        edgeBufferFull = true;
      }
    }
  }

  // ── 2. ASYNCHRONOUS FIREBASE CLOUD STREAMING (Every 10 seconds) ──
  if (currentMillis - lastCloudSync >= CLOUD_INTERVAL) {
    lastCloudSync = currentMillis;
    
    if (WiFi.status() == WL_CONNECTED) {
      // Stream parameters out sequentially to maintain lower power levels
      Firebase.setFloat(firebaseData, "/zoneA/temperature", tempA);
      Firebase.setFloat(firebaseData, "/zoneA/humidity", humA);
      Firebase.setFloat(firebaseData, "/zoneB/temperature", tempB);
      Firebase.setFloat(firebaseData, "/zoneB/humidity", humB);
      Firebase.setString(firebaseData, "/system/lidStatus", (digitalRead(SENSOR_LEFT_PIN) == HIGH || digitalRead(SENSOR_RIGHT_PIN) == HIGH) ? "open" : "closed");
      Firebase.setInt(firebaseData, "/system/localUptime", (int)(currentMillis / 1000));
      
      Serial.println("[CLOUD SYNC] Telemetry packets streamed to Realtime Database.");
    }
  }

  // ── 3. DOORS POLARITY PARSING (INPUT_PULLUP) ──
  int reed1 = digitalRead(SENSOR_LEFT_PIN);  
  int reed2 = digitalRead(SENSOR_RIGHT_PIN); 

  int leftDoorOpen  = (reed1 == HIGH) ? 0 : 1;
  int rightDoorOpen = (reed2 == HIGH) ? 1 : 0;
  bool generalDoorBreach = (leftDoorOpen || rightDoorOpen);

  // ── 4. SAFETY NOTIFICATION LOGIC ──
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
      Serial.printf("[LOCAL SAFE] ZoneA: %.1fC | ZoneB: %.1fC | Wi-Fi: %s\n", 
                    tempA, tempB, (WiFi.status() == WL_CONNECTED) ? "CONNECTED" : "DISCONNECTED");
    }
  }

  // ── 5. RENDER GUI DISPLAY FRAME ──
  oled.clearBuffer();
  oled.setFont(u8g2_font_5x7_tf);
  oled.drawStr(0, 7, "MedChill AI Monitor");
  oled.drawLine(0, 9, 128, 9);

  char bufA[32], bufB[32], doorBuf[32], netBuf[32];
  
  if (isnan(tempA)) sprintf(bufA, "Zone A: SENSOR FAULT");
  else sprintf(bufA, "Zone A: %.1fC  H:%.0f%%", tempA, humA);
  oled.drawStr(0, 22, bufA);

  if (isnan(tempB)) sprintf(bufB, "Zone B: SENSOR FAULT");
  else sprintf(bufB, "Zone B: %.1fC  H:%.0f%%", tempB, humB);
  oled.drawStr(0, 38, bufB);

  sprintf(doorBuf, "Doors -> L:%s  R:%s", leftDoorOpen ? "OPEN" : "OK", rightDoorOpen ? "OPEN" : "OK");
  oled.drawStr(0, 50, doorBuf);

  oled.drawLine(0, 53, 128, 53);
  
  sprintf(netBuf, "NET: %s", (WiFi.status() == WL_CONNECTED) ? "ONLINE ★" : "OFFLINE Failsafe");
  oled.drawStr(0, 63, netBuf);

  oled.sendBuffer();
}