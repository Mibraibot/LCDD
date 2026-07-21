#ifndef LORA_MODULE_H
#define LORA_MODULE_H

#include <Arduino.h>

void setupLoRa();
void sendLoRa(String payload);
void reinitLoRa(); // reset + konfigurasi ulang radio (pemulihan mandiri)

#endif