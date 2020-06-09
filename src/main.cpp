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

VirtualDelay lcdBacklightDelay;
unsigned int backlightDelayDuration = 30000;
unsigned long backlightPreviousMillis = 0;

// initializes the pumps
int pumpCount = 2;
Pump pump1(RELAY1);
Pump pump2(RELAY2);
Pump pump[2] = {pump1, pump2};

// initializes two sets of variables used in the timers
// first timer at position 0, second timer at position 1
uint8_t startHour[2] = {0, 0};
uint8_t startMinute[2] = {0, 0};
uint8_t duration[2] = {0, 0};
uint8_t isOn[2] = {0, 0};
uint8_t calendar[2][7] = {{0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}};

bool pumpActive[2] = {false, false};

char calendar_on[7] = {'S', 'M', 'T', 'W', 'T', 'F', 'S'};
char calendar_off[7] = {'s', 'm', 't', 'w', 't', 'f', 's'};

String dayNames[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

char convertedCalendar[7];

// stores actual time
tmElements_t actualTime;

tmElements_t newTime;

// if something is editing, do not display the cycling display (time, pump1, pump2)
bool isEditing = false;
bool isEditingCalendar = false;
bool isMenu = false;
// this is cycler between time, pump1 and pump2
int cycler = 0;

uint8_t menuPosition = 0;
int editingPosition = 0;
int calendarPosition = 0;
int cursorPositionsPumpLength = 11;
int cursorPositionsPump[11][2] = {
    // first pump screen
    {3, 0}, // Hours
    {6, 0}, // Minutes
    {12, 0}, // Duration
    {3, 1}, // Monday
    {4, 1}, // Tuesday
    {5, 1}, // Wednesday
    {6, 1}, // Thursday
    {7, 1}, // Friday, Friday, let's get down on Friday
    {8, 1}, // Saturday
    {9, 1}, // Sunday
    {11, 1} // On/Off
};
int cursorPositionTimeLength = 5;
int cursorPositionTime[5][2] = {
    {0, 1}, // Day
    {3, 1}, // Month
    {6, 1}, // Year
    {11, 0}, // Hour
    {14, 0} // Minute
};

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
 * reads settings from EEPROM
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

/**
 * updates settings to EEPROM
 * uses update function to save write cycles
**/
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
 * saves time to RTC module
**/
void saveTimeToRTC(tmElements_t newTm) {
    time_t t;
    tmElements_t tm;

    tm.Year = newTm.Year;
    tm.Month = newTm.Month;
    tm.Day = newTm.Day;
    tm.Hour = newTm.Hour;
    tm.Minute = newTm.Minute;
    tm.Second = 0;
    t = makeTime(tm);

    if (rtc.set(t) == 0) {
      // Serial.println("Time set!");
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
 * returns caps or nocaps (if the day is on or off) character of the calendar.
**/
char calendarDayForPrint(int id, int value) {
    if (value == 0) {
        return calendar_off[id];
    } else {
        return calendar_on[id];
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
        lcd.print("ON ");
    } else {
        lcd.print("OFF");
    }

}

/**
 * creates time screen layout for LCD
**/
void printTimeToLCD() {
    lcd.clear();

    lcd.print(dayNames[actualTime.Wday - 1]);
    lcd.setCursor(0, 1);
    lcd.print(to2digits(actualTime.Day));
    lcd.print(".");
    lcd.print(to2digits(actualTime.Month));
    lcd.print(".");
    lcd.print(actualTime.Year + 1970); // must add 1970, because tm time is counted after 1970

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

/**
 * prints "screen" to LCD. Like time screen or pump screen
 * defined by id
**/
void showEditScreen(int id) {
    switch (id) {
    case 0:
        printTimeToLCD();
        break;

    case 1:
        printTimerValuesToLCD(0);
        break;

    case 2:
        printTimerValuesToLCD(1);
        break;

    default:
        break;
    }
}

/**
 * cycling of the screens
**/
void lcdCycler() {
    if (!isEditing) {
        vDelay.start(vDelayDuration);
        if(vDelay.elapsed()) {
            showEditScreen(cycler);
            cycler++;
            if (cycler > 2) {
                cycler = 0;
            }
        }
    }
}

/**
 * watcher for handling backlight turning on/off
**/
void lcdBacklightTick() {
    unsigned long currentMillis = millis();

    if (currentMillis - backlightPreviousMillis >= backlightDelayDuration) {
        lcd.noBacklight();
    } else {
        lcd.backlight();
    }
}

/**
 * shows configuration menu screen
**/
void menuScreen(int id) {
    lcd.clear();
    lcd.print("CONFIG MENU:");
    lcd.setCursor(0, 1);
    menuPosition = id;

    switch (id) {
    case 0:
        lcd.print("Time and date");
        break;

    case 1:
        lcd.print("Pump #1");
        break;

    case 2:
        lcd.print("Pump #2");
        break;

    default:
        break;
    }
}

/**
 * handles longpress of the rotary encoder
 * used for entering to config menu and saving configuration
**/
void rotaryButtonLongPressHandler() {
    if (isEditing == false) {
        isEditing = true; // first command here
        // entering editing mode
        menuPosition = 0;
        isMenu = true;
        newTime = actualTime;
        menuScreen(menuPosition);
    } else {
        // exiting editing mode
        lcd.clear();
        lcd.cursor_off();
        lcd.print("Saving config...");
        delay(2000);
        editingPosition = 0;
        calendarPosition = 0;
        if (menuPosition == 0) {
            saveTimeToRTC(newTime);
        } else {
            updateEEPROMSettings();
        }
        isEditing = false; // this must be the last command here.
    }
}

/**
 * sets cursor to position defined by the editinPosition parameter
**/
void setCursorPosition() {
    if (menuPosition == 0) {
        lcd.setCursor(cursorPositionTime[editingPosition][0], cursorPositionTime[editingPosition][1]);
    } else {
       lcd.setCursor(cursorPositionsPump[editingPosition][0], cursorPositionsPump[editingPosition][1]);
    }
}

/**
 * handles click of the rotary encoder
**/
void rotaryButtonClickHandler() {
    if (isEditing) {
        if (isMenu) {
            // click confirms menu
            isMenu = false;
            showEditScreen(menuPosition);
            lcd.cursor_on();
            setCursorPosition();

        } else {
            editingPosition++;
            if (menuPosition == 0) {
               if (editingPosition > cursorPositionTimeLength - 1) editingPosition = 0;
            } else {
                if (editingPosition > cursorPositionsPumpLength - 1) editingPosition = 0;
            }
            setCursorPosition();
        }
    }
}

/**
 * handles increasing and decreasing the value of "value" parameter within the limits
**/
void encoderAddValue(RotaryEncoder::Direction direction, uint8_t &value, int bottomLimit, int upperLimit) {
    if (direction == RotaryEncoder::Direction::CLOCKWISE) {
        value++;
    } else if (direction == RotaryEncoder::Direction::COUNTERCLOCKWISE) {
        if (value == bottomLimit) { // because of uint8_t cannot go to negative numbers
            value = upperLimit;
        } else {
            value--;
        }
    }
    if (value > upperLimit) value = bottomLimit;
};

/**
 * handles rotating of the rotary encoder
**/
void rotaryEncoderTick() {
    static int pos = 0;
    encoder.tick();

    int newPos = encoder.getPosition();

    if (pos != newPos) {
        RotaryEncoder::Direction direction = encoder.getDirection();
        backlightPreviousMillis = millis(); // when rotated, extend delay for backlight

        if (isEditing) {
            if (isMenu) {
                // user is in the main menu
                encoderAddValue(direction, menuPosition, 0, 2);
                menuScreen(menuPosition);
            } else {
                if (menuPosition == 0) {
                    // set time and date
                    switch (editingPosition) {
                    case 0: // day
                        encoderAddValue(direction, newTime.Day, 1, 31);
                        lcd.print(to2digits(newTime.Day));
                        break;

                    case 1: // month
                        encoderAddValue(direction, newTime.Month, 1, 12);
                        lcd.print(to2digits(newTime.Month));
                        break;

                    case 2: // year
                        encoderAddValue(direction, newTime.Year, 0, 255);
                        lcd.print(to2digits(newTime.Year + 1970));
                        break;

                    case 3: // hour
                        encoderAddValue(direction, newTime.Hour, 0, 23);
                        lcd.print(to2digits(newTime.Hour));
                        break;

                    case 4: // minute
                        encoderAddValue(direction, newTime.Minute, 0, 59);
                        lcd.print(to2digits(newTime.Minute));
                        break;

                    default:
                        break;
                    }
                } else {
                    // set pumps
                    int pumpPosition = menuPosition - 1;

                    switch (editingPosition) {
                    case 0: // hours
                        encoderAddValue(direction, startHour[pumpPosition], 0, 23);
                        lcd.print(to2digits(startHour[pumpPosition]));
                        break;

                    case 1: // minutes
                        encoderAddValue(direction, startMinute[pumpPosition], 0, 59);
                        lcd.print(to2digits(startMinute[pumpPosition]));
                        break;

                    case 2: // duration
                        encoderAddValue(direction, duration[pumpPosition], 1, 59);
                        lcd.print(to2digits(duration[pumpPosition]));
                        break;

                    case 3: // calendar
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                        encoderAddValue(direction, calendar[pumpPosition][editingPosition - 3], 0, 1);
                        lcd.print(calendarDayForPrint(editingPosition - 3, calendar[pumpPosition][editingPosition - 3]));
                        break;

                    case 10: // on/off
                        encoderAddValue(direction, isOn[pumpPosition], 0, 1);
                        if (isOn[pumpPosition] == 1) {
                            lcd.print("ON ");
                        } else {
                            lcd.print("OFF");
                        }
                        break;

                    default:
                        break;
                    }
                }
                setCursorPosition();
            }
        } // isEditing
        pos = newPos;
    }
}

/**
 * watches the activation time and activates the pumps
**/
void pumpActivationWatcher() {
    for (int i = 0; i < pumpCount; i++) {
        if (isOn[i] == 1) {
            if (calendar[i][actualTime.Wday - 1] == 1) { // weekday is matched. -1 because Wday is from 1, not 0
                if ((actualTime.Hour == startHour[i]) && (actualTime.Minute == startMinute[i]) && (actualTime.Second == 0)) { // time is matched
                    // start the pump
                    pumpActive[i] = true;
                } else if ((actualTime.Hour == startHour[i]) && (actualTime.Minute == startMinute[i]) && (actualTime.Second == duration[i])) {
                    // stops the pump
                    pumpActive[i] = false;
                }
            }
        }
    }
}

// ============================== SETUP & LOOP =================================

void setup() {
    Serial.begin(9600);

    // inits LCD
    lcd.begin();
    lcd.backlight();
    createCustomChars();
    lcd.home();
    lcd.print("PlantPumper v2");
    lcd.setCursor(0, 1);
    lcd.print("booting up...");

    // inits rotary encoder
    rotaryButton.attachClick(rotaryButtonClickHandler);
    rotaryButton.attachLongPressStop(rotaryButtonLongPressHandler);

    // load settings from EEPROM
    readEEPROMSettings();
}

void loop() {
    timeWatcher();
    pumpWatcher();
    rotaryButton.tick();
    rotaryEncoderTick();
    lcdBacklightTick();
    lcdCycler();
    pumpActivationWatcher();
}