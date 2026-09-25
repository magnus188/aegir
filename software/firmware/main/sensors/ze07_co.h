#pragma once
#include <cstddef>
#include <cstdint>

// Winsen ZE07-CO V1.7 default initiative-upload frames only. Never sends calibration commands.
class Ze07CoDecoder {
public:
    bool push(uint8_t byte, uint32_t now_ms);
    bool latest(uint32_t now_ms, float &ppm) const;
    static bool decode(const uint8_t frame[9], float &ppm);
private:
    uint8_t window_[9]{};
    size_t count_ = 0;
    float ppm_ = 0;
    uint32_t last_ms_ = 0;
    bool valid_ = false;
};
