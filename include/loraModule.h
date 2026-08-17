#pragma once

#include <Arduino.h>

bool setupLoRa();

bool isLoRaReady();

bool sendLoRaPacket(const char* message);

bool receiveLoRaPacket(
    String& message,
    int& rssi
);