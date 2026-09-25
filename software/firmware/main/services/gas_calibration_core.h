#pragma once
#include "gas_calibration_service.h"
#include <cstddef>

namespace gas_cal {
constexpr uint32_t kVersion = 1;
constexpr uint32_t kStaleMs = 2500;
constexpr uint32_t kHistoryMs = 10000;
constexpr size_t kHistoryCapacity = 64;

class Storage {
public:
    virtual ~Storage() = default;
    virtual bool load(gas_cal_channel_t channel, bool simulated, gas_cal_record_t &record) = 0;
    virtual bool save(const gas_cal_record_t &record) = 0;
};

uint32_t record_crc(const gas_cal_record_t &record);
// Integrity permits archived acquisition profiles; conversion requires record_valid.
bool record_integrity_valid(const gas_cal_record_t &record, gas_cal_channel_t channel, bool simulated);
bool record_valid(const gas_cal_record_t &record, gas_cal_channel_t channel, bool simulated);

class Core {
public:
    explicit Core(Storage &storage, bool simulated);
    void initialize();
    void reset_history(gas_cal_channel_t channel);
    void ingest(gas_cal_channel_t channel, const gas_raw_sample_t &sample);
    bool sample(gas_cal_channel_t channel, uint32_t now, gas_raw_sample_t &out) const;
    bool record(gas_cal_channel_t channel, gas_cal_record_t &out) const;
    bool point(gas_cal_channel_t channel, unsigned index, gas_cal_point_t &out) const;
    gas_cal_result_t capture(gas_cal_channel_t channel, unsigned index,
                             const gas_cal_reference_t &reference, uint32_t now);
    gas_cal_result_t commit(gas_cal_channel_t channel, uint32_t now);
    void cancel(gas_cal_channel_t channel);
    bool convert(gas_cal_channel_t channel, double voltage, double &percent) const;
private:
    struct Channel {
        gas_raw_sample_t latest{};
        gas_raw_sample_t history[kHistoryCapacity]{};
        size_t count = 0;
        bool present = false;
        gas_cal_record_t saved{};
        gas_cal_point_t pending[2]{};
    } channels_[GAS_CAL_CHANNEL_COUNT];
    Storage &storage_;
    bool simulated_;
    bool initialized_ = false;
};
}
