#include <Arduino.h>
#include "wifi_scanner.h" // Sertakan file header kita
#include <vector>

unsigned long lastScanTime = 0;
const unsigned long scanInterval = 15000; // 15 detik

void setup() {
  Serial.begin(115200);
  delay(1000); // Beri waktu untuk inisialisasi Serial

  Serial.println("\n=====================================");
  Serial.println("  ESP32 WiFi Analyzer Project");
  Serial.println("  Data akan dikirim dalam format JSON");
  Serial.println("=====================================\n");
}

void loop() {
  // Scan setiap interval
  if (millis() - lastScanTime >= scanInterval) {
    
    // Lakukan scan WiFi
    int networkCount = startWiFiScan();

    if (networkCount > 0) {
      // Ambil hasil scan
      std::vector<WiFiNetworkInfo> networks = getScanResults();

      // Tampilkan di Serial Monitor (format human readable)
      Serial.println("\n--- Daftar Jaringan WiFi Ditemukan ---");
      for (const auto& net : networks) {
        Serial.print("SSID: ");
        Serial.print(net.ssid);
        Serial.print(" | RSSI: ");
        Serial.print(net.rssi);
        Serial.print(" dBm");
        Serial.print(" | BSSID: ");
        Serial.print(net.bssid);
        Serial.print(" | Channel: ");
        Serial.print(net.channel);
        Serial.print(" | Security: ");
        Serial.println(net.security);
      }
      Serial.println("--------------------------------------\n");
      
      // KIRIM DATA DALAM FORMAT JSON VIA SERIAL
      Serial.println("Mengirim data JSON...");
      kirimJSONkeSerial();
      Serial.println(); // Spasi biar rapi
      
    } else {
      Serial.println("Tidak ada jaringan WiFi yang ditemukan di sekitar.");
    }
    
    lastScanTime = millis();
    Serial.println("Menunggu 15 detik sebelum scan berikutnya...");
  }
}