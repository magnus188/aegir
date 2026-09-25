#pragma once
#include "gas_calibration_service.h"

// Acquisition-only bridge. User interface code uses gas_calibration_service.h.
void gas_calibration_publish(gas_cal_channel_t channel, const gas_raw_sample_t &sample);
// Commit an acquisition frame only while its oxygen setup generation is still
// current. Selection/reset and publication share the same configuration lock.
bool gas_calibration_publish_frame(uint32_t oxygen_generation,
    const gas_raw_sample_t samples[GAS_CAL_CHANNEL_COUNT],
    const bool sampled[GAS_CAL_CHANNEL_COUNT], bool reset_history);
void gas_calibration_reset_history(void);
bool gas_calibration_convert(gas_cal_channel_t channel, double voltage, double &percent);
// The returned revision belongs to the exact record used for this conversion.
// A later UI save cannot relabel the returned value with a new record revision.
bool gas_calibration_convert_snapshot(gas_cal_channel_t channel, double voltage,
                                      double &percent, uint32_t &applied_revision);
uint32_t gas_calibration_now_ms(void);
