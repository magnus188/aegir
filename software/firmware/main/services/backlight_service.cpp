#include "backlight_service.h"
#include "board/guition_jc4880p443.h"
#include <esp_err.h>
#include <esp_log.h>
#include <mutex>
namespace {
constexpr char TAG[]="BACKLIGHT";
std::mutex lock;
uint8_t brightness=100;
bool initialized=false,standby=false;
bool apply(uint8_t target) {
    const esp_err_t err=guition_backlight_set(target);
    if(err!=ESP_OK) {ESP_LOGE(TAG,"Backlight apply failed: %s",esp_err_to_name(err));return false;}
    return true;
}
}
void backlight_init(void) {std::lock_guard<std::mutex> guard(lock);initialized=true;}
void backlight_set(uint8_t percent) {
    std::lock_guard<std::mutex> guard(lock);
    percent=percent<10 ? 10:percent>100 ? 100:percent;
    if(!apply(standby ? 0:percent))return;
    initialized=true;brightness=percent;
}
uint8_t backlight_get(void) {std::lock_guard<std::mutex> guard(lock);return brightness;}
bool backlight_set_standby(bool enabled) {
    std::lock_guard<std::mutex> guard(lock);
    if(!apply(enabled ? 0:brightness))return false;
    initialized=true;standby=enabled;return true;
}
bool backlight_is_standby(void) {std::lock_guard<std::mutex> guard(lock);return initialized && standby;}
