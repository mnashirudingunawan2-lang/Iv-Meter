// Salin file ini menjadi secrets.h lalu isi. secrets.h masuk .gitignore.
#pragma once

#define WIFI_SSID  "nama_wifi"
#define WIFI_PASS  "password_wifi"

#define MQTT_HOST  "broker.hivemq.com"
#define MQTT_PORT  1883
// Awalan topik: buat panjang dan acak, jangan ditulis di repo GitHub.
// Isi yang sama dimasukkan di GUI lewat ?t=
#define MQTT_TOPIC "ivmeter-GANTI-DENGAN-KODE-ACAK"
