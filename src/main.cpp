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

void createCustomChars() {
    lcd.createChar(0, chr_Clock);
    lcd.createChar(1, chr_First);
    lcd.createChar(2, chr_Second);
    lcd.createChar(3, chr_Faucet);
    lcd.createChar(4, chr_Calendar);

    lcd.createChar(10, chr_M);
    lcd.createChar(20, chr_T);
    lcd.createChar(30, chr_W);
    lcd.createChar(40, chr_F);
    lcd.createChar(50, chr_S);

    lcd.createChar(11, chr_M_inv);
    lcd.createChar(21, chr_T_inv);
    lcd.createChar(31, chr_W_inv);
    lcd.createChar(41, chr_F_inv);
    lcd.createChar(51, chr_S_inv);
}

// ===============================================================

void setup() {
    lcd.begin();
    createCustomChars();
    lcd.home();
}

void loop() {
    
}