#include "alarmController.h"

#include <Arduino.h>

#include "appState.h"
#include "buttons.h"
#include "rtcModule.h"
#include "displayModule.h"
#include "nfcModule.h"
#include "audioModule.h"
#include "loraModule.h"

// ============================================================
// DISMISS ALARM
// ============================================================

void dismissAlarm(const char* reason, bool notifyRemote) {
    if (!alarmRinging) return;

    alarmRinging = false;

    stopAlarmBeep();
    clearRTCAlarmFlag();

    previousMinute = 255;
    previousSecond = 255;
    forceFullRefresh = true;

    Serial.print("ALARM DISMISSED - ");
    Serial.println(reason);

    if (notifyRemote && isLoRaReady()) {
        sendLoRaPacket("ALARM_STOPPED");
    }
}

// ============================================================
// LORA
// ============================================================

static void handleLoRaMessage(const String& message) {
    if (message != "ALARM_OFF") return;

    Serial.println("REMOTE ALARM_OFF RECEIVED");

    if (alarmRinging) {
        dismissAlarm("LORA REMOTE", true);
    } else {
        // The remote may be retrying because it missed
        // our previous ALARM_STOPPED response.
        sendLoRaPacket("ALARM_STOPPED");
    }
}

static void checkAlarmLoRa() {
    String message;
    int rssi;

    if (!receiveLoRaPacket(message, rssi)) return;

    Serial.print("LoRa received: ");
    Serial.print(message);
    Serial.print(" RSSI: ");
    Serial.print(rssi);
    Serial.println(" dBm");

    handleLoRaMessage(message);
}

// ============================================================
// START ALARM
// ============================================================

static void startAlarm(const DateTime& now) {
    alarmRinging = true;

    Serial.println("ALARM 1 TRIGGERED!");

    startAlarmBeep();

    if (isLoRaReady()) {
        sendLoRaPacket("ALARM_STARTED");
    }

    drawAlarmRingingScreen(now);
}

// ============================================================
// UPDATE
// ============================================================

void updateAlarm(const DateTime& now) {
    // Always listen for remote packets.
    checkAlarmLoRa();

    // Start the alarm if the DS3231 alarm fired.
    if (!alarmRinging && rtcAlarmTriggered()) {
        startAlarm(now);
    }

    if (!alarmRinging) return;

    // Keep the speaker beep running.
    updateAlarmBeep();

    // Check LoRa again while ringing because the remote
    // needs to be as responsive as possible.
    checkAlarmLoRa();

    if (!alarmRinging) return;

    // Authorized NFC card dismisses the alarm.
    if (checkAuthorizedNFC()) {
        dismissAlarm("RFID", true);
        return;
    }

    // Local SET button also dismisses the alarm.
    if (setButton.shortPressed) {
        dismissAlarm("SET BUTTON", true);
    }
}

bool isAlarmRinging() {
    return alarmRinging;
}