#include "buttons.h"
#include "config.h"

ButtonState setButton = {
    SET_BUTTON, HIGH, HIGH, 0, 0, false, false, false
};

ButtonState plusButton = {
    PLUS_BUTTON, HIGH, HIGH, 0, 0, false, false, false
};

ButtonState minusButton = {
    MINUS_BUTTON, HIGH, HIGH, 0, 0, false, false, false
};

static void updateButton(ButtonState& button) {
    button.shortPressed = false;
    button.longPressed = false;

    bool reading = digitalRead(button.pin);

    if (reading != button.lastReading) {
        button.lastDebounceTime = millis();
    }

    if (millis() - button.lastDebounceTime >= DEBOUNCE_DELAY) {
        if (reading != button.stableState) {
            button.stableState = reading;

            if (button.stableState == LOW) {
                button.pressStartTime = millis();
                button.longPressTriggered = false;
            } else if (!button.longPressTriggered) {
                button.shortPressed = true;
            }
        }
    }

    if (
        button.stableState == LOW &&
        !button.longPressTriggered &&
        millis() - button.pressStartTime >= LONG_PRESS_TIME
    ) {
        button.longPressed = true;
        button.longPressTriggered = true;
    }

    button.lastReading = reading;
}

void setupButtons() {
    pinMode(PLUS_BUTTON, INPUT_PULLUP);
    pinMode(MINUS_BUTTON, INPUT_PULLUP);
    pinMode(SET_BUTTON, INPUT_PULLUP);
}

void updateButtons() {
    updateButton(setButton);
    updateButton(plusButton);
    updateButton(minusButton);
}