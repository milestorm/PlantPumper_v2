#include <Arduino.h>
#include <customChars.h>
#include <pinout.h>
#include <LiquidCrystal_I2C.h>
#include <Time.h>
#include <DS1302RTC.h>
#include <OneButton.h>
#include <RotaryEncoder.h>
#include <EEPROM.h>
#include <avdweb_VirtualDelay.h>

class Pump {
    int pumpPin;

    public:
    Pump(int pin) {
        pumpPin = pin;
        pinMode(pumpPin, OUTPUT);
        digitalWrite(pumpPin, HIGH);
    }

    void startWater() {
        if (digitalRead(pumpPin) == HIGH) {
            digitalWrite(pumpPin, LOW); // turns ON the pump
        }
    }

    void stopWater() {
        if (digitalRead(pumpPin) == LOW) {
            digitalWrite(pumpPin, HIGH); // turns OFF the pump
        }
    }
};

// Set the LCD address to 0x27 in PCF8574 by NXP and Set to 0x3F in PCF8574A by Ti
LiquidCrystal_I2C lcd(0x27, 16, 2);
// Set RTC module
DS1302RTC rtc(RTC_RST, RTC_DAT, RTC_CLK);
// Set Rotary Encoder
RotaryEncoder encoder(ROTARYENCODER_PIN1, ROTARYENCODER_PIN2);
OneButton rotaryButton(ROTARYENCODER_BUTTON, true);

VirtualDelay vDelay;
int vDelayDuration = 4000;

// initializes the pumps
int pumpCount = 2;
Pump pump1(RELAY1);
Pump pump2(RELAY2);
Pump pump[2] = {pump1, pump2};

// initializes two sets of variables used in the timers
// first timer at position 0, second timer at position 1
int startHour[2] = {0, 0};
int startMinute[2] = {0, 0};
int duration[2] = {0, 0};
int isOn[2] = {0, 0};
int calendar[2][7] = {{0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}};

bool pumpActive[2] = {false, false};

char calendar_on[7] = {'M', 'T', 'W', 'T', 'F', 'S', 'S'};
char calendar_off[7] = {'m', 't', 'w', 't', 'f', 's', 's'};

String dayNames[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

char convertedCalendar[7];

// stores actual time
tmElements_t actualTime;

// if something is editing, do not display the cycling display (time, pump1, pump2)
bool isEditing = false;
// this is cycler between time, pump1 and pump2
int cycler = 0;

/**
 * inits custom characters for LCD. Max 8 custom characters :(
**/
void createCustomChars() {
    lcd.createChar(0, chr_Clock);
    lcd.createChar(1, chr_First);
    lcd.createChar(2, chr_Second);
    lcd.createChar(3, chr_Faucet);
    lcd.createChar(4, chr_Calendar);
    lcd.createChar(5, chr_Drop);
}

/**
 * reads settings from eeprom
 * if the values are not valid, 0 is applied
**/
void readEEPROMSettings() {
    int addr = 0;

    for (int i = 0; i < pumpCount; i++) {
        startHour[i] = EEPROM.read(addr);
        if (startHour[i] > 24) { startHour[i] = 0; }
        addr++;

        startMinute[i] = EEPROM.read(addr);
        if (startMinute[i] > 60) { startMinute[i] = 0; }
        addr++;

        duration[i] = EEPROM.read(addr);
        if (duration[i] > 60) { duration[i] = 0; }
        addr++;

        isOn[i] = EEPROM.read(addr);
        if (isOn[i] > 2) { isOn[i] = 0; }
        addr++;

        for (int j = 0; j < 7; j++) {
            calendar[i][j] = EEPROM.read(addr);
            if (calendar[i][j] > 2) { calendar[i][j] = 0; }
            addr++;
        }
    }
}

void updateEEPROMSettings() {
    int addr = 0;

    for (int i = 0; i < pumpCount; i++) {
        EEPROM.update(addr, startHour[i]);
        addr++;
        EEPROM.update(addr, startMinute[i]);
        addr++;
        EEPROM.update(addr, duration[i]);
        addr++;
        EEPROM.update(addr, isOn[i]);
        addr++;
        for (int j = 0; j < 7; j++) {
            EEPROM.update(addr, calendar[i][j]);
            addr++;
        }
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
void convertCalendar(int id) {
    for (int i = 0; i < 7; i++) {
        if (calendar[id][i] == 0) {
            convertedCalendar[i] = calendar_off[i];
        } else {
            convertedCalendar[i] = calendar_on[i];
        }
    }
}

/**
 * creates "pump" screen layout for LCD
**/
void printTimerValuesToLCD(int timerId) {
    lcd.clear();

    convertCalendar(timerId);

    lcd.write(timerId + 1); // number 1 pump symbol
    lcd.setCursor(2, 0);
    lcd.write(0); // clock symbol
    lcd.setCursor(3, 0);
    lcd.print(to2digits(startHour[timerId]));
    lcd.print(":");
    lcd.print(to2digits(startMinute[timerId]));
    lcd.setCursor(11, 0);
    lcd.write(3); // faucet symbol
    lcd.print(to2digits(duration[timerId]));
    lcd.print("s");

    // second line of lcd
    lcd.setCursor(2, 1);
    lcd.write(4); // calendar symbol
    lcd.setCursor(3, 1);
    for (int i = 0; i < 7; i++) { // prints calendar
        lcd.print(convertedCalendar[i]);
    }
    lcd.setCursor(11, 1);
    if (isOn[timerId] == 1) { // prints enabled state of current pump
        lcd.print("ON");
    } else {
        lcd.print("OFF");
    }

}

void printTimeToLCD() {
    lcd.clear();

    lcd.print(dayNames[actualTime.Wday - 1]);
    lcd.print(" ");
    lcd.print(actualTime.Day);
    lcd.print(".");
    lcd.print(actualTime.Month);
    lcd.print(".");

    lcd.setCursor(11, 0);
    lcd.print(to2digits(actualTime.Hour));
    lcd.print(":");
    lcd.print(to2digits(actualTime.Minute));
}

/**
 * watches, if pumpActive, and turns on or off the relays
 * must be placed in loop()
**/
void pumpWatcher() {
    for (int i = 0; i < pumpCount; i++) {
        if (pumpActive[i] == true) {
            pump[i].startWater();
        } else {
            pump[i].stopWater();
        }
    }
}

/**
 * reads time from RTCmodule
 * must be placed in loop()
**/
void timeWatcher() {
    if (!rtc.read(actualTime)) {
        // reads time and puts it in actualTime
    } else {
        lcd.clear();
        lcd.print("RTC read error!");
        delay(5000);
    }
}

void lcdCycler() {
    if (!isEditing) {
        vDelay.start(vDelayDuration);
        if(vDelay.elapsed()) {
            switch (cycler) {
            case 0:
                printTimeToLCD();
                cycler++;
                break;

            case 1:
                printTimerValuesToLCD(0);
                cycler++;
                break;

            case 2:
                printTimerValuesToLCD(1);
                cycler = 0;
                break;

            default:
                break;
            }
        }
    }
}

void rotaryButtonClickHandler() {
    if (isEditing) {
        encoder.setPosition(0);
    }
}

void rotaryButtonLongPressHandler() {
    if (isEditing == false) {
        isEditing = true; // first command here
        // entering editing mode
        lcd.clear();
        lcd.print("CONFIG MODE");
        delay(3000);
        printTimerValuesToLCD(0);
    } else {
        // exiting editing mode
        lcd.clear();
        lcd.cursor_off();
        lcd.print("SAVING...");
        delay(3000);
        isEditing = false; // this must be the last command here.
    }
}

void rotaryEncoderTick() {
    if (isEditing) {
        static int pos = 0;
        encoder.tick();

        int newPos = encoder.getPosition();
        if (pos != newPos) {

            lcd.cursor_on();
            lcd.setCursor(0, 1);
            lcd.print(newPos);
            lcd.print(" ");
            lcd.setCursor(0, 1);
            pos = newPos;
        }
    }
}

// ===============================================================

void setup() {
    lcd.begin();
    createCustomChars();
    lcd.home();
    lcd.print("PlantPumper v2");
    lcd.setCursor(0, 1);
    lcd.print("booting up...");

    rotaryButton.attachClick(rotaryButtonClickHandler);
    rotaryButton.attachLongPressStop(rotaryButtonLongPressHandler);

    readEEPROMSettings();
}

void loop() {
    timeWatcher();
    pumpWatcher();
    rotaryButton.tick();
    rotaryEncoderTick();

    lcdCycler();
}