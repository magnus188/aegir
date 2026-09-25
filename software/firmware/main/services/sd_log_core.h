#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace sd_log {
enum class Mode { Idle, Analysis, Calibration };
enum class Kind { Results, Raw, Event };
enum class State { Starting, Ready, Recording, Pausing, Paused, Ejected, Unavailable };
struct Status {
    State state = State::Starting;
    Mode activity = Mode::Idle;
    uint64_t accepted = 0, written = 0, synced = 0, dropped = 0;
    uint64_t capacity_bytes = 0;
    uint32_t session = 0;
    bool closed = true;
    char detail[96] = "SD starting";
    char path[96] = {};
};
class Backend {
public:
    virtual ~Backend() = default;
    virtual bool mount(uint64_t &capacity_bytes) = 0;
    virtual bool open(uint32_t session, Mode mode, char *path, size_t size) = 0;
    virtual bool append(Kind kind, const char *row) = 0;
    virtual bool flush() = 0;
    // Must close all owned file handles even after a flush/write error.
    virtual bool close() = 0;
    virtual bool unmount() = 0;
    virtual const char *error() const = 0;
};
// One consumer performs every Backend operation. Producers never perform I/O
// and only try-lock the bounded queue. Busy/full queues explicitly count loss.
class Logger {
public:
    static constexpr size_t kCapacity = 64, kRowBytes = 768;
    explicit Logger(Backend &backend) : backend_(backend) {}
    void mode(Mode mode);
    void end_mode(Mode expected);
    bool submit(Kind kind, const char *row);
    void pause(bool eject = false);
    void resume();
    void retry();
    // Only before the writer exists (e.g. task allocation failure).
    void startup_failed();
    bool quiescent() const;
    Status status() const;
    void pump(uint32_t now_ms);
private:
    struct Row { uint32_t session; Mode mode; Kind kind; char text[kRowBytes]; };
    Backend &backend_;
    mutable std::mutex lock_;
    std::array<Row,kCapacity> rows_{};
    size_t head_ = 0, count_ = 0;
    Mode mode_ = Mode::Idle;
    uint32_t generation_ = 1, open_generation_ = 0, last_flush_ = 0;
    bool paused_ = false, held_ = false, eject_ = false, retry_ = true, mounted_ = false;
    Status status_{};
    std::atomic<uint64_t> contention_drops_{0};
    std::atomic<bool> busy_{false};
    std::atomic<bool> requested_{false};
    void failure(const char *message);
    bool close_current();
};
const char *state_label(State state);
}
