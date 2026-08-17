#include "loraModule.h"

#include "config.h"

// ============================================================
// SX127x REGISTERS
// ============================================================

#define REG_FIFO                 0x00
#define REG_OP_MODE              0x01
#define REG_FRF_MSB              0x06
#define REG_FRF_MID              0x07
#define REG_FRF_LSB              0x08
#define REG_PA_CONFIG            0x09
#define REG_FIFO_ADDR_PTR        0x0D
#define REG_FIFO_TX_BASE_ADDR    0x0E
#define REG_FIFO_RX_BASE_ADDR    0x0F
#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_IRQ_FLAGS            0x12
#define REG_RX_NB_BYTES          0x13
#define REG_RSSI_VALUE           0x1B
#define REG_MODEM_CONFIG_1       0x1D
#define REG_MODEM_CONFIG_2       0x1E
#define REG_PREAMBLE_MSB         0x20
#define REG_PREAMBLE_LSB         0x21
#define REG_PAYLOAD_LENGTH       0x22
#define REG_MODEM_CONFIG_3       0x26
#define REG_DIO_MAPPING_1        0x40
#define REG_VERSION              0x42
#define REG_PA_DAC               0x4D

#define MODE_LONG_RANGE_MODE     0x80
#define MODE_SLEEP               0x00
#define MODE_STDBY               0x01
#define MODE_TX                  0x03
#define MODE_RX_CONTINUOUS       0x05

#define IRQ_TX_DONE_MASK         0x08
#define IRQ_RX_DONE_MASK         0x40
#define IRQ_PAYLOAD_CRC_ERROR    0x20

static bool loraReady = false;

// ============================================================
// PRIVATE SPI
// ============================================================

static uint8_t loraSpiTransfer(uint8_t value) {
    uint8_t result = 0;

    for (int i = 7; i >= 0; i--) {
        digitalWrite(LORA_SCK, LOW);
        digitalWrite(LORA_MOSI, (value >> i) & 1);

        delayMicroseconds(2);

        digitalWrite(LORA_SCK, HIGH);

        result <<= 1;

        if (digitalRead(LORA_MISO)) {
            result |= 1;
        }

        delayMicroseconds(2);
    }

    digitalWrite(LORA_SCK, LOW);

    return result;
}

static void loraWriteRegister(uint8_t address, uint8_t value) {
    digitalWrite(LORA_CS, LOW);

    loraSpiTransfer(address | 0x80);
    loraSpiTransfer(value);

    digitalWrite(LORA_CS, HIGH);
}

static uint8_t loraReadRegister(uint8_t address) {
    digitalWrite(LORA_CS, LOW);

    loraSpiTransfer(address & 0x7F);

    uint8_t value = loraSpiTransfer(0x00);

    digitalWrite(LORA_CS, HIGH);

    return value;
}

static void resetLoRa() {
    digitalWrite(LORA_RST, LOW);
    delay(10);

    digitalWrite(LORA_RST, HIGH);
    delay(20);
}

static void setLoRaFrequency(uint32_t frequency) {
    uint64_t frf =
        ((uint64_t)frequency << 19) /
        32000000;

    loraWriteRegister(REG_FRF_MSB, frf >> 16);
    loraWriteRegister(REG_FRF_MID, frf >> 8);
    loraWriteRegister(REG_FRF_LSB, frf);
}

static void enterLoRaReceiveMode() {
    loraWriteRegister(REG_DIO_MAPPING_1, 0x00);
    loraWriteRegister(REG_IRQ_FLAGS, 0xFF);

    loraWriteRegister(
        REG_OP_MODE,
        MODE_LONG_RANGE_MODE |
        MODE_RX_CONTINUOUS
    );
}

// ============================================================
// PUBLIC
// ============================================================

bool setupLoRa() {
    Serial.println("Starting LoRa");

    pinMode(LORA_CS, OUTPUT);
    pinMode(LORA_RST, OUTPUT);
    pinMode(LORA_SCK, OUTPUT);
    pinMode(LORA_MOSI, OUTPUT);
    pinMode(LORA_MISO, INPUT);
    pinMode(LORA_DIO0, INPUT);

    digitalWrite(LORA_CS, HIGH);
    digitalWrite(LORA_RST, HIGH);
    digitalWrite(LORA_SCK, LOW);
    digitalWrite(LORA_MOSI, LOW);

    resetLoRa();

    uint8_t version =
        loraReadRegister(
            REG_VERSION
        );

    Serial.print("LoRa RegVersion = 0x");
    Serial.println(version, HEX);

    if (version != 0x12) {
        Serial.println("SX127x not detected - LoRa disabled");

        loraReady = false;
        return false;
    }

    Serial.println("SX127x detected");

    loraWriteRegister(
        REG_OP_MODE,
        MODE_LONG_RANGE_MODE |
        MODE_SLEEP
    );

    delay(10);

    setLoRaFrequency(
        LORA_FREQUENCY
    );

    loraWriteRegister(REG_FIFO_TX_BASE_ADDR, 0x00);
    loraWriteRegister(REG_FIFO_RX_BASE_ADDR, 0x00);

    loraWriteRegister(REG_MODEM_CONFIG_1, 0x72);
    loraWriteRegister(REG_MODEM_CONFIG_2, 0x74);
    loraWriteRegister(REG_MODEM_CONFIG_3, 0x04);

    loraWriteRegister(REG_PREAMBLE_MSB, 0x00);
    loraWriteRegister(REG_PREAMBLE_LSB, 0x08);

    loraWriteRegister(REG_PA_CONFIG, 0x8F);
    loraWriteRegister(REG_PA_DAC, 0x84);

    enterLoRaReceiveMode();

    loraReady = true;

    Serial.println("LoRa ready");

    return true;
}

bool isLoRaReady() {
    return loraReady;
}

bool sendLoRaPacket(const char* message) {
    if (!loraReady) return false;

    uint8_t length = strlen(message);

    Serial.print("LoRa sending: ");
    Serial.println(message);

    loraWriteRegister(
        REG_OP_MODE,
        MODE_LONG_RANGE_MODE |
        MODE_STDBY
    );

    loraWriteRegister(
        REG_FIFO_ADDR_PTR,
        0x00
    );

    for (uint8_t i = 0; i < length; i++) {
        loraWriteRegister(
            REG_FIFO,
            message[i]
        );
    }

    loraWriteRegister(
        REG_PAYLOAD_LENGTH,
        length
    );

    loraWriteRegister(
        REG_DIO_MAPPING_1,
        0x40
    );

    loraWriteRegister(
        REG_IRQ_FLAGS,
        0xFF
    );

    loraWriteRegister(
        REG_OP_MODE,
        MODE_LONG_RANGE_MODE |
        MODE_TX
    );

    unsigned long start = millis();

    while (!(loraReadRegister(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK)) {
        if (millis() - start > 3000) {
            Serial.println("LoRa TX TIMEOUT");

            enterLoRaReceiveMode();

            return false;
        }

        delay(1);
    }

    loraWriteRegister(
        REG_IRQ_FLAGS,
        IRQ_TX_DONE_MASK
    );

    enterLoRaReceiveMode();

    Serial.println("LoRa packet sent");

    return true;
}

bool receiveLoRaPacket(String& message, int& rssi) {
    if (!loraReady) return false;

    uint8_t flags =
        loraReadRegister(
            REG_IRQ_FLAGS
        );

    if (!(flags & IRQ_RX_DONE_MASK)) {
        return false;
    }

    loraWriteRegister(
        REG_IRQ_FLAGS,
        flags
    );

    if (flags & IRQ_PAYLOAD_CRC_ERROR) {
        Serial.println("LoRa CRC error");
        return false;
    }

    uint8_t length =
        loraReadRegister(
            REG_RX_NB_BYTES
        );

    uint8_t address =
        loraReadRegister(
            REG_FIFO_RX_CURRENT_ADDR
        );

    loraWriteRegister(
        REG_FIFO_ADDR_PTR,
        address
    );

    message = "";

    for (uint8_t i = 0; i < length; i++) {
        message +=
            (char)loraReadRegister(
                REG_FIFO
            );
    }

    rssi =
        loraReadRegister(
            REG_RSSI_VALUE
        ) - 157;

    return true;
}