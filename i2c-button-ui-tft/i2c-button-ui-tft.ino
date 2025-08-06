#include <Wire.h>
#include <TFT_eSPI.h>
#include <Adafruit_PCF8574.h>

TFT_eSPI tft = TFT_eSPI();
Adafruit_PCF8574 pcf;

// P0–P3 used for buttons A–D
const int buttonPins[] = {0, 1, 2, 3};
const int numButtons = 4;

int lastStates[numButtons] = {HIGH, HIGH, HIGH, HIGH};
unsigned long lastPressTime[numButtons] = {0};
bool isHighlighted[numButtons] = {false};

unsigned long highlightDuration = 500; // milliseconds

const int textX = 80;
const int startY = 60;
const int spacingY = 50;

// Draw button label (highlighted if needed)
void drawButton(int idx, bool highlight = false) {
  int y = startY + idx * spacingY;
  tft.setTextColor(highlight ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(textX, y);
  tft.print("Button ");
  tft.print((char)('A' + idx));
}

void setup() {
  Serial.begin(115200);

  // TFT Init
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // Draw initial button labels
  for (int i = 0; i < numButtons; i++) {
    drawButton(i, false);
  }

  // I2C + PCF8574 Init
  Wire.begin(21, 22); // SDA, SCL
  if (!pcf.begin(0x20, &Wire)) {
    Serial.println("PCF8574 not found!");
    while (1);
  }

  // Configure button pins with internal pull-ups
  for (int i = 0; i < numButtons; i++) {
    pcf.pinMode(buttonPins[i], INPUT_PULLUP);
  }
}

void loop() {
  for (int i = 0; i < numButtons; i++) {
    int currentState = pcf.digitalRead(buttonPins[i]);
    if (lastStates[i] == HIGH && currentState == LOW) {
      Serial.printf("Button %c Pressed\n", 'A' + i);
      drawButton(i, true);
      isHighlighted[i] = true;
      lastPressTime[i] = millis();
    }
    if (isHighlighted[i] && (millis() - lastPressTime[i] >= highlightDuration)) {
      drawButton(i, false);
      isHighlighted[i] = false;
    }
    lastStates[i] = currentState;
  }
  delay(10);
}
