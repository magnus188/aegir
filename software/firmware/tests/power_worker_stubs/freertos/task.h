#pragma once
#include "FreeRTOS.h"
BaseType_t xTaskCreate(void(*)(void *),const char *,uint32_t,void *,unsigned,TaskHandle_t *);
TaskHandle_t xTaskGetCurrentTaskHandle();
