#pragma once
#include "sensor_interface.h"
esp_err_t sensor_hardware_read(sensor_readings_t *out);
void sensor_hardware_stop(void);
esp_err_t sensor_hardware_start();
bool sensor_hardware_stop_wait(uint32_t timeout_ms);
bool sensor_hardware_service_healthy();
