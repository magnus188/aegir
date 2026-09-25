#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <atomic>
namespace ota {
enum class Family { Pre3, V3 };
struct Release {
    std::string version, notes, url;
    uint32_t size = 0;
    uint8_t sha256[32]{};
    bool newer = false;
};
class Response {
public:
    bool append(const char *bytes, size_t count);
    void clear() { body_.clear(); overflow_ = false; }
    const std::string &body() const { return body_; }
    bool overflowed() const { return overflow_; }
private:
    std::string body_;
    bool overflow_ = false;
};
bool version(const char *text, uint32_t (&parts)[3]);
bool parse_release(const char *json, size_t size, Family family, const char *current,
                   const char *repository_url, uint32_t slot_size, Release &out, std::string &error);
bool image_identity(const char *project, const char *version, const char *expected_project, const char *expected_version);
// Optional sensors and network connectivity are not image-health prerequisites.
bool boot_healthy(unsigned ui_heartbeats, bool worker_responsive, bool storage_compatible = true);
// Started before essential initialization. A separate task can expire probation
// even if app_main blocks. Completion and timeout have a single atomic winner.
class BootProbation {
public:
    static constexpr uint32_t kDeadlineMs = 60000;
    enum class Completion { Timely, Timeout, AlreadyExpired };
    void begin(uint32_t now_ms, bool required);
    Completion complete(uint32_t now_ms);
    bool expire(uint32_t now_ms);
    bool pending() const;
private:
    enum State { Completed, Checking, Expired };
    std::atomic<State> state_{Completed};
    uint32_t start_ = 0;
};
enum class TransferStep { More, Complete, Error };
enum class InstallResult { Ok, Cancelled, BeginFailed, IdentityMismatch, TransferFailed, Truncated, DigestMismatch, ActivationFailed, Timeout };
struct ImageIdentity { char project[32]{}; char version[32]{}; };
class ImageTransport {
public:
    virtual ~ImageTransport() = default;
    virtual bool begin() = 0;
    virtual bool identity(ImageIdentity &out) = 0;
    virtual TransferStep transfer() = 0;
    virtual uint32_t received() = 0;
    virtual bool complete() = 0;
    virtual bool digest_matches() = 0;
    virtual bool activate() = 0;
    virtual void abort() = 0;
    virtual bool cancelled() = 0;
    virtual uint32_t now_ms() = 0;
};
// This exact state machine wraps ESP-IDF flash operations and runs under fault injection.
InstallResult install(ImageTransport &transport, const char *project, const Release &release,
                      uint32_t timeout_ms = 600000);
const char *install_message(InstallResult result);
}
