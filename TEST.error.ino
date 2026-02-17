/*
  Stable Flicker with Independent Audio
  - LED flicker timing is exact
  - Audio no longer distorts duty cycle
*/

const int switchPin = 3;
const int ledPin    = 5;
const int soundPin  = 7;

// Flicker control
float frequency = 40.0;
unsigned long halfPeriod = 12500;

// Timing state
unsigned long lastToggleTime = 0;
bool ledState = false;

// Audio timing (10 kHz square wave)
unsigned long lastSoundToggle = 0;
bool soundState = false;
const unsigned long soundHalfPeriod = 100; // ~10 kHz (adjusted)

void setup() {
  Serial.begin(9600);

  pinMode(ledPin, OUTPUT);
  pinMode(soundPin, OUTPUT);
  pinMode(switchPin, INPUT);

  randomSeed(analogRead(0));

  Serial.println("Enter flicker frequency in Hz (1–100):");

}

void loop() {
  unsigned long now = micros();

  /* -------- SERIAL FREQUENCY INPUT -------- */
  if (Serial.available() > 0) {
    frequency = Serial.parseFloat();

    if (frequency > 0 && frequency <= 100) {
      halfPeriod = (unsigned long)(500000.0 / frequency);
      Serial.print("Frequency set to ");
      Serial.print(frequency);
      Serial.println(" Hz");
    } else {
      Serial.println("Invalid frequency");
    }

    while (Serial.available()) Serial.read();
  }

  /* -------- LED FLICKER STATE MACHINE -------- */
  if (now - lastToggleTime >= halfPeriod) {
    lastToggleTime += halfPeriod;

    ledState = !ledState;
    digitalWrite(ledPin, ledState ? HIGH : LOW);
  }

  /* -------- SOUND OUTPUT (ONLY WHEN LED ON) -------- */
  if (ledState) {
    if (now - lastSoundToggle >= soundHalfPeriod) {
      lastSoundToggle += soundHalfPeriod;

      soundState = !soundState;
      digitalWrite(soundPin, soundState ? HIGH : LOW);
    }
  } else {
    digitalWrite(soundPin, LOW); // ensure silence during LED OFF
  }
}