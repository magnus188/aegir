#pragma once
#include "settings_service.h"

namespace settings_validation {
struct LimitPair { setting_key_t advisory, alarm; };
inline constexpr LimitPair pairs[] = {
    {SETTING_PPO2_WORKING_X100, SETTING_PPO2_SECONDARY_X100},
    {SETTING_DENSITY_ADVISORY_X10, SETTING_DENSITY_ALARM_X10},
    {SETTING_CO_ADVISORY_PPM, SETTING_CO_ALARM_PPM},
    {SETTING_HUMIDITY_ADVISORY_PCT, SETTING_HUMIDITY_ALARM_PCT},
};
inline bool ordered(const int32_t* values) {
    for (const auto& pair : pairs)
        if (values[pair.advisory] >= values[pair.alarm]) return false;
    return true;
}
// Older firmware allowed inverted limits. Repair only the affected pairs,
// preserving other preferences and leaving the archived journal untouched.
inline void normalize(int32_t* values, const setting_def_t* definitions) {
    for (const auto& pair : pairs) if (values[pair.advisory] >= values[pair.alarm]) {
        values[pair.advisory] = definitions[pair.advisory].default_value;
        values[pair.alarm] = definitions[pair.alarm].default_value;
    }
}
}
