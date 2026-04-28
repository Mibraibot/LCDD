#ifndef CONFIG_JAMMER_WIFI_H
#define CONFIG_JAMMER_WIFI_H

// Definisikan pin NRF24L01 yang diinginkan
// Anda menyebutkan CE_PIN 4 dan CSN_PIN 5, namun dalam teks yang diberikan,
// pin untuk NRF24L01 adalah:
// NRF_CE_PIN_A    5
// NRF_CSN_PIN_A   17
// NRF_CE_PIN_B    16
// NRF_CSN_PIN_B   4
// NRF_CE_PIN_C    15
// NRF_CSN_PIN_C   2
//
// Untuk mencontohkan dengan pin yang Anda minta, kita akan menggunakan CE_PIN 4 dan CSN_PIN 5
// Namun, perlu diperhatikan bahwa ini mungkin tidak sesuai dengan konfigurasi perangkat keras Anda.
// Jika Anda menggunakan modul NRF24L01 terpisah, Anda mungkin perlu mendefinisikan pin ini secara terpisah
// atau memodifikasi struktur yang ada.

// Contoh penggunaan pin yang diminta (sesuaikan jika perlu berdasarkan hardware Anda)
#define NRF_JAMMER_CE_PIN   4
#define NRF_JAMMER_CSN_PIN  5

// Definisikan dimensi layar (dari teks)
#define SCREEN_W 128
#define SCREEN_H 64

// Definisi lain yang mungkin diperlukan, seperti font, dll.
// #include "setting.h" // Asumsi setting.h ada
// #include <Arduino.h>
// #include <U8g2lib.h>
// #include <SPI.h>
// #include <RF24.h>
// #include <vector>
// #include <string>
// #include <math.h>

// Anda perlu mendeklarasikan objek radio yang akan digunakan, contoh:
// extern RF24 radio; // Jika menggunakan satu radio
// Atau jika menggunakan multi-radio seperti dalam contoh:
// extern RF24 RadioA;
// extern RF24 RadioB;
// extern RF24 RadioC;

// Deklarasi fungsi jamming
void start_wifi_jamming();
void stop_wifi_jamming();
bool is_wifi_jamming_active();

#endif // CONFIG_JAMMER_WIFI_H
