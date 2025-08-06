#include <Wire.h>
#include "Adafruit_PCF8574.h"
#include <Adafruit_NeoPixel.h>

// --- I2C Button Setup ---
Adafruit_PCF8574 pcf8574;
const int buttonPins[] = {0, 1, 2, 3};
const int numButtons = 4;
int buttonStates[numButtons];
int lastButtonStates[numButtons];

// --- Onboard RGB LED Setup ---
#define LED_PIN    48 // GPIO pin for the onboard NeoPixel LED
#define NUM_LEDS   1  // There is only one LED on the board

// Create a NeoPixel object
Adafruit_NeoPixel pixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  // --- Initialize I2C Expander ---
  if (!pcf8574.begin(0x20, &Wire)) {
    Serial.println("Couldn't find PCF8574 chip");
    while (1);
  }


  for (int i = 0; i < numButtons; i++) {
    pcf8574.pinMode(buttonPins[i], INPUT_PULLUP);
    lastButtonStates[i] = HIGH;
  }

  // --- Initialize RGB LED ---
  pixel.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  pixel.clear();           // Set pixel 'off' on startup
  pixel.setBrightness(50); // Set brightness to a medium-low value (0-255)
  Serial.println("Project Initialized. Press buttons to control the LED.");
}

void loop() {
  // Loop through each button
  for (int i = 0; i < numButtons; i++) {
    buttonStates[i] = pcf8574.digitalRead(buttonPins[i]);

    // Check if a button has just been pressed
    if (buttonStates[i] == LOW && lastButtonStates[i] == HIGH) {

      // Take action based on which button was pressed
      switch (i) {
        case 0: // First button: Set color to RED
          Serial.println("Button A: Set LED to RED");
          pixel.setPixelColor(0, pixel.Color(255, 0, 0)); // Red
          break;
        case 1: // Second button: Set color to GREEN
          Serial.println("Button B: Set LED to GREEN");
          pixel.setPixelColor(0, pixel.Color(0, 255, 0)); // Green
          break;
        case 2: // Third button: Set color to BLUE
          Serial.println("Button C: Set LED to BLUE");
          pixel.setPixelColor(0, pixel.Color(0, 0, 255)); // Blue
          break;
        case 3: // Fourth button: Turn the light out (RESET)
          Serial.println("Button D: Turn LED OFF");
          pixel.clear(); // Same as setting color to (0, 0, 0)
          break;
      }
      pixel.show();   // Send the updated color to the pixel!
      delay(50);      // Debounce delay
    }

    lastButtonStates[i] = buttonStates[i];
  }
}


