/* Shared SDMMC controller ownership for the Guition P4 card and C6 SDIO.
 * Pinned to ESP-IDF 5.5.4; the checked Hosted patch is part of this contract. */
#include "guition_sdmmc.h"

#include <string.h>
#include "freertos/semphr.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "esp_idf_version.h"

#if !CONFIG_IDF_TARGET_ESP32P4 || ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5, 5, 4)
#error "Guition SDMMC arbitration must be reviewed for this target/IDF version"
#endif

#define CARD_SLOT 0
#define HOSTED_SLOT 1
#define LOCK_WAIT pdMS_TO_TICKS(250)

static StaticSemaphore_t s_mutex_storage;
static SemaphoreHandle_t s_mutex;
static portMUX_TYPE s_init_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_hosted;
static bool s_card_attempt;
static bool s_partial_slot;
static bool s_card_residual;
static esp_err_t s_card_deinit_error;
static sdmmc_card_t *s_card;
static sd_pwr_ctrl_handle_t s_card_power;
static char s_path[64];

esp_err_t guition_sdmmc_lock(TickType_t ticks)
{
    portENTER_CRITICAL(&s_init_mux);
    if (!s_mutex) {
        s_mutex = xSemaphoreCreateRecursiveMutexStatic(&s_mutex_storage);
    }
    portEXIT_CRITICAL(&s_init_mux);
    if (!s_mutex) return ESP_ERR_NO_MEM;
    return xSemaphoreTakeRecursive(s_mutex, ticks) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

void guition_sdmmc_unlock(void)
{
    configASSERT(s_mutex);
    configASSERT(xSemaphoreGiveRecursive(s_mutex) == pdTRUE);
}

/* Must hold the guard. Never decrement IDF's slot count for an init that failed
 * before it incremented that count (IDF deinit_slot itself does not check this).
 * A partial init with another owner alive is quarantined until that owner exits. */
static esp_err_t release_slot(int slot, int other_slots)
{
    sdmmc_host_state_t state;
    esp_err_t err = sdmmc_host_get_state(&state);
    if (err != ESP_OK) return err;
    if (!state.host_initialized) return other_slots ? ESP_ERR_INVALID_STATE : ESP_OK;
    if (state.num_of_init_slots == other_slots + 1) {
        err = sdmmc_host_deinit_slot(slot);
    } else if (state.num_of_init_slots == 0 && other_slots == 0) {
        /* Only this failed attempt owns the initialized, slot-less controller. */
        err = sdmmc_host_deinit();
    } else if (state.num_of_init_slots == other_slots && other_slots != 0) {
        s_partial_slot = true;
        return ESP_OK;
    } else {
        return ESP_ERR_INVALID_STATE;
    }
    if (err == ESP_OK && other_slots == 0) s_partial_slot = false;
    return err;
}

static esp_err_t check_owned_state(void)
{
    sdmmc_host_state_t state;
    esp_err_t err = sdmmc_host_get_state(&state);
    if (err != ESP_OK) return err;
    if (s_card_residual) return ESP_ERR_INVALID_STATE;
    const int owned = (s_hosted ? 1 : 0) + (s_card ? 1 : 0);
    return state.num_of_init_slots == owned &&
           state.host_initialized == (owned != 0) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t guition_sdmmc_hosted_acquire(int slot, const sdmmc_slot_config_t *cfg)
{
    if (!cfg || slot != HOSTED_SLOT || cfg->width != 4 ||
        cfg->clk != 18 || cfg->cmd != 19 || cfg->d0 != 14 ||
        cfg->d1 != 15 || cfg->d2 != 16 || cfg->d3 != 17 ||
        cfg->cd != GPIO_NUM_NC || cfg->wp != GPIO_NUM_NC || cfg->flags != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = guition_sdmmc_lock(LOCK_WAIT);
    if (err != ESP_OK) return err;
    if (s_hosted || s_partial_slot || s_card_attempt) {
        err = ESP_ERR_INVALID_STATE;
        goto done;
    }
    err = check_owned_state();
    if (err != ESP_OK) goto done;
    err = sdmmc_host_init();
    if (err != ESP_OK) goto done;
    err = sdmmc_host_init_slot(slot, cfg);
    if (err == ESP_OK) {
        s_hosted = true;
    } else {
        esp_err_t cleanup = release_slot(slot, s_card ? 1 : 0);
        if (cleanup != ESP_OK) err = cleanup;
    }
done:
    guition_sdmmc_unlock();
    return err;
}

esp_err_t guition_sdmmc_hosted_release(void)
{
    esp_err_t err = guition_sdmmc_lock(LOCK_WAIT);
    if (err != ESP_OK) return err;
    if (!s_hosted) {
        err = ESP_ERR_INVALID_STATE;
    } else {
        err = release_slot(HOSTED_SLOT, (s_card || s_card_residual) ? 1 : 0);
        if (err == ESP_OK) s_hosted = false;
    }
    guition_sdmmc_unlock();
    return err;
}

static bool card_slot_live(int slot)
{
    return slot == CARD_SLOT && (s_card_attempt || s_card != NULL);
}

/* FATFS retains this host table in its card. Every callback that accesses the
 * controller uses the same mutex as Hosted, including configuration and reads.
 * DMA-buffer allocation stays with IDF; no long-lived global lock during idle. */
#define GUARDED_ERR(name, args, call) \
    static esp_err_t name args { \
        esp_err_t err = guition_sdmmc_lock(LOCK_WAIT); \
        if (err != ESP_OK) return err; \
        err = card_slot_live(slot) ? (call) : ESP_ERR_INVALID_STATE; \
        guition_sdmmc_unlock(); \
        return err; \
    }

GUARDED_ERR(card_width, (int slot, size_t width), sdmmc_host_set_bus_width(slot, width))
GUARDED_ERR(card_ddr, (int slot, bool enabled), sdmmc_host_set_bus_ddr_mode(slot, enabled))
GUARDED_ERR(card_clock, (int slot, uint32_t freq), sdmmc_host_set_card_clk(slot, freq))
GUARDED_ERR(card_cont_clock, (int slot, bool enabled), sdmmc_host_set_cclk_always_on(slot, enabled))
GUARDED_ERR(card_transaction, (int slot, sdmmc_command_t *cmd), sdmmc_host_do_transaction(slot, cmd))
GUARDED_ERR(card_real_freq, (int slot, int *freq), sdmmc_host_get_real_freq(slot, freq))
GUARDED_ERR(card_delay, (int slot, sdmmc_delay_phase_t phase), sdmmc_host_set_input_delay(slot, phase))
GUARDED_ERR(card_is_uhs, (int slot, bool *uhs), sdmmc_host_is_slot_set_to_uhs1(slot, uhs))

static size_t card_get_width(int slot)
{
    if (guition_sdmmc_lock(LOCK_WAIT) != ESP_OK) return 0;
    size_t width = card_slot_live(slot) ? sdmmc_host_get_slot_width(slot) : 0;
    guition_sdmmc_unlock();
    return width;
}

static bool card_alignment(int slot, const void *buffer, size_t size)
{
    if (guition_sdmmc_lock(LOCK_WAIT) != ESP_OK) return false;
    bool aligned = card_slot_live(slot) && sdmmc_host_check_buffer_alignment(slot, buffer, size);
    guition_sdmmc_unlock();
    return aligned;
}

static esp_err_t card_host_init(void)
{
    /* Called only inside the guarded mount; VFS then calls raw init_slot. */
    return s_card_attempt ? sdmmc_host_init() : ESP_ERR_INVALID_STATE;
}

static esp_err_t card_host_deinit(int slot)
{
    esp_err_t err = guition_sdmmc_lock(LOCK_WAIT);
    if (err != ESP_OK) return err;
    if (!card_slot_live(slot)) {
        err = ESP_ERR_INVALID_STATE;
    } else {
        err = release_slot(CARD_SLOT, s_hosted ? 1 : 0);
        /* VFS ignores this callback's result and frees its card anyway. Card
         * memory lifetime must never masquerade as residual driver ownership. */
        s_card_deinit_error = err;
        s_card_residual = err != ESP_OK;
        s_card_attempt = false;
        s_card = NULL;
    }
    guition_sdmmc_unlock();
    return err;
}

static esp_err_t release_card_power(void)
{
    if (!s_card_power) return ESP_OK;
    esp_err_t err = sd_pwr_ctrl_del_on_chip_ldo(s_card_power);
    if (err == ESP_OK) s_card_power = NULL;
    return err;
}

esp_err_t guition_sdmmc_mount(const char *path,
                            const esp_vfs_fat_mount_config_t *config,
                            sdmmc_card_t **card)
{
    if (!path || !config || !card || path[0] != '/' ||
        strlen(path) >= sizeof(s_path) || config->format_if_mount_failed) {
        return ESP_ERR_INVALID_ARG;
    }
    *card = NULL;
    esp_err_t err = guition_sdmmc_lock(LOCK_WAIT);
    if (err != ESP_OK) return err;
    if (s_card || s_card_attempt || s_partial_slot || s_card_residual) {
        err = ESP_ERR_INVALID_STATE;
        goto done;
    }
    err = check_owned_state();
    if (err != ESP_OK) goto done;
    /* Retain a failed-to-release handle rather than acquiring LDO4 twice. */
    err = release_card_power();
    if (err != ESP_OK) goto done;
    sd_pwr_ctrl_ldo_config_t ldo = { .ldo_chan_id = 4 };
    err = sd_pwr_ctrl_new_on_chip_ldo(&ldo, &s_card_power);
    if (err != ESP_OK) goto done;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = CARD_SLOT;
    host.flags = SDMMC_HOST_FLAG_4BIT | SDMMC_HOST_FLAG_1BIT | SDMMC_HOST_FLAG_DEINIT_ARG;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    host.command_timeout_ms = 1000;
    host.pwr_ctrl_handle = s_card_power;
    host.init = card_host_init;
    host.deinit_p = card_host_deinit;
    host.set_bus_width = card_width;
    host.get_bus_width = card_get_width;
    host.set_bus_ddr_mode = card_ddr;
    host.set_card_clk = card_clock;
    host.set_cclk_always_on = card_cont_clock;
    host.do_transaction = card_transaction;
    host.get_real_freq = card_real_freq;
    host.set_input_delay = card_delay;
    host.check_buffer_alignment = card_alignment;
    host.is_slot_set_to_uhs1 = card_is_uhs;
    /* Removable memory only: SDIO functions/interrupts are not accepted. */
    host.io_int_enable = NULL;
    host.io_int_wait = NULL;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = 43; slot.cmd = 44;
    slot.d0 = 39; slot.d1 = 40; slot.d2 = 41; slot.d3 = 42;
    slot.d4 = slot.d5 = slot.d6 = slot.d7 = GPIO_NUM_NC;
    slot.cd = slot.wp = GPIO_NUM_NC;
    slot.flags = 0; /* External pull-ups required; never substitute weak internals. */
    s_card_attempt = true;
    s_card_deinit_error = ESP_OK;
    err = esp_vfs_fat_sdmmc_mount(path, &host, &slot, config, card);
    if (err == ESP_OK) {
        s_card = *card;
        memcpy(s_path, path, strlen(path) + 1);
    } else {
        *card = NULL; /* VFS may have assigned and then freed its out pointer. */
        esp_err_t cleanup = s_card_residual ? s_card_deinit_error : release_card_power();
        if (cleanup != ESP_OK) err = cleanup;
    }
    s_card_attempt = false;
done:
    guition_sdmmc_unlock();
    return err;
}

esp_err_t guition_sdmmc_unmount(const char *path, sdmmc_card_t *card)
{
    if (!path || !card) return ESP_ERR_INVALID_ARG;
    esp_err_t err = guition_sdmmc_lock(LOCK_WAIT);
    if (err != ESP_OK) return err;
    if (s_card != card || strcmp(path, s_path) != 0) {
        err = ESP_ERR_INVALID_STATE;
    } else {
        s_card_deinit_error = ESP_OK;
        err = esp_vfs_fat_sdcard_unmount(path, card);
        if (!s_card) {
            s_path[0] = '\0';
            esp_err_t cleanup = s_card_residual ? s_card_deinit_error : release_card_power();
            if (cleanup != ESP_OK) err = cleanup;
        }
    }
    guition_sdmmc_unlock();
    return err;
}

bool guition_sdmmc_is_mounted(const sdmmc_card_t *card)
{
    /* Conservative on contention: do not let a caller discard a live handle. */
    if (guition_sdmmc_lock(LOCK_WAIT) != ESP_OK) return card != NULL;
    bool mounted = card != NULL && s_card == card;
    guition_sdmmc_unlock();
    return mounted;
}
