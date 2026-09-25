#pragma once
#include <cstdint>
using TickType_t=uint32_t;
using BaseType_t=int;
using TaskHandle_t=void *;
constexpr int pdPASS=1,configTICK_RATE_HZ=1000;
void vTaskDelay(TickType_t);
