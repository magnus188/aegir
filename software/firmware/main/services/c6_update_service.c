#include "c6_update_service.h"
#include "c6_recovery_checks.h"

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_hosted.h"
#include "esp_hosted_api_types.h"
#include "esp_hosted_ota.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"

static const char *TAG = "C6_UPDATE";
static const char *PARTITION_LABEL = "slave_fw";
static const char *EXPECTED_PROJECT = TRIMIX_C6_IMAGE_PROJECT;
static const char *EXPECTED_VERSION = TRIMIX_C6_IMAGE_VERSION;

enum {
    OTA_CHUNK_SIZE = 1500,
    HASH_CHUNK_SIZE = 4096,
    EXPECTED_IMAGE_SIZE = TRIMIX_C6_IMAGE_BYTES,
};

static const uint8_t EXPECTED_SHA256[32] = {
    0x69, 0x45, 0x38, 0xb5, 0x63, 0xe9, 0xf0, 0x97,
    0x36, 0xca, 0xa1, 0x6a, 0x37, 0xe7, 0xfd, 0xd6,
    0x01, 0x84, 0x3f, 0x70, 0xb5, 0x90, 0x0e, 0xfe,
    0xf6, 0x95, 0x54, 0xf6, 0x87, 0xbf, 0x14, 0x71,
};

static bool version_is_at_least_2_6(const esp_hosted_coprocessor_fwver_t *version)
{
    return c6_recovery_needs_activation(version->major1, version->minor1);
}

static esp_err_t verify_staged_image(const esp_partition_t *partition)
{
    if (partition->size < EXPECTED_IMAGE_SIZE) {
        ESP_LOGE(TAG, "The staged C6 partition is too small");
        return ESP_ERR_INVALID_SIZE;
    }

    esp_image_header_t image_header = {0};
    esp_err_t err = esp_partition_read(partition, 0, &image_header, sizeof(image_header));
    if (err != ESP_OK || image_header.magic != ESP_IMAGE_HEADER_MAGIC) {
        ESP_LOGE(TAG, "The staged C6 image header is invalid");
        return err == ESP_OK ? ESP_ERR_INVALID_ARG : err;
    }

    esp_app_desc_t app_desc = {0};
    const size_t app_desc_offset = sizeof(esp_image_header_t) +
                                   sizeof(esp_image_segment_header_t);
    err = esp_partition_read(partition, app_desc_offset, &app_desc, sizeof(app_desc));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not read the staged C6 image description: %s",
                 esp_err_to_name(err));
        return err;
    }

    if (!c6_recovery_descriptor_valid(partition->size, image_header.chip_id,
        app_desc.project_name, sizeof(app_desc.project_name), app_desc.version, sizeof(app_desc.version))) {
        ESP_LOGE(TAG, "Refusing C6 image project='%s' version='%s' (expected %s %s)",
                 "invalid-or-unexpected", "invalid-or-unexpected", EXPECTED_PROJECT, EXPECTED_VERSION);
        return ESP_ERR_INVALID_VERSION;
    }

    uint8_t chunk[HASH_CHUNK_SIZE];
    uint8_t digest[32];
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    if (mbedtls_sha256_starts(&sha, 0) != 0) {
        mbedtls_sha256_free(&sha);
        return ESP_FAIL;
    }

    for (uint32_t offset = 0; offset < EXPECTED_IMAGE_SIZE;) {
        const uint32_t remaining = EXPECTED_IMAGE_SIZE - offset;
        const uint32_t length = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
        err = esp_partition_read(partition, offset, chunk, length);
        if (err != ESP_OK || mbedtls_sha256_update(&sha, chunk, length) != 0) {
            mbedtls_sha256_free(&sha);
            return err == ESP_OK ? ESP_FAIL : err;
        }
        offset += length;
    }

    const int sha_result = mbedtls_sha256_finish(&sha, digest);
    mbedtls_sha256_free(&sha);
    if (sha_result != 0 || memcmp(digest, EXPECTED_SHA256, sizeof(digest)) != 0) {
        ESP_LOGE(TAG, "The staged C6 image SHA-256 does not match the approved Guition image");
        return ESP_ERR_INVALID_CRC;
    }

    ESP_LOGI(TAG, "Validated %s %s (%u bytes, SHA-256 matched)", app_desc.project_name,
             app_desc.version, EXPECTED_IMAGE_SIZE);
    return ESP_OK;
}

static esp_err_t stream_update(const esp_partition_t *partition, uint32_t image_len)
{
    uint8_t chunk[OTA_CHUNK_SIZE];
    esp_err_t err = esp_hosted_slave_ota_begin();
    if (err != ESP_OK) {
        return err;
    }

    for (uint32_t offset = 0; offset < image_len;) {
        const uint32_t remaining = image_len - offset;
        const uint32_t length = remaining < sizeof(chunk) ? remaining : sizeof(chunk);

        err = esp_partition_read(partition, offset, chunk, length);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "C6 image read failed at offset %" PRIu32 ": %s", offset,
                     esp_err_to_name(err));
            esp_hosted_slave_ota_end();
            return err;
        }

        err = esp_hosted_slave_ota_write(chunk, length);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "C6 OTA write failed at offset %" PRIu32 ": %s", offset,
                     esp_err_to_name(err));
            esp_hosted_slave_ota_end();
            return err;
        }

        offset += length;
        if ((offset % (150 * OTA_CHUNK_SIZE)) < OTA_CHUNK_SIZE || offset == image_len) {
            ESP_LOGI(TAG, "C6 update progress: %" PRIu32 "/%" PRIu32 " bytes", offset,
                     image_len);
        }
    }

    return esp_hosted_slave_ota_end();
}

esp_err_t c6_update_service_run(void)
{
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, PARTITION_LABEL);
    if (partition == NULL) {
        ESP_LOGE(TAG, "Partition '%s' was not found", PARTITION_LABEL);
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = verify_staged_image(partition);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "Connecting to the Guition ESP32-C6");
    err = esp_hosted_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP-Hosted initialization failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_hosted_connect_to_slave();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not connect to the ESP32-C6: %s", esp_err_to_name(err));
        return err;
    }

    esp_hosted_coprocessor_fwver_t current = {0};
    err = esp_hosted_get_coprocessor_fwversion(&current);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not read the ESP32-C6 firmware version: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "ESP32-C6 is running %" PRIu32 ".%" PRIu32 ".%" PRIu32,
             current.major1, current.minor1, current.patch1);
    if (current.major1 == 2 && current.minor1 == 12 && current.patch1 == 9) {
        ESP_LOGI(TAG, "ESP32-C6 firmware is already current");
        return ESP_OK;
    }

    const bool activation_supported = version_is_at_least_2_6(&current);
    ESP_LOGW(TAG, "Updating ESP32-C6 to %s; keep USB power connected", EXPECTED_VERSION);
    err = stream_update(partition, EXPECTED_IMAGE_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP32-C6 update failed: %s", esp_err_to_name(err));
        return err;
    }

    if (activation_supported) {
        err = esp_hosted_slave_ota_activate();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ESP32-C6 activation failed: %s", esp_err_to_name(err));
            return err;
        }
    }

    ESP_LOGI(TAG, "ESP32-C6 update complete; restarting the display controller");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}
