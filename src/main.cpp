#include <Arduino.h>
#include <customChars.h>
#include <pinout.h>
#include <LiquidCrystal_I2C.h>
#include <Time.h>
#include <DS1302RTC.h>
#include <OneButton.h>
#include <RotaryEncoder.h>


// Set the LCD address to 0x27 in PCF8574 by NXP and Set to 0x3F in PCF8574A by Ti
LiquidCrystal_I2C lcd(0x27, 16, 2);
// Set RTC module
DS1302RTC rtc(RTC_RST, RTC_DAT, RTC_CLK);



// ===============================================================

void setup() {
    lcd.begin();
    // lcd.createChar(0, customChar);
    lcd.home();
}

void loop() {
    
}