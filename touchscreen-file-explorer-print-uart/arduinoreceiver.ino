// --- Code for Arduino Uno/Nano ---
#include <AltSoftSerial.h>
#include <SoftwareSerial.h>


// ESP32 communication using AltSoftSerial (fixed pins: RX=8, TX=9)
AltSoftSerial espSerial;  // RX = Pin 8, TX = Pin 9


// Thermal printer on pins 2 (RX) and 3 (TX)
SoftwareSerial printerSerial(2, 3); // RX, TX


void setup() {
  // For debugging on the Serial Monitor (USB)
  Serial.begin(9600);
  Serial.println("Arduino is ready. Waiting for data...");


  // For communication with the ESP32
  espSerial.begin(9600);


  // For communication with the thermal printer
  printerSerial.begin(9600);
}


void loop() {
  // Check if data has arrived from the ESP32
  if (espSerial.available()) {
    // Read the incoming line
    String line = espSerial.readStringUntil('\n');
    line.trim();


    if (line.length() > 0) {
      // Debug output
      Serial.println("Received: " + line);


      // Send the line to the thermal printer
      printerSerial.println(line);
      Serial.println("Printed: " + line);


      // Send the required "OK" response back to the ESP32
      espSerial.println("OK");
    }
  }
}
