#include "gas_calibration_journal.h"
#include <cstring>

namespace gas_cal {
bool Journal::load(gas_cal_channel_t c, bool simulated, gas_cal_record_t &r) {
    uint8_t active = 0;
    return slots_.selected(c, simulated, active) == SelectionResult::Present && active < 2 &&
        // Keep an intact older profile available as history and as the revision
        // counter baseline. It is never approved for conversion by this read.
        slots_.read(c, simulated, active, r) && record_integrity_valid(r, c, simulated);
}
bool Journal::save(const gas_cal_record_t &r) {
    if (!record_valid(r, r.channel, r.simulated)) return false;
    uint8_t previous = 1;
    const auto result = slots_.selected(r.channel, r.simulated, previous);
    if (result == SelectionResult::Error || previous > 1) return false;
    const uint8_t next = previous ^ 1u;
    // Publishing the selector is a separate final commit. An interruption while
    // writing or verifying the inactive slot leaves the previous selection intact.
    if (!slots_.write(r, next)) return false;
    gas_cal_record_t readback{};
    if (!slots_.read(r.channel, r.simulated, next, readback) || !record_valid(readback, r.channel, r.simulated) ||
        std::memcmp(&r, &readback, sizeof(r)) != 0) return false;
    return slots_.select(r.channel, r.simulated, next);
}
}
