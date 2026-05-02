#include <Arduino.h>
#include "nrf_module.h"
#include "lora_module.h"

// Variabel untuk menyimpan waktu awal (saat upload/compile)
int startHr, startMin, startSec;

// ================= FUNGSI PARSING WAKTU LAPTOP =================
void setInitialTime() {
  // Makro __TIME__ mengambil jam laptop format "HH:MM:SS" saat dicompile
  sscanf(__TIME__, "%d:%d:%d", &startHr, &startMin, &startSec);
}

// ================= FUNGSI WAKTU WIB (OFF-LINE) =================
String getWIBTimestamp() {
  unsigned long totalSeconds = (millis() / 1000) + (startHr * 3600) + (startMin * 60) + startSec;
  
  int seconds = totalSeconds % 60;
  int minutes = (totalSeconds / 60) % 60;
  int hours = (totalSeconds / 3600) % 24;
  
  char buf[20];
  sprintf(buf, "%02d:%02d:%02d", hours, minutes, seconds);
  return String(buf);
}

void setup() {
  Serial.begin(115200);
  
  // Ambil waktu dari laptop saat compile
  setInitialTime();
  
  // Inisialisasi Modul NRF
  setupNRF();
  Serial.println("{\"status\":\"success\", \"message\":\"NRF Scanner Started\"}");

  // Inisialisasi Modul LoRa (Placeholder)
  setupLoRa();
}

void loop() {
  // 1. Scan NRF
  String spectrumData = scanNRF();

  // 2. Format JSON untuk NRF menjadi sebuah String Payload
  String jsonPayload = "{\n";
  jsonPayload += "  \"node\": \"Node 1\",\n";
  jsonPayload += "  \"timestamp_wib\": \"" + getWIBTimestamp() + "\",\n";
  jsonPayload += "  \"data_hex\": \"" + spectrumData + "\"\n";
  jsonPayload += "}";

  // Tampilkan payload ke Serial Monitor
  Serial.println(jsonPayload);

  // 3. Kirim data (sebagai TX) via LoRa
  sendLoRa(jsonPayload);

  delay(500); 
}