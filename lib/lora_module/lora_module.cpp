#include "lora_module.h"
#include <LoRa.h>

// --- PIN LORA ---
#define LORA_SS    5
#define LORA_RST   14
#define LORA_DIO0  26

void setupLoRa() {
  // Set pin untuk modul LoRa
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  // Inisialisasi LoRa pada frekuensi 433MHz
  if (!LoRa.begin(433E6)) {
    Serial.println("{\"status\":\"error\", \"message\":\"Starting LoRa failed! Cek koneksi pin.\"}");
  } else {
    // Konfigurasi harus sama dengan Node 1
    LoRa.setSpreadingFactor(12);
    LoRa.setSyncWord(0x34);
    
    Serial.println("{\"status\":\"success\", \"message\":\"LoRa Module Initialized (TX Mode)\"}");
  }
}

void sendLoRa(String payload) {
  // Mulai membuat paket data
  LoRa.beginPacket();
  // Memasukkan payload ke dalam paket
  LoRa.print(payload);
  // Mengirim paket
  LoRa.endPacket();
  
  Serial.println("{\"status\":\"success\", \"message\":\"Data sent via LoRa TX\"}");
}
