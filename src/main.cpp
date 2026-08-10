#include <Arduino.h>
#include <Adafruit_NeoPixel.h> // LED configuration
#include <DS3231.h> // RTC library
#include <GxEPD2_BW.h> // Eink Library
#include <Wire.h>
#include <SPI.h>
#include <Fonts/FreeMonoBold24pt7b.h>// font for EINK?
#include <Adafruit_GFX.h>
// #include <JetBrainsMonoBold75pt7b.h>
#include <InconsolataBold75pt7b.h>

// =========================
// General pins
#define NUM_PIXELS 1
#define SET_BUTTON 21
#define LED_PIN 48
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
// =========================

// RTC
RTClib myRTC;

// LED
Adafruit_NeoPixel led(
    NUM_PIXELS,
    LED_PIN,
    NEO_GRB + NEO_KHZ800
);

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

        // // Hours: 1
        // display.setCursor(60, 145);
        // display.print("1");

        // // Hours: 0
        // display.setCursor(145, 145);
        // display.print("0");

        // // Minutes: 5
        // display.setCursor(230, 145);
        // display.print("5");

        // // Minutes: 6
        // display.setCursor(315, 145);
        // display.print("6");

    } while (display.nextPage());
}

void drawCenteredDigit(char digit, int x1, int y1, int x2, int y2) {

    int16_t textX, textY;
    uint16_t textWidth, textHeight;

    display.getTextBounds(
        String(digit),
        0,
        0,
        &textX,
        &textY,
        &textWidth,
        &textHeight
    );

    int cursorX = x1 + ((x2 - x1 - textWidth) / 2) - textX;
    int cursorY = y1 + ((y2 - y1 - textHeight) / 2) - textY;

    // Tell the display which area we want to update
    display.setPartialWindow(x1, y1, x2 - x1, y2 - y1);

    display.firstPage();

    do {
        display.setCursor(cursorX, cursorY);
        display.print(digit);

    } while (display.nextPage());
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

    // Button
    pinMode(SET_BUTTON, INPUT_PULLUP);

    // LED
    led.begin();
    led.clear();
    led.show();

    // Init EINK Display
    display.init(115200);

    // Setup EINK Display
    display.setRotation(0);
    display.setFont(&inconsolata_bold75pt7b);
    display.setTextColor(GxEPD_BLACK);

    // Draw Hello World
    display.setFullWindow();
    // display.firstPage();

    // do {
    //     display.fillScreen(GxEPD_WHITE);
    //     display.setCursor(0,15);
    //     display.print("Hello World! This is Xavier Jenkins E-Ink Display!");
    // } while (display.nextPage());

    drawDisplayBoarder();
    drawCenteredDigit('1', 25, 25, 112, 225);
    drawCenteredDigit('0', 112, 25, 200, 225);
    drawCenteredDigit('5', 200, 25, 288, 225);
    drawCenteredDigit('6', 288, 25, 375, 225);


    Serial.println("ESP32 Ready!");
}

void loop() {

    DateTime now = myRTC.now();
    String currentTime =    String(now.day()) + "/" +
                            String(now.month()) + "/" +
                            String(now.year()) + " " +
                            String(now.hour()) + ":" +
                            String(now.minute()) + ":" +
                            String(now.second());

    Serial.println(currentTime);

    Serial.print(" Since midnight 1/1/1970 = ");
    Serial.print(now.unixtime());
    Serial.print("s = ");
    Serial.print(now.unixtime() / 86400L);
    Serial.println("d");


    // display.setFullWindow();
    // display.firstPage();
    // do {
    //     display.fillScreen(GxEPD_WHITE);
    //     display.setCursor(0,15);
    //     display.print(currentTime);
    // } while (display.nextPage());


    if (digitalRead(SET_BUTTON) == LOW) {
        led.setPixelColor(0, led.Color(255, 0, 0)); // Red
        led.show();

        Serial.println("SET button pressed");
    } 
    else {
        led.clear();
        led.show();
    }

    delay(5000); // 5 seconds
}

