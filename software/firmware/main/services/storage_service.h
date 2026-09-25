#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
enum storage_read_result_t { STORAGE_OK, STORAGE_MISSING, STORAGE_ERROR };
bool storage_init();
bool storage_ready();
bool storage_writes_allowed();
const char *storage_status_message();
void storage_accept_boot();
bool storage_pause_writes(bool paused, uint32_t timeout_ms = 1000);
// Low-level NVS mutation and commit run under the same gate as maintenance.
// Callback must not call storage APIs or acquire configuration/calibration locks.
bool storage_run_write(const std::function<bool()> &operation, uint32_t timeout_ms = 1000);
storage_read_result_t storage_read_blob(const char *namespace_name, uint32_t schema, void *out, size_t bytes);
bool storage_write_blob(const char *namespace_name, uint32_t schema, const void *data, size_t bytes);
