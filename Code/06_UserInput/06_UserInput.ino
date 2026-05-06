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

#define TOSECONDS 1000000

// Pin variables used to set pin I/O
const int switchPin = 5;   // Pin used to read switch 1 state (HIGH = random, LOW = 40hz)
const int ledPin = 11;      // Pin used for digital LED strip signal
const int speakerPin = 6;  // Pin used for digital sound output
Encoder myEnc(3, 2);
LiquidCrystal_I2C lcd(0x27, 20, 4);

const int menuLength = 5;

const char* menuItems[] = {
  "S Base Freq",
  "S Duration",
  "S Stim Freq",
  "L Duration",
  "L Stim Freq"
};

const char* menuUnits[] = {
  "Hz",
  "ms",
  "Hz",
  "ms",
  "Hz"
};

int menuValues[menuLength] = {50, 50, 50, 50, 50}; // default values

// --- State ---
enum AppState { STATE_MENU, STATE_EDIT };
AppState appState = STATE_MENU;

int menuIndex = 0;
int menuScrollOffset = 0; // which item is shown on row 0
const int VISIBLE_ROWS = 3; // row 3 reserved for instructions

const int BTN_PIN = 4;
long lastEncPos = 0;
bool lastBtnState = HIGH;
unsigned long lastDebounce = 0;
bool btnHandled = false;

int ledState = LOW;
int speakerState = LOW;
int overallState = LOW;

// Timing variables used for accurate microsecond delays
unsigned long currentTime;
unsigned long previousTimeLED;
unsigned long previousTimeSpeaker;
unsigned long previousTimeOverall;

float frequencyLED;
float frequencySpeaker;

// Default = 40 hz
unsigned const long delay40Hz = 12500;
unsigned const long delay1ms = 1000;
unsigned long delayLED;
unsigned long delayLEDOn = delay40Hz;
unsigned long delayLEDOff = delay40Hz;
unsigned long delaySpeaker;
unsigned long delaySpeakerOn = delay40Hz;
unsigned long delaySpeakerOff = delay40Hz;
unsigned long delayRandom;
unsigned long delayOverall;
unsigned long delayOverallOn = 0.05 * 60 * TOSECONDS;
unsigned long delayOverallOff = 0.05 * 60 * TOSECONDS;
unsigned long delayPhase;

// User inputs
float stimulationValue;
char stimulationMode;
char stimulationInput;
unsigned int speakerTone = 10000;

void setup() {
  // Initialize serial output
  Serial.begin(9600);

  // Initialize pin state for input/output
  pinMode(speakerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(switchPin, INPUT);

  // Initialize random function by reading from an unconnected pin
  randomSeed(analogRead(0));

  pinMode(BTN_PIN, INPUT_PULLUP);
  lcd.init();
  lcd.backlight();
  displayMenu();

  // Prompt the user to give an input
  Serial.println("\nEnter flicker frequency in Hz (e.g. 40):");
}

void loop() {
  // Read user frequency from Serial
  if (Serial.available() > 1) {
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
          stimulationValue *= 1000;
          if (stimulationValue <= delaySpeakerOn + delaySpeakerOff) {
            delaySpeakerOff = delaySpeakerOn + delaySpeakerOff - stimulationValue;
            delaySpeakerOn = stimulationValue;
            Serial.println("duration changed");
          } else {
            Serial.println("duration longer than total period");
          }
          break;
        case 'F':
          stimulationValue = 1000000 / stimulationValue;
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
          delayPhase = stimulationValue * 1000;
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
          stimulationValue *= 1000;
          if (stimulationValue <= delayLEDOn + delayLEDOff) {
            delayLEDOff = delayLEDOn + delayLEDOff - stimulationValue;
            delayLEDOn = stimulationValue;
            Serial.println("duration changed");
          } else {
            Serial.println("duration longer than total period");
          }
          break;
        case 'F':
          stimulationValue = 1000000 / stimulationValue;
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
          delayOverallOn = stimulationValue * 60 * TOSECONDS;
          break;
        case 'F':
          delayOverallOff = stimulationValue * 60 * TOSECONDS;
          break;
        default:
          Serial.println("Invalid overall configuration");
          break;
      }
    } else {  // Invalid modification
      Serial.println("Invalid stimulation method");
    }  // End of invalid modification
    while (Serial.available()) Serial.read();

    // "reset" stimulation state and phase offset
    digitalWrite(ledPin, LOW);
    digitalWrite(speakerPin, LOW);
    if (delaySpeakerOn + delaySpeakerOff == delayLEDOn + delayLEDOff) {
      previousTimeSpeaker = micros();
      previousTimeLED = previousTimeSpeaker - delayPhase;
    } else {
      previousTimeSpeaker = micros();
      previousTimeLED = previousTimeSpeaker;
    }
  }

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
      menuValues[menuIndex] = constrain(menuValues[menuIndex] + dir, 0, 100);
      displayEdit();
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
      appState = STATE_MENU;
      displayMenu();
    }
  }
  lastBtnState = btn;

  // Set current time for all timings
  currentTime = micros();

  // Overall frequency
  if (currentTime - previousTimeOverall >= delayOverall) {
    if (overallState) {
      digitalWrite(ledPin, LOW);
      noTone(speakerPin);
      delayOverall = delayOverallOff;
    } else {
      delayOverall = delayOverallOn;

      // "reset" stimulation state and phase offset
      digitalWrite(ledPin, LOW);
      digitalWrite(speakerPin, LOW);
      if (delaySpeakerOn + delaySpeakerOff == delayLEDOn + delayLEDOff) {
        previousTimeSpeaker = micros();
        previousTimeLED = previousTimeSpeaker - delayPhase;
      } else {
        previousTimeSpeaker = micros();
        previousTimeLED = previousTimeSpeaker;
      }
    }
    overallState = !overallState;
    previousTimeOverall = micros();
  }

  if (!overallState) {
    return;
  }

  // Take switch input to determine stimulation frequency
  int switchValue = digitalRead(switchPin);

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
      digitalWrite(ledPin, ledState);
      previousTimeLED = micros();
    }

    if (currentTime - previousTimeSpeaker >= delaySpeaker) {
      if (speakerState) {
        delaySpeaker = delaySpeakerOff;
        noTone(speakerPin);
      } else {
        delaySpeaker = delaySpeakerOn;
        tone(speakerPin, speakerTone);
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
      digitalWrite(ledPin, ledState);
      previousTimeLED = micros();
    }

    if (currentTime - previousTimeSpeaker >= delaySpeaker) {
      if (speakerState) {
        delaySpeaker = delay40Hz + delayRandom - delay1ms;
        noTone(speakerPin);
      } else {
        delaySpeaker = delay1ms;
        tone(speakerPin, speakerTone);
      }
      speakerState = !speakerState;
      previousTimeSpeaker = micros();
    }
  }
}

void displayMenu() {
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
    char lineBuf[21];
    snprintf(lineBuf, sizeof(lineBuf), "%-13s%3d%-3s",
             menuItems[itemIndex],
             menuValues[itemIndex],
             menuUnits[itemIndex]);
    lcd.print(lineBuf);
  }

  // Row 3: instruction bar
  lcd.setCursor(0, 3);
  lcd.print("  [Click to edit]   ");
}

void displayEdit() {
  lcd.clear();

  // Row 0: label
  lcd.setCursor(0, 0);
  lcd.print("Editing:");

  // Row 1: item name
  lcd.setCursor(0, 1);
  lcd.print(menuItems[menuIndex]);

  // Row 2: value + unit, large and centered
  char valBuf[21];
  snprintf(valBuf, sizeof(valBuf), "  Value: %3d %s", menuValues[menuIndex], menuUnits[menuIndex]);
  lcd.setCursor(0, 2);
  lcd.print(valBuf);

  // Row 3: instruction
  lcd.setCursor(0, 3);
  lcd.print(" [Click to confirm] ");
}