// Required Libraries
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_PCF8574.h>
#include <vector>


// ----------------------------------------------------------------
// --- HARDWARE & DISPLAY CONFIGURATION
// ----------------------------------------------------------------


// Display settings from the target UI
#define SCREEN_WIDTH      480 // Landscape
#define SCREEN_HEIGHT     320 // Landscape
#define LVGL_TICK_PERIOD  5   // LVGL's internal time base in milliseconds


// File Explorer Hardware Settings
#define SD_CS 5 // SD Card Chip Select pin


// Using 7 buttons via PCF8574
const uint8_t buttonPins[7] = {0, 1, 2, 3, 4, 5, 6}; // UP, DOWN, SELECT, BACK, LEFT, RIGHT, TOGGLE
const int numButtons = 7;




// ----------------------------------------------------------------
// --- OBJECTS & DRIVERS
// ----------------------------------------------------------------


TFT_eSPI tft = TFT_eSPI();
Adafruit_PCF8574 pcf;


// LVGL drawing buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * 20];




// ----------------------------------------------------------------
// --- FILE EXPLORER VARIABLES
// ----------------------------------------------------------------


String currentPath = "/";
static String currentCSVPath = "";
static int csv_col_offset = 0;
static const int visible_cols = 4;
static int csv_max_col = 0;
static int csvViewMode = 0; // 0=Chart, 1=Table, 2=Histogram


int selected_index = 0;
int first_visible_index = 0;


// Charting Variables
static std::vector<std::vector<float>> all_csv_data; // Holds all numeric data from the CSV
static std::vector<String> csv_headers;               // Holds column headers
static int chart_data_offset = 0;                     // Starting point for the chart viewport
static const int CHART_VISIBLE_POINTS = 50;           // Number of points to show on the chart at once




// ----------------------------------------------------------------
// --- LVGL & UI VARIABLES
// ----------------------------------------------------------------


// UI Objects for the static top panels
lv_obj_t *label_flow;
lv_obj_t *label_cmm;
lv_obj_t *label_cfm;
lv_obj_t *label_volume;
lv_obj_t *label_liters;


// Container for the file explorer content
lv_obj_t *file_content_container;
lv_obj_t *file_list = nullptr;




// ----------------------------------------------------------------
// --- LVGL DISPLAY & INPUT DRIVER FUNCTIONS
// ----------------------------------------------------------------


void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void touchpad_read(lv_indev_drv_t *indev, lv_indev_data_t *data);




// ----------------------------------------------------------------
// --- UI CREATION FUNCTION PROTOTYPES
// ----------------------------------------------------------------


void setup_ui();
void ui_create_file_list(const char* path);
void ui_create_file_viewer(const char* filePath);
void ui_create_csv_table(const char* filePath);
void ui_create_csv_chart(const char* filePath);
void ui_create_csv_histogram(const char* filePath);
void event_handler_file_list(lv_event_t * e);
void event_handler_back_button(lv_event_t * e);
void load_csv_data(const char* filePath);




// ----------------------------------------------------------------
// --- ARDUINO SETUP & LOOP
// ----------------------------------------------------------------


void setup() {
    Serial.begin(115200);


    // Initialize PCF8574 for buttons
    if (!pcf.begin(0x20, &Wire)) {
        Serial.println("Couldn't find PCF8574");
        while (1);
    }
    for (int i = 0; i < numButtons; i++) {
        pcf.pinMode(buttonPins[i], INPUT_PULLUP);
    }


    // Initialize TFT and LVGL
    tft.begin();
    tft.setRotation(1); // Set to landscape mode


    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * 20);


    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);


    // Initialize LVGL Input Driver for buttons
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = touchpad_read;
    lv_indev_t * my_indev = lv_indev_drv_register(&indev_drv);
    lv_group_t * g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(my_indev, g);


    // Initialize SD Card
    if (!SD.begin(SD_CS)) {
        lv_obj_t* label = lv_label_create(lv_scr_act());
        lv_label_set_text(label, "SD Card Mount Failed!\nCheck wiring.");
        lv_obj_center(label);
        while (1);
    }


    // Create the main user interface
    setup_ui();


    // Initial population of the file explorer
    ui_create_file_list(currentPath.c_str());
}


void loop() {
    lv_tick_inc(LVGL_TICK_PERIOD);
    lv_timer_handler();
    delay(LVGL_TICK_PERIOD);
}




// ----------------------------------------------------------------
// --- USER INTERFACE SETUP
// ----------------------------------------------------------------


void setup_ui() {
    // --- Create Panels for Static Data (as per original UI) ---
    lv_obj_t *flow_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(flow_panel, 225, 100);
    lv_obj_align(flow_panel, LV_ALIGN_TOP_LEFT, 10, 10);


    lv_obj_t *volume_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(volume_panel, 225, 100);
    lv_obj_align(volume_panel, LV_ALIGN_TOP_RIGHT, -10, 10);


    // --- Create Content for the Flow Panel ---
    lv_obj_t *title_flow = lv_label_create(flow_panel);
    lv_label_set_text(title_flow, "Flow Rate");
    lv_obj_set_style_text_color(title_flow, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(title_flow, LV_ALIGN_TOP_LEFT, 5, 5);


    label_flow = lv_label_create(flow_panel);
    lv_label_set_text(label_flow, "- L/min"); // Empty data
    lv_obj_set_style_text_color(label_flow, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(label_flow, LV_ALIGN_TOP_LEFT, 10, 25);


    label_cmm = lv_label_create(flow_panel);
    lv_label_set_text(label_cmm, "- CMM"); // Empty data
    lv_obj_set_style_text_color(label_cmm, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_align_to(label_cmm, label_flow, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);


    label_cfm = lv_label_create(flow_panel);
    lv_label_set_text(label_cfm, "- CFM"); // Empty data
    lv_obj_set_style_text_color(label_cfm, lv_palette_main(LV_PALETTE_YELLOW), 0);
    lv_obj_align_to(label_cfm, label_cmm, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);


    // --- Create Content for the Volume Panel ---
    lv_obj_t *title_volume = lv_label_create(volume_panel);
    lv_label_set_text(title_volume, "Total Volume");
    lv_obj_set_style_text_color(title_volume, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(title_volume, LV_ALIGN_TOP_LEFT, 5, 5);


    label_volume = lv_label_create(volume_panel);
    lv_label_set_text(label_volume, "- mL"); // Empty data
    lv_obj_set_style_text_color(label_volume, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_align(label_volume, LV_ALIGN_CENTER, -20, 10);


    label_liters = lv_label_create(volume_panel);
    lv_label_set_text(label_liters, "(- L)"); // Empty data
    lv_obj_align_to(label_liters, label_volume, LV_ALIGN_OUT_RIGHT_BOTTOM, 5, 0);


    // --- Create Container for the File Explorer ---
    file_content_container = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(file_content_container); // Make it a plain, invisible container
    lv_obj_set_size(file_content_container, 460, 205);
    lv_obj_align(file_content_container, LV_ALIGN_BOTTOM_MID, 0, -5);
}




// ----------------------------------------------------------------
// --- FILE EXPLORER UI FUNCTIONS
// ----------------------------------------------------------------


String formatSize(size_t bytes) {
    if (bytes < 1024) return String(bytes) + " B";
    if (bytes < 1024 * 1024) return String(bytes / 1024.0, 1) + " KB";
    return String(bytes / 1024.0 / 1024.0, 1) + " MB";
}


void ui_create_file_list(const char* path) {
    lv_obj_clean(file_content_container); // Clean only the container
    currentPath = path;
    currentCSVPath = "";
    selected_index = 0;
    first_visible_index = 0;


    file_list = lv_list_create(file_content_container); // Create list inside the container
    lv_obj_set_size(file_list, lv_pct(100), lv_pct(100)); // Fill the container
    lv_obj_center(file_list);
    lv_group_add_obj(lv_group_get_default(), file_list);
    lv_obj_clear_flag(file_list, LV_OBJ_FLAG_SCROLL_ON_FOCUS);


    File root = SD.open(path);
    if (!root) {
        lv_list_add_text(file_list, "Failed to open directory");
        return;
    }


    if (currentPath != "/") {
        lv_obj_t * btn = lv_list_add_btn(file_list, LV_SYMBOL_DIRECTORY, " .. (Back)");
        lv_obj_add_event_cb(btn, event_handler_back_button, LV_EVENT_CLICKED, NULL);
    }


    File file = root.openNextFile();
    while (file) {
        String fileName = file.name();
        String displayText;
        const char* icon;


        if (file.isDirectory()) {
            icon = LV_SYMBOL_DIRECTORY;
            displayText = " " + fileName.substring(fileName.lastIndexOf('/') + 1);
        } else {
            icon = LV_SYMBOL_FILE;
            displayText = " " + fileName.substring(fileName.lastIndexOf('/') + 1) + " (" + formatSize(file.size()) + ")";
        }


        lv_obj_t * btn = lv_list_add_btn(file_list, icon, displayText.c_str());
        lv_obj_add_event_cb(btn, event_handler_file_list, LV_EVENT_CLICKED, strdup(file.path()));
        file.close();
        file = root.openNextFile();
    }
   
    // Auto-focus the first item in the list
    if (file_list) {
        uint32_t child_count = lv_obj_get_child_cnt(file_list);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t* obj = lv_obj_get_child(file_list, i);
            if (lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE)) {
                selected_index = i;
                lv_group_focus_obj(obj);
                break;
            }
        }
    }
}


void ui_create_file_viewer(const char* filePath) {
    if (String(filePath).endsWith(".csv")) {
        load_csv_data(filePath);
        if (all_csv_data.empty()) {
             lv_obj_clean(file_content_container);
             lv_obj_t* label = lv_label_create(file_content_container);
             lv_label_set_text(label, "CSV has no data rows.");
             lv_obj_center(label);
             return;
        }
       
        // Reset view settings
        csv_col_offset = 0;
        csv_max_col = 0;
        chart_data_offset = 0;
        csvViewMode = 0;
        ui_create_csv_chart(filePath); // Start with chart view
        return;
    }


    // Generic file viewer
    lv_obj_clean(file_content_container);
    File selectedFile = SD.open(filePath);
    if (!selectedFile) {
        ui_create_file_list(currentPath.c_str());
        return;
    }


    lv_obj_t * ta = lv_textarea_create(file_content_container);
    lv_obj_set_size(ta, lv_pct(100), lv_pct(100));
    lv_obj_center(ta);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_14, 0);


    String content = "File: " + String(selectedFile.name()) + "\n\n";
    while (selectedFile.available()) {
        content += (char)selectedFile.read();
    }
    lv_textarea_set_text(ta, content.c_str());
    selectedFile.close();
}


void ui_create_csv_table(const char* filePath) {
    currentCSVPath = filePath;
    csvViewMode = 1;
    lv_obj_clean(file_content_container);


    File file = SD.open(filePath);
    if (!file) {
        ui_create_file_list(currentPath.c_str());
        return;
    }


    lv_obj_t* table = lv_table_create(file_content_container);
    lv_obj_set_size(table, lv_pct(100), lv_pct(100));
    lv_obj_center(table);
    lv_table_set_col_cnt(table, visible_cols);
    lv_table_set_col_width(table, 0, 110);
    lv_table_set_col_width(table, 1, 110);
    lv_table_set_col_width(table, 2, 110);
    lv_table_set_col_width(table, 3, 110);


    char line[256];
    int row = 0;
    csv_max_col = 0;
    while (file.available() && row < 40) {
        size_t len = file.readBytesUntil('\n', line, sizeof(line) - 1);
        line[len] = '\0';
        if(len == 0) continue;


        String strLine = String(line);
        int col = 0;
        int lastComma = -1;
        int visible_col_idx = 0;


        for (int i = 0; i <= strLine.length(); i++) {
            if (strLine[i] == ',' || i == strLine.length()) {
                if (col >= csv_col_offset && visible_col_idx < visible_cols) {
                    String cell = strLine.substring(lastComma + 1, i);
                    cell.trim();
                    lv_table_set_cell_value(table, row, visible_col_idx, cell.c_str());
                    visible_col_idx++;
                }
                lastComma = i;
                col++;
            }
        }
        if (row == 0) {
           csv_max_col = col;
        }
        row++;
    }
    file.close();
}


void load_csv_data(const char* filePath) {
    all_csv_data.clear();
    csv_headers.clear();
   
    File file = SD.open(filePath);
    if (!file) return;


    // Read header
    String header_line = file.readStringUntil('\n');
    header_line.trim();
    int lastComma = -1;
    for (int i = 0; i <= header_line.length(); i++) {
        if (header_line[i] == ',' || i == header_line.length()) {
            csv_headers.push_back(header_line.substring(lastComma + 1, i));
            lastComma = i;
        }
    }


    if (csv_headers.size() < 2) {
        file.close();
        return;
    }


    // Read all data rows
    while (file.available()) {
        String data_line = file.readStringUntil('\n');
        data_line.trim();
        if (data_line.length() == 0) continue;


        std::vector<float> row_data;
        lastComma = -1;
        for (int i = 0; i <= data_line.length(); i++) {
            if (data_line[i] == ',' || i == data_line.length()) {
                row_data.push_back(data_line.substring(lastComma + 1, i).toFloat());
                lastComma = i;
            }
        }
        if (row_data.size() == csv_headers.size()) {
            all_csv_data.push_back(row_data);
        }
    }
    file.close();
}


void ui_create_csv_chart(const char* filePath) {
    currentCSVPath = filePath;
    csvViewMode = 0;
    lv_obj_clean(file_content_container);


    if (csv_headers.size() < 2) {
        lv_obj_t* label = lv_label_create(file_content_container);
        lv_label_set_text(label, "CSV requires a header and\nat least two data columns.");
        lv_obj_center(label);
        return;
    }


    // Find Min/Max for the VISIBLE data segment
    float primary_min = 1.0e+38, primary_max = -1.0e+38;
    float secondary_min = 1.0e+38, secondary_max = -1.0e+38;
   
    int end_point = std::min((int)(chart_data_offset + CHART_VISIBLE_POINTS), (int)all_csv_data.size());


    for (int i = chart_data_offset; i < end_point; ++i) {
        for (int j = 1; j < csv_headers.size(); ++j) {
            float val = all_csv_data[i][j];
            if (csv_headers[j].indexOf("Volume") != -1) {
                if (val < primary_min) primary_min = val;
                if (val > primary_max) primary_max = val;
            } else {
                if (val < secondary_min) secondary_min = val;
                if (val > secondary_max) secondary_max = val;
            }
        }
    }


    if (end_point == chart_data_offset) { return; }
    if (primary_max < primary_min) { primary_min = 0; primary_max = 1; }
    if (secondary_max < secondary_min) { secondary_min = 0; secondary_max = 1; }




    // Create Chart and Series
    lv_obj_t* chart = lv_chart_create(file_content_container);
    lv_obj_set_size(chart, 380, 140); // Slightly reduce height for labels
    lv_obj_align(chart, LV_ALIGN_TOP_MID, 0, 0);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, end_point - chart_data_offset);
   
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, (lv_coord_t)(primary_min - (primary_max * 0.05)), (lv_coord_t)(primary_max * 1.05));
    lv_chart_set_range(chart, LV_CHART_AXIS_SECONDARY_Y, (lv_coord_t)(secondary_min - (secondary_max * 0.05)), (lv_coord_t)(secondary_max * 1.05));
   
    // Configure Y-axis ticks
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 5, 2, true, 50);
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_SECONDARY_Y, 10, 5, 5, 2, true, 50);


    // --- COMPATIBILITY FIX: Manually create X-axis labels for older LVGL ---
    // Remove the incompatible lv_chart_set_x_tick_texts function and instead use simple ticks
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 10, 5, 6, 2, false, 25);
   
    lv_color_t series_colors[] = { lv_palette_main(LV_PALETTE_CYAN), lv_palette_main(LV_PALETTE_RED), lv_palette_main(LV_PALETTE_GREEN), lv_palette_main(LV_PALETTE_ORANGE), lv_palette_main(LV_PALETTE_PURPLE) };
    int num_colors = sizeof(series_colors) / sizeof(series_colors[0]);
    int num_series = csv_headers.size() - 1;


    std::vector<lv_chart_series_t*> chart_series;
    for (int i = 0; i < num_series; ++i) {
        lv_chart_axis_t axis = (csv_headers[i + 1].indexOf("Volume") != -1) ? LV_CHART_AXIS_PRIMARY_Y : LV_CHART_AXIS_SECONDARY_Y;
        lv_chart_series_t* ser = lv_chart_add_series(chart, series_colors[i % num_colors], axis);
        chart_series.push_back(ser);
    }
   
    // Populate Series with VISIBLE Data
    for (int i = chart_data_offset; i < end_point; ++i) {
        for (int j = 1; j < csv_headers.size(); ++j) {
            lv_chart_set_next_value(chart, chart_series[j - 1], (lv_coord_t)all_csv_data[i][j]);
        }
    }
   
    // Create titles and legend with proper layout
    lv_obj_t* x_axis_title = lv_label_create(file_content_container);
    lv_label_set_text(x_axis_title, csv_headers[0].c_str());
    lv_obj_align_to(x_axis_title, chart, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
   
    lv_obj_t* legend_cont = lv_obj_create(file_content_container);
    lv_obj_remove_style_all(legend_cont);
    lv_obj_set_size(legend_cont, 400, LV_SIZE_CONTENT);
    lv_obj_align_to(legend_cont, x_axis_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_obj_set_flex_flow(legend_cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(legend_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(legend_cont, 15, 0);


    for (int i = 0; i < num_series; ++i) {
        lv_obj_t* color_box = lv_obj_create(legend_cont);
        lv_obj_set_size(color_box, 15, 15);
        lv_obj_set_style_bg_color(color_box, series_colors[i % num_colors], 0);
        lv_obj_clear_flag(color_box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_border_width(color_box, 0, 0);


        lv_obj_t* legend_label = lv_label_create(legend_cont);
        lv_label_set_text(legend_label, csv_headers[i + 1].c_str());
    }
   
    lv_obj_t* progress_label = lv_label_create(file_content_container);
    char buf[64];
    snprintf(buf, sizeof(buf), "Showing points %d - %d of %d", chart_data_offset + 1, end_point, all_csv_data.size());
    lv_label_set_text(progress_label, buf);
    lv_obj_align_to(progress_label, legend_cont, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
}


void ui_create_csv_histogram(const char* filePath) {
    currentCSVPath = filePath;
    csvViewMode = 2;
    lv_obj_clean(file_content_container);


    File file = SD.open(filePath);
    if (!file) {
        ui_create_file_list(currentPath.c_str());
        return;
    }


    float min_val = 1.0e+38, max_val = -1.0e+38;
    int data_points_count = 0;
    file.readStringUntil('\n'); // Skip header


    while (file.available()) {
        file.readStringUntil(',');
        float flow_val = file.readStringUntil(',').toFloat();
        file.readStringUntil('\n');


        if (flow_val > 0) {
            if (flow_val < min_val) min_val = flow_val;
            if (flow_val > max_val) max_val = flow_val;
            data_points_count++;
        }
    }


    if (data_points_count == 0) {
        file.close();
        lv_obj_t* label = lv_label_create(file_content_container);
        lv_label_set_text(label, "No positive data for histogram.");
        lv_obj_center(label);
        return;
    }


    const int NUM_BINS = 10;
    int bins[NUM_BINS] = {0};
    float bin_width = (max_val - min_val) / NUM_BINS;


    file.seek(0);
    file.readStringUntil('\n');


    while (file.available()) {
        file.readStringUntil(',');
        float flow_val = file.readStringUntil(',').toFloat();
        file.readStringUntil('\n');
       
        if (flow_val > 0) {
            int bin_index = (bin_width > 0) ? (int)((flow_val - min_val) / bin_width) : 0;
            if (bin_index >= NUM_BINS) bin_index = NUM_BINS - 1;
            if (bin_index < 0) bin_index = 0;
            bins[bin_index]++;
        }
    }
    file.close();


    int max_freq = 0;
    for (int count : bins) {
        if (count > max_freq) max_freq = count;
    }
   
    lv_obj_t* chart = lv_chart_create(file_content_container);
    lv_obj_set_size(chart, lv_pct(100), lv_pct(100));
    lv_obj_center(chart);
    lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(chart, NUM_BINS);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, max_freq + (max_freq / 10));
    lv_obj_set_style_pad_column(chart, 5, 0);


    lv_chart_series_t* series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    for (int count : bins) {
        lv_chart_set_next_value(chart, series, count);
    }
}




// ----------------------------------------------------------------
// --- EVENT HANDLERS
// ----------------------------------------------------------------


void event_handler_file_list(lv_event_t * e) {
    char* filePath = (char*)lv_event_get_user_data(e);
    File f = SD.open(filePath);
    bool isDir = f.isDirectory();
    f.close();


    if (isDir) {
        ui_create_file_list(filePath);
    } else {
        ui_create_file_viewer(filePath);
    }
    free(filePath);
}


void event_handler_back_button(lv_event_t * e) {
    if (currentPath != "/") {
        int lastSlash = currentPath.lastIndexOf('/');
        String path = (lastSlash > 0) ? currentPath.substring(0, lastSlash) : "/";
        ui_create_file_list(path.c_str());
    }
}




// ----------------------------------------------------------------
// --- DRIVER CALLBACKS
// ----------------------------------------------------------------


void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);


    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();


    lv_disp_flush_ready(disp);
}


// ----------------------------------------------------------------
// --- TOUCHPAD DRIVER
// ----------------------------------------------------------------
void touchpad_read(lv_indev_drv_t *indev, lv_indev_data_t *data) {
    static int8_t last_key = -1;
    int8_t current_key = -1;


    // Find the currently pressed button
    for (int i = 0; i < numButtons; i++) {
        if (!pcf.digitalRead(buttonPins[i])) {
            current_key = i;
            break;
        }
    }


    // This block handles a NEW button press
    if (current_key != -1 && last_key == -1) {
        last_key = current_key; // Register the press
        data->state = LV_INDEV_STATE_PRESSED;


        // The switch statement now correctly runs ONCE per press
        switch(current_key) {
            case 0: // UP
                data->key = LV_KEY_UP;
                if (file_list && currentCSVPath.isEmpty() && selected_index > 0) {
                    int i = selected_index - 1;
                    while (i >= 0) {
                        lv_obj_t* btn = lv_obj_get_child(file_list, i);
                        if (lv_obj_has_flag(btn, LV_OBJ_FLAG_CLICKABLE)) {
                            selected_index = i;
                            lv_group_focus_obj(btn);
                            if (selected_index < first_visible_index) {
                                first_visible_index = selected_index;
                                lv_obj_scroll_to_view_recursive(btn, LV_ANIM_OFF);
                            }
                            break;
                        }
                        i--;
                    }
                }
                break;


            case 1: // DOWN
                data->key = LV_KEY_DOWN;
                if (file_list && currentCSVPath.isEmpty()) {
                    int count = lv_obj_get_child_cnt(file_list);
                    int i = selected_index + 1;
                    while (i < count) {
                        lv_obj_t* btn = lv_obj_get_child(file_list, i);
                        if (lv_obj_has_flag(btn, LV_OBJ_FLAG_CLICKABLE)) {
                            selected_index = i;
                            lv_group_focus_obj(btn);
                            if (selected_index >= first_visible_index + 7) {
                                first_visible_index++;
                                lv_obj_scroll_to_view_recursive(btn, LV_ANIM_OFF);
                            }
                            break;
                        }
                        i++;
                    }
                }
                break;


            case 2: // SELECT
                data->key = LV_KEY_ENTER;
                if (file_list && !currentCSVPath.length()) {
                    lv_obj_t* btn = lv_obj_get_child(file_list, selected_index);
                    if (btn) lv_event_send(btn, LV_EVENT_CLICKED, NULL);
                }
                break;


            case 3: // BACK
                data->key = LV_KEY_ESC;
                if (!currentCSVPath.isEmpty()) {
                    ui_create_file_list(currentPath.c_str());
                } else {
                    event_handler_back_button(NULL);
                }
                break;


            case 4: // LEFT
                data->key = LV_KEY_LEFT;
                if (!currentCSVPath.isEmpty()) {
                    if (csvViewMode == 1) { // Table View
                        if (csv_col_offset > 0) {
                            csv_col_offset -= visible_cols;
                            if (csv_col_offset < 0) csv_col_offset = 0;
                            ui_create_csv_table(currentCSVPath.c_str());
                        }
                    } else if (csvViewMode == 0) { // Chart View
                        if (chart_data_offset > 0) {
                            chart_data_offset -= 10; // Pan left
                            if (chart_data_offset < 0) chart_data_offset = 0;
                            ui_create_csv_chart(currentCSVPath.c_str());
                        }
                    }
                }
                break;


            case 5: // RIGHT
                data->key = LV_KEY_RIGHT;
                if (!currentCSVPath.isEmpty()) {
                    if (csvViewMode == 1) { // Table View
                        if ((csv_col_offset + visible_cols) < csv_max_col) {
                            csv_col_offset += visible_cols;
                            ui_create_csv_table(currentCSVPath.c_str());
                        }
                    } else if (csvViewMode == 0) { // Chart View
                        if ((chart_data_offset + CHART_VISIBLE_POINTS) < all_csv_data.size()) {
                            chart_data_offset += 10; // Pan right
                            if (chart_data_offset > (int)all_csv_data.size() - CHART_VISIBLE_POINTS) {
                                chart_data_offset = all_csv_data.size() - CHART_VISIBLE_POINTS;
                            }
                            ui_create_csv_chart(currentCSVPath.c_str());
                        }
                    }
                }
                break;


            case 6: // TOGGLE VIEW
                data->key = LV_KEY_NEXT;
                if (!currentCSVPath.isEmpty()) {
                    csvViewMode = (csvViewMode + 1) % 3; // Cycle 0, 1, 2
                    switch (csvViewMode) {
                        case 0: ui_create_csv_chart(currentCSVPath.c_str()); break;
                        case 1: ui_create_csv_table(currentCSVPath.c_str()); break;
                        case 2: ui_create_csv_histogram(currentCSVPath.c_str()); break;
                    }
                }
                break;
        }
    }
    // This block handles a button RELEASE
    else if (current_key == -1 && last_key != -1) {
        data->state = LV_INDEV_STATE_RELEASED;
        // The key released is the one we stored in last_key
        switch(last_key) {
            case 0: data->key = LV_KEY_UP; break;
            case 1: data->key = LV_KEY_DOWN; break;
            case 2: data->key = LV_KEY_ENTER; break;
            case 3: data->key = LV_KEY_ESC; break;
            case 4: data->key = LV_KEY_LEFT; break;
            case 5: data->key = LV_KEY_RIGHT; break;
            case 6: data->key = LV_KEY_NEXT; break;
        }
        last_key = -1; // Reset for the next press
    }
}
