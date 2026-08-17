#pragma once

#include <Arduino.h>
#include <DS3231.h>

void setupDisplay();

void drawFullScreen(const DateTime& now);
void drawNormalPartial(const DateTime& now);

void drawTime(const DateTime& now);
void drawTimeValues(uint8_t hour, uint8_t minute);

void drawHourValues(uint8_t hour);
void drawMinuteValues(uint8_t minute);

void drawAlarmRingingScreen(const DateTime& now);
void drawAlarmStatus();

void drawSecondBar(uint8_t second);

void drawSetDayLabel();
void drawSetMonthLabel();
void drawSetYearLabel();

void drawStatusText(const String& text);

void markSettingDisplayDirty();
void renderCurrentSettingValue();