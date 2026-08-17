#include "nfcModule.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PN532.h>

#include "config.h"

static Adafruit_PN532 nfc(
    PN532_IRQ,
    PN532_RESET,
    &Wire1
);

static bool nfcReady = false;

static unsigned long lastNfcPollMillis = 0;

static const uint8_t ALLOWED_UID_1[] = {
    0x89, 0xE1, 0x8C, 0x29
};

static const uint8_t ALLOWED_UID_2[] = {
    0x50, 0x1F, 0xF8, 0x5C
};

static bool uidMatches(
    const uint8_t* uid,
    uint8_t uidLength,
    const uint8_t* allowedUid,
    uint8_t allowedLength
) {
    if (uidLength != allowedLength) return false;

    for (uint8_t i = 0; i < uidLength; i++) {
        if (uid[i] != allowedUid[i]) return false;
    }

    return true;
}

static bool isAllowedCard(const uint8_t* uid, uint8_t uidLength) {
    return uidMatches(
        uid,
        uidLength,
        ALLOWED_UID_1,
        sizeof(ALLOWED_UID_1)
    ) ||
    uidMatches(
        uid,
        uidLength,
        ALLOWED_UID_2,
        sizeof(ALLOWED_UID_2)
    );
}

static void printUid(const uint8_t* uid, uint8_t uidLength) {
    for (uint8_t i = 0; i < uidLength; i++) {
        if (uid[i] < 0x10) Serial.print("0");

        Serial.print(uid[i], HEX);

        if (i < uidLength - 1) Serial.print(":");
    }
}

void setupNFC() {
    Serial.println("Configuring NFC I2C pins");

    bool pinsSet = Wire1.setPins(
        NFC_SDA_PIN,
        NFC_SCL_PIN
    );

    Serial.print("NFC pins configured: ");
    Serial.println(pinsSet ? "YES" : "NO");

    Serial.println("Starting PN532");

    nfc.begin();

    uint32_t versionData = nfc.getFirmwareVersion();

    if (!versionData) {
        Serial.println("PN532 not found - RFID disabled");

        nfcReady = false;
        return;
    }

    Serial.print("PN532 found, firmware ");
    Serial.print((versionData >> 16) & 0xFF);
    Serial.print(".");
    Serial.println((versionData >> 8) & 0xFF);

    if (!nfc.SAMConfig()) {
        Serial.println("PN532 SAMConfig failed");

        nfcReady = false;
        return;
    }

    nfcReady = true;

    Serial.println("PN532 ready");
}

bool isNFCReady() {
    return nfcReady;
}

bool checkAuthorizedNFC() {
    if (!nfcReady) return false;

    if (millis() - lastNfcPollMillis < NFC_POLL_INTERVAL) {
        return false;
    }

    lastNfcPollMillis = millis();

    uint8_t uid[7];
    uint8_t uidLength = 0;

    bool success = nfc.readPassiveTargetID(
        PN532_MIFARE_ISO14443A,
        uid,
        &uidLength,
        50
    );

    if (!success) return false;

    Serial.print("RFID detected: ");

    printUid(uid, uidLength);

    Serial.println();

    if (isAllowedCard(uid, uidLength)) {
        Serial.println("Authorized alarm card");
        return true;
    }

    Serial.println("Unauthorized RFID card");

    return false;
}