#include "splash_screen.h"

#include "../../images/aegir_splash_images.h"
#include "../../styles/styles.h"

lv_obj_t* splash_screen_create(void) {
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(STYLE_COLOR_BG_DARK), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // These alpha-only images share one white tint and keep their source
    // artwork independent of the device's 480 x 800 color palette.
    lv_obj_t* mark = lv_image_create(screen);
    lv_image_set_src(mark, &aegir_splash_mark);
    lv_obj_set_style_image_recolor(mark, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_set_style_image_recolor_opa(mark, LV_OPA_COVER, 0);
    lv_obj_align(mark, LV_ALIGN_TOP_MID, 0, 166);

    lv_obj_t* wordmark = lv_image_create(screen);
    lv_image_set_src(wordmark, &aegir_splash_wordmark);
    lv_obj_set_style_image_recolor(wordmark, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_set_style_image_recolor_opa(wordmark, LV_OPA_COVER, 0);
    lv_obj_align(wordmark, LV_ALIGN_TOP_MID, 0, 493);

    return screen;
}
