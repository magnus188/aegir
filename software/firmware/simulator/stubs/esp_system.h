#pragma once

#include <stdio.h>

static inline void esp_restart(void) {
    fprintf(stderr, "[sim] esp_restart requested\n");
}
