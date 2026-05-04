/*
 40 Hz Flicker
 
 This code generates 40 Hz flicker at 50% duty cycle during which 
 the led strip turns on for 12.5 ms and then off for 12.5 ms.
 
 Equivalent audio flicker is a 10 khz tone (50% duty cycle square wave)
 turning on and off at 40 hz. (12.5 ms on and 12.5 ms off) 

 Switch Control
 SW1 - pin 3 - flicker rate (40 hz / random)
*/

#include <Encoder.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define TO_SECONDS 1000000
#define TO_MILIS 1000
#define TO_MINUTES 60000000

// Pin variables used to set pin I/O
const int LED_PIN = 11;     // Pin used for digital LED strip signal
const int SPEAKER_PIN = 6;  // Pin used for digital sound output

Encoder myEnc(2, 3);
const int BTN_PIN = 4;
long lastEncPos = 0;
bool lastBtnState = HIGH;
unsigned long lastDebounce = 0;
bool btnHandled = false;

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
unsigned long delaySpeakerOn = delay1ms;
unsigned long delaySpeakerOff = delay40Hz * 2 - delay1ms;
unsigned long delayRandom;
unsigned long delayOverall;
unsigned long delayOverallOn = 0.05 * TO_MINUTES;
unsigned long delayOverallOff = 0.05 * TO_MINUTES;
unsigned long delayPhase;
unsigned long result;
unsigned long switchValue;

unsigned int speakerTone = 10000;
unsigned long encoderInputVal;

// Setting up stimulation states
int ledState = LOW;
int speakerState = LOW;
int overallState = LOW;

// User inputs
float stimulationValue;
char stimulationMode;
char stimulationInput;

// Getters for LCD
typedef unsigned long (*ValueGetter)();

unsigned long getOverallOn() {
  return delayOverallOn / TO_MINUTES;
}
unsigned long getOverallOff() {
  return delayOverallOff / TO_MINUTES;
}
unsigned long getOverallRandom() {
  return switchValue;
}
unsigned long getSpeakerTone() {
  return speakerTone;
}
unsigned long getSpeakerDuration() {
  return delaySpeakerOn / TO_MILIS;
}
unsigned long getSpeakerFreq() {
  return TO_SECONDS / (delaySpeakerOn + delaySpeakerOff);
}
unsigned long getSpeakerPhase() {
  return delayPhase / TO_MILIS;
}
unsigned long getLEDDuration() {
  return delayLEDOn / TO_MILIS;
}
unsigned long getLEDFreq() {
  return TO_SECONDS / (delayLEDOn + delayLEDOff);
}

// Setting up the LCD and its I2C code
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Setting up the LCD menus
const int menuLength = 9;
const char* menuItems[] = {
  "O On Dur",
  "O Off Dur",
  "O Random",
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
  "",
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
  getOverallRandom,
  getSpeakerTone,
  getSpeakerDuration,
  getSpeakerFreq,
  getSpeakerPhase,
  getLEDDuration,
  getLEDFreq
};

// Setting up LCD states
enum AppState { STATE_MENU,
                STATE_EDIT };
AppState appState = STATE_MENU;

// Setting up LCD menu offsets etc
int menuIndex = 0;
int menuScrollOffset = 0;    // which item is shown on row 0
const int VISIBLE_ROWS = 4;  // row 3 reserved for instructions

void setup(void) {
  // Start serial output
  Serial.begin(9600);

  // Initialize pin I/O
  pinMode(SPEAKER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // Seed random function
  randomSeed(analogRead(0));

  // Initialize LCD
  pinMode(BTN_PIN, INPUT_PULLUP);
  lcd.init();
  lcd.backlight();
  displayMenu();

  // Prompt the user to give an input
  Serial.println("Enter Serial Input:");
}

void loop(void) {
  // Read user frequency from Serial
  if (Serial.available() > 0) {
    serialInput();
    displayMenu();
    // Remove extra characters from serial buffer
    while (Serial.available()) Serial.read();
    // Reset stimulation
    stimulationReset();
  }

  // Read user inputs
  encoderInput();

  // Set current time for all timings
  currentTime = micros();

  // Run system on/off logic
  overallStimLogic();
  // Serial.println(currentTime);
  // Serial.println(previousTimeOverall);
  // Serial.println(previousTimeOverall + delayOverall);
  // Serial.println(overallState);

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
    if (itemIndex == menuIndex) {
      lcd.print(">");
    } else {
      lcd.print(" ");
    }
    // Print name (up to 13 chars), then value+unit right-aligned in remaining space
    char lineBuf[20];
    snprintf(lineBuf, sizeof(lineBuf), "%-11s%4lu%-3s",
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
  encoderInputVal = menuValues[menuIndex]();

  lcd.clear();

  // Row 0: label
  lcd.setCursor(0, 0);
  lcd.print("Editing:");

  // Row 1: item name
  lcd.setCursor(0, 1);
  lcd.print(menuItems[menuIndex]);

  // Row 2: value + unit, large and centered
  char valBuf[21];
  snprintf(valBuf, sizeof(valBuf), "  Value: %3lu %s", encoderInputVal, menuUnits[menuIndex]);
  lcd.setCursor(0, 2);
  lcd.print(valBuf);

  // Row 3: instruction
  lcd.setCursor(0, 3);
  lcd.print("[Click to confirm]");
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
        Serial.print("tone: ");
        if (stimulationValue) {
          // menuIndex = 3;
          // applyNewValue(stimulationInput);
          speakerTone = stimulationValue;
          Serial.println("changed");
        } else {
          Serial.println(speakerTone);
        }
        break;
      case 'D':
        menuIndex = 4;
        // applyNewValue(stimulationInput);
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
        menuIndex = 5;
        // applyNewValue(stimulationInput);
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
        menuIndex = 6;
        // applyNewValue(stimulationInput);
        delayPhase = stimulationValue * TO_MILIS;
        Serial.println("speaker phase changed");
        break;
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
        menuIndex = 7;
        // applyNewValue(stimulationInput);
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
        menuIndex = 8;
        // applyNewValue(stimulationInput);
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
    Serial.print("Overall ");
    switch (stimulationInput) {
      case 'N':
        Serial.print("on duration: ");
        // menuIndex = 0;
        // applyNewValue(stimulationInput);
        if (stimulationValue) {
          Serial.print("changed to ");
          Serial.print(stimulationValue);
          Serial.println(" minutes");
          delayOverallOn = stimulationValue * TO_MINUTES;
        } else {
          Serial.print(delayOverallOn / TO_MINUTES);
          Serial.println(" minutes");
        }
        break;
      case 'F':
        Serial.print("off duration: ");
        // menuIndex = 1;
        // applyNewValue(stimulationInput);
        if (stimulationValue) {
          // Serial.println(sprintf("changed to %f minutes", stimulationValue));
          delayOverallOff = stimulationValue * TO_MINUTES;
        } else {
          // Serial.println(sprintf("%lu us", delayOverallOff / TO_MINUTES));
        }
        break;
      default:
        Serial.println("Invalid overall configuration");
        break;
    }
  } else {  // Invalid modification
    Serial.println("Invalid stimulation method");
  }  // End of invalid modification
}

void applyNewValue(float newVal) {
  switch (menuIndex) {
    case 0: delayOverallOn = newVal * TO_MINUTES; break;   // Overall on
    case 1: delayOverallOff = newVal * TO_MINUTES; break;  // Overall off
    case 2: switchValue = newVal; break;                   // Overall random
    case 3: speakerTone = newVal; break;                   // Speaker tone
    case 4:                                                // Speaker duration
      newVal *= TO_MILIS;
      if (newVal <= delaySpeakerOn + delaySpeakerOff) {
        delaySpeakerOff = delaySpeakerOn + delaySpeakerOff - newVal;
        delaySpeakerOn = newVal;
      } else {
        Serial.println("duration longer than total period");
      }
      break;
    case 5:  // Speaker frequency
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
    case 6: delayPhase = newVal * TO_MILIS; break;  // Speaker phase
    case 7:                                         // LED duration
      newVal *= TO_MILIS;
      if (newVal <= delayLEDOn + delayLEDOff) {
        delayLEDOff = delayLEDOn + delayLEDOff - newVal;
        delayLEDOn = newVal;
        Serial.println("duration changed");
      } else {
        Serial.println("duration longer than total period");
      }
      break;
    case 8:  // LED frequency
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
  digitalWrite(LED_PIN, LOW);
  digitalWrite(SPEAKER_PIN, LOW);
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
      digitalWrite(LED_PIN, LOW);
      noTone(SPEAKER_PIN);
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
  // User input frequency
  if (switchValue == 0) {
    // currentTime - previousTimeLEDOff >= delayLEDOff
    if (currentTime - previousTimeLED >= delayLED) {
      if (ledState) {
        delayLED = delayLEDOff;
      } else {
        delayLED = delayLEDOn;
      }
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      previousTimeLED = micros();
    }

    if (currentTime - previousTimeSpeaker >= delaySpeaker) {
      if (speakerState) {
        delaySpeaker = delaySpeakerOff;
        noTone(SPEAKER_PIN);
      } else {
        delaySpeaker = delaySpeakerOn;
        tone(SPEAKER_PIN, speakerTone);
      }
      speakerState = !speakerState;
      previousTimeSpeaker = micros();
    }
  }
  // RANDOM - first half output is 12.5 ms, second half delay is a random amount of time averaging 12.5 ms
  else {
    delayRandom = random(0.2 * TO_MILIS, (2 * delayLEDOff - 0.2) * TO_MILIS);

    // currentTime - previousTimeLEDOff >= delayLEDOff
    if (currentTime - previousTimeLED >= delayLED) {
      if (ledState) {
        delayLED = delayLEDOff;
      } else {
        delayLED = delayRandom;
      }
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      previousTimeLED = micros();
    }

    if (currentTime - previousTimeSpeaker >= delaySpeaker) {
      if (speakerState) {
        delaySpeaker = delayLEDOff + delayRandom - delay1ms;
        noTone(SPEAKER_PIN);
      } else {
        delaySpeaker = delay1ms;
        tone(SPEAKER_PIN, speakerTone);
      }
      speakerState = !speakerState;
      previousTimeSpeaker = micros();
    }
  }
}

void encoderInput(void) {
  long encPos = myEnc.read() / 4;

  if (encPos != lastEncPos) {
    int dir = (encPos > lastEncPos) ? 1 : -1;
    lastEncPos = encPos;

    if (appState == STATE_MENU) {
      menuIndex = constrain(menuIndex + dir, 0, menuLength - 1);
      // scroll offset: keep selected item visible
      if (menuIndex < menuScrollOffset) menuScrollOffset = menuIndex;
      if (menuIndex >= menuScrollOffset + VISIBLE_ROWS) menuScrollOffset = menuIndex - VISIBLE_ROWS + 1;
      displayMenu();
    } else {
      encoderInputVal += dir;
      updateDisplayEdit();
    }
  }
  // --- Button debounce ---
  bool btn = digitalRead(BTN_PIN);
  if (btn != lastBtnState) {
    lastDebounce = millis();
    btnHandled = false;
  }
  if (!btnHandled && (millis() - lastDebounce) > 50 && btn == LOW) {
    btnHandled = true;
    if (appState == STATE_MENU) {
      appState = STATE_EDIT;
      displayEdit();
    } else {
      applyNewValue(encoderInputVal);
      appState = STATE_MENU;
      displayMenu();
    }
  }
  lastBtnState = btn;
}

void updateDisplayEdit() {
  lcd.setCursor(0, 2);
  lcd.print("New Val:           ");
  lcd.setCursor(9, 2);
  lcd.print(encoderInputVal);
  lcd.print(" ");
  lcd.print(menuUnits[menuIndex]);
}
