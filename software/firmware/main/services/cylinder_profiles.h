#pragma once

#include "analysis/analysis_calculator.h"
#include "services/analysis_history.h"
#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CYLINDER_PROFILE_CAPACITY 6

typedef struct {
    bool configured;
    bool needs_recheck;
    char name[24];
    char serial[24];
    float oxygen_percent;
    float helium_percent;
    float planned_depth_m;
    analysis_gas_mode_t gas_mode;
    uint32_t last_analyzed_ms;
} cylinder_profile_t;

void cylinder_profiles_init(void);
uint8_t cylinder_profiles_count(void);
uint8_t cylinder_profiles_selected_index(void);
bool cylinder_profiles_get(uint8_t index, cylinder_profile_t* out);
bool cylinder_profiles_get_selected(cylinder_profile_t* out);
esp_err_t cylinder_profiles_select(uint8_t index);
esp_err_t cylinder_profiles_select_next(void);
esp_err_t cylinder_profiles_set(uint8_t index, const cylinder_profile_t* profile);
esp_err_t cylinder_profiles_update_selected_from_record(const analysis_history_record_t* record);
esp_err_t cylinder_profiles_mark_selected_recheck(bool needs_recheck);
void cylinder_profiles_reset_defaults(void);

#ifdef __cplusplus
}
#endif
