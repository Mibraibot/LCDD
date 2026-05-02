#include "wifi_scanner.h"
#include <vector>
#include <string>
#include <ArduinoJson.h>
#include <WiFi.h>  // TAMBAHKAN INI! Library WiFi untuk ESP32

// Global variable untuk menyimpan hasil scan
static std::vector<WiFiNetworkInfo> scanResults;

// Fungsi untuk memulai proses scan
int startWiFiScan() {
    scanResults.clear(); // Bersihkan hasil sebelumnya

    Serial.println("Memulai scan WiFi...");
    // Inisialisasi WiFi ke mode STA (Station)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(); // Putuskan koneksi jika ada sebelumnya
    delay(100);

    int n = WiFi.scanNetworks(); // Lakukan scan

    Serial.print("Scan selesai. Ditemukan ");
    Serial.print(n);
    Serial.println(" jaringan.");

    if (n > 0) {
        for (int i = 0; i < n; ++i) {
            WiFiNetworkInfo network;
            network.ssid = WiFi.SSID(i);
            network.rssi = WiFi.RSSI(i);
            network.channel = WiFi.channel(i);

            // Format BSSID menjadi string
            byte bssid_bytes[6];
            memcpy(bssid_bytes, WiFi.BSSID(i), 6);
            char bssid_str[18]; // 17 chars + null terminator
            snprintf(bssid_str, sizeof(bssid_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                     bssid_bytes[0], bssid_bytes[1], bssid_bytes[2],
                     bssid_bytes[3], bssid_bytes[4], bssid_bytes[5]);
            network.bssid = String(bssid_str);

            // Tentukan jenis keamanan
            switch (WiFi.encryptionType(i)) {
                case WIFI_AUTH_OPEN:
                    network.security = "Open";
                    break;
                case WIFI_AUTH_WEP:
                    network.security = "WEP";
                    break;
                case WIFI_AUTH_WPA_PSK:
                    network.security = "WPA-PSK";
                    break;
                case WIFI_AUTH_WPA2_PSK:
                    network.security = "WPA2-PSK";
                    break;
                case WIFI_AUTH_WPA_WPA2_PSK:
                    network.security = "WPA/WPA2-PSK";
                    break;
                case WIFI_AUTH_WPA2_ENTERPRISE:
                    network.security = "WPA2-Enterprise";
                    break;
                case WIFI_AUTH_MAX: // Terkadang ini digunakan untuk WPA3
                    network.security = "WPA3-Personal";
                    break;
                default:
                    network.security = "Unknown";
                    break;
            }
            scanResults.push_back(network);
        }
    }
    return n;
}

// Fungsi untuk mendapatkan daftar jaringan WiFi yang ditemukan
std::vector<WiFiNetworkInfo> getScanResults() {
    return scanResults;
}

// FUNGSI BARU: Kirim hasil scan sebagai JSON via Serial
void kirimJSONkeSerial() {
    // Buat dokumen JSON dengan kapasitas cukup
    StaticJsonDocument<4096> doc;
    
    // Tambah metadata
    doc["timestamp"] = millis();
    doc["total_networks"] = scanResults.size();
    
    // Buat array untuk networks
    JsonArray networks = doc.createNestedArray("networks");
    
    // Loop setiap hasil scan
    for (const auto& wifi : scanResults) {
        JsonObject obj = networks.createNestedObject();
        obj["ssid"] = wifi.ssid;
        obj["rssi"] = wifi.rssi;
        obj["channel"] = wifi.channel;
        obj["bssid"] = wifi.bssid;
        obj["security"] = wifi.security;
    }
    
    // Kirim JSON ke Serial
    serializeJson(doc, Serial);
    Serial.println(); // Tambah newline sebagai penanda akhir data
    
    Serial.println("Data JSON telah dikirim via Serial");
}