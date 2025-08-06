// --- HARDWARE & DISPLAY CONFIGURATION ---
#define TFT_BL_PIN 27


// --- OBJECTS, DRIVERS, and VARIABLES ---
static uint8_t current_brightness = 255;


void setup() {
    // ... (kode lainnya) ...
    analogWrite(TFT_BL_PIN, current_brightness); // Mengatur kecerahan awal saat startup
    // ... (kode lainnya) ...


    // Membuat tombol untuk pengaturan kecerahan
    lv_obj_t* btn_brt = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_brt, 45, 40);
    lv_obj_align(btn_brt, LV_ALIGN_TOP_RIGHT, -55, 5);
    lv_obj_add_event_cb(btn_brt, event_handler_brightness_button, LV_EVENT_CLICKED, NULL);
    lv_obj_t* label_brt = lv_label_create(btn_brt);
    lv_label_set_text(label_brt, LV_SYMBOL_SETTINGS);
    lv_obj_center(label_brt);
   
    // ... (kode lainnya) ...
}


void event_handler_brightness_button(lv_event_t * e) {
    // Set kecerahan ke maksimal agar dialog terlihat jelas
    analogWrite(TFT_BL_PIN, 255);


    // Buat lapisan latar belakang gelap
    lv_obj_t* overlay = lv_obj_create(lv_scr_act());
    // ... (styling overlay) ...
   
    // Buat kontainer untuk slider
    lv_obj_t* slider_container = lv_obj_create(overlay);
    lv_obj_set_size(slider_container, 300, 120);
    lv_obj_center(slider_container);


    // Label judul "Brightness"
    lv_obj_t* label_title = lv_label_create(slider_container);
    lv_label_set_text(label_title, "Brightness");
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 10);


    // Buat slider
    lv_obj_t* slider = lv_slider_create(slider_container);
    lv_slider_set_range(slider, 0, 255);
    lv_slider_set_value(slider, current_brightness, LV_ANIM_OFF); // Set nilai awal slider
    lv_obj_set_width(slider, 250);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);


    // Buat label untuk menampilkan persentase
    lv_obj_t* label_percent = lv_label_create(slider_container);
    int initial_percent = (current_brightness * 100) / 255;
    lv_label_set_text_fmt(label_percent, "%d%%", initial_percent);
    lv_obj_align_to(label_percent, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);


    // Tambahkan event saat nilai slider berubah, panggil 'slider_event_cb'
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, label_percent);


    // Event untuk menutup dialog
    lv_obj_add_event_cb(overlay, [](lv_event_t* ev) {
        // Kembalikan kecerahan ke nilai terakhir yang disimpan
        analogWrite(TFT_BL_PIN, current_brightness);
        lv_obj_del(lv_event_get_target(ev)); // Hapus dialog
    }, LV_EVENT_CLICKED, NULL);
}


static void slider_event_cb(lv_event_t * e) {
    // Ambil objek slider dan label dari event
    lv_obj_t* slider = lv_event_get_target(e);
    lv_obj_t* label_percent = (lv_obj_t*)lv_event_get_user_data(e);


    // Dapatkan nilai slider (0-255)
    int value = lv_slider_get_value(slider);
   
    // Perbarui kecerahan fisik dan simpan nilainya
    analogWrite(TFT_BL_PIN, value);
    current_brightness = value;
   
    // Hitung dan perbarui teks persentase
    int percent = (value * 100) / 255;
    lv_label_set_text_fmt(label_percent, "%d%%", percent);
}
