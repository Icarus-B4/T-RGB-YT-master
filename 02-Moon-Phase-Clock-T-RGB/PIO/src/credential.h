// credential.h
#ifndef CREDENTIAL_H
#define CREDENTIAL_H

struct WifiNetwork {
    const char* ssid;
    const char* password;
};

const WifiNetwork networks[] = {
    {"DeepCore", "ZrrJ-6H6t-xZb4-gq4D"},  // Home
    {"BTTON3REE", "5341@M5Sam"}           // Hotspot
};

const int numNetworks = sizeof(networks) / sizeof(networks[0]);

#endif
