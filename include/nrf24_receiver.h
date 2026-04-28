#ifndef NRF24_RECEIVER_H
#define NRF24_RECEIVER_H

#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

#define CE_PIN 4
#define CSN_PIN 5

const uint8_t RX_ADDRESS[6] = "00001";

class NRF24Receiver {

private:
    RF24 radio;
    uint8_t currentChannel;

public:

    NRF24Receiver(uint8_t cePin, uint8_t csnPin);

    bool begin();

    bool setChannelAndOpenPipe(uint8_t channel, uint8_t pipe);

    bool dataAvailable();

    bool readData(char* buffer, size_t bufferSize);

    int8_t readRSSI();

    void startListening();

    void stopListening();
};

#endif