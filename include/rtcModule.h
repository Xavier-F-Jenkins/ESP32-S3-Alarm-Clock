#pragma once

#include <Arduino.h>
#include <DS3231.h>

void setupRTC();

DateTime getCurrentTime();

bool rtcAlarmTriggered();
void clearRTCAlarmFlag();

void loadAlarmFromRTC();
void saveAlarmToRTC();
void setRTCAlarmEnabled(bool enabled);

void adjustRTC(const DateTime& newTime);

uint8_t to12Hour(uint8_t hour24);

String getAmPm(uint8_t hour24);
String getDateString(const DateTime& now);
String getMonthName(uint8_t month);

bool isLeapYear(uint16_t year);
uint8_t daysInMonth(uint8_t month, uint16_t year);

void clampSettingDay();