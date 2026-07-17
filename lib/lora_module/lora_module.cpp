#include "lora_module.h"
#include <LoRa.h>

#define LORA_SS    5
#define LORA_RST   14
#define LORA_DIO0  26

void setupLoRa() {
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("{\"status\":\"error\", \"message\":\"LoRa failed\"}");
  } else {
    LoRa.setSpreadingFactor(9);
    LoRa.setSyncWord(0x34);
    Serial.println("{\"status\":\"success\", \"message\":\"LoRa Ready\"}");
  }
}

void sendLoRa(String payload) {
  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();
  Serial.println("{\"status\":\"success\", \"message\":\"Data sent\"}");
}