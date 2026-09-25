#pragma once

// Trimix Analyzer version information
#define TRIMIX_ANALYZER_VERSION_MAJOR 0
#define TRIMIX_ANALYZER_VERSION_MINOR 2
#define TRIMIX_ANALYZER_VERSION_PATCH 0
#define TRIMIX_ANALYZER_VERSION "0.2.0"

// Build information (these can be set during build process)
#ifndef BUILD_DATE
#define BUILD_DATE __DATE__
#endif

#ifndef BUILD_TIME
#define BUILD_TIME __TIME__
#endif

#ifndef GIT_COMMIT
#define GIT_COMMIT "unknown"
#endif

// GitHub repository information
#define GITHUB_OWNER "magnus188"
#define GITHUB_REPO "ziphius"
#define GITHUB_API_URL "https://api.github.com/repos/" GITHUB_OWNER "/" GITHUB_REPO "/releases/latest"
#define GITHUB_RELEASES_URL "https://github.com/" GITHUB_OWNER "/" GITHUB_REPO "/releases"

// Function to get full version string
const char* get_version_string(void);
const char* get_build_info(void);
