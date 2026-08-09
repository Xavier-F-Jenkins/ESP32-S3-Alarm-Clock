#include <Arduino.h>
#include <Adafruit_NeoPixel.h> // LED configuration
#include <DS3231.h> // RTC library
#include <GxEPD2_BW.h> // Eink Library
#include <Wire.h>
#include <SPI.h>
#include <Fonts/FreeMonoBold9pt7b.h> // font for EINK?

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
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_BLACK);

    // Draw Hello World
    display.setFullWindow();
    display.firstPage();

    do {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(0,15);
        display.print("Hello World! This is Xavier Jenkins E-Ink Display!");
    } while (display.nextPage());

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

    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(0,15);
        display.print(currentTime);
    } while (display.nextPage());


    if (digitalRead(SET_BUTTON) == LOW) {
        led.setPixelColor(0, led.Color(255, 0, 0)); // Red
        led.show();

        Serial.println("SET button pressed");
    } 
    else {
        led.clear();
        led.show();
    }

    delay(15000); // 15 seconds
}