#include "services/sd_log_core.h"
#include "services/storage_service.h"
#include "services/maintenance_service.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <future>
#include <thread>
namespace {
unsigned assertions=0;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"SD maintenance failure line %d: %s\n",__LINE__,#x);std::abort();}++assertions;}while(0)
struct Medium : sd_log::Backend {
    std::atomic<bool> blocked{false},inside{false};bool available=true;
    bool mount(uint64_t &) override{return available;}
    bool open(uint32_t,sd_log::Mode,char *,size_t) override{return true;}
    bool append(sd_log::Kind,const char *) override {inside=true;while(blocked.load())std::this_thread::sleep_for(std::chrono::milliseconds(1));return true;}
    bool flush() override{return true;}bool close() override{return true;}bool unmount() override{return true;}
    const char *error() const override{return "missing card";}
} medium;
sd_log::Logger logger(medium);
uint32_t now(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
bool stop(uint32_t ms){return ms>0;}bool power(){return true;}
bool flush(uint32_t ms){logger.pause();const auto start=now();while(!logger.quiescent() && now()-start<ms)std::this_thread::sleep_for(std::chrono::milliseconds(1));return logger.quiescent();}
void resume(){logger.resume();}
}
int main(){
    CHECK(storage_init());const uint32_t original=0x12345678;CHECK(storage_write_blob("sdtest",1,&original,sizeof(original)));
    maintenance_register_hooks({stop,flush,resume,power});logger.pump(now());logger.mode(sd_log::Mode::Analysis);
    std::atomic<bool> run{true};std::thread writer([&]{while(run){logger.pump(now());std::this_thread::sleep_for(std::chrono::milliseconds(1));}});
    medium.blocked=true;CHECK(logger.submit(sd_log::Kind::Raw,"before pause"));
    const auto deadline=now()+2000;while(!medium.inside && int32_t(deadline-now())>0)std::this_thread::yield();CHECK(medium.inside);
    const auto start=now();CHECK(!maintenance_begin(MAINTENANCE_OTA,25));CHECK(now()-start<500);
    CHECK(!maintenance_is_active());CHECK(storage_writes_allowed());CHECK(!logger.quiescent());
    auto drain=std::async(std::launch::async,[]{return maintenance_begin(MAINTENANCE_OTA,1000);});
    std::this_thread::sleep_for(std::chrono::milliseconds(10));CHECK(drain.wait_for(std::chrono::milliseconds(0))==std::future_status::timeout);
    medium.blocked=false;CHECK(drain.get());CHECK(maintenance_is_active());CHECK(logger.quiescent());
    CHECK(!logger.submit(sd_log::Kind::Raw,"must not write while Held"));logger.retry();CHECK(logger.quiescent());
    CHECK(!storage_write_blob("sdtest",1,&start,sizeof(start)));uint32_t preserved=0;
    CHECK(storage_read_blob("sdtest",1,&preserved,sizeof(preserved))==STORAGE_OK);CHECK(preserved==original);
    maintenance_finish(false);CHECK(!maintenance_is_active());CHECK(storage_writes_allowed());
    CHECK(logger.submit(sd_log::Kind::Results,"after cancellation"));
    CHECK(maintenance_begin(MAINTENANCE_SHUTDOWN,1000));CHECK(logger.quiescent());maintenance_finish(false);
    run=false;writer.join();
    logger.pause();logger.pump(now());
    // Idle writer wakeups cannot revoke an acknowledged maintenance barrier.
    for(unsigned i=0;i<1000;++i) {logger.pump(now());CHECK(logger.quiescent());}
    logger.resume();
    // Optional media absent still permits the normal NVS/OTA barrier.
    logger.pause(true);logger.pump(now());medium.available=false;logger.retry();logger.pump(now());
    CHECK(logger.status().state==sd_log::State::Unavailable);
    std::thread drain_missing([&]{std::this_thread::sleep_for(std::chrono::milliseconds(1));logger.pump(now());});
    CHECK(maintenance_begin(MAINTENANCE_OTA,1000));drain_missing.join();CHECK(logger.quiescent());maintenance_finish(false);
    CHECK(storage_read_blob("sdtest",1,&preserved,sizeof(preserved))==STORAGE_OK);CHECK(preserved==original);
    std::printf("Production SD/NVS/maintenance coordination: %u assertions passed\n",assertions);
}
