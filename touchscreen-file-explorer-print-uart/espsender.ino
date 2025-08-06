#include <lvgl.h>
#include <TFT_eSPI.h>
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <vector>
#include <HardwareSerial.h>


// ----------------------------------------------------------------
// --- HARDWARE & DISPLAY CONFIGURATION
// ----------------------------------------------------------------
#define SCREEN_WIDTH      480
#define SCREEN_HEIGHT     320
#define LVGL_TICK_PERIOD  5
#define SD_CS 5
#define TFT_BL_PIN 27
#define ARDUINO_TX 17
#define ARDUINO_RX 16


// ----------------------------------------------------------------
// --- GT911 CAPACITIVE TOUCH DRIVER
// ----------------------------------------------------------------
#define GT_CMD_WR           0XBA
#define GT_CMD_RD           0XBB
#define GT911_READ_XY_REG   0x814E
#define GT_CTRL_REG         0x8040
const int IIC_SCL = 32, IIC_SDA = 33, IIC_RST = 25;
#define IIC_SCL_0   digitalWrite(IIC_SCL,LOW)
#define IIC_SCL_1   digitalWrite(IIC_SCL,HIGH)
#define IIC_SDA_0   digitalWrite(IIC_SDA,LOW)
#define IIC_SDA_1   digitalWrite(IIC_SDA,HIGH)
#define READ_SDA    digitalRead(IIC_SDA)
typedef struct { uint8_t TouchCount; uint16_t X[5]; uint16_t Y[5]; } GT911_Dev;
GT911_Dev Dev_Now; bool has_touched = false;
void delay_us(unsigned int xus) { for (; xus > 1; xus--); }
void SDA_IN(void) { pinMode(IIC_SDA, INPUT); }
void SDA_OUT(void) { pinMode(IIC_SDA, OUTPUT); }
void IIC_Start(void) { SDA_OUT(); IIC_SDA_1; IIC_SCL_1; delay_us(4); IIC_SDA_0; delay_us(4); IIC_SCL_0; }
void IIC_Stop(void) { SDA_OUT(); IIC_SCL_0; IIC_SDA_0; delay_us(4); IIC_SCL_1; IIC_SDA_1; delay_us(4); }
uint8_t IIC_Wait_Ack(void) { uint8_t t=0; SDA_IN(); IIC_SDA_1; delay_us(1); IIC_SCL_1; delay_us(1); while(READ_SDA){t++; if(t>250){IIC_Stop();return 1;}} IIC_SCL_0; return 0; }
void IIC_Ack(void) { IIC_SCL_0; SDA_OUT(); IIC_SDA_0; delay_us(2); IIC_SCL_1; delay_us(2); IIC_SCL_0; }
void IIC_NAck(void) { IIC_SCL_0; SDA_OUT(); IIC_SDA_1; delay_us(2); IIC_SCL_1; delay_us(2); IIC_SCL_0; }
void IIC_Send_Byte(uint8_t txd) { uint8_t t; SDA_OUT();IIC_SCL_0; for(t=0;t<8;t++){if((txd&0x80)>>7)IIC_SDA_1;else IIC_SDA_0;txd<<=1;delay_us(2);IIC_SCL_1;delay_us(2);IIC_SCL_0;delay_us(2);}}
uint8_t IIC_Read_Byte(unsigned char ack){unsigned char i,r=0;SDA_IN();for(i=0;i<8;i++){IIC_SCL_0;delay_us(2);IIC_SCL_1;r<<=1;if(READ_SDA)r++;delay_us(1);}if(!ack)IIC_NAck();else IIC_Ack();return r;}
uint8_t GT911_WR_Reg(uint16_t r,uint8_t *b,uint8_t l){uint8_t i,ret=0;IIC_Start();IIC_Send_Byte(GT_CMD_WR);IIC_Wait_Ack();IIC_Send_Byte(r>>8);IIC_Wait_Ack();IIC_Send_Byte(r&0xFF);IIC_Wait_Ack();for(i=0;i<l;i++){IIC_Send_Byte(b[i]);ret=IIC_Wait_Ack();if(ret)break;}IIC_Stop();return ret;}
void GT911_RD_Reg(uint16_t r,uint8_t *b,uint8_t l){uint8_t i;IIC_Start();IIC_Send_Byte(GT_CMD_WR);IIC_Wait_Ack();IIC_Send_Byte(r>>8);IIC_Wait_Ack();IIC_Send_Byte(r&0xFF);IIC_Wait_Ack();IIC_Start();IIC_Send_Byte(GT_CMD_RD);IIC_Wait_Ack();for(i=0;i<l;i++){b[i]=IIC_Read_Byte(i==(l-1)?0:1);}IIC_Stop();}
void GT911_Scan(void){uint8_t b[41],c=0;GT911_RD_Reg(GT911_READ_XY_REG,b,1);if((b[0]&0x80)==0){has_touched=0;GT911_WR_Reg(GT911_READ_XY_REG,&c,1);delay(10);}else{has_touched=1;Dev_Now.TouchCount=b[0]&0x0f;if(Dev_Now.TouchCount>5||Dev_Now.TouchCount==0){has_touched=0;GT911_WR_Reg(GT911_READ_XY_REG,&c,1);return;}GT911_RD_Reg(GT911_READ_XY_REG+1,&b[1],Dev_Now.TouchCount*8);GT911_WR_Reg(GT911_READ_XY_REG,&c,1);Dev_Now.X[0]=((uint16_t)b[3]<<8)+b[2];Dev_Now.Y[0]=((uint16_t)b[5]<<8)+b[4];}}
void gt911_init(){pinMode(IIC_SDA,OUTPUT);pinMode(IIC_SCL,OUTPUT);pinMode(IIC_RST,OUTPUT);delay(50);digitalWrite(IIC_RST,LOW);delay(10);digitalWrite(IIC_RST,HIGH);delay(50);uint8_t b[1]={0x00};GT911_WR_Reg(GT_CTRL_REG,b,1);}


// ----------------------------------------------------------------
// --- OBJECTS, DRIVERS, and VARIABLES
// ----------------------------------------------------------------
TFT_eSPI tft = TFT_eSPI();
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * 20];


HardwareSerial SerialToArduino(1);


String currentPath = "/";
static String currentCSVPath = "";
static std::vector<String> csv_headers;
static int csvViewMode = 0;
static int chart_data_offset = 0;
static const int CHART_VISIBLE_POINTS = 10;
const char* series_hex_colors[] = {"#00BCD4", "#FF9800", "#9C27B0"};


static std::vector<unsigned long> line_start_positions;
static int total_data_rows = 0;
static int table_row_offset = 0;
static const int TABLE_PAGE_SIZE = 15;
static uint8_t current_brightness = 255;


lv_obj_t *main_container;
lv_obj_t *ui_btn_back, *ui_btn_toggle_view, *ui_btn_print;


// ----------------------------------------------------------------
// --- FUNCTION PROTOTYPES
// ----------------------------------------------------------------
String formatSize(size_t bytes);
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void touch_driver_read_cb(lv_indev_drv_t *indev, lv_indev_data_t *data);
bool index_csv_data(const char* filePath);
std::vector<std::vector<float>> get_csv_rows(int start_index, int count);
void ui_create_file_list(const char* path);
void ui_create_file_viewer(const char* filePath);
void ui_create_csv_table(const char* filePath);
void ui_create_csv_chart(const char* filePath);
void event_handler_go_back(lv_event_t * e);
void event_handler_toggle_view(lv_event_t * e);
void chart_event_handler(lv_event_t * e);
void chart_nav_button_event_handler(lv_event_t * e);
void event_handler_brightness_button(lv_event_t * e);
void event_handler_print_csv(lv_event_t * e);
void printCSV(String filename);
void sendLine(const char *line);
void waitForOK();
int splitCSV(String line, String *fields, int maxFields);




// ----------------------------------------------------------------
// --- ARDUINO SETUP & LOOP
// ----------------------------------------------------------------
void setup() {
    Serial.begin(9600);
    SerialToArduino.begin(9600, SERIAL_8N1, ARDUINO_RX, ARDUINO_TX);


    gt911_init();
    tft.begin();
    tft.setRotation(1);


    analogWrite(TFT_BL_PIN, current_brightness);


    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * 20);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_driver_read_cb;
    lv_indev_drv_register(&indev_drv);


    if (!SD.begin(SD_CS)) {
        lv_obj_t* label = lv_label_create(lv_scr_act());
        lv_label_set_text(label, "SD Card Mount Failed!");
        lv_obj_center(label);
        while (1);
    }
   
    // --- Create Top Bar Buttons ---
    ui_btn_back = lv_btn_create(lv_scr_act());
    lv_obj_set_size(ui_btn_back, 45, 40);
    lv_obj_align(ui_btn_back, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_add_event_cb(ui_btn_back, event_handler_go_back, LV_EVENT_CLICKED, NULL);
    lv_obj_t* back_label = lv_label_create(ui_btn_back);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);


    ui_btn_toggle_view = lv_btn_create(lv_scr_act());
    lv_obj_set_size(ui_btn_toggle_view, 45, 40);
    lv_obj_align(ui_btn_toggle_view, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_add_event_cb(ui_btn_toggle_view, event_handler_toggle_view, LV_EVENT_CLICKED, NULL);
    lv_obj_t* toggle_label = lv_label_create(ui_btn_toggle_view);
    lv_label_set_text(toggle_label, LV_SYMBOL_EYE_OPEN);
    lv_obj_center(toggle_label);


    lv_obj_t* btn_brt = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_brt, 45, 40);
    lv_obj_align(btn_brt, LV_ALIGN_TOP_RIGHT, -55, 5);
    lv_obj_add_event_cb(btn_brt, event_handler_brightness_button, LV_EVENT_CLICKED, NULL);
    lv_obj_t* label_brt = lv_label_create(btn_brt);
    lv_label_set_text(label_brt, LV_SYMBOL_SETTINGS);
    lv_obj_center(label_brt);


    // --- Create Print Button --- // <-- ADDED FOR PRINTING
    ui_btn_print = lv_btn_create(lv_scr_act());
    lv_obj_set_size(ui_btn_print, 45, 40);
    lv_obj_align(ui_btn_print, LV_ALIGN_TOP_RIGHT, -105, 5);
    lv_obj_add_event_cb(ui_btn_print, event_handler_print_csv, LV_EVENT_CLICKED, NULL);
    lv_obj_t* print_label = lv_label_create(ui_btn_print);
    lv_label_set_text(print_label, LV_SYMBOL_TINT); // Or LV_SYMBOL_SAVE
    lv_obj_center(print_label);
   
    // --- Create Main Content Area ---
    main_container = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(main_container);
    lv_obj_set_size(main_container, lv_pct(100), SCREEN_HEIGHT - 50);
    lv_obj_align(main_container, LV_ALIGN_BOTTOM_MID, 0, 0);


    ui_create_file_list(currentPath.c_str());
}


void loop() {
    lv_timer_handler();
    delay(LVGL_TICK_PERIOD);
}




// ----------------------------------------------------------------
// --- EVENT HANDLERS
// ----------------------------------------------------------------
void event_handler_go_back(lv_event_t* e){if(!currentCSVPath.isEmpty()){ui_create_file_list(currentPath.c_str());}else if(currentPath!="/"){int l=currentPath.lastIndexOf('/');String p=(l>0)?currentPath.substring(0,l):"/";ui_create_file_list(p.c_str());}}
void event_handler_toggle_view(lv_event_t* e){if(!currentCSVPath.isEmpty()){csvViewMode=(csvViewMode+1)%2;if(csvViewMode==0){ui_create_csv_chart(currentCSVPath.c_str());}else{table_row_offset=0;ui_create_csv_table(currentCSVPath.c_str());}}}
void chart_nav_button_event_handler(lv_event_t * e) {intptr_t d=(intptr_t)lv_event_get_user_data(e);chart_data_offset+=d*CHART_VISIBLE_POINTS;if(chart_data_offset<0)chart_data_offset=0;if(total_data_rows<=CHART_VISIBLE_POINTS){chart_data_offset=0;}else if(chart_data_offset>total_data_rows-CHART_VISIBLE_POINTS){chart_data_offset=total_data_rows-CHART_VISIBLE_POINTS;}ui_create_csv_chart(currentCSVPath.c_str());}
void chart_event_handler(lv_event_t* e){lv_event_code_t c=lv_event_get_code(e);lv_obj_t* chart=lv_event_get_target(e);lv_obj_t* info_label=static_cast<lv_obj_t*>(lv_event_get_user_data(e));if(c==LV_EVENT_RELEASED){lv_indev_t*i=lv_indev_get_act();if(!i)return;lv_point_t v;lv_indev_get_vect(i,&v);if(abs(v.x)>40){int s=(v.x<0)?1:-1;chart_data_offset+=s*(CHART_VISIBLE_POINTS/2);if(chart_data_offset<0)chart_data_offset=0;if(total_data_rows<=CHART_VISIBLE_POINTS){chart_data_offset=0;}else if(chart_data_offset>total_data_rows-CHART_VISIBLE_POINTS){chart_data_offset=total_data_rows-CHART_VISIBLE_POINTS;}ui_create_csv_chart(currentCSVPath.c_str());return;}}if(c==LV_EVENT_PRESSING||c==LV_EVENT_PRESSED){lv_point_t p;lv_indev_get_point(lv_indev_get_act(),&p);lv_obj_transform_point(chart,&p,true,false);int n=lv_chart_get_point_count(chart);if(n<2)return;lv_coord_t w=lv_obj_get_content_width(chart);int id=(p.x*(n-1)+w/2)/w;if(id<0)id=0;if(id>=n)id=n-1;lv_point_t pp;lv_chart_get_point_pos_by_id(chart,lv_chart_get_series_next(chart,NULL),id,&pp);static lv_point_t lp[2];lp[0]={pp.x,0};lp[1]={pp.x,lv_obj_get_height(chart)};lv_obj_t*cl=lv_obj_get_child(chart,0);lv_line_set_points(cl,lp,2);lv_obj_clear_flag(cl,LV_OBJ_FLAG_HIDDEN);int idx=chart_data_offset+id;if(idx>=total_data_rows)return;std::vector<std::vector<float>>rows=get_csv_rows(idx,1);if(rows.empty())return;String s;s+=csv_headers[0]+": "+String(rows[0][0],2);for(size_t i=1;i<csv_headers.size();i++){s+="\n";s+=String(series_hex_colors[(i-1)%3])+" "+csv_headers[i]+":# "+String(rows[0][i],2);}lv_label_set_text(info_label,s.c_str());}}
void event_handler_brightness_button(lv_event_t * e) {lv_obj_t*m=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(m);lv_obj_set_size(m,lv_pct(100),lv_pct(100));lv_obj_set_style_bg_color(m,lv_color_black(),0);lv_obj_set_style_bg_opa(m,LV_OPA_50,0);lv_obj_add_event_cb(m,[](lv_event_t*ev){lv_obj_del(lv_event_get_target(ev));},LV_EVENT_CLICKED,NULL);lv_obj_t*s_c=lv_obj_create(m);lv_obj_set_size(s_c,300,100);lv_obj_center(s_c);lv_obj_t*l=lv_label_create(s_c);lv_label_set_text(l,"Brightness");lv_obj_align(l,LV_ALIGN_TOP_MID,0,10);lv_obj_t*s=lv_slider_create(s_c);lv_slider_set_range(s,0,255);lv_slider_set_value(s,current_brightness,LV_ANIM_OFF);lv_obj_set_width(s,250);lv_obj_center(s);lv_obj_add_event_cb(s,[](lv_event_t*ev){lv_obj_t*sl=lv_event_get_target(ev);int v=lv_slider_get_value(sl);analogWrite(TFT_BL_PIN,v);current_brightness=v;},LV_EVENT_VALUE_CHANGED,NULL);}


// --- Print Event Handler --- // <-- ADDED FOR PRINTING
void event_handler_print_csv(lv_event_t * e) {
    if (currentCSVPath.isEmpty()) return;


    // Create a modal overlay to show status
    lv_obj_t* overlay = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);


    lv_obj_t* msg_box = lv_msgbox_create(overlay, "Printing", "Sending data to printer...\nPlease wait.", NULL, false);
    lv_obj_center(msg_box);


    lv_timer_handler(); // Allow LVGL to draw the modal
    delay(50);


    printCSV(currentCSVPath); // Call the blocking print function


    lv_obj_del(overlay); // Clean up the modal
}


// ----------------------------------------------------------------
// --- UI SCREEN CREATION
// ----------------------------------------------------------------
String formatSize(size_t bytes){if(bytes<1024)return String(bytes)+" B";if(bytes<1024*1024)return String(bytes/1024.0,1)+" KB";return String(bytes/1024.0/1024.0,1)+" MB";}


void ui_create_file_list(const char* path) {
    lv_obj_add_flag(ui_btn_toggle_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_btn_print, LV_OBJ_FLAG_HIDDEN);
    currentPath=="/"?lv_obj_add_flag(ui_btn_back,LV_OBJ_FLAG_HIDDEN):lv_obj_clear_flag(ui_btn_back,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(main_container);
    currentPath=path;currentCSVPath="";
    lv_obj_set_flex_flow(main_container,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_container,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_t* title_label=lv_label_create(main_container);
    lv_label_set_text(title_label,"SD CARD FILE EXPLORER");
    lv_obj_set_style_text_font(title_label,&lv_font_montserrat_14,0);
    lv_obj_set_style_pad_top(title_label,5,0);
    lv_obj_set_style_pad_bottom(title_label,5,0);
    lv_obj_t* file_list=lv_list_create(main_container);
    lv_obj_set_size(file_list,lv_pct(100),LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(file_list,1);
    File root=SD.open(path);
    if(!root){lv_list_add_text(file_list,"Failed to open directory");return;}
    File file=root.openNextFile();
    while(file){
        String d=file.isDirectory()?file.name():String(file.name())+" ("+formatSize(file.size())+")";
        lv_obj_t* b=lv_list_add_btn(file_list,file.isDirectory()?LV_SYMBOL_DIRECTORY:LV_SYMBOL_FILE,d.c_str());
        lv_obj_add_event_cb(b,[](lv_event_t*e){char*f=(char*)lv_event_get_user_data(e);ui_create_file_viewer(f);free(f);},LV_EVENT_CLICKED,strdup(file.path()));
        file.close();file=root.openNextFile();
    }
}


void ui_create_file_viewer(const char* filePath) {
    if (String(filePath).endsWith(".csv")) {
        if (!index_csv_data(filePath)) {
            lv_obj_clean(main_container);
            lv_obj_t* label = lv_label_create(main_container);
            lv_label_set_text(label, "CSV has no valid data rows.");
            lv_obj_center(label);
        } else {
            currentCSVPath = filePath;
            csvViewMode = 0;
            chart_data_offset = 0;
            ui_create_csv_chart(filePath);
        }
    } else {
        currentCSVPath = ""; // Not a CSV file
        lv_obj_add_flag(ui_btn_toggle_view, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_btn_print, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_btn_back, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clean(main_container);
        File file = SD.open(filePath);
        if (file) {
            lv_obj_t* ta = lv_textarea_create(main_container);
            lv_obj_set_size(ta, lv_pct(100), lv_pct(100));
            String content;
            while(file.available()) { content += (char)file.read(); }
            lv_textarea_set_text(ta, content.c_str());
            file.close();
        }
    }
}


void ui_create_csv_chart(const char* filePath) {
    lv_obj_clear_flag(ui_btn_back,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_btn_toggle_view,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_btn_print, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(main_container);
    lv_obj_t* info_label=lv_label_create(main_container);
    lv_label_set_text(info_label,"Tap or drag on the chart to see values.");
    lv_obj_set_width(info_label,lv_pct(98));
    lv_label_set_long_mode(info_label,LV_LABEL_LONG_WRAP);
    lv_obj_align(info_label,LV_ALIGN_TOP_LEFT,5,5);
    lv_label_set_recolor(info_label,true);
    lv_obj_t* chart_area=lv_obj_create(main_container);
    lv_obj_remove_style_all(chart_area);
    lv_obj_set_size(chart_area,lv_pct(100),lv_obj_get_height(main_container)-70);
    lv_obj_align(chart_area,LV_ALIGN_BOTTOM_MID,0,0);
    lv_obj_t* btn_left=lv_btn_create(chart_area);
    lv_obj_set_size(btn_left,40,40);
    lv_obj_align(btn_left,LV_ALIGN_LEFT_MID,5,0);
    lv_obj_add_event_cb(btn_left,chart_nav_button_event_handler,LV_EVENT_CLICKED,(void*)-1);
    lv_obj_t* label_left=lv_label_create(btn_left);
    lv_label_set_text(label_left,LV_SYMBOL_LEFT);
    lv_obj_center(label_left);
    lv_obj_t* btn_right=lv_btn_create(chart_area);
    lv_obj_set_size(btn_right,40,40);
    lv_obj_align(btn_right,LV_ALIGN_RIGHT_MID,-5,0);
    lv_obj_add_event_cb(btn_right,chart_nav_button_event_handler,LV_EVENT_CLICKED,(void*)1);
    lv_obj_t* label_right=lv_label_create(btn_right);
    lv_label_set_text(label_right,LV_SYMBOL_RIGHT);
    lv_obj_center(label_right);
    lv_obj_t* chart=lv_chart_create(chart_area);
    lv_obj_set_size(chart,350,lv_pct(85));
    lv_obj_align(chart,LV_ALIGN_CENTER,0,0);
    lv_obj_add_event_cb(chart,chart_event_handler,LV_EVENT_ALL,info_label);
    lv_obj_t* cursor_line=lv_line_create(chart);
    static lv_style_t style_line;lv_style_init(&style_line);
    lv_style_set_line_width(&style_line,2);
    lv_style_set_line_color(&style_line,lv_palette_main(LV_PALETTE_RED));
    lv_obj_add_style(cursor_line,&style_line,0);
    lv_obj_add_flag(cursor_line,LV_OBJ_FLAG_HIDDEN);
    int points_to_draw=min(total_data_rows-chart_data_offset,CHART_VISIBLE_POINTS);
    if(points_to_draw<=0)return;
    std::vector<std::vector<float>> visible_data=get_csv_rows(chart_data_offset,points_to_draw);
    if(visible_data.empty())return;
    float min_val=1.0e+38,max_val=-1.0e+38;
    for(const auto& row:visible_data)for(size_t j=1;j<row.size();++j){if(row[j]<min_val)min_val=row[j];if(row[j]>max_val)max_val=row[j];}
    if(max_val<=min_val){min_val=0;max_val=1;}
    lv_chart_set_type(chart,LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart,points_to_draw);
    lv_chart_set_range(chart,LV_CHART_AXIS_PRIMARY_Y,(lv_coord_t)(min_val*0.95),(lv_coord_t)(max_val*1.05));
    lv_chart_set_div_line_count(chart,5,10);
    lv_color_t colors[]={lv_palette_main(LV_PALETTE_CYAN),lv_palette_main(LV_PALETTE_ORANGE),lv_palette_main(LV_PALETTE_PURPLE)};
    for(size_t i=1;i<csv_headers.size();++i){
        lv_chart_series_t* ser=lv_chart_add_series(chart,colors[(i-1)%3],LV_CHART_AXIS_PRIMARY_Y);
        for(int j=0;j<points_to_draw;++j){lv_chart_set_next_value(chart,ser,(lv_coord_t)visible_data[j][i]);}
    }
    lv_obj_t* x_axis_label=lv_label_create(chart_area);
    if(!csv_headers.empty())lv_label_set_text(x_axis_label,csv_headers[0].c_str());
    lv_obj_align_to(x_axis_label,chart,LV_ALIGN_OUT_BOTTOM_MID,0,10);
    lv_obj_clear_flag(x_axis_label,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* y_axis_label=lv_label_create(chart_area);
    lv_label_set_text(y_axis_label,"Value");
    static lv_style_t style_rotated;lv_style_init(&style_rotated);
    lv_style_set_transform_angle(&style_rotated,2700);
    lv_obj_add_style(y_axis_label,&style_rotated,0);
    lv_obj_align_to(y_axis_label,chart,LV_ALIGN_OUT_LEFT_MID,-20,0);
    lv_obj_clear_flag(y_axis_label,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* p_label=lv_label_create(chart);
    lv_label_set_text_fmt(p_label,"%d-%d of %d",chart_data_offset+1,chart_data_offset+points_to_draw,total_data_rows);
    lv_obj_set_style_text_color(p_label,lv_color_hex(0x888888),0);
    lv_obj_align(p_label,LV_ALIGN_TOP_RIGHT,-5,5);
}


void ui_create_csv_table(const char* filePath) {
    lv_obj_clear_flag(ui_btn_back, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_btn_toggle_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_btn_print, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(main_container);
    lv_obj_set_flex_flow(main_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_t* nav_bar = lv_obj_create(main_container);
    lv_obj_remove_style_all(nav_bar);
    lv_obj_set_width(nav_bar, lv_pct(100));
    lv_obj_set_height(nav_bar, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(nav_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* btn_up = lv_btn_create(nav_bar);
    lv_obj_t* label_up = lv_label_create(btn_up);
    lv_label_set_text(label_up, LV_SYMBOL_UP " Up");
    lv_obj_add_event_cb(btn_up, [](lv_event_t * e) {
        if (table_row_offset > 0) { table_row_offset -= TABLE_PAGE_SIZE; if (table_row_offset < 0) table_row_offset = 0; ui_create_csv_table(currentCSVPath.c_str()); }
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* progress_label = lv_label_create(nav_bar);
    int end_row = min(table_row_offset + TABLE_PAGE_SIZE, total_data_rows);
    lv_label_set_text_fmt(progress_label, "Rows %d-%d of %d", table_row_offset + 1, end_row, total_data_rows);
    lv_obj_t* btn_down = lv_btn_create(nav_bar);
    lv_obj_t* label_down = lv_label_create(btn_down);
    lv_label_set_text(label_down, LV_SYMBOL_DOWN " Down");
    lv_obj_add_event_cb(btn_down, [](lv_event_t * e) {
        if (table_row_offset + TABLE_PAGE_SIZE < total_data_rows) { table_row_offset += TABLE_PAGE_SIZE; ui_create_csv_table(currentCSVPath.c_str()); }
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* table_container = lv_obj_create(main_container);
    lv_obj_remove_style_all(table_container);
    lv_obj_set_flex_grow(table_container, 1);
    lv_obj_set_width(table_container, lv_pct(100));
    std::vector<std::vector<float>> page_data = get_csv_rows(table_row_offset, TABLE_PAGE_SIZE);
    lv_obj_t* table = lv_table_create(table_container);
    lv_obj_set_size(table, lv_pct(100), lv_pct(100));
    lv_table_set_col_cnt(table, csv_headers.size());
    lv_table_set_row_cnt(table, page_data.size() + 1);
    for (size_t i=0;i<csv_headers.size();++i){lv_table_set_col_width(table,i,120);lv_table_set_cell_value(table,0,i,csv_headers[i].c_str());}
    char cell[16]; for(size_t i=0;i<page_data.size();++i)for(size_t j=0;j<page_data[i].size();++j){snprintf(cell,sizeof(cell),"%.2f",page_data[i][j]);lv_table_set_cell_value(table,i+1,j,cell);}
}


// ----------------------------------------------------------------
// --- DATA HANDLING & PRINTING
// ----------------------------------------------------------------
bool index_csv_data(const char* filePath) {
    line_start_positions.clear();
    csv_headers.clear();
    total_data_rows = 0;
    File file = SD.open(filePath);
    if (!file) return false;
    if (file.available()) {
        String h = file.readStringUntil('\n'); h.trim(); int l = -1;
        for(int i=0; i<=h.length(); i++) if(h[i]==',' || i==h.length()) { csv_headers.push_back(h.substring(l+1,i)); l=i; }
    }
    if(csv_headers.size() < 1) { file.close(); return false; }
    while(file.available()) {
        line_start_positions.push_back(file.position());
        file.readStringUntil('\n');
    }
    total_data_rows = line_start_positions.size();
    file.close();
    return total_data_rows > 0;
}


std::vector<std::vector<float>> get_csv_rows(int start_index, int count) {
    std::vector<std::vector<float>> rows;
    if (start_index < 0 || start_index >= total_data_rows) return rows;
    File file = SD.open(currentCSVPath.c_str());
    if (!file) return rows;
    file.seek(line_start_positions[start_index]);
    for (int k=0; k < count && file.available(); k++) {
        String d = file.readStringUntil('\n'); d.trim(); if (d.length() == 0) continue;
        std::vector<float> r; int l = -1;
        for (int i=0; i<=d.length(); i++) if (d[i]==',' || i==d.length()) { r.push_back(d.substring(l+1,i).toFloat()); l=i; }
        if (r.size() == csv_headers.size()) { rows.push_back(r); }
    }
    file.close();
    return rows;
}




void printCSV(String filename) {
    if (!SD.exists(filename.c_str())) {
        Serial.println("File does not exist!");
        return;
    }


    File file = SD.open(filename.c_str());
    if (!file) {
        Serial.println("Could not open file.");
            return;
        }


        sendLine("No.  Time     Flow     SetPt");


        int lineNum = 0;
        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.length() == 0) continue;


            if (line.startsWith("Time") || line.startsWith("Datetime") || line.startsWith(csv_headers[0])) continue;


            String fields[4];
            int fieldCount = splitCSV(line, fields, 4);
            if (fieldCount < 3) continue;


            String timeOnly = fields[0];
            int spaceIdx = timeOnly.indexOf(' ');
            if (spaceIdx > 0) timeOnly = timeOnly.substring(spaceIdx + 1);


            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%02d   %-8s %-7s %-7s",
                    ++lineNum,
                    timeOnly.c_str(),
                 fields[1].c_str(),
                 fields[2].c_str());


        sendLine(buffer);
    }
    file.close();
}


void sendLine(const char *line) {
    Serial.println(line); // For debugging in Serial Monitor
    SerialToArduino.println(line);
    waitForOK();
}


void waitForOK() {
    unsigned long start = millis();
    String response = "";
    while (millis() - start < 5000) { // 5 second timeout
        while (SerialToArduino.available()) {
            char c = SerialToArduino.read();
            if (c == '\n' || c == '\r') {
                if (response == "OK") return;
                response = "";
            } else {
                response += c;
            }
        }
    }
}


int splitCSV(String line, String *fields, int maxFields) {
    int index = 0;
    int last_comma = -1;
    for (int i=0; i < line.length() && index < maxFields; i++){
      if(line.charAt(i) == ','){
        fields[index++] = line.substring(last_comma + 1, i);
        last_comma = i;
      }
    }
    if (index < maxFields) {
        fields[index++] = line.substring(last_comma + 1);
    }
   
    while (index < maxFields) fields[index++] = "";
    return index;
}




// ----------------------------------------------------------------
// --- LVGL DRIVER CALLBACKS
// ----------------------------------------------------------------
void my_disp_flush(lv_disp_drv_t*d,const lv_area_t*a,lv_color_t*p){uint32_t w=lv_area_get_width(a),h=lv_area_get_height(a);tft.startWrite();tft.setAddrWindow(a->x1,a->y1,w,h);tft.pushColors((uint16_t*)&p->full,w*h,true);tft.endWrite();lv_disp_flush_ready(d);}
void touch_driver_read_cb(lv_indev_drv_t*d,lv_indev_data_t*a){GT911_Scan();if(!has_touched){a->state=LV_INDEV_STATE_REL;return;}a->state=LV_INDEV_STATE_PR;a->point.x=Dev_Now.Y[0];a->point.y=SCREEN_HEIGHT-Dev_Now.X[0];}
