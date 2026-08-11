#include <Arduino.h>
// #include <Adafruit_NeoPixel.h> // LED configuration
#include <DS3231.h> // RTC library
#include <GxEPD2_BW.h> // Eink Library
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <InconsolataBold75pt7b.h> // Display Fonts
#include <InconsolataBold48pt7b.h>
#include <InconsolataBold32pt7b.h>
#include <InconsolataBold24pt7b.h>

// =========================
// General pins
// #define NUM_PIXELS 1
#define PLUS_BUTTON 48
#define MINUS_BUTTON 47
#define SET_BUTTON 21
// I2C pins - DS3231 RTC
#define SDA_PIN 10
#define SCL_PIN 11
// E-ink pins
#define EINK_MOSI_SDA_PIN 15
#define EINK_SCK_SCL_PIN 17
#define EINK_CS_PIN 7
#define EINK_DC_PIN 6
#define EINK_RES_PIN 5
#define EINK_BUSY_PIN 4
// Display reigons
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
// Date region
#define DATE_X1 25
#define DATE_Y1 228
#define DATE_X2 375
#define DATE_Y2 275
// Fonts
#define TIME_FONT_XL &inconsolata_bold75pt7b
#define DATE_FONT &inconsolata_bold24pt7b
// =========================

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

// Initialise minute
uint8_t previousMinute = 255;

// RTC
RTClib myRTC;


// E-ink display
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

void drawDisplayBoarder() {
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        // Top Padding boundary
        //               xS  yS   xE   yE
        display.drawLine(0, 25, 400, 25, GxEPD_BLACK);
        // Bottom Padding Boundary
        display.drawLine(0, 275, 400, 275, GxEPD_BLACK);
        // LHS Padding Boundary
        display.drawLine(25, 0, 25, 300, GxEPD_BLACK);
        // RHS Padding Boundary
        display.drawLine(375, 0, 375, 300, GxEPD_BLACK);

        // draw horizontal 1/5 bottom segment
        for (int x = 25; x < 375; x += 5) {
            display.drawLine(x, 225, x + 2, 225, GxEPD_BLACK);
        }

        // Draw verticle middle dotted border
        for (int y = 25; y < 225; y += 5) {
            display.drawLine(200, y, 200, y + 2, GxEPD_BLACK);
        }
        // Draw verticle LHS middle dotted border
        for (int y = 25; y < 225; y += 5) {
            display.drawLine(288, y, 288, y + 2, GxEPD_BLACK);
        }

        // Draw verticle RHS middle dotted border
        for (int y = 25; y < 225; y += 5) {
            display.drawLine(112, y, 112, y + 2, GxEPD_BLACK);
        }

    } while (display.nextPage());
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

    // Actual visible character position
    int actualX = cursorX + textX;
    int actualY = cursorY + textY;

    // Small padding around the character
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

void drawTime(DateTime now) {

    uint8_t hour = now.hour();
    uint8_t minute = now.minute();

    char hour1 = '0' + (hour / 10);
    char hour2 = '0' + (hour % 10);

    char minute1 = '0' + (minute / 10);
    char minute2 = '0' + (minute % 10);

    display.setFont(TIME_FONT_XL);

    drawCenteredDigit(
        hour1,
        HOUR1_X1, HOUR1_Y1,
        HOUR1_X2, HOUR1_Y2
    );

    drawCenteredDigit(
        hour2,
        HOUR2_X1, HOUR2_Y1,
        HOUR2_X2, HOUR2_Y2
    );

    drawCenteredDigit(
        minute1,
        MINUTE1_X1, MINUTE1_Y1,
        MINUTE1_X2, MINUTE1_Y2
    );

    drawCenteredDigit(
        minute2,
        MINUTE2_X1, MINUTE2_Y1,
        MINUTE2_X2, MINUTE2_Y2
    );
}

void setup() {
    Serial.begin(115200);

    // Start I2C
    Wire.begin(SDA_PIN, SCL_PIN);
    
    // Start SPI
    SPI.begin(
        EINK_SCK_SCL_PIN,
        -1,
        EINK_MOSI_SDA_PIN,
        EINK_CS_PIN
    );

    // +, -, SET Buttons
    pinMode(PLUS_BUTTON, INPUT_PULLUP);
    pinMode(MINUS_BUTTON, INPUT_PULLUP);
    pinMode(SET_BUTTON, INPUT_PULLUP);

    // Init EINK Display
    display.init(115200);

    // Setup EINK Display
    display.setRotation(0);
    display.setFont(TIME_FONT_XL);
    display.setTextColor(GxEPD_BLACK);

    display.setFullWindow();
    drawDisplayBoarder();

    drawCenteredDigit(
        ':',
        COLON_X1, COLON_Y1,
        COLON_X2, COLON_Y2
    );

    Serial.println("ESP32 Ready!");
}

void loop() {

    // Read current date/time from RTC
    DateTime now = myRTC.now();

    // Only update the e-ink display when the minute changes
    if (now.minute() != previousMinute) {

        // =========================
        // Draw current time
        // =========================

        display.setFont(TIME_FONT_XL);
        drawTime(now);

        // =========================
        // Create current date string
        // =========================

        String currentDate =
            String(days[now.dayOfTheWeek()]) + " " +
            String(now.day()) + " " +
            String(months[now.month() - 1]) + " " +
            String(now.year());

        // =========================
        // Draw current date
        // =========================

        display.setFont(DATE_FONT);

        drawCenteredText(
            currentDate,
            DATE_X1,
            DATE_Y1,
            DATE_X2,
            DATE_Y2
        );

        // Remember which minute we just displayed
        previousMinute = now.minute();

        // Debug output
        Serial.print("Display updated: ");
        Serial.print(now.hour());
        Serial.print(":");

        if (now.minute() < 10) {
            Serial.print("0");
        }

        Serial.println(now.minute());
        Serial.println(currentDate);
    }


    // Check RTC approximately once per second
    delay(1000);
}

