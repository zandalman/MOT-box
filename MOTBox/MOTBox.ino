// Include libraries
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>
#include "Adafruit_MAX31855.h"

// User defined parameters
const float warningTemp = 30;
const float thresholdTemp = 35;
const float holdTime = 5;
const float interlockTime = 2;
const float loopDelay = 0.5;

// Define pins
const int interlockPin = 10;
const int TCpins[] = {23, 22, 21, 20, 17, 16};
const int flowPin = 3;
const int currentPin = A0;
const int MISOpin = 12;
const int SCKpin = 13;

// Initialize the lcd screen
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

// Initialize the thermocouples
Adafruit_MAX31855 TCs[] = {
    Adafruit_MAX31855(SCKpin, TCpins[0], MISOpin),
    Adafruit_MAX31855(SCKpin, TCpins[1], MISOpin),
    Adafruit_MAX31855(SCKpin, TCpins[2], MISOpin),
    Adafruit_MAX31855(SCKpin, TCpins[3], MISOpin),
    Adafruit_MAX31855(SCKpin, TCpins[4], MISOpin),
    Adafruit_MAX31855(SCKpin, TCpins[5], MISOpin)
};

// Define lcd backlight states
#define OFF 0x0
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

// Define interlock status values
const int NORMAL = 0;
const int WARNING = 1;
const int DANGER = 2;

// Define lcd display modes
const int NUMERIC = 0;
const int VISUAL = 1;

// Define lcd channels
const String channels[] = {
  "ch1: TC",
  "ch2: TC",
  "ch3: TC",
  "ch4: TC",
  "ch5: TC",
  "ch6: TC",
  "ch7: flowmeter",
  "ch8: current"
};

// Define variables
int channel = 0;
float currentInterlockTime = 0;
float currentHoldTime = 0;
int interlockStatus = NORMAL;
int displayMode = NUMERIC;
volatile double flow = 0;

// Define data arrays
const String dataLabels[] = {
  "TC1", "TC2", "TC3", "TC4", "TC5", "TC6",
  "TC1int", "TC2int", "TC3int", "TC4int", "TC5int", "TC6int",
  "flow", "current"
};
double dataValues[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// Return true if the select button is pushed and false otherwise
bool selectButtonPushed() {
  uint8_t buttons = lcd.readButtons();
  if (buttons) {
    if (buttons & BUTTON_SELECT) {
      return true;
    } else {
      return false;
    }
  } else {
    return false;
  }
}

// Activate the interlock
void activateInterlock() {
  // activate the interlock
  digitalWrite(interlockPin, LOW);
  // indicate that interlock is activated on lcd
  clearLCD();
  lcd.print("Interlock on");
  lcd.setBacklight(RED);
  // delay until select button has been held for hold time
  currentHoldTime = 0;
  while (currentHoldTime < holdTime) {
    if (selectButtonPushed()) {
      currentHoldTime += 0.2;
    } else {
      currentHoldTime = 0;
    }
    delay(200);
  }
  // deactivate the interlock
  digitalWrite(interlockPin, HIGH);
  // indicate that interlock is deactivated on lcd
  clearLCD();
  lcd.print("Interlock off");
  lcd.setBacklight(GREEN);
  delay(5000);
  // return to normal lcd display
  clearLCD();
  lcd.setBacklight(WHITE);
}

// Clear lcd and reset cursor
void clearLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
}

// Check for button presses
void checkLCDButtons() {
  uint8_t buttons = lcd.readButtons();
  if (buttons) {
    clearLCD();
    // change display mode with up and down buttons
    if (buttons & BUTTON_UP) {
      displayMode = 1 - displayMode;
    }
    if (buttons & BUTTON_DOWN) {
      displayMode = 1 - displayMode;
    }
    // change display channel with left and right buttons
    if (buttons & BUTTON_LEFT) {
      if (channel > 0) {
        channel -= 1;
      } else {
        channel = 7;
      }
    }
    if (buttons & BUTTON_RIGHT) {
      if (channel < 7) {
        channel += 1;
      } else {
        channel = 0;
      }
    }
    if (buttons & BUTTON_SELECT) {
      1+1; // do nothing
    }
  }
}

// Fill data array with data from sensors
void readSensors() {
  // read temperature
  for (int i = 0; i < 6; i++) {
    dataValues[i] = TCs[i].readCelsius();
    dataValues[i + 6] = TCs[i].readInternal();
  }
  // read flow rate
  // for model FL-S402B flowmeter, pulse frequency (Hz) = 23 * flow rate (L/min)
  dataValues[12] = 1000 * flow / (60 * 23 * loopDelay); // mL/s
  // reset pulses on flow meter
  flow = 0;
  // read current
  // for model HCSE current sensor, output (A) = input (A) / 200
  dataValues[13] = 200 * 5 * analogRead(currentPin) / 1024; // A
}

// Print data array to lcd
void printToLCD() {
  clearLCD();
  lcd.print(channels[channel]);
  lcd.setCursor(0, 1);
  if (displayMode == NUMERIC) {
    if (channel < 6) {
      lcd.print(dataValues[channel]);
      lcd.print(" (");
      lcd.print(dataValues[channel + 6]);
      lcd.print(")");
    } else if (channel == 6) {
      lcd.print(dataValues[12]);
    } else if (channel == 7) {
      lcd.print(dataValues[13]);
    }
  } else if (displayMode == VISUAL) {
    if (channel < 6) {
      int numBlocks = 16 * min(1, dataValues[channel] / thresholdTemp);
      for (int i = 0; i < numBlocks; i++) {
        lcd.write(255);
      }
    }
  }
}

// Print data area to serial
void printToSerial() {
  for (int i = 0; i < 13; i++) {
    Serial.print(dataValues[i]);
    Serial.print(",");
  }
  Serial.print(dataValues[14]);
  Serial.println(",");
}

// Check interlock status
void checkInterlock() {
  // check interlock status
  interlockStatus = NORMAL;
  for (int i = 0; i < 12; i++) {
    if (dataValues[i] > thresholdTemp) {
      channel = i;
      interlockStatus = DANGER;
    } else if (dataValues[i] > warningTemp) {
      channel = i;
      interlockStatus = WARNING;
    }
  }
  // indicate interlock status on lcd
  if (interlockStatus == NORMAL) {
    lcd.setBacklight(WHITE);
    currentInterlockTime = 0;
  } else if (interlockStatus == WARNING) {
    lcd.setBacklight(YELLOW);
  } else if (interlockStatus == DANGER) {
    if (currentInterlockTime > interlockTime) {
      activateInterlock();
    } else {
      currentInterlockTime += loopDelay;
    }
  }
}

void pulse() {
  flow += 1;
}

void setup() {
  delay(1000);
  // begin serial
  Serial.begin(9600);
  // set output pins
  pinMode(interlockPin, OUTPUT);
  // attach interrupt to flow pin
  attachInterrupt(flowPin, pulse, RISING);
  // start the lcd
  lcd.begin(16, 2);
  lcd.setBacklight(WHITE);
  // turn off the interlock
  digitalWrite(interlockPin, HIGH);
  delay(500);
}

void loop() {
  checkLCDButtons();
  readSensors();
  checkInterlock();
  printToLCD();
  printToSerial();
  delay(loopDelay * 1000);
}
