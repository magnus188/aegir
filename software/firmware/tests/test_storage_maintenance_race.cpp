#include "services/storage_service.h"
#include "services/maintenance_service.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <future>
#include <mutex>
#include <thread>

namespace {
int passed = 0, failed = 0;
void check(bool ok, const char *label) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", label); ok ? ++passed : ++failed; }
bool stop(uint32_t budget) { return budget > 0; }
bool power() { return true; }
std::atomic<unsigned> resumed{0};
void resume() { ++resumed; }
bool wait_until(const std::function<bool()> &predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!predicate()) { if (std::chrono::steady_clock::now() >= deadline) return false; std::this_thread::yield(); }
    return true;
}
struct Block {
    std::promise<void> entered, release;
    std::shared_future<void> released = release.get_future().share();
    void hold() { entered.set_value(); released.wait(); }
    bool started() { return entered.get_future().wait_for(std::chrono::seconds(2)) == std::future_status::ready; }
};
}
int main() {
    check(storage_init(), "Storage initializes for production gate concurrency tests");
    maintenance_register_hooks({stop, nullptr, resume, power});
    std::atomic<unsigned> selector{1};
    Block inactive;
    auto writer = std::async(std::launch::async, [&] {
        return storage_run_write([&] { inactive.hold(); return true; });
    });
    check(inactive.started(), "Inactive-slot NVS transaction is in flight");
    auto pause = std::async(std::launch::async, [] { return maintenance_begin(MAINTENANCE_OTA, 1500); });
    check(wait_until([] { return !storage_writes_allowed(); }), "Pause request closes the write gate before draining current transaction");
    check(pause.wait_for(std::chrono::milliseconds(10)) == std::future_status::timeout,
          "Maintenance cannot report Held while an NVS transaction remains in flight");
    check(!storage_run_write([&] { selector = 2; return true; }, 10),
          "New selector commit rejected during pending pause");
    inactive.release.set_value();
    check(writer.get() && pause.get(), "Current transaction completes before maintenance enters Held");
    check(!storage_run_write([&] { selector = 2; return true; }) && selector == 1,
          "Pause between journal phases preserves the prior active calibration");
    maintenance_finish(false);
    check(storage_writes_allowed() && !maintenance_is_active(), "Cancellation releases gate and resumes measurement hook");

    Block committing;
    auto commit = std::async(std::launch::async, [&] {
        return storage_run_write([&] { committing.hold(); selector = 2; return true; });
    });
    check(committing.started(), "Final selector commit enters its critical section");
    auto drain = std::async(std::launch::async, [] { return maintenance_begin(MAINTENANCE_SHUTDOWN, 1500); });
    check(wait_until([] { return !storage_writes_allowed(); }), "Shutdown waits for an already-started selector commit");
    committing.release.set_value();
    check(commit.get() && drain.get() && selector == 2,
          "Already-started selector commits completely before Held");
    check(!storage_run_write([&] { selector = 3; return true; }) && selector == 2,
          "No later selector change can occur after Held");
    maintenance_finish(false);

    Block stalled;
    auto slow = std::async(std::launch::async, [&] {
        return storage_run_write([&] { stalled.hold(); selector = 3; return true; });
    });
    check(stalled.started(), "Slow NVS transaction holds the production storage mutex");
    const auto start = std::chrono::steady_clock::now();
    const bool held = maintenance_begin(MAINTENANCE_OTA, 25);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    check(!held && elapsed < 500 && !maintenance_is_active(),
          "Storage drain timeout rejects maintenance within its budget instead of claiming Held");
    check(storage_writes_allowed(), "Rejected pause clears its pending gate without erasing committed data");
    stalled.release.set_value(); check(slow.get() && selector == 3, "Rejected maintenance lets the complete in-flight save finish");

    std::atomic<bool> done{false};
    std::atomic<unsigned> changes{0};
    std::thread contender([&] {
        while (!done) storage_run_write([&] { std::this_thread::yield(); ++changes; return true; }, 10);
    });
    bool invariant = true;
    for (unsigned cycle = 0; cycle < 100; ++cycle) {
        if (!maintenance_begin(MAINTENANCE_OTA, 1000)) { invariant = false; break; }
        const auto frozen = changes.load();
        for (unsigned spin = 0; spin < 20; ++spin) std::this_thread::yield();
        invariant = invariant && frozen == changes.load() && !storage_writes_allowed();
        maintenance_finish(false);
    }
    done = true; contender.join();
    check(invariant, "100 concurrent maintenance/save cycles preserve the no-commit-after-Held invariant");
    std::printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
