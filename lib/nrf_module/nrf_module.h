#ifndef NRF_MODULE_H
#define NRF_MODULE_H

#include <Arduino.h>

// Fungsi untuk inisialisasi NRF24L01
void setupNRF();

// Fungsi untuk melakukan scanning NRF dan mengembalikan data dalam bentuk string hex
String scanNRF();

#endif
