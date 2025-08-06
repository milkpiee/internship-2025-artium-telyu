# internship-2025-artium-telyu

This repository showcases the development of a touchscreen-based HMI system for a High Volume Air Sampler (HVAS) device during our internship at PT Artium Multikarya Indonesia, from June 30 to August 9, 2025. The project features real-time sensor logging, a file explorer UI with button and touchscreen input, SD card storage, brightness control, and thermal printing.

**🧠 Key Focus Areas**

- Development of a touchscreen-based and button input Human Machine Interface (HMI) for an HVAS device
- Real-time air quality data logging to SD card using ESP32
- File explorer UI with navigation via both I2C push buttons and touchscreen input
- Integration with a thermal printer for direct file/data printing
- Sensor data acquisition, processing, and visualization using LVGL
- Adjustable screen brightness for improved HMI usability

**🛠 Tools**

  ESP32-S3, ESP32-3248S035, Aduino UNO R3

  Arduino / C++

  LVGL v8.3.11 (UI library)

  I2C (PCF8574) for push buttons

  YF-S201 Water Flow Sensor 

  Thermal Printer Goojprt QR204 

  Thermal Printer KL-420 TTL

**📌 Notes**

- All projects are written in C++ (Arduino framework) for ESP32 boards.
- Tested primarily on ESP32-S3 and ESP32-3248S035 boards with TFT LCD displays.
- Hardware-specific configurations (e.g., pin assignments, display drivers) are included in each project folder.
- All UI elements are built using LVGL v8.3.11.
- SD card should be formatted as FAT32, and CSV/text files must follow expected structures for parsing and display.
- Thermal printer integration requires proper UART configuration and compatible command set (tested with 58mm printer).
- Some projects use I2C expanders (PCF8574) for input buttons – ensure proper pull-ups and address configuration.

**🤝 Acknowledgments**

Thank you to the R&D team at PT Artium Multikarya Indonesia for their guidance and support throughout this internship. Their mentorship helped me explore embedded systems through real-world development, integration, and testing processes.

