#pragma once
#include "gas_calibration_core.h"

namespace gas_cal {
enum class SelectionResult { Present, Missing, Error };
class Slots {
public:
    virtual ~Slots() = default;
    virtual SelectionResult selected(gas_cal_channel_t channel, bool simulated, uint8_t &slot) = 0;
    virtual bool read(gas_cal_channel_t channel, bool simulated, uint8_t slot, gas_cal_record_t &record) = 0;
    virtual bool write(const gas_cal_record_t &record, uint8_t slot) = 0;
    virtual bool select(gas_cal_channel_t channel, bool simulated, uint8_t slot) = 0;
};
class Journal : public Storage {
public:
    explicit Journal(Slots &slots) : slots_(slots) {}
    bool load(gas_cal_channel_t channel, bool simulated, gas_cal_record_t &record) override;
    bool save(const gas_cal_record_t &record) override;
private:
    Slots &slots_;
};
}
