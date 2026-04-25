#include <Encoder.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

Encoder myEnc(2, 3);
LiquidCrystal_I2C lcd(0x27, 20, 4);

// --- Menu Configuration ---
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

// -------------------------------------------------------
void setup() {
  pinMode(BTN_PIN, INPUT_PULLUP);
  lcd.init();
  lcd.backlight();
  displayMenu();
  tone(6, 10000);
}

// -------------------------------------------------------
void loop() {
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
}

// -------------------------------------------------------
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

// -------------------------------------------------------
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