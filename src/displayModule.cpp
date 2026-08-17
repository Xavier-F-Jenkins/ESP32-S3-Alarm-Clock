#include "displayModule.h"

#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>

#include <InconsolataBold75pt7b.h>
#include <InconsolataBold24pt7b.h>

#include "config.h"
#include "appState.h"
#include "rtcModule.h"

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

#define SECOND_BAR_X 25
#define SECOND_BAR_Y 285
#define SECOND_BAR_WIDTH 350
#define SECOND_BAR_HEIGHT 10

#define TIME_FONT_XL &inconsolata_bold75pt7b
#define DATE_FONT &inconsolata_bold24pt7b

static GxEPD2_BW<
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
// PRIVATE DRAW HELPERS
// ============================================================

static void drawCenteredDigitToBuffer(char digit, int x1, int y1, int x2, int y2) {
    int16_t textX, textY;
    uint16_t textWidth, textHeight;

    String text = String(digit);

    display.getTextBounds(text, 0, 0, &textX, &textY, &textWidth, &textHeight);

    int cursorX = x1 + ((x2 - x1 - textWidth) / 2) - textX;
    int cursorY = y1 + ((y2 - y1 - textHeight) / 2) - textY;

    display.setCursor(cursorX, cursorY);
    display.print(digit);
}

static void drawCenteredTextToBuffer(String text, int x1, int y1, int x2, int y2) {
    int16_t textX, textY;
    uint16_t textWidth, textHeight;

    display.getTextBounds(text, 0, 0, &textX, &textY, &textWidth, &textHeight);

    int cursorX = x1 + ((x2 - x1 - textWidth) / 2) - textX;
    int cursorY = y1 + ((y2 - y1 - textHeight) / 2) - textY;

    display.setCursor(cursorX, cursorY);
    display.print(text);
}

static void drawCenteredDigit(char digit, int x1, int y1, int x2, int y2) {
    int16_t textX, textY;
    uint16_t textWidth, textHeight;

    String text = String(digit);

    display.getTextBounds(text, 0, 0, &textX, &textY, &textWidth, &textHeight);

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

static void drawCenteredText(String text, int x1, int y1, int x2, int y2) {
    int16_t textX, textY;
    uint16_t textWidth, textHeight;

    display.getTextBounds(text, 0, 0, &textX, &textY, &textWidth, &textHeight);

    int cursorX = x1 + ((x2 - x1 - textWidth) / 2) - textX;
    int cursorY = y1 + ((y2 - y1 - textHeight) / 2) - textY;

    display.setPartialWindow(x1, y1, x2 - x1, y2 - y1);
    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(cursorX, cursorY);
        display.print(text);
    } while (display.nextPage());
}

static void drawBordersToBuffer() {
    display.drawLine(0, 25, 400, 25, GxEPD_BLACK);
    display.drawLine(0, 275, 400, 275, GxEPD_BLACK);
    display.drawLine(25, 0, 25, 300, GxEPD_BLACK);
    display.drawLine(375, 0, 375, 300, GxEPD_BLACK);

    for (int x = 25; x < 375; x += 5) {
        display.drawLine(x, 225, x + 2, 225, GxEPD_BLACK);
    }

    for (int y = 25; y < 225; y += 5) {
        display.drawLine(200, y, 200, y + 2, GxEPD_BLACK);
        display.drawLine(288, y, 288, y + 2, GxEPD_BLACK);
        display.drawLine(112, y, 112, y + 2, GxEPD_BLACK);
    }
}

static void drawSecondBarToBuffer(uint8_t second) {
    int filledWidth = map(second, 0, 59, 0, SECOND_BAR_WIDTH);

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

// ============================================================
// PUBLIC FUNCTIONS
// ============================================================

void setupDisplay() {
    pinMode(EINK_CS_PIN, OUTPUT);
    pinMode(EINK_DC_PIN, OUTPUT);
    pinMode(EINK_RES_PIN, OUTPUT);
    pinMode(EINK_BUSY_PIN, INPUT);

    digitalWrite(EINK_CS_PIN, HIGH);
    digitalWrite(EINK_DC_PIN, HIGH);
    digitalWrite(EINK_RES_PIN, HIGH);

    SPI.begin(EINK_SCK_SCL_PIN, -1, EINK_MOSI_SDA_PIN);

    Serial.println("E-ink SPI started");

    display.init(115200);
    display.setRotation(0);
    display.setTextColor(GxEPD_BLACK);

    Serial.println("Display ready");
}

void drawHourValues(uint8_t hour24) {
    uint8_t hour = to12Hour(hour24);

    char hour1 = '0' + (hour / 10);
    char hour2 = '0' + (hour % 10);

    display.setFont(TIME_FONT_XL);

    drawCenteredDigit(hour1, HOUR1_X1, HOUR1_Y1, HOUR1_X2, HOUR1_Y2);
    drawCenteredDigit(hour2, HOUR2_X1, HOUR2_Y1, HOUR2_X2, HOUR2_Y2);

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

void drawTime(const DateTime& now) {
    drawTimeValues(now.hour(), now.minute());
}

void drawAlarmRingingScreen(const DateTime& now) {
    drawTime(now);

    display.setFont(DATE_FONT);

    drawCenteredText("WAKE UP!", 100, 25, 300, 60);
    drawCenteredText("WAKE UP!", DATE_X1, DATE_Y1, DATE_X2, DATE_Y2);
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

void drawStatusText(const String& text) {
    display.setFont(DATE_FONT);

    drawCenteredText(
        text,
        DATE_X1,
        DATE_Y1,
        DATE_X2,
        DATE_Y2
    );
}

void drawSetDayLabel() {
    drawStatusText("SET DAY " + String(settingDay));
}

void drawSetMonthLabel() {
    drawStatusText("SET MONTH " + getMonthName(settingMonth));
}

void drawSetYearLabel() {
    drawStatusText("SET YEAR " + String(settingYear));
}

void drawSecondBar(uint8_t second) {
    int filledWidth = map(second, 0, 59, 0, SECOND_BAR_WIDTH);

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

void drawFullScreen(const DateTime& now) {
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

        drawCenteredDigitToBuffer(hour1, HOUR1_X1, HOUR1_Y1, HOUR1_X2, HOUR1_Y2);
        drawCenteredDigitToBuffer(hour2, HOUR2_X1, HOUR2_Y1, HOUR2_X2, HOUR2_Y2);
        drawCenteredDigitToBuffer(':', COLON_X1, COLON_Y1, COLON_X2, COLON_Y2);

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

        drawSecondBarToBuffer(now.second());

    } while (display.nextPage());

    previousSecond = now.second();

    Serial.println("FULL DISPLAY REFRESH");
}

void drawNormalPartial(const DateTime& now) {
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

void markSettingDisplayDirty() {
    settingDisplayDirty = true;
    lastSettingInputMillis = millis();
}

void renderCurrentSettingValue() {
    if (!settingDisplayDirty) return;

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