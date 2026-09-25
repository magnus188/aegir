#include "dive_planner_screen.h"
#include "gas_calculator.h"
#include "../../styles/styles.h"
#include "../../components/navbar.h"
#include "../screen_manager.h"
#include <esp_log.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static const char* TAG = "DIVE_PLANNER";

namespace {

// Layout constants
constexpr int SCREEN_WIDTH = 480;
constexpr int SCREEN_HEIGHT = 800;
constexpr int NAVBAR_HEIGHT = 70;
constexpr int CONTENT_PAD = 16;
constexpr int SLIDER_ROW_HEIGHT = 90;
constexpr int SLIDER_HEIGHT = 20;
constexpr int CARD_RADIUS = 12;
constexpr int RESULT_CARD_HEIGHT = 100;

// Slider ranges
constexpr int DEPTH_MIN = 0;
constexpr int DEPTH_MAX = 150;
constexpr int PPO2_MIN = 100;  // 1.00 bar * 100
constexpr int PPO2_MAX = 200;  // 2.00 bar * 100
constexpr int O2_MIN = 0;
constexpr int O2_MAX = 100;
constexpr int EAD_MIN = 0;
constexpr int EAD_MAX = 60;
constexpr int HELIUM_MIN = 0;
constexpr int HELIUM_MAX = 100;

// Which parameter can be locked
enum class LockTarget {
    NONE,
    DEPTH,
    PPO2,
    O2,
    EAD,
    HELIUM
};

// Numpad input state
struct NumpadState {
    lv_obj_t* modal = nullptr;
    lv_obj_t* input_label = nullptr;
    lv_obj_t* title_label = nullptr;
    LockTarget editing_target = LockTarget::NONE;
    char input_buffer[16] = {0};
    int input_len = 0;
    float min_val = 0;
    float max_val = 100;
    int decimals = 0;  // 0 for integers, 2 for PPO2
};

// Screen state
struct DivePlannerState {
    lv_obj_t* screen = nullptr;
    
    // Sliders
    lv_obj_t* depth_slider = nullptr;
    lv_obj_t* ppo2_slider = nullptr;
    lv_obj_t* o2_slider = nullptr;
    lv_obj_t* ead_slider = nullptr;
    lv_obj_t* helium_slider = nullptr;
    
    // Value labels
    lv_obj_t* depth_value = nullptr;
    lv_obj_t* ppo2_value = nullptr;
    lv_obj_t* o2_value = nullptr;
    lv_obj_t* ead_value = nullptr;
    lv_obj_t* helium_value = nullptr;
    
    // Lock buttons
    lv_obj_t* depth_lock = nullptr;
    lv_obj_t* ppo2_lock = nullptr;
    lv_obj_t* o2_lock = nullptr;
    lv_obj_t* ead_lock = nullptr;
    lv_obj_t* helium_lock = nullptr;
    
    // Trimix section
    lv_obj_t* trimix_toggle = nullptr;
    lv_obj_t* trimix_section = nullptr;
    
    // Result display
    lv_obj_t* result_o2 = nullptr;
    lv_obj_t* result_mod = nullptr;
    lv_obj_t* result_mix = nullptr;
    lv_obj_t* result_density = nullptr;
    lv_obj_t* blend_source = nullptr;
    lv_obj_t* blend_pressure = nullptr;
    lv_obj_t* blend_additions = nullptr;
    lv_obj_t* blend_status = nullptr;
    
    // Numpad
    NumpadState numpad;
    
    // Current values
    float depth = 30.0f;      // meters
    float ppo2 = 1.40f;       // bar
    float o2 = 21.0f;         // percent
    float ead = 30.0f;        // meters
    float helium = 0.0f;      // percent
    float blend_start_pressure = 50.0f;
    float blend_final_pressure = 200.0f;
    uint8_t blend_source_index = 0;
    
    // Lock state - two independent groups
    // Top group: DEPTH, PPO2, O2 (one lock allowed)
    // Bottom group: EAD, HELIUM (one lock allowed)
    LockTarget top_lock = LockTarget::NONE;
    LockTarget bottom_lock = LockTarget::NONE;
    bool trimix_enabled = false;
    bool updating = false;  // Prevent recursive updates
};

DivePlannerState g_state;

struct SourceMix {
    const char* name;
    float o2;
    float he;
};

constexpr SourceMix SOURCE_MIXES[] = {
    {"Air", 20.9f, 0.0f},
    {"EAN32", 32.0f, 0.0f},
    {"18/45", 18.0f, 45.0f},
};
constexpr uint8_t SOURCE_MIX_COUNT = sizeof(SOURCE_MIXES) / sizeof(SOURCE_MIXES[0]);

// Forward declarations
void update_all_displays();
void update_blend_result();
void recalculate_from_depth();
void recalculate_from_ppo2();
void recalculate_from_ead();
void recalculate_from_helium();
void update_lock_button_styles();
void show_numpad(LockTarget target, const char* title, float current_val, float min_val, float max_val, int decimals);
void close_numpad();
void apply_numpad_value();

// Check if target is in top group (Depth, PPO2, O2)
bool is_top_group(LockTarget target) {
    return target == LockTarget::DEPTH || target == LockTarget::PPO2 || target == LockTarget::O2;
}

// Check if target is in bottom group (EAD, Helium)
bool is_bottom_group(LockTarget target) {
    return target == LockTarget::EAD || target == LockTarget::HELIUM;
}

// Check if a target is currently locked
bool is_locked(LockTarget target) {
    if (is_top_group(target)) {
        return g_state.top_lock == target;
    } else if (is_bottom_group(target)) {
        return g_state.bottom_lock == target;
    }
    return false;
}

// Update value label text
void update_value_label(lv_obj_t* label, const char* format, float value) {
    char buf[32];
    snprintf(buf, sizeof(buf), format, value);
    lv_label_set_text(label, buf);
}

// Set lock button appearance
void set_lock_button_style(lv_obj_t* btn, bool is_locked) {
    if (is_locked) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(STYLE_COLOR_WARNING), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_label_set_text(lv_obj_get_child(btn, 0), LV_SYMBOL_OK);
    } else {
        lv_obj_set_style_bg_color(btn, lv_color_hex(STYLE_COLOR_SURFACE), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_label_set_text(lv_obj_get_child(btn, 0), LV_SYMBOL_MINUS);
    }
}

void update_lock_button_styles() {
    set_lock_button_style(g_state.depth_lock, g_state.top_lock == LockTarget::DEPTH);
    set_lock_button_style(g_state.ppo2_lock, g_state.top_lock == LockTarget::PPO2);
    set_lock_button_style(g_state.o2_lock, g_state.top_lock == LockTarget::O2);
    if (g_state.ead_lock) {
        set_lock_button_style(g_state.ead_lock, g_state.bottom_lock == LockTarget::EAD);
    }
    if (g_state.helium_lock) {
        set_lock_button_style(g_state.helium_lock, g_state.bottom_lock == LockTarget::HELIUM);
    }
}

// Update result card
void update_results() {
    // Calculate MOD based on user's O2 setting
    float mod = calc_mod(g_state.o2, g_state.ppo2);
    
    // Calculate gas density at depth (g/L)
    // Density = (FO2 * 1.429 + FN2 * 1.251 + FHe * 0.179) * P_abs
    // where P_abs = depth/10 + 1 (in bar)
    float p_abs = (g_state.depth / 10.0f) + 1.0f;
    float fo2 = g_state.o2 / 100.0f;
    float fhe = g_state.helium / 100.0f;
    float fn2 = 1.0f - fo2 - fhe;
    if (fn2 < 0) fn2 = 0;
    float density = (fo2 * 1.429f + fn2 * 1.251f + fhe * 0.179f) * p_abs;
    
    // Calculate PPO2 at depth for the current O2%
    float ppo2_at_depth = calc_ppo2(g_state.depth, g_state.o2);
    
    char buf[64];
    snprintf(buf, sizeof(buf), "PPO2: %.2f", ppo2_at_depth);
    lv_label_set_text(g_state.result_o2, buf);
    
    snprintf(buf, sizeof(buf), "MOD: %.0fm", mod);
    lv_label_set_text(g_state.result_mod, buf);
    
    // Show mix recommendation
    if (g_state.trimix_enabled && g_state.helium > 0) {
        float n2 = 100.0f - g_state.o2 - g_state.helium;
        if (n2 < 0) n2 = 0;
        snprintf(buf, sizeof(buf), "Mix: %.0f/%.0f (O2/He)", g_state.o2, g_state.helium);
    } else {
        if (g_state.o2 > 21.5f) {
            snprintf(buf, sizeof(buf), "EAN%.0f (Nitrox)", g_state.o2);
        } else {
            snprintf(buf, sizeof(buf), "Air (21%% O2)");
        }
    }
    lv_label_set_text(g_state.result_mix, buf);
    
    // Update density with color warning
    snprintf(buf, sizeof(buf), "%.1f g/L", density);
    lv_label_set_text(g_state.result_density, buf);
    // Color code: green < 5.2, yellow 5.2-6.0, red > 6.0
    if (density > 6.0f) {
        lv_obj_set_style_text_color(g_state.result_density, lv_color_hex(STYLE_COLOR_ERROR), 0);
    } else if (density > 5.2f) {
        lv_obj_set_style_text_color(g_state.result_density, lv_color_hex(STYLE_COLOR_WARNING), 0);
    } else {
        lv_obj_set_style_text_color(g_state.result_density, lv_color_hex(STYLE_COLOR_SUCCESS), 0);
    }
}

void update_blend_result() {
    if (!g_state.blend_source || !g_state.blend_pressure ||
        !g_state.blend_additions || !g_state.blend_status) {
        return;
    }

    const SourceMix& source = SOURCE_MIXES[g_state.blend_source_index % SOURCE_MIX_COUNT];
    char buf[128];
    snprintf(buf, sizeof(buf), "Source %s: O2 %.1f%% He %.0f%%",
             source.name, source.o2, source.he);
    lv_label_set_text(g_state.blend_source, buf);

    snprintf(buf, sizeof(buf), "Top-up %.0f -> %.0f bar toward O2 %.0f%% He %.0f%%",
             g_state.blend_start_pressure, g_state.blend_final_pressure,
             g_state.o2, g_state.trimix_enabled ? g_state.helium : 0.0f);
    lv_label_set_text(g_state.blend_pressure, buf);

    blend_topup_result_t plan = calc_blend_topup(g_state.blend_start_pressure,
                                                 g_state.blend_final_pressure,
                                                 source.o2,
                                                 source.he,
                                                 g_state.o2,
                                                 g_state.trimix_enabled ? g_state.helium : 0.0f);
    if (plan.valid) {
        snprintf(buf, sizeof(buf), "Add O2 %.0fb  He %.0fb  Air %.0fb",
                 plan.oxygen_add_bar, plan.helium_add_bar, plan.air_add_bar);
        lv_label_set_text(g_state.blend_additions, buf);
        lv_label_set_text(g_state.blend_status, plan.status);
        lv_obj_set_style_text_color(g_state.blend_status, lv_color_hex(STYLE_COLOR_SUCCESS), 0);
    } else {
        lv_label_set_text(g_state.blend_additions, "--");
        lv_label_set_text(g_state.blend_status, plan.status);
        lv_obj_set_style_text_color(g_state.blend_status, lv_color_hex(STYLE_COLOR_WARNING), 0);
    }
}

void update_all_displays() {
    update_value_label(g_state.depth_value, "%.0f m", g_state.depth);
    update_value_label(g_state.ppo2_value, "%.2f bar", g_state.ppo2);
    update_value_label(g_state.o2_value, "%.0f %%", g_state.o2);
    
    if (g_state.ead_value) {
        update_value_label(g_state.ead_value, "%.0f m", g_state.ead);
    }
    if (g_state.helium_value) {
        update_value_label(g_state.helium_value, "%.0f %%", g_state.helium);
    }
    
    // Update slider positions
    lv_slider_set_value(g_state.depth_slider, (int)g_state.depth, LV_ANIM_OFF);
    lv_slider_set_value(g_state.ppo2_slider, (int)(g_state.ppo2 * 100), LV_ANIM_OFF);
    lv_slider_set_value(g_state.o2_slider, (int)g_state.o2, LV_ANIM_OFF);
    
    if (g_state.ead_slider) {
        lv_slider_set_value(g_state.ead_slider, (int)g_state.ead, LV_ANIM_OFF);
    }
    if (g_state.helium_slider) {
        lv_slider_set_value(g_state.helium_slider, (int)g_state.helium, LV_ANIM_OFF);
    }
    
    update_results();
    update_blend_result();
}

// Recalculation functions based on which slider changed
void recalculate_from_depth() {
    if (g_state.updating) return;
    g_state.updating = true;
    
    // Handle top group lock - if PPO2 is locked, adjust O2 to maintain PPO2
    if (g_state.top_lock == LockTarget::PPO2) {
        g_state.o2 = calc_o2_for_depth_ppo2(g_state.depth, g_state.ppo2);
        g_state.o2 = clamp_float(g_state.o2, O2_MIN, O2_MAX);
    }
    
    if (g_state.trimix_enabled) {
        switch (g_state.bottom_lock) {
            case LockTarget::EAD:
                // Depth changed, EAD locked -> adjust helium
                g_state.helium = calc_helium_for_ead(g_state.depth, g_state.ead);
                break;
            case LockTarget::HELIUM:
                // Depth changed, He locked -> adjust EAD
                g_state.ead = calc_ead(g_state.depth, g_state.helium);
                break;
            default:
                // No trimix lock, just update EAD
                g_state.ead = calc_ead(g_state.depth, g_state.helium);
                break;
        }
    }
    
    update_all_displays();
    g_state.updating = false;
}

void recalculate_from_ppo2() {
    if (g_state.updating) return;
    g_state.updating = true;
    
    // PPO2 change affects recommended O2%, shown in results
    update_all_displays();
    g_state.updating = false;
}

void recalculate_from_ead() {
    if (g_state.updating) return;
    g_state.updating = true;
    
    // Check bottom lock for trimix calculations
    switch (g_state.bottom_lock) {
        case LockTarget::HELIUM:
            // EAD changed, helium locked -> adjust depth
            g_state.depth = calc_depth_for_ead(g_state.ead, g_state.helium);
            g_state.depth = clamp_float(g_state.depth, DEPTH_MIN, DEPTH_MAX);
            break;
        default:
            // No helium lock, adjust helium by default
            g_state.helium = calc_helium_for_ead(g_state.depth, g_state.ead);
            break;
    }
    
    update_all_displays();
    g_state.updating = false;
}

void recalculate_from_helium() {
    if (g_state.updating) return;
    g_state.updating = true;
    
    // Check bottom lock for trimix calculations
    switch (g_state.bottom_lock) {
        case LockTarget::EAD:
            // Helium changed, EAD locked -> adjust depth
            g_state.depth = calc_depth_for_ead(g_state.ead, g_state.helium);
            g_state.depth = clamp_float(g_state.depth, DEPTH_MIN, DEPTH_MAX);
            break;
        default:
            // No EAD lock, adjust EAD by default
            g_state.ead = calc_ead(g_state.depth, g_state.helium);
            break;
    }
    
    update_all_displays();
    g_state.updating = false;
}

// Event handlers
void depth_slider_event_cb(lv_event_t* e) {
    if (is_locked(LockTarget::DEPTH)) return;  // Can't change locked slider
    
    g_state.depth = (float)lv_slider_get_value(g_state.depth_slider);
    recalculate_from_depth();
}

void ppo2_slider_event_cb(lv_event_t* e) {
    if (is_locked(LockTarget::PPO2)) return;
    
    g_state.ppo2 = lv_slider_get_value(g_state.ppo2_slider) / 100.0f;
    recalculate_from_ppo2();
}

void ead_slider_event_cb(lv_event_t* e) {
    if (is_locked(LockTarget::EAD)) return;
    
    g_state.ead = (float)lv_slider_get_value(g_state.ead_slider);
    recalculate_from_ead();
}

void helium_slider_event_cb(lv_event_t* e) {
    if (is_locked(LockTarget::HELIUM)) return;
    
    g_state.helium = (float)lv_slider_get_value(g_state.helium_slider);
    recalculate_from_helium();
}

void o2_slider_event_cb(lv_event_t* e) {
    if (is_locked(LockTarget::O2)) return;
    
    g_state.o2 = (float)lv_slider_get_value(g_state.o2_slider);
    // O2 affects density calculation, just update displays
    update_all_displays();
}

void lock_button_event_cb(lv_event_t* e) {
    LockTarget target = (LockTarget)(intptr_t)lv_event_get_user_data(e);
    
    // Determine which group this lock belongs to
    // Pressing a lock button always locks that target (auto-unlocks any other in same group)
    // Press again on already-locked target to unlock
    if (is_top_group(target)) {
        // Top group (Depth, PPO2, O2) - auto-switch: lock new target directly
        if (g_state.top_lock == target) {
            // Already locked - unlock it
            g_state.top_lock = LockTarget::NONE;
        } else {
            // Lock this one (automatically unlocks any previous in this group)
            g_state.top_lock = target;
        }
        ESP_LOGI(TAG, "Top lock changed to: %d", (int)g_state.top_lock);
    } else if (is_bottom_group(target)) {
        // Bottom group (EAD, Helium) - auto-switch: lock new target directly
        if (g_state.bottom_lock == target) {
            // Already locked - unlock it
            g_state.bottom_lock = LockTarget::NONE;
        } else {
            // Lock this one (automatically unlocks any previous in this group)
            g_state.bottom_lock = target;
        }
        ESP_LOGI(TAG, "Bottom lock changed to: %d", (int)g_state.bottom_lock);
    }
    
    update_lock_button_styles();
}

// Numpad button map
static const char* numpad_map[] = {
    "7", "8", "9", "\n",
    "4", "5", "6", "\n",
    "1", "2", "3", "\n",
    ",", "0", ".", "\n",
    LV_SYMBOL_BACKSPACE, "OK", ""
};

void numpad_button_event_cb(lv_event_t* e) {
    lv_obj_t* btnm = (lv_obj_t*)lv_event_get_target(e);
    uint32_t id = lv_buttonmatrix_get_selected_button(btnm);
    const char* txt = lv_buttonmatrix_get_button_text(btnm, id);
    
    if (txt == nullptr) return;
    
    NumpadState& np = g_state.numpad;
    
    if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
        // Backspace
        if (np.input_len > 0) {
            np.input_len--;
            np.input_buffer[np.input_len] = '\0';
        }
    } else if (strcmp(txt, "OK") == 0) {
        // Apply value and close
        apply_numpad_value();
        close_numpad();
        return;
    } else if (strcmp(txt, ",") == 0 || strcmp(txt, ".") == 0) {
        // Decimal point - only allow one, and only if decimals are allowed
        if (np.decimals > 0 && np.input_len < 14) {
            // Check if there's already a decimal point
            bool has_decimal = false;
            for (int i = 0; i < np.input_len; i++) {
                if (np.input_buffer[i] == '.') {
                    has_decimal = true;
                    break;
                }
            }
            if (!has_decimal) {
                np.input_buffer[np.input_len++] = '.';
                np.input_buffer[np.input_len] = '\0';
            }
        }
    } else {
        // Digit
        if (np.input_len < 14) {
            np.input_buffer[np.input_len++] = txt[0];
            np.input_buffer[np.input_len] = '\0';
        }
    }
    
    // Update display
    if (np.input_len > 0) {
        lv_label_set_text(np.input_label, np.input_buffer);
    } else {
        lv_label_set_text(np.input_label, "0");
    }
}

void numpad_cancel_event_cb(lv_event_t* e) {
    close_numpad();
}

void apply_numpad_value() {
    NumpadState& np = g_state.numpad;
    
    if (np.input_len == 0) return;
    
    // Parse the value
    float value = strtof(np.input_buffer, nullptr);
    value = clamp_float(value, np.min_val, np.max_val);
    
    // Apply to the appropriate state variable
    switch (np.editing_target) {
        case LockTarget::DEPTH:
            g_state.depth = value;
            recalculate_from_depth();
            break;
        case LockTarget::PPO2:
            g_state.ppo2 = value;
            recalculate_from_ppo2();
            break;
        case LockTarget::O2:
            g_state.o2 = value;
            update_all_displays();
            break;
        case LockTarget::EAD:
            g_state.ead = value;
            recalculate_from_ead();
            break;
        case LockTarget::HELIUM:
            g_state.helium = value;
            recalculate_from_helium();
            break;
        default:
            break;
    }
}

void close_numpad() {
    if (g_state.numpad.modal) {
        lv_obj_delete(g_state.numpad.modal);
        g_state.numpad.modal = nullptr;
        g_state.numpad.input_label = nullptr;
        g_state.numpad.title_label = nullptr;
        g_state.numpad.editing_target = LockTarget::NONE;
    }
}

void show_numpad(LockTarget target, const char* title, float current_val, float min_val, float max_val, int decimals) {
    // Close any existing numpad
    close_numpad();
    
    NumpadState& np = g_state.numpad;
    np.editing_target = target;
    np.min_val = min_val;
    np.max_val = max_val;
    np.decimals = decimals;
    np.input_len = 0;
    np.input_buffer[0] = '\0';
    
    // Create modal backdrop
    np.modal = lv_obj_create(g_state.screen);
    lv_obj_set_size(np.modal, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(np.modal, 0, 0);
    lv_obj_set_style_bg_color(np.modal, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(np.modal, LV_OPA_70, 0);
    lv_obj_set_style_border_width(np.modal, 0, 0);
    lv_obj_set_style_radius(np.modal, 0, 0);
    lv_obj_clear_flag(np.modal, LV_OBJ_FLAG_SCROLLABLE);
    
    // Dialog container - full width for better touch usability
    lv_obj_t* dialog = lv_obj_create(np.modal);
    lv_obj_set_size(dialog, 460, 700);
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, lv_color_hex(STYLE_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(dialog, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dialog, 24, 0);
    lv_obj_set_style_border_width(dialog, 0, 0);
    lv_obj_set_style_pad_all(dialog, 20, 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    np.title_label = lv_label_create(dialog);
    lv_label_set_text(np.title_label, title);
    lv_obj_set_style_text_font(np.title_label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(np.title_label, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(np.title_label, LV_ALIGN_TOP_MID, 0, 0);

    // Current value display
    np.input_label = lv_label_create(dialog);
    char buf[32];
    if (decimals > 0) {
        snprintf(buf, sizeof(buf), "%.2f", current_val);
    } else {
        snprintf(buf, sizeof(buf), "%.0f", current_val);
    }
    lv_label_set_text(np.input_label, buf);
    lv_obj_set_style_text_font(np.input_label, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(np.input_label, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(np.input_label, LV_ALIGN_TOP_MID, 0, 50);

    // Range hint
    lv_obj_t* range_label = lv_label_create(dialog);
    if (decimals > 0) {
        snprintf(buf, sizeof(buf), "(%.2f - %.2f)", min_val, max_val);
    } else {
        snprintf(buf, sizeof(buf), "(%.0f - %.0f)", min_val, max_val);
    }
    lv_label_set_text(range_label, buf);
    lv_obj_set_style_text_font(range_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(range_label, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(range_label, LV_ALIGN_TOP_MID, 0, 90);

    // Numpad button matrix - large touch-friendly buttons
    lv_obj_t* btnm = lv_buttonmatrix_create(dialog);
    lv_buttonmatrix_set_map(btnm, numpad_map);
    lv_obj_set_size(btnm, 420, 420);
    lv_obj_align(btnm, LV_ALIGN_TOP_MID, 0, 130);

    // Style the button matrix
    lv_obj_set_style_bg_color(btnm, lv_color_hex(STYLE_COLOR_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btnm, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btnm, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btnm, 8, LV_PART_MAIN);

    lv_obj_set_style_bg_color(btnm, lv_color_hex(0x404040), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(btnm, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(btnm, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), LV_PART_ITEMS);
    lv_obj_set_style_text_font(btnm, &lv_font_montserrat_32, LV_PART_ITEMS);
    lv_obj_set_style_radius(btnm, 16, LV_PART_ITEMS);

    lv_obj_add_event_cb(btnm, numpad_button_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // Cancel button - large touch target
    lv_obj_t* cancel_btn = lv_btn_create(dialog);
    lv_obj_set_size(cancel_btn, 180, 56);
    lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(STYLE_COLOR_ERROR), 0);
    lv_obj_set_style_radius(cancel_btn, 12, 0);

    lv_obj_t* cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_22, 0);
    lv_obj_center(cancel_label);

    lv_obj_add_event_cb(cancel_btn, numpad_cancel_event_cb, LV_EVENT_CLICKED, nullptr);

    ESP_LOGI(TAG, "Numpad opened for %s", title);
}

// Value label tap handlers
void value_label_click_cb(lv_event_t* e) {
    LockTarget target = (LockTarget)(intptr_t)lv_event_get_user_data(e);
    
    switch (target) {
        case LockTarget::DEPTH:
            show_numpad(target, "Depth (m)", g_state.depth, DEPTH_MIN, DEPTH_MAX, 0);
            break;
        case LockTarget::PPO2:
            show_numpad(target, "PPO2 (bar)", g_state.ppo2, PPO2_MIN / 100.0f, PPO2_MAX / 100.0f, 2);
            break;
        case LockTarget::O2:
            show_numpad(target, "O2 (%)", g_state.o2, O2_MIN, O2_MAX, 0);
            break;
        case LockTarget::EAD:
            show_numpad(target, "EAD (m)", g_state.ead, EAD_MIN, EAD_MAX, 0);
            break;
        case LockTarget::HELIUM:
            show_numpad(target, "Helium (%)", g_state.helium, HELIUM_MIN, HELIUM_MAX, 0);
            break;
        default:
            break;
    }
}

void trimix_toggle_event_cb(lv_event_t* e) {
    g_state.trimix_enabled = lv_obj_has_state(g_state.trimix_toggle, LV_STATE_CHECKED);
    
    if (g_state.trimix_section) {
        if (g_state.trimix_enabled) {
            lv_obj_clear_flag(g_state.trimix_section, LV_OBJ_FLAG_HIDDEN);
            // Keep any existing bottom lock
        } else {
            lv_obj_add_flag(g_state.trimix_section, LV_OBJ_FLAG_HIDDEN);
            // Clear bottom lock when disabling trimix
            g_state.bottom_lock = LockTarget::NONE;
            update_lock_button_styles();
            g_state.helium = 0;
            g_state.ead = g_state.depth;  // EAD equals depth with no helium
        }
    }
    
    update_all_displays();
    ESP_LOGI(TAG, "Trimix mode: %s", g_state.trimix_enabled ? "enabled" : "disabled");
}

void blend_source_cb(lv_event_t*) {
    g_state.blend_source_index = (g_state.blend_source_index + 1U) % SOURCE_MIX_COUNT;
    update_blend_result();
}

void blend_pressure_cb(lv_event_t* e) {
    intptr_t packed = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    bool final_pressure = packed > 1000;
    int delta = static_cast<int>(final_pressure ? packed - 1000 : packed);
    if (final_pressure) {
        g_state.blend_final_pressure += static_cast<float>(delta);
        g_state.blend_final_pressure = clamp_float(g_state.blend_final_pressure, 20.0f, 300.0f);
        if (g_state.blend_final_pressure <= g_state.blend_start_pressure) {
            g_state.blend_final_pressure = g_state.blend_start_pressure + 10.0f;
        }
    } else {
        g_state.blend_start_pressure += static_cast<float>(delta);
        g_state.blend_start_pressure = clamp_float(g_state.blend_start_pressure, 0.0f, 290.0f);
        if (g_state.blend_start_pressure >= g_state.blend_final_pressure) {
            g_state.blend_start_pressure = g_state.blend_final_pressure - 10.0f;
        }
    }
    update_blend_result();
}

// Create a slider row with label, value, slider, and lock button
lv_obj_t* create_slider_row(lv_obj_t* parent, const char* label_text, 
                            int min_val, int max_val, int initial_val,
                            lv_obj_t** out_slider, lv_obj_t** out_value, lv_obj_t** out_lock,
                            lv_event_cb_t slider_cb, LockTarget lock_target) {
    // Card container
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, SCREEN_WIDTH - 2 * CONTENT_PAD, SLIDER_ROW_HEIGHT);
    lv_obj_set_style_bg_color(card, lv_color_hex(STYLE_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, CARD_RADIUS, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    
    // Top row: Label + Value + Lock button
    lv_obj_t* top_row = lv_obj_create(card);
    lv_obj_remove_style_all(top_row);
    lv_obj_set_size(top_row, lv_pct(100), 30);
    lv_obj_set_flex_flow(top_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(top_row, LV_ALIGN_TOP_MID, 0, 0);
    
    // Label
    lv_obj_t* label = lv_label_create(top_row);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    
    // Value label (clickable for numpad input)
    lv_obj_t* value = lv_label_create(top_row);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(value, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_add_flag(value, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(value, value_label_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)lock_target);
    *out_value = value;
    
    // Lock button
    lv_obj_t* lock_btn = lv_btn_create(top_row);
    lv_obj_set_size(lock_btn, 36, 28);
    lv_obj_set_style_bg_color(lock_btn, lv_color_hex(STYLE_COLOR_SURFACE), 0);
    lv_obj_set_style_radius(lock_btn, 6, 0);
    lv_obj_set_style_border_width(lock_btn, 1, 0);
    lv_obj_set_style_border_color(lock_btn, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_set_style_shadow_width(lock_btn, 0, 0);
    
    lv_obj_t* lock_icon = lv_label_create(lock_btn);
    lv_label_set_text(lock_icon, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_font(lock_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lock_icon, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_center(lock_icon);
    
    lv_obj_add_event_cb(lock_btn, lock_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)lock_target);
    *out_lock = lock_btn;
    
    // Slider
    lv_obj_t* slider = lv_slider_create(card);
    lv_obj_set_size(slider, lv_pct(100), SLIDER_HEIGHT);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_slider_set_range(slider, min_val, max_val);
    lv_slider_set_value(slider, initial_val, LV_ANIM_OFF);
    
    // Slider styling
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, SLIDER_HEIGHT / 2, LV_PART_MAIN);
    
    lv_obj_set_style_bg_color(slider, lv_color_hex(STYLE_COLOR_DIVE_PLAN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, SLIDER_HEIGHT / 2, LV_PART_INDICATOR);
    
    lv_obj_set_style_bg_color(slider, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 6, LV_PART_KNOB);  // Knob size
    
    lv_obj_add_event_cb(slider, slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    *out_slider = slider;
    
    return card;
}

// Create trimix toggle row
lv_obj_t* create_trimix_toggle(lv_obj_t* parent) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, SCREEN_WIDTH - 2 * CONTENT_PAD, 60);
    lv_obj_set_style_bg_color(card, lv_color_hex(STYLE_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, CARD_RADIUS, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_hor(card, 16, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    
    // Label
    lv_obj_t* label = lv_label_create(card);
    lv_label_set_text(label, "Enable Trimix");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
    
    // Toggle switch
    lv_obj_t* sw = lv_switch_create(card);
    lv_obj_set_size(sw, 50, 26);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
    
    // Switch styling
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(STYLE_COLOR_DIVE_PLAN), LV_PART_INDICATOR);
    lv_obj_add_state(sw, LV_STATE_DEFAULT);  // Set default state style
    lv_obj_set_style_bg_color(sw, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), LV_PART_KNOB);
    
    lv_obj_add_event_cb(sw, trimix_toggle_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    g_state.trimix_toggle = sw;
    
    return card;
}

// Create result card
lv_obj_t* create_result_card(lv_obj_t* parent) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, SCREEN_WIDTH - 2 * CONTENT_PAD, RESULT_CARD_HEIGHT);
    lv_obj_set_style_bg_color(card, lv_color_hex(STYLE_COLOR_PRIMARY), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, CARD_RADIUS, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // O2 result
    g_state.result_o2 = lv_label_create(card);
    lv_obj_set_style_text_font(g_state.result_o2, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g_state.result_o2, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    
    // MOD result
    g_state.result_mod = lv_label_create(card);
    lv_obj_set_style_text_font(g_state.result_mod, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g_state.result_mod, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    
    // Mix recommendation (full width below)
    g_state.result_mix = lv_label_create(card);
    lv_obj_set_style_text_font(g_state.result_mix, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(g_state.result_mix, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    
    // Gas density
    g_state.result_density = lv_label_create(card);
    lv_obj_set_style_text_font(g_state.result_density, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(g_state.result_density, lv_color_hex(STYLE_COLOR_SUCCESS), 0);
    
    return card;
}

lv_obj_t* create_blend_button(lv_obj_t* parent, const char* text, int x, int y, int w,
                              lv_event_cb_t cb, void* data) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, 34);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(STYLE_COLOR_BG_CARD), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(STYLE_COLOR_DIVE_PLAN), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, data);
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);
    return btn;
}

lv_obj_t* create_blend_card(lv_obj_t* parent) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, SCREEN_WIDTH - 2 * CONTENT_PAD, 178);
    lv_obj_set_style_bg_color(card, lv_color_hex(STYLE_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, CARD_RADIUS, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "Blend top-up");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    g_state.blend_source = lv_label_create(card);
    lv_label_set_text(g_state.blend_source, "--");
    lv_obj_set_style_text_font(g_state.blend_source, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_state.blend_source, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(g_state.blend_source, LV_ALIGN_TOP_LEFT, 0, 30);

    g_state.blend_pressure = lv_label_create(card);
    lv_label_set_text(g_state.blend_pressure, "--");
    lv_obj_set_style_text_font(g_state.blend_pressure, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_state.blend_pressure, lv_color_hex(STYLE_COLOR_TEXT_DIM), 0);
    lv_obj_align(g_state.blend_pressure, LV_ALIGN_TOP_LEFT, 0, 54);

    g_state.blend_additions = lv_label_create(card);
    lv_label_set_text(g_state.blend_additions, "--");
    lv_obj_set_style_text_font(g_state.blend_additions, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(g_state.blend_additions, lv_color_hex(STYLE_COLOR_TEXT_LIGHT), 0);
    lv_obj_align(g_state.blend_additions, LV_ALIGN_TOP_LEFT, 0, 82);

    g_state.blend_status = lv_label_create(card);
    lv_label_set_text(g_state.blend_status, "--");
    lv_obj_set_style_text_font(g_state.blend_status, &lv_font_montserrat_14, 0);
    lv_obj_set_width(g_state.blend_status, SCREEN_WIDTH - 2 * CONTENT_PAD - 24);
    lv_label_set_long_mode(g_state.blend_status, LV_LABEL_LONG_DOT);
    lv_obj_align(g_state.blend_status, LV_ALIGN_TOP_LEFT, 0, 108);

    create_blend_button(card, "Source", 0, 132, 84, blend_source_cb, nullptr);
    create_blend_button(card, "Start -10", 92, 132, 92, blend_pressure_cb,
                        reinterpret_cast<void*>(static_cast<intptr_t>(-10)));
    create_blend_button(card, "Start +10", 192, 132, 92, blend_pressure_cb,
                        reinterpret_cast<void*>(static_cast<intptr_t>(10)));
    create_blend_button(card, "Final +10", 292, 132, 100, blend_pressure_cb,
                        reinterpret_cast<void*>(static_cast<intptr_t>(1010)));

    return card;
}

}  // namespace

lv_obj_t* dive_planner_screen_create(void) {
    ESP_LOGI(TAG, "Creating dive planner screen");
    
    // Reset state
    g_state = DivePlannerState{};
    
    // Create screen
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(STYLE_COLOR_BG_DARK), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    g_state.screen = screen;
    
    // Navbar with back button
    navbar_create_with_back(screen, "Dive Planner", nullptr);
    
    // Content area - disable all scrolling to prevent unwanted scroll behavior
    lv_obj_t* content = lv_obj_create(screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, SCREEN_WIDTH, SCREEN_HEIGHT - NAVBAR_HEIGHT);
    lv_obj_set_pos(content, 0, NAVBAR_HEIGHT);
    lv_obj_set_style_pad_all(content, CONTENT_PAD, 0);
    lv_obj_set_style_pad_row(content, 12, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    
    // Result card at top
    create_result_card(content);
    
    // Depth slider
    create_slider_row(content, "Depth", DEPTH_MIN, DEPTH_MAX, (int)g_state.depth,
                      &g_state.depth_slider, &g_state.depth_value, &g_state.depth_lock,
                      depth_slider_event_cb, LockTarget::DEPTH);
    
    // PPO2 slider
    create_slider_row(content, "PPO2", PPO2_MIN, PPO2_MAX, (int)(g_state.ppo2 * 100),
                      &g_state.ppo2_slider, &g_state.ppo2_value, &g_state.ppo2_lock,
                      ppo2_slider_event_cb, LockTarget::PPO2);
    
    // O2 slider
    create_slider_row(content, "O2", O2_MIN, O2_MAX, (int)g_state.o2,
                      &g_state.o2_slider, &g_state.o2_value, &g_state.o2_lock,
                      o2_slider_event_cb, LockTarget::O2);
    
    // Trimix toggle
    create_trimix_toggle(content);
    
    // Trimix section (hidden by default)
    g_state.trimix_section = lv_obj_create(content);
    lv_obj_remove_style_all(g_state.trimix_section);
    lv_obj_set_size(g_state.trimix_section, SCREEN_WIDTH - 2 * CONTENT_PAD, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(g_state.trimix_section, 12, 0);
    lv_obj_set_flex_flow(g_state.trimix_section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_state.trimix_section, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(g_state.trimix_section, LV_OBJ_FLAG_HIDDEN);  // Hidden initially
    
    // EAD slider (in trimix section)
    create_slider_row(g_state.trimix_section, "EAD (Target)", EAD_MIN, EAD_MAX, (int)g_state.ead,
                      &g_state.ead_slider, &g_state.ead_value, &g_state.ead_lock,
                      ead_slider_event_cb, LockTarget::EAD);
    
    // Helium slider (in trimix section)
    create_slider_row(g_state.trimix_section, "Helium", HELIUM_MIN, HELIUM_MAX, (int)g_state.helium,
                      &g_state.helium_slider, &g_state.helium_value, &g_state.helium_lock,
                      helium_slider_event_cb, LockTarget::HELIUM);

    create_blend_card(content);
    
    // Initial display update
    update_all_displays();
    update_lock_button_styles();
    
    ESP_LOGI(TAG, "Dive planner screen created");
    return screen;
}
