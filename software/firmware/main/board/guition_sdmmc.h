#pragma once

#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* JC4880P443 candidate wiring; see patches/esp_hosted/README.md for evidence
 * and the remaining physical pin/power qualification. No format or hotplug. */
esp_err_t guition_sdmmc_mount(const char *path,
                            const esp_vfs_fat_mount_config_t *config,
                            sdmmc_card_t **card);
/* Caller closes every file and stops filesystem users before unmount. */
esp_err_t guition_sdmmc_unmount(const char *path, sdmmc_card_t *card);
/* IDF may free the card before reporting a VFS unregister error. */
bool guition_sdmmc_is_mounted(const sdmmc_card_t *card);

/* Patched ESP-Hosted uses the SAME recursive guard. Teardown holds it before
 * cancelling workers, so no cancelled worker can strand this mutex. */
esp_err_t guition_sdmmc_lock(TickType_t wait_ticks);
void guition_sdmmc_unlock(void);
esp_err_t guition_sdmmc_hosted_acquire(int slot,
                                    const sdmmc_slot_config_t *config);
esp_err_t guition_sdmmc_hosted_release(void);

#ifdef __cplusplus
}
#endif
