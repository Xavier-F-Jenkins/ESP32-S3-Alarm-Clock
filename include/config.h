#pragma once

// ============================================================
// BUTTONS
// ============================================================

#define PLUS_BUTTON 35
#define MINUS_BUTTON 47
#define SET_BUTTON 21

#define DEBOUNCE_DELAY 40
#define LONG_PRESS_TIME 3000

// ============================================================
// RTC
// ============================================================

#define RTC_SDA_PIN 10
#define RTC_SCL_PIN 11

// ============================================================
// NFC
// ============================================================

#define NFC_SDA_PIN 1
#define NFC_SCL_PIN 2

#define PN532_IRQ -1
#define PN532_RESET -1

#define NFC_POLL_INTERVAL 100

// ============================================================
// E-INK
// ============================================================

#define EINK_MOSI_SDA_PIN 15
#define EINK_SCK_SCL_PIN 17
#define EINK_CS_PIN 7
#define EINK_DC_PIN 6
#define EINK_RES_PIN 5
#define EINK_BUSY_PIN 4

#define FULL_REFRESH_INTERVAL 3
#define SETTING_DISPLAY_DELAY 300

// ============================================================
// AUDIO
// ============================================================

#define I2S_BCLK 12
#define I2S_LRC 13
#define I2S_DOUT 14

#define BEEP_INTERVAL 500
#define BEEP_FREQUENCY 1000
#define BEEP_AMPLITUDE 12000
#define AUDIO_SAMPLE_RATE 16000

// ============================================================
// LORA
// ============================================================

#define LORA_DIO0 42
#define LORA_RST 41
#define LORA_CS 40
#define LORA_SCK 39
#define LORA_MOSI 38
#define LORA_MISO 18

#define LORA_FREQUENCY 915000000