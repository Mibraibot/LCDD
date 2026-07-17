#include "nrf_module.h"
#include <RF24.h>
#include <SPI.h>
#include <nRF24L01.h>

// ================= PIN SESUAI HARDWARE =================
#define NRF_CE 27
#define NRF_CSN 4
#define SCK_PIN 18
#define MISO_PIN 19
#define MOSI_PIN 23

RF24 radio(NRF_CE, NRF_CSN);
const char hex_chars[] = "0123456789ABCDEF";

// ================= LOW LEVEL READ REGISTER =================
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
    Serial.println(
        "{\"status\":\"error\", \"message\":\"NRF24L01 Not Found\"}");
    while (1)
      ;
  }

  radio.setAutoAck(false);
  radio.disableCRC();
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_2MBPS);
  radio.stopListening();
}

String scanNRF() {
  String spectrumData = "";

  // Loop scan 125 channel
  for (int ch = 0; ch < 125; ch++) {
    int hits = 0;

    radio.setChannel(ch);
    radio.startListening();
    // Tunggu PLL stabil (biasanya sekitar 130us)
    delayMicroseconds(130);

    // OPTIMASI: Iterasi diubah secara spesifik menjadi 220 kali pengulangan.
    // (220 * 10 mikrodetik = 2,2 milidetik per channel).
    // Mempercepat pengiriman secara signifikan dengan akurasi tangkapan yang tetap sangat baik.
    for (int s = 0; s < 220; s++) {
      delayMicroseconds(10);
      if (readRegister(0x09) & 1) {
        hits = 1; 
      }
    }

    radio.stopListening();

    // Hasilnya akan selalu '0' atau '1'
    spectrumData += hex_chars[hits];
  }
  return spectrumData;
}