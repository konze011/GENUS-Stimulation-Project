/*
 40 Hz Flicker
 
 This code generates 40 Hz flicker at 50% duty cycle during which 
 the led strip turns on for 12.5 ms and then off for 12.5 ms.
 
 Equivalent audio flicker is a 10 khz tone (50% duty cycle square wave)
 turning on and off at 40 hz. (12.5 ms on and 12.5 ms off) 

 Switch Control
 SW1 - pin 3 - flicker rate (40 hz / random)
*/

// DISCUSS RANDOM DELAY. HOW BIG SHOULD THE RANGE BE?

// Pin variables used to set pin I/O
const int switchPin = 3;   // Pin used to read switch 1 state (HIGH = random, LOW = 40hz)
const int ledPin = 5;      // Pin used for digital LED strip signal
const int speakerPin = 7;  // Pin used for digital sound output

int ledState = LOW;
int speakerState = LOW;

// Timing variables used for accurate microsecond delays
unsigned long currentTime;
unsigned long previousTimeLED;
unsigned long previousTimeSpeaker;

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
    }       // End of LED modification
    else {  // Invalid modification
      Serial.println("Invalid stimulation method");
    }  // End of invalid modification
    while (Serial.available()) Serial.read();
    digitalWrite(ledPin, LOW);
    digitalWrite(speakerPin, LOW);
    previousTimeLED = micros();
    previousTimeSpeaker = previousTimeLED;
  }

  // Take switch input to determine stimulation frequency
  int switchValue = digitalRead(switchPin);

  // User input frequency
  if (switchValue == LOW) {
    currentTime = micros();

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
    currentTime = micros();

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