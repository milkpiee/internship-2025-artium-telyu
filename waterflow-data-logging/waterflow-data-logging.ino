// Core Libraries
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <Arduino.h>
#include <WiFi.h>
#include "time.h"


// Libraries for SD Card Logging
#include <SPI.h>
#include <SD.h>


// --- USER CONFIGURATION ---
const char* ssid = "your_SSID";
const char* password = "your_PASSWORD";


const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600; // WIB is UTC+7
const int   daylightOffset_sec = 0;


// --- HARDWARE & DISPLAY CONFIGURATION ---
#define SCREEN_WIDTH      480
#define SCREEN_HEIGHT     320
#define LVGL_TICK_PERIOD  5
#define SENSOR_PIN        21
#define SENSOR_INTERVAL   1000
#define SD_CS_PIN         5


// --- OBJECTS & VARIABLES ---
TFT_eSPI tft = TFT_eSPI();
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * 8];


lv_obj_t *label_lpm, *label_cmm, *label_cfm, *label_liters;
lv_obj_t *chart;
lv_chart_series_t *flow_series;
lv_obj_t *status_label;


float calibrationFactor = 7.5;
volatile byte pulseCount = 0;
byte pulsesLastSecond = 0;
float flowRateLPM = 0.0;
float flowCMM = 0.0;
float flowCFM = 0.0;
unsigned long totalMilliLitres = 0;
unsigned long previousMillis = 0;


// MODIFICATION: Changed from a const char* to a char array to hold the dynamic filename.
char logFileName[35];


// --- CORE FUNCTIONS ---
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}


void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}


// --- DATA LOGGING FUNCTION ---
// No changes needed here, it now uses the global logFileName set in setup()
void logDataToSD() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time for logging");
    lv_label_set_text(status_label, "Time Sync Fail");
    return;
  }
 
  char timestamp[20];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);


  char dataString[100];
  sprintf(dataString, "%s,%.2f,%.4f,%.3f,%lu", timestamp, flowRateLPM, flowCMM, flowCFM, totalMilliLitres);


  File dataFile = SD.open(logFileName, FILE_APPEND);
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
  } else {
    Serial.println("Error opening log file!");
    lv_label_set_text(status_label, "SD Write Fail");
  }
}


// --- UI SETUP & UPDATE ---
// (No changes in this section)
void setup_ui() {
  lv_obj_t *flow_panel = lv_obj_create(lv_scr_act());
  lv_obj_set_size(flow_panel, 225, 120);
  lv_obj_align(flow_panel, LV_ALIGN_TOP_LEFT, 10, 10);


  lv_obj_t *title_flow = lv_label_create(flow_panel);
  lv_label_set_text(title_flow, "Flow Rate");
  lv_obj_align(title_flow, LV_ALIGN_TOP_LEFT, 5, -5);


  label_lpm = lv_label_create(flow_panel);
  lv_label_set_text(label_lpm, "0.0 L/min");
  lv_obj_align(label_lpm, LV_ALIGN_TOP_LEFT, 10, 25);


  label_cmm = lv_label_create(flow_panel);
  lv_label_set_text(label_cmm, "0.0000 m³/min");
  lv_obj_align(label_cmm, LV_ALIGN_TOP_LEFT, 10, 50);


  label_cfm = lv_label_create(flow_panel);
  lv_label_set_text(label_cfm, "0.000 ft³/min");
  lv_obj_align(label_cfm, LV_ALIGN_TOP_LEFT, 10, 75);


  lv_obj_t *volume_panel = lv_obj_create(lv_scr_act());
  lv_obj_set_size(volume_panel, 225, 120);
  lv_obj_align(volume_panel, LV_ALIGN_TOP_RIGHT, -10, 10);


  lv_obj_t *title_volume = lv_label_create(volume_panel);
  lv_label_set_text(title_volume, "Total Volume");
  lv_obj_align(title_volume, LV_ALIGN_TOP_LEFT, 5, -5);


  label_liters = lv_label_create(volume_panel);
  lv_label_set_text(label_liters, "0 mL");
  lv_obj_align(label_liters, LV_ALIGN_CENTER, 0, 10);
  lv_obj_set_style_text_font(label_liters, &lv_font_montserrat_14, 0);


  status_label = lv_label_create(lv_scr_act());
  lv_label_set_text(status_label, "Initializing...");
  lv_obj_set_style_text_color(status_label, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_align(status_label, LV_ALIGN_BOTTOM_LEFT, 10, -10);


  chart = lv_chart_create(lv_scr_act());
  lv_obj_set_size(chart, 460, 160);
  lv_obj_align(chart, LV_ALIGN_BOTTOM_MID, 0, -35);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 50);
  lv_chart_set_point_count(chart, 60);
  flow_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_CYAN), LV_CHART_AXIS_PRIMARY_Y);
}


void update_display_data() {
  char buffer[32];


  snprintf(buffer, sizeof(buffer), "%.1f L/min", flowRateLPM);
  lv_label_set_text(label_lpm, buffer);
 
  snprintf(buffer, sizeof(buffer), "%.4f m³/min", flowCMM);
  lv_label_set_text(label_cmm, buffer);


  snprintf(buffer, sizeof(buffer), "%.3f ft³/min", flowCFM);
  lv_label_set_text(label_cfm, buffer);


  snprintf(buffer, sizeof(buffer), "%lu mL", totalMilliLitres);
  lv_label_set_text(label_liters, buffer);


  lv_chart_set_next_value(chart, flow_series, (int16_t)flowRateLPM);
  lv_chart_refresh(chart);
}


// --- MAIN SETUP & LOOP ---
void setup() {
  Serial.begin(115200);


  tft.begin();
  tft.setRotation(1);
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * 8);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_WIDTH;
  disp_drv.ver_res = SCREEN_HEIGHT;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);
  setup_ui();


  lv_label_set_text(status_label, "Connecting to WiFi...");
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println(" CONNECTED!");
  lv_label_set_text(status_label, "WiFi Connected");


  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);


  // --- MODIFICATION STARTS HERE ---


  // Create a new, unique filename for each session
  lv_label_set_text(status_label, "Getting time for filename...");
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time for filename");
    lv_label_set_text(status_label, "Time Sync Fail");
    // Use a default filename if time sync fails
    strcpy(logFileName, "/log_timeless.csv");
  } else {
    // Format the time into a string like "/log_2025-07-09_12-54-00.csv"
    strftime(logFileName, sizeof(logFileName), "/log_%Y-%m-%d_%H-%M-%S.csv", &timeinfo);
  }


  Serial.print("Logging to new file: ");
  Serial.println(logFileName);
 
  // Initialize SD Card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Card Mount Failed");
    lv_label_set_text(status_label, "SD Card Error!");
    while (1) delay(10); // Halt on error
  }
 
  // Create the new file and write the header row
  File dataFile = SD.open(logFileName, FILE_WRITE);
  if (dataFile) {
    dataFile.println("DateTime,LitersPerMinute,CubicMetersPerMinute,CubicFeetPerMinute,TotalVolume_mL");
    dataFile.close();
    lv_label_set_text(status_label, "New Log Created");
  } else {
    Serial.println("Error creating new log file!");
    lv_label_set_text(status_label, "SD Write Fail");
  }
  // --- MODIFICATION ENDS HERE ---


  pinMode(SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), pulseCounter, FALLING);
  previousMillis = millis();
}


void loop() {
  lv_tick_inc(LVGL_TICK_PERIOD);
  lv_timer_handler();


  if (millis() - previousMillis >= SENSOR_INTERVAL) {
    noInterrupts();
    pulsesLastSecond = pulseCount;
    pulseCount = 0;
    interrupts();


    float frequency = (float)pulsesLastSecond / (SENSOR_INTERVAL / 1000.0);
    flowRateLPM = frequency / calibrationFactor;
    flowCMM = flowRateLPM * 0.001;
    flowCFM = flowRateLPM * 0.0353147;
    totalMilliLitres += (flowRateLPM / 60) * SENSOR_INTERVAL;


    update_display_data();
    logDataToSD();


    Serial.printf("Flow: %.1f L/min, CMM: %.4f, CFM: %.3f, Total: %lu mL\n",
                  flowRateLPM, flowCMM, flowCFM, totalMilliLitres);


    previousMillis = millis();
  }
  delay(5);
}
