#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

const char* WIFI_SSID = "NAMA_WIFI";
const char* WIFI_PASS = "PASSWORD_WIFI";

// Alamat server setelah di-deploy (https). Untuk tes lokal: "http://192.168.x.x:5000/api/data"
const char* SERVER_URL = "https://NAMA-APP-KAMU.onrender.com/api/data";
const char* API_KEY    = "ganti-kunci-rahasia";   // harus sama dengan API_KEY di server

// ====== GANTI dengan pembacaan sensor aslimu (ADC / INA219 / dll) ======
float readVoltage() { return 0.0; }   // Volt
float readCurrent() { return 0.0; }   // Ampere
// ========================================================================

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi OK: " + WiFi.localIP().toString());
}

void setup() {
  Serial.begin(115200);
  connectWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();

  String body = "{\"voltage\":" + String(readVoltage(), 4) +
                ",\"current\":" + String(readCurrent(), 5) + "}";

  WiFiClientSecure client;
  client.setInsecure();               // abaikan verifikasi sertifikat (cukup untuk proyek ini)
  HTTPClient http;
  if (http.begin(client, SERVER_URL)) {
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-Key", API_KEY);
    int code = http.POST(body);
    Serial.printf("POST %s -> %d\n", body.c_str(), code);
    http.end();
  }
  delay(1000);                        // kirim tiap 1 detik
}
