#include "version.h"
#include <stdio.h>

static char version_buffer[64];
static char build_info_buffer[128];

const char* get_version_string(void) {
    snprintf(version_buffer, sizeof(version_buffer), 
             "%d.%d.%d", 
             TRIMIX_ANALYZER_VERSION_MAJOR, 
             TRIMIX_ANALYZER_VERSION_MINOR, 
             TRIMIX_ANALYZER_VERSION_PATCH);
    return version_buffer;
}

const char* get_build_info(void) {
    snprintf(build_info_buffer, sizeof(build_info_buffer),
             "Built on %s %s\nCommit: %s",
             BUILD_DATE, BUILD_TIME, GIT_COMMIT);
    return build_info_buffer;
}
