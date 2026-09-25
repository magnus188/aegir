cmake_minimum_required(VERSION 3.16)

# Keep ESP-IDF's application descriptor aligned with the version used by the
# OTA service and release assets.  Falling back to git describe here would
# report the last S3-era tag from P4 firmware built between releases.
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/main/version.h" TRIMIX_VERSION_DEFINE
     REGEX "^#define TRIMIX_ANALYZER_VERSION ")
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" PROJECT_VER
       "${TRIMIX_VERSION_DEFINE}")
if(NOT PROJECT_VER)
    message(FATAL_ERROR "Could not read TRIMIX_ANALYZER_VERSION from main/version.h")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

add_compile_definitions( LV_CONF_INCLUDE_SIMPLE=1 )
idf_build_set_property(
    COMPILE_OPTIONS
    "-I${CMAKE_CURRENT_LIST_DIR}/main;-I${CMAKE_CURRENT_LIST_DIR}/main/ui/config"
    APPEND
)

project(Trimix_analyzer)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/guition_hosted_sdmmc.cmake")
