/*
 * Guition JC4880P443C_I_W display/touch support.
 *
 * The panel needs a manual ST7701S command sequence. The stock ST7701
 * component attempts an unsupported DSI ID read and leaves this board black.
 * Pin assignments, timings, and the command table are based on the MIT-licensed
 * guition-jc4880p4-bsp by ultramcu and Guition's board documentation.
 */

#include "guition_jc4880p443.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "guition_st7701_init.h"

static const char* TAG = "GUITION_BOARD";

enum {
    LCD_RESET_GPIO = 5,
    LCD_BACKLIGHT_GPIO = 23,
    TOUCH_RESET_GPIO = 3,
    TOUCH_SDA_GPIO = 7,
    TOUCH_SCL_GPIO = 8,
};

static esp_ldo_channel_handle_t s_dsi_phy_ldo;
static esp_lcd_dsi_bus_handle_t s_dsi_bus;
static esp_lcd_panel_io_handle_t s_panel_io;
static esp_lcd_panel_handle_t s_panel;
static i2c_master_bus_handle_t s_touch_i2c;
static esp_lcd_panel_io_handle_t s_touch_io;
static esp_lcd_touch_handle_t s_touch;
static bool s_backlight_ready;

static esp_err_t init_backlight(void)
{
    if (s_backlight_ready) {
        return ESP_OK;
    }

    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_1,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "backlight timer init failed");

    const ledc_channel_config_t channel = {
        .gpio_num = LCD_BACKLIGHT_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_1,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "backlight channel init failed");
    s_backlight_ready = true;
    return ESP_OK;
}

esp_err_t guition_backlight_set(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    ESP_RETURN_ON_ERROR(init_backlight(), TAG, "backlight init failed");

    const uint32_t duty = (1023U * percent) / 100U;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty),
                        TAG, "backlight duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1),
                        TAG, "backlight update failed");
    return ESP_OK;
}

static esp_err_t reset_panel(void)
{
    const gpio_config_t reset_config = {
        .pin_bit_mask = 1ULL << LCD_RESET_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&reset_config), TAG, "panel reset GPIO init failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_RESET_GPIO, 0), TAG, "panel reset low failed");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_RESET_GPIO, 1), TAG, "panel reset high failed");
    vTaskDelay(pdMS_TO_TICKS(120));
    return ESP_OK;
}

static esp_err_t send_panel_init_sequence(void)
{
    for (size_t i = 0; i < BOARD_P4_PANEL_INIT_CMDS_SIZE; ++i) {
        const board_p4_panel_init_cmd_t* entry = &board_p4_panel_init_cmds[i];
        ESP_RETURN_ON_ERROR(
            esp_lcd_panel_io_tx_param(s_panel_io, entry->cmd, entry->data, entry->len),
            TAG, "panel command 0x%02x failed", entry->cmd);
        if (entry->delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(entry->delay_ms));
        }
    }
    return ESP_OK;
}

esp_err_t guition_display_init(esp_lcd_panel_handle_t* panel,
                               esp_lcd_panel_io_handle_t* panel_io)
{
    ESP_RETURN_ON_FALSE(panel && panel_io, ESP_ERR_INVALID_ARG, TAG, "missing output handle");
    if (s_panel) {
        *panel = s_panel;
        *panel_io = s_panel_io;
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing Guition JC4880P443 ST7701S display");

    const esp_ldo_channel_config_t ldo_config = {
        .chan_id = 3,
        .voltage_mv = 2500,
    };
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_config, &s_dsi_phy_ldo),
                        TAG, "DSI PHY power failed");

    const esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = 2,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = 500,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_config, &s_dsi_bus),
                        TAG, "DSI bus init failed");

    const esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(s_dsi_bus, &dbi_config, &s_panel_io),
                        TAG, "DSI command IO init failed");

    const esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = 34,
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .out_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = 3,
        .video_timing = {
            .h_size = GUITION_LCD_H_RES,
            .v_size = GUITION_LCD_V_RES,
            .hsync_pulse_width = 12,
            .hsync_back_porch = 42,
            .hsync_front_porch = 42,
            .vsync_pulse_width = 2,
            .vsync_back_porch = 8,
            .vsync_front_porch = 166,
        },
        .flags.use_dma2d = true,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_dpi(s_dsi_bus, &dpi_config, &s_panel),
                        TAG, "DPI panel init failed");

    ESP_RETURN_ON_ERROR(reset_panel(), TAG, "panel reset failed");
    ESP_RETURN_ON_ERROR(send_panel_init_sequence(), TAG, "ST7701S sequence failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "DPI panel start failed");
    ESP_RETURN_ON_ERROR(guition_backlight_set(100), TAG, "backlight enable failed");

    *panel = s_panel;
    *panel_io = s_panel_io;
    ESP_LOGI(TAG, "Guition display initialized at 480x800");
    return ESP_OK;
}

esp_err_t guition_touch_init(esp_lcd_touch_handle_t* touch)
{
    ESP_RETURN_ON_FALSE(touch, ESP_ERR_INVALID_ARG, TAG, "missing touch output handle");
    if (s_touch) {
        *touch = s_touch;
        return ESP_OK;
    }

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = TOUCH_SDA_GPIO,
        .scl_io_num = TOUCH_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &s_touch_i2c),
                        TAG, "touch I2C init failed");

    const gpio_config_t reset_config = {
        .pin_bit_mask = 1ULL << TOUCH_RESET_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&reset_config), TAG, "touch reset GPIO init failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(TOUCH_RESET_GPIO, 0), TAG, "touch reset low failed");
    esp_rom_delay_us(10000);
    ESP_RETURN_ON_ERROR(gpio_set_level(TOUCH_RESET_GPIO, 1), TAG, "touch reset high failed");
    esp_rom_delay_us(50000);

    uint8_t address = 0;
    if (i2c_master_probe(s_touch_i2c, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS, 100) == ESP_OK) {
        address = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS;
    } else if (i2c_master_probe(s_touch_i2c, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP, 100) == ESP_OK) {
        address = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP;
    } else {
        return ESP_ERR_NOT_FOUND;
    }

    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_config.dev_addr = address;
    io_config.scl_speed_hz = 400000;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_touch_i2c, &io_config, &s_touch_io),
                        TAG, "touch panel IO init failed");

    const esp_lcd_touch_config_t touch_config = {
        .x_max = GUITION_LCD_H_RES,
        .y_max = GUITION_LCD_V_RES,
        .rst_gpio_num = TOUCH_RESET_GPIO,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt911(s_touch_io, &touch_config, &s_touch),
                        TAG, "GT911 init failed");

    *touch = s_touch;
    ESP_LOGI(TAG, "GT911 touch initialized at 0x%02x", address);
    return ESP_OK;
}
