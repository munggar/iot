/*
  =========================================
  Smart Monitoring System - ESP32 + DHT11
  Kirim data ke Render.com via HTTPS POST
  =========================================
  Wiring:
    DHT11 VCC  --> 3.3V ESP32
    DHT11 GND  --> GND ESP32
    DHT11 DATA --> GPIO4 ESP32

  Library Arduino IDE yang dibutuhkan:
    1. DHT sensor library  (by Adafruit)
    2. Adafruit Unified Sensor (by Adafruit) -- dependensi DHT
    3. ArduinoJson          (by Benoit Blanchon)
    ESP32 board package sudah include WiFi, HTTPClient, WiFiClientSecure
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>   // WAJIB untuk HTTPS Render
#include <DHT.h>
#include <ArduinoJson.h>

// ===================================================
//   KONFIGURASI — EDIT BAGIAN INI SEBELUM UPLOAD
// ===================================================

// WiFi
const char* WIFI_SSID     = "NAMA_WIFI_KAMU";    // ← Ganti
const char* WIFI_PASSWORD = "PASSWORD_WIFI";      // ← Ganti

// URL Render — ganti setelah deploy berhasil
// Format: https://NAMA-PROJECT.onrender.com/api/data
const char* SERVER_URL = "https://NAMA-PROJECT.onrender.com/api/data";

// ID perangkat (boleh diganti)
const String DEVICE_ID = "ESP32-DHT11-001";

// Threshold Alert
const float TEMP_HIGH = 35.0;   // °C batas atas
const float TEMP_LOW  = 15.0;   // °C batas bawah
const float HUM_HIGH  = 80.0;   // % batas atas
const float HUM_LOW   = 20.0;   // % batas bawah

// Interval kirim data (ms). 15000 = 15 detik
// Jangan terlalu cepat agar tidak membebani Render free tier
const unsigned long SEND_INTERVAL = 15000;

// ===================================================

#define DHTPIN  4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define LED_STATUS 2

unsigned long lastSendTime = 0;
int failCount = 0;

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(LED_STATUS, OUTPUT);
  digitalWrite(LED_STATUS, LOW);

  Serial.println("\n============================================");
  Serial.println("  Smart Monitoring System - Render Mode   ");
  Serial.println("============================================");
  Serial.println("[Target] " + String(SERVER_URL));

  dht.begin();
  Serial.println("[DHT11] Sensor aktif di GPIO " + String(DHTPIN));

  connectWiFi();
}

// ===== LOOP UTAMA =====
void loop() {
  // Reconnect WiFi jika terputus
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Koneksi terputus, mencoba reconnect...");
    digitalWrite(LED_STATUS, LOW);
    connectWiFi();
    return;
  }

  unsigned long now = millis();
  if (now - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = now;

    // Baca sensor dengan retry
    float temperature = NAN, humidity = NAN;
    for (int attempt = 0; attempt < 3 && (isnan(temperature) || isnan(humidity)); attempt++) {
      temperature = dht.readTemperature();
      humidity    = dht.readHumidity();
      if (isnan(temperature) || isnan(humidity)) delay(2000);
    }

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("[DHT11] ERROR: Gagal baca sensor setelah 3 percobaan!");
      Serial.println("[DHT11] Cek: wiring, pull-up resistor 10kΩ, coba GPIO lain");
      blinkLED(3, 200);
      return;
    }

    float heatIndex = dht.computeHeatIndex(temperature, humidity, false);
    String status   = determineStatus(temperature, humidity);
    String alert    = determineAlert(temperature, humidity);

    // Tampilkan di Serial Monitor
    Serial.println("\n--- Pembacaan Sensor ---");
    Serial.printf("Suhu       : %.1f C\n",  temperature);
    Serial.printf("Kelembaban : %.1f %%\n", humidity);
    Serial.printf("Heat Index : %.1f C\n",  heatIndex);
    Serial.printf("Status     : %s\n",      status.c_str());
    Serial.printf("Uptime     : %lu detik\n", millis() / 1000);
    if (alert != "none") {
      Serial.println("*** ALERT: " + alert + " ***");
    }
    Serial.println("------------------------");

    // Kirim ke Render
    sendDataToServer(temperature, humidity, heatIndex, status, alert);
  }
}

// ===== KONEKSI WiFi =====
void connectWiFi() {
  Serial.print("[WiFi] Menghubungkan ke: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts % 20 == 0) {
      Serial.println("\n[WiFi] Mencoba ulang...");
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_STATUS, HIGH);
    Serial.println("\n[WiFi] TERHUBUNG!");
    Serial.println("[WiFi] IP     : " + WiFi.localIP().toString());
    Serial.printf("[WiFi] RSSI   : %d dBm\n", WiFi.RSSI());
    failCount = 0;
  } else {
    Serial.println("\n[WiFi] GAGAL terhubung.");
    Serial.println("[WiFi] Pastikan SSID dan password benar.");
    Serial.println("[WiFi] Pastikan ESP32 dalam jangkauan WiFi.");
    blinkLED(5, 300);
    delay(5000); // Tunggu 5 detik sebelum retry
  }
}

// ===== KIRIM DATA KE RENDER (HTTPS) =====
void sendDataToServer(float temp, float hum, float heatIdx,
                      String status, String alert) {
  if (WiFi.status() != WL_CONNECTED) return;

  // WiFiClientSecure untuk HTTPS
  WiFiClientSecure client;
  client.setInsecure(); // Skip verifikasi SSL cert — OK untuk tugas/demo
                        // Untuk produksi: gunakan client.setCACert(cert)

  HTTPClient http;
  if (!http.begin(client, SERVER_URL)) {
    Serial.println("[HTTPS] Gagal inisialisasi koneksi");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000); // 15 detik — Render kadang butuh waktu bangun dari sleep

  // JSON payload
  StaticJsonDocument<300> doc;
  doc["device_id"]   = DEVICE_ID;
  doc["temperature"] = round(temp    * 10) / 10.0;
  doc["humidity"]    = round(hum     * 10) / 10.0;
  doc["heat_index"]  = round(heatIdx * 10) / 10.0;
  doc["status"]      = status;
  doc["alert"]       = alert;
  doc["wifi_rssi"]   = WiFi.RSSI();
  doc["uptime"]      = (unsigned long)(millis() / 1000);

  String jsonBody;
  serializeJson(doc, jsonBody);
  Serial.println("[Payload] " + jsonBody);

  Serial.print("[HTTPS] Mengirim ke Render... ");
  int httpCode = http.POST(jsonBody);

  if (httpCode > 0) {
    if (httpCode == 200 || httpCode == 201) {
      Serial.printf("SUKSES (HTTP %d)\n", httpCode);
      Serial.println("[Response] " + http.getString());
      blinkLED(1, 150);
      failCount = 0;
    } else {
      Serial.printf("SERVER ERROR: HTTP %d\n", httpCode);
      Serial.println("[Response] " + http.getString());
      failCount++;
    }
  } else {
    // httpCode negatif = koneksi gagal sebelum sampai server
    Serial.printf("KONEKSI GAGAL: %s\n", http.errorToString(httpCode).c_str());
    Serial.println("[Hint] Kemungkinan server Render sedang sleep (~30 detik)");
    Serial.println("[Hint] Coba lagi sebentar, atau cek URL di Serial");
    failCount++;
    blinkLED(2, 300);
  }

  // Jika gagal 5x berturut-turut, coba reconnect WiFi
  if (failCount >= 5) {
    Serial.println("[WiFi] Terlalu banyak gagal, reconnect WiFi...");
    WiFi.disconnect();
    delay(2000);
    connectWiFi();
    failCount = 0;
  }

  http.end();
}

// ===== TENTUKAN STATUS =====
String determineStatus(float temp, float hum) {
  if (temp >= TEMP_HIGH || temp <= TEMP_LOW) return "warning";
  if (hum  >= HUM_HIGH  || hum  <= HUM_LOW)  return "warning";
  if (temp > 30.0 || hum > 70.0)             return "caution";
  return "normal";
}

// ===== TENTUKAN ALERT =====
String determineAlert(float temp, float hum) {
  if (temp >= TEMP_HIGH) return "Suhu terlalu tinggi: " + String(temp, 1) + "C";
  if (temp <= TEMP_LOW)  return "Suhu terlalu rendah: " + String(temp, 1) + "C";
  if (hum  >= HUM_HIGH)  return "Kelembaban terlalu tinggi: " + String(hum, 1) + "%";
  if (hum  <= HUM_LOW)   return "Kelembaban terlalu rendah: " + String(hum, 1) + "%";
  return "none";
}

// ===== BLINK LED =====
void blinkLED(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_STATUS, LOW);
    delay(delayMs);
    digitalWrite(LED_STATUS, HIGH);
    delay(delayMs);
  }
}
