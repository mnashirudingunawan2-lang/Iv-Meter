/*
  iv_meter_esp12e-2.ino
  ESP-12E (ESP8266) + INA226: kirim tegangan, arus, daya ke broker MQTT tiap 1 detik.
  Web di GitHub Pages (index.html / iv_meter_gui-2.html) membaca topik yang sama.

  Board di Arduino IDE: "NodeMCU 1.0 (ESP-12E Module)" atau "Generic ESP8266 Module".
  Library: PubSubClient (Nick O'Leary), pasang lewat Library Manager.

  Wiring INA226 (modul diberi daya 3V3, BUKAN 5V):
    VCC -> 3V3, GND -> GND
    SDA -> GPIO4 (D2), SCL -> GPIO5 (D1)
    Panel (+) -> VIN+ ; VIN- -> kaki tengah (W) potensio ; kaki B potensio -> (-) panel
    Pin VBUS dijumper ke VIN-, semua GND disatukan.

  Buka web dengan:  https://www.ivmeter-mng.com/?t=<MQTT_TOPIC>
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <time.h>

// ====== ISI BAGIAN INI ======
const char* WIFI_SSID  = "100MB";            // harus WiFi 2,4 GHz
const char* WIFI_PASS  = "8karakter";

const char* MQTT_HOST  = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;
// Topik: harus SAMA dengan yang dimasukkan di web (?t=...). Buat panjang dan acak.
const char* MQTT_TOPIC = "ivmeter-eb9j2sjlv86n75";
// ============================

// ---------------- INA226 ----------------
const uint8_t  SDA_PIN   = 4;       // D2
const uint8_t  SCL_PIN   = 5;       // D1
const uint8_t  INA_ADDR  = 0x40;    // A0 dan A1 ke GND
const float    SHUNT_OHM = 0.1f;    // shunt pada modul INA226 (maks arus ~0,8 A)

const uint8_t REG_CONFIG = 0x00;
const uint8_t REG_SHUNT  = 0x01;
const uint8_t REG_BUS    = 0x02;
const uint8_t REG_MANUF  = 0xFE;

bool inaWrite(uint8_t reg, uint16_t val) {
  Wire.beginTransmission(INA_ADDR);
  Wire.write(reg);
  Wire.write(val >> 8);
  Wire.write(val & 0xFF);
  return Wire.endTransmission() == 0;
}

bool inaRead(uint8_t reg, uint16_t &val) {
  Wire.beginTransmission(INA_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(INA_ADDR, (uint8_t)2) != 2) return false;
  val = (uint16_t)Wire.read() << 8;
  val |= Wire.read();
  return true;
}

bool inaBegin() {
  uint16_t id;
  if (!inaRead(REG_MANUF, id) || id != 0x5449) return false;
  // rata-rata 16 sampel, konversi 1,1 ms, mode kontinu shunt + bus
  return inaWrite(REG_CONFIG, 0x4527);
}

bool bacaSensor(float &v, float &i) {
  uint16_t rb, rs;
  if (!inaRead(REG_BUS, rb) || !inaRead(REG_SHUNT, rs)) return false;
  v = rb * 0.00125f;                           // 1,25 mV per LSB
  i = (int16_t)rs * 2.5e-6f / SHUNT_OHM;       // 2,5 uV per LSB
  if (i < 0) i = 0;
  return true;
}

// ---------------- WiFi & MQTT ----------------
WiFiClient   net;
PubSubClient mqtt(net);
String       topicLatest;
uint32_t     lastPub = 0, lastTry = 0;

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Menyambung WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi OK: " + WiFi.localIP().toString());
}

void connectMqtt() {
  if (mqtt.connected()) return;
  if (lastTry != 0 && millis() - lastTry < 5000) return;
  lastTry = millis();
  String cid = "ivmeter-esp-" + String(ESP.getChipId(), HEX);
  Serial.print("Menyambung MQTT... ");
  Serial.println(mqtt.connect(cid.c_str()) ? "ok" : "gagal");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println(inaBegin() ? "\nINA226 siap."
                            : "\nINA226 tidak terdeteksi. Cek SDA/SCL dan alamat 0x40.");

  WiFi.setAutoReconnect(true);
  connectWiFi();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");   // timestamp UTC

  topicLatest = String(MQTT_TOPIC) + "/latest";
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  Serial.println("Topik: " + topicLatest);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  connectMqtt();
  mqtt.loop();

  if (millis() - lastPub >= 1000) {
    lastPub = millis();
    float v, i;
    if (bacaSensor(v, i)) {
      char buf[96];
      snprintf(buf, sizeof(buf), "{\"v\":%.4f,\"i\":%.5f,\"p\":%.6f,\"t\":%lu}",
               v, i, v * i, (unsigned long)time(nullptr));
      bool ok = mqtt.connected() && mqtt.publish(topicLatest.c_str(), buf, true);  // retained
      Serial.printf("%s -> %s\n", buf, ok ? "terkirim" : "GAGAL");
    } else {
      Serial.println("Gagal membaca INA226");
    }
  }
}
