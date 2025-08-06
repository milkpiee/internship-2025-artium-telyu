#include <Wire.h>
#include "Adafruit_PCF8574.h" // Use the Adafruit library header

// Create an Adafruit PCF8574 object
Adafruit_PCF8574 pcf8574;

// An array to hold the button pin numbers on the PCF8574
const int buttonPins[] = {0, 1, 2, 3}; // Adafruit library uses simple integers for pins
const int numButtons = 4;

// Arrays to store the current and previous state of each button
int buttonStates[numButtons];
int lastButtonStates[numButtons];

void setup() {
  Serial.begin(115200);
  // Initialize the PCF8574 with its I2C address
  // Note: Pass the address to the begin() function
  if (!pcf8574.begin(0x20, &Wire)) {
    Serial.println("Couldn't find PCF8574 chip");
    while (1);
  }
  // With the Adafruit library, you need to set each pin as an input
  for (int i = 0; i < numButtons; i++) {
    pcf8574.pinMode(buttonPins[i], INPUT_PULLUP);
    lastButtonStates[i] = HIGH; // Initialize button state
  }
  Serial.println("I2C Button Project Initialized (Adafruit Library).");
}


void loop() {
  // Loop through each button
  for (int i = 0; i < numButtons; i++) {
    // Read the state of the current button from the PCF8574
    // Note: The function is now digitalRead()
    buttonStates[i] = pcf8574.digitalRead(buttonPins[i]);
    // Check if the button has just been pressed (transition from HIGH to LOW)
    if (buttonStates[i] == LOW && lastButtonStates[i] == HIGH) {
      // Send the corresponding character based on which button was pressed
      switch (i) {
        case 0:
          Serial.println("A");
          break;
        case 1:
          Serial.println("B");
          break;
        case 2:
          Serial.println("C");
          break;
        case 3:
          Serial.println("D");
          break;
      }
      // A short delay to debounce this specific button press
      delay(50);
    }
    // Save the current state for the next loop
    lastButtonStates[i] = buttonStates[i];
  }
  // A small delay to keep the main loop from running too fast
  delay(10);
}
