#include <Arduino.h>
#include "config.h"
#include "appState.h"
#include "buttons.h"
#include "rtcModule.h"
#include "displayModule.h"
#include "nfcModule.h"
#include "audioModule.h"
#include "loraModule.h"
#include "alarmController.h"
#include "settingsController.h"

// ============================================================
// NORMAL CLOCK DISPLAY
// ============================================================

void updateNormalClockDisplay(const DateTime& now) {
    if (clockMode != NORMAL_MODE) return;

    // Minute update
    if (now.minute() != previousMinute) {
        if (
            forceFullRefresh ||
            minuteUpdatesSinceFullRefresh >= FULL_REFRESH_INTERVAL - 1
        ) {
            drawFullScreen(now);

            minuteUpdatesSinceFullRefresh = 0;
            forceFullRefresh = false;
        } else {
            drawNormalPartial(now);
            minuteUpdatesSinceFullRefresh++;
        }

        previousMinute = now.minute();
    }

    // Seconds progress bar
    if (now.second() != previousSecond) {
        drawSecondBar(now.second());
        previousSecond = now.second();
    }
}

// ============================================================
// SETUP
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("ESP32 ALARM CLOCK");

    setupRTC();
    setupNFC();
    setupButtons();
    setupDisplay();
    setupLoRa();
    setupAudio();

    Serial.println("ESP32 Ready!");
}

// ============================================================
// LOOP
// ============================================================

void loop() {
    DateTime now = getCurrentTime();

    updateButtons();

    // Alarm controller handles:
    // RTC trigger, beep, NFC, LoRa and dismissal.
    updateAlarm(now);

    if (!isAlarmRinging()) {
        updateSettings(now);
        updateNormalClockDisplay(now);
    }

    delay(1);
}