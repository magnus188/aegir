#include "services/backlight_service.h"
#include <esp_err.h>
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
namespace {
unsigned passed=0,failed=0;
std::atomic<bool> fail_pwm{false};
std::atomic<unsigned> active{0},overlap{0};
std::atomic<uint8_t> physical_pwm{100};
void check(bool ok,const char *label) {
    std::cout<<(ok?"PASS ":"FAIL ")<<label<<'\n';ok?++passed:++failed;
}
}
extern "C" esp_err_t guition_backlight_set(uint8_t percent) {
    if(active.fetch_add(1))++overlap;
    std::this_thread::yield();
    const bool failed=fail_pwm.load();
    if(!failed)physical_pwm.store(percent);
    --active;return failed?ESP_FAIL:ESP_OK;
}
int main() {
    backlight_init();backlight_set(63);
    check(backlight_get()==63 && physical_pwm==63,"Normal brightness reaches actual board PWM boundary");
    check(backlight_set_standby(true) && physical_pwm==0 && backlight_is_standby(),"Standby commands actual zero PWM, bypassing normal10-percent minimum");
    check(backlight_get()==63,"Blanking preserves saved brightness");
    backlight_set(48);
    check(physical_pwm==0 && backlight_get()==48 && backlight_is_standby(),"Settings changes cannot relight standby screen");
    check(backlight_set_standby(false) && physical_pwm==48 && !backlight_is_standby(),"Wake restores the latest saved brightness");
    fail_pwm=true;
    check(!backlight_set_standby(true) && physical_pwm==48 && !backlight_is_standby(),"Failed screen-off write does not falsely claim a dark display");
    backlight_set(75);check(backlight_get()==48,"Failed normal PWM update preserves saved state");
    fail_pwm=false;backlight_set_standby(true);fail_pwm=true;
    check(!backlight_set_standby(false) && backlight_is_standby() && physical_pwm==0,"Failed wake leaves confirmed override active");
    fail_pwm=false;
    std::vector<std::thread> threads;
    for(unsigned n=0;n<4;++n)threads.emplace_back([n] {
        for(unsigned i=0;i<2000;++i) {
            if(n%2)backlight_set(static_cast<uint8_t>((i%91)+10));
            else {backlight_set_standby((i&1)==0);(void)backlight_get();(void)backlight_is_standby();}
        }
    });
    for(auto &t:threads)t.join();
    check(overlap==0,"8000 concurrent settings/standby operations serialize the actual PWM boundary");
    backlight_set_standby(true);backlight_set(100);
    check(physical_pwm==0 && backlight_get()==100,"Concurrent workload ends with independently verified dark override");
    backlight_set_standby(false);backlight_set(0);
    check(physical_pwm==10 && backlight_get()==10,"Ordinary minimum brightness remains10percent");
    backlight_set(255);check(physical_pwm==100 && backlight_get()==100,"Ordinary maximum brightness remains100percent");
    std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
