#pragma once

#include <Arduino.h>

enum ClockMode {
    NORMAL_MODE,
    SET_HOUR_MODE,
    SET_MINUTE_MODE,
    SET_DAY_MODE,
    SET_MONTH_MODE,
    SET_YEAR_MODE,
    SET_ALARM_HOUR_MODE,
    SET_ALARM_MINUTE_MODE
};

extern ClockMode clockMode;

extern uint8_t settingHour;
extern uint8_t settingMinute;
extern uint8_t settingDay;
extern uint8_t settingMonth;
extern uint16_t settingYear;

extern uint8_t alarmHour;
extern uint8_t alarmMinute;

extern bool alarmEnabled;
extern bool alarmRinging;

extern uint8_t previousMinute;
extern uint8_t minuteUpdatesSinceFullRefresh;

extern bool forceFullRefresh;
extern bool settingDisplayDirty;

extern unsigned long lastSettingInputMillis;