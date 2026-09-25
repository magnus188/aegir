#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define TRIMIX_C6_IMAGE_BYTES 1247408u
#define TRIMIX_C6_IMAGE_PROJECT "network_adapter"
#define TRIMIX_C6_IMAGE_VERSION "2.12.9"
static inline bool c6_recovery_descriptor_valid(uint32_t partition_size, uint16_t chip,
                                               const char *project, size_t project_size,
                                               const char *version, size_t version_size) {
    return partition_size >= TRIMIX_C6_IMAGE_BYTES && chip == 0x000d && project && version &&
        memchr(project, 0, project_size) && memchr(version, 0, version_size) &&
        strcmp(project, TRIMIX_C6_IMAGE_PROJECT) == 0 && strcmp(version, TRIMIX_C6_IMAGE_VERSION) == 0;
}
static inline bool c6_recovery_needs_activation(uint32_t major, uint32_t minor) {
    return major > 2 || (major == 2 && minor >= 6);
}
