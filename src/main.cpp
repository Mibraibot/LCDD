#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>
#include "lora_module.h"
#include "nrf_module.h"
#include <LoRa.h>
#include <WiFi.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define BUTTON_PIN 15

// GLOBAL VARIABLE LORA SYNC & IDENTITAS
unsigned long baseTime = 0;
unsigned long localMillisAtSync = 0;
bool synced = false;

const String NODE_ID = "Node2"; // UBAH INI SESUAI IDENTITAS FISIK NODE (Node1/2/3)

// STATE & MODE APP
enum AppMode { MODE_STANDBY, MODE_DRONE_DETECT, MODE_SCAN_WIFI, MODE_INFO };
AppMode currentMode = MODE_STANDBY;

bool buttonState = HIGH;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

bool waitingForDoubleClick = false;
unsigned long clickTimer = 0;
unsigned long infoStateTimer = 0; 

int animFrame = 0;
unsigned long animTimer = 0;

bool isMyTurn = false;
String currentSpectrumData = "";
int currentThreatLevel = 1;

enum WifiState { WIFI_STATE_SCANNING, WIFI_STATE_SHOW_RESULTS };
WifiState currentWifiState = WIFI_STATE_SCANNING;
unsigned long wifiStateTimer = 0;
int totalNetworksFound = 0;
int currentNetworkIndex = 0;

String getWIBTimestamp() {
  if (!synced) return "00:00:00";
  unsigned long currentEpoch = baseTime + ((millis() - localMillisAtSync) / 1000);
  int seconds = currentEpoch % 60;
  int minutes = (currentEpoch / 60) % 60;
  int hours = (currentEpoch / 3600) % 24;
  char buf[20];
  sprintf(buf, "%02d:%02d:%02d", hours, minutes, seconds);
  return String(buf);
}

int calculateThreatLevel(String data_hex) {
  int total_energy = 0;
  int high_peaks = 0;
  for (int i = 0; i < data_hex.length(); i++) {
    int val = 0;
    char c = data_hex[i];
    if (c >= '0' && c <= '9') val = c - '0';
    else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
    total_energy += val;
    if (val >= 4) high_peaks++;
  }
  if (total_energy >= 30 || high_peaks >= 2) return 3; 
  else if (total_energy >= 15) return 2; 
  else return 1; 
}

// LAYAR OLED STATIS (TIDAK ADA DELAY ATAU ANIMASI BLOKING)
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
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("Node: "));
    display.println(NODE_ID);
    display.setCursor(0, 20);
    display.println(F("Threat Status:"));
    display.setCursor(0, 35);
    display.setTextSize(2);
    
    if (currentThreatLevel == 1) display.println(F("SAFE"));
    else if (currentThreatLevel == 2) display.println(F("WARNING"));
    else if (currentThreatLevel == 3) display.println(F("CRITICAL"));
    else display.println(F("UNKNOWN"));
  } else if (currentMode == MODE_SCAN_WIFI) {
    if (currentWifiState == WIFI_STATE_SCANNING) {
      display.setTextSize(2);
      display.setCursor(10, 10);
      display.print(F("WIFI SCAN"));
      display.setCursor(25, 40);
      for (int i = 0; i <= animFrame; i++) {
        display.print(F("| "));
      }
    } else if (currentWifiState == WIFI_STATE_SHOW_RESULTS) {
      display.setTextSize(1);
      display.setCursor(0, 0);
      if (totalNetworksFound == 0) {
        display.println(F("No WiFi Found!"));
      } else {
        display.print(F("Found WiFi ("));
        display.print(currentNetworkIndex + 1);
        display.print(F("/"));
        display.print(totalNetworksFound);
        display.println(F(")"));
        display.println(F("----------------"));
        
        String ssidStr = WiFi.SSID(currentNetworkIndex);
        if (ssidStr.length() > 10) display.setTextSize(1); 
        else display.setTextSize(2);
        
        display.println(ssidStr);
        display.setTextSize(1);
        display.print(F("RSSI : "));
        display.print(WiFi.RSSI(currentNetworkIndex));
        display.println(F(" dBm"));
        display.print(F("CH   : "));
        display.println(WiFi.channel(currentNetworkIndex));
      }
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

void waitForSync() {
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
    if (millis() - lastDotTime > 500) {
      lastDotTime = millis();
      display.fillRect(10, 45, 100, 10, SSD1306_BLACK); 
      display.setCursor(10, 45);
      for (int i = 0; i <= dotCount; i++) display.print(".");
      display.display();
      dotCount = (dotCount + 1) % 5;
    }

    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String incoming = "";
      while (LoRa.available()) incoming += (char)LoRa.read();
      
      if (incoming.indexOf("\"type\":\"sync\"") >= 0) {
        int idx = incoming.indexOf("\"time\":");
        if (idx >= 0) {
          String ts = incoming.substring(idx + 7, incoming.indexOf("}", idx));
          baseTime = ts.toInt();
          localMillisAtSync = millis();
          synced = true;

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

// LOGIKA DRONE DETECT SUPER CEPAT (LANGSUNG TEMBAK TANPA JEDA)
void handleDroneDetectLogic() {
  if (currentMode != MODE_DRONE_DETECT) return;

  if (isMyTurn) {
    currentSpectrumData = scanNRF();
    currentThreatLevel = calculateThreatLevel(currentSpectrumData);

    String jsonPayload = "{\n";
    jsonPayload += "  \"node\": \"" + NODE_ID + "\",\n";
    jsonPayload += "  \"captured_at\": \"" + getWIBTimestamp() + "\",\n";
    jsonPayload += "  \"data_hex\": \"" + currentSpectrumData + "\"\n";
    jsonPayload += "}";

    Serial.println("Mengirim via LoRa...");
    sendLoRa(jsonPayload);
    
    isMyTurn = false; 
    updateDisplay(); 
  }
}

void handleScanWifiLogic() {
  if (currentMode != MODE_SCAN_WIFI) return;

  unsigned long currentMillis = millis();
  bool needUpdate = false;

  if (currentWifiState == WIFI_STATE_SCANNING) {
    if (currentMillis - animTimer > 500) {
      animFrame = (animFrame + 1) % 4;
      animTimer = currentMillis;
      needUpdate = true;
    }
    
    int n = WiFi.scanComplete();
    if (n >= 0) {
      totalNetworksFound = n;
      currentNetworkIndex = 0;
      currentWifiState = WIFI_STATE_SHOW_RESULTS;
      wifiStateTimer = currentMillis;
      needUpdate = true;
    } else if (n == -2) {
      WiFi.scanNetworks(true);
    }
  } else if (currentWifiState == WIFI_STATE_SHOW_RESULTS) {
    if (currentMillis - wifiStateTimer > 2500) {
      currentNetworkIndex++;
      wifiStateTimer = currentMillis;
      needUpdate = true;
      
      if (totalNetworksFound == 0 || currentNetworkIndex >= totalNetworksFound) {
        WiFi.scanDelete(); 
        currentMode = MODE_DRONE_DETECT;
        updateDisplay();
      }
    }
  }
  if (needUpdate) updateDisplay();
}

void checkForCommands() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) incoming += (char)LoRa.read();

    if (incoming.indexOf("\"type\":\"sync\"") >= 0) {
      int idx = incoming.indexOf("\"time\":");
      if (idx >= 0) {
        String ts = incoming.substring(idx + 7, incoming.indexOf("}", idx));
        baseTime = ts.toInt();
        localMillisAtSync = millis();
        synced = true;
      }
    } else if (incoming.indexOf("\"type\":\"command\"") >= 0) {
      if (incoming.indexOf("\"command\":\"POLL_" + NODE_ID + "\"") >= 0) {
        Serial.println(">>> Menerima Komando Panggilan Gateway <<<");
        
        if (currentMode == MODE_STANDBY || currentMode == MODE_DRONE_DETECT) {
          currentMode = MODE_DRONE_DETECT;
          isMyTurn = true;
        } else {
          String jsonPayload = "{\n";
          jsonPayload += "  \"node\": \"" + NODE_ID + "\",\n";
          jsonPayload += "  \"captured_at\": \"" + getWIBTimestamp() + "\",\n";
          String hexToSend = (currentSpectrumData == "") ? "0000000000000000" : currentSpectrumData;
          jsonPayload += "  \"data_hex\": \"" + hexToSend + "\"\n";
          jsonPayload += "}";
          sendLoRa(jsonPayload);
        }
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Gagal menemukan SSD1306. Cek wiring!"));
    for (;;);
  }

  setupNRF();
  setupLoRa();
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.println("System Ready.");
  waitForSync();
  updateDisplay();
}

void loop() {
  checkForCommands();

  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) { 
        if (waitingForDoubleClick && (millis() - clickTimer < 300)) {
          waitingForDoubleClick = false;
          currentMode = MODE_INFO;
          infoStateTimer = millis();
          updateDisplay();
        } else {
          waitingForDoubleClick = true;
          clickTimer = millis();
        }
      }
    }
  }
  lastButtonState = reading;

  if (waitingForDoubleClick && (millis() - clickTimer >= 300)) {
    waitingForDoubleClick = false;
    if (currentMode == MODE_STANDBY) {
      currentMode = MODE_DRONE_DETECT;
    } else if (currentMode == MODE_DRONE_DETECT) {
      currentMode = MODE_SCAN_WIFI;
      currentWifiState = WIFI_STATE_SCANNING;
      wifiStateTimer = millis();
      animTimer = millis();
      animFrame = 0;
      WiFi.scanNetworks(true); 
    } else {
      currentMode = MODE_STANDBY;
    }
    updateDisplay();
  }

  if (currentMode == MODE_DRONE_DETECT) {
    handleDroneDetectLogic();
  } else if (currentMode == MODE_SCAN_WIFI) {
    handleScanWifiLogic();
  } else if (currentMode == MODE_INFO) {
    if (millis() - infoStateTimer >= 3000) {
      currentMode = MODE_STANDBY;
      updateDisplay();
    }
  }
}