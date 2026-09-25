# Include after project(...): IDF has resolved dependencies and created targets.
# Original managed sources stay pristine. Every configure checks all preimages.
if(CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE)
    if(NOT IDF_TARGET STREQUAL "esp32p4")
        message(FATAL_ERROR "Guition SDMMC patch requires ESP32-P4")
    endif()
    idf_component_get_property(_guition_hosted_lib espressif__esp_hosted COMPONENT_LIB)
    idf_component_get_property(_guition_hosted_dir espressif__esp_hosted COMPONENT_DIR)
    set(_guition_patch_dir "${CMAKE_BINARY_DIR}/guition-hosted-sdmmc")
    execute_process(COMMAND "${PYTHON}" "${PROJECT_DIR}/scripts/apply_hosted_sdmmc_patch.py"
        --source "${_guition_hosted_dir}" --output "${_guition_patch_dir}"
        RESULT_VARIABLE _guition_patch_result)
    if(NOT _guition_patch_result EQUAL 0)
        message(FATAL_ERROR "Checked Guition SDMMC patch failed; refusing unarbitrated Hosted build")
    endif()
    set(_guition_replacements
        host/port/esp/freertos/src/port_esp_hosted_host_sdio.c
        host/drivers/transport/sdio/sdio_drv.c
        host/drivers/transport/transport_drv.c
        host/api/src/esp_hosted_api.c)
    get_target_property(_guition_sources ${_guition_hosted_lib} SOURCES)
    foreach(_relative IN LISTS _guition_replacements)
        set(_matches 0)
        set(_retained)
        foreach(_source IN LISTS _guition_sources)
            get_filename_component(_absolute "${_source}" ABSOLUTE BASE_DIR "${_guition_hosted_dir}")
            if(_absolute STREQUAL "${_guition_hosted_dir}/${_relative}")
                math(EXPR _matches "${_matches} + 1")
            else()
                list(APPEND _retained "${_source}")
            endif()
        endforeach()
        if(NOT _matches EQUAL 1)
            message(FATAL_ERROR "Expected exactly one Hosted source entry for ${_relative}; got ${_matches}")
        endif()
        list(APPEND _retained "${_guition_patch_dir}/${_relative}")
        set(_guition_sources "${_retained}")
    endforeach()
    set_property(TARGET ${_guition_hosted_lib} PROPERTY SOURCES "${_guition_sources}")
    target_include_directories(${_guition_hosted_lib} PRIVATE "${PROJECT_DIR}/main")
    target_link_libraries(${_guition_hosted_lib} PRIVATE idf::fatfs)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${PROJECT_DIR}/scripts/apply_hosted_sdmmc_patch.py"
        "${PROJECT_DIR}/patches/esp_hosted/manifest.json"
        "${PROJECT_DIR}/patches/esp_hosted/2.12.9-guition-sdmmc.patch")
endif()
