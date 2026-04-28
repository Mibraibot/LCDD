#include "nrf24_receiver.h"

NRF24Receiver::NRF24Receiver(uint8_t cePin, uint8_t csnPin)
: radio(cePin, csnPin)
{
}

bool NRF24Receiver::begin() {

    if (!radio.begin()) {
        Serial.println("NRF24L01 receiver: Gagal menginisialisasi.");
        Serial.println("Periksa koneksi SPI dan power.");
        return false;
    }

    Serial.println("NRF24L01 receiver: Inisialisasi berhasil.");

    radio.setPALevel(RF24_PA_LOW);
    radio.setDataRate(RF24_1MBPS);

    radio.setAutoAck(false);
    radio.disableCRC();

    return true;
}

bool NRF24Receiver::setChannelAndOpenPipe(uint8_t channel, uint8_t pipe) {

    if (channel > 127) {
        Serial.println("Error: Channel harus 0-127");
        return false;
    }

    if (pipe > 5) {
        Serial.println("Error: Pipe harus 0-5");
        return false;
    }

    currentChannel = channel;

    radio.stopListening();

    radio.setChannel(currentChannel);

    radio.openReadingPipe(pipe, RX_ADDRESS);

    radio.startListening();

    Serial.print("Listening Channel ");
    Serial.print(currentChannel);
    Serial.print(" Pipe ");
    Serial.println(pipe);

    return true;
}

bool NRF24Receiver::dataAvailable() {

    return radio.available();
}

bool NRF24Receiver::readData(char* buffer, size_t bufferSize) {

    if (!radio.available()) {
        return false;
    }

    radio.read(buffer, bufferSize);

    return true;
}

int8_t NRF24Receiver::readRSSI() {

    /*
    NRF24L01 tidak memiliki RSSI.
    Kita gunakan RPD (Received Power Detector)
    */

    if (radio.testRPD()) {
        return -60;  // sinyal kuat
    }

    return -90;     // sinyal lemah
}

void NRF24Receiver::startListening() {

    radio.startListening();
}

void NRF24Receiver::stopListening() {

    radio.stopListening();
}