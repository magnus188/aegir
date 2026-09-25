cmake_minimum_required(VERSION 3.20)

project(trimix_host_tools LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TRIMIX_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)

set_source_files_properties(
    "${TRIMIX_ROOT}/managed_components/lvgl__lvgl/src/draw/sw/blend/helium/lv_blend_helium.S"
    "${TRIMIX_ROOT}/managed_components/lvgl__lvgl/src/draw/sw/blend/neon/lv_blend_neon.S"
    PROPERTIES HEADER_FILE_ONLY TRUE
)

add_subdirectory("${TRIMIX_ROOT}/managed_components/lvgl__lvgl" "${CMAKE_BINARY_DIR}/lvgl")
get_target_property(LVGL_SOURCES lvgl SOURCES)
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*lv_blend_(helium|neon)\\.S$")
set_target_properties(lvgl PROPERTIES SOURCES "${LVGL_SOURCES}")
target_include_directories(lvgl PUBLIC ${SDL2_INCLUDE_DIRS})
target_link_directories(lvgl PUBLIC ${SDL2_LIBRARY_DIRS})
target_compile_options(lvgl PUBLIC ${SDL2_CFLAGS_OTHER})
target_link_libraries(lvgl PUBLIC ${SDL2_LIBRARIES})

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
    "${TRIMIX_ROOT}/main/services/sd_log_core.cpp"
    "${TRIMIX_ROOT}/main/services/sd_log_files.cpp"
    "${TRIMIX_ROOT}/main/services/sd_log_format.cpp"
    "${TRIMIX_ROOT}/main/services/sd_log_service.cpp"
    "${TRIMIX_ROOT}/main/version.cpp"
    "${TRIMIX_ROOT}/main/analysis/analysis_calculator.cpp"
    "${TRIMIX_ROOT}/main/sensors/sensor_interface.cpp"
    "${TRIMIX_ROOT}/main/sensors/sensor_hardware.cpp"
    "${TRIMIX_ROOT}/main/sensors/ads122c04.cpp"
    "${TRIMIX_ROOT}/main/sensors/ze07_co.cpp"
    "${TRIMIX_ROOT}/main/sensors/co_qualification.cpp"
    "${TRIMIX_ROOT}/main/services/gas_calibration_core.cpp"
    "${TRIMIX_ROOT}/main/services/gas_calibration_journal.cpp"
    "${TRIMIX_ROOT}/main/services/gas_calibration_service.cpp"
    "${TRIMIX_ROOT}/main/services/oxygen_selection_core.cpp"
    "${TRIMIX_ROOT}/main/services/oxygen_selection_service.cpp"
    "${TRIMIX_ROOT}/main/services/blob_journal.cpp"
    "${TRIMIX_ROOT}/main/services/storage_service.cpp"
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
    "${CMAKE_CURRENT_LIST_DIR}/mocks/settings_service_mock.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/mocks/wifi_service_mock.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/mocks/battery_service_mock.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/mocks/backlight_service_mock.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/mocks/ota_service_mock.cpp"
)

add_library(trimix_host_app STATIC ${TRIMIX_UI_SOURCES})
target_compile_definitions(trimix_host_app PUBLIC TRIMIX_SIMULATOR=1)
target_include_directories(trimix_host_app PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/stubs"
    ${TRIMIX_COMMON_INCLUDE_DIRS}
)
target_link_libraries(trimix_host_app PUBLIC lvgl)

add_executable(trimix_simulator "${CMAKE_CURRENT_LIST_DIR}/src/main.cpp")
target_link_libraries(trimix_simulator PRIVATE trimix_host_app)

enable_testing()

add_executable(test_gas_calculator
    "${CMAKE_CURRENT_LIST_DIR}/tests/test_gas_calculator.cpp"
    "${TRIMIX_ROOT}/main/ui/screens/dive_planner/gas_calculator.cpp"
)
target_include_directories(test_gas_calculator PRIVATE
    "${TRIMIX_ROOT}/main/ui/screens/dive_planner"
)
add_test(NAME gas_calculator COMMAND test_gas_calculator)

add_executable(test_analysis_calculator
    "${TRIMIX_ROOT}/tests/test_analysis_calculator.cpp"
    "${TRIMIX_ROOT}/main/analysis/analysis_calculator.cpp"
)
target_include_directories(test_analysis_calculator PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/stubs"
    ${TRIMIX_COMMON_INCLUDE_DIRS}
)
add_test(NAME analysis_calculator COMMAND test_analysis_calculator)

add_executable(test_sensor_interface
    "${TRIMIX_ROOT}/tests/test_sensor_interface.cpp"
    "${TRIMIX_ROOT}/main/sensors/sensor_interface.cpp"
    "${TRIMIX_ROOT}/main/services/gas_calibration_core.cpp"
    "${TRIMIX_ROOT}/main/services/gas_calibration_journal.cpp"
    "${TRIMIX_ROOT}/main/services/gas_calibration_service.cpp"
    "${TRIMIX_ROOT}/main/services/oxygen_selection_core.cpp"
    "${TRIMIX_ROOT}/main/services/oxygen_selection_service.cpp"
    "${TRIMIX_ROOT}/main/services/blob_journal.cpp"
    "${TRIMIX_ROOT}/main/services/storage_service.cpp"
)
target_include_directories(test_sensor_interface PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/stubs"
    ${TRIMIX_COMMON_INCLUDE_DIRS}
)
add_test(NAME sensor_interface COMMAND test_sensor_interface)

add_executable(test_gas_acquisition
    "${TRIMIX_ROOT}/tests/test_gas_acquisition.cpp"
    "${TRIMIX_ROOT}/main/sensors/ads122c04.cpp"
    "${TRIMIX_ROOT}/main/sensors/ze07_co.cpp"
    "${TRIMIX_ROOT}/main/services/gas_calibration_core.cpp"
    "${TRIMIX_ROOT}/main/services/gas_calibration_journal.cpp"
)
target_include_directories(test_gas_acquisition PRIVATE "${CMAKE_CURRENT_LIST_DIR}/stubs" ${TRIMIX_COMMON_INCLUDE_DIRS})
add_test(NAME gas_acquisition COMMAND test_gas_acquisition)

add_executable(test_co_qualification "${TRIMIX_ROOT}/tests/test_co_qualification.cpp"
    "${TRIMIX_ROOT}/main/sensors/co_qualification.cpp" "${TRIMIX_ROOT}/main/sensors/ze07_co.cpp")
target_include_directories(test_co_qualification PRIVATE "${TRIMIX_ROOT}/main")
add_test(NAME co_qualification COMMAND test_co_qualification)
add_executable(test_co_hardware_worker "${TRIMIX_ROOT}/tests/sd_log_optional_stubs.cpp" "${TRIMIX_ROOT}/tests/test_co_hardware_worker.cpp"
    "${TRIMIX_ROOT}/main/sensors/sensor_hardware.cpp" "${TRIMIX_ROOT}/main/sensors/acquisition_engine.cpp"
    "${TRIMIX_ROOT}/main/sensors/ads122c04.cpp" "${TRIMIX_ROOT}/main/sensors/co_qualification.cpp"
    "${TRIMIX_ROOT}/main/sensors/ze07_co.cpp" "${TRIMIX_ROOT}/main/services/ota_core.cpp"
    "${TRIMIX_ROOT}/tests/third_party/cjson/cJSON.c")
target_compile_definitions(test_co_hardware_worker PRIVATE ESP_PLATFORM=1)
target_include_directories(test_co_hardware_worker PRIVATE "${TRIMIX_ROOT}/tests/co_worker_stubs"
    "${TRIMIX_ROOT}/tests/power_worker_stubs" "${TRIMIX_ROOT}/tests/backlight_stubs"
    "${TRIMIX_ROOT}/simulator/stubs" "${TRIMIX_ROOT}/tests/third_party/cjson" "${TRIMIX_ROOT}/main")
target_link_libraries(test_co_hardware_worker PRIVATE Threads::Threads)
add_test(NAME co_hardware_worker COMMAND test_co_hardware_worker)
add_test(NAME co_optional_uart_absent COMMAND test_co_hardware_worker missing-uart)

add_executable(test_analysis_history
    "${TRIMIX_ROOT}/tests/test_analysis_history.cpp"
    "${TRIMIX_ROOT}/main/services/analysis_history.cpp"
)
target_compile_definitions(test_analysis_history PRIVATE TRIMIX_SIMULATOR=1)
target_include_directories(test_analysis_history PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/stubs"
    ${TRIMIX_COMMON_INCLUDE_DIRS}
)
add_test(NAME analysis_history COMMAND test_analysis_history)

add_executable(test_cylinder_profiles
    "${TRIMIX_ROOT}/tests/test_cylinder_profiles.cpp"
    "${TRIMIX_ROOT}/main/services/cylinder_profiles.cpp"
    "${TRIMIX_ROOT}/main/services/mix_label_service.cpp"
    "${TRIMIX_ROOT}/main/analysis/analysis_calculator.cpp"
)
target_compile_definitions(test_cylinder_profiles PRIVATE TRIMIX_SIMULATOR=1)
target_include_directories(test_cylinder_profiles PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/stubs"
    ${TRIMIX_COMMON_INCLUDE_DIRS}
)
add_test(NAME cylinder_profiles COMMAND test_cylinder_profiles)

add_executable(test_ui_smoke "${CMAKE_CURRENT_LIST_DIR}/tests/test_ui_smoke.cpp")
target_link_libraries(test_ui_smoke PRIVATE trimix_host_app)
add_test(NAME ui_smoke COMMAND test_ui_smoke)

add_executable(test_oxygen_selection "${TRIMIX_ROOT}/tests/test_oxygen_selection.cpp" "${TRIMIX_ROOT}/main/services/oxygen_selection_core.cpp")
add_executable(test_ota_storage "${TRIMIX_ROOT}/tests/test_ota_storage.cpp"
    "${TRIMIX_ROOT}/main/services/ota_core.cpp" "${TRIMIX_ROOT}/main/services/blob_journal.cpp"
    "${TRIMIX_ROOT}/main/services/storage_service.cpp" "${TRIMIX_ROOT}/main/services/maintenance_service.cpp"
    "${TRIMIX_ROOT}/tests/third_party/cjson/cJSON.c")
target_include_directories(test_ota_storage PRIVATE "${TRIMIX_ROOT}/tests/third_party/cjson")
add_executable(test_power_environment "${TRIMIX_ROOT}/tests/test_power_environment.cpp"
    "${TRIMIX_ROOT}/main/sensors/power_monitor.cpp" "${TRIMIX_ROOT}/main/sensors/environment_monitor.cpp"
    "${TRIMIX_ROOT}/main/services/charge_profile.cpp"
    "${TRIMIX_ROOT}/main/third_party/bme280/bme280.c")
foreach(review_test test_oxygen_selection test_ota_storage test_power_environment)
    target_include_directories(${review_test} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/stubs" ${TRIMIX_COMMON_INCLUDE_DIRS})
    add_test(NAME ${review_test} COMMAND ${review_test})
endforeach()

add_executable(test_acquisition_engine "${TRIMIX_ROOT}/tests/test_acquisition_engine.cpp"
    "${TRIMIX_ROOT}/main/sensors/acquisition_engine.cpp" "${TRIMIX_ROOT}/main/sensors/ads122c04.cpp")
target_include_directories(test_acquisition_engine PRIVATE "${CMAKE_CURRENT_LIST_DIR}/stubs" ${TRIMIX_COMMON_INCLUDE_DIRS})
add_test(NAME acquisition_engine COMMAND test_acquisition_engine)

find_package(Threads REQUIRED)
add_executable(test_charge_standby "${TRIMIX_ROOT}/tests/test_charge_standby.cpp"
    "${TRIMIX_ROOT}/main/services/charge_profile.cpp" "${TRIMIX_ROOT}/main/services/charge_standby_policy.cpp"
    "${TRIMIX_ROOT}/main/sensors/power_monitor.cpp")
target_include_directories(test_charge_standby PRIVATE "${CMAKE_CURRENT_LIST_DIR}/stubs" ${TRIMIX_COMMON_INCLUDE_DIRS})
add_test(NAME charge_standby COMMAND test_charge_standby)
add_executable(test_usb_input_path "${TRIMIX_ROOT}/tests/test_usb_input_path.cpp"
    "${TRIMIX_ROOT}/main/sensors/power_monitor.cpp" "${TRIMIX_ROOT}/main/services/charge_profile.cpp")
target_include_directories(test_usb_input_path PRIVATE "${CMAKE_CURRENT_LIST_DIR}/stubs" ${TRIMIX_COMMON_INCLUDE_DIRS})
add_test(NAME usb_input_path COMMAND test_usb_input_path)
add_executable(test_backlight_standby "${TRIMIX_ROOT}/tests/test_backlight_standby.cpp"
    "${TRIMIX_ROOT}/main/services/backlight_service.cpp")
target_include_directories(test_backlight_standby PRIVATE "${TRIMIX_ROOT}/tests/backlight_stubs"
    "${CMAKE_CURRENT_LIST_DIR}/stubs" ${TRIMIX_COMMON_INCLUDE_DIRS})
target_link_libraries(test_backlight_standby PRIVATE Threads::Threads)
add_test(NAME backlight_standby COMMAND test_backlight_standby)
add_executable(test_power_worker_standby "${TRIMIX_ROOT}/tests/sd_log_optional_stubs.cpp" "${TRIMIX_ROOT}/tests/test_power_worker_standby.cpp"
    "${TRIMIX_ROOT}/main/services/system_power.cpp" "${TRIMIX_ROOT}/main/services/charge_standby_policy.cpp"
    "${TRIMIX_ROOT}/main/services/maintenance_service.cpp" "${TRIMIX_ROOT}/main/services/backlight_service.cpp"
    "${TRIMIX_ROOT}/main/sensors/power_monitor.cpp" "${TRIMIX_ROOT}/main/sensors/usb_input_policy.cpp"
    "${TRIMIX_ROOT}/main/sensors/usb_source_monitor.cpp" "${TRIMIX_ROOT}/main/sensors/bc12_monitor.cpp")
target_compile_definitions(test_power_worker_standby PRIVATE ESP_PLATFORM=1)
target_include_directories(test_power_worker_standby PRIVATE "${TRIMIX_ROOT}/tests/power_worker_stubs"
    "${TRIMIX_ROOT}/tests/backlight_stubs" "${CMAKE_CURRENT_LIST_DIR}/stubs" ${TRIMIX_COMMON_INCLUDE_DIRS})
target_link_libraries(test_power_worker_standby PRIVATE Threads::Threads)
foreach(power_case normal maintenance maintenance-last bus-loss probation charger-irq kill-retry
        hiz-requalify hiz-ignored hiz-maintenance hiz-maintenance-wait hiz-delay hiz-timeout hiz-adc-ignored
        legacy-dcp legacy-cdp legacy-gap legacy-detach legacy-replug legacy-reset
        legacy-before-detection legacy-during-detection legacy-after-collection)
    add_test(NAME power_worker_${power_case} COMMAND test_power_worker_standby ${power_case})
endforeach()
add_executable(test_storage_maintenance_race "${TRIMIX_ROOT}/tests/test_storage_maintenance_race.cpp"
    "${TRIMIX_ROOT}/main/services/storage_service.cpp" "${TRIMIX_ROOT}/main/services/blob_journal.cpp"
    "${TRIMIX_ROOT}/main/services/maintenance_service.cpp")
target_include_directories(test_storage_maintenance_race PRIVATE "${CMAKE_CURRENT_LIST_DIR}/stubs" ${TRIMIX_COMMON_INCLUDE_DIRS})
target_link_libraries(test_storage_maintenance_race PRIVATE Threads::Threads)
add_test(NAME storage_maintenance_race COMMAND test_storage_maintenance_race)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME system_review_tools COMMAND "${Python3_EXECUTABLE}" "${TRIMIX_ROOT}/tests/test_system_review_tools.py")

add_executable(test_usb_source "${TRIMIX_ROOT}/tests/test_usb_source.cpp" "${TRIMIX_ROOT}/main/sensors/usb_source_monitor.cpp")
target_include_directories(test_usb_source PRIVATE "${CMAKE_CURRENT_LIST_DIR}/stubs" ${TRIMIX_COMMON_INCLUDE_DIRS})
add_test(NAME usb_source COMMAND test_usb_source)

add_executable(test_bc12_source "${TRIMIX_ROOT}/tests/test_bc12_source.cpp" "${TRIMIX_ROOT}/main/sensors/bc12_monitor.cpp")
target_include_directories(test_bc12_source PRIVATE "${CMAKE_CURRENT_LIST_DIR}/stubs" ${TRIMIX_COMMON_INCLUDE_DIRS})
add_test(NAME bc12_source COMMAND test_bc12_source)
add_executable(test_usb_input_policy "${TRIMIX_ROOT}/tests/test_usb_input_policy.cpp" "${TRIMIX_ROOT}/main/sensors/usb_input_policy.cpp")
target_include_directories(test_usb_input_policy PRIVATE "${CMAKE_CURRENT_LIST_DIR}/stubs" ${TRIMIX_COMMON_INCLUDE_DIRS})
add_test(NAME usb_input_policy COMMAND test_usb_input_policy)

# Compile the actual ESP storage branch with a fault-injectable low-level NVS
# adapter. Each startup state runs in a fresh process, matching singleton init.
add_executable(test_esp_storage_adapter "${TRIMIX_ROOT}/tests/test_esp_storage_adapter.cpp"
    "${TRIMIX_ROOT}/main/services/storage_service.cpp"
    "${TRIMIX_ROOT}/main/services/blob_journal.cpp")
target_compile_definitions(test_esp_storage_adapter PRIVATE ESP_PLATFORM=1)
target_include_directories(test_esp_storage_adapter PRIVATE
    "${TRIMIX_ROOT}/tests/esp_storage_stubs" "${TRIMIX_ROOT}/main")
target_link_libraries(test_esp_storage_adapter PRIVATE Threads::Threads)
foreach(storage_case no-free-pages new-nvs-version pending-rollback accepted-upgrade valid-image)
    add_test(NAME esp_storage_${storage_case} COMMAND test_esp_storage_adapter ${storage_case})
endforeach()

add_executable(test_sd_log "${TRIMIX_ROOT}/main/services/sd_log_format.cpp" "${TRIMIX_ROOT}/tests/test_sd_log.cpp" "${TRIMIX_ROOT}/main/services/sd_log_core.cpp" "${TRIMIX_ROOT}/main/services/sd_log_files.cpp")
target_include_directories(test_sd_log PRIVATE "${TRIMIX_ROOT}/main" "${TRIMIX_ROOT}/simulator/stubs")
target_link_libraries(test_sd_log PRIVATE Threads::Threads)
add_test(NAME sd_logging COMMAND test_sd_log)

add_executable(test_sd_maintenance "${TRIMIX_ROOT}/tests/test_sd_maintenance.cpp" "${TRIMIX_ROOT}/main/services/sd_log_core.cpp" "${TRIMIX_ROOT}/main/services/maintenance_service.cpp" "${TRIMIX_ROOT}/main/services/storage_service.cpp" "${TRIMIX_ROOT}/main/services/blob_journal.cpp")
target_include_directories(test_sd_maintenance PRIVATE "${TRIMIX_ROOT}/main")
target_link_libraries(test_sd_maintenance PRIVATE Threads::Threads)
add_test(NAME sd_maintenance COMMAND test_sd_maintenance)

add_test(NAME guition_sdmmc COMMAND "${Python3_EXECUTABLE}" "${TRIMIX_ROOT}/tests/test_guition_sdmmc.py")
if(CMAKE_C_FLAGS MATCHES "fsanitize=address")
    set_tests_properties(guition_sdmmc PROPERTIES ENVIRONMENT "TRIMIX_SDMMC_SANITIZE=address,undefined;ASAN_OPTIONS=detect_leaks=0")
endif()
