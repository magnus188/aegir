cmake_minimum_required(VERSION 3.20)

project(trimix_web_demo LANGUAGES C CXX)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "Use Emscripten: emcmake cmake -S web -B web/build")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TRIMIX_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(TRIMIX_LVGL_VERSION "9.3.0" CACHE STRING "LVGL version used by the web demo")
set(TRIMIX_LOCAL_LVGL "${TRIMIX_ROOT}/managed_components/lvgl__lvgl")

# ESP-IDF generates managed_components locally, but that directory is intentionally
# not committed. Reuse it when present and otherwise fetch the firmware's pinned
# LVGL version for CI and clean checkouts.
if(EXISTS "${TRIMIX_LOCAL_LVGL}/CMakeLists.txt")
    set(TRIMIX_LVGL_SOURCE "${TRIMIX_LOCAL_LVGL}")
else()
    include(FetchContent)
    FetchContent_Declare(
        trimix_lvgl
        URL "https://github.com/lvgl/lvgl/archive/refs/tags/v${TRIMIX_LVGL_VERSION}.tar.gz"
        URL_HASH "SHA256=7037bb7237187426a49411d6864799abcd664aa875a5b3d142969fab46b65910"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_GetProperties(trimix_lvgl)
    if(NOT trimix_lvgl_POPULATED)
        FetchContent_Populate(trimix_lvgl)
    endif()
    set(TRIMIX_LVGL_SOURCE "${trimix_lvgl_SOURCE_DIR}")
endif()

set(LV_BUILD_CONF_PATH "${TRIMIX_ROOT}/simulator/lv_conf/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)

# SIMD assembly files are not valid WebAssembly input. LVGL still discovers them
# through its source glob, so mark them as headers before creating the target and
# remove them from the target's final source list as an extra guard.
set_source_files_properties(
    "${TRIMIX_LVGL_SOURCE}/src/draw/sw/blend/helium/lv_blend_helium.S"
    "${TRIMIX_LVGL_SOURCE}/src/draw/sw/blend/neon/lv_blend_neon.S"
    PROPERTIES HEADER_FILE_ONLY TRUE
)

add_subdirectory("${TRIMIX_LVGL_SOURCE}" "${CMAKE_BINARY_DIR}/lvgl")
get_target_property(LVGL_SOURCES lvgl SOURCES)
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*lv_blend_(helium|neon)\\.S$")
set_target_properties(lvgl PROPERTIES SOURCES "${LVGL_SOURCES}")
target_compile_options(lvgl PUBLIC "-sUSE_SDL=2")

set(TRIMIX_COMMON_INCLUDE_DIRS
    "${TRIMIX_ROOT}/main"
    "${TRIMIX_ROOT}/main/ui/screens"
    "${TRIMIX_ROOT}/main/ui/lvgl"
    "${TRIMIX_ROOT}/main/ui/fonts"
    "${TRIMIX_ROOT}/main/ui/styles"
    "${TRIMIX_ROOT}/main/ui/components"
    "${TRIMIX_ROOT}/main/ui/config"
    "${TRIMIX_ROOT}/main/analysis"
    "${TRIMIX_ROOT}/main/sensors"
    "${TRIMIX_ROOT}/main/services"
    "${TRIMIX_ROOT}/main/board"
)

set(TRIMIX_UI_SOURCES
    "${TRIMIX_ROOT}/main/version.cpp"
    "${TRIMIX_ROOT}/main/analysis/analysis_calculator.cpp"
    "${TRIMIX_ROOT}/main/sensors/sensor_interface.cpp"
    "${TRIMIX_ROOT}/main/services/analysis_history.cpp"
    "${TRIMIX_ROOT}/main/services/cylinder_profiles.cpp"
    "${TRIMIX_ROOT}/main/services/mix_label_service.cpp"
    "${TRIMIX_ROOT}/main/ui/screens/screen_manager.cpp"
    "${TRIMIX_ROOT}/main/ui/screens/home/home_screen.cpp"
    "${TRIMIX_ROOT}/main/ui/screens/analyse/analyse_screen.cpp"
    "${TRIMIX_ROOT}/main/ui/screens/history/history_screen.cpp"
    "${TRIMIX_ROOT}/main/ui/screens/cylinders/cylinders_screen.cpp"
    "${TRIMIX_ROOT}/main/ui/screens/settings/settings_screen.cpp"
    "${TRIMIX_ROOT}/main/ui/screens/settings/wifi_screen.cpp"
    "${TRIMIX_ROOT}/main/ui/screens/settings/update_screen.cpp"
    "${TRIMIX_ROOT}/main/ui/screens/settings/device_screen.cpp"
    "${TRIMIX_ROOT}/main/ui/screens/settings/calibrate_screen.cpp"
    "${TRIMIX_ROOT}/main/ui/screens/settings/safety_screen.cpp"
    "${TRIMIX_ROOT}/main/ui/screens/dive_planner/dive_planner_screen.cpp"
    "${TRIMIX_ROOT}/main/ui/screens/dive_planner/gas_calculator.cpp"
    "${TRIMIX_ROOT}/main/ui/styles/styles.cpp"
    "${TRIMIX_ROOT}/main/ui/components/navbar.cpp"
    "${TRIMIX_ROOT}/main/ui/components/status_icons.cpp"
    "${TRIMIX_ROOT}/main/ui/fonts/custom_font_normal_16.c"
    "${TRIMIX_ROOT}/main/ui/fonts/custom_font_bold_20.c"
    "${TRIMIX_ROOT}/main/ui/fonts/custom_font_bold_24.c"
    "${TRIMIX_ROOT}/simulator/mocks/settings_service_mock.cpp"
    "${TRIMIX_ROOT}/simulator/mocks/wifi_service_mock.cpp"
    "${TRIMIX_ROOT}/simulator/mocks/battery_service_mock.cpp"
    "${TRIMIX_ROOT}/simulator/mocks/backlight_service_mock.cpp"
    "${TRIMIX_ROOT}/simulator/mocks/ota_service_mock.cpp"
)

add_library(trimix_web_app STATIC ${TRIMIX_UI_SOURCES})
target_compile_definitions(trimix_web_app PUBLIC TRIMIX_SIMULATOR=1 TRIMIX_WEB=1)
target_compile_options(trimix_web_app PUBLIC "-sUSE_SDL=2")
target_include_directories(trimix_web_app PUBLIC
    "${TRIMIX_ROOT}/simulator/stubs"
    ${TRIMIX_COMMON_INCLUDE_DIRS}
)
target_link_libraries(trimix_web_app PUBLIC lvgl)

add_executable(index "${CMAKE_CURRENT_LIST_DIR}/src/main.cpp")
target_compile_options(index PRIVATE "-sUSE_SDL=2")
target_link_libraries(index PRIVATE trimix_web_app)
target_link_options(index PRIVATE
    "-sUSE_SDL=2"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=67108864"
    "-sENVIRONMENT=web"
    "-sNO_EXIT_RUNTIME=1"
    "SHELL:--shell-file ${CMAKE_CURRENT_LIST_DIR}/shell.html"
)
set_target_properties(index PROPERTIES SUFFIX ".html" OUTPUT_NAME "index")
set_property(TARGET index APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/shell.html")

file(WRITE "${CMAKE_BINARY_DIR}/.nojekyll" "")
