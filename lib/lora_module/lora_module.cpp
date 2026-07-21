#include "lora_module.h"
#include <LoRa.h>

#define LORA_SS    5
#define LORA_RST   14
#define LORA_DIO0  26

// ============================================================================
// KONFIGURASI RADIO — HARUS IDENTIK DENGAN GATEWAY!
//
// SF7 (sebelumnya SF9): airtime turun ~4x lipat.
//   - Balasan node (payload 182 byte, data_hex murni 125 karakter '0'/'1'):
//     ~923 ms -> ~292 ms
//   - Dengan RSSI terukur -35 s/d -47 dBm, margin link masih sangat besar.
//   - Bila node dipindah jauh dari gateway (RSSI < -100 dBm), naikkan
//     LORA_SF ke 8 atau 9 DI KEDUA SISI (node & gateway).
//
// TX power 10 dBm (sebelumnya default 17 dBm): mengurangi lonjakan arus
// saat transmit ~2x — mencegah brownout/restart pada catu daya USB lemah
// (penyebab umum node "timeout terus" di gateway). Gateway juga memakai 10.
// ============================================================================
#define LORA_SF       7
#define LORA_TX_DBM   10

static void configureLoRa() {
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSyncWord(0x34);
  LoRa.setTxPower(LORA_TX_DBM);
  // CRC aktif: paket korup langsung dibuang radio, tidak membuang satu
  // siklus polling gateway hanya untuk data rusak.
  LoRa.enableCrc();
}

void setupLoRa() {
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("{\"status\":\"error\", \"message\":\"LoRa failed\"}");
  } else {
    configureLoRa();
    Serial.println("{\"status\":\"success\", \"message\":\"LoRa Ready\"}");
  }
}

// Pemulihan mandiri: LoRa.begin() memicu reset hardware modul lewat pin RST
// (pin yang memang sudah terpasang — tanpa perubahan kabel), lalu seluruh
// konfigurasi radio ditulis ulang. Dipakai bila node lama tidak mendengar
// paket apa pun (radio macet, mis. efek glitch di bus SPI bersama NRF24).
void reinitLoRa() {
  LoRa.sleep();
  if (!LoRa.begin(433E6)) {
    Serial.println("{\"status\":\"error\", \"message\":\"LoRa reinit failed\"}");
  } else {
    configureLoRa();
    Serial.println("{\"status\":\"success\", \"message\":\"LoRa reinit OK\"}");
  }
}

void sendLoRa(String payload) {
  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();
  Serial.println("{\"status\":\"success\", \"message\":\"Data sent\"}");
}
