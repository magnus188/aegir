#include "version.h"

#include <cstdio>
#include <cstring>

namespace {

int tests_passed = 0;
int tests_failed = 0;

void assert_true(bool condition, const char* test_name) {
    if (condition) {
        std::printf("  PASS %s\n", test_name);
        tests_passed++;
    } else {
        std::printf("  FAIL %s\n", test_name);
        tests_failed++;
    }
}

}  // namespace

int main() {
    std::printf("Version Consistency Tests\n");
    std::printf("=========================\n\n");

    char expected[32];
    std::snprintf(expected, sizeof(expected), "%d.%d.%d",
                  TRIMIX_ANALYZER_VERSION_MAJOR,
                  TRIMIX_ANALYZER_VERSION_MINOR,
                  TRIMIX_ANALYZER_VERSION_PATCH);

    assert_true(std::strcmp(expected, TRIMIX_ANALYZER_VERSION) == 0,
                "numeric version macros match TRIMIX_ANALYZER_VERSION");
    assert_true(std::strcmp(get_version_string(), TRIMIX_ANALYZER_VERSION) == 0,
                "get_version_string returns TRIMIX_ANALYZER_VERSION");
    assert_true(std::strstr(get_build_info(), "Commit:") != nullptr,
                "build info includes commit field");

    std::printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
