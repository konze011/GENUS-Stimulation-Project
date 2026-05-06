/*
 40 Hz Flicker
 
 This code generates 40 Hz flicker at 50% duty cycle during which 
 the led strip turns on for 12.5 ms and then off for 12.5 ms.
 
 Equivalent audio flicker is a 10 khz tone (50% duty cycle square wave)
 turning on and off at 40 hz. (12.5 ms on and 12.5 ms off) 

 Switch Control
 SW1 - pin 3 - flicker rate (40 hz / random)
*/

#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define TO_SECONDS 1000000
#define TO_MILIS 1000
#define TO_MINUTES 60000000
#define UP_KEY '2'
#define DOWN_KEY '8'
#define SELECT_KEY '#'
#define DELETE_KEY '*'

// Pin variables used to set pin I/O
const int SWITCHPIN = 3;   // Pin used to read switch 1 state (HIGH = random, LOW = 40hz)
const int LEDPIN = 11;      // Pin used for digital LED strip signal
const int SPEAKERPIN = 16;  // Pin used for digital sound output
const byte KEYPAD_ROWS = 4;   // Number of rows the keypad has
const byte KEYPAD_COLS = 4;   // Number of columns the keypad has
byte rowPins[KEYPAD_ROWS] = {9, 8, 7, 6};   // Which pins the keypad rows map to
byte colPins[KEYPAD_COLS] = {5, 4, 3, 2};     // Which pins the keypad columns map to

// Timing variables used for microsecond delays
unsigned const long delay40Hz = 12500;
unsigned const long delay1ms = 1000;
unsigned long currentTime;
unsigned long previousTimeLED;
unsigned long previousTimeSpeaker;
unsigned long previousTimeOverall;
unsigned long delayLED;
unsigned long delayLEDOn = delay40Hz;
unsigned long delayLEDOff = delay40Hz;
unsigned long delaySpeaker;
unsigned long delaySpeakerOn = delay40Hz;
unsigned long delaySpeakerOff = delay40Hz;
unsigned long delayRandom;
unsigned long delayOverall;
unsigned long delayOverallOn = 2 * TO_MINUTES;
unsigned long delayOverallOff = 0.05 * TO_MINUTES;
unsigned long delayPhase;
unsigned long result;

unsigned int speakerTone = 10000;

// Setting up stimulation states
int ledState = LOW;
int speakerState = LOW;
int overallState = LOW;

// User inputs
float stimulationValue;
char stimulationMode;
char stimulationInput;
String keypadBuffer = "";

// Getters for LCD
typedef unsigned long (*ValueGetter)();

unsigned long getOverallOn() {return delayOverallOn / TO_MINUTES;}
unsigned long getOverallOff() {return delayOverallOff / TO_MINUTES;}
unsigned long getSpeakerTone() {return speakerTone;}
unsigned long getSpeakerDuration() {return delaySpeakerOn / TO_MILIS;}
unsigned long getSpeakerFreq() {return TO_SECONDS / (delaySpeakerOn + delaySpeakerOff);}
unsigned long getSpeakerPhase() {return delayPhase / TO_MILIS;}
unsigned long getLEDDuration() {return delayLEDOn / TO_MILIS;}
unsigned long getLEDFreq() {return TO_SECONDS / (delayLEDOn + delayLEDOff);}

// Setting up the LCD and its I2C code
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Setting up the LCD menus
const int menuLength = 8;
const char* menuItems[] = {
  "O On Dur",
  "O Off Dur",
  "S Base Freq",
  "S Duration",
  "S Stim Freq",
  "S Phase",
  "L Duration",
  "L Stim Freq"
};
const char* menuUnits[] = {
  "min",
  "min",
  "Hz",
  "ms",
  "Hz",
  "ms",
  "ms",
  "Hz"
};
ValueGetter menuValues[menuLength] = {
  getOverallOn,
  getOverallOff,
  getSpeakerTone,
  getSpeakerDuration,
  getSpeakerFreq,
  getSpeakerPhase,
  getLEDDuration,
  getLEDFreq
};

// Setting up LCD states
enum AppState {STATE_MENU, STATE_EDIT};
AppState appState = STATE_MENU;

// Setting up LCD menu offsets etc
int menuIndex = 0;
int menuScrollOffset = 0; // which item is shown on row 0
const int VISIBLE_ROWS = 4; // row 3 reserved for instructions

// Setting up the keypad
char keys[KEYPAD_ROWS][KEYPAD_COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);

void setup(void) {
  // Start serial output
  Serial.begin(9600);

  // Initialize pin I/O
  pinMode(SPEAKERPIN, OUTPUT);
  pinMode(LEDPIN, OUTPUT);
  pinMode(SWITCHPIN, INPUT);

  // Seed random function
  randomSeed(analogRead(0));

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  displayMenu();

  // Prompt the user to give an input
  Serial.println();
}

void loop(void) {
  // Read user frequency from Serial
  if (Serial.available() > 0) {
    serialInput();
    // Remove extra characters from serial buffer
    while (Serial.available()) Serial.read();
    // Reset stimulation
    stimulationReset();
  }

  // Read user inputs from keypad
  keypadInput();

  // Set current time for all timings
  currentTime = micros();

  // Run system on/off logic
  overallStimLogic();

  // End the loop if the system is off
  if (!overallState) {
    return;
  }

  // Run stimulation logic
  stimulationLogic();
}

void displayMenu(void) {
  lcd.clear();

  for (int row = 0; row < VISIBLE_ROWS; row++) {
    int itemIndex = menuScrollOffset + row;
    if (itemIndex >= menuLength) break;

    lcd.setCursor(0, row);
    if (itemIndex == menuIndex) {lcd.print(">");}
    else {lcd.print(" ");}
    // Print name (up to 13 chars), then value+unit right-aligned in remaining space
    char lineBuf[20];
    snprintf(lineBuf, sizeof(lineBuf),"%-11s%4lu%-3s",
             menuItems[itemIndex],
             menuValues[itemIndex](),
             menuUnits[itemIndex]);
    lcd.print(lineBuf);
  }

  // // Row 3: instruction bar
  // lcd.setCursor(0, 3);
  // lcd.print("  [Click to edit]   ");
}

void displayEdit(void) {
  lcd.clear();

  // Row 0: label
  lcd.setCursor(0, 0);
  lcd.print("Editing:");

  // Row 1: item name
  lcd.setCursor(0, 1);
  lcd.print(menuItems[menuIndex]);

  // Row 2: value + unit, large and centered
  char valBuf[21];
  snprintf(valBuf, sizeof(valBuf), "  Value: %3lu %s", menuValues[menuIndex](), menuUnits[menuIndex]);
  lcd.setCursor(0, 2);
  lcd.print(valBuf);

  // Row 3: instruction
  lcd.setCursor(0, 3);
  lcd.print("[# to confirm]");
}

void serialInput(void) {
  // Set user input to corresponding variables
    stimulationMode = Serial.read();
    stimulationInput = Serial.read();
    stimulationValue = Serial.parseFloat();

    // Set the inputs to uppercase
    stimulationMode = stimulationMode & ~(0x20);
    stimulationInput = stimulationInput & ~(0x20);

    // Determine if the speaker or LEDs should be modified
    // Speaker modification
    if (stimulationMode == 'S') {
      Serial.print("Speaker ");
      switch (stimulationInput) {
        case 'T':
          speakerTone = stimulationValue;
          Serial.println("tone changed");
          break;
        case 'D':
          stimulationValue *= TO_MILIS;
          if (stimulationValue <= delaySpeakerOn + delaySpeakerOff) {
            delaySpeakerOff = delaySpeakerOn + delaySpeakerOff - stimulationValue;
            delaySpeakerOn = stimulationValue;
            Serial.println("duration changed");
          } else {
            Serial.println("duration longer than total period");
          }
          break;
        case 'F':
          stimulationValue = TO_SECONDS / stimulationValue;
          if (delaySpeakerOn < stimulationValue) {
            delaySpeakerOff = stimulationValue - delaySpeakerOn;
            Serial.println("frequency changed. Speaker duration unchanged.");
          } else {
            delaySpeakerOn = stimulationValue / 2;
            delaySpeakerOff = delaySpeakerOn;
            Serial.println("frequency changed. Speaker duration set to 50%.");
          }
          break;
        case 'P':
          delayPhase = stimulationValue * TO_MILIS;
          Serial.println("speaker phase changed");
        default:
          Serial.println("Invalid speaker configuration");
          break;
      }
    }
    // LED modification
    else if (stimulationMode == 'L') {
      Serial.print("LED ");
      switch (stimulationInput) {
        case 'D':
          stimulationValue *= TO_MILIS;
          if (stimulationValue <= delayLEDOn + delayLEDOff) {
            delayLEDOff = delayLEDOn + delayLEDOff - stimulationValue;
            delayLEDOn = stimulationValue;
            Serial.println("duration changed");
          } else {
            Serial.println("duration longer than total period");
          }
          break;
        case 'F':
          stimulationValue = TO_SECONDS / stimulationValue;
          if (delayLEDOn < stimulationValue) {
            delayLEDOff = stimulationValue - delayLEDOn;
            Serial.println("frequency changed. LED duration unchanged.");
          } else {
            delayLEDOn = stimulationValue / 2;
            delayLEDOff = delayLEDOn;
            Serial.println("frequency changed. LED duration set to 50%.");
          }
          break;
        default:
          Serial.println("Invalid LED configuration");
          break;
      }
    }  // End of LED modification
    // Overall modification
    else if (stimulationMode == 'O') {
      switch (stimulationInput) {
        case 'N':
          delayOverallOn = stimulationValue * TO_MINUTES;
          break;
        case 'F':
          delayOverallOff = stimulationValue * TO_MINUTES;
          break;
        default:
          Serial.println("Invalid overall configuration");
          break;
      }
    } else {  // Invalid modification
      Serial.println("Invalid stimulation method");
    }  // End of invalid modification
}

void keypadInput(void) {
  char key = keypad.getKey();
  if (!key) return; // Exit if no key pressed

  if (appState == STATE_MENU) {
    switch (key) {
      case UP_KEY: // Scroll Up
        if (menuIndex > 0) {
          menuIndex--;
          if (menuIndex < menuScrollOffset + 1 && menuScrollOffset > 0) {
            menuScrollOffset--;
          }
        }
        displayMenu();
        break;
      case DOWN_KEY: // Scroll Down
        if (menuIndex < menuLength - 1) {
          menuIndex++;
          if (menuIndex >= menuScrollOffset + VISIBLE_ROWS - 1 && 
              menuScrollOffset < menuLength - VISIBLE_ROWS) {
            menuScrollOffset++;
          }
        }
        displayMenu();
        break;
      case SELECT_KEY: // Select parameter to edit
        appState = STATE_EDIT;
        keypadBuffer = "";
        displayEdit();
        break;
    }
  } 
  else if (appState == STATE_EDIT) {
    if (key >= '0' && key <= '9') {
      keypadBuffer += key; // Append digit
      updateDisplayEdit(); // Show what the user is typing
    } 
    else if (key == SELECT_KEY) { // Use SELECT_KEY to confirm change/go back
      if (keypadBuffer.length() > 0) {
        applyNewValue(keypadBuffer.toInt());
      }
      appState = STATE_MENU;
      displayMenu();
    } 
    else if (key == DELETE_KEY) { // Use DELETE_KEY to delete/go back
      if (keypadBuffer.length() > 0) {
        keypadBuffer.remove(keypadBuffer.length() - 1);
        updateDisplayEdit();
      }
      else {
        appState = STATE_MENU;
        displayMenu();
      }
    }
  }
}

void updateDisplayEdit(void) {
  lcd.setCursor(0, 2);
  lcd.print("New Val:           "); // Clear line
  lcd.setCursor(9, 2);
  lcd.print(keypadBuffer); 
  lcd.print(" ");
  lcd.print(menuUnits[menuIndex]);
}

void applyNewValue(unsigned long newVal) {
  switch (menuIndex) {
    case 0: delayOverallOn = newVal * TO_MINUTES; break;   // Overall on
    case 1: delayOverallOff = newVal * TO_MINUTES; break;  // Overall off
    case 2: speakerTone = newVal; break;  // Speaker tone
    case 3: // Speaker duration
      newVal *= TO_MILIS;
      if (newVal <= delaySpeakerOn + delaySpeakerOff) {
        delaySpeakerOff = delaySpeakerOn + delaySpeakerOff - newVal;
        delaySpeakerOn = newVal;
      } else {
        Serial.println("duration longer than total period");
      }
      break;          
    case 4: // Speaker frequency
      newVal = TO_SECONDS / newVal;
      if (delaySpeakerOn < newVal) {
        delaySpeakerOff = newVal - delaySpeakerOn;
        Serial.println("frequency changed. Speaker duration unchanged.");
      } else {
        delaySpeakerOn = newVal / 2;
        delaySpeakerOff = delaySpeakerOn;
        Serial.println("frequency changed. Speaker duration set to 50%.");
      }
      break;
    case 5: delayPhase = newVal * TO_MILIS; break;  // Speaker phase
    case 6: // LED duration
      newVal *= TO_MILIS;
      if (newVal <= delayLEDOn + delayLEDOff) {
        delayLEDOff = delayLEDOn + delayLEDOff - newVal;
        delayLEDOn = newVal;
        Serial.println("duration changed");
      } else {
        Serial.println("duration longer than total period");
      }
      break;
    case 7: // LED frequency
      newVal = TO_SECONDS / newVal;
      if (delayLEDOn < newVal) {
        delayLEDOff = newVal - delayLEDOn;
        Serial.println("frequency changed. LED duration unchanged.");
      } else {
        delayLEDOn = newVal / 2;
        delayLEDOff = delayLEDOn;
        Serial.println("frequency changed. LED duration set to 50%.");
      }
      break;
  }
  stimulationReset();
}

void stimulationReset(void) {
  // "reset" stimulation state and phase offset
    digitalWrite(LEDPIN, LOW);
    digitalWrite(SPEAKERPIN, LOW);
    if (delaySpeakerOn + delaySpeakerOff == delayLEDOn + delayLEDOff) {
      previousTimeSpeaker = micros();
      previousTimeLED = previousTimeSpeaker - delayPhase;
    } else {
      previousTimeSpeaker = micros();
      previousTimeLED = previousTimeSpeaker;
    }
}

void overallStimLogic(void) {
  // Overall frequency
  if (currentTime - previousTimeOverall >= delayOverall) {
    if (overallState) {
      digitalWrite(LEDPIN, LOW);
      noTone(SPEAKERPIN);
      delayOverall = delayOverallOff;
    } else {
      stimulationReset();
      delayOverall = delayOverallOn;
    }
    overallState = !overallState;
    previousTimeOverall = micros();
  }
}

void stimulationLogic(void) {
  // Take switch input to determine stimulation frequency
  int switchValue = digitalRead(SWITCHPIN);

  // User input frequency
  if (switchValue == LOW) {
    // currentTime - previousTimeLEDOff >= delayLEDOff
    if (currentTime - previousTimeLED >= delayLED) {
      if (ledState) {
        delayLED = delayLEDOff;
      } else {
        delayLED = delayLEDOn;
      }
      ledState = !ledState;
      digitalWrite(LEDPIN, ledState);
      previousTimeLED = micros();
    }

    if (currentTime - previousTimeSpeaker >= delaySpeaker) {
      if (speakerState) {
        delaySpeaker = delaySpeakerOff;
        noTone(SPEAKERPIN);
      } else {
        delaySpeaker = delaySpeakerOn;
        tone(SPEAKERPIN, speakerTone);
      }
      speakerState = !speakerState;
      previousTimeSpeaker = micros();
    }
  }
  // RANDOM - first half output is 12.5 ms, second half delay is a random amount of time averaging 12.5 ms
  else {
    // currentTime - previousTimeLEDOff >= delayLEDOff
    if (currentTime - previousTimeLED >= delayLED) {
      if (ledState) {
        delayLED = delay40Hz;
      } else {
        delayRandom = random(200, 24800);
        delayLED = delayRandom;
      }
      ledState = !ledState;
      digitalWrite(LEDPIN, ledState);
      previousTimeLED = micros();
    }

    if (currentTime - previousTimeSpeaker >= delaySpeaker) {
      if (speakerState) {
        delaySpeaker = delay40Hz + delayRandom - delay1ms;
        noTone(SPEAKERPIN);
      } else {
        delaySpeaker = delay1ms;
        tone(SPEAKERPIN, speakerTone);
      }
      speakerState = !speakerState;
      previousTimeSpeaker = micros();
    }
  }
}
