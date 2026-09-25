#pragma once

#include <chrono>
#include <thread>

using TickType_t = unsigned int;
using BaseType_t = int;

#ifndef pdTRUE
#define pdTRUE 1
#endif

#ifndef pdFALSE
#define pdFALSE 0
#endif

#ifndef portMAX_DELAY
#define portMAX_DELAY 0xffffffffU
#endif

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) (ms)
#endif

static inline void vTaskDelay(TickType_t ticks) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ticks));
}
