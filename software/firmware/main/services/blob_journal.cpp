#include "blob_journal.h"
#include <cstring>

namespace persistent {
namespace {
constexpr size_t kHeader = 16, kMaximumPayload = 4096;
void append32(std::vector<uint8_t> &v, uint32_t n) {
    for (unsigned i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>(n >> (8 * i)));
}
uint32_t read32(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
bool valid(const std::vector<uint8_t> &v, uint32_t schema, size_t size) {
    return size <= kMaximumPayload && v.size() == size + kHeader &&
        read32(v.data()) == 0x31524a54 && read32(v.data() + 4) == schema &&
        read32(v.data() + 8) == size && read32(v.data() + 12) == crc32(v.data() + kHeader, size);
}
}
uint32_t crc32(const uint8_t *data, size_t size) {
    uint32_t crc = 0xffffffff;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
Read Journal::load(uint32_t schema, void *out, size_t size) {
    if (!out || !size || size > kMaximumPayload) return Read::Error;
    std::vector<uint8_t> selection, bytes;
    const auto result = backend_.get("active", selection);
    if (result != Read::Ok) return result;
    if (selection.size() != 1 || selection[0] > 1 ||
        backend_.get(selection[0] ? "b" : "a", bytes) != Read::Ok || !valid(bytes, schema, size)) return Read::Error;
    std::memcpy(out, bytes.data() + kHeader, size);
    return Read::Ok;
}
bool Journal::save(uint32_t schema, const void *data, size_t size) {
    if (!data || !size || size > kMaximumPayload) return false;
    std::vector<uint8_t> selection;
    const auto result = backend_.get("active", selection);
    if (result == Read::Error || (result == Read::Ok && (selection.size() != 1 || selection[0] > 1))) return false;
    const uint8_t next = result == Read::Missing ? 0 : selection[0] ^ 1;
    if (result == Read::Ok) {
        std::vector<uint8_t> old;
        if (backend_.get(selection[0] ? "b" : "a", old) != Read::Ok || !valid(old, schema, size)) return false;
    }
    std::vector<uint8_t> bytes;
    append32(bytes, 0x31524a54); append32(bytes, schema); append32(bytes, size);
    append32(bytes, crc32(static_cast<const uint8_t *>(data), size));
    const auto *payload = static_cast<const uint8_t *>(data);
    bytes.insert(bytes.end(), payload, payload + size);
    const char *key = next ? "b" : "a";
    std::vector<uint8_t> verified;
    if (!backend_.put(key, bytes) || backend_.get(key, verified) != Read::Ok || verified != bytes) return false;
    if (!backend_.put("active", {next})) return false;
    return backend_.get("active", selection) == Read::Ok && selection == std::vector<uint8_t>{next};
}
}
