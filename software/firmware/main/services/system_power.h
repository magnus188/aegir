#pragma once
#include "sensors/power_monitor.h"
#include "sensors/environment_monitor.h"
#include "sensors/usb_input_policy.h"
bool system_power_start();
bool system_power_ready();
// Called only after successful startup/probation and compatible storage.
void system_power_accept_startup();
bool system_power_charge_standby();
bool system_power_ota_allowed();
bool system_power_shutdown(uint32_t timeout_ms);
power_monitor::Sample system_power_sample();
environment_monitor::Sample system_environment_sample();
usb_input::Result system_usb_input_sample();
