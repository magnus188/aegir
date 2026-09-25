// Existing peripheral fixtures isolate SD; test_sd_log exercises its real core.
#include "services/sd_log_service.h"
bool sd_log_pause(uint32_t) { return true; }
void sd_log_resume() {}
void sd_log_raw(gas_cal_channel_t,const gas_raw_sample_t &,const oxygen_selection_status_t &,uint32_t,uint32_t) {}
