/*============================================================================================================
INTELLIGENT IRRIGATION SYSTEM - IOT ENABLED WATER MANAGEMENT SYSTEM WITH INTRUSION DETECTION MECHANISM
==============================================================================================================
Author Name: Tharis Selvaraj
Description: This program is designed to automate water management using
             ESP32 microcontroller and multiple sensors. The code integrates soil moisture sensors, 
             rain sensor, water tank level sensor, and PIR intrusion sensor to make real-time decisions.
=============================================================================================================*/
// --- BLYNK CREDENTIALS ---
#define BLYNK_TEMPLATE_ID "TMPL3LKIMP1Fg"
#define BLYNK_TEMPLATE_NAME "Smart Farming"
#define BLYNK_AUTH_TOKEN "e6YiQXKl9y4SK2hMX-ZcdP_PnHHLsTzJ"

#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- Pins ---
#define MOISTURE_1_PIN 34
#define MOISTURE_2_PIN 35
#define MOISTURE_3_PIN 32
#define RAIN_PIN 25       
#define TANK_PIN 33       
#define PUMP_1_PIN 26
#define PUMP_2_PIN 27
#define PUMP_3_PIN 14

// --- Security Pins ---
#define RADAR_PIN 13
#define SIREN_PIN 12
#define STROBE_PIN 2

LiquidCrystal_I2C lcd(0x27, 20, 4);
BlynkTimer timer;

// --- Plant Database ---
struct Plant { String name; int threshold; };
Plant cropDB[15] = {
  {"Cactus", 10}, {"Aloe Vera", 15}, {"Rosemary", 20}, {"Lavender", 20}, {"Snake Plant", 25},
  {"Thyme", 30},  {"Carrots", 35},   {"Tomato", 40},   {"Pepper", 40},   {"Corn", 45},
  {"Cucumber", 50},{"Basil", 50},    {"Lettuce", 60},  {"Mint", 70},     {"Watermelon", 80}
};

int p1_index = 7, p2_index = 7, p3_index = 7; 

// --- Dynamic Variables ---
const int tankEmptyThreshold = 15; 
int m1 = 0, m2 = 0, m3 = 0, tankLevelPct = 0;
bool rainDetected = false, tankEmpty = false;
bool p1_status = false, p2_status = false, p3_status = false;

// Security defaults to ON at boot
bool securityActive = true;

// --- Manual Override Modes (0=Auto, 1=ON, 2=OFF) ---
int p1_mode = 0;
int p2_mode = 0;
int p3_mode = 0;

// --- Security Variables ---
bool intrusionActive = false;
int alarmTimer = 0; 
bool flashState = false;

// --- Display Variables ---
unsigned long lastDisplayChange = 0;
const unsigned long displayInterval = 2500; 
int displayState = 1; 
bool systemAlertActive = false;

// ==========================================
// BLYNK CLOUD COMMANDS
// ==========================================
// Plant Menus
BLYNK_WRITE(V9)  { int i = param.asInt() - 1; if(i >= 0 && i < 15) p1_index = i; }
BLYNK_WRITE(V10) { int i = param.asInt() - 1; if(i >= 0 && i < 15) p2_index = i; }
BLYNK_WRITE(V11) { int i = param.asInt() - 1; if(i >= 0 && i < 15) p3_index = i; }

// Manual Pump Controls
BLYNK_WRITE(V14) { p1_mode = param.asInt(); }
BLYNK_WRITE(V15) { p2_mode = param.asInt(); }
BLYNK_WRITE(V16) { p3_mode = param.asInt(); }

// Security Switch
BLYNK_WRITE(V12) { 
  securityActive = param.asInt(); 
  if(!securityActive) {
    intrusionActive = false; 
    digitalWrite(SIREN_PIN, LOW);
    digitalWrite(STROBE_PIN, LOW);
  }
}

// ==========================================
// SYSTEM LOGIC (Runs every 1 sec)
// ==========================================
void processSystem() {
  m1 = map(analogRead(MOISTURE_1_PIN), 0, 4095, 0, 100);
  m2 = map(analogRead(MOISTURE_2_PIN), 0, 4095, 0, 100);
  m3 = map(analogRead(MOISTURE_3_PIN), 0, 4095, 0, 100);
  tankLevelPct = map(analogRead(TANK_PIN), 0, 4095, 0, 100);
  
  rainDetected = !digitalRead(RAIN_PIN); 
  tankEmpty = (tankLevelPct < tankEmptyThreshold);
  bool motionDetected = digitalRead(RADAR_PIN);

  // Debug Tracker
  Serial.print("PIR Sensor is reading: ");
  Serial.println(motionDetected);

  // --- 1. Security Breach Logic ---
  if (securityActive) {
    if (motionDetected && !intrusionActive) {
      intrusionActive = true;
      alarmTimer = 10; 
      Blynk.logEvent("intrusion"); 
    }
  }

  if (intrusionActive) {
    digitalWrite(SIREN_PIN, HIGH);
    digitalWrite(STROBE_PIN, !digitalRead(STROBE_PIN)); 
    alarmTimer--;
    if (alarmTimer <= 0) {
      intrusionActive = false; 
      digitalWrite(SIREN_PIN, LOW);
      digitalWrite(STROBE_PIN, LOW);
    }
  }

  // --- 2. Manual + Auto Irrigation Logic ---
  if (tankEmpty || rainDetected) {
    // Hardware safety overrides everything
    digitalWrite(PUMP_1_PIN, LOW); p1_status = false;
    digitalWrite(PUMP_2_PIN, LOW); p2_status = false;
    digitalWrite(PUMP_3_PIN, LOW); p3_status = false;
  } else {
    // Pump 1 Logic
    if (p1_mode == 1)      p1_status = true;  // Force ON
    else if (p1_mode == 2) p1_status = false; // Force OFF
    else                   p1_status = (m1 < cropDB[p1_index].threshold); // Auto
    digitalWrite(PUMP_1_PIN, p1_status ? HIGH : LOW);
    
    // Pump 2 Logic
    if (p2_mode == 1)      p2_status = true;
    else if (p2_mode == 2) p2_status = false;
    else                   p2_status = (m2 < cropDB[p2_index].threshold);
    digitalWrite(PUMP_2_PIN, p2_status ? HIGH : LOW);
    
    // Pump 3 Logic
    if (p3_mode == 1)      p3_status = true;
    else if (p3_mode == 2) p3_status = false;
    else                   p3_status = (m3 < cropDB[p3_index].threshold);
    digitalWrite(PUMP_3_PIN, p3_status ? HIGH : LOW);
  }

  // --- 3. Push to Blynk ---
  Blynk.virtualWrite(V1, m1); Blynk.virtualWrite(V2, m2); Blynk.virtualWrite(V3, m3);
  Blynk.virtualWrite(V4, tankLevelPct);
  Blynk.virtualWrite(V5, p1_status ? 1 : 0); Blynk.virtualWrite(V6, p2_status ? 1 : 0); Blynk.virtualWrite(V7, p3_status ? 1 : 0);
  Blynk.virtualWrite(V8, rainDetected ? 1 : 0);
  Blynk.virtualWrite(V13, intrusionActive ? 1 : 0); 
}

// ==========================================
// MAIN SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  pinMode(RAIN_PIN, INPUT_PULLUP);
  pinMode(PUMP_1_PIN, OUTPUT); pinMode(PUMP_2_PIN, OUTPUT); pinMode(PUMP_3_PIN, OUTPUT);
  
  // Security Pins
  pinMode(RADAR_PIN, INPUT_PULLDOWN); // Stabilized PIR pin
  pinMode(SIREN_PIN, OUTPUT); 
  pinMode(STROBE_PIN, OUTPUT);
  
  lcd.init(); lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("Connecting to Blynk");
  Blynk.begin(BLYNK_AUTH_TOKEN, "Wokwi-GUEST", "");
  lcd.clear(); lcd.print("Blynk Connected!");
  delay(1000);
  timer.setInterval(1000L, processSystem);
}

// ==========================================
// MAIN LOOP & LCD DISPLAY
// ==========================================
void loop() {
  Blynk.run(); 
  timer.run(); 

  // --- LCD Alert Hierarchy ---
  if (intrusionActive) {
    if (!systemAlertActive) lcd.clear(); systemAlertActive = true;
    
    // Create a flashing effect for the "BREACH" text every 500ms
    if (millis() % 1000 < 500) {
      lcd.setCursor(0, 0); lcd.print("!X!X!X!X!X!X!X!X!X!X");
      lcd.setCursor(0, 1); lcd.print("X  SECURITY BREACH X");
      lcd.setCursor(0, 2); lcd.print("!  ANIMAL DETECTED !");
      lcd.setCursor(0, 3); lcd.print("X!X!X!X!X!X!X!X!X!X!");
    } else {
      lcd.setCursor(0, 0); lcd.print("X!X!X!X!X!X!X!X!X!X!");
      lcd.setCursor(0, 1); lcd.print("!  SECURITY BREACH !");
      lcd.setCursor(0, 2); lcd.print("X  ANIMAL DETECTED X");
      lcd.setCursor(0, 3); lcd.print("!X!X!X!X!X!X!X!X!X!X");
    }
  } 
  else if (tankEmpty) {
    if (!systemAlertActive) lcd.clear(); systemAlertActive = true;
    lcd.setCursor(0, 0); lcd.print("!   ALERT: TANK    !");
    lcd.setCursor(0, 1); lcd.print("!      EMPTY       !");
  } 
  else if (rainDetected) {
    if (!systemAlertActive) lcd.clear(); systemAlertActive = true;
    lcd.setCursor(0, 0); lcd.print("* INFO: RAINING   *");
    lcd.setCursor(0, 1); lcd.print("* PUMPS DISABLED  *");
  } 
  else {
    if (systemAlertActive) { lcd.clear(); systemAlertActive = false; }
    
    if (millis() - lastDisplayChange > displayInterval) {
      displayState++; if (displayState > 3) displayState = 1; 
      lastDisplayChange = millis();
    }

    // Top row now cleanly shows system status without mentioning security unless there is a breach
    lcd.setCursor(0, 0); lcd.print("--- SYSTEM ACTIVE ---");
    
    // LCD now shows if a pump is running in MANUAL or AUTO mode
    if (displayState == 1) {
      lcd.setCursor(0, 1); lcd.print(" F1: "); lcd.print(cropDB[p1_index].name); lcd.print("      ");
      lcd.setCursor(0, 2); lcd.print(" Moisture: "); lcd.print(m1); lcd.print("% ("); lcd.print(cropDB[p1_index].threshold); lcd.print("%) ");
      lcd.setCursor(0, 3); lcd.print(" P1: "); lcd.print(p1_status ? "ON " : "OFF"); lcd.print(p1_mode != 0 ? " [MANUAL] " : " [AUTO]   ");
    } else if (displayState == 2) {
      lcd.setCursor(0, 1); lcd.print(" F2: "); lcd.print(cropDB[p2_index].name); lcd.print("      ");
      lcd.setCursor(0, 2); lcd.print(" Moisture: "); lcd.print(m2); lcd.print("% ("); lcd.print(cropDB[p2_index].threshold); lcd.print("%) ");
      lcd.setCursor(0, 3); lcd.print(" P2: "); lcd.print(p2_status ? "ON " : "OFF"); lcd.print(p2_mode != 0 ? " [MANUAL] " : " [AUTO]   ");
    } else if (displayState == 3) {
      lcd.setCursor(0, 1); lcd.print(" F3: "); lcd.print(cropDB[p3_index].name); lcd.print("      ");
      lcd.setCursor(0, 2); lcd.print(" Moisture: "); lcd.print(m3); lcd.print("% ("); lcd.print(cropDB[p3_index].threshold); lcd.print("%) ");
      lcd.setCursor(0, 3); lcd.print(" P3: "); lcd.print(p3_status ? "ON " : "OFF"); lcd.print(p3_mode != 0 ? " [MANUAL] " : " [AUTO]   ");
    }
  }
}