#pragma once
#include <cstdint>
enum maintenance_reason_t { MAINTENANCE_OTA, MAINTENANCE_RESTART, MAINTENANCE_SHUTDOWN, MAINTENANCE_MODE_CHANGE };
struct maintenance_hooks_t {
    bool (*stop_measurements)(uint32_t timeout_ms) = nullptr;
    bool (*flush_state)(uint32_t timeout_ms) = nullptr;
    void (*resume_measurements)() = nullptr;
    bool (*power_ok)() = nullptr;
};
void maintenance_register_hooks(const maintenance_hooks_t &hooks);
bool maintenance_begin(maintenance_reason_t reason, uint32_t timeout_ms);
// Release after failed/cancelled maintenance. Successful OTA holds until reboot.
void maintenance_finish(bool success);
bool maintenance_is_active();
bool maintenance_restart(maintenance_reason_t reason = MAINTENANCE_RESTART);
