#include "nrf_module.h"
#include <RF24.h>
#include <SPI.h>

#define NRF_CE 27
#define NRF_CSN 4
#define SCK_PIN 18
#define MISO_PIN 19
#define MOSI_PIN 23

RF24 radio(NRF_CE, NRF_CSN);
const char hex_chars[] = "0123456789ABCDEF";

uint8_t readRegister(uint8_t reg) {
  uint8_t result;
  digitalWrite(NRF_CSN, LOW);
  SPI.transfer(reg & 0x1F);
  result = SPI.transfer(0xFF);
  digitalWrite(NRF_CSN, HIGH);
  return result;
}

void setupNRF() {
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
}

String scanNRF() {
  String spectrumData = "";
  
  for (int ch = 0; ch < 125; ch++) {
    int hits = 0;
    radio.setChannel(ch);
    radio.startListening();
    delayMicroseconds(130); // Waktu wajib hardware untuk PLL Lock

    // OPTIMASI EKSTREM: Max 150 sampel (1.5ms) dan langsung break jika ketemu
    for (int s = 0; s < 150; s++) {
      delayMicroseconds(10);
      if (readRegister(0x09) & 1) {
        hits = 1; 
        break; // Hemat waktu instan
      }
    }
    radio.stopListening();
    spectrumData += hex_chars[hits];
  }
  return spectrumData;
}