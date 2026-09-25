#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GUITION_LCD_H_RES 480
#define GUITION_LCD_V_RES 800

// Hardware support for the Guition JC4880P443C_I_W / JC-ESP32P4-M3 board.
esp_err_t guition_display_init(esp_lcd_panel_handle_t* panel,
                               esp_lcd_panel_io_handle_t* panel_io);
esp_err_t guition_touch_init(esp_lcd_touch_handle_t* touch);
esp_err_t guition_backlight_set(uint8_t percent);

#ifdef __cplusplus
}
#endif
