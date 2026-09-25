#pragma once
#include "sd_log_service.h"
namespace sd_log {
bool raw(char *row,size_t size,uint64_t elapsed,gas_cal_channel_t channel,const gas_raw_sample_t &sample,const oxygen_selection_status_t &selection,uint32_t calibration,uint32_t acquisition);
bool result(char *row,size_t size,uint64_t elapsed,const sensor_readings_t &readings,const analysis_result_t *analysis);
}
