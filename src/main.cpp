#include <Arduino.h>
#include <Adafruit_NeoPixel.h> // LED configuration
#include <DS3231.h> // RTC library
#include <Wire.h>

#define NUM_PIXELS 1
#define SET_BUTTON 21
#define LED_PIN 48
#define SDA_PIN 10 //I2C RTC
#define SCL_PIN 11 //I2C RTC

RTClib myRTC;

Adafruit_NeoPixel led(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    Serial.begin(115200);

    Wire.begin(SDA_PIN, SCL_PIN);
    pinMode(SET_BUTTON, INPUT_PULLUP);

    led.begin();
    led.clear();
    led.show();

    Serial.println("ESP32 Ready!");
}

void loop() {

    delay(1000);

    DateTime now = myRTC.now();
    
    Serial.print(now.day(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.year(), DEC);
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();

    Serial.print(" Since midnight 1/1/1970 = ");
    Serial.print(now.unixtime());
    Serial.print("s = ");
    Serial.print(now.unixtime() / 86400L);
    Serial.println("d");
    


    if (digitalRead(SET_BUTTON) == LOW) {
        led.setPixelColor(0, led.Color(255, 0, 0)); // Red
        led.show();

        Serial.println("SET button pressed");
    } 
    else {
        led.clear();
        led.show();
    }


}