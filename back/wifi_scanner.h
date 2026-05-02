#ifndef WIFI_SCANNER_H
#define WIFI_SCANNER_H

#include <Arduino.h>
#include <vector>
#include <string>

struct WiFiNetworkInfo {
    String ssid;
    int32_t rssi;
    int32_t channel;
    String bssid;
    String security;
};

int startWiFiScan();
std::vector<WiFiNetworkInfo> getScanResults();
void kirimJSONkeSerial();

#endif