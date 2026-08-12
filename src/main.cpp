#include <Arduino.h>
#include <DS3231.h>
#include <GxEPD2_BW.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>

#include <InconsolataBold75pt7b.h>
#include <InconsolataBold48pt7b.h>
#include <InconsolataBold32pt7b.h>
#include <InconsolataBold24pt7b.h>

// ================= PIN DEFINITIONS ===============

// Buttons
#define PLUS_BUTTON 35
#define MINUS_BUTTON 47
#define SET_BUTTON 21

// I2C - DS3231
#define SDA_PIN 10
#define SCL_PIN 11

// E-ink
#define EINK_MOSI_SDA_PIN 15
#define EINK_SCK_SCL_PIN 17
#define EINK_CS_PIN 7
#define EINK_DC_PIN 6
#define EINK_RES_PIN 5
#define EINK_BUSY_PIN 4


// ================= DISPLAY REGIONS ===============

// Left hour digit
#define HOUR1_X1 20
#define HOUR1_Y1 27
#define HOUR1_X2 105
#define HOUR1_Y2 223

// Right hour digit
#define HOUR2_X1 100
#define HOUR2_Y1 27
#define HOUR2_X2 178
#define HOUR2_Y2 223

// Colon
#define COLON_X1 178
#define COLON_Y1 27
#define COLON_X2 222
#define COLON_Y2 223

// Left minute digit
#define MINUTE1_X1 222
#define MINUTE1_Y1 27
#define MINUTE1_X2 300
#define MINUTE1_Y2 223

// Right minute digit
#define MINUTE2_X1 295
#define MINUTE2_Y1 27
#define MINUTE2_X2 380
#define MINUTE2_Y2 223

// Date / setting status
#define DATE_X1 25
#define DATE_Y1 228
#define DATE_X2 375
#define DATE_Y2 275

// AM / PM
#define AMPM_X1 330
#define AMPM_Y1 25
#define AMPM_X2 375
#define AMPM_Y2 60

// Alarm indicator
#define ALARM_STATUS_X1 25
#define ALARM_STATUS_Y1 25
#define ALARM_STATUS_X2 95
#define ALARM_STATUS_Y2 60


// ================= FONTS ===============

#define TIME_FONT_XL &inconsolata_bold75pt7b
#define DATE_FONT &inconsolata_bold24pt7b


// ================= DATE NAMES ===============

const char* days[] = {
    "SUN",
    "MON",
    "TUE",
    "WED",
    "THU",
    "FRI",
    "SAT"
};

const char* months[] = {
    "JAN",
    "FEB",
    "MAR",
    "APR",
    "MAY",
    "JUN",
    "JUL",
    "AUG",
    "SEP",
    "OCT",
    "NOV",
    "DEC"
};


// ================= CLOCK STATE ===============

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


// ================= CLOCK SETTING VALUES ===============

uint8_t settingHour = 0;
uint8_t settingMinute = 0;
uint8_t settingDay = 1;
uint8_t settingMonth = 1;
uint16_t settingYear = 2026;


// ================= ALARM VALUES ===============

uint8_t alarmHour = 7;
uint8_t alarmMinute = 0;

bool alarmEnabled = false;


// ================= DISPLAY UPDATE STATE ===============

uint8_t previousMinute = 255;

const uint8_t FULL_REFRESH_INTERVAL = 3;

uint8_t minuteUpdatesSinceFullRefresh = 0;

bool forceFullRefresh = true;


// ================= BUTTON STATE ===============

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
    SET_BUTTON,
    HIGH,
    HIGH,
    0,
    0,
    false,
    false,
    false
};

ButtonState plusButton = {
    PLUS_BUTTON,
    HIGH,
    HIGH,
    0,
    0,
    false,
    false,
    false
};

ButtonState minusButton = {
    MINUS_BUTTON,
    HIGH,
    HIGH,
    0,
    0,
    false,
    false,
    false
};


// ================= RTC ===============

RTClib myRTC;
DS3231 rtcClock(Wire);


// ================= E-INK DISPLAY ===============

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


// ================= TIME HELPERS ===============

uint8_t to12Hour(uint8_t hour24) {

    uint8_t hour12 = hour24 % 12;

    if (hour12 == 0) {
        hour12 = 12;
    }

    return hour12;
}


String getAmPm(uint8_t hour24) {

    if (hour24 < 12) {
        return "AM";
    }

    return "PM";
}


String getDateString(DateTime now) {

    return String(days[now.dayOfTheWeek()]) + " " +
           String(now.day()) + " " +
           String(months[now.month() - 1]) + " " +
           String(now.year());
}


// ================= DATE HELPERS ===============

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

    uint8_t maxDay =
        daysInMonth(settingMonth, settingYear);

    if (settingDay > maxDay) {
        settingDay = maxDay;
    }
}


// ================= BUTTON HANDLING ===============

void updateButton(ButtonState &button) {

    button.shortPressed = false;
    button.longPressed = false;

    bool reading = digitalRead(button.pin);

    if (reading != button.lastReading) {
        button.lastDebounceTime = millis();
    }

    if (
        millis() - button.lastDebounceTime >=
        DEBOUNCE_DELAY
    ) {

        if (reading != button.stableState) {

            button.stableState = reading;

            if (button.stableState == LOW) {

                button.pressStartTime = millis();
                button.longPressTriggered = false;
            }

            else {

                if (!button.longPressTriggered) {
                    button.shortPressed = true;
                }
            }
        }
    }

    if (
        button.stableState == LOW &&
        !button.longPressTriggered &&
        millis() - button.pressStartTime >=
            LONG_PRESS_TIME
    ) {

        button.longPressed = true;
        button.longPressTriggered = true;
    }

    button.lastReading = reading;
}


// ================= DRAWING HELPERS ===============

void drawCenteredDigitToBuffer(
    char digit,
    int x1,
    int y1,
    int x2,
    int y2
) {

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

    int cursorX =
        x1 +
        ((x2 - x1 - textWidth) / 2) -
        textX;

    int cursorY =
        y1 +
        ((y2 - y1 - textHeight) / 2) -
        textY;

    display.setCursor(cursorX, cursorY);
    display.print(digit);
}


void drawCenteredTextToBuffer(
    String text,
    int x1,
    int y1,
    int x2,
    int y2
) {

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

    int cursorX =
        x1 +
        ((x2 - x1 - textWidth) / 2) -
        textX;

    int cursorY =
        y1 +
        ((y2 - y1 - textHeight) / 2) -
        textY;

    display.setCursor(cursorX, cursorY);
    display.print(text);
}


void drawCenteredDigit(
    char digit,
    int x1,
    int y1,
    int x2,
    int y2
) {

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

    int cursorX =
        x1 +
        ((x2 - x1 - textWidth) / 2) -
        textX;

    int cursorY =
        y1 +
        ((y2 - y1 - textHeight) / 2) -
        textY;

    int actualX = cursorX + textX;
    int actualY = cursorY + textY;

    int padding = 2;

    display.setPartialWindow(
        actualX - padding,
        actualY - padding,
        textWidth + padding * 2,
        textHeight + padding * 2
    );

    display.firstPage();

    do {

        display.fillScreen(GxEPD_WHITE);

        display.setCursor(
            cursorX,
            cursorY
        );

        display.print(digit);

    } while (display.nextPage());
}


void drawCenteredText(
    String text,
    int x1,
    int y1,
    int x2,
    int y2
) {

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

    int cursorX =
        x1 +
        ((x2 - x1 - textWidth) / 2) -
        textX;

    int cursorY =
        y1 +
        ((y2 - y1 - textHeight) / 2) -
        textY;

    display.setPartialWindow(
        x1,
        y1,
        x2 - x1,
        y2 - y1
    );

    display.firstPage();

    do {

        display.fillScreen(GxEPD_WHITE);

        display.setCursor(
            cursorX,
            cursorY
        );

        display.print(text);

    } while (display.nextPage());
}


// ================= BORDER DRAWING ===============

void drawBordersToBuffer() {

    display.drawLine(
        0, 25,
        400, 25,
        GxEPD_BLACK
    );

    display.drawLine(
        0, 275,
        400, 275,
        GxEPD_BLACK
    );

    display.drawLine(
        25, 0,
        25, 300,
        GxEPD_BLACK
    );

    display.drawLine(
        375, 0,
        375, 300,
        GxEPD_BLACK
    );

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


// ================= TIME DRAWING ===============

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

    char minute1 =
        '0' + (minute / 10);

    char minute2 =
        '0' + (minute % 10);

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


void drawTimeValues(
    uint8_t hour,
    uint8_t minute
) {

    drawHourValues(hour);
    drawMinuteValues(minute);
}


void drawTime(DateTime now) {

    drawTimeValues(
        now.hour(),
        now.minute()
    );
}


// ================= ALARM DISPLAY ===============

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


// ================= SETTING LABELS ===============

void drawSetDayLabel() {

    display.setFont(DATE_FONT);

    String text =
        "SET DAY " +
        String(settingDay);

    drawCenteredText(
        text,
        DATE_X1,
        DATE_Y1,
        DATE_X2,
        DATE_Y2
    );
}


void drawSetMonthLabel() {

    display.setFont(DATE_FONT);

    String text =
        "SET MONTH " +
        String(months[settingMonth - 1]);

    drawCenteredText(
        text,
        DATE_X1,
        DATE_Y1,
        DATE_X2,
        DATE_Y2
    );
}


void drawSetYearLabel() {

    display.setFont(DATE_FONT);

    String text =
        "SET YEAR " +
        String(settingYear);

    drawCenteredText(
        text,
        DATE_X1,
        DATE_Y1,
        DATE_X2,
        DATE_Y2
    );
}


// ================= FULL SCREEN DRAWING ===============

void drawFullScreen(DateTime now) {

    uint8_t hour12 =
        to12Hour(now.hour());

    char hour1 =
        '0' + (hour12 / 10);

    char hour2 =
        '0' + (hour12 % 10);

    char minute1 =
        '0' + (now.minute() / 10);

    char minute2 =
        '0' + (now.minute() % 10);

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

    } while (display.nextPage());

    Serial.println("FULL DISPLAY REFRESH");
}


// ================= NORMAL PARTIAL UPDATE ===============

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


// ================= ALARM RTC HELPERS ===============

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

    if (
        hour <= 23 &&
        minute <= 59
    ) {

        alarmHour = hour;
        alarmMinute = minute;
    }

    else {

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
    }

    else {

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


// ================= MODE ENTRY HELPERS ===============

void enterClockSetting(DateTime now) {

    settingHour = now.hour();
    settingMinute = now.minute();
    settingDay = now.day();
    settingMonth = now.month();
    settingYear = now.year();

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

    clockMode = SET_ALARM_HOUR_MODE;

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

    Serial.println("SET ALARM HOUR MODE");
}


// ================= SETUP ===============

void setup() {

    Serial.begin(115200);

    Wire.begin(
        SDA_PIN,
        SCL_PIN
    );

    SPI.begin(
        EINK_SCK_SCL_PIN,
        -1,
        EINK_MOSI_SDA_PIN,
        EINK_CS_PIN
    );

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

    display.init(115200);

    display.setRotation(0);

    display.setTextColor(
        GxEPD_BLACK
    );

    loadAlarmFromRTC();

    Serial.println("ESP32 Ready!");
}


// ================= LOOP ===============

void loop() {

    DateTime now = myRTC.now();

    updateButton(setButton);
    updateButton(plusButton);
    updateButton(minusButton);


    // ================= NORMAL MODE ===============

    if (clockMode == NORMAL_MODE) {

        if (now.minute() != previousMinute) {

            if (
                forceFullRefresh ||
                minuteUpdatesSinceFullRefresh >=
                    FULL_REFRESH_INTERVAL - 1
            ) {

                drawFullScreen(now);

                minuteUpdatesSinceFullRefresh = 0;
                forceFullRefresh = false;
            }

            else {

                drawNormalPartial(now);

                minuteUpdatesSinceFullRefresh++;
            }

            previousMinute =
                now.minute();
        }


        // ================= LONG SET - CLOCK SETTINGS ===============

        if (setButton.longPressed) {

            enterClockSetting(now);
        }


        // ================= LONG PLUS - ALARM SETTINGS ===============

        else if (plusButton.longPressed) {

            enterAlarmSetting();
        }


        // ================= LONG MINUS - TOGGLE ALARM ===============

        else if (minusButton.longPressed) {

            alarmEnabled = !alarmEnabled;

            if (alarmEnabled) {

                rtcClock.turnOnAlarm(1);

                Serial.println(
                    "ALARM 1 ENABLED"
                );
            }

            else {

                rtcClock.turnOffAlarm(1);

                Serial.println(
                    "ALARM 1 DISABLED"
                );
            }

            drawAlarmStatus();
        }
    }


    // ================= SET HOUR MODE ===============

    else if (clockMode == SET_HOUR_MODE) {

        if (plusButton.shortPressed) {

            settingHour++;

            if (settingHour > 23) {
                settingHour = 0;
            }

            drawHourValues(
                settingHour
            );
        }

        if (minusButton.shortPressed) {

            if (settingHour == 0) {
                settingHour = 23;
            }

            else {
                settingHour--;
            }

            drawHourValues(
                settingHour
            );
        }

        if (setButton.shortPressed) {

            clockMode =
                SET_MINUTE_MODE;

            display.setFont(DATE_FONT);

            drawCenteredText(
                "SET MIN",
                DATE_X1,
                DATE_Y1,
                DATE_X2,
                DATE_Y2
            );
        }
    }


    // ================= SET MINUTE MODE ===============

    else if (clockMode == SET_MINUTE_MODE) {

        if (plusButton.shortPressed) {

            settingMinute++;

            if (settingMinute > 59) {
                settingMinute = 0;
            }

            drawMinuteValues(
                settingMinute
            );
        }

        if (minusButton.shortPressed) {

            if (settingMinute == 0) {
                settingMinute = 59;
            }

            else {
                settingMinute--;
            }

            drawMinuteValues(
                settingMinute
            );
        }

        if (setButton.shortPressed) {

            clockMode =
                SET_DAY_MODE;

            drawSetDayLabel();
        }
    }


    // ================= SET DAY MODE ===============

    else if (clockMode == SET_DAY_MODE) {

        uint8_t maxDay =
            daysInMonth(
                settingMonth,
                settingYear
            );

        if (plusButton.shortPressed) {

            settingDay++;

            if (settingDay > maxDay) {
                settingDay = 1;
            }

            drawSetDayLabel();
        }

        if (minusButton.shortPressed) {

            if (settingDay <= 1) {
                settingDay = maxDay;
            }

            else {
                settingDay--;
            }

            drawSetDayLabel();
        }

        if (setButton.shortPressed) {

            clockMode =
                SET_MONTH_MODE;

            drawSetMonthLabel();
        }
    }


    // ================= SET MONTH MODE ===============

    else if (clockMode == SET_MONTH_MODE) {

        if (plusButton.shortPressed) {

            settingMonth++;

            if (settingMonth > 12) {
                settingMonth = 1;
            }

            clampSettingDay();

            drawSetMonthLabel();
        }

        if (minusButton.shortPressed) {

            if (settingMonth <= 1) {
                settingMonth = 12;
            }

            else {
                settingMonth--;
            }

            clampSettingDay();

            drawSetMonthLabel();
        }

        if (setButton.shortPressed) {

            clockMode =
                SET_YEAR_MODE;

            drawSetYearLabel();
        }
    }


    // ================= SET YEAR MODE ===============

    else if (clockMode == SET_YEAR_MODE) {

        if (plusButton.shortPressed) {

            settingYear++;

            if (settingYear > 2099) {
                settingYear = 2000;
            }

            clampSettingDay();

            drawSetYearLabel();
        }

        if (minusButton.shortPressed) {

            if (settingYear <= 2000) {
                settingYear = 2099;
            }

            else {
                settingYear--;
            }

            clampSettingDay();

            drawSetYearLabel();
        }

        if (setButton.shortPressed) {

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

            previousMinute = 255;

            forceFullRefresh = true;

            Serial.println(
                "CLOCK / DATE SAVED"
            );
        }
    }


    // ================= SET ALARM HOUR MODE ===============

    else if (
        clockMode ==
        SET_ALARM_HOUR_MODE
    ) {

        if (plusButton.shortPressed) {

            alarmHour++;

            if (alarmHour > 23) {
                alarmHour = 0;
            }

            drawHourValues(
                alarmHour
            );
        }

        if (minusButton.shortPressed) {

            if (alarmHour == 0) {
                alarmHour = 23;
            }

            else {
                alarmHour--;
            }

            drawHourValues(
                alarmHour
            );
        }

        if (setButton.shortPressed) {

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


    // ================= SET ALARM MINUTE MODE ===============

    else if (
        clockMode ==
        SET_ALARM_MINUTE_MODE
    ) {

        if (plusButton.shortPressed) {

            alarmMinute++;

            if (alarmMinute > 59) {
                alarmMinute = 0;
            }

            drawMinuteValues(
                alarmMinute
            );
        }

        if (minusButton.shortPressed) {

            if (alarmMinute == 0) {
                alarmMinute = 59;
            }

            else {
                alarmMinute--;
            }

            drawMinuteValues(
                alarmMinute
            );
        }

        if (setButton.shortPressed) {

            saveAlarmToRTC();

            clockMode =
                NORMAL_MODE;

            previousMinute = 255;

            forceFullRefresh = true;

            Serial.println(
                "ALARM TIME SAVED"
            );
        }
    }


    // ================= ALARM TRIGGER CHECK ===============

    if (rtcClock.checkIfAlarm(1)) {

        Serial.println(
            "ALARM 1 TRIGGERED!"
        );

        // Later:
        // start audio playback here.
    }


    delay(5);
}