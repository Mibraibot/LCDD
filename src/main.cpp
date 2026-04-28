#include <Arduino.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// ================= PIN SESUAI HARDWARE =================
#define NRF_CE  27
#define NRF_CSN 4   
#define SCK_PIN  18
#define MISO_PIN 19
#define MOSI_PIN 23

RF24 radio(NRF_CE, NRF_CSN);
const char hex_chars[] = "0123456789ABCDEF";

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

// ================= LOW LEVEL READ REGISTER =================
uint8_t readRegister(uint8_t reg) {
  uint8_t result;
  digitalWrite(NRF_CSN, LOW);
  SPI.transfer(reg & 0x1F); 
  result = SPI.transfer(0xFF);
  digitalWrite(NRF_CSN, HIGH);
  return result;
}

void setup() {
  Serial.begin(115200);
  
  // Ambil waktu dari laptop saat compile
  setInitialTime();
  
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
  pinMode(NRF_CSN, OUTPUT);
  digitalWrite(NRF_CSN, HIGH);

  if (!radio.begin()) {
    Serial.println("{\"status\":\"error\", \"message\":\"NRF24L01 Not Found\"}");
    while (1);
  }

  radio.setAutoAck(false);
  radio.disableCRC();
  radio.setPALevel(RF24_PA_MAX); 
  radio.setDataRate(RF24_2MBPS); 
  radio.stopListening();

  Serial.println("{\"status\":\"success\", \"message\":\"Scanner Started\"}");
}

void loop() {
  String spectrumData = "";
  
  // Loop scan 125 channel
  for (int ch = 0; ch < 125; ch++) {
    int hits = 0;
    for (int s = 0; s < 10; s++) {
      radio.setChannel(ch);
      radio.startListening();
      delayMicroseconds(500); 
      if (readRegister(0x09) & 1) {
        hits++;
      }
      radio.stopListening();
    }
    if (hits > 15) hits = 15;
    spectrumData += hex_chars[hits];
  }

  // ================= JSON FORMAT PROFESIONAL =================
  Serial.println("{");
  Serial.println("  \"node\": \"Node 1\",");
  Serial.print("  \"timestamp_wib\": \""); 
  Serial.print(getWIBTimestamp()); 
  Serial.println("\",");
  Serial.print("  \"data_hex\": \""); 
  Serial.print(spectrumData); 
  Serial.println("\"");
  Serial.println("}");

  delay(500); 
}