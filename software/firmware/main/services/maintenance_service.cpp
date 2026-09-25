#include "maintenance_service.h"
#include "storage_service.h"
#include <atomic>
#include <mutex>
#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
#include <esp_system.h>
#include <esp_timer.h>
#else
#include <chrono>
#endif
namespace {
enum Stage { Idle, Preparing, Held };
std::atomic<Stage> stage{Idle};
std::mutex hook_lock;
maintenance_hooks_t hooks;
uint32_t now_ms() {
#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
    return esp_timer_get_time() / 1000;
#else
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}
}
void maintenance_register_hooks(const maintenance_hooks_t &value) {
    std::lock_guard<std::mutex> guard(hook_lock);
    if (stage == Idle) hooks = value;
}
bool maintenance_begin(maintenance_reason_t reason, uint32_t timeout_ms) {
    Stage expected = Idle;
    if (!timeout_ms || !stage.compare_exchange_strong(expected, Preparing)) return false;
    maintenance_hooks_t copy;
    { std::lock_guard<std::mutex> guard(hook_lock); copy = hooks; }
    const uint32_t start = now_ms();
    if (!copy.stop_measurements || (reason == MAINTENANCE_OTA && (!copy.power_ok || !copy.power_ok()))) {
        stage = Idle; return false;
    }
    auto remaining = [&] { const uint32_t elapsed = now_ms() - start; return elapsed >= timeout_ms ? 0u : timeout_ms - elapsed; };
    const bool stopped = copy.stop_measurements(remaining());
    const bool flushed = stopped && remaining() && (!copy.flush_state || copy.flush_state(remaining()));
    if (!flushed || !remaining() || !storage_pause_writes(true, remaining())) {
        if (copy.resume_measurements) copy.resume_measurements();
        stage = Idle; return false;
    }
    stage = Held;
    return true;
}
void maintenance_finish(bool success) {
    if (stage != Held || success) return;
    maintenance_hooks_t copy;
    { std::lock_guard<std::mutex> guard(hook_lock); copy = hooks; }
    // Keep the maintenance hold if storage cannot be released within its bound.
    if (!storage_pause_writes(false, 1000)) return;
    if (copy.resume_measurements) copy.resume_measurements();
    stage = Idle;
}
bool maintenance_is_active() { return stage != Idle; }
bool maintenance_restart(maintenance_reason_t reason) {
    if (stage != Held && !maintenance_begin(reason, 3000)) return false;
#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
    esp_restart();
#endif
    return true;
}
