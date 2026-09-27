#include "home_screen.h"
#include "../screen_manager.h"
#include "../../components/status_icons.h"
#include "../../images/menu_icons.h"
#include "../../styles/styles.h"
#include <esp_log.h>
#include <cstdint>

static const char* TAG = "HOME_SCREEN";

namespace {

constexpr int SCREEN_WIDTH = 480;
constexpr uint32_t MENU_BG = 0x08131A;
constexpr uint32_t MENU_TILE = 0x10232C;
constexpr uint32_t MENU_BORDER = 0x365B6E;
constexpr uint32_t MENU_ICON = 0xC3D4E5;
constexpr uint32_t MENU_CYAN = 0x42D9E9;

void menu_button_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    const auto target = static_cast<screen_id_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    ESP_LOGI(TAG, "Menu button clicked, navigating to screen %d", target);
    screen_manager_show(target);
}

lv_obj_t* shape(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color, int radius = 0) {
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

void add_menu_icon(lv_obj_t* parent, const lv_image_dsc_t* artwork, bool hero) {
    lv_obj_t* icon = lv_image_create(parent);
    lv_image_set_src(icon, artwork);
    lv_obj_set_style_image_recolor(icon, lv_color_hex(hero ? MENU_CYAN : MENU_ICON), 0);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_obj_set_pos(icon, hero ? 38 : 12, hero ? 36 : 30);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
}

void create_tile(lv_obj_t* screen, const char* text, screen_id_t target,
                 int x, int y, int w, int h, bool hero, const lv_image_dsc_t* artwork) {
    lv_obj_t* tile = lv_obj_create(screen);
    lv_obj_set_pos(tile, x, y);
    lv_obj_set_size(tile, w, h);
    lv_obj_set_style_bg_color(tile, lv_color_hex(MENU_TILE), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(tile, lv_color_hex(0x183844), LV_STATE_PRESSED);
    lv_obj_set_style_radius(tile, 9, 0);
    lv_obj_set_style_border_width(tile, 1, 0);
    lv_obj_set_style_border_color(tile, lv_color_hex(hero ? MENU_CYAN : MENU_BORDER), 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tile, menu_button_event_cb, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(target)));

    add_menu_icon(tile, artwork, hero);
    shape(tile, hero ? 140 : 88, hero ? 35 : 34, 1, hero ? 65 : 56,
          hero ? MENU_CYAN : MENU_BORDER);

    lv_obj_t* title = lv_label_create(tile);
    lv_label_set_text(title, text);
    lv_obj_set_style_text_font(title, hero ? &lv_font_montserrat_36 : &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_set_width(title, hero ? 245 : 84);
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(title, hero ? 165 : 99, hero ? 44 : (target == SCREEN_DIVE_PLANNER ? 42 : 52));

    lv_obj_t* arrow = lv_label_create(tile);
    lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(arrow, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(arrow, lv_color_hex(hero ? MENU_CYAN : MENU_ICON), 0);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -9, 0);
}

}  // namespace

lv_obj_t* home_screen_create(void) {
    ESP_LOGI(TAG, "Creating home screen");
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(MENU_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* status = status_icons_create(screen);
    lv_obj_align(status, LV_ALIGN_TOP_RIGHT, -18, 18);

    constexpr int side = 18;
    constexpr int gap = 12;
    constexpr int small_w = (SCREEN_WIDTH - side * 2 - gap) / 2;
    create_tile(screen, "Analyse", SCREEN_ANALYSE, side, 76, SCREEN_WIDTH - side * 2, 136, true, &menu_icon_analyse);
    create_tile(screen, "Dive\nPlanner", SCREEN_DIVE_PLANNER, side, 226, small_w, 124, false, &menu_icon_dive_planner);
    create_tile(screen, "History", SCREEN_HISTORY, side + small_w + gap, 226, small_w, 124, false, &menu_icon_history);
    create_tile(screen, "Cylinders", SCREEN_CYLINDERS, side, 362, small_w, 124, false, &menu_icon_cylinders);
    create_tile(screen, "Settings", SCREEN_SETTINGS, side + small_w + gap, 362, small_w, 124, false, &menu_icon_settings);
    return screen;
}
