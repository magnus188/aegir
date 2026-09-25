#pragma once

#include <stdio.h>

#ifndef ESP_LOGI
#define ESP_LOGI(tag, fmt, ...) fprintf(stderr, "[I] %s: " fmt "\n", tag, ##__VA_ARGS__)
#endif

#ifndef ESP_LOGW
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "[W] %s: " fmt "\n", tag, ##__VA_ARGS__)
#endif

#ifndef ESP_LOGE
#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "[E] %s: " fmt "\n", tag, ##__VA_ARGS__)
#endif

#ifndef ESP_LOGD
#define ESP_LOGD(tag, fmt, ...) do { (void)(tag); } while (0)
#endif

#ifndef ESP_LOGV
#define ESP_LOGV(tag, fmt, ...) do { (void)(tag); } while (0)
#endif
