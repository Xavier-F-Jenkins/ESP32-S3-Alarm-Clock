#include "appState.h"

ClockMode clockMode = NORMAL_MODE;

uint8_t settingHour = 0;
uint8_t settingMinute = 0;
uint8_t settingDay = 1;
uint8_t settingMonth = 1;
uint16_t settingYear = 2026;

uint8_t alarmHour = 7;
uint8_t alarmMinute = 0;

bool alarmEnabled = false;
bool alarmRinging = false;

uint8_t previousMinute = 255;
uint8_t minuteUpdatesSinceFullRefresh = 0;

bool forceFullRefresh = true;
bool settingDisplayDirty = false;

unsigned long lastSettingInputMillis = 0;