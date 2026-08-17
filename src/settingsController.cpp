#include "settingsController.h"
#include <Arduino.h>
#include "appState.h"
#include "buttons.h"
#include "rtcModule.h"
#include "displayModule.h"

// ============================================================
// ENTER SETTINGS
// ============================================================

static void enterClockSetting(const DateTime& now) {
    settingHour = now.hour();
    settingMinute = now.minute();
    settingDay = now.day();
    settingMonth = now.month();
    settingYear = now.year();

    settingDisplayDirty = false;
    clockMode = SET_HOUR_MODE;

    drawStatusText("SET HOUR");

    Serial.println("SET HOUR MODE");
}

static void enterAlarmSetting() {
    settingDisplayDirty = false;
    clockMode = SET_ALARM_HOUR_MODE;

    drawTimeValues(alarmHour, alarmMinute);
    drawStatusText("SET ALM HR");

    Serial.println("SET ALARM HOUR MODE");
}

// ============================================================
// NORMAL MODE BUTTONS
// ============================================================

static void updateNormalMode(const DateTime& now) {
    if (setButton.longPressed) {
        enterClockSetting(now);
    } else if (plusButton.longPressed) {
        enterAlarmSetting();
    } else if (minusButton.longPressed) {
        alarmEnabled = !alarmEnabled;

        setRTCAlarmEnabled(alarmEnabled);

        if (alarmEnabled) {
            Serial.println("ALARM 1 ENABLED");
        } else {
            Serial.println("ALARM 1 DISABLED");
        }

        drawAlarmStatus();
    }
}

// ============================================================
// SET HOUR
// ============================================================

static void updateSetHour() {
    if (plusButton.shortPressed) {
        settingHour++;

        if (settingHour > 23) {
            settingHour = 0;
        }

        markSettingDisplayDirty();
    }

    if (minusButton.shortPressed) {
        if (settingHour == 0) {
            settingHour = 23;
        } else {
            settingHour--;
        }

        markSettingDisplayDirty();
    }

    if (setButton.shortPressed) {
        settingDisplayDirty = false;
        clockMode = SET_MINUTE_MODE;

        drawStatusText("SET MIN");
    }
}

// ============================================================
// SET MINUTE
// ============================================================

static void updateSetMinute() {
    if (plusButton.shortPressed) {
        settingMinute++;

        if (settingMinute > 59) {
            settingMinute = 0;
        }

        markSettingDisplayDirty();
    }

    if (minusButton.shortPressed) {
        if (settingMinute == 0) {
            settingMinute = 59;
        } else {
            settingMinute--;
        }

        markSettingDisplayDirty();
    }

    if (setButton.shortPressed) {
        settingDisplayDirty = false;
        clockMode = SET_DAY_MODE;

        drawSetDayLabel();
    }
}

// ============================================================
// SET DAY
// ============================================================

static void updateSetDay() {
    uint8_t maxDay = daysInMonth(settingMonth, settingYear);

    if (plusButton.shortPressed) {
        settingDay++;

        if (settingDay > maxDay) {
            settingDay = 1;
        }

        markSettingDisplayDirty();
    }

    if (minusButton.shortPressed) {
        if (settingDay <= 1) {
            settingDay = maxDay;
        } else {
            settingDay--;
        }

        markSettingDisplayDirty();
    }

    if (setButton.shortPressed) {
        settingDisplayDirty = false;
        clockMode = SET_MONTH_MODE;

        drawSetMonthLabel();
    }
}

// ============================================================
// SET MONTH
// ============================================================

static void updateSetMonth() {
    if (plusButton.shortPressed) {
        settingMonth++;

        if (settingMonth > 12) {
            settingMonth = 1;
        }

        clampSettingDay();
        markSettingDisplayDirty();
    }

    if (minusButton.shortPressed) {
        if (settingMonth <= 1) {
            settingMonth = 12;
        } else {
            settingMonth--;
        }

        clampSettingDay();
        markSettingDisplayDirty();
    }

    if (setButton.shortPressed) {
        settingDisplayDirty = false;
        clockMode = SET_YEAR_MODE;

        drawSetYearLabel();
    }
}

// ============================================================
// SET YEAR
// ============================================================

static void updateSetYear() {
    if (plusButton.shortPressed) {
        settingYear++;

        if (settingYear > 2099) {
            settingYear = 2000;
        }

        clampSettingDay();
        markSettingDisplayDirty();
    }

    if (minusButton.shortPressed) {
        if (settingYear <= 2000) {
            settingYear = 2099;
        } else {
            settingYear--;
        }

        clampSettingDay();
        markSettingDisplayDirty();
    }

    if (setButton.shortPressed) {
        settingDisplayDirty = false;

        DateTime newTime(
            settingYear,
            settingMonth,
            settingDay,
            settingHour,
            settingMinute,
            0
        );

        adjustRTC(newTime);

        clockMode = NORMAL_MODE;

        previousMinute = 255;
        forceFullRefresh = true;

        Serial.println("CLOCK / DATE SAVED");
    }
}

// ============================================================
// SET ALARM HOUR
// ============================================================

static void updateSetAlarmHour() {
    if (plusButton.shortPressed) {
        alarmHour++;

        if (alarmHour > 23) {
            alarmHour = 0;
        }

        markSettingDisplayDirty();
    }

    if (minusButton.shortPressed) {
        if (alarmHour == 0) {
            alarmHour = 23;
        } else {
            alarmHour--;
        }

        markSettingDisplayDirty();
    }

    if (setButton.shortPressed) {
        settingDisplayDirty = false;
        clockMode = SET_ALARM_MINUTE_MODE;

        drawStatusText("SET ALM MIN");
    }
}

// ============================================================
// SET ALARM MINUTE
// ============================================================

static void updateSetAlarmMinute() {
    if (plusButton.shortPressed) {
        alarmMinute++;

        if (alarmMinute > 59) {
            alarmMinute = 0;
        }

        markSettingDisplayDirty();
    }

    if (minusButton.shortPressed) {
        if (alarmMinute == 0) {
            alarmMinute = 59;
        } else {
            alarmMinute--;
        }

        markSettingDisplayDirty();
    }

    if (setButton.shortPressed) {
        settingDisplayDirty = false;

        saveAlarmToRTC();

        clockMode = NORMAL_MODE;

        previousMinute = 255;
        forceFullRefresh = true;

        Serial.println("ALARM TIME SAVED");
    }
}

// ============================================================
// MAIN SETTINGS UPDATE
// ============================================================

void updateSettings(const DateTime& now) {
    switch (clockMode) {
        case NORMAL_MODE:
            updateNormalMode(now);
            break;

        case SET_HOUR_MODE:
            updateSetHour();
            break;

        case SET_MINUTE_MODE:
            updateSetMinute();
            break;

        case SET_DAY_MODE:
            updateSetDay();
            break;

        case SET_MONTH_MODE:
            updateSetMonth();
            break;

        case SET_YEAR_MODE:
            updateSetYear();
            break;

        case SET_ALARM_HOUR_MODE:
            updateSetAlarmHour();
            break;

        case SET_ALARM_MINUTE_MODE:
            updateSetAlarmMinute();
            break;
    }

    renderCurrentSettingValue();
}