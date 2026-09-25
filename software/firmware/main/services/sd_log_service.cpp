#include "sd_log_service.h"
#include "sd_log_files.h"
#include "sd_log_format.h"
#include "version.h"
#include <atomic>
#include <cmath>
#include <cstdio>

#if defined(ESP_PLATFORM) && !defined(TRIMIX_SIMULATOR)
#include "board/guition_sdmmc.h"
#include <esp_random.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace {
uint64_t uptime() { return static_cast<uint64_t>(esp_timer_get_time()/1000); }
class Card final : public sd_log::Volume {
    sdmmc_card_t *card_=nullptr;
    char error_[96]="Card not mounted";
public:
    bool mount(uint64_t &capacity) override {
        if(card_) {
            guition_sdmmc_unmount("/sdcard",card_);
            if(guition_sdmmc_is_mounted(card_)) {
                std::snprintf(error_,sizeof(error_),"SD controller still owns previous mount; retry later");return false;
            }
            card_=nullptr;
        }
        esp_vfs_fat_mount_config_t cfg{};
        cfg.format_if_mount_failed=false; cfg.max_files=3; cfg.allocation_unit_size=16384;
        const auto result=guition_sdmmc_mount("/sdcard",&cfg,&card_);
        if(result!=ESP_OK) {
            std::snprintf(error_,sizeof(error_),"Card mount %s; no formatting attempted",esp_err_to_name(result)); return false;
        }
        capacity=static_cast<uint64_t>(card_->csd.capacity)*card_->csd.sector_size; return true;
    }
    bool unmount() override {
        if(card_) {
            const auto result=guition_sdmmc_unmount("/sdcard",card_);
            if(!guition_sdmmc_is_mounted(card_))card_=nullptr;
            if(result!=ESP_OK || card_) {std::snprintf(error_,sizeof(error_),"SD unmount %s; removal not confirmed",esp_err_to_name(result));return false;}
        }
        return true;
    }
    const char *error() const override { return error_; }
};
Card card;
sd_log::Files files(card,"/sdcard",esp_random(),TRIMIX_ANALYZER_VERSION);
sd_log::Logger logger(files);
std::atomic<bool> started{false};
void worker(void *) {
    for(;;) { logger.pump(static_cast<uint32_t>(uptime())); vTaskDelay(pdMS_TO_TICKS(10)); }
}
}
void sd_log_start() {
    if(started.exchange(true))return;
    if(xTaskCreate(worker,"sd_writer",6144,nullptr,2,nullptr)!=pdPASS) {logger.startup_failed();started.store(false);}
}
bool sd_log_pause(uint32_t timeout) {
    logger.pause();
    if(!started.load())return true;
    const uint64_t start=uptime();
    while(!logger.quiescent() && uptime()-start<timeout)vTaskDelay(pdMS_TO_TICKS(10));
    return logger.quiescent();
}
#else
#include <chrono>
namespace {
uint64_t uptime() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
class NoCard final : public sd_log::Volume {
public:
    bool mount(uint64_t &) override { return false; }
    bool unmount() override {return true;}
    const char *error() const override { return "Simulator: no physical SD card"; }
};
NoCard card;
sd_log::Files files(card,"/sdcard",0,TRIMIX_ANALYZER_VERSION);
sd_log::Logger logger(files);
}
void sd_log_start() { logger.pump(static_cast<uint32_t>(uptime())); }
bool sd_log_pause(uint32_t) { logger.pause(); logger.pump(static_cast<uint32_t>(uptime())); return logger.quiescent(); }
#endif
void sd_log_mode(sd_log::Mode mode) { logger.mode(mode); }
void sd_log_end_mode(sd_log::Mode expected) { logger.end_mode(expected); }
sd_log::Status sd_log_status() { return logger.status(); }
void sd_log_resume() { logger.resume(); }
void sd_log_eject() { logger.pause(true); }
void sd_log_retry() { logger.retry(); sd_log_start(); }
void sd_log_raw(gas_cal_channel_t channel,const gas_raw_sample_t &r,const oxygen_selection_status_t &selection,uint32_t calibration,uint32_t acquisition) {
    char row[sd_log::Logger::kRowBytes];
    logger.submit(sd_log::Kind::Raw,sd_log::raw(row,sizeof(row),uptime(),channel,r,selection,calibration,acquisition)?row:nullptr);
}
void sd_log_result(const sensor_readings_t &r,const analysis_result_t *a) {
    char row[sd_log::Logger::kRowBytes];
    logger.submit(sd_log::Kind::Results,sd_log::result(row,sizeof(row),uptime(),r,a)?row:nullptr);
}
void sd_log_calibration_event(const char *event,gas_cal_channel_t channel,gas_cal_result_t result,const gas_cal_reference_t *reference) {
    gas_cal_record_t record{}; const bool saved=gas_calibration_get_record(channel,&record);
    char row[sd_log::Logger::kRowBytes];
    // event is an internal literal, never user-entered text or an SD pathname.
    const int length=std::snprintf(row,sizeof(row),"%llu,%s,%d,%d,%lu,%lu,%.6f,%.6f,%.6f,%.9f,%.9f",
        static_cast<unsigned long long>(uptime()),event,int(channel),int(result),
        static_cast<unsigned long>(saved?record.revision:0),static_cast<unsigned long>(saved?record.acquisition_revision:0),
        reference?reference->oxygen_percent:NAN,reference?reference->helium_percent:NAN,
        reference && reference->uncertainty_known?reference->uncertainty_pp:NAN,
        saved?record.percent_per_volt:NAN,saved?-record.zero_voltage_v*record.percent_per_volt:NAN);
    logger.submit(sd_log::Kind::Event,length>=0 && size_t(length)<sizeof(row)?row:nullptr);
}
