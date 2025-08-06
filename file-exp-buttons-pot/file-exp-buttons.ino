#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <Adafruit_PCF8574.h>
#include <lvgl.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 480
#define SD_CS 5

// Using 7 buttons
const uint8_t buttonPins[7] = {0, 1, 2, 3, 4, 5, 6};
const int numButtons = 7;

TFT_eSPI tft = TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT);
Adafruit_PCF8574 pcf;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_WIDTH * 20];

String currentPath = "/";
static String currentCSVPath = "";
static int csv_col_offset = 0;
static const int visible_cols = 4;
static int csv_max_col = 0;

// 0=Chart, 1=Table, 2=Histogram
static int csvViewMode = 0;

lv_obj_t* file_list = nullptr;
int selected_index = 0;
int first_visible_index = 0;

// Function Prototypes
void display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void touchpad_read(lv_indev_drv_t *indev, lv_indev_data_t *data);
void ui_create_file_list(const char* path);
void ui_create_file_viewer(const char* filePath);
void ui_create_csv_table(const char* filePath);
void ui_create_csv_chart(const char* filePath);
void ui_create_csv_histogram(const char* filePath);
void ui_create_status_bar(const char* filename);
void ui_show_loading_screen();
void event_handler_file_list(lv_event_t * e);
void event_handler_back_button(lv_event_t * e);

void setup() {
    Serial.begin(115200);
    if (!pcf.begin(0x20, &Wire)) while (1);
    for (int i = 0; i < numButtons; i++) pcf.pinMode(buttonPins[i], INPUT_PULLUP);

    lv_init();
    tft.begin();
    tft.setRotation(0);

    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, SCREEN_WIDTH * 20);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = display_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = touchpad_read;
    lv_indev_t * my_indev = lv_indev_drv_register(&indev_drv);
    lv_group_t * g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(my_indev, g);

    if (!SD.begin(SD_CS)) {
        lv_obj_t* label = lv_label_create(lv_scr_act());
        lv_label_set_text(label, "SD Card Mount Failed!\nCheck wiring.");
        lv_obj_center(label);
        while (1); // Halt on critical error
    }
    ui_create_file_list(currentPath.c_str());
}

void loop() {
    lv_tick_inc(5);
    lv_timer_handler();
    delay(5);
}


String formatSize(size_t bytes) {
    if (bytes < 1024) return String(bytes) + " B";
    if (bytes < 1024 * 1024) return String(bytes / 1024.0, 1) + " KB";
    return String(bytes / 1024.0 / 1024.0, 1) + " MB";
}

void ui_create_status_bar(const char* filename) {
    lv_obj_t* status_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(status_bar, SCREEN_WIDTH, 25);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 2, 0);

    lv_obj_t* file_label = lv_label_create(status_bar);
    String name_str = String(filename);
    String short_name = name_str.substring(name_str.lastIndexOf('/') + 1);
    lv_label_set_text(file_label, short_name.c_str());
    lv_label_set_long_mode(file_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(file_label, SCREEN_WIDTH - 20);
    lv_obj_set_style_text_color(file_label, lv_color_white(), 0);
    lv_obj_center(file_label);
}

void ui_show_loading_screen() {
    lv_obj_t* bg = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(bg);
    lv_obj_set_size(bg, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_opa(bg, LV_OPA_70, 0);
    lv_obj_set_style_bg_color(bg, lv_color_black(), 0);

    lv_obj_t* spinner = lv_spinner_create(bg, 1000, 60);
    lv_obj_set_size(spinner, 80, 80);
    lv_obj_center(spinner);
}

void ui_create_csv_table(const char* filePath) {
    currentCSVPath = filePath;
    csvViewMode = 1;
    lv_obj_clean(lv_scr_act());
    ui_create_status_bar(filePath);

    File file = SD.open(filePath);
    if (!file) {
        ui_create_file_list(currentPath.c_str());
        return;
    }

    lv_obj_t* table = lv_table_create(lv_scr_act());
    lv_obj_set_size(table, tft.width(), tft.height() - 25);
    lv_obj_align(table, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_table_set_col_cnt(table, visible_cols);

    int col_width = 160;
    for (int i = 0; i < visible_cols; i++) {
        lv_table_set_col_width(table, i, col_width);
    }

    char line[256];
    int row = 0;
    while (file.available() && row < 40) {
        size_t len = file.readBytesUntil('\n', line, sizeof(line) - 1);
        line[len] = '\0';
        if(len == 0) continue;

        String strLine = String(line);
        int col = 0;
        int lastComma = -1;
        int visible_col_idx = 0;

        for (int i = 0; i <= strLine.length(); i++) {
            if (strLine[i] == ',' || strLine[i] == '\0') {
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
        if (col > csv_max_col) {
            csv_max_col = col;
        }
        row++;
    }
    file.close();
}

void ui_create_file_list(const char* path) {
    lv_obj_clean(lv_scr_act());
    currentPath = path;
    currentCSVPath = "";
    selected_index = 0;
    first_visible_index = 0;

    lv_obj_t * title_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(title_bar, tft.width(), 30);
    lv_obj_align(title_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_palette_main(LV_PALETTE_BLUE_GREY), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);

    lv_obj_t* title_bar_label = lv_label_create(title_bar);
    lv_label_set_text(title_bar_label, "SD Card Explorer");
    lv_obj_set_style_text_color(title_bar_label, lv_color_white(), 0);
    lv_obj_center(title_bar_label)

    file_list = lv_list_create(lv_scr_act());
    lv_obj_set_size(file_list, tft.width(), tft.height() - 30);
    lv_obj_align(file_list, LV_ALIGN_BOTTOM_MID, 0, 0);
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
        lv_group_add_obj(lv_group_get_default(), btn);
    }

    File file = root.openNextFile();
    while (file) {
        String fileName = file.name();
        String displayText;
        const char* icon;

        if (file.isDirectory()) {
            icon = LV_SYMBOL_DIRECTORY;
            displayText = " " + fileName;
        } else {
            icon = LV_SYMBOL_FILE;
            displayText = " " + fileName + " (" + formatSize(file.size()) + ")";
        }

        lv_obj_t * btn = lv_list_add_btn(file_list, icon, displayText.c_str());
        lv_obj_add_event_cb(btn, event_handler_file_list, LV_EVENT_CLICKED, strdup(file.path()));
        lv_group_add_obj(lv_group_get_default(), btn);

        file.close();
        file = root.openNextFile();
    }

    if (file_list) {
        uint16_t child_count = lv_obj_get_child_cnt(file_list);
        for (uint16_t i = 0; i < child_count; i++) {
            lv_obj_t* obj = lv_obj_get_child(file_list, i);
            if (lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE)) {
                selected_index = i;
                lv_group_focus_obj(obj);
                break;
            }
        }
    }
}


void ui_create_csv_histogram(const char* filePath) {
    currentCSVPath = filePath;
    csvViewMode = 2;
    lv_obj_clean(lv_scr_act());
    ui_create_status_bar(filePath);

    static lv_style_t style_title;
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_14);

    File file = SD.open(filePath);
    if (!file) {
        ui_create_file_list(currentPath.c_str());
        return;
    }

    float min_val = 1.0e+38, max_val = -1.0e+38;
    int data_points_count = 0;
   
    file.readStringUntil('\n');
    while(file.available()) {
        String line = file.readStringUntil('\n');
        if (line.length() > 0) {
            int first_comma = line.indexOf(',');
            String val_str = line.substring(first_comma + 1);
            float flow_val = val_str.toFloat();
            if (flow_val > 0) {
                if (flow_val < min_val) min_val = flow_val;
                if (flow_val > max_val) max_val = flow_val;
                data_points_count++;
            }
        }
    }

    if (data_points_count == 0) {
        file.close();
        lv_obj_t* label = lv_label_create(lv_scr_act());
        lv_label_set_text(label, "No valid data for histogram.");
        lv_obj_center(label);
        return;
    }

    const int NUM_BINS = 10;
    int bins[NUM_BINS] = {0};
    float bin_width = (max_val - min_val) / NUM_BINS;
   
    file.seek(0);
    file.readStringUntil('\n');

    while(file.available()) {
        String line = file.readStringUntil('\n');
        if (line.length() > 0) {
            int first_comma = line.indexOf(',');
            String val_str = line.substring(first_comma + 1);
            float flow_val = val_str.toFloat();
            if (flow_val > 0) {
                int bin_index = (bin_width > 0) ? (int)((flow_val - min_val) / bin_width) : 0;
                if (bin_index >= NUM_BINS) bin_index = NUM_BINS - 1;
                bins[bin_index]++;
            }
        }
    }
    file.close();

    int max_freq = 0;
    for (int count : bins) {
        if (count > max_freq) max_freq = count;
    }
   
    lv_obj_t* title_label = lv_label_create(lv_scr_act());
    lv_label_set_text(title_label, "Flow Rate Distribution");
    lv_obj_add_style(title_label, &style_title, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 35);

    lv_obj_t* chart = lv_chart_create(lv_scr_act());
    lv_obj_set_size(chart, 250, 250);
    lv_obj_align(chart, LV_ALIGN_CENTER, 20, 30);
    lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(chart, NUM_BINS);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, max_freq + (max_freq/10));
    lv_obj_set_style_pad_column(chart, 5, 0);

    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 5, 2, 6, 2, true, 40);
   
    lv_chart_series_t* series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    for (int count : bins) {
        lv_chart_set_next_value(chart, series, count);
    }
   
    lv_obj_t* y_label = lv_label_create(lv_scr_act());
    lv_label_set_text(y_label, "Frequency");
    lv_obj_set_style_transform_angle(y_label, 2700, 0);
    lv_obj_align_to(y_label, chart, LV_ALIGN_OUT_LEFT_MID, -15, 0);

    lv_obj_t* x_label = lv_label_create(lv_scr_act());
    lv_label_set_text(x_label, "Flow (L/min)");
    lv_obj_align_to(x_label, chart, LV_ALIGN_OUT_BOTTOM_MID, 0, 35);
}

void ui_create_csv_chart(const char* filePath) {
    currentCSVPath = filePath;
    csvViewMode = 0;
    lv_obj_clean(lv_scr_act());
    ui_create_status_bar(filePath);

    File file = SD.open(filePath);
    if (!file) {
        ui_create_file_list(currentPath.c_str());
        return;
    }

    String header_line;
    if (file.available()) {
        header_line = file.readStringUntil('\n');
    }
    header_line.trim();

    const int MAX_HEADERS = 16;
    String headers[MAX_HEADERS];
    int header_count = 0;
    if (header_line.length() > 0) {
        int last_comma = -1;
        for (int i = 0; i < header_line.length() && header_count < MAX_HEADERS; i++) {
            if (header_line.charAt(i) == ',') {
                headers[header_count] = header_line.substring(last_comma + 1, i);
                headers[header_count].trim();
                last_comma = i;
                header_count++;
            }
        }
        headers[header_count] = header_line.substring(last_comma + 1);
        headers[header_count].trim();
        header_count++;
    }

    int point_count = 0;
    float min_val = 1.0e+38;
    float max_val = -1.0e+38;
   
    char line[256];
    while (file.available()) {
        size_t len = file.readBytesUntil('\n', line, sizeof(line) - 1);
        line[len] = '\0';
        if (len == 0) continue;
        point_count++;
        String str = String(line);
        int col = 0, last = -1, vis = 0;
        for (int i = 0; i <= str.length(); i++) {
            if (str[i] == ',' || str[i] == '\0') {
                if (col >= (csv_col_offset + 1) && vis < visible_cols) {
                    String cell = str.substring(last + 1, i);
                    float val = cell.toFloat();
                    if (val < min_val) min_val = val;
                    if (val > max_val) max_val = val;
                    vis++;
                }
                last = i;
                col++;
            }
        }
        if (col > csv_max_col) csv_max_col = col;
    }


    if (point_count == 0) {
        file.close();
        lv_obj_t* label = lv_label_create(lv_scr_act());
        lv_label_set_text(label, "No data in CSV.");
        lv_obj_center(label);
        return;
    }


    lv_obj_t* chart = lv_chart_create(lv_scr_act());
    lv_obj_set_size(chart, SCREEN_WIDTH - 60, SCREEN_HEIGHT - 125);
    lv_obj_align(chart, LV_ALIGN_TOP_LEFT, 50, 40);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
   
    lv_chart_set_point_count(chart, point_count);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, (lv_coord_t)min_val, (lv_coord_t)max_val + 5);

    lv_chart_set_div_line_count(chart, 5, point_count > 10 ? 10 : point_count);
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 5, 2, true, 50);
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 10, 5, point_count > 10 ? 10 : point_count, 1, true, 30);

    const int MAX_SERIES = visible_cols;
    lv_chart_series_t* series[MAX_SERIES];
    for (int i = 0; i < MAX_SERIES; i++) {
        series[i] = lv_chart_add_series(chart, lv_palette_main((lv_palette_t)(i % _LV_PALETTE_LAST)), LV_CHART_AXIS_PRIMARY_Y);
    }

    file.seek(0);
    if (file.available()) file.readStringUntil('\n');

    while (file.available()) {
        size_t len = file.readBytesUntil('\n', line, sizeof(line) - 1);
        line[len] = '\0';
        if (len == 0) continue;

        String str = String(line);
        int col = 0, last = -1, vis = 0;
        for (int i = 0; i <= str.length(); i++) {
            if (str[i] == ',' || str[i] == '\0') {
                if (col >= (csv_col_offset + 1) && vis < visible_cols) {
                    String cell = str.substring(last + 1, i);
                    lv_chart_set_next_value(chart, series[vis], cell.toFloat());
                    vis++;
                }
                last = i;
                col++;
            }
        }
    }
    file.close();
   
    lv_obj_t* legend_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(legend_cont, SCREEN_WIDTH, 60);
    lv_obj_align_to(legend_cont, chart, LV_ALIGN_OUT_BOTTOM_LEFT, -50, 15);
    lv_obj_set_flex_flow(legend_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(legend_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i=0; i < visible_cols; i++) {
        int header_idx = i + csv_col_offset + 1;
        if(header_idx < header_count) {
            lv_color_t series_color = lv_palette_main((lv_palette_t)(i % _LV_PALETTE_LAST));
           
            lv_obj_t* color_box = lv_obj_create(legend_cont);
            lv_obj_set_size(color_box, 15, 15);
            lv_obj_set_style_bg_color(color_box, series_color, 0);
            lv_obj_set_style_border_width(color_box, 0, 0);


            lv_obj_t* legend_label = lv_label_create(legend_cont);
            lv_label_set_text(legend_label, headers[header_idx].c_str());
        }
    }

    lv_obj_t* y_label = lv_label_create(lv_scr_act());
    lv_label_set_text(y_label, "L/min");
    lv_obj_align_to(y_label, chart, LV_ALIGN_OUT_LEFT_MID, -5, 0);


    lv_obj_t* x_label = lv_label_create(lv_scr_act());
    lv_label_set_text(x_label, header_count > 0 ? headers[0].c_str() : "Index");
    lv_obj_align_to(x_label, chart, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
}

void ui_create_file_viewer(const char* filePath) {
    if (String(filePath).endsWith(".csv")) {
        csv_col_offset = 0;
        csv_max_col = 0;
        csvViewMode = 0;
        ui_create_csv_chart(filePath);
        return;
    }

    lv_obj_clean(lv_scr_act());
    File selectedFile = SD.open(filePath);
    if (!selectedFile) {
        ui_create_file_list(currentPath.c_str());
        return;
    }

    lv_obj_t * ta = lv_textarea_create(lv_scr_act());
    lv_obj_set_size(ta, tft.width(), tft.height());
    lv_obj_align(ta, LV_ALIGN_CENTER, 0, 0);
    lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
    lv_obj_set_scrollbar_mode(ta, LV_SCROLLBAR_MODE_AUTO);

    String content = "File: " + String(selectedFile.name()) + "\n\n";
    while (selectedFile.available()) content += (char)selectedFile.read();
    lv_textarea_set_text(ta, content.c_str());
    selectedFile.close();
}

void event_handler_file_list(lv_event_t * e) {
    char* filePath = (char*)lv_event_get_user_data(e);
    File f = SD.open(filePath);
    if (f && f.isDirectory()) {
        ui_create_file_list(filePath);
    } else {
        ui_create_file_viewer(filePath);
    }
    f.close();
    free(filePath);
}

void event_handler_back_button(lv_event_t * e) {
    String path = currentPath;
    int lastSlash = path.lastIndexOf('/');
    path = (lastSlash > 0) ? path.substring(0, lastSlash) : "/";
    ui_create_file_list(path.c_str());
}

void display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

void touchpad_read(lv_indev_drv_t *indev, lv_indev_data_t *data) {
    static int8_t last_key = -1;
    int8_t current_key = -1;

    for (int i = 0; i < numButtons; i++) {
        if (!pcf.digitalRead(buttonPins[i])) {
            current_key = i;
            break;
        }
    }

    if (current_key != -1 && last_key == -1) {
        data->state = LV_INDEV_STATE_PRESSED;
        last_key = current_key;

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
                            if (selected_index >= first_visible_index + 10) {
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
                if (!currentCSVPath.isEmpty() && csvViewMode != 2 && csv_col_offset > 0) {
                    csv_col_offset -= visible_cols;
                    if (csv_col_offset < 0) csv_col_offset = 0;
                    if(csvViewMode == 0) ui_create_csv_chart(currentCSVPath.c_str()); else ui_create_csv_table(currentCSVPath.c_str());
                }
                break;
            case 5: // RIGHT
                data->key = LV_KEY_RIGHT;
                if (!currentCSVPath.isEmpty() && csvViewMode != 2 && (csv_col_offset + visible_cols) < (csv_max_col - 1)) {
                    csv_col_offset += visible_cols;
                    if(csvViewMode == 0) ui_create_csv_chart(currentCSVPath.c_str()); else ui_create_csv_table(currentCSVPath.c_str());
                }
                break;
            case 6: // TOGGLE VIEW
                data->key = LV_KEY_NEXT;
                if (!currentCSVPath.isEmpty()) {
                    ui_show_loading_screen();
                    lv_timer_handler();
                    delay(50);
                    csvViewMode = (csvViewMode + 1) % 3; // Cycle 0, 1, 2
                    switch (csvViewMode) {
                        case 0: ui_create_csv_chart(currentCSVPath.c_str()); break;
                        case 1: ui_create_csv_table(currentCSVPath.c_str()); break;
                        case 2: ui_create_csv_histogram(currentCSVPath.c_str()); break;
                    }
                }
                break;
        }
    } else if (current_key == -1 && last_key != -1) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->key = last_key;
        last_key = -1;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
