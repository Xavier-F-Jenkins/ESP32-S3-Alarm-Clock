#include "audioModule.h"
#include <Arduino.h>
#include <driver/i2s_std.h>
#include <math.h>
#include "config.h"

static bool beepOn = false;

static unsigned long previousBeepMillis = 0;

static i2s_chan_handle_t i2sTxChannel = nullptr;

static bool audioReady = false;
static bool i2sEnabled = false;

static void writeAudioBuffer(int16_t* buffer, size_t bufferSize) {
    if (!audioReady || !i2sEnabled || i2sTxChannel == nullptr) {
        return;
    }

    size_t bytesWritten = 0;

    esp_err_t result = i2s_channel_write(
        i2sTxChannel,
        buffer,
        bufferSize,
        &bytesWritten,
        portMAX_DELAY
    );

    if (result != ESP_OK) {
        Serial.print("I2S write error: ");
        Serial.println(esp_err_to_name(result));
    }
}

static void playBeepChunk() {
    const int samples = 256;

    static float phase = 0.0f;
    static int16_t buffer[samples * 2];

    for (int i = 0; i < samples; i++) {
        int16_t sample = (int16_t)(sinf(phase) * BEEP_AMPLITUDE);

        phase += 2.0f * PI * BEEP_FREQUENCY / AUDIO_SAMPLE_RATE;

        if (phase >= 2.0f * PI) {
            phase -= 2.0f * PI;
        }

        buffer[i * 2] = sample;
        buffer[i * 2 + 1] = sample;
    }

    writeAudioBuffer(buffer, sizeof(buffer));
}

static void playSilenceChunk() {
    const int samples = 256;

    static int16_t silence[samples * 2] = {0};

    writeAudioBuffer(
        silence,
        sizeof(silence)
    );
}

void setupAudio() {
    Serial.println("Starting I2S audio");

    i2s_chan_config_t channelConfig =
        I2S_CHANNEL_DEFAULT_CONFIG(
            I2S_NUM_0,
            I2S_ROLE_MASTER
        );

    esp_err_t result = i2s_new_channel(
        &channelConfig,
        &i2sTxChannel,
        nullptr
    );

    Serial.print("I2S channel create: ");
    Serial.println(esp_err_to_name(result));

    if (result != ESP_OK) {
        audioReady = false;
        return;
    }

    i2s_std_config_t stdConfig = {};

    stdConfig.clk_cfg =
        I2S_STD_CLK_DEFAULT_CONFIG(
            AUDIO_SAMPLE_RATE
        );

    stdConfig.slot_cfg =
        I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_STEREO
        );

    stdConfig.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    stdConfig.gpio_cfg.bclk = (gpio_num_t)I2S_BCLK;
    stdConfig.gpio_cfg.ws = (gpio_num_t)I2S_LRC;
    stdConfig.gpio_cfg.dout = (gpio_num_t)I2S_DOUT;
    stdConfig.gpio_cfg.din = I2S_GPIO_UNUSED;

    stdConfig.gpio_cfg.invert_flags.mclk_inv = false;
    stdConfig.gpio_cfg.invert_flags.bclk_inv = false;
    stdConfig.gpio_cfg.invert_flags.ws_inv = false;

    result = i2s_channel_init_std_mode(
        i2sTxChannel,
        &stdConfig
    );

    Serial.print("I2S standard mode init: ");
    Serial.println(esp_err_to_name(result));

    if (result != ESP_OK) {
        audioReady = false;
        return;
    }

    result = i2s_channel_enable(
        i2sTxChannel
    );

    Serial.print("I2S channel enable: ");
    Serial.println(esp_err_to_name(result));

    if (result != ESP_OK) {
        audioReady = false;
        i2sEnabled = false;
        return;
    }

    audioReady = true;
    i2sEnabled = true;

    Serial.println("I2S audio ready");
}

void startAlarmBeep() {
    if (!audioReady) {
        Serial.println("Cannot start beep: I2S not ready");
        return;
    }

    if (!i2sEnabled) {
        esp_err_t result = i2s_channel_enable(
            i2sTxChannel
        );

        if (result != ESP_OK) {
            Serial.print("Failed to enable I2S: ");
            Serial.println(esp_err_to_name(result));
            return;
        }

        i2sEnabled = true;
    }

    beepOn = true;
    previousBeepMillis = millis();

    Serial.println("Alarm beep started");
}

void stopAlarmBeep() {
    beepOn = false;

    if (audioReady && i2sEnabled && i2sTxChannel != nullptr) {
        esp_err_t result = i2s_channel_disable(
            i2sTxChannel
        );

        Serial.print("I2S disabled: ");
        Serial.println(esp_err_to_name(result));

        if (result == ESP_OK) {
            i2sEnabled = false;
        }
    }

    Serial.println("Alarm beep stopped");
}

void updateAlarmBeep() {
    if (!audioReady || !i2sEnabled) return;

    unsigned long currentMillis = millis();

    if (currentMillis - previousBeepMillis >= BEEP_INTERVAL) {
        previousBeepMillis = currentMillis;
        beepOn = !beepOn;
    }

    if (beepOn) {
        playBeepChunk();
    } else {
        playSilenceChunk();
    }
}

bool isAudioReady() {
    return audioReady;
}