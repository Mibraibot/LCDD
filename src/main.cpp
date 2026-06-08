#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#include "lora_module.h"
#include "nrf_module.h"
#include <LoRa.h>

// ============================================================================
// KONFIGURASI OLED & TOMBOL
// ============================================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define BUTTON_PIN 15

// ============================================================================
// GLOBAL VARIABLE LORA SYNC & IDENTITAS
// ============================================================================
unsigned long baseTime = 0;
unsigned long localMillisAtSync = 0;
bool synced = false;

// --- KONFIGURASI IDENTITAS NODE ---
const String NODE_ID =
    "Node3"; // UBAH INI JADI "Node2" ATAU "Node3" UNTUK NODE LAIN

unsigned long getSendDelay() {
  if (NODE_ID == "Node2")
    return 500; // Jeda 0.5 detik
  if (NODE_ID == "Node3")
    return 1000; // Jeda 1 detik
  return 0;      // Node 1 tanpa jeda
}

// ============================================================================
// STATE & MODE APP
// ============================================================================
enum AppMode { MODE_STANDBY, MODE_DRONE_DETECT, MODE_SCAN_WIFI, MODE_INFO };
AppMode currentMode = MODE_STANDBY;

// Variabel Debouncing
bool buttonState = HIGH;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Variabel Double Click
bool waitingForDoubleClick = false;
unsigned long clickTimer = 0;
unsigned long infoStateTimer = 0; // Timer untuk auto-exit info

// Variabel Animasi Universal
int animFrame = 0;
unsigned long animTimer = 0;

// Variabel Siklus Drone Detect
bool isMyTurn = false;
enum DroneState { STATE_SCANNING, STATE_SHOW_DATA, STATE_SENDING };
DroneState currentDroneState = STATE_SCANNING;
unsigned long droneStateTimer = 0;
bool hasSentThisCycle = false;
String currentSpectrumData = "";
int currentThreatLevel = 1;

// Variabel Siklus Scan Wifi
enum WifiState {
  WIFI_STATE_SCANNING,
  WIFI_STATE_SHOW_AP1,
  WIFI_STATE_SHOW_AP2,
  WIFI_STATE_SHOW_AP3
};
WifiState currentWifiState = WIFI_STATE_SCANNING;
unsigned long wifiStateTimer = 0;

// ============================================================================
// GET CURRENT TIME FROM SYNC
// ============================================================================
String getWIBTimestamp() {
  if (!synced)
    return "00:00:00";
  unsigned long currentEpoch =
      baseTime + ((millis() - localMillisAtSync) / 1000);
  int seconds = currentEpoch % 60;
  int minutes = (currentEpoch / 60) % 60;
  int hours = (currentEpoch / 3600) % 24;
  char buf[20];
  sprintf(buf, "%02d:%02d:%02d", hours, minutes, seconds);
  return String(buf);
}

// ============================================================================
// FUNGSI KALKULASI THREAT (Decision Tree Ringan)
// ============================================================================
int calculateThreatLevel(String data_hex) {
  int total_energy = 0;
  int high_peaks = 0;

  for (int i = 0; i < data_hex.length(); i++) {
    int val = 0;
    char c = data_hex[i];
    if (c >= '0' && c <= '9')
      val = c - '0';
    else if (c >= 'a' && c <= 'f')
      val = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
      val = c - 'A' + 10;

    total_energy += val;
    if (val >= 4)
      high_peaks++;
  }

  if (total_energy >= 30 || high_peaks >= 2)
    return 3; // CRITICAL (DRONE)
  else if (total_energy >= 15)
    return 2; // WARNING (BLUETOOTH)
  else
    return 1; // SAFE (WIFI)
}

// ============================================================================
// FUNGSI RENDER OLED
// ============================================================================
void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  if (currentMode == MODE_STANDBY) {
    display.setTextSize(1);
    display.println(F("Hello,"));
    display.setTextSize(2);
    display.println(F("LCDD"));
    display.setTextSize(1);
    display.println(F("(LOW COST DRONE"));
    display.println(F(" DETECTION)"));
    display.setCursor(0, 50);
    display.println(F("Pilih mode (klik)"));
  } else if (currentMode == MODE_DRONE_DETECT) {
    if (currentDroneState == STATE_SCANNING) {
      display.setTextSize(2);
      display.setCursor(5, 10);
      display.print(F("WAITING"));
      display.setTextSize(1);
      display.setCursor(5, 30);
      display.print(F("Gateway Poll"));
      display.setTextSize(3);
      display.setCursor(85, 30);
      if (animFrame == 0)
        display.print(F("-"));
      else if (animFrame == 1)
        display.print(F("\\"));
      else if (animFrame == 2)
        display.print(F("|"));
      else if (animFrame == 3)
        display.print(F("/"));
    } else if (currentDroneState == STATE_SHOW_DATA) {
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print(F("Node: "));
      display.println(NODE_ID);
      display.setCursor(0, 20);
      display.println(F("Threat Status:"));
      display.setCursor(0, 35);
      display.setTextSize(2);
      if (currentThreatLevel == 1)
        display.println(F("SAFE"));
      else if (currentThreatLevel == 2)
        display.println(F("WARNING"));
      else if (currentThreatLevel == 3)
        display.println(F("CRITICAL"));
      else
        display.println(F("UNKNOWN"));
    } else if (currentDroneState == STATE_SENDING) {
      display.setTextSize(1);
      display.setCursor(10, 20);
      display.println(F("Sending To"));
      display.setCursor(10, 35);
      display.println(F("Gateway..."));
    }
  } else if (currentMode == MODE_SCAN_WIFI) {
    if (currentWifiState == WIFI_STATE_SCANNING) {
      display.setTextSize(2);
      display.setCursor(10, 10);
      display.print(F("WIFI SCAN"));
      display.setCursor(25, 40);
      for (int i = 0; i <= animFrame; i++) {
        display.print(F("| "));
      }
    } else if (currentWifiState == WIFI_STATE_SHOW_AP1) {
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println(F("Found WiFi (1/3)"));
      display.println(F("----------------"));
      display.setTextSize(2);
      display.println(F("RASA PAWON"));
      display.setTextSize(1);
      display.println(F("RSSI : -45 dBm\nCH   : 4"));
    } else if (currentWifiState == WIFI_STATE_SHOW_AP2) {
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println(F("Found WiFi (2/3)"));
      display.println(F("----------------"));
      display.setTextSize(2);
      display.println(F("Kos_Putra"));
      display.setTextSize(1);
      display.println(F("RSSI : -68 dBm\nCH   : 1"));
    } else if (currentWifiState == WIFI_STATE_SHOW_AP3) {
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println(F("Found WiFi (3/3)"));
      display.println(F("----------------"));
      display.setTextSize(2);
      display.println(F("Warkop_Free"));
      display.setTextSize(1);
      display.println(F("RSSI : -80 dBm\nCH   : 11"));
    }
  } else if (currentMode == MODE_INFO) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("--- NODE INFO ---"));
    display.setCursor(0, 15);
    display.print(F("Nama: "));
    display.println(NODE_ID);
    display.setCursor(0, 30);
    display.println(F("Lokasi (Lat, Lng):"));
    display.setCursor(0, 45);
    display.println(F("-6.200000"));
    display.setCursor(0, 55);
    display.println(F("106.816666"));
  }
  display.display();
}

// ============================================================================
// LOGIKA SINKRONISASI AWAL (BLOCKING SESAAT)
// ============================================================================
void waitForSync() {
  Serial.println("====================================");
  Serial.println("Menunggu Sync Timestamp Gateway...");

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println(F("Waiting Sync"));
  display.setCursor(10, 35);
  display.println(F("From Gateway..."));
  display.display();

  int dotCount = 0;
  unsigned long lastDotTime = millis();

  while (!synced) {
    // Efek loading di OLED
    if (millis() - lastDotTime > 500) {
      lastDotTime = millis();
      display.fillRect(10, 45, 100, 10, SSD1306_BLACK); // Hapus titik lama
      display.setCursor(10, 45);
      for (int i = 0; i <= dotCount; i++)
        display.print(".");
      display.display();
      dotCount = (dotCount + 1) % 5;
    }

    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String incoming = "";
      while (LoRa.available()) {
        incoming += (char)LoRa.read();
      }
      Serial.println("Data diterima: " + incoming);

      if (incoming.indexOf("\"type\":\"sync\"") >= 0) {
        // PERHATIAN: Diubah menjadi "time" menyesuaikan dengan Gateway
        int idx = incoming.indexOf("\"time\":");
        if (idx >= 0) {
          String ts = incoming.substring(idx + 7, incoming.indexOf("}", idx));
          baseTime = ts.toInt();
          localMillisAtSync = millis();
          synced = true;

          Serial.println("SYNC BERHASIL | Base Timestamp: " + String(baseTime));

          display.clearDisplay();
          display.setCursor(10, 25);
          display.println(F("SYNC SUCCESS!"));
          display.display();
          delay(1000);
        }
      }
    }
    delay(10);
  }
}

// ============================================================================
// LOGIKA SIKLUS DRONE DETECT (NON-BLOCKING)
// ============================================================================
void handleDroneDetectLogic() {
  if (currentMode != MODE_DRONE_DETECT)
    return;

  unsigned long currentMillis = millis();
  bool needUpdate = false;

  if (currentDroneState == STATE_SCANNING) {
    if (!isMyTurn) {
      // Idle waiting for poll
      if (currentMillis - animTimer > 500) {
        animFrame = (animFrame + 1) % 4;
        animTimer = currentMillis;
        needUpdate = true;
      }
    } else {
      // It is my turn! Scan immediately
      currentSpectrumData = scanNRF();
      currentThreatLevel = calculateThreatLevel(currentSpectrumData);

      currentDroneState = STATE_SHOW_DATA;
      droneStateTimer = currentMillis;
      needUpdate = true;
    }
  } else if (currentDroneState == STATE_SHOW_DATA) {
    if (currentMillis - droneStateTimer > 1000) { // Tahan layar 1 Detik
      currentDroneState = STATE_SENDING;
      droneStateTimer = currentMillis;
      hasSentThisCycle = false;
      needUpdate = true;
    }
  } else if (currentDroneState == STATE_SENDING) {
    if (!hasSentThisCycle) {
      String jsonPayload = "{\n";
      jsonPayload += "  \"node\": \"" + NODE_ID + "\",\n";
      jsonPayload += "  \"timestamp_wib\": \"" + getWIBTimestamp() + "\",\n";
      jsonPayload += "  \"data_hex\": \"" + currentSpectrumData + "\"\n";
      jsonPayload += "}";

      Serial.println("Mengirim via LoRa:");
      Serial.println(jsonPayload);
      sendLoRa(jsonPayload);
      hasSentThisCycle = true;
      isMyTurn = false; // Selesai giliran
    }

    if (currentMillis - droneStateTimer > 1000) {
      currentDroneState = STATE_SCANNING;
      droneStateTimer = currentMillis;
      animFrame = 0;
      needUpdate = true;
    }
  }

  if (needUpdate)
    updateDisplay();
}

// ============================================================================
// LOGIKA SIKLUS WIFI SCAN (NON-BLOCKING)
// ============================================================================
void handleScanWifiLogic() {
  if (currentMode != MODE_SCAN_WIFI)
    return;

  unsigned long currentMillis = millis();
  bool needUpdate = false;

  if (currentWifiState == WIFI_STATE_SCANNING) {
    if (currentMillis - animTimer > 500) {
      animFrame = (animFrame + 1) % 4;
      animTimer = currentMillis;
      needUpdate = true;
    }
    if (currentMillis - wifiStateTimer > 3000) {
      currentWifiState = WIFI_STATE_SHOW_AP1;
      wifiStateTimer = currentMillis;
      needUpdate = true;
    }
  } else if (currentWifiState == WIFI_STATE_SHOW_AP1) {
    if (currentMillis - wifiStateTimer > 2500) {
      currentWifiState = WIFI_STATE_SHOW_AP2;
      wifiStateTimer = currentMillis;
      needUpdate = true;
    }
  } else if (currentWifiState == WIFI_STATE_SHOW_AP2) {
    if (currentMillis - wifiStateTimer > 2500) {
      currentWifiState = WIFI_STATE_SHOW_AP3;
      wifiStateTimer = currentMillis;
      needUpdate = true;
    }
  } else if (currentWifiState == WIFI_STATE_SHOW_AP3) {
    if (currentMillis - wifiStateTimer > 2500) {
      currentWifiState = WIFI_STATE_SCANNING;
      wifiStateTimer = currentMillis;
      animFrame = 0;
      needUpdate = true;
    }
  }

  if (needUpdate)
    updateDisplay();
}

// ============================================================================
// LOGIKA PEMANTAUAN KOMANDO DARI GATEWAY (REMOTE CONTROL)
// ============================================================================
void checkForCommands() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    // Cek apakah ini pesan komando atau sync
    if (incoming.indexOf("\"type\":\"sync\"") >= 0) {
      int idx = incoming.indexOf("\"time\":");
      if (idx >= 0) {
        String ts = incoming.substring(idx + 7, incoming.indexOf("}", idx));
        baseTime = ts.toInt();
        localMillisAtSync = millis();
        synced = true;
        Serial.println(">>> Menerima SYNC Dinamis | Base Timestamp: " +
                       String(baseTime));
        // Note: Tidak mengubah currentMode, sehingga tidak kembali ke Standby
        // jika sedang scan
      }
    } else if (incoming.indexOf("\"type\":\"command\"") >= 0) {
      if (incoming.indexOf("\"command\":\"POLL_" + NODE_ID + "\"") >= 0) {
        Serial.println(">>> Menerima Komando Panggilan Gateway <<<");
        isMyTurn = true;
        currentMode = MODE_DRONE_DETECT;
        currentDroneState = STATE_SCANNING;
        droneStateTimer = millis();
        animTimer = millis();
        animFrame = 0;
        updateDisplay();
      }
    }
  }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Gagal menemukan SSD1306. Cek wiring!"));
    for (;;)
      ;
  }

  // Init NRF & LoRa
  setupNRF();
  setupLoRa();
  Serial.println("System Ready.");

  // Meminta sinkronisasi dari Gateway (wajib sebelum masuk standby)
  waitForSync();

  // Render tampilan standby di awal
  updateDisplay();
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
  // --- Pantau Komando Jarak Jauh dari Gateway Terus-Menerus ---
  checkForCommands();

  // --- Logika Deteksi Tombol (Debouncing) ---
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) { // Tombol diklik
        if (waitingForDoubleClick && (millis() - clickTimer < 300)) {
          // DOUBLE CLICK DETECTED
          waitingForDoubleClick = false;
          currentMode = MODE_INFO;
          infoStateTimer = millis();
          Serial.println("Masuk Mode: Info");
          updateDisplay();
        } else {
          // First Click
          waitingForDoubleClick = true;
          clickTimer = millis();
        }
      }
    }
  }
  lastButtonState = reading;

  // Eksekusi Single Click jika timer habis (tidak ada klik kedua)
  if (waitingForDoubleClick && (millis() - clickTimer >= 300)) {
    waitingForDoubleClick = false;

    if (currentMode == MODE_STANDBY) {
      currentMode = MODE_DRONE_DETECT;
      currentDroneState = STATE_SCANNING;
      droneStateTimer = millis();
      animTimer = millis();
      animFrame = 0;
      Serial.println("Masuk Mode: Drone Detect");
    } else if (currentMode == MODE_DRONE_DETECT) {
      currentMode = MODE_SCAN_WIFI;
      currentWifiState = WIFI_STATE_SCANNING;
      wifiStateTimer = millis();
      animTimer = millis();
      animFrame = 0;
      Serial.println("Masuk Mode: Scan Wifi");
    } else {
      // Dari SCAN WIFI atau INFO kembali ke STANDBY
      currentMode = MODE_STANDBY;
      Serial.println("Masuk Mode: Standby");
    }
    updateDisplay();
  }

  // --- Jalankan Siklus Animasi Berdasarkan Mode ---
  if (currentMode == MODE_DRONE_DETECT) {
    handleDroneDetectLogic();
  } else if (currentMode == MODE_SCAN_WIFI) {
    handleScanWifiLogic();
  } else if (currentMode == MODE_INFO) {
    if (millis() - infoStateTimer >= 3000) {
      currentMode = MODE_STANDBY;
      Serial.println("Masuk Mode: Standby (Auto Exit Info)");
      updateDisplay();
    }
  }
}
