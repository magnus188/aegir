#include "sd_log_format.h"
#include <cstdio>
#include <cmath>
namespace sd_log {
bool raw(char *row,size_t size,uint64_t elapsed,gas_cal_channel_t channel,const gas_raw_sample_t &r,
                const oxygen_selection_status_t &selection,uint32_t calibration,uint32_t acquisition) {
    const int length=std::snprintf(row,size,"%llu,%lu,%d,%d,%lu,%lu,%ld,%.9f,%u,%.6f,%.6f,%.4f,%.4f,%.6f,%u,%lu,%lu,%u,%lu",
        static_cast<unsigned long long>(elapsed),static_cast<unsigned long>(r.timestamp_ms),int(channel),int(selection.choice),
        static_cast<unsigned long>(r.sequence),static_cast<unsigned long>(selection.generation),static_cast<long>(r.adc_code),
        r.voltage_v,unsigned(r.gain),r.excitation_v,r.bias_v,r.temperature_c,r.humidity_pct,r.pressure_bar,unsigned(r.environment_valid),
        static_cast<unsigned long>(calibration),static_cast<unsigned long>(r.faults),unsigned(r.warmed),static_cast<unsigned long>(acquisition));
    return length>=0 && size_t(length)<size;
}
bool result(char *row,size_t size,uint64_t elapsed,const sensor_readings_t &r,const analysis_result_t *a) {
    const int length=std::snprintf(row,size,"%llu,%lu,%lu,%d,%d,%lu,%lu,%d,%u,%.6f,%.6f,%.6f,%.6f,%.6f,%.3f,%u,%.3f,%.3f,%.6f,%u,%u,%u,%u,%d,%.3f,%.3f,%d,%lu",
        static_cast<unsigned long long>(elapsed),static_cast<unsigned long>(r.timestamp_ms),static_cast<unsigned long>(r.sequence),
        int(r.source),int(r.oxygen_selection),static_cast<unsigned long>(r.oxygen_selection_generation),
        static_cast<unsigned long>(r.oxygen_calibration_revision),int(r.status),unsigned(a && a->valid),r.oxygen_percent,r.helium_percent,
        a && a->valid?a->oxygen_percent:NAN,a && a->valid?a->helium_percent:NAN,a && a->valid?a->nitrogen_percent:NAN,
        r.co_valid?r.co_ppm:NAN,unsigned(r.co_valid),r.environment_valid?r.temperature_c:NAN,
        r.environment_valid?r.humidity_pct:NAN,r.environment_valid?r.pressure_bar:NAN,unsigned(r.environment_valid),
        unsigned(r.oxygen_calibrated),unsigned(r.helium_calibrated),unsigned(r.calibration_unvalidated),
        a?int(a->gas_mode):-1,a?a->planned_depth_m:NAN,a && a->valid?a->mod_working_m:NAN,a?int(a->severity):-1,
        static_cast<unsigned long>(r.helium_calibration_revision));
    return length>=0 && size_t(length)<size;
}
}
