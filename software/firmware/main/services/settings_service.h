#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Settings version - increment when adding/changing settings
#define SETTINGS_VERSION 2

// =============================================================================
// Setting Categories
// =============================================================================
typedef enum {
    SETTINGS_CAT_DEVICE,       // Device settings (display, sound, units)
    SETTINGS_CAT_SAFETY,       // Safety limits and warnings (future)
    SETTINGS_CAT_CALIBRATION,  // Sensor calibration values (future)
    SETTINGS_CAT_COUNT
} settings_category_t;

// =============================================================================
// Setting Keys
// =============================================================================
typedef enum {
    // Device settings
    SETTING_BRIGHTNESS,           // 0-100%
    SETTING_SCREEN_TIMEOUT,       // 0=never, 1=1min, 2=3min, 3=5min
    SETTING_SOUND_ENABLED,        // 0=off, 1=on
    SETTING_UNITS_DEPTH,          // 0=meters, 1=feet
    SETTING_UNITS_TEMP,           // 0=celsius, 1=fahrenheit
    SETTING_UNITS_PRESSURE,       // 0=bar, 1=psi
    SETTING_PPO2_WORKING_X100,    // e.g. 140 = 1.40 bar
    SETTING_PPO2_SECONDARY_X100,  // e.g. 160 = 1.60 bar
    SETTING_DENSITY_ADVISORY_X10, // e.g. 52 = 5.2 g/L
    SETTING_DENSITY_ALARM_X10,    // e.g. 63 = 6.3 g/L
    SETTING_CO2_ADVISORY_PPM,     // e.g. 500 ppm
    
    SETTING_COUNT
} setting_key_t;

// =============================================================================
// Unit enums for readability
// =============================================================================
typedef enum {
    UNITS_DEPTH_METERS = 0,
    UNITS_DEPTH_FEET = 1
} units_depth_t;

typedef enum {
    UNITS_TEMP_CELSIUS = 0,
    UNITS_TEMP_FAHRENHEIT = 1
} units_temp_t;

typedef enum {
    UNITS_PRESSURE_BAR = 0,
    UNITS_PRESSURE_PSI = 1
} units_pressure_t;

// =============================================================================
// Setting metadata
// =============================================================================
typedef struct {
    setting_key_t key;
    settings_category_t category;
    const char* name;           // Human-readable name
    const char* nvs_key;        // NVS storage key (max 15 chars)
    int32_t default_value;      // Default value
    int32_t min_value;          // Minimum allowed value
    int32_t max_value;          // Maximum allowed value
} setting_def_t;

// =============================================================================
// API Functions
// =============================================================================

/**
 * Initialize the settings service
 * Loads saved settings from NVS, applies defaults for missing values
 */
void settings_init(void);

/**
 * Get a setting value
 * @param key Setting key
 * @return Current value (or default if not set)
 */
int32_t settings_get(setting_key_t key);

/**
 * Set a setting value
 * @param key Setting key
 * @param value New value (will be clamped to valid range)
 * @return true if value changed, false if same or invalid
 */
bool settings_set(setting_key_t key, int32_t value);

/**
 * Reset a single setting to its default value
 * @param key Setting key
 */
void settings_reset(setting_key_t key);

/**
 * Reset all settings in a category to defaults
 * @param category Category to reset
 */
void settings_reset_category(settings_category_t category);

/**
 * Reset ALL settings to factory defaults
 */
void settings_factory_reset(void);

/**
 * Get the default value for a setting
 * @param key Setting key
 * @return Default value
 */
int32_t settings_get_default(setting_key_t key);

/**
 * Check if a setting has been modified from default
 * @param key Setting key
 * @return true if different from default
 */
bool settings_is_modified(setting_key_t key);

/**
 * Get setting definition (metadata)
 * @param key Setting key
 * @return Pointer to setting definition, or nullptr if invalid
 */
const setting_def_t* settings_get_def(setting_key_t key);

/**
 * Get category name
 * @param category Category enum
 * @return Human-readable category name
 */
const char* settings_get_category_name(settings_category_t category);

#ifdef __cplusplus
}
#endif
