#pragma once
#include <cstdint>
#include "services/gas_calibration_service.h"
namespace gas_acquisition {
// Overall assembly revision for tooling. Calibration compatibility uses the
// affected channel's profile revision below, not this aggregate number.
constexpr uint32_t kRevision = 2;
constexpr uint8_t kOxygenGain = 8;
constexpr uint8_t kHeliumGain = 1;
struct Profile {
    uint32_t revision;
    uint8_t gain;
    uint8_t samples_per_second;
    uint16_t internal_reference_mv;
    bool pga_bypass;
    bool identity_bounds_qualified;
};
// Increment the affected profile revision when its analog circuit, reference,
// rate, filter or conversion meaning changes. Unaffected channel records remain
// compatible across OTA. He revision2 uses symmetric 680 ohm input resistors.
constexpr Profile kOxygenProfile{1, kOxygenGain, 20, 2048, false, false};
constexpr Profile kHeliumProfile{2, kHeliumGain, 20, 2048, true, false};
constexpr uint32_t revision_for(gas_cal_channel_t channel) {
    return channel == GAS_CAL_HELIUM ? kHeliumProfile.revision : kOxygenProfile.revision;
}
}
