#include <Arduino.h>
#include <DS3231.h>
#include <GxEPD2_BW.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PN532.h>
#include <driver/i2s_std.h>
#include <math.h>

#include <InconsolataBold75pt7b.h>
#include <InconsolataBold24pt7b.h>

// ============================================================
// PIN DEFINITIONS
// ============================================================

// Buttons
#define PLUS_BUTTON 35
#define MINUS_BUTTON 47
#define SET_BUTTON 21

// RTC I2C
#define RTC_SDA_PIN 10
#define RTC_SCL_PIN 11

// PN532 I2C
#define NFC_SDA_PIN 1
#define NFC_SCL_PIN 2

// E-ink
#define EINK_MOSI_SDA_PIN 15
#define EINK_SCK_SCL_PIN 17
#define EINK_CS_PIN 7
#define EINK_DC_PIN 6
#define EINK_RES_PIN 5
#define EINK_BUSY_PIN 4

// MAX98357A
#define I2S_BCLK 12
#define I2S_LRC 13
#define I2S_DOUT 14

// LoRa
#define LORA_DIO0 42
#define LORA_RST 41
#define LORA_CS 40
#define LORA_SCK 39
#define LORA_MOSI 38
#define LORA_MISO 18

// PN532 - IRQ/reset not physically connected
#define PN532_IRQ -1
#define PN532_RESET -1

// ============================================================
// DISPLAY REGIONS
// ============================================================

#define HOUR1_X1 20
#define HOUR1_Y1 27
#define HOUR1_X2 105
#define HOUR1_Y2 223

#define HOUR2_X1 100
#define HOUR2_Y1 27
#define HOUR2_X2 178
#define HOUR2_Y2 223

#define COLON_X1 178
#define COLON_Y1 27
#define COLON_X2 222
#define COLON_Y2 223

#define MINUTE1_X1 222
#define MINUTE1_Y1 27
#define MINUTE1_X2 300
#define MINUTE1_Y2 223

#define MINUTE2_X1 295
#define MINUTE2_Y1 27
#define MINUTE2_X2 380
#define MINUTE2_Y2 223

#define DATE_X1 25
#define DATE_Y1 228
#define DATE_X2 375
#define DATE_Y2 275

#define AMPM_X1 330
#define AMPM_Y1 25
#define AMPM_X2 375
#define AMPM_Y2 60

#define ALARM_STATUS_X1 25
#define ALARM_STATUS_Y1 25
#define ALARM_STATUS_X2 95
#define ALARM_STATUS_Y2 60

// Seconds progress bar
#define SECOND_BAR_X 25
#define SECOND_BAR_Y 285
#define SECOND_BAR_WIDTH 350
#define SECOND_BAR_HEIGHT 10

// ============================================================
// FONTS
// ============================================================

#define TIME_FONT_XL &inconsolata_bold75pt7b
#define DATE_FONT &inconsolata_bold24pt7b

// ============================================================
// DATE NAMES
// ============================================================

const char* days[] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

const char* months[] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};

// ============================================================
// CLOCK STATE
// ============================================================

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

ClockMode clockMode = NORMAL_MODE;

uint8_t settingHour = 0;
uint8_t settingMinute = 0;
uint8_t settingDay = 1;
uint8_t settingMonth = 1;
uint16_t settingYear = 2026;

// ============================================================
// ALARM STATE
// ============================================================

uint8_t alarmHour = 7;
uint8_t alarmMinute = 0;

bool alarmEnabled = false;
bool alarmRinging = false;

// ============================================================
// DISPLAY UPDATE STATE
// ============================================================

uint8_t previousMinute = 255;
uint8_t previousSecond = 255;

const uint8_t FULL_REFRESH_INTERVAL = 3;

uint8_t minuteUpdatesSinceFullRefresh = 0;

bool forceFullRefresh = true;

bool settingDisplayDirty = false;
unsigned long lastSettingInputMillis = 0;

const unsigned long SETTING_DISPLAY_DELAY = 300;

// ============================================================
// BUTTON STATE
// ============================================================

const unsigned long DEBOUNCE_DELAY = 40;
const unsigned long LONG_PRESS_TIME = 3000;

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

ButtonState setButton = {
    SET_BUTTON, HIGH, HIGH, 0, 0, false, false, false
};

ButtonState plusButton = {
    PLUS_BUTTON, HIGH, HIGH, 0, 0, false, false, false
};

ButtonState minusButton = {
    MINUS_BUTTON, HIGH, HIGH, 0, 0, false, false, false
};

// ============================================================
// RTC
// ============================================================

DS3231 rtcClock(Wire);

// ============================================================
// PN532
// ============================================================

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET, &Wire1);

bool nfcReady = false;

const uint8_t ALLOWED_UID_1[] = {
    0x89, 0xE1, 0x8C, 0x29
};

const uint8_t ALLOWED_UID_2[] = {
    0x50, 0x1F, 0xF8, 0x5C
};

unsigned long lastNfcPollMillis = 0;

const unsigned long NFC_POLL_INTERVAL = 100;

// ============================================================
// AUDIO
// ============================================================

const unsigned long BEEP_INTERVAL = 500;

const int BEEP_FREQUENCY = 1000;
const int BEEP_AMPLITUDE = 12000;
const int AUDIO_SAMPLE_RATE = 16000;

bool beepOn = false;

unsigned long previousBeepMillis = 0;

i2s_chan_handle_t i2sTxChannel = nullptr;

bool audioReady = false;
bool i2sEnabled = false;

// ============================================================
// LORA REGISTERS
// ============================================================

#define REG_FIFO                 0x00
#define REG_OP_MODE              0x01
#define REG_FRF_MSB              0x06
#define REG_FRF_MID              0x07
#define REG_FRF_LSB              0x08
#define REG_PA_CONFIG            0x09
#define REG_FIFO_ADDR_PTR        0x0D
#define REG_FIFO_TX_BASE_ADDR    0x0E
#define REG_FIFO_RX_BASE_ADDR    0x0F
#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_IRQ_FLAGS            0x12
#define REG_RX_NB_BYTES          0x13
#define REG_RSSI_VALUE           0x1B
#define REG_MODEM_CONFIG_1       0x1D
#define REG_MODEM_CONFIG_2       0x1E
#define REG_PREAMBLE_MSB         0x20
#define REG_PREAMBLE_LSB         0x21
#define REG_PAYLOAD_LENGTH       0x22
#define REG_MODEM_CONFIG_3       0x26
#define REG_DIO_MAPPING_1        0x40
#define REG_VERSION              0x42
#define REG_PA_DAC               0x4D

#define MODE_LONG_RANGE_MODE     0x80
#define MODE_SLEEP               0x00
#define MODE_STDBY               0x01
#define MODE_TX                  0x03
#define MODE_RX_CONTINUOUS       0x05

#define IRQ_TX_DONE_MASK         0x08
#define IRQ_RX_DONE_MASK         0x40
#define IRQ_PAYLOAD_CRC_ERROR    0x20

bool loraReady = false;

// ============================================================
// DISPLAY
// ============================================================

GxEPD2_BW<
    GxEPD2_420_GDEY042T81,
    GxEPD2_420_GDEY042T81::HEIGHT
> display(
    GxEPD2_420_GDEY042T81(
        EINK_CS_PIN,
        EINK_DC_PIN,
        EINK_RES_PIN,
        EINK_BUSY_PIN
    )
);

// ============================================================
// TIME HELPERS
// ============================================================

uint8_t to12Hour(uint8_t hour24) {
    uint8_t hour12 = hour24 % 12;

    if (hour12 == 0) {
        hour12 = 12;
    }

    return hour12;
}

String getAmPm(uint8_t hour24) {
    return hour24 < 12 ? "AM" : "PM";
}

String getDateString(DateTime now) {
    return String(days[now.dayOfTheWeek()]) + " " +
           String(now.day()) + " " +
           String(months[now.month() - 1]) + " " +
           String(now.year());
}

// ============================================================
// DATE HELPERS
// ============================================================

bool isLeapYear(uint16_t year) {
    if (year % 400 == 0) {
        return true;
    }

    if (year % 100 == 0) {
        return false;
    }

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

// ============================================================
// BUTTON HANDLING
// ============================================================

void updateButton(ButtonState& button) {
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

void markSettingDisplayDirty() {
    settingDisplayDirty = true;
    lastSettingInputMillis = millis();
}

// ============================================================
// DRAWING HELPERS
// ============================================================

void drawCenteredDigitToBuffer(char digit, int x1, int y1, int x2, int y2) {
    int16_t textX, textY;
    uint16_t textWidth, textHeight;

    String text = String(digit);

    display.getTextBounds(
        text,
        0,
        0,
        &textX,
        &textY,
        &textWidth,
        &textHeight
    );

    int cursorX = x1 + ((x2 - x1 - textWidth) / 2) - textX;
    int cursorY = y1 + ((y2 - y1 - textHeight) / 2) - textY;

    display.setCursor(cursorX, cursorY);
    display.print(digit);
}

void drawCenteredTextToBuffer(String text, int x1, int y1, int x2, int y2) {
    int16_t textX, textY;
    uint16_t textWidth, textHeight;

    display.getTextBounds(
        text,
        0,
        0,
        &textX,
        &textY,
        &textWidth,
        &textHeight
    );

    int cursorX = x1 + ((x2 - x1 - textWidth) / 2) - textX;
    int cursorY = y1 + ((y2 - y1 - textHeight) / 2) - textY;

    display.setCursor(cursorX, cursorY);
    display.print(text);
}

void drawCenteredDigit(char digit, int x1, int y1, int x2, int y2) {
    int16_t textX, textY;
    uint16_t textWidth, textHeight;

    String text = String(digit);

    display.getTextBounds(
        text,
        0,
        0,
        &textX,
        &textY,
        &textWidth,
        &textHeight
    );

    int cursorX = x1 + ((x2 - x1 - textWidth) / 2) - textX;
    int cursorY = y1 + ((y2 - y1 - textHeight) / 2) - textY;

    int actualX = cursorX + textX;
    int actualY = cursorY + textY;

    const int padding = 2;

    display.setPartialWindow(
        actualX - padding,
        actualY - padding,
        textWidth + padding * 2,
        textHeight + padding * 2
    );

    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(cursorX, cursorY);
        display.print(digit);
    } while (display.nextPage());
}

void drawCenteredText(String text, int x1, int y1, int x2, int y2) {
    int16_t textX, textY;
    uint16_t textWidth, textHeight;

    display.getTextBounds(
        text,
        0,
        0,
        &textX,
        &textY,
        &textWidth,
        &textHeight
    );

    int cursorX = x1 + ((x2 - x1 - textWidth) / 2) - textX;
    int cursorY = y1 + ((y2 - y1 - textHeight) / 2) - textY;

    display.setPartialWindow(
        x1,
        y1,
        x2 - x1,
        y2 - y1
    );

    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(cursorX, cursorY);
        display.print(text);
    } while (display.nextPage());
}

// ============================================================
// SECOND BAR
// ============================================================

void drawSecondBarToBuffer(uint8_t second) {
    int filledWidth = map(
        second,
        0,
        59,
        0,
        SECOND_BAR_WIDTH
    );

    display.drawRect(
        SECOND_BAR_X,
        SECOND_BAR_Y,
        SECOND_BAR_WIDTH,
        SECOND_BAR_HEIGHT,
        GxEPD_BLACK
    );

    if (filledWidth > 0) {
        display.fillRect(
            SECOND_BAR_X,
            SECOND_BAR_Y,
            filledWidth,
            SECOND_BAR_HEIGHT,
            GxEPD_BLACK
        );
    }
}

void drawSecondBar(uint8_t second) {
    int filledWidth = map(
        second,
        0,
        59,
        0,
        SECOND_BAR_WIDTH
    );

    display.setPartialWindow(
        SECOND_BAR_X,
        SECOND_BAR_Y,
        SECOND_BAR_WIDTH,
        SECOND_BAR_HEIGHT
    );

    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);

        display.drawRect(
            SECOND_BAR_X,
            SECOND_BAR_Y,
            SECOND_BAR_WIDTH,
            SECOND_BAR_HEIGHT,
            GxEPD_BLACK
        );

        if (filledWidth > 0) {
            display.fillRect(
                SECOND_BAR_X,
                SECOND_BAR_Y,
                filledWidth,
                SECOND_BAR_HEIGHT,
                GxEPD_BLACK
            );
        }

    } while (display.nextPage());
}

// ============================================================
// BORDER DRAWING
// ============================================================

void drawBordersToBuffer() {
    display.drawLine(0, 25, 400, 25, GxEPD_BLACK);
    display.drawLine(0, 275, 400, 275, GxEPD_BLACK);
    display.drawLine(25, 0, 25, 300, GxEPD_BLACK);
    display.drawLine(375, 0, 375, 300, GxEPD_BLACK);

    for (int x = 25; x < 375; x += 5) {
        display.drawLine(
            x,
            225,
            x + 2,
            225,
            GxEPD_BLACK
        );
    }

    for (int y = 25; y < 225; y += 5) {
        display.drawLine(
            200,
            y,
            200,
            y + 2,
            GxEPD_BLACK
        );

        display.drawLine(
            288,
            y,
            288,
            y + 2,
            GxEPD_BLACK
        );

        display.drawLine(
            112,
            y,
            112,
            y + 2,
            GxEPD_BLACK
        );
    }
}

// ============================================================
// TIME DRAWING
// ============================================================

void drawHourValues(uint8_t hour24) {
    uint8_t hour = to12Hour(hour24);

    char hour1 = '0' + (hour / 10);
    char hour2 = '0' + (hour % 10);

    display.setFont(TIME_FONT_XL);

    drawCenteredDigit(
        hour1,
        HOUR1_X1,
        HOUR1_Y1,
        HOUR1_X2,
        HOUR1_Y2
    );

    drawCenteredDigit(
        hour2,
        HOUR2_X1,
        HOUR2_Y1,
        HOUR2_X2,
        HOUR2_Y2
    );

    display.setFont(DATE_FONT);

    drawCenteredText(
        getAmPm(hour24),
        AMPM_X1,
        AMPM_Y1,
        AMPM_X2,
        AMPM_Y2
    );
}

void drawMinuteValues(uint8_t minute) {
    char minute1 = '0' + (minute / 10);
    char minute2 = '0' + (minute % 10);

    display.setFont(TIME_FONT_XL);

    drawCenteredDigit(
        minute1,
        MINUTE1_X1,
        MINUTE1_Y1,
        MINUTE1_X2,
        MINUTE1_Y2
    );

    drawCenteredDigit(
        minute2,
        MINUTE2_X1,
        MINUTE2_Y1,
        MINUTE2_X2,
        MINUTE2_Y2
    );
}

void drawTimeValues(uint8_t hour, uint8_t minute) {
    drawHourValues(hour);
    drawMinuteValues(minute);
}

void drawTime(DateTime now) {
    drawTimeValues(
        now.hour(),
        now.minute()
    );
}

// ============================================================
// ALARM DISPLAY
// ============================================================

void drawAlarmRingingScreen(DateTime now) {
    drawTime(now);

    display.setFont(DATE_FONT);

    drawCenteredText(
        "WAKE UP!",
        100,
        25,
        300,
        60
    );

    drawCenteredText(
        "WAKE UP!",
        DATE_X1,
        DATE_Y1,
        DATE_X2,
        DATE_Y2
    );
}

void drawAlarmStatus() {
    display.setFont(DATE_FONT);

    drawCenteredText(
        alarmEnabled ? "A1" : "",
        ALARM_STATUS_X1,
        ALARM_STATUS_Y1,
        ALARM_STATUS_X2,
        ALARM_STATUS_Y2
    );
}

// ============================================================
// SETTING LABELS
// ============================================================

void drawSetDayLabel() {
    display.setFont(DATE_FONT);

    drawCenteredText(
        "SET DAY " + String(settingDay),
        DATE_X1,
        DATE_Y1,
        DATE_X2,
        DATE_Y2
    );
}

void drawSetMonthLabel() {
    display.setFont(DATE_FONT);

    drawCenteredText(
        "SET MONTH " + String(months[settingMonth - 1]),
        DATE_X1,
        DATE_Y1,
        DATE_X2,
        DATE_Y2
    );
}

void drawSetYearLabel() {
    display.setFont(DATE_FONT);

    drawCenteredText(
        "SET YEAR " + String(settingYear),
        DATE_X1,
        DATE_Y1,
        DATE_X2,
        DATE_Y2
    );
}

void renderCurrentSettingValue() {
    if (!settingDisplayDirty) {
        return;
    }

    if (millis() - lastSettingInputMillis < SETTING_DISPLAY_DELAY) {
        return;
    }

    settingDisplayDirty = false;

    if (clockMode == SET_HOUR_MODE) {
        drawHourValues(settingHour);
    } else if (clockMode == SET_MINUTE_MODE) {
        drawMinuteValues(settingMinute);
    } else if (clockMode == SET_DAY_MODE) {
        drawSetDayLabel();
    } else if (clockMode == SET_MONTH_MODE) {
        drawSetMonthLabel();
    } else if (clockMode == SET_YEAR_MODE) {
        drawSetYearLabel();
    } else if (clockMode == SET_ALARM_HOUR_MODE) {
        drawHourValues(alarmHour);
    } else if (clockMode == SET_ALARM_MINUTE_MODE) {
        drawMinuteValues(alarmMinute);
    }
}

// ============================================================
// FULL DISPLAY
// ============================================================

void drawFullScreen(DateTime now) {
    uint8_t hour12 = to12Hour(now.hour());

    char hour1 = '0' + (hour12 / 10);
    char hour2 = '0' + (hour12 % 10);
    char minute1 = '0' + (now.minute() / 10);
    char minute2 = '0' + (now.minute() % 10);

    display.setFullWindow();
    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);

        drawBordersToBuffer();

        display.setFont(TIME_FONT_XL);

        drawCenteredDigitToBuffer(
            hour1,
            HOUR1_X1,
            HOUR1_Y1,
            HOUR1_X2,
            HOUR1_Y2
        );

        drawCenteredDigitToBuffer(
            hour2,
            HOUR2_X1,
            HOUR2_Y1,
            HOUR2_X2,
            HOUR2_Y2
        );

        drawCenteredDigitToBuffer(
            ':',
            COLON_X1,
            COLON_Y1,
            COLON_X2,
            COLON_Y2
        );

        drawCenteredDigitToBuffer(
            minute1,
            MINUTE1_X1,
            MINUTE1_Y1,
            MINUTE1_X2,
            MINUTE1_Y2
        );

        drawCenteredDigitToBuffer(
            minute2,
            MINUTE2_X1,
            MINUTE2_Y1,
            MINUTE2_X2,
            MINUTE2_Y2
        );

        display.setFont(DATE_FONT);

        drawCenteredTextToBuffer(
            getAmPm(now.hour()),
            AMPM_X1,
            AMPM_Y1,
            AMPM_X2,
            AMPM_Y2
        );

        drawCenteredTextToBuffer(
            getDateString(now),
            DATE_X1,
            DATE_Y1,
            DATE_X2,
            DATE_Y2
        );

        if (alarmEnabled) {
            drawCenteredTextToBuffer(
                "A1",
                ALARM_STATUS_X1,
                ALARM_STATUS_Y1,
                ALARM_STATUS_X2,
                ALARM_STATUS_Y2
            );
        }

        drawSecondBarToBuffer(
            now.second()
        );

    } while (display.nextPage());

    previousSecond = now.second();

    Serial.println(
        "FULL DISPLAY REFRESH"
    );
}

void drawNormalPartial(DateTime now) {
    drawTime(now);

    display.setFont(DATE_FONT);

    drawCenteredText(
        getDateString(now),
        DATE_X1,
        DATE_Y1,
        DATE_X2,
        DATE_Y2
    );

    drawAlarmStatus();
}

// ============================================================
// RTC ALARM HELPERS
// ============================================================

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
        if (hour == 12) {
            hour = 0;
        }

        if (alarmPM) {
            hour += 12;
        }
    }

    if (hour <= 23 && minute <= 59) {
        alarmHour = hour;
        alarmMinute = minute;
    } else {
        alarmHour = 7;
        alarmMinute = 0;
    }

    alarmEnabled =
        rtcClock.checkAlarmEnabled(1);
}

void saveAlarmToRTC() {
    rtcClock.setAlarm1Simple(
        alarmHour,
        alarmMinute
    );

    if (alarmEnabled) {
        rtcClock.turnOnAlarm(1);
    } else {
        rtcClock.turnOffAlarm(1);
    }

    rtcClock.checkIfAlarm(1);

    Serial.print("Alarm time saved: ");
    Serial.print(to12Hour(alarmHour));
    Serial.print(":");

    if (alarmMinute < 10) {
        Serial.print("0");
    }

    Serial.print(alarmMinute);
    Serial.print(" ");
    Serial.println(getAmPm(alarmHour));
}

// ============================================================
// MODE ENTRY
// ============================================================

void enterClockSetting(DateTime now) {
    settingHour = now.hour();
    settingMinute = now.minute();
    settingDay = now.day();
    settingMonth = now.month();
    settingYear = now.year();

    settingDisplayDirty = false;

    clockMode = SET_HOUR_MODE;

    display.setFont(DATE_FONT);

    drawCenteredText(
        "SET HOUR",
        DATE_X1,
        DATE_Y1,
        DATE_X2,
        DATE_Y2
    );

    Serial.println("SET HOUR MODE");
}

void enterAlarmSetting() {
    settingDisplayDirty = false;

    clockMode =
        SET_ALARM_HOUR_MODE;

    drawTimeValues(
        alarmHour,
        alarmMinute
    );

    display.setFont(DATE_FONT);

    drawCenteredText(
        "SET ALM HR",
        DATE_X1,
        DATE_Y1,
        DATE_X2,
        DATE_Y2
    );

    Serial.println(
        "SET ALARM HOUR MODE"
    );
}

// ============================================================
// NFC
// ============================================================

bool uidMatches(
    const uint8_t* uid,
    uint8_t uidLength,
    const uint8_t* allowedUid,
    uint8_t allowedLength
) {
    if (uidLength != allowedLength) {
        return false;
    }

    for (uint8_t i = 0; i < uidLength; i++) {
        if (uid[i] != allowedUid[i]) {
            return false;
        }
    }

    return true;
}

bool isAllowedCard(
    const uint8_t* uid,
    uint8_t uidLength
) {
    return uidMatches(
        uid,
        uidLength,
        ALLOWED_UID_1,
        sizeof(ALLOWED_UID_1)
    ) ||
    uidMatches(
        uid,
        uidLength,
        ALLOWED_UID_2,
        sizeof(ALLOWED_UID_2)
    );
}

void printUid(
    const uint8_t* uid,
    uint8_t uidLength
) {
    for (uint8_t i = 0; i < uidLength; i++) {
        if (uid[i] < 0x10) {
            Serial.print("0");
        }

        Serial.print(
            uid[i],
            HEX
        );

        if (i < uidLength - 1) {
            Serial.print(":");
        }
    }
}

void setupNFC() {
    Serial.println(
        "Configuring NFC I2C pins"
    );

    bool pinsSet =
        Wire1.setPins(
            NFC_SDA_PIN,
            NFC_SCL_PIN
        );

    Serial.print(
        "NFC pins configured: "
    );

    Serial.println(
        pinsSet ? "YES" : "NO"
    );

    Serial.println(
        "Starting PN532"
    );

    nfc.begin();

    uint32_t versionData =
        nfc.getFirmwareVersion();

    if (!versionData) {
        Serial.println(
            "PN532 not found - RFID disabled"
        );

        nfcReady = false;
        return;
    }

    Serial.print(
        "PN532 found, firmware "
    );

    Serial.print(
        (versionData >> 16) & 0xFF
    );

    Serial.print(".");

    Serial.println(
        (versionData >> 8) & 0xFF
    );

    if (!nfc.SAMConfig()) {
        Serial.println(
            "PN532 SAMConfig failed"
        );

        nfcReady = false;
        return;
    }

    nfcReady = true;

    Serial.println(
        "PN532 ready"
    );
}

// ============================================================
// AUDIO
// ============================================================

void setupAudio() {
    Serial.println(
        "Starting I2S audio"
    );

    i2s_chan_config_t channelConfig =
        I2S_CHANNEL_DEFAULT_CONFIG(
            I2S_NUM_0,
            I2S_ROLE_MASTER
        );

    esp_err_t result =
        i2s_new_channel(
            &channelConfig,
            &i2sTxChannel,
            nullptr
        );

    Serial.print(
        "I2S channel create: "
    );

    Serial.println(
        esp_err_to_name(result)
    );

    if (result != ESP_OK) {
        audioReady = false;
        return;
    }

    i2s_std_config_t stdConfig = {};

    stdConfig.clk_cfg =
        I2S_STD_CLK_DEFAULT_CONFIG(
            AUDIO_SAMPLE_RATE
        );

    stdConfig.slot_cfg =
        I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_STEREO
        );

    stdConfig.gpio_cfg.mclk =
        I2S_GPIO_UNUSED;

    stdConfig.gpio_cfg.bclk =
        (gpio_num_t)I2S_BCLK;

    stdConfig.gpio_cfg.ws =
        (gpio_num_t)I2S_LRC;

    stdConfig.gpio_cfg.dout =
        (gpio_num_t)I2S_DOUT;

    stdConfig.gpio_cfg.din =
        I2S_GPIO_UNUSED;

    stdConfig.gpio_cfg.invert_flags.mclk_inv =
        false;

    stdConfig.gpio_cfg.invert_flags.bclk_inv =
        false;

    stdConfig.gpio_cfg.invert_flags.ws_inv =
        false;

    result =
        i2s_channel_init_std_mode(
            i2sTxChannel,
            &stdConfig
        );

    Serial.print(
        "I2S standard mode init: "
    );

    Serial.println(
        esp_err_to_name(result)
    );

    if (result != ESP_OK) {
        audioReady = false;
        return;
    }

    result =
        i2s_channel_enable(
            i2sTxChannel
        );

    Serial.print(
        "I2S channel enable: "
    );

    Serial.println(
        esp_err_to_name(result)
    );

    if (result != ESP_OK) {
        audioReady = false;
        i2sEnabled = false;
        return;
    }

    audioReady = true;
    i2sEnabled = true;

    Serial.println(
        "I2S audio ready"
    );
}

void writeAudioBuffer(
    int16_t* buffer,
    size_t bufferSize
) {
    if (
        !audioReady ||
        !i2sEnabled ||
        i2sTxChannel == nullptr
    ) {
        return;
    }

    size_t bytesWritten = 0;

    esp_err_t result =
        i2s_channel_write(
            i2sTxChannel,
            buffer,
            bufferSize,
            &bytesWritten,
            portMAX_DELAY
        );

    if (result != ESP_OK) {
        Serial.print(
            "I2S write error: "
        );

        Serial.println(
            esp_err_to_name(result)
        );
    }
}

void playBeepChunk() {
    const int samples = 256;

    static float phase = 0.0f;
    static int16_t buffer[samples * 2];

    for (int i = 0; i < samples; i++) {
        int16_t sample =
            (int16_t)(
                sinf(phase) *
                BEEP_AMPLITUDE
            );

        phase +=
            2.0f *
            PI *
            BEEP_FREQUENCY /
            AUDIO_SAMPLE_RATE;

        if (phase >= 2.0f * PI) {
            phase -= 2.0f * PI;
        }

        buffer[i * 2] = sample;
        buffer[i * 2 + 1] = sample;
    }

    writeAudioBuffer(
        buffer,
        sizeof(buffer)
    );
}

void playSilenceChunk() {
    const int samples = 256;

    static int16_t silence[samples * 2] = {0};

    writeAudioBuffer(
        silence,
        sizeof(silence)
    );
}

void startAlarmBeep() {
    if (!audioReady) {
        Serial.println(
            "Cannot start beep: I2S not ready"
        );

        return;
    }

    if (!i2sEnabled) {
        esp_err_t result =
            i2s_channel_enable(
                i2sTxChannel
            );

        if (result != ESP_OK) {
            Serial.print(
                "Failed to enable I2S: "
            );

            Serial.println(
                esp_err_to_name(result)
            );

            return;
        }

        i2sEnabled = true;
    }

    beepOn = true;

    previousBeepMillis =
        millis();

    Serial.println(
        "Alarm beep started"
    );
}

void stopAlarmBeep() {
    beepOn = false;

    if (
        audioReady &&
        i2sEnabled &&
        i2sTxChannel != nullptr
    ) {
        esp_err_t result =
            i2s_channel_disable(
                i2sTxChannel
            );

        Serial.print(
            "I2S disabled: "
        );

        Serial.println(
            esp_err_to_name(result)
        );

        if (result == ESP_OK) {
            i2sEnabled = false;
        }
    }

    Serial.println(
        "Alarm beep stopped"
    );
}

void updateAlarmBeep() {
    if (
        !audioReady ||
        !i2sEnabled
    ) {
        return;
    }

    unsigned long currentMillis =
        millis();

    if (
        currentMillis -
        previousBeepMillis >=
        BEEP_INTERVAL
    ) {
        previousBeepMillis =
            currentMillis;

        beepOn = !beepOn;
    }

    if (beepOn) {
        playBeepChunk();
    } else {
        playSilenceChunk();
    }
}

// ============================================================
// LORA BIT-BANGED SPI
// ============================================================

uint8_t loraSpiTransfer(uint8_t value) {
    uint8_t result = 0;

    for (int i = 7; i >= 0; i--) {
        digitalWrite(
            LORA_SCK,
            LOW
        );

        digitalWrite(
            LORA_MOSI,
            (value >> i) & 1
        );

        delayMicroseconds(2);

        digitalWrite(
            LORA_SCK,
            HIGH
        );

        result <<= 1;

        if (
            digitalRead(
                LORA_MISO
            )
        ) {
            result |= 1;
        }

        delayMicroseconds(2);
    }

    digitalWrite(
        LORA_SCK,
        LOW
    );

    return result;
}

void loraWriteRegister(
    uint8_t address,
    uint8_t value
) {
    digitalWrite(
        LORA_CS,
        LOW
    );

    loraSpiTransfer(
        address | 0x80
    );

    loraSpiTransfer(
        value
    );

    digitalWrite(
        LORA_CS,
        HIGH
    );
}

uint8_t loraReadRegister(
    uint8_t address
) {
    digitalWrite(
        LORA_CS,
        LOW
    );

    loraSpiTransfer(
        address & 0x7F
    );

    uint8_t value =
        loraSpiTransfer(
            0x00
        );

    digitalWrite(
        LORA_CS,
        HIGH
    );

    return value;
}

void resetLoRa() {
    digitalWrite(
        LORA_RST,
        LOW
    );

    delay(10);

    digitalWrite(
        LORA_RST,
        HIGH
    );

    delay(20);
}

void setLoRaFrequency(
    uint32_t frequency
) {
    uint64_t frf =
        ((uint64_t)frequency << 19) /
        32000000;

    loraWriteRegister(
        REG_FRF_MSB,
        frf >> 16
    );

    loraWriteRegister(
        REG_FRF_MID,
        frf >> 8
    );

    loraWriteRegister(
        REG_FRF_LSB,
        frf
    );
}

void enterLoRaReceiveMode() {
    loraWriteRegister(
        REG_DIO_MAPPING_1,
        0x00
    );

    loraWriteRegister(
        REG_IRQ_FLAGS,
        0xFF
    );

    loraWriteRegister(
        REG_OP_MODE,
        MODE_LONG_RANGE_MODE |
        MODE_RX_CONTINUOUS
    );
}

bool setupLoRa() {
    Serial.println(
        "Starting LoRa"
    );

    pinMode(
        LORA_CS,
        OUTPUT
    );

    pinMode(
        LORA_RST,
        OUTPUT
    );

    pinMode(
        LORA_SCK,
        OUTPUT
    );

    pinMode(
        LORA_MOSI,
        OUTPUT
    );

    pinMode(
        LORA_MISO,
        INPUT
    );

    pinMode(
        LORA_DIO0,
        INPUT
    );

    digitalWrite(
        LORA_CS,
        HIGH
    );

    digitalWrite(
        LORA_RST,
        HIGH
    );

    digitalWrite(
        LORA_SCK,
        LOW
    );

    digitalWrite(
        LORA_MOSI,
        LOW
    );

    resetLoRa();

    uint8_t version =
        loraReadRegister(
            REG_VERSION
        );

    Serial.print(
        "LoRa RegVersion = 0x"
    );

    Serial.println(
        version,
        HEX
    );

    if (version != 0x12) {
        Serial.println(
            "SX127x not detected - LoRa disabled"
        );

        loraReady = false;
        return false;
    }

    Serial.println(
        "SX127x detected"
    );

    loraWriteRegister(
        REG_OP_MODE,
        MODE_LONG_RANGE_MODE |
        MODE_SLEEP
    );

    delay(10);

    setLoRaFrequency(
        915000000
    );

    loraWriteRegister(
        REG_FIFO_TX_BASE_ADDR,
        0x00
    );

    loraWriteRegister(
        REG_FIFO_RX_BASE_ADDR,
        0x00
    );

    loraWriteRegister(
        REG_MODEM_CONFIG_1,
        0x72
    );

    loraWriteRegister(
        REG_MODEM_CONFIG_2,
        0x74
    );

    loraWriteRegister(
        REG_MODEM_CONFIG_3,
        0x04
    );

    loraWriteRegister(
        REG_PREAMBLE_MSB,
        0x00
    );

    loraWriteRegister(
        REG_PREAMBLE_LSB,
        0x08
    );

    loraWriteRegister(
        REG_PA_CONFIG,
        0x8F
    );

    loraWriteRegister(
        REG_PA_DAC,
        0x84
    );

    enterLoRaReceiveMode();

    loraReady = true;

    Serial.println(
        "LoRa ready"
    );

    return true;
}

bool sendLoRaPacket(
    const char* message
) {
    if (!loraReady) {
        return false;
    }

    uint8_t length =
        strlen(message);

    Serial.print(
        "LoRa sending: "
    );

    Serial.println(
        message
    );

    loraWriteRegister(
        REG_OP_MODE,
        MODE_LONG_RANGE_MODE |
        MODE_STDBY
    );

    loraWriteRegister(
        REG_FIFO_ADDR_PTR,
        0x00
    );

    for (
        uint8_t i = 0;
        i < length;
        i++
    ) {
        loraWriteRegister(
            REG_FIFO,
            message[i]
        );
    }

    loraWriteRegister(
        REG_PAYLOAD_LENGTH,
        length
    );

    loraWriteRegister(
        REG_DIO_MAPPING_1,
        0x40
    );

    loraWriteRegister(
        REG_IRQ_FLAGS,
        0xFF
    );

    loraWriteRegister(
        REG_OP_MODE,
        MODE_LONG_RANGE_MODE |
        MODE_TX
    );

    unsigned long start =
        millis();

    while (
        !(
            loraReadRegister(
                REG_IRQ_FLAGS
            ) &
            IRQ_TX_DONE_MASK
        )
    ) {
        if (
            millis() - start >
            3000
        ) {
            Serial.println(
                "LoRa TX TIMEOUT"
            );

            enterLoRaReceiveMode();

            return false;
        }

        delay(1);
    }

    loraWriteRegister(
        REG_IRQ_FLAGS,
        IRQ_TX_DONE_MASK
    );

    enterLoRaReceiveMode();

    Serial.println(
        "LoRa packet sent"
    );

    return true;
}

// ============================================================
// ALARM DISMISS
// ============================================================

void dismissAlarm(
    const char* reason,
    bool notifyRemote = true
) {
    if (!alarmRinging) {
        return;
    }

    alarmRinging = false;

    stopAlarmBeep();

    rtcClock.checkIfAlarm(1);

    previousMinute = 255;
    previousSecond = 255;

    forceFullRefresh = true;

    Serial.print(
        "ALARM DISMISSED - "
    );

    Serial.println(
        reason
    );

    if (
        notifyRemote &&
        loraReady
    ) {
        sendLoRaPacket(
            "ALARM_STOPPED"
        );
    }
}

// ============================================================
// LORA RECEIVE
// ============================================================

void handleLoRaMessage(
    const String& message
) {
    if (
        message ==
        "ALARM_OFF"
    ) {
        Serial.println(
            "REMOTE ALARM_OFF RECEIVED"
        );

        if (alarmRinging) {
            dismissAlarm(
                "LORA REMOTE",
                true
            );
        } else {
            sendLoRaPacket(
                "ALARM_STOPPED"
            );
        }
    }
}

void checkLoRa() {
    if (!loraReady) {
        return;
    }

    uint8_t flags =
        loraReadRegister(
            REG_IRQ_FLAGS
        );

    if (
        !(flags & IRQ_RX_DONE_MASK)
    ) {
        return;
    }

    loraWriteRegister(
        REG_IRQ_FLAGS,
        flags
    );

    if (
        flags &
        IRQ_PAYLOAD_CRC_ERROR
    ) {
        Serial.println(
            "LoRa CRC error"
        );

        return;
    }

    uint8_t length =
        loraReadRegister(
            REG_RX_NB_BYTES
        );

    uint8_t address =
        loraReadRegister(
            REG_FIFO_RX_CURRENT_ADDR
        );

    loraWriteRegister(
        REG_FIFO_ADDR_PTR,
        address
    );

    String message = "";

    for (
        uint8_t i = 0;
        i < length;
        i++
    ) {
        message +=
            (char)loraReadRegister(
                REG_FIFO
            );
    }

    int rssi =
        loraReadRegister(
            REG_RSSI_VALUE
        ) - 157;

    Serial.print(
        "LoRa received: "
    );

    Serial.print(
        message
    );

    Serial.print(
        " RSSI: "
    );

    Serial.print(
        rssi
    );

    Serial.println(
        " dBm"
    );

    handleLoRaMessage(
        message
    );
}

// ============================================================
// NFC CHECK
// ============================================================

void checkAlarmNFC() {
    if (
        !alarmRinging ||
        !nfcReady
    ) {
        return;
    }

    if (
        millis() -
        lastNfcPollMillis <
        NFC_POLL_INTERVAL
    ) {
        return;
    }

    lastNfcPollMillis =
        millis();

    uint8_t uid[7];
    uint8_t uidLength = 0;

    bool success =
        nfc.readPassiveTargetID(
            PN532_MIFARE_ISO14443A,
            uid,
            &uidLength,
            50
        );

    if (!success) {
        return;
    }

    Serial.print(
        "RFID detected: "
    );

    printUid(
        uid,
        uidLength
    );

    Serial.println();

    if (
        isAllowedCard(
            uid,
            uidLength
        )
    ) {
        Serial.println(
            "Authorized alarm card"
        );

        dismissAlarm(
            "RFID",
            true
        );
    } else {
        Serial.println(
            "Unauthorized RFID card"
        );
    }
}

// ============================================================
// SETUP
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println(
        "ESP32 ALARM CLOCK"
    );

    // RTC
    Wire.begin(
        RTC_SDA_PIN,
        RTC_SCL_PIN,
        100000
    );

    Serial.println(
        "RTC I2C started"
    );

    // NFC
    setupNFC();

    // Buttons
    pinMode(
        PLUS_BUTTON,
        INPUT_PULLUP
    );

    pinMode(
        MINUS_BUTTON,
        INPUT_PULLUP
    );

    pinMode(
        SET_BUTTON,
        INPUT_PULLUP
    );

    // E-ink
    pinMode(
        EINK_CS_PIN,
        OUTPUT
    );

    pinMode(
        EINK_DC_PIN,
        OUTPUT
    );

    pinMode(
        EINK_RES_PIN,
        OUTPUT
    );

    pinMode(
        EINK_BUSY_PIN,
        INPUT
    );

    digitalWrite(
        EINK_CS_PIN,
        HIGH
    );

    digitalWrite(
        EINK_DC_PIN,
        HIGH
    );

    digitalWrite(
        EINK_RES_PIN,
        HIGH
    );

    SPI.begin(
        EINK_SCK_SCL_PIN,
        -1,
        EINK_MOSI_SDA_PIN
    );

    Serial.println(
        "E-ink SPI started"
    );

    display.init(
        115200
    );

    display.setRotation(0);

    display.setTextColor(
        GxEPD_BLACK
    );

    Serial.println(
        "Display ready"
    );

    // RTC alarm
    loadAlarmFromRTC();

    Serial.println(
        "RTC ready"
    );

    // LoRa
    setupLoRa();

    // Audio
    setupAudio();

    Serial.println(
        "ESP32 Ready!"
    );
}

// ============================================================
// LOOP
// ============================================================

void loop() {
    DateTime now =
        RTClib::now(Wire);

    updateButton(
        setButton
    );

    updateButton(
        plusButton
    );

    updateButton(
        minusButton
    );

    checkLoRa();

    // ========================================================
    // ALARM TRIGGER
    // ========================================================

    if (
        !alarmRinging &&
        rtcClock.checkIfAlarm(1)
    ) {
        alarmRinging = true;

        Serial.println(
            "ALARM 1 TRIGGERED!"
        );

        startAlarmBeep();

        if (loraReady) {
            sendLoRaPacket(
                "ALARM_STARTED"
            );
        }

        drawAlarmRingingScreen(
            now
        );
    }

    // ========================================================
    // ALARM ACTIVE
    // ========================================================

    if (alarmRinging) {
        updateAlarmBeep();

        checkLoRa();

        if (!alarmRinging) {
            delay(1);
            return;
        }

        checkAlarmNFC();

        if (!alarmRinging) {
            delay(1);
            return;
        }

        if (
            setButton.shortPressed
        ) {
            dismissAlarm(
                "SET BUTTON",
                true
            );
        }

        delay(1);
        return;
    }

    // ========================================================
    // NORMAL CLOCK MODE
    // ========================================================

    if (
        clockMode ==
        NORMAL_MODE
    ) {
        // --------------------------------------------
        // Minute update
        // --------------------------------------------

        if (
            now.minute() !=
            previousMinute
        ) {
            if (
                forceFullRefresh ||
                minuteUpdatesSinceFullRefresh >=
                FULL_REFRESH_INTERVAL - 1
            ) {
                drawFullScreen(
                    now
                );

                minuteUpdatesSinceFullRefresh =
                    0;

                forceFullRefresh =
                    false;
            } else {
                drawNormalPartial(
                    now
                );

                minuteUpdatesSinceFullRefresh++;
            }

            previousMinute =
                now.minute();
        }

        // --------------------------------------------
        // Seconds progress bar
        // --------------------------------------------

        if (
            now.second() !=
            previousSecond
        ) {
            drawSecondBar(
                now.second()
            );

            previousSecond =
                now.second();
        }

        // --------------------------------------------
        // Enter clock setting
        // --------------------------------------------

        if (
            setButton.longPressed
        ) {
            enterClockSetting(
                now
            );
        }

        // --------------------------------------------
        // Enter alarm setting
        // --------------------------------------------

        else if (
            plusButton.longPressed
        ) {
            enterAlarmSetting();
        }

        // --------------------------------------------
        // Toggle alarm
        // --------------------------------------------

        else if (
            minusButton.longPressed
        ) {
            alarmEnabled =
                !alarmEnabled;

            if (alarmEnabled) {
                rtcClock.turnOnAlarm(
                    1
                );

                Serial.println(
                    "ALARM 1 ENABLED"
                );
            } else {
                rtcClock.turnOffAlarm(
                    1
                );

                Serial.println(
                    "ALARM 1 DISABLED"
                );
            }

            drawAlarmStatus();
        }
    }

    // ========================================================
    // SET HOUR
    // ========================================================

    else if (
        clockMode ==
        SET_HOUR_MODE
    ) {
        if (
            plusButton.shortPressed
        ) {
            settingHour++;

            if (
                settingHour > 23
            ) {
                settingHour = 0;
            }

            markSettingDisplayDirty();
        }

        if (
            minusButton.shortPressed
        ) {
            if (
                settingHour == 0
            ) {
                settingHour = 23;
            } else {
                settingHour--;
            }

            markSettingDisplayDirty();
        }

        if (
            setButton.shortPressed
        ) {
            settingDisplayDirty =
                false;

            clockMode =
                SET_MINUTE_MODE;

            display.setFont(
                DATE_FONT
            );

            drawCenteredText(
                "SET MIN",
                DATE_X1,
                DATE_Y1,
                DATE_X2,
                DATE_Y2
            );
        }
    }

    // ========================================================
    // SET MINUTE
    // ========================================================

    else if (
        clockMode ==
        SET_MINUTE_MODE
    ) {
        if (
            plusButton.shortPressed
        ) {
            settingMinute++;

            if (
                settingMinute > 59
            ) {
                settingMinute = 0;
            }

            markSettingDisplayDirty();
        }

        if (
            minusButton.shortPressed
        ) {
            if (
                settingMinute == 0
            ) {
                settingMinute = 59;
            } else {
                settingMinute--;
            }

            markSettingDisplayDirty();
        }

        if (
            setButton.shortPressed
        ) {
            settingDisplayDirty =
                false;

            clockMode =
                SET_DAY_MODE;

            drawSetDayLabel();
        }
    }

    // ========================================================
    // SET DAY
    // ========================================================

    else if (
        clockMode ==
        SET_DAY_MODE
    ) {
        uint8_t maxDay =
            daysInMonth(
                settingMonth,
                settingYear
            );

        if (
            plusButton.shortPressed
        ) {
            settingDay++;

            if (
                settingDay >
                maxDay
            ) {
                settingDay = 1;
            }

            markSettingDisplayDirty();
        }

        if (
            minusButton.shortPressed
        ) {
            if (
                settingDay <= 1
            ) {
                settingDay =
                    maxDay;
            } else {
                settingDay--;
            }

            markSettingDisplayDirty();
        }

        if (
            setButton.shortPressed
        ) {
            settingDisplayDirty =
                false;

            clockMode =
                SET_MONTH_MODE;

            drawSetMonthLabel();
        }
    }

    // ========================================================
    // SET MONTH
    // ========================================================

    else if (
        clockMode ==
        SET_MONTH_MODE
    ) {
        if (
            plusButton.shortPressed
        ) {
            settingMonth++;

            if (
                settingMonth > 12
            ) {
                settingMonth = 1;
            }

            clampSettingDay();

            markSettingDisplayDirty();
        }

        if (
            minusButton.shortPressed
        ) {
            if (
                settingMonth <= 1
            ) {
                settingMonth = 12;
            } else {
                settingMonth--;
            }

            clampSettingDay();

            markSettingDisplayDirty();
        }

        if (
            setButton.shortPressed
        ) {
            settingDisplayDirty =
                false;

            clockMode =
                SET_YEAR_MODE;

            drawSetYearLabel();
        }
    }

    // ========================================================
    // SET YEAR
    // ========================================================

    else if (
        clockMode ==
        SET_YEAR_MODE
    ) {
        if (
            plusButton.shortPressed
        ) {
            settingYear++;

            if (
                settingYear > 2099
            ) {
                settingYear = 2000;
            }

            clampSettingDay();

            markSettingDisplayDirty();
        }

        if (
            minusButton.shortPressed
        ) {
            if (
                settingYear <= 2000
            ) {
                settingYear = 2099;
            } else {
                settingYear--;
            }

            clampSettingDay();

            markSettingDisplayDirty();
        }

        if (
            setButton.shortPressed
        ) {
            settingDisplayDirty =
                false;

            DateTime newTime(
                settingYear,
                settingMonth,
                settingDay,
                settingHour,
                settingMinute,
                0
            );

            rtcClock.adjust(
                newTime
            );

            clockMode =
                NORMAL_MODE;

            previousMinute =
                255;

            previousSecond =
                255;

            forceFullRefresh =
                true;

            Serial.println(
                "CLOCK / DATE SAVED"
            );
        }
    }

    // ========================================================
    // SET ALARM HOUR
    // ========================================================

    else if (
        clockMode ==
        SET_ALARM_HOUR_MODE
    ) {
        if (
            plusButton.shortPressed
        ) {
            alarmHour++;

            if (
                alarmHour > 23
            ) {
                alarmHour = 0;
            }

            markSettingDisplayDirty();
        }

        if (
            minusButton.shortPressed
        ) {
            if (
                alarmHour == 0
            ) {
                alarmHour = 23;
            } else {
                alarmHour--;
            }

            markSettingDisplayDirty();
        }

        if (
            setButton.shortPressed
        ) {
            settingDisplayDirty =
                false;

            clockMode =
                SET_ALARM_MINUTE_MODE;

            display.setFont(
                DATE_FONT
            );

            drawCenteredText(
                "SET ALM MIN",
                DATE_X1,
                DATE_Y1,
                DATE_X2,
                DATE_Y2
            );
        }
    }

    // ========================================================
    // SET ALARM MINUTE
    // ========================================================

    else if (
        clockMode ==
        SET_ALARM_MINUTE_MODE
    ) {
        if (
            plusButton.shortPressed
        ) {
            alarmMinute++;

            if (
                alarmMinute > 59
            ) {
                alarmMinute = 0;
            }

            markSettingDisplayDirty();
        }

        if (
            minusButton.shortPressed
        ) {
            if (
                alarmMinute == 0
            ) {
                alarmMinute = 59;
            } else {
                alarmMinute--;
            }

            markSettingDisplayDirty();
        }

        if (
            setButton.shortPressed
        ) {
            settingDisplayDirty =
                false;

            saveAlarmToRTC();

            clockMode =
                NORMAL_MODE;

            previousMinute =
                255;

            previousSecond =
                255;

            forceFullRefresh =
                true;

            Serial.println(
                "ALARM TIME SAVED"
            );
        }
    }

    renderCurrentSettingValue();

    delay(1);
}