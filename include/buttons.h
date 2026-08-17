#pragma once

#include <Arduino.h>

struct ButtonState {
    uint8_t pin;
    bool lastReading;
    bool stableState;

    unsigned long lastDebounceTime;
    unsigned long pressStartTime;

    bool longPressTriggered;
    bool shortPressed;
    bool longPressed;
};

extern ButtonState setButton;
extern ButtonState plusButton;
extern ButtonState minusButton;

void setupButtons();
void updateButtons();