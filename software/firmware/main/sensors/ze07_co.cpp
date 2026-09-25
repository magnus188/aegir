#include "ze07_co.h"
#include <cstring>
bool Ze07CoDecoder::decode(const uint8_t f[9], float &ppm) {
    if (!f || f[0] != 0xff || f[1] != 0x04 || f[2] != 0x03 || f[3] != 1) return false;
    uint8_t sum = 0;
    for (unsigned i = 1; i < 9; ++i) sum = static_cast<uint8_t>(sum + f[i]);
    const unsigned value = (static_cast<unsigned>(f[4]) << 8) | f[5];
    const unsigned range = (static_cast<unsigned>(f[6]) << 8) | f[7];
    if (sum || range != 5000 || value > range) return false;
    ppm = value / 10.0f; return true;
}
bool Ze07CoDecoder::push(uint8_t byte, uint32_t now_ms) {
    if (count_ == sizeof(window_)) { std::memmove(window_, window_ + 1, sizeof(window_) - 1); --count_; }
    window_[count_++] = byte;
    float value;
    if (count_ == sizeof(window_) && decode(window_, value)) {
        ppm_ = value; last_ms_ = now_ms; valid_ = true; count_ = 0; return true;
    }
    return false;
}
bool Ze07CoDecoder::latest(uint32_t now_ms, float &ppm) const {
    if (!valid_ || now_ms - last_ms_ > 3000) return false;
    ppm = ppm_; return true;
}
