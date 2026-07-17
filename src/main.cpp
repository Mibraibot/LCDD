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
#define BUTTON_PIN 15

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================================================
// KONFIGURASI IDENTITAS NODE
// ============================================================================
const char NODE_ID[] = "Node3"; // UBAH JADI "Node2" / "Node3" UNTUK NODE LAIN

String pollCmd; // Pola komando poll milik node ini (dibentuk sekali di setup)

// ============================================================================
// GLOBAL VARIABLE LORA SYNC
// ============================================================================
unsigned long baseTime = 0;
unsigned long localMillisAtSync = 0;
bool synced = false;

// ============================================================================
// STATE & MODE APP
// ============================================================================
enum AppMode { MODE_STANDBY, MODE_DETECT, MODE_INFO };
AppMode currentMode = MODE_STANDBY;

// Layar "Sending" (ditahan sebentar agar terbaca, non-blocking)
bool showingSendScreen = false;
unsigned long sendScreenTimer = 0;
const unsigned long SEND_SCREEN_HOLD = 800; // ms

// Debounce tombol
bool buttonState = HIGH;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Double click
bool waitingForDoubleClick = false;
unsigned long clickTimer = 0;
unsigned long infoStateTimer = 0;

// Animasi spinner
uint8_t animFrame = 0;
unsigned long animTimer = 0;

// Data scan terakhir (dipakai ulang bila perlu)
String lastSpectrumData = "";

// ============================================================================
// TIMESTAMP WIB DARI SYNC (tanpa String, langsung ke buffer)
// ============================================================================
void getWIBTimestamp(char *buf, size_t len) {
  if (!synced) {
    snprintf(buf, len, "00:00:00");
    return;
  }
  unsigned long epoch = baseTime + ((millis() - localMillisAtSync) / 1000);
  snprintf(buf, len, "%02lu:%02lu:%02lu", (epoch / 3600) % 24,
           (epoch / 60) % 60, epoch % 60);
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

  } else if (currentMode == MODE_DETECT) {
    if (showingSendScreen) {
      display.setTextSize(1);
      display.setCursor(10, 20);
      display.println(F("Sending To"));
      display.setCursor(10, 35);
      display.println(F("Gateway..."));
    } else {
      display.setTextSize(2);
      display.setCursor(5, 10);
      display.print(F("WAITING"));
      display.setTextSize(1);
      display.setCursor(5, 30);
      display.print(F("Gateway Poll"));
      display.setTextSize(3);
      display.setCursor(85, 30);
      const char spinner[4] = {'-', '\\', '|', '/'};
      display.print(spinner[animFrame]);
    }

  } else if (currentMode == MODE_INFO) {
    display.setTextSize(1);
    display.println(F("--- NODE INFO ---"));
    display.setCursor(0, 15);
    display.print(F("Nama: "));
    display.println(NODE_ID);
    display.setCursor(0, 30);
    display.println(F("Lokasi  :"));
    display.setCursor(0, 45);
    display.println(F("Diatur via Dashboard"));
  }
  display.display();
}

// ============================================================================
// PARSING PESAN SYNC (dipakai waitForSync & checkForCommands)
// ============================================================================
bool applySync(const String &msg) {
  int idx = msg.indexOf(F("\"time\":"));
  if (idx < 0)
    return false;
  baseTime = msg.substring(idx + 7, msg.indexOf('}', idx)).toInt();
  localMillisAtSync = millis();
  synced = true;
  Serial.print(F(">>> SYNC diterima | Base Timestamp: "));
  Serial.println(baseTime);
  return true;
}

// ============================================================================
// SCAN + KIRIM BALASAN POLL KE GATEWAY
// showOnOled=false -> balas di background (layar tidak diganggu)
// ============================================================================
void replyToPoll(bool showOnOled) {
  if (showOnOled) {
    showingSendScreen = true;
    sendScreenTimer = millis();
    updateDisplay(); // Tampilkan "Sending" SEBELUM scan+kirim
  }

  lastSpectrumData = scanNRF(); // ~290 ms

  char ts[9];
  getWIBTimestamp(ts, sizeof(ts));

  // JSON kompak satu baris: airtime LoRa lebih singkat, tetap kompatibel
  // dengan parser gateway (ArduinoJson) dan regex app.py
  char payload[224];
  snprintf(payload, sizeof(payload),
           "{\"node\":\"%s\",\"timestamp_wib\":\"%s\",\"data_hex\":\"%s\"}",
           NODE_ID, ts, lastSpectrumData.c_str());

  Serial.println(F("Mengirim via LoRa:"));
  Serial.println(payload);
  sendLoRa(String(payload));
}

// ============================================================================
// SINKRONISASI AWAL (BLOCKING SESAAT, WAJIB SEBELUM STANDBY)
// ============================================================================
void waitForSync() {
  Serial.println(F("===================================="));
  Serial.println(F("Menunggu Sync Timestamp Gateway..."));

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println(F("Waiting Sync"));
  display.setCursor(10, 35);
  display.println(F("From Gateway..."));
  display.display();

  uint8_t dotCount = 0;
  unsigned long lastDotTime = millis();

  while (!synced) {
    if (millis() - lastDotTime > 500) {
      lastDotTime = millis();
      display.fillRect(10, 45, 100, 10, SSD1306_BLACK);
      display.setCursor(10, 45);
      for (uint8_t i = 0; i <= dotCount; i++)
        display.print('.');
      display.display();
      dotCount = (dotCount + 1) % 5;
    }

    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String incoming;
      incoming.reserve(packetSize);
      while (LoRa.available())
        incoming += (char)LoRa.read();

      if (incoming.indexOf(F("\"type\":\"sync\"")) >= 0 &&
          applySync(incoming)) {
        display.clearDisplay();
        display.setCursor(10, 25);
        display.println(F("SYNC SUCCESS!"));
        display.display();
        delay(1000);
      }
    }
    delay(10);
  }
}

// ============================================================================
// PEMANTAUAN KOMANDO DARI GATEWAY (SYNC DINAMIS + POLL)
// ============================================================================
void checkForCommands() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize)
    return;

  String incoming;
  incoming.reserve(packetSize);
  while (LoRa.available())
    incoming += (char)LoRa.read();

  if (incoming.indexOf(F("\"type\":\"sync\"")) >= 0) {
    // Sync dinamis: perbarui jam tanpa mengubah mode/layar
    applySync(incoming);

  } else if (incoming.indexOf(pollCmd) >= 0) {
    Serial.println(F(">>> Menerima Komando Panggilan Gateway <<<"));

    if (currentMode == MODE_INFO) {
      // Balas di background agar layar INFO tidak terganggu
      replyToPoll(false);
    } else {
      // Dari STANDBY otomatis masuk mode deteksi
      currentMode = MODE_DETECT;
      replyToPoll(true);
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

  setupNRF();
  setupLoRa();

  // Pola komando lengkap agar tidak salah cocok (mis. POLL_Node1 vs Node10)
  pollCmd = String(F("\"command\":\"POLL_")) + NODE_ID + "\"";
  lastSpectrumData.reserve(126);

  Serial.println(F("System Ready."));

  waitForSync();
  updateDisplay();
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
  unsigned long now = millis();

  // --- Pantau komando gateway terus-menerus ---
  checkForCommands();

  // --- Debouncing tombol ---
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState)
    lastDebounceTime = now;

  if ((now - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        if (waitingForDoubleClick && (now - clickTimer < 300)) {
          // DOUBLE CLICK -> Mode Info
          waitingForDoubleClick = false;
          currentMode = MODE_INFO;
          infoStateTimer = now;
          Serial.println(F("Masuk Mode: Info"));
          updateDisplay();
        } else {
          waitingForDoubleClick = true;
          clickTimer = now;
        }
      }
    }
  }
  lastButtonState = reading;

  // --- Single click: toggle Standby <-> Detect ---
  if (waitingForDoubleClick && (now - clickTimer >= 300)) {
    waitingForDoubleClick = false;

    if (currentMode == MODE_STANDBY) {
      currentMode = MODE_DETECT;
      showingSendScreen = false;
      animFrame = 0;
      animTimer = now;
      Serial.println(F("Masuk Mode: Drone Detect"));
    } else {
      currentMode = MODE_STANDBY;
      Serial.println(F("Masuk Mode: Standby"));
    }
    updateDisplay();
  }

  // --- Logika layar per mode (non-blocking) ---
  if (currentMode == MODE_DETECT) {
    if (showingSendScreen) {
      if (now - sendScreenTimer > SEND_SCREEN_HOLD) {
        showingSendScreen = false;
        animFrame = 0;
        animTimer = now;
        updateDisplay();
      }
    } else if (now - animTimer > 500) {
      animFrame = (animFrame + 1) % 4;
      animTimer = now;
      updateDisplay();
    }
  } else if (currentMode == MODE_INFO) {
    if (now - infoStateTimer >= 3000) {
      currentMode = MODE_STANDBY;
      Serial.println(F("Masuk Mode: Standby (Auto Exit Info)"));
      updateDisplay();
    }
  }
}