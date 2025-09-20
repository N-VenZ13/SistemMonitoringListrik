#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PZEM004Tv30.h>
#include <HardwareSerial.h>
#include <LiquidCrystal_I2C.h>

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2); // Alamat I2C mungkin 0x3F atau 0x27 tergantung modul

// --- Konfigurasi WiFi & Server ---
#define RELAY1_PIN 18
#define RELAY2_PIN 19

const char* ssid = "admin";         // Ganti dengan SSID WiFi Anda
const char* password = "admin123"; // Ganti dengan Password WiFi Anda

// Ganti dengan IP Address PC/Laptop Anda yang menjalankan Laravel
const char* serverIp = "nyimas.xyz"; // PASTIKAN INI IP SERVER LARAVEL ANDA
const int serverPort = 443;

// URL API Laravel
String serverBaseUrl = "https://" + String(serverIp) + ":" + String(serverPort);
String pzemStoreUrl = serverBaseUrl + "/api/pzem/store";
String relayStatusUrl = serverBaseUrl + "/api/relay/status";
String limitConfigUrl = serverBaseUrl + "/api/limit/config";

// Gunakan HardwareSerial2 untuk komunikasi dengan PZEM (Pin 16=RX, 17=TX di ESP32)
HardwareSerial pzemSerial(2); 
PZEM004Tv30 pzem(pzemSerial, 16, 17); // Sesuaikan pin RX/TX jika Anda menggunakan pin berbeda

// --- Variabel Global ---
const long interval = 1000;           // Interval utama loop (ms) -> 1 detik

// --- Variabel untuk Fitur Batas Pemakaian ---
bool limitFeatureActive = false;
float energyLimitKwh = 0.0; 
bool limitExceededNotified = false; 
unsigned long lastLimitConfigCheckMillis = 0;
const long limitConfigIntervalMillis = 5 * 60 * 1000; // Cek konfigurasi limit setiap 5 menit

// --- Variabel untuk mengontrol pesan di LCD ---
String lcdLine1Override = "";
String lcdLine2Override = "";
unsigned long lcdOverrideEndMillis = 0;
const long lcdOverrideDurationMillis = 10000; // Tampilkan pesan override selama 10 detik


// --- Fungsi Setup ---
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000); // Tunggu Serial Monitor siap (timeout 5 detik)
  Serial.println("\n[SETUP] Starting...");

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  Serial.println("[SETUP] LCD Initialized.");

  // Inisialisasi PZEM Serial
  pzemSerial.begin(9600, SERIAL_8N1, 16, 17); // Pastikan pin RX/TX sesuai
  Serial.println("[SETUP] PZEM Serial Initialized.");

  // Inisialisasi Pin Relay
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, LOW); // Set state awal relay (misalnya LOW = OFF)
  digitalWrite(RELAY2_PIN, LOW);
  Serial.println("[SETUP] Relay Pins Initialized.");

  // Mulai koneksi WiFi
  connectToWiFi();

  // Ambil konfigurasi batas awal jika WiFi terkoneksi
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[SETUP] WiFi Connected. Checking initial limit configuration...");
    checkLimitConfiguration();
    lastLimitConfigCheckMillis = millis(); // Set timer awal untuk pengecekan periodik
  } else {
    Serial.println("[SETUP] WiFi not connected. Skipping initial limit configuration check.");
    lastLimitConfigCheckMillis = 0; // Akan dicek di loop pertama jika WiFi konek nanti
  }
  Serial.println("[SETUP] Setup complete.");
}

// --- Fungsi Koneksi WiFi ---
void connectToWiFi() {
  Serial.print("[WiFi] Connecting to: ");
  Serial.println(ssid);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  WiFi.mode(WIFI_STA); // Set ESP32 sebagai WiFi Station
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) { // Timeout sekitar 15 detik
    delay(500);
    Serial.print(".");
    lcd.setCursor(attempts % 16, 1); 
    lcd.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");
    lcd.setCursor(0, 1);
    // lcd.print(WiFi.localIP());
    delay(2000); 
  } else {
    Serial.println("\n[WiFi] Failed to connect.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed!");
    delay(3000); 
  }
}

// --- Fungsi Cek Konfigurasi Batas dari Server ---
void checkLimitConfiguration() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[LIMIT_CFG] Cannot check, WiFi disconnected.");
    return;
  }

  HTTPClient http;
  Serial.print("[LIMIT_CFG] Checking from: ");
  Serial.println(limitConfigUrl);
  http.begin(limitConfigUrl);
  http.addHeader("Accept", "application/json");

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("[LIMIT_CFG] Received: " + payload);

    StaticJsonDocument<128> doc; 
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      bool prevLimitFeatureActive = limitFeatureActive;
      float prevEnergyLimitKwh = energyLimitKwh;

      limitFeatureActive = doc["active"] | false; 
      energyLimitKwh = doc["limit_kwh"] | 0.0;   

      Serial.printf("[LIMIT_CFG] Status: %s, Limit kWh: %.3f\n", limitFeatureActive ? "Aktif" : "Tidak Aktif", energyLimitKwh);

      if (prevLimitFeatureActive != limitFeatureActive || abs(prevEnergyLimitKwh - energyLimitKwh) > 0.001) {
        limitExceededNotified = false;
        Serial.println("[LIMIT_CFG] Config changed, notification flag reset.");
      }
    } else {
      Serial.print("[LIMIT_CFG] Failed to parse JSON: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.printf("[LIMIT_CFG] Failed. HTTP Code: %d, Error: %s\n", httpCode, http.errorToString(httpCode).c_str());
  }
  http.end();
}


// --- Fungsi Loop Utama ---
void loop() {
  unsigned long currentMillis = millis();

  // 0. Cek Konfigurasi Batas Pemakaian secara Periodik
  if (WiFi.status() == WL_CONNECTED && (currentMillis - lastLimitConfigCheckMillis >= limitConfigIntervalMillis || (lastLimitConfigCheckMillis == 0 && WiFi.status() == WL_CONNECTED))) {
    Serial.println("[LOOP] Time to check limit configuration.");
    checkLimitConfiguration();
    lastLimitConfigCheckMillis = currentMillis;
  }

  // 1. Cek Koneksi WiFi & Reconnect jika perlu
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[LOOP] WiFi disconnected. Attempting reconnect...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Reconnect");  
    connectToWiFi(); 

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[LOOP] Reconnect failed. Skipping cycle.");
        delay(10000); 
        return; 
    }
  }

  // 2. Baca Sensor PZEM
  float voltage = pzem.voltage();
  float current = pzem.current();
  float power = pzem.power();
  float energy = pzem.energy(); 

  if (isnan(voltage) || isnan(current) || isnan(power) || isnan(energy)) {
    Serial.println("[PZEM] Failed to read from sensor!");
    
    // Jika ada error baca, set semua nilai yang akan dikirim ke server menjadi 0
    voltage = 0.0;
    current = 0.0;
    power = 0.0;
    energy = 0.0; // <-- TAMBAHKAN INI

    HTTPClient httpRelay;
  // Serial.print("[HTTP_RELAY] Getting status from: "); Serial.println(relayStatusUrl); // Kurangi log
  httpRelay.begin(relayStatusUrl);
  httpRelay.addHeader("Accept", "application/json");

  int httpRelayCode = httpRelay.GET();
  String relay1_status_str = "LOW";
  String relay2_status_str = "LOW";
  bool command_reset_pzem = false; // Variabel untuk menampung perintah reset

  if (httpRelayCode == HTTP_CODE_OK) {
    String payload = httpRelay.getString();
    // Serial.println("[HTTP_RELAY] Received: " + payload); // Kurangi log

    StaticJsonDocument<192> relayDoc; // Ukuran sedikit lebih besar untuk key baru
    DeserializationError error = deserializeJson(relayDoc, payload);

    if (!error) {
      relay1_status_str = relayDoc["relay1"] | "LOW";
      relay2_status_str = relayDoc["relay2"] | "LOW"; // Untuk lampu
      command_reset_pzem = relayDoc["reset_pzem_energy"] | false; // Ambil flag reset

      digitalWrite(RELAY1_PIN, relay1_status_str.equalsIgnoreCase("HIGH") ? HIGH : LOW); // Relay1 tetap dikontrol (misal, selalu LOW)
      digitalWrite(RELAY2_PIN, relay2_status_str.equalsIgnoreCase("HIGH") ? HIGH : LOW); // Kontrol lampu
      // Serial.printf("[HTTP_RELAY] R1: %s, R2(Lampu): %s, CMD_Reset: %s\n",
      //               relay1_status_str.c_str(), relay2_status_str.c_str(),
      //               command_reset_pzem ? "true" : "false"); // Kurangi log
    } else {
      Serial.print("[HTTP_RELAY] Failed to parse JSON: "); Serial.println(error.c_str());
    }
  } else {
    Serial.printf("[HTTP_RELAY] Failed. HTTP Code: %d Error: %s\n",
                  httpRelayCode, httpRelay.errorToString(httpRelayCode).c_str());
  }
  httpRelay.end();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PZEM Read Error");
    delay(2000); 

    
  }
//  Serial.printf("[PZEM] V:%.2fV I:%.3fA P:%.2fW E:%.3fkWh\n", voltage, current, power, energy);

  // --- Logika Pengecekan Batas Pemakaian ---
  if (limitFeatureActive && energyLimitKwh > 0.001) { 
    if (energy >= energyLimitKwh) {
      if (!limitExceededNotified) {
        // Serial.println("!!! [LIMIT] WARNING: ENERGY LIMIT EXCEEDED !!!");
        // lcdLine1Override = "!!LIMIT OVER!!";
        // lcdLine2Override = String(energy, 2) + ">" + String(energyLimitKwh, 2) + "kWh";
        // lcdOverrideEndMillis = currentMillis + lcdOverrideDurationMillis; 
        limitExceededNotified = true; 
      }
    } else {
      if (limitExceededNotified) {
        // Serial.println("[LIMIT] INFO: Energy usage back below limit.");
        // lcdLine1Override = "Batas Normal :)";
        // lcdLine2Override = String(energy, 2) + "<" + String(energyLimitKwh, 2) + "kWh";
        // lcdOverrideEndMillis = currentMillis + lcdOverrideDurationMillis;
        limitExceededNotified = false; 
      }
    }
  }



  // 3. Kirim Data ke Server Laravel
  HTTPClient httpPzem;
  Serial.print("[HTTP_PZEM] Sending data to: "); Serial.println(pzemStoreUrl);
  httpPzem.begin(pzemStoreUrl); 
  httpPzem.addHeader("Content-Type", "application/json");
  httpPzem.addHeader("Accept", "application/json"); 

  StaticJsonDocument<200> jsonDoc; 
  jsonDoc["tegangan"] = voltage;
  jsonDoc["arus"] = current;
  jsonDoc["daya"] = power;
  jsonDoc["energi"] = energy; 

  String jsonData;
  serializeJson(jsonDoc, jsonData);
//  Serial.print("[HTTP_PZEM] Payload: "); Serial.println(jsonData);

  int httpResponseCode = httpPzem.POST(jsonData);
  if (httpResponseCode > 0) {
//    Serial.printf("[HTTP_PZEM] Sent! HTTP Code: %d\n", httpResponseCode);
  } else {
    Serial.printf("[HTTP_PZEM] Failed. HTTP Error: %s\n", httpPzem.errorToString(httpResponseCode).c_str());
  }
  httpPzem.end(); 

//   // 4. Ambil Status Relay dari Server
//   HTTPClient httpRelay;
//   Serial.print("[HTTP_RELAY] Getting status from: "); Serial.println(relayStatusUrl);
//   httpRelay.begin(relayStatusUrl); 
//   httpRelay.addHeader("Accept", "application/json");

//   int httpRelayCode = httpRelay.GET();
//   String relay1_status_str = "LOW"; // Default
//   String relay2_status_str = "LOW"; // Default

//   if (httpRelayCode == HTTP_CODE_OK) { 
//     String payload = httpRelay.getString();
//     Serial.println("[HTTP_RELAY] Received: " + payload);

//     StaticJsonDocument<128> relayDoc; 
//     DeserializationError error = deserializeJson(relayDoc, payload);

//     if (!error) {
//       relay1_status_str = relayDoc["relay1"] | "LOW"; 
//       relay2_status_str = relayDoc["relay2"] | "LOW";

//       digitalWrite(RELAY1_PIN, relay1_status_str.equalsIgnoreCase("HIGH") ? HIGH : LOW);
//       digitalWrite(RELAY2_PIN, relay2_status_str.equalsIgnoreCase("HIGH") ? HIGH : LOW);
// //      Serial.printf("[HTTP_RELAY] R1: %s, R2: %s\n", relay1_status_str.c_str(), relay2_status_str.c_str());
//     } else {
//       Serial.print("[HTTP_RELAY] Failed to parse JSON: "); Serial.println(error.c_str());
//     }
//   } else {
//     Serial.printf("[HTTP_RELAY] Failed. HTTP Code: %d Error: %s\n", 
//                   httpRelayCode, httpRelay.errorToString(httpRelayCode).c_str());
//   }
//   httpRelay.end(); 

// 4. Ambil Status Relay (dan Perintah Reset) dari Server
  HTTPClient httpRelay;
  // Serial.print("[HTTP_RELAY] Getting status from: "); Serial.println(relayStatusUrl); // Kurangi log
  httpRelay.begin(relayStatusUrl);
  httpRelay.addHeader("Accept", "application/json");

  int httpRelayCode = httpRelay.GET();
  String relay1_status_str = "LOW";
  String relay2_status_str = "LOW";
  bool command_reset_pzem = false; // Variabel untuk menampung perintah reset

  if (httpRelayCode == HTTP_CODE_OK) {
    String payload = httpRelay.getString();
    // Serial.println("[HTTP_RELAY] Received: " + payload); // Kurangi log

    StaticJsonDocument<192> relayDoc; // Ukuran sedikit lebih besar untuk key baru
    DeserializationError error = deserializeJson(relayDoc, payload);

    if (!error) {
      relay1_status_str = relayDoc["relay1"] | "LOW";
      relay2_status_str = relayDoc["relay2"] | "LOW"; // Untuk lampu
      command_reset_pzem = relayDoc["reset_pzem_energy"] | false; // Ambil flag reset

      digitalWrite(RELAY1_PIN, relay1_status_str.equalsIgnoreCase("HIGH") ? HIGH : LOW); // Relay1 tetap dikontrol (misal, selalu LOW)
      digitalWrite(RELAY2_PIN, relay2_status_str.equalsIgnoreCase("HIGH") ? HIGH : LOW); // Kontrol lampu
      // Serial.printf("[HTTP_RELAY] R1: %s, R2(Lampu): %s, CMD_Reset: %s\n",
      //               relay1_status_str.c_str(), relay2_status_str.c_str(),
      //               command_reset_pzem ? "true" : "false"); // Kurangi log
    } else {
      Serial.print("[HTTP_RELAY] Failed to parse JSON: "); Serial.println(error.c_str());
    }
  } else {
    Serial.printf("[HTTP_RELAY] Failed. HTTP Code: %d Error: %s\n",
                  httpRelayCode, httpRelay.errorToString(httpRelayCode).c_str());
  }
  httpRelay.end();

  // --- EKSEKUSI PERINTAH RESET PZEM JIKA ADA ---
  if (command_reset_pzem) {
    Serial.println("[PZEM_CMD] Perintah reset energi diterima dari server. Mereset PZEM...");
    if (pzem.resetEnergy()) {
      Serial.println("[PZEM_CMD] Energi PZEM BERHASIL direset!");
      // Tampilkan di LCD
      lcdLine1Override = "PZEM RESET OK";
      lcdLine2Override = "Energi = 0";
      lcdOverrideEndMillis = millis() + 5000; // Tampilkan 5 detik
      // Reset juga flag notifikasi batas di ESP32
      limitExceededNotified = false;
    } else {
      Serial.println("[PZEM_CMD] GAGAL mereset energi PZEM.");
      lcdLine1Override = "PZEM RESET FAIL";
      lcdLine2Override = "";
      lcdOverrideEndMillis = millis() + 5000;
    }
    // Tidak perlu mengirim konfirmasi balik ke server karena server sudah otomatis reset flag-nya.
  }

  // 5. Update Tampilan LCD
  if (currentMillis < lcdOverrideEndMillis && lcdLine1Override.length() > 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(lcdLine1Override.substring(0,16)); // Pastikan tidak overflow LCD
    lcd.setCursor(0, 1);
    lcd.print(lcdLine2Override.substring(0,16)); // Pastikan tidak overflow LCD
  } else {
    lcdLine1Override = ""; 
    lcdLine2Override = "";
    float biaya = energy * 1444; // Tarif listrik per kWh
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("E:"); lcd.print(energy, 3); lcd.print("kWh"); 
    lcd.setCursor(0, 1);
    // Tampilkan Daya sebagai default
    lcd.print("Biaya: Rp"); lcd.print(biaya, 1); lcd.print(""); 
    // Alternatif: Tampilkan status relay
    // lcd.print("R1:"); lcd.print(relay1_status_str); lcd.print(" R2:"); lcd.print(relay2_status_str.substring(0,3));
  }

  // 6. Tunggu sebelum iterasi berikutnya
  delay(interval); 
}
