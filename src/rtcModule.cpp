#include "rtcModule.h"

#include <Wire.h>

#include "config.h"
#include "appState.h"

static DS3231 rtcClock(Wire);

static const char* days[] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

static const char* months[] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};

void setupRTC() {
    Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN, 100000);
    Serial.println("RTC I2C started");

    loadAlarmFromRTC();
    Serial.println("RTC ready");
}

DateTime getCurrentTime() {
    return RTClib::now(Wire);
}

bool rtcAlarmTriggered() {
    return rtcClock.checkIfAlarm(1);
}

void clearRTCAlarmFlag() {
    rtcClock.checkIfAlarm(1);
}

void loadAlarmFromRTC() {
    byte alarmDay = 0;
    byte hour = 0;
    byte minute = 0;
    byte second = 0;
    byte alarmBits = 0;

    bool alarmByDay = false;
    bool alarm12Hour = false;
    bool alarmPM = false;

    rtcClock.getA1Time(
        alarmDay,
        hour,
        minute,
        second,
        alarmBits,
        alarmByDay,
        alarm12Hour,
        alarmPM,
        true
    );

    if (alarm12Hour) {
        if (hour == 12) hour = 0;
        if (alarmPM) hour += 12;
    }

    if (hour <= 23 && minute <= 59) {
        alarmHour = hour;
        alarmMinute = minute;
    } else {
        alarmHour = 7;
        alarmMinute = 0;
    }

    alarmEnabled = rtcClock.checkAlarmEnabled(1);
}

void saveAlarmToRTC() {
    rtcClock.setAlarm1Simple(alarmHour, alarmMinute);

    if (alarmEnabled) {
        rtcClock.turnOnAlarm(1);
    } else {
        rtcClock.turnOffAlarm(1);
    }

    rtcClock.checkIfAlarm(1);

    Serial.print("Alarm time saved: ");
    Serial.print(to12Hour(alarmHour));
    Serial.print(":");

    if (alarmMinute < 10) Serial.print("0");

    Serial.print(alarmMinute);
    Serial.print(" ");
    Serial.println(getAmPm(alarmHour));
}

void setRTCAlarmEnabled(bool enabled) {
    if (enabled) {
        rtcClock.turnOnAlarm(1);
    } else {
        rtcClock.turnOffAlarm(1);
    }
}

void adjustRTC(const DateTime& newTime) {
    rtcClock.adjust(newTime);
}

uint8_t to12Hour(uint8_t hour24) {
    uint8_t hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;
    return hour12;
}

String getAmPm(uint8_t hour24) {
    return hour24 < 12 ? "AM" : "PM";
}

String getDateString(const DateTime& now) {
    return String(days[now.dayOfTheWeek()]) + " " +
           String(now.day()) + " " +
           String(months[now.month() - 1]) + " " +
           String(now.year());
}

String getMonthName(uint8_t month) {
    if (month < 1 || month > 12) return "";
    return months[month - 1];
}

bool isLeapYear(uint16_t year) {
    if (year % 400 == 0) return true;
    if (year % 100 == 0) return false;
    return year % 4 == 0;
}

uint8_t daysInMonth(uint8_t month, uint16_t year) {
    switch (month) {
        case 1:
        case 3:
        case 5:
        case 7:
        case 8:
        case 10:
        case 12:
            return 31;

        case 4:
        case 6:
        case 9:
        case 11:
            return 30;

        case 2:
            return isLeapYear(year) ? 29 : 28;
    }

    return 31;
}

void clampSettingDay() {
    uint8_t maxDay = daysInMonth(settingMonth, settingYear);

    if (settingDay > maxDay) {
        settingDay = maxDay;
    }
}