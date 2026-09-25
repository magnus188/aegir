#include "mix_label_service.h"
#include <cstdio>

namespace {

const char* safe_text(const char* value, const char* fallback) {
    return value && value[0] != '\0' ? value : fallback;
}

}  // namespace

extern "C" {

void mix_label_build_text(const analysis_history_record_t* record,
                          const cylinder_profile_t* profile,
                          char* out,
                          size_t out_size) {
    if (!out || out_size == 0) {
        return;
    }
    if (!record) {
        std::snprintf(out, out_size, "No analysis label available");
        return;
    }

    const char* cylinder_name = profile ? safe_text(profile->name, "Unassigned") : "Unassigned";
    const char* cylinder_serial = profile ? safe_text(profile->serial, "No serial") : "No serial";

    std::snprintf(out, out_size,
                  "TRIMIX ANALYSER LABEL\n"
                  "Cylinder: %s\n"
                  "Serial: %s\n"
                  "Mode: %s\n"
                  "Mix: %s\n"
                  "O2: %.1f%%  He: %.0f%%  N2: %.1f%%\n"
                  "Planned depth: %.0fm\n"
                  "MOD: %.0f/%.0fm  PPO2: %.2f\n"
                  "Density: %.1f g/L  CO2: %.0f ppm\n"
                  "Status: %s\n"
                  "Share payload: trimix-label-v1",
                  cylinder_name,
                  cylinder_serial,
                  analysis_gas_mode_label(record->gas_mode),
                  record->mix_label,
                  record->oxygen_percent,
                  record->helium_percent,
                  record->nitrogen_percent,
                  record->planned_depth_m,
                  record->mod_working_m,
                  record->mod_secondary_m,
                  record->ppo2_at_depth,
                  record->gas_density_g_l,
                  record->co2_ppm,
                  analysis_severity_label(record->severity));
}

void mix_label_build_csv(const analysis_history_record_t* record,
                         const cylinder_profile_t* profile,
                         char* out,
                         size_t out_size) {
    if (!out || out_size == 0) {
        return;
    }
    if (!record) {
        std::snprintf(out, out_size, "error,no analysis");
        return;
    }

    const char* cylinder_name = profile ? safe_text(profile->name, "Unassigned") : "Unassigned";
    const char* cylinder_serial = profile ? safe_text(profile->serial, "No serial") : "No serial";

    std::snprintf(out, out_size,
                  "cylinder,serial,mode,mix,o2,he,n2,depth,mod_work,mod_secondary,ppo2,density,co2,status\n"
                  "%s,%s,%s,%s,%.1f,%.0f,%.1f,%.0f,%.0f,%.0f,%.2f,%.1f,%.0f,%s",
                  cylinder_name,
                  cylinder_serial,
                  analysis_gas_mode_label(record->gas_mode),
                  record->mix_label,
                  record->oxygen_percent,
                  record->helium_percent,
                  record->nitrogen_percent,
                  record->planned_depth_m,
                  record->mod_working_m,
                  record->mod_secondary_m,
                  record->ppo2_at_depth,
                  record->gas_density_g_l,
                  record->co2_ppm,
                  analysis_severity_label(record->severity));
}

}  // extern "C"
