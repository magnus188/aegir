#pragma once
#include <stdint.h>
#include <esp_err.h>
// Narrow boundary stub: the actual backlight service remains compiled.
extern "C" esp_err_t guition_backlight_set(uint8_t percent);
