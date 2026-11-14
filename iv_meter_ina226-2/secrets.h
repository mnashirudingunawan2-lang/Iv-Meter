// Salin file ini menjadi secrets.h lalu isi. secrets.h masuk .gitignore.
#pragma once

#define WIFI_SSID  "100MB"
#define WIFI_PASS  "8karakter"

#define MQTT_HOST  "broker.hivemq.com"
#define MQTT_PORT  1883
// Awalan topik: buat panjang dan acak, jangan ditulis di repo GitHub.
// Isi yang sama dimasukkan di GUI lewat ?t=
#define MQTT_TOPIC "ivmeter-eb9j2sjlv86n75"
