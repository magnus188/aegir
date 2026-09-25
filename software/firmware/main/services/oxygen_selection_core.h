#pragma once
#include "oxygen_selection_service.h"
#include <cstddef>
#include "sensors/gas_acquisition_config.h"

namespace oxygen_selection {
constexpr uint32_t kSchema = 1;
struct Record {
    uint32_t format_version;
    uint32_t generation;
    uint32_t choice;
    uint32_t acquisition_revision;
    uint32_t profile_revision;
    uint32_t minimum_calibration_revision;
    uint32_t simulated;
    uint32_t reserved;
};
enum class ReadResult { Missing, Present, Error };
class Storage {
public:
    virtual ~Storage() = default;
    virtual ReadResult read(Record &record) = 0;
    virtual bool write(const Record &record) = 0;
};
bool channel_for(oxygen_selection_t choice, gas_cal_channel_t &channel);
bool record_valid(const Record &record, bool simulated,
                  uint32_t supported_profile_revision = gas_acquisition::kOxygenProfile.revision);
oxygen_air_advice_t air_advice(bool known_air, const gas_raw_sample_t *ao2, const gas_raw_sample_t *jj);
class Core {
public:
    Core(Storage &storage, bool simulated,
         uint32_t supported_profile_revision = gas_acquisition::kOxygenProfile.revision)
        : storage_(storage), simulated_(simulated), profile_revision_(supported_profile_revision) {}
    void initialize();
    oxygen_selection_status_t status(uint32_t current_calibration_revision) const;
    oxygen_selection_result_t confirm(oxygen_selection_t choice, bool replacement, uint32_t previous_revision);
    bool selected(gas_cal_channel_t &channel) const;
    bool usable(gas_cal_channel_t channel, uint32_t revision) const;
private:
    Storage &storage_;
    bool simulated_;
    const uint32_t profile_revision_;
    bool initialized_ = false;
    bool storage_error_ = false;
    Record record_{};
};
}
