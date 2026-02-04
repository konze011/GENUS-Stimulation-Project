/*
 40 Hz Flicker
 
 This code generates 40 Hz flicker at 50% duty cycle during which 
 the led strip turns on for 12.5 ms and then off for 12.5 ms.
 
 Equivalent audio flicker is a 10 khz tone (50% duty cycle square wave)
 turning on and off at 40 hz. (12.5 ms on and 12.5 ms off) 

 Switch Control
 SW1 - pin 3 - flicker rate (40 hz / random)
*/

// ASK ABOUT DUTY CYCLE
// SHOULD THE DUTY CYCLE BE 50%
// SHOULD THE DUTY CYCLE BE EASILY CHANGED

// ASK ABOUT SPEAKERS
// WHAT SHOULD THE FREQUENCY BE
// WHAT SHOULD THE AMPLITUDE BE
// HOW DOES THE PSUEDO FREQUENCY WORK

// ASK ABOUT SHAM FREQUENCY
// ASK HOW THE SHAM FREQUENCY SHOULD BE IMPLEMENTED
// CONSTANT ON VARIABLE OFF
// VARIABLE ON CONSTANT OFF
// VARIABLE ON VARIABLE OFF
// SOME OTHER IMPLEMENTATION

// Pin variables used to set pin I/O
const int switchPin = 3;  // Pin used to read switch 1 state (HIGH = random, LOW = 40hz)
const int ledPin = 5;     // Pin used for digital LED strip signal
const int soundPin = 7;   // Pin used for digital sound output

// Timing variables used for accurate microsecond delays
int startTime;
int endTime;

// Delay variables used to set the duration of stimulation frequency delays
int delay1khz = 500;  //  1 kHz ~ 1000 microseconds per cycle or 500 microseconds at 50% duty cycle
// int delay10khz = 50;	  // Calculated value for 10 kHZ ~ 100 microseconds per cycle or 50 microseconds at 50% duty cycle
int delay10khz = 43;    // Adjusted value based on oscilloscope measurement
int delay40hz = 12500;  // 40 hz ~ 25000 microseconds per cycle or 12500 microseconds at 50% duty cycle
int delayRnd;           // Randomized delay value that should average 12500 microseconds


void setup() {
  // Initialize serial output
  Serial.begin(9600);

  // Initialize pin state for input/output
  pinMode(soundPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(switchPin INPUT);

  // Initialize random function by reading from an unconnected pin
  randomSeed(analogRead(0));
}

void loop() {
  // Take switch input to determine stimulation frequency
  int switchValue = digitalRead(switchPin);

  // 40 Hz - first half output is 12.5 ms, second half delay is 12.5 ms (50% duty cycle)
  if (switchValue == LOW) {
    Serial.print("40 Hz\n");  // Debug output. Delete later.
    // Begin "on" half of 40 Hz cycle
    startTime = micros();
    endTime = startTime;

    // Turn on LED string
    digitalWrite(ledPin, HIGH);

    // This seems really weird. Look into speakers/speaker code.
    // Turn on speakers at "10 kHz"
    // Timing loop for 10 kHz sound
    while ((endTime - startTime) < delay40hz) {
      digitalWrite(soundPin, HIGH);   // 10 kHz -- 50% duty cycle
      delayMicroseconds(delay10khz);  //        -- on for 50 microseconds

      digitalWrite(soundPin, LOW);    // Turns signal Off
      delayMicroseconds(delay10khz);  // Wait 50 microseconds

      endTime = micros();
    }

    // Begin "off" half of 40 Hz cycle
    digitalWrite(ledPin, LOW);
    delayMicroseconds(delay40hz);
  }
  // RANDOM - first half output is 12.5 ms, second half delay is a random amount of time averaging 12.5 ms
  else {
    Serial.print("Random\n");  // Debug output. Delete later.
    // Begin "on" half of sham cycle
    startTime = micros();
    endTime = startTime;

    // Turn on LED string
    digitalWrite(ledPin, HIGH);

    // Timing loop for first half of cycle and optional sound output
    while ((endTime - startTime) < delay40hz) {
      digitalWrite(soundPin, HIGH);   // 10 kHz -- 50% duty cycle
      delayMicroseconds(delay10khz);  //        -- on for 50 microseconds

      digitalWrite(soundPin, LOW);    // Turns signal Off
      delayMicroseconds(delay10khz);  // Wait 50 microseconds
      endTime = micros();
    }

    // Begin "off" half of 40 Hz cycle
    digitalWrite(ledPin, LOW);
    // set up the random delay -- code is provided for mili- or microsecond accuracy of delay
    // delayRnd = random(8000, 17001);  // random value between 8000 and 17000 -- should average 12500 microseconds
    // delayMicroseconds(delayRnd);     // second half of duty cycle varies from 8 to 17 ms - average ~12.5 ms
    delayRnd = random(8, 18);  // random value between 8 and 17 milliseconds, which should average 12.5 ms
    delay(delayRnd);           // delay the desired number of milliseconds
  }                            // random
}
