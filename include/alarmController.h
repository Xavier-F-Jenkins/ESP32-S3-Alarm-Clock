#pragma once

#include <DS3231.h>

void updateAlarm(const DateTime& now);
void dismissAlarm(const char* reason, bool notifyRemote = true);

bool isAlarmRinging();