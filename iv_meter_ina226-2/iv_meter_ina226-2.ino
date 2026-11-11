/*
  iv_meter_ina226-2.ino
  IV meter panel surya: INA226 (I2C) + potensio sebagai beban variabel.
  ESP32 / ESP8266 mem-publish tegangan, arus, daya ke broker MQTT publik tiap 1 detik.
  GUI (iv_meter_gui-2.html) di GitHub Pages subscribe ke topik yang sama.

  Topik   : <MQTT_TOPIC>/latest   (retained)
  Payload : {"v":<volt>,"i":<ampere>,"p":<watt>,"t":<epoch detik UTC>}

  Wiring (lihat skematik):
    INA226 VCC -> 3V3, GND -> GND
    INA226 SDA/SCL -> SDA/SCL board (ESP32: GPIO21/22, ESP8266: D2/D1)
    Panel (+) -> VIN+ ; VIN- -> kaki tengah (W) potensio ; kaki B potensio -> (-) panel
    Pin VBUS dijumper ke VIN-, semua GND disatukan.

  Library: PubSubClient (Nick O'Leary) lewat Library Manager.
  Kredensial: salin secrets.example.h menjadi secrets.h lalu isi.
*/

#include <Wire.h>
#include <time.h>
#include <PubSubClient.h>
#include "secrets.h"

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #error "Pilih board ESP32 atau ESP8266"
#endif

// ---------------- konfigurasi ----------------
const uint8_t  INA_ADDR   = 0x40;    // A0 dan A1 ke GND
const float    SHUNT_OHM  = 0.1f;    // shunt pada modul INA226 (maks arus ~0,8 A)
const uint32_t READ_MS    = 250;
const uint32_t PUBLISH_MS = 1000;

const uint8_t REG_CONFIG = 0x00;
const uint8_t REG_SHUNT  = 0x01;
const uint8_t REG_BUS    = 0x02;
const uint8_t REG_MANUF  = 0xFE;

WiFiClient   net;
PubSubClient mqtt(net);
String       topicLatest;

float    vLast = 0, iLast = 0, pLast = 0;
bool     hasData = false;
uint32_t lastRead = 0, lastPub = 0, lastTry = 0;

// ---------------- INA226 ----------------
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

bool bacaSensor() {
  uint16_t rb, rs;
  if (!inaRead(REG_BUS, rb) || !inaRead(REG_SHUNT, rs)) return false;
  vLast = rb * 0.00125f;                       // 1,25 mV per LSB
  iLast = (int16_t)rs * 2.5e-6f / SHUNT_OHM;   // 2,5 uV per LSB
  if (iLast < 0) iLast = 0;
  pLast = vLast * iLast;
  hasData = true;
  return true;
}

// ---------------- jaringan ----------------
void connectMqtt() {
  if (mqtt.connected() || WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastTry < 5000 && lastTry != 0) return;
  lastTry = millis();
  String cid = "ivmeter-esp-" + String((uint32_t)random(0xFFFFFF), HEX);
  Serial.print("Menyambung MQTT... ");
  Serial.println(mqtt.connect(cid.c_str()) ? "ok" : "gagal");
}

void publishData() {
  if (!hasData || !mqtt.connected()) return;
  char buf[96];
  snprintf(buf, sizeof(buf), "{\"v\":%.4f,\"i\":%.5f,\"p\":%.6f,\"t\":%lu}",
           vLast, iLast, pLast, (unsigned long)time(nullptr));
  mqtt.publish(topicLatest.c_str(), buf, true);   // retained
}

// ---------------- setup & loop ----------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Wire.begin();

  Serial.println(inaBegin() ? "INA226 siap."
                            : "INA226 tidak terdeteksi. Cek SDA/SCL dan alamat 0x40.");

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Menyambung WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nIP: ");
  Serial.println(WiFi.localIP());

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");   // timestamp UTC

  topicLatest = String(MQTT_TOPIC) + "/latest";
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  Serial.print("Topik: ");
  Serial.println(topicLatest);
}

void loop() {
  connectMqtt();
  mqtt.loop();

  if (millis() - lastRead >= READ_MS) {
    lastRead = millis();
    bacaSensor();
  }
  if (millis() - lastPub >= PUBLISH_MS) {
    lastPub = millis();
    publishData();
    if (hasData) Serial.printf("V=%.3f V  I=%.5f A  P=%.5f W\n", vLast, iLast, pLast);
  }
}
