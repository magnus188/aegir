#pragma once
#include "gas_calibration_service.h"

#ifdef __cplusplus
extern "C" {
#endif
// Configuration identity is deliberately separate from persisted calibration IDs.
typedef enum { OXYGEN_UNCONFIGURED = 0, OXYGEN_AO2 = 1, OXYGEN_JJCCR = 2 } oxygen_selection_t;
typedef struct {
    oxygen_selection_t choice;
    bool configured;
    bool calibration_required;
    bool storage_error;
    bool simulated;
    uint32_t generation;
    uint32_t profile_revision;
    uint32_t minimum_calibration_revision;
} oxygen_selection_status_t;
typedef enum {
    OXYGEN_SELECT_OK = 0, OXYGEN_SELECT_BAD_CHOICE, OXYGEN_SELECT_STORAGE_ERROR
} oxygen_selection_result_t;
// Loading never picks a default physical sensor. First setup needs explicit confirmation.
void oxygen_selection_get_status(oxygen_selection_status_t *out);
bool oxygen_selection_selected_channel(gas_cal_channel_t *out);
bool oxygen_selection_calibration_usable(gas_cal_channel_t channel, uint32_t calibration_revision);
bool oxygen_selection_channel_enabled(gas_cal_channel_t channel);
// Replacement requires a new calibration even when the selected type is unchanged.
oxygen_selection_result_t oxygen_selection_confirm(oxygen_selection_t choice, bool replacement);
const char *oxygen_selection_label(oxygen_selection_t choice);
// The wizard enables both input measurements temporarily; never used to select
// a channel automatically. Disabling restores ordinary selected-input sampling.
void oxygen_selection_set_probe_active(bool active);
bool oxygen_selection_probe_active(void);
// Known-air evidence can guide manual input selection, never identify sensor type.
typedef enum {
    OXYGEN_AIR_CONFIRM_REFERENCE = 0, OXYGEN_AIR_WAIT,
    OXYGEN_AIR_CHECK_AO2_INPUT, OXYGEN_AIR_CHECK_JJ_INPUT, OXYGEN_AIR_AMBIGUOUS
} oxygen_air_advice_t;
oxygen_air_advice_t oxygen_selection_air_advice(bool known_air_confirmed);
const char *oxygen_selection_air_advice_label(oxygen_air_advice_t advice);
#ifdef __cplusplus
}
#endif
