#include <Arduino.h>
#include <DS3231.h>
#include <GxEPD2_BW.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <InconsolataBold75pt7b.h>
#include <InconsolataBold24pt7b.h>

// =====================================================
// PIN DEFINITIONS
// =====================================================

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


// =====================================================
// DISPLAY REGIONS
// =====================================================

// Hour digit 1
#define HOUR1_X1 20
#define HOUR1_Y1 27
#define HOUR1_X2 105
#define HOUR1_Y2 223

// Hour digit 2
#define HOUR2_X1 100
#define HOUR2_Y1 27
#define HOUR2_X2 178
#define HOUR2_Y2 223

// Colon
#define COLON_X1 178
#define COLON_Y1 27
#define COLON_X2 222
#define COLON_Y2 223

// Minute digit 1
#define MINUTE1_X1 222
#define MINUTE1_Y1 27
#define MINUTE1_X2 300
#define MINUTE1_Y2 223

// Minute digit 2
#define MINUTE2_X1 295
#define MINUTE2_Y1 27
#define MINUTE2_X2 380
#define MINUTE2_Y2 223

// Date / setting-status area
#define DATE_X1 25
#define DATE_Y1 228
#define DATE_X2 375
#define DATE_Y2 275

// AM / PM area
#define AMPM_X1 330
#define AMPM_Y1 25
#define AMPM_X2 375
#define AMPM_Y2 60


// =====================================================
// FONTS
// =====================================================

#define TIME_FONT_XL &inconsolata_bold75pt7b
#define DATE_FONT &inconsolata_bold24pt7b


// =====================================================
// DATE NAMES
// =====================================================

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


// =====================================================
// CLOCK STATE
// =====================================================

enum ClockMode {
    NORMAL_MODE,
    SET_HOUR_MODE,
    SET_MINUTE_MODE
};

ClockMode clockMode = NORMAL_MODE;

uint8_t settingHour = 0;
uint8_t settingMinute = 0;

uint8_t previousMinute = 255;


// =====================================================
// FULL REFRESH CONTROL
// =====================================================

// Full refresh every 3 minute changes
const uint8_t FULL_REFRESH_INTERVAL = 3;

uint8_t minuteUpdatesSinceFullRefresh = 0;

// Forces a full refresh when the clock first starts
// or after changing the RTC time.
bool forceFullRefresh = true;


// =====================================================
// BUTTON DEBOUNCE
// =====================================================

const unsigned long DEBOUNCE_DELAY = 40;

// SET
bool lastSetReading = HIGH;
bool stableSetState = HIGH;
unsigned long lastSetDebounceTime = 0;

// PLUS
bool lastPlusReading = HIGH;
bool stablePlusState = HIGH;
unsigned long lastPlusDebounceTime = 0;

// MINUS
bool lastMinusReading = HIGH;
bool stableMinusState = HIGH;
unsigned long lastMinusDebounceTime = 0;


// =====================================================
// RTC
// =====================================================

RTClib myRTC;
DS3231 rtcClock(Wire);


// =====================================================
// E-INK DISPLAY
// =====================================================

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


// =====================================================
// TIME HELPERS
// =====================================================

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


// =====================================================
// BUTTON HELPER
// =====================================================

bool buttonPressed(
    uint8_t pin,
    bool &lastReading,
    bool &stableState,
    unsigned long &lastDebounceTime
) {

    bool reading = digitalRead(pin);

    // The raw reading changed.
    // Restart the debounce timer.
    if (reading != lastReading) {
        lastDebounceTime = millis();
    }

    bool pressed = false;

    // Only accept the new state after it has remained
    // stable for the debounce period.
    if ((millis() - lastDebounceTime) >= DEBOUNCE_DELAY) {

        if (reading != stableState) {

            stableState = reading;

            // INPUT_PULLUP means LOW = pressed
            if (stableState == LOW) {
                pressed = true;
            }
        }
    }

    lastReading = reading;

    return pressed;
}


// =====================================================
// LOW-LEVEL DRAWING FUNCTIONS
//
// These only draw into the CURRENT display buffer.
// They do not initiate their own refresh.
// =====================================================

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


void drawBordersToBuffer() {

    // Outer borders
    display.drawLine(0, 25, 400, 25, GxEPD_BLACK);
    display.drawLine(0, 275, 400, 275, GxEPD_BLACK);
    display.drawLine(25, 0, 25, 300, GxEPD_BLACK);
    display.drawLine(375, 0, 375, 300, GxEPD_BLACK);

    // Horizontal dotted separator
    for (int x = 25; x < 375; x += 5) {
        display.drawLine(
            x,
            225,
            x + 2,
            225,
            GxEPD_BLACK
        );
    }

    // Middle dotted line
    for (int y = 25; y < 225; y += 5) {
        display.drawLine(
            200,
            y,
            200,
            y + 2,
            GxEPD_BLACK
        );
    }

    // Minute separator
    for (int y = 25; y < 225; y += 5) {
        display.drawLine(
            288,
            y,
            288,
            y + 2,
            GxEPD_BLACK
        );
    }

    // Hour separator
    for (int y = 25; y < 225; y += 5) {
        display.drawLine(
            112,
            y,
            112,
            y + 2,
            GxEPD_BLACK
        );
    }
}


// =====================================================
// PARTIAL REFRESH FUNCTIONS
// =====================================================

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


// =====================================================
// TIME DRAWING
// =====================================================

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

    // AM / PM may also change when adjusting hour
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


void drawTimeValues(
    uint8_t hour24,
    uint8_t minute
) {

    drawHourValues(hour24);
    drawMinuteValues(minute);
}


void drawTime(DateTime now) {

    drawTimeValues(
        now.hour(),
        now.minute()
    );
}


// =====================================================
// NORMAL PARTIAL CLOCK UPDATE
// =====================================================

void drawNormalPartial(DateTime now) {

    display.setFont(TIME_FONT_XL);

    drawTime(now);

    display.setFont(DATE_FONT);

    drawCenteredText(
        getAmPm(now.hour()),
        AMPM_X1,
        AMPM_Y1,
        AMPM_X2,
        AMPM_Y2
    );

    drawCenteredText(
        getDateString(now),
        DATE_X1,
        DATE_Y1,
        DATE_X2,
        DATE_Y2
    );
}


// =====================================================
// FULL SCREEN REFRESH
// =====================================================

void drawFullScreen(DateTime now) {

    uint8_t hour12 = to12Hour(now.hour());

    char hour1 = '0' + (hour12 / 10);
    char hour2 = '0' + (hour12 % 10);

    char minute1 = '0' + (now.minute() / 10);
    char minute2 = '0' + (now.minute() % 10);

    display.setFullWindow();

    display.firstPage();

    do {

        // Completely clear the screen
        display.fillScreen(GxEPD_WHITE);

        // Borders
        drawBordersToBuffer();

        // -------------------------
        // Large time
        // -------------------------

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

        // -------------------------
        // AM / PM and date
        // -------------------------

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

    } while (display.nextPage());

    Serial.println("FULL DISPLAY REFRESH");
}


// =====================================================
// SETUP
// =====================================================

void setup() {

    Serial.begin(115200);

    // -------------------------
    // I2C
    // -------------------------

    Wire.begin(
        SDA_PIN,
        SCL_PIN
    );

    // -------------------------
    // SPI
    // -------------------------

    SPI.begin(
        EINK_SCK_SCL_PIN,
        -1,
        EINK_MOSI_SDA_PIN,
        EINK_CS_PIN
    );

    // -------------------------
    // Buttons
    // -------------------------

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

    // -------------------------
    // Display
    // -------------------------

    display.init(115200);

    display.setRotation(0);

    display.setTextColor(
        GxEPD_BLACK
    );

    Serial.println("ESP32 Ready!");
}


// =====================================================
// LOOP
// =====================================================

void loop() {

    DateTime now = myRTC.now();


    // =================================================
    // READ / DEBOUNCE BUTTONS
    // =================================================

    bool setPressed = buttonPressed(
        SET_BUTTON,
        lastSetReading,
        stableSetState,
        lastSetDebounceTime
    );

    bool plusPressed = buttonPressed(
        PLUS_BUTTON,
        lastPlusReading,
        stablePlusState,
        lastPlusDebounceTime
    );

    bool minusPressed = buttonPressed(
        MINUS_BUTTON,
        lastMinusReading,
        stableMinusState,
        lastMinusDebounceTime
    );


    // =================================================
    // NORMAL MODE
    // =================================================

    if (clockMode == NORMAL_MODE) {

        // Only update when minute changes
        if (now.minute() != previousMinute) {

            // -----------------------------------------
            // Full refresh every 3 minute changes
            // -----------------------------------------

            if (
                forceFullRefresh ||
                minuteUpdatesSinceFullRefresh >=
                    (FULL_REFRESH_INTERVAL - 1)
            ) {

                drawFullScreen(now);

                minuteUpdatesSinceFullRefresh = 0;
                forceFullRefresh = false;
            }

            // -----------------------------------------
            // Otherwise partial update
            // -----------------------------------------

            else {

                drawNormalPartial(now);

                minuteUpdatesSinceFullRefresh++;
            }

            previousMinute = now.minute();


            Serial.print("Display updated: ");
            Serial.print(to12Hour(now.hour()));
            Serial.print(":");

            if (now.minute() < 10) {
                Serial.print("0");
            }

            Serial.print(now.minute());
            Serial.print(" ");
            Serial.println(getAmPm(now.hour()));
        }


        // -----------------------------------------
        // Enter SET HOUR mode
        // -----------------------------------------

        if (setPressed) {

            settingHour = now.hour();
            settingMinute = now.minute();

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
    }


    // =================================================
    // SET HOUR MODE
    // =================================================

    else if (clockMode == SET_HOUR_MODE) {

        // -----------------------------------------
        // Increase hour
        // -----------------------------------------

        if (plusPressed) {

            settingHour++;

            if (settingHour > 23) {
                settingHour = 0;
            }

            // Only redraw the hour + AM/PM,
            // not all four digits.
            drawHourValues(settingHour);

            Serial.print("Setting hour: ");
            Serial.print(to12Hour(settingHour));
            Serial.print(" ");
            Serial.println(getAmPm(settingHour));
        }


        // -----------------------------------------
        // Decrease hour
        // -----------------------------------------

        if (minusPressed) {

            if (settingHour == 0) {
                settingHour = 23;
            }
            else {
                settingHour--;
            }

            drawHourValues(settingHour);

            Serial.print("Setting hour: ");
            Serial.print(to12Hour(settingHour));
            Serial.print(" ");
            Serial.println(getAmPm(settingHour));
        }


        // -----------------------------------------
        // Move to SET MINUTE mode
        // -----------------------------------------

        if (setPressed) {

            clockMode = SET_MINUTE_MODE;

            display.setFont(DATE_FONT);

            drawCenteredText(
                "SET MIN",
                DATE_X1,
                DATE_Y1,
                DATE_X2,
                DATE_Y2
            );

            Serial.println("SET MINUTE MODE");
        }
    }


    // =================================================
    // SET MINUTE MODE
    // =================================================

    else if (clockMode == SET_MINUTE_MODE) {

        // -----------------------------------------
        // Increase minute
        // -----------------------------------------

        if (plusPressed) {

            settingMinute++;

            if (settingMinute > 59) {
                settingMinute = 0;
            }

            // Only redraw minute digits
            drawMinuteValues(settingMinute);

            Serial.print("Setting minute: ");
            Serial.println(settingMinute);
        }


        // -----------------------------------------
        // Decrease minute
        // -----------------------------------------

        if (minusPressed) {

            if (settingMinute == 0) {
                settingMinute = 59;
            }
            else {
                settingMinute--;
            }

            drawMinuteValues(settingMinute);

            Serial.print("Setting minute: ");
            Serial.println(settingMinute);
        }


        // -----------------------------------------
        // Save time
        // -----------------------------------------

        if (setPressed) {

            DateTime newTime(
                now.year(),
                now.month(),
                now.day(),
                settingHour,
                settingMinute,
                0
            );

            rtcClock.adjust(newTime);

            clockMode = NORMAL_MODE;

            // Force the next normal update
            previousMinute = 255;

            // Do a complete refresh after setting
            // the clock so SET MIN disappears and
            // any accumulated ghosting is cleared.
            forceFullRefresh = true;

            Serial.println("TIME SAVED");
        }
    }


    // Fast loop is okay now because the buttons
    // are debounced using millis().
    delay(5);
}