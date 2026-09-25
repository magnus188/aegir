#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Battery hardware type
typedef enum {
    BATTERY_HW_MOCK = 0,       // Mock/simulation mode
    BATTERY_HW_ADC,            // Direct ADC voltage reading
    BATTERY_HW_MAX17048,       // MAX17048G I2C fuel gauge
    BATTERY_HW_UNAVAILABLE
} battery_hw_type_t;

/**
 * Initialize battery monitoring
 * Call this once during system startup
 */
void battery_service_init(void);
bool battery_is_available(void);

/**
 * Read current battery voltage and calculate percentage
 * @return Battery percentage (0-100)
 */
uint8_t battery_get_percentage(void);

/**
 * Check if battery is currently charging
 * @return true if charging
 */
bool battery_is_charging(void);

/**
 * Get raw battery voltage in millivolts
 * @return Voltage in mV
 */
uint32_t battery_get_voltage_mv(void);

/**
 * Start periodic battery monitoring task
 * Updates the status icons automatically
 */
void battery_start_monitoring(void);

/**
 * Stop battery monitoring task
 */
void battery_stop_monitoring(void);

/**
 * Get detected hardware type
 * @return The battery hardware type in use
 */
battery_hw_type_t battery_get_hw_type(void);

/**
 * Check if fuel gauge (MAX17048G) is available
 * @return true if fuel gauge detected on I2C
 */
bool battery_has_fuel_gauge(void);

/**
 * Get State of Charge from fuel gauge
 * More accurate than voltage-based percentage
 * @return SOC as float (0.0 - 100.0) with 1/256% resolution
 */
float battery_get_soc(void);

/**
 * Get charge/discharge rate from fuel gauge
 * @return Rate in %/hour (positive=charging, negative=discharging)
 */
float battery_get_charge_rate(void);

#ifdef __cplusplus
}
#endif
