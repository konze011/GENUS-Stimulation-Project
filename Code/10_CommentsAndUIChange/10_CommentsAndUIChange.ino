/*
 GENUS Stimulus Control
 
 This code generates audio and visual stimulation following GENUS
 guidelines. There are 9 configurable parameters shown on the LCD
 attached to the system. Input can be given through the serial
 monitor or with the rotary encoder.
*/

#include <Encoder.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Conversion factors to change from microseconds
#define TO_SECONDS 1000000
#define TO_MILIS 1000
#define TO_MINUTES 60000000

// Offset for the random function (can range from the offset to
// twice the LED off duration minus the offset)
#define RANDOM_OFFSET 0.2

// Setting which pins to use
#define BTN_PIN 4
#define LED_PIN 11
#define SPEAKER_PIN 6

// Setting up the rotary encoder
Encoder myEnc(2, 3);
int lastEncPos = 0;
bool lastBtnState = HIGH;
unsigned long lastDebounce = 0;
bool btnEdge = false;
unsigned long encoderInputVal;

// Setting up the speaker
unsigned int speakerTone = 10000;

// Timing variables
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
unsigned long randomActive;

// Setting up stimulation states
int ledState = LOW;
int speakerState = LOW;
int overallState = LOW;

// User serial inputs
float stimulationValue;
char stimulationMode;
char stimulationInput;

// Getters for displaying parameters on the LCD
typedef unsigned long (*ValueGetter)();
unsigned long getOverallOn() {
  return delayOverallOn / TO_MINUTES;
}
unsigned long getOverallOff() {
  return delayOverallOff / TO_MINUTES;
}
unsigned long getOverallRandom() {
  return randomActive;
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
const char* menuParams[] = {
  "O On Dur",
  "O Off Dur",
  "O Random",
  "S Tone",
  "S Duration",
  "S Freq",
  "S Phase",
  "L Duration",
  "L Freq"
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
int menuIndex = 0;           // Index of the hovered parameter
int menuScrollOffset = 0;    // Index of the first parameter to render
const int VISIBLE_ROWS = 4;  // Number of visible rows on the LCD

// Setting up LCD parameter list menu vs edit menu
enum AppState { STATE_PARAMS,
                STATE_EDIT };
AppState appState = STATE_PARAMS;

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
  displayParams();

  // Prompt the user to give an input
  Serial.println("To use serial input begin with 'O', 'S' or 'L' to indicate the stimulation mode.");
  Serial.println("Then type the first letter of what is displayed as a parameter on the LCD ('F', 'D', etc.).");
  Serial.println("Finally, type what you would like the new value of the parameter to be.");
  Serial.println("The units will automatically match what is shown on the LCD");
  Serial.println();
  Serial.println("Enter serial input:");
}

void loop(void) {
  // Read user frequency from Serial
  if (Serial.available() > 0) {
    serialInput();                             // Read and update values based on serial input
    displayParams();                           // Update the LCD to match the new parameter values
    while (Serial.available()) Serial.read();  // Remove extra characters from serial buffer
  }
  encoderInput();  // Check for encoder input

  currentTime = micros();  // Set current time for all timings

  overallStimLogic();   // Run system on/off logic
  if (!overallState) {  // Prevent stimulation if the system is off
    return;
  }

  stimulationLogic();  // Run stimulation logic
}

// Update what is shown on the LCD to show all modifiable parameters
void displayParams(void) {
  appState = STATE_PARAMS;  // Change the LCD to "show parameters" mode
  lcd.clear();              // Clear the LCD of old values

  // Print each individual row of the LCD
  for (int row = 0; row < VISIBLE_ROWS; row++) {
    int parameterIndex = menuScrollOffset + row;  // Variable describing the index of the parameters shown on the LCD
    if (parameterIndex >= menuLength) break;      // Prevent printing options outside the given list

    // Print the "hovered option" arrow
    lcd.setCursor(0, row);
    if (parameterIndex == menuIndex) {
      lcd.print(">");
    } else {
      lcd.print(" ");
    }

    // Print parameter, then value, then unit
    char lineBuf[20];
    snprintf(lineBuf, sizeof(lineBuf), "%-11s%4lu%-3s",
             menuParams[parameterIndex],
             menuValues[parameterIndex](),
             menuUnits[parameterIndex]);
    lcd.print(lineBuf);
  }
}

// Update what is shown on the LCD to show the edit parameter screen
void displayEdit(void) {
  appState = STATE_EDIT;                      // Change the LCD to "show parameters" mode
  encoderInputVal = menuValues[menuIndex]();  // Set the starting point value to the parameter's current value
  lcd.clear();                                // Clear the LCD of old values

  // Row 0 shows the editing label
  lcd.setCursor(0, 0);
  lcd.print("Editing:");

  // Row 1 shows the parameter name
  lcd.setCursor(0, 1);
  lcd.print(menuParams[menuIndex]);

  // Row 2 shows the new value and its unit
  char valBuf[21];
  snprintf(valBuf, sizeof(valBuf), "  Value: %3lu %s", encoderInputVal, menuUnits[menuIndex]);
  lcd.setCursor(0, 2);
  lcd.print(valBuf);

  // Row 3 shows how to finalize the new value
  lcd.setCursor(0, 3);
  lcd.print("[Click to confirm]");
}

// Read serial input and adjust parameters accordingly
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
      // Tone
      case 'T':
        if (stimulationValue) {
          // menuIndex = 3;
          // applyNewValue(stimulationInput);
          speakerTone = stimulationValue;
          Serial.print("tone changed to: ");
        } else {
          Serial.print("tone is currently: ");
        }
        Serial.print(speakerTone);
        Serial.println(" Hz");
        break;

      // On duration
      case 'D':
        // menuIndex = 4;
        // applyNewValue(stimulationInput);
        stimulationValue *= TO_MILIS;
        if (stimulationValue <= delaySpeakerOn + delaySpeakerOff) {
          delaySpeakerOff = delaySpeakerOn + delaySpeakerOff - stimulationValue;
          delaySpeakerOn = stimulationValue;
          Serial.println("duration changed");
        } else {
          Serial.println("duration unchanged. New duration would be longer than total period");
        }
        printSpeakerInfo();
        break;

      // Frequency
      case 'F':
        // menuIndex = 5;
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
        printSpeakerInfo();
        break;

      // Phase
      case 'P':
        // menuIndex = 6;
        // applyNewValue(stimulationInput);
        if (stimulationValue) {
          delayPhase = stimulationValue * TO_MILIS;
          Serial.print("phase changed to: ");
        } else {
          Serial.print("phase is currently: ");
        }
        Serial.print(delayPhase / TO_MILIS);
        Serial.print(" ms");
        break;

      // Invalid speaker parameter
      default:
        Serial.println("Invalid speaker parameter");
        break;
    }  // End of switch
  }    // End of speaker modification

  // LED modification
  else if (stimulationMode == 'L') {
    Serial.print("LED ");
    switch (stimulationInput) {
      // On duration
      case 'D':
        // menuIndex = 7;
        // applyNewValue(stimulationInput);
        stimulationValue *= TO_MILIS;
        if (stimulationValue <= delayLEDOn + delayLEDOff) {
          delayLEDOff = delayLEDOn + delayLEDOff - stimulationValue;
          delayLEDOn = stimulationValue;
          Serial.println("duration changed");
        } else {
          Serial.println("duration unchanged. New duration would be longer than total period");
        }
        printLEDInfo();
        break;

      // Frequency
      case 'F':
        // menuIndex = 8;
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
        printLEDInfo();
        break;

      // Invalid LED parameter
      default:
        Serial.println("Invalid LED parameter");
        break;
    }  // End of switch
  }    // End of LED modification

  // Overall modification
  else if (stimulationMode == 'O') {
    Serial.print("Overall ");
    switch (stimulationInput) {
      // On duration
      case 'N':
        // menuIndex = 0;
        // applyNewValue(stimulationInput);
        if (stimulationValue) {
          delayOverallOn = stimulationValue * TO_MINUTES;
          Serial.print("on duration changed to: ");
        } else {
          Serial.print("on duration is currently: ");
        }
        Serial.print(delayOverallOn / TO_MINUTES);
        Serial.println(" minutes");
        break;

      // Off duration
      case 'F':
        if (stimulationValue) {
          delayOverallOff = stimulationValue * TO_MINUTES;
          Serial.println("off duration changed to: ");
        } else {
          Serial.print("off duration is currently: ");
        }
        Serial.println(delayOverallOff / TO_MINUTES);
        Serial.println(" minutes");
        break;

      // Random activation
      case 'R':
        randomActive = stimulationValue;
        Serial.print("random is ");
        if (randomActive == 0) {
          Serial.println("disabled");
        } else {
          Serial.println("active");
        }
        break;

      // Invalid overall parameter
      default:
        Serial.println("Invalid overall parameter");
        break;
    }  // End of switch
  }    //End of overall modification

  // Invalid mode
  else {
    Serial.println("Invalid stimulation mode");
  }  // End of invalid mode

  stimulationReset();  // Reset stimulation timers
}

// Print speaker frequency info
void printSpeakerInfo(void) {
  Serial.println();

  Serial.print("Speaker frequency: ");
  Serial.print(TO_SECONDS / (double)(delaySpeakerOn + delaySpeakerOff));
  Serial.println(" Hz");

  Serial.print("Speaker on duration: ");
  Serial.print(((double)delaySpeakerOn) / TO_MILIS);
  Serial.println(" ms");

  Serial.print("Speaker off duration: ");
  Serial.print(((double)delaySpeakerOff) / TO_MILIS);
  Serial.println(" ms");
}

// Print LED frequency info
void printLEDInfo(void) {
  Serial.println();

  Serial.print("LED frequency: ");
  Serial.print(TO_SECONDS / (double)(delayLEDOn + delayLEDOff));
  Serial.println(" Hz");

  Serial.print("LED on duration: ");
  Serial.print(((double)delayLEDOn) / TO_MILIS);
  Serial.println(" ms");

  Serial.print("LED off duration: ");
  Serial.print(((double)delayLEDOff) / TO_MILIS);
  Serial.println(" ms");
}

// changes parameter at index newVal in LCD parameter list
void applyNewValue(float newVal) {
  switch (menuIndex) {
    case 0: delayOverallOn = newVal * TO_MINUTES; break;   // Overall on
    case 1: delayOverallOff = newVal * TO_MINUTES; break;  // Overall off
    case 2: randomActive = newVal; break;                  // Overall random
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
  stimulationReset();  // Reset stimulation timers
}

// "Reset" stimulation state and phase offset
void stimulationReset(void) {
  // Turn off LEDs and speakers
  digitalWrite(LED_PIN, LOW);
  digitalWrite(SPEAKER_PIN, LOW);

  // Adjust the "previous time flipped" portion of the delay math
  previousTimeSpeaker = currentTime;
  previousTimeLED = previousTimeSpeaker - delayPhase;
  previousTimeOverall = currentTime;

  // Adjust the "target delay" portion of the delay math
  delaySpeaker = delaySpeakerOff;
  delayLED = delayLEDOff;
  delayOverall = delayOverallOn;
}

// Overall frequency logic
void overallStimLogic(void) {
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

// LED and speaker stimulation logic
void stimulationLogic(void) {
  // Non-random operation
  if (randomActive == 0) {
    // LED delay math
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

    // Speaker delay math
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

  // Random operation
  else {
    delayRandom = random(RANDOM_OFFSET * TO_MILIS, 2 * delayLEDOff - RANDOM_OFFSET * TO_MILIS);

    // LED delay math
    if (currentTime - previousTimeLED >= delayLED) {
      if (ledState) {
        delayLED = delayRandom;
      } else {
        delayLED = delayLEDOn;
      }
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      previousTimeLED = micros();
    }

    // Speaker delay math
    if (currentTime - previousTimeSpeaker >= delaySpeaker) {
      if (speakerState) {
        delaySpeaker = delayLEDOn + delayRandom - delay1ms;
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

// Handle when the encoder is turned regardless of LCD display state
void encoderInput(void) {
  int encPos = myEnc.read() / 4;

  // Handle turning input
  if (encPos != lastEncPos) {
    int dir = (encPos > lastEncPos) ? 1 : -1;
    lastEncPos = encPos;

    // Parameter menu
    if (appState == STATE_PARAMS) {
      // Ensure the escape the parameter list
      menuIndex = constrain(menuIndex + dir, 0, menuLength - 1);

      // Leave a one parameter buffer at the top when possible while scrolling up
      if (menuIndex > 0
          && menuIndex < menuScrollOffset + 1
          && menuScrollOffset > 0) {
        menuScrollOffset--;
      }

      // Leave a one parameter buffer at the bottom when possible while scrolling down
      if (menuIndex < menuLength - 1
          && menuIndex >= menuScrollOffset + VISIBLE_ROWS - 1
          && menuScrollOffset < menuLength - VISIBLE_ROWS) {
        menuScrollOffset++;
      }

      displayParams();
    }

    // Edit menu
    else {
      // Ensure the new value won't overflow or underflow before changing it
      if (0 < encoderInputVal && encoderInputVal < 4294967295) {
        encoderInputVal += dir;
      }
      char valBuf[21];
      snprintf(valBuf, sizeof(valBuf), "  Value: %3lu %s", encoderInputVal, menuUnits[menuIndex]);
      lcd.setCursor(0, 2);
      lcd.print(valBuf);

      // lcd.setCursor(0, 2);
      // lcd.print("New Val:           ");
      // lcd.setCursor(9, 2);
      // lcd.print(encoderInputVal);
      // lcd.print(" ");
      // lcd.print(menuUnits[menuIndex]);
    }
  }

  // Handle button input
  bool btn = digitalRead(BTN_PIN);
  if (btn != lastBtnState) {
    lastDebounce = currentTime;
    btnEdge = true;
  }
  if (btnEdge
      && (currentTime - lastDebounce) > (50 * TO_MILIS)
      && btn == LOW) {
    btnEdge = false;

    // Button pressed on parameter menu
    if (appState == STATE_PARAMS) {
      displayEdit();
    }

    // Button pressed on edit menu
    else {
      applyNewValue(encoderInputVal);
      displayParams();
    }
  }
  lastBtnState = btn;
}
