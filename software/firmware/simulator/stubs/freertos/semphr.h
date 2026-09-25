#pragma once

#include "FreeRTOS.h"

using SemaphoreHandle_t = void*;

static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    return reinterpret_cast<SemaphoreHandle_t>(1);
}

static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t) {
    return pdTRUE;
}

static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t) {
    return pdTRUE;
}
