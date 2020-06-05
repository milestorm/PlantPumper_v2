#include <Arduino.h>
#include <customChars.h>
#include <pinout.h>
#include <LiquidCrystal_I2C.h>
#include <Time.h>
#include <DS1302RTC.h>
#include <OneButton.h>
#include <RotaryEncoder.h>
#include <EEPROM.h>


// Set the LCD address to 0x27 in PCF8574 by NXP and Set to 0x3F in PCF8574A by Ti
LiquidCrystal_I2C lcd(0x27, 16, 2);
// Set RTC module
DS1302RTC rtc(RTC_RST, RTC_DAT, RTC_CLK);
// Set Rotary Encoder
RotaryEncoder encoder(ROTARYENCODER_PIN1, ROTARYENCODER_PIN2);

// initializes two sets of variables used in the timers
int startHour_1 = 0;
int startMinute_1 = 0;
int duration_1 = 0;
int isOn_1 = 0;
int calendar_1[7] = {0, 0, 0, 0, 0, 0, 0};

int startHour_2 = 0;
int startMinute_2 = 0;
int duration_2 = 0;
int isOn_2 = 0;
int calendar_2[7] = {0, 0, 0, 0, 0, 0, 0};

int convertedCalendar[7];

/**
 * inits custom characters for LCD
**/
void createCustomChars() {
    lcd.createChar(0, chr_Clock);
    lcd.createChar(1, chr_First);
    lcd.createChar(2, chr_Second);
    lcd.createChar(3, chr_Faucet);
    lcd.createChar(4, chr_Calendar);
    lcd.createChar(5, chr_Drop);

    lcd.createChar(10, chr_M);
    lcd.createChar(11, chr_T);
    lcd.createChar(12, chr_W);
    lcd.createChar(13, chr_T);
    lcd.createChar(14, chr_F);
    lcd.createChar(15, chr_S);
    lcd.createChar(16, chr_S);

    lcd.createChar(20, chr_M_inv);
    lcd.createChar(21, chr_T_inv);
    lcd.createChar(22, chr_W_inv);
    lcd.createChar(23, chr_T_inv);
    lcd.createChar(24, chr_F_inv);
    lcd.createChar(25, chr_S_inv);
    lcd.createChar(26, chr_S_inv);
}

/**
 * reads settings from eeprom
 * if the values are not valid, 0 is applied
**/
void readEEPROMSettings() {
    // read values for first timer
    startHour_1 = EEPROM.read(0);
    if (startHour_1 > 24) { startHour_1 = 0; }
    startMinute_1 = EEPROM.read(1);
    if (startMinute_1 > 60) { startMinute_1 = 0; }
    duration_1 = EEPROM.read(2);
    if (duration_1 > 60) { duration_1 = 0; }
    isOn_1 = EEPROM.read(3);
    if (isOn_1 > 2) { isOn_1 = 0; }
    for (int i = 0; i < 7; i++) {
        int addr = 4;
        calendar_1[i] = EEPROM.read(addr);
        if (calendar_1[i] > 2) { calendar_1[i] = 0; }
        addr++;
    }

    // read values for second timer
    startHour_2 = EEPROM.read(20);
    if (startHour_2 > 24) { startHour_2 = 0; }
    startMinute_2 = EEPROM.read(21);
    if (startMinute_2 > 60) { startMinute_2 = 0; }
    duration_2 = EEPROM.read(22);
    if (duration_2 > 60) { duration_2 = 0; }
    isOn_2 = EEPROM.read(23);
    if (isOn_2 > 2) { isOn_2 = 0; }
    for (int i = 0; i < 7; i++) {
        int addr = 24;
        calendar_2[i] = EEPROM.read(addr);
        if (calendar_2[i] > 2) { calendar_2[i] = 0; }
        addr++;
    }

}

void updateEEPROMSettings(int id) {
    switch (id) {
    case 1:
        // update values for first timer
        EEPROM.update(0, startHour_1);
        EEPROM.update(1, startMinute_1);
        EEPROM.update(2, duration_1);
        EEPROM.update(3, isOn_1);
        for (int i = 0; i < 7; i++) {
            int addr = 4;
            EEPROM.update(addr, calendar_1[i]);
            addr++;
        }
        break;

    case 2:
        // update values for second timer
        EEPROM.update(20, startHour_2);
        EEPROM.update(21, startMinute_2);
        EEPROM.update(22, duration_2);
        EEPROM.update(23, isOn_2);
        for (int i = 0; i < 7; i++) {
            int addr = 24;
            EEPROM.update(addr, calendar_2[i]);
            addr++;
        }
        break;

    default:
        break;
    }
}

/**
 * converts number to string with leading zero for 0-9
**/
String to2digits(int number) {
    String res = String(number);
    if (number >= 0 && number < 10)
        res = "0" + res;
    return res;
}

/**
 * converts calendar array of 1 and 0 to custom character values
**/
void convertCalendar(int *calendarArray) {
    for (int i = 0; i < 7; i++) {
        if (calendarArray[i] == 0) {
            convertedCalendar[i] = 10 + calendarArray[i];
        } else {
            convertedCalendar[i] = 20 + calendarArray[i];
        }
    }
}

void printTimerValuesToLCD(int timerId) {
	char buffer[50];

    lcd.clear();

    switch (timerId) {
    case 1:
        // first line of lcd
        convertCalendar(calendar_1);

        lcd.write(1); // number 1 pump symbol
        lcd.setCursor(2, 0);
        lcd.write(0); // clock symbol
        lcd.setCursor(3, 0);
        snprintf(buffer, 50, "%s:%s    %ss", to2digits(startHour_1), to2digits(startMinute_1), to2digits(duration_1));
        lcd.print(buffer);
        lcd.setCursor(11, 0);
        lcd.write(3); // faucet symbol

        // second line of lcd
        lcd.setCursor(2, 1);
        lcd.write(4); // calendar symbol
        for (int i = 0; i < 7; i++) { // prints calendar
            lcd.setCursor(3 + i, 1);
            lcd.write(convertedCalendar[i]);
        }
        lcd.setCursor(11, 1);
        if (isOn_1 == 1) { // prints enabled state of current pump
            lcd.print("ON");
        } else {
            lcd.print("OFF");
        }

        break;

    case 2:
        // second line of lcd
        convertCalendar(calendar_2);

        lcd.write(1); // number 1 pump symbol
        lcd.setCursor(2, 0);
        lcd.write(0); // clock symbol
        lcd.setCursor(3, 0);
        snprintf(buffer, 50, "%s:%s    %ss", to2digits(startHour_2), to2digits(startMinute_2), to2digits(duration_2));
        lcd.print(buffer);
        lcd.setCursor(11, 0);
        lcd.write(3); // faucet symbol

        // second line of lcd
        lcd.setCursor(2, 1);
        lcd.write(4); // calendar symbol
        for (int i = 0; i < 7; i++) { // prints calendar
            lcd.setCursor(3 + i, 1);
            lcd.write(convertedCalendar[i]);
        }
        lcd.setCursor(11, 1);
        if (isOn_2 == 1) { // prints enabled state of current pump
            lcd.print("ON");
        } else {
            lcd.print("OFF");
        }
        break;

    default:
        break;
    }

}

// ===============================================================

void setup() {
    lcd.begin();
    createCustomChars();
    lcd.home();
}

void loop() {
    
}