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
// PEMULIHAN MANDIRI RADIO
// Gateway mem-poll tiap ~1-3 dtk, jadi hening total selama 60 dtk berarti
// radio kemungkinan macet (mis. efek glitch di bus SPI yang dipakai bersama
// NRF24) — bukan sekadar giliran poll yang belum datang. Solusinya reset +
// konfigurasi ulang modul LoRa murni via software (reinitLoRa).
// ============================================================================
unsigned long lastPacketHeard = 0;
const unsigned long RADIO_STALL_MS = 60000;

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
// PARSING WAKTU DARI PAKET GATEWAY
// Berlaku untuk broadcast sync ({"type":"sync","time":...}) MAUPUN waktu
// yang dititipkan gateway di setiap poll ({"command":...,"time":...}).
// ============================================================================
bool applySync(const String &msg) {
  int idx = msg.indexOf(F("\"time\":"));
  if (idx < 0)
    return false;
  bool firstSync = !synced;
  baseTime = msg.substring(idx + 7, msg.indexOf('}', idx)).toInt();
  localMillisAtSync = millis();
  synced = true;
  if (firstSync) {
    // Refresh berkala berlangsung diam-diam; hanya sync pertama yang dicetak
    Serial.print(F(">>> SYNC diterima | Base Timestamp: "));
    Serial.println(baseTime);
  }
  return true;
}

// ============================================================================
// SCAN + KIRIM BALASAN POLL KE GATEWAY
// showOnOled=false -> balas di background (layar tidak diganggu)
// ============================================================================
void replyToPoll(bool showOnOled) {
  lastSpectrumData = scanNRF(); // ~290 ms (sampling TIDAK diubah)

  char ts[9];
  getWIBTimestamp(ts, sizeof(ts));

  // data_hex dikirim apa adanya: murni 125 karakter '0'/'1' (konsep RPD
  // nRF24, satu karakter per kanal) — tanpa pemadatan/akumulasi apa pun.
  // Di SF7 airtime payload 182 byte ini ~292 ms.
  char payload[224];
  snprintf(payload, sizeof(payload),
           "{\"node\":\"%s\",\"timestamp_wib\":\"%s\",\"data_hex\":\"%s\"}",
           NODE_ID, ts, lastSpectrumData.c_str());

  // Kirim DULU, log serial & OLED menyusul: keduanya (~45 ms) tadinya
  // menahan balasan di jalur kritis poll->reply. Gateway menerima ~45 ms
  // lebih cepat tanpa mengubah isi data sedikit pun.
  sendLoRa(String(payload));

  Serial.println(F("Terkirim via LoRa:"));
  Serial.println(payload);

  if (showOnOled) {
    showingSendScreen = true;
    sendScreenTimer = millis();
    updateDisplay();
  }
}

// ============================================================================
// PEMROSESAN SATU PAKET LORA (dipakai waitForSync & checkForCommands)
// Sync dan poll TIDAK lagi eksklusif: setiap paket yang membawa "time"
// memperbarui jam (broadcast sync ATAU poll ber-piggyback waktu), dan bila
// paket itu juga poll untuk node ini, langsung dibalas. Dengan ini node yang
// baru restart ikut siklus polling lagi pada poll PERTAMA yang didengarnya,
// tidak perlu menunggu broadcast sync 5-menit — penyebab utama node terlihat
// "timeout terus" di gateway.
// ============================================================================
void processLoRaMessage(const String &incoming) {
  if (incoming.indexOf(F("\"time\":")) >= 0)
    applySync(incoming);

  if (incoming.indexOf(pollCmd) >= 0) {
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
  lastPacketHeard = millis();

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

    // Radio hening terlalu lama saat menunggu sync juga dipulihkan di sini,
    // agar node tidak terjebak selamanya bila radionya yang bermasalah
    if (millis() - lastPacketHeard > RADIO_STALL_MS) {
      Serial.println(F(">>> 60 dtk tanpa paket LoRa — re-init radio"));
      reinitLoRa();
      lastPacketHeard = millis();
    }

    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      lastPacketHeard = millis();
      String incoming;
      incoming.reserve(packetSize);
      while (LoRa.available())
        incoming += (char)LoRa.read();

      // Waktu bisa datang dari broadcast sync maupun poll ber-piggyback;
      // bila paket itu poll untuk node ini, langsung ikut dibalas di sini.
      processLoRaMessage(incoming);
      if (synced) {
        display.clearDisplay();
        display.setCursor(10, 25);
        display.println(F("SYNC SUCCESS!"));
        display.display();
        delay(500);
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

  lastPacketHeard = millis();

  String incoming;
  incoming.reserve(packetSize);
  while (LoRa.available())
    incoming += (char)LoRa.read();

  processLoRaMessage(incoming);
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

  // --- Pemulihan mandiri bila radio hening terlalu lama ---
  if (now - lastPacketHeard > RADIO_STALL_MS) {
    Serial.println(F(">>> 60 dtk tanpa paket LoRa — re-init radio"));
    reinitLoRa();
    lastPacketHeard = millis();
  }

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