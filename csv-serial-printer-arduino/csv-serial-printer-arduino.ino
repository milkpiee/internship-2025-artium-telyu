#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h>


// --- SD Card ---
const int chipSelectPin = 4;


// --- Printer Pins ---
#define PRINTER_RX 2  // To printer TX
#define PRINTER_TX 3  // To printer RX
SoftwareSerial printer(PRINTER_RX, PRINTER_TX);


void setup() {
  Serial.begin(9600);
  printer.begin(9600); // Default baud for Goojprt printers


  while (!Serial);


  Serial.println("\nInitializing SD card...");
  if (!SD.begin(chipSelectPin)) {
    Serial.println("Initialization failed! Halting.");
    while (1);
  }


  Serial.println("✅ Initialization successful.");
  Serial.println("\n--- Ready for Commands ---");
  Serial.println("Send 'list' to see available .csv files.");
  Serial.println("--------------------------");


  // Initialize printer
  printer.write(27); printer.write(64); // ESC @ - reset printer
}


void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();


    if (command.equalsIgnoreCase("list")) {
      listCsvFiles();
    } else if (command.endsWith(".csv") || command.endsWith(".CSV")) {
      printCsvFile(command);
    } else if (command.length() > 0) {
      Serial.print("Unknown command: ");
      Serial.println(command);
      Serial.println("Please send 'list' or a valid .csv filename.");
    }
  }
}


void listCsvFiles() {
  File root = SD.open("/");
  if (!root) {
    Serial.println("Failed to open root directory");
    return;
  }


  Serial.println("\n--- Available CSV Files ---");
  bool csvFound = false;


  root.rewindDirectory();
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;


    String name = String(entry.name());
    if (name.endsWith(".csv") || name.endsWith(".CSV")) {
      Serial.println(entry.name());
      csvFound = true;
    }
    entry.close();
  }


  if (!csvFound) {
    Serial.println("No .csv files found.");
  }
  Serial.println("---------------------------");
  Serial.println("Type a filename to print it.");
  root.close();
}


String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;


  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}


// --- MODIFIED: This function is now more robust against missing data ---
void printCsvFile(String filename) {
  File dataFile = SD.open(filename.c_str());


  if (dataFile) {
    Serial.print("Printing file: ");
    Serial.println(filename);


    // --- Header with advanced formatting ---
    printer.write(27); printer.write('a'); printer.write((byte)1);   // Center align
    printer.write(29); printer.write('!'); printer.write((byte)16);  // Double height
    printer.println("DATA LOG");
    printer.write(29); printer.write('!'); printer.write((byte)0);   // Normal size text
    printer.println("Date: 17/07/2025");
    printer.println();
    printer.write(27); printer.write('a'); printer.write((byte)0);   // Left align
    printer.println("No.  Time      Flow    SetPt");
    printer.println("--------------------------------");


    // Skip CSV header
    if (dataFile.available()) {
      dataFile.readStringUntil('\n');
    }


    int rowNumber = 0;
    while (dataFile.available()) {
      String line = dataFile.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;


      rowNumber++;


      // --- NEW: Robust data handling ---
      String datetimeStr = getValue(line, ',', 0);
      String flowStr     = getValue(line, ',', 1);
      String setpointStr = getValue(line, ',', 2);
     
      // Extract time from datetime string
      String timeStr = datetimeStr;
      int spaceIndex = datetimeStr.indexOf(' ');
      if (spaceIndex != -1) {
        timeStr = datetimeStr.substring(spaceIndex + 1);
      }
     
      // If any value is missing, replace it with a placeholder
      if (timeStr.length() == 0) timeStr = "---";
      if (flowStr.length() == 0) flowStr = "---";
      if (setpointStr.length() == 0) setpointStr = "---";
     
      char buffer[64];
      sprintf(buffer, "%02d   %-9s %7s %s", rowNumber, timeStr.c_str(), flowStr.c_str(), setpointStr.c_str());
     
      printer.println(buffer);
      Serial.println(buffer); // Also print to serial monitor for debugging
    }


    // --- Footer with QR Code REMOVED ---
    printer.println("--------------------------------");
    printer.write(27); printer.write('a'); printer.write((byte)1); // Center align
    printer.println("Log Complete");
   
    printer.println();
    printer.println();
    printer.write(29); printer.write('V'); printer.write((byte)1); // Cut the paper


    dataFile.close();
    Serial.println("✅ Done printing.");
  } else {
    Serial.print("Error opening: ");
    Serial.println(filename);
  }
}
