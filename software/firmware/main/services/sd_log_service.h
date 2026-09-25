#pragma once
#include "sd_log_core.h"
#include "sensors/sensor_interface.h"
#include "analysis/analysis_calculator.h"
#include "gas_calibration_service.h"
void sd_log_start();
void sd_log_mode(sd_log::Mode mode);
void sd_log_end_mode(sd_log::Mode expected);
void sd_log_raw(gas_cal_channel_t channel,const gas_raw_sample_t &sample,
                const oxygen_selection_status_t &selection,uint32_t calibration_revision,uint32_t acquisition_revision);
void sd_log_result(const sensor_readings_t &readings,const analysis_result_t *analysis=nullptr);
void sd_log_calibration_event(const char *event,gas_cal_channel_t channel,gas_cal_result_t result,
                              const gas_cal_reference_t *reference=nullptr);
sd_log::Status sd_log_status();
bool sd_log_pause(uint32_t timeout_ms);
void sd_log_resume();
void sd_log_eject();
void sd_log_retry();
