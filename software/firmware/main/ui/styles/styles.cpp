#include "styles.h"
#include <esp_log.h>

static const char* TAG = "STYLES";

// Declare external custom fonts from lv_conf.h
LV_FONT_DECLARE(custom_font_normal_16);
LV_FONT_DECLARE(custom_font_bold_20);
LV_FONT_DECLARE(custom_font_bold_24);

// Font pointers
static const lv_font_t* font_normal = nullptr;
static const lv_font_t* font_bold = nullptr;
static const lv_font_t* font_light = nullptr;
static const lv_font_t* font_button = nullptr;

void styles_init(void) {
    ESP_LOGI(TAG, "Initializing styles");

    // Use custom fonts from assets
    font_normal = &custom_font_normal_16;
    font_bold = &custom_font_bold_20;
    font_light = &lv_font_montserrat_14;  // Use built-in for light
    font_button = &custom_font_bold_24;

    ESP_LOGI(TAG, "Styles initialized with custom fonts");
}

const lv_font_t* styles_get_font_normal(void) {
    return font_normal ? font_normal : &lv_font_montserrat_16;
}

const lv_font_t* styles_get_font_bold(void) {
    return font_bold ? font_bold : &lv_font_montserrat_20;
}

const lv_font_t* styles_get_font_light(void) {
    return font_light ? font_light : &lv_font_montserrat_14;
}

const lv_font_t* styles_get_font_button(void) {
    return font_button ? font_button : &lv_font_montserrat_18;
}
