#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_TIMEOUT 0x107
#define CONFIG_IDF_TARGET_ESP32P4 1
#define ESP_IDF_VERSION_VAL(a,b,c) ((a)*10000+(b)*100+(c))
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(5,5,4)
#define GPIO_NUM_NC -1
#define SDMMC_HOST_FLAG_4BIT 2
#define SDMMC_HOST_FLAG_1BIT 1
#define SDMMC_HOST_FLAG_DEINIT_ARG 32
#define SDMMC_FREQ_DEFAULT 20000
#define SDMMC_HOST_DEFAULT() {0}
#define SDMMC_SLOT_CONFIG_DEFAULT() {.cd=-1,.wp=-1}
typedef uint32_t TickType_t;
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(x) (x)
#define pdTRUE 1
#define configASSERT(x) assert(x)
typedef pthread_mutex_t StaticSemaphore_t;
typedef pthread_mutex_t *SemaphoreHandle_t;
typedef pthread_mutex_t portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED PTHREAD_MUTEX_INITIALIZER
#define portENTER_CRITICAL(m) pthread_mutex_lock(m)
#define portEXIT_CRITICAL(m) pthread_mutex_unlock(m)
SemaphoreHandle_t xSemaphoreCreateRecursiveMutexStatic(StaticSemaphore_t *);
int xSemaphoreTakeRecursive(SemaphoreHandle_t, TickType_t);
int xSemaphoreGiveRecursive(SemaphoreHandle_t);
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t);
typedef int sdmmc_delay_phase_t;
typedef struct {int dummy;} sdmmc_command_t;
typedef void *sd_pwr_ctrl_handle_t;
typedef struct {int ldo_chan_id;} sd_pwr_ctrl_ldo_config_t;
typedef struct {
 uint32_t flags; int slot,max_freq_khz; int command_timeout_ms;
 sd_pwr_ctrl_handle_t pwr_ctrl_handle;
 esp_err_t (*init)(void); esp_err_t (*deinit_p)(int);
 esp_err_t (*set_bus_width)(int,size_t); size_t (*get_bus_width)(int);
 esp_err_t (*set_bus_ddr_mode)(int,bool); esp_err_t (*set_card_clk)(int,uint32_t);
 esp_err_t (*set_cclk_always_on)(int,bool);
 esp_err_t (*do_transaction)(int,sdmmc_command_t*);
 esp_err_t (*get_real_freq)(int,int*); esp_err_t (*set_input_delay)(int,sdmmc_delay_phase_t);
 bool (*check_buffer_alignment)(int,const void*,size_t);
 esp_err_t (*is_slot_set_to_uhs1)(int,bool*);
 esp_err_t (*io_int_enable)(int); esp_err_t (*io_int_wait)(int,TickType_t);
} sdmmc_host_t;
typedef struct {sdmmc_host_t host;} sdmmc_card_t;
typedef struct {int clk,cmd,d0,d1,d2,d3,d4,d5,d6,d7,cd,wp; unsigned width,flags;} sdmmc_slot_config_t;
typedef struct {bool host_initialized; int num_of_init_slots;} sdmmc_host_state_t;
typedef struct {bool format_if_mount_failed; int max_files; size_t allocation_unit_size;} esp_vfs_fat_mount_config_t;
esp_err_t sdmmc_host_init(void);
esp_err_t sdmmc_host_init_slot(int,const sdmmc_slot_config_t*);
esp_err_t sdmmc_host_deinit_slot(int);
esp_err_t sdmmc_host_deinit(void);
esp_err_t sdmmc_host_get_state(sdmmc_host_state_t*);
esp_err_t sdmmc_host_set_bus_width(int,size_t);
size_t sdmmc_host_get_slot_width(int);
esp_err_t sdmmc_host_set_bus_ddr_mode(int,bool);
esp_err_t sdmmc_host_set_card_clk(int,uint32_t);
esp_err_t sdmmc_host_set_cclk_always_on(int,bool);
esp_err_t sdmmc_host_do_transaction(int,sdmmc_command_t*);
esp_err_t sdmmc_host_get_real_freq(int,int*);
esp_err_t sdmmc_host_set_input_delay(int,sdmmc_delay_phase_t);
bool sdmmc_host_check_buffer_alignment(int,const void*,size_t);
esp_err_t sdmmc_host_is_slot_set_to_uhs1(int,bool*);
esp_err_t sd_pwr_ctrl_new_on_chip_ldo(const sd_pwr_ctrl_ldo_config_t*,sd_pwr_ctrl_handle_t*);
esp_err_t sd_pwr_ctrl_del_on_chip_ldo(sd_pwr_ctrl_handle_t);
esp_err_t esp_vfs_fat_sdmmc_mount(const char*,const sdmmc_host_t*,const void*,const esp_vfs_fat_mount_config_t*,sdmmc_card_t**);
esp_err_t esp_vfs_fat_sdcard_unmount(const char*,sdmmc_card_t*);
