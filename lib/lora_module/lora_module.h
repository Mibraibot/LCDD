#ifndef LORA_MODULE_H
#define LORA_MODULE_H

#include <Arduino.h>

// Fungsi untuk inisialisasi LoRa
void setupLoRa();

// Fungsi untuk mengirim data via LoRa
void sendLoRa(String payload);

#endif
