#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace persistent {
enum class Read { Ok, Missing, Error };
class Backend {
public:
    virtual ~Backend() = default;
    virtual Read get(const char *key, std::vector<uint8_t> &bytes) = 0;
    virtual bool put(const char *key, const std::vector<uint8_t> &bytes) = 0;
};
// Namespace and schema are the compatibility boundary. Never mutate a legacy namespace.
class Journal {
public:
    explicit Journal(Backend &backend) : backend_(backend) {}
    Read load(uint32_t schema, void *out, size_t size);
    bool save(uint32_t schema, const void *data, size_t size);
private:
    Backend &backend_;
};
uint32_t crc32(const uint8_t *data, size_t size);
}
