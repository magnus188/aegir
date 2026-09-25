#include "services/sd_log_core.h"
#include "services/sd_log_files.h"
#include "services/sd_log_format.h"
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
#include <unistd.h>
using namespace sd_log;
unsigned checks=0;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"Failed line %d: %s\n",__LINE__,#x);std::abort(); } ++checks; } while(0)
struct Fake : Backend {
    bool available=true,open_ok=true,write_ok=true,sync_ok=true,close_ok=true,unmount_ok=true;
    unsigned mounts=0,opens=0,closes=0,syncs=0,unmounts=0;
    std::vector<std::string> rows;
    std::function<void()> on_open,on_write,on_mount;
    bool mount(uint64_t &capacity) override {++mounts;capacity=32000000000ULL;if(on_mount)on_mount();return available;}
    bool open(uint32_t session,Mode,char *p,size_t n) override {++opens;std::snprintf(p,n,"session-%u",session);if(on_open)on_open();return open_ok;}
    bool append(Kind,const char *row) override {if(on_write)on_write();if(write_ok)rows.push_back(row);return write_ok;}
    bool flush() override {++syncs;return sync_ok;}
    bool close() override {++closes;return close_ok;}
    bool unmount() override {++unmounts;return unmount_ok;}
    const char *error() const override {return "injected media failure";}
};
void state_tests() {
    Fake io;Logger l(io);l.pump(0);CHECK(l.status().state==State::Ready);CHECK(io.mounts==1);
    CHECK(!l.submit(Kind::Raw,"idle"));l.mode(Mode::Analysis);
    CHECK(l.submit(Kind::Results,"1,valid"));l.pump(1);CHECK(io.opens==1);CHECK(l.status().written==1);
    CHECK(io.rows[0]=="0,1,valid\n");CHECK(l.status().state==State::Recording);
    l.pump(1001);CHECK(io.syncs==1);CHECK(l.status().synced==1);l.mode(Mode::Idle);l.pump(1010);CHECK(l.status().closed);
    l.mode(Mode::Calibration);CHECK(l.submit(Kind::Raw,"2,nan,fault"));l.pump(1011);CHECK(io.opens==2);
    l.pause();CHECK(!l.submit(Kind::Raw,"paused"));CHECK(!l.quiescent());l.pump(1012);CHECK(l.quiescent());
    l.retry();l.pump(1012);CHECK(l.quiescent());CHECK(io.mounts==1); // UI retry cannot defeat maintenance
    l.resume();CHECK(l.submit(Kind::Raw,"3,resumed"));l.pump(1013);CHECK(io.opens==3);
    l.pause(true);l.pump(1014);CHECK(l.quiescent());CHECK(l.status().state==State::Ejected);
    l.resume();CHECK(!l.submit(Kind::Raw,"ejected"));CHECK(io.mounts==1);
    l.retry();l.pump(1015);CHECK(io.mounts==2);CHECK(l.submit(Kind::Raw,"retry"));l.pump(1016);
    CHECK(l.status().written==4);
}
void early_pause_tests() {
    Fake io;Logger l(io);l.mode(Mode::Analysis);CHECK(l.submit(Kind::Raw,"before mount"));
    l.pause();CHECK(!l.quiescent());l.pump(1);CHECK(l.quiescent());CHECK(io.mounts==0);CHECK(l.status().dropped==1);
    l.resume();l.pump(2);CHECK(io.mounts==1);CHECK(l.status().state==State::Ready);
    l.pause(true);CHECK(!l.quiescent());l.pump(3);CHECK(l.quiescent());CHECK(io.unmounts==1);
}
void failure_tests() {
    for(unsigned fault=0;fault<5;++fault) {
        Fake io;Logger l(io);if(fault==0)io.available=false;l.mode(Mode::Analysis);l.pump(0);
        if(fault==0) {CHECK(l.status().state==State::Unavailable);CHECK(!l.submit(Kind::Raw,"x"));}
        else {
            CHECK(l.submit(Kind::Raw,"x"));CHECK(l.submit(Kind::Raw,"y"));
            if(fault==1)io.open_ok=false;if(fault==2)io.write_ok=false;
            l.pump(1);
            if(fault==3) {io.sync_ok=false;l.pump(1001);}
            if(fault==4) {io.close_ok=false;l.pause();l.pump(2);l.pump(3);}
            CHECK(l.status().state==State::Unavailable);CHECK(l.status().closed);
        }
        l.pause();l.pump(4);CHECK(l.quiescent()); // failed optional media cannot block OTA forever
        l.resume();l.pump(5);CHECK(io.mounts==1); // no unbounded retry loop
        io.available=io.open_ok=io.write_ok=io.sync_ok=io.close_ok=io.unmount_ok=true;
        l.retry();l.pump(6);CHECK(io.mounts==2);CHECK(l.status().state==State::Ready);
    }
}
void queue_tests() {
    Fake io;Logger l(io);l.pump(0);l.mode(Mode::Analysis);
    for(unsigned i=0;i<Logger::kCapacity;++i)CHECK(l.submit(Kind::Raw,"queued"));
    CHECK(!l.submit(Kind::Raw,"overflow"));CHECK(l.status().dropped==1);
    l.pause();for(unsigned i=0;i<Logger::kCapacity+1;++i)l.pump(i+1);
    CHECK(l.quiescent());CHECK(l.status().written==Logger::kCapacity);CHECK(io.rows[0]=="1,queued\n");
    l.resume();std::string oversize(Logger::kRowBytes,'x');CHECK(!l.submit(Kind::Raw,oversize.c_str()));CHECK(l.status().dropped==2);
    CHECK(l.submit(Kind::Raw,"wrap"));l.pump(UINT32_MAX-10);const auto sync=io.syncs;l.pump(1000);CHECK(io.syncs==sync+1);
}
void unmount_tests() {
    Fake io;Logger l(io);l.pump(0);l.mode(Mode::Analysis);CHECK(l.submit(Kind::Raw,"one"));l.pump(1);
    io.unmount_ok=false;l.pause(true);l.pump(2);CHECK(l.status().state==State::Unavailable);
    CHECK(std::string(l.status().detail).find("removal not confirmed")!=std::string::npos);
    const auto attempts=io.unmounts;l.pump(3);CHECK(io.unmounts==attempts); // no background retry storm
    l.retry();l.pump(4);CHECK(l.status().state==State::Unavailable);CHECK(io.mounts==1);
    l.pause();l.retry();l.pump(5);CHECK(l.quiescent());CHECK(io.mounts==1); // retained mount has no open files or I/O
    l.resume();io.unmount_ok=true;l.retry();l.pump(6);CHECK(l.status().state==State::Ready);CHECK(io.mounts==2);
}
void rollover_failure_tests() {
    Fake io;Logger l(io);l.pump(0);l.mode(Mode::Analysis);
    CHECK(l.submit(Kind::Raw,"old session"));l.pump(1);
    l.mode(Mode::Calibration);CHECK(l.submit(Kind::Raw,"popped row"));CHECK(l.submit(Kind::Raw,"queued row"));
    io.close_ok=false;io.unmount_ok=false;l.pump(2);
    CHECK(l.status().state==State::Unavailable);CHECK(l.status().closed);
    CHECK(io.opens==1);CHECK(io.rows.size()==1);CHECK(l.status().dropped==2);
    l.pause();l.pump(3);CHECK(l.quiescent());CHECK(io.opens==1);
    l.resume();io.close_ok=io.unmount_ok=true;l.retry();l.pump(4);
    CHECK(l.status().state==State::Ready);CHECK(l.submit(Kind::Raw,"explicit retry"));l.pump(5);CHECK(io.rows.size()==2);
}
void concurrent_tests() {
    for(unsigned point=0;point<3;++point) {
        Fake io;Logger l(io);std::mutex m;std::condition_variable cv;bool inside=false,release=false;
        auto stall=[&] {std::unique_lock<std::mutex> lock(m);inside=true;cv.notify_all();cv.wait(lock,[&]{return release;});};
        if(point==0)io.on_mount=stall;
        else {l.pump(0);if(point==1)io.on_open=stall;else io.on_write=stall;}
        l.mode(Mode::Analysis);CHECK(l.submit(Kind::Raw,"first"));
        std::thread writer([&]{l.pump(1);});
        {std::unique_lock<std::mutex> lock(m);cv.wait(lock,[&]{return inside;});}
        CHECK(l.submit(Kind::Raw,"during IO")); // producer never waits for device I/O
        l.pause();CHECK(!l.quiescent());CHECK(!l.submit(Kind::Raw,"after barrier"));
        {std::lock_guard<std::mutex> lock(m);release=true;cv.notify_all();}
        writer.join();io.on_open=io.on_write=io.on_mount=nullptr;
        l.pump(2);l.pump(3);CHECK(l.quiescent());CHECK(l.status().written==2);
    }
}
struct TempVolume : Volume {
    bool mounted=false;
    bool mount(uint64_t &c) override {mounted=true;c=123456;return true;}
    bool unmount() override {mounted=false;return true;}
    const char *error() const override {return "volume error";}
};
std::string read(const std::filesystem::path &p) {std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
void filesystem_tests() {
    char name[]="/tmp/trimix-sd-XXXXXX";CHECK(mkdtemp(name));const std::filesystem::path root(name);
    std::ofstream(root/"USER.TXT")<<"user data\n";
    TempVolume v;Files files(v,name,0x12345678,"test");Logger l(files);l.mode(Mode::Analysis);l.pump(0);
    CHECK(l.submit(Kind::Results,"1,2,3"));l.pump(1);const auto first=l.status();
    l.pause(true);l.pump(2);CHECK(l.quiescent());CHECK(!v.mounted);
    CHECK(read(root/"USER.TXT")=="user data\n");
    const auto csv=read(std::filesystem::path(first.path)/"RESULTS.CSV");
    CHECK(csv.find("schema=2")!=std::string::npos);CHECK(csv.find("dropped_total,uptime_ms")!=std::string::npos);
    CHECK(csv.find("severity,helium_calibration_revision\n")!=std::string::npos);
    CHECK(csv.find("0,1,2,3\n")!=std::string::npos);
    // Reboot with identical identity chooses a different path, preserving old rows.
    TempVolume v2;Files files2(v2,name,0x12345678,"test");Logger second(files2);second.mode(Mode::Analysis);second.pump(0);
    CHECK(second.submit(Kind::Raw,"4,nan"));second.pump(1);second.pause(true);second.pump(2);
    CHECK(std::string(second.status().path)!=first.path);
    CHECK(read(std::filesystem::path(first.path)/"RESULTS.CSV")==csv);
    std::filesystem::remove_all(root);
}
std::vector<std::string> fields(const char *row) {std::vector<std::string> out;std::stringstream s(row);std::string x;while(std::getline(s,x,','))out.push_back(x);return out;}
void format_tests() {
    char row[Logger::kRowBytes];gas_raw_sample_t sample{};sample.timestamp_ms=12;sample.sequence=17;sample.adc_code=-123;sample.voltage_v=NAN;sample.faults=GAS_FAULT_NONFINITE;
    oxygen_selection_status_t selected{};
    CHECK(raw(row,sizeof(row),0x100000001ULL,GAS_CAL_HELIUM,sample,selected,55,6));auto r=fields(row);
    CHECK(r.size()==19);CHECK(r[0]=="4294967297");CHECK(r[1]=="12");CHECK(r[2]=="2");CHECK(r[4]=="17");CHECK(r[6]=="-123");CHECK(r[7]=="nan");CHECK(r[15]=="55");CHECK(r[18]=="6");
    CHECK(!raw(row,8,1,GAS_CAL_AO2,sample,selected,0,0));
    sensor_readings_t input{};input.co_ppm=99;input.temperature_c=40;input.humidity_pct=70;input.pressure_bar=1.2;
    input.oxygen_calibration_revision=7;input.helium_calibration_revision=13;
    CHECK(result(row,sizeof(row),100,input,nullptr));auto v=fields(row);CHECK(v.size()==28);CHECK(v[6]=="7");CHECK(v[27]=="13");
    CHECK(v[8]=="0");CHECK(v[11]=="nan");CHECK(v[14]=="nan");CHECK(v[16]=="nan");CHECK(v[17]=="nan");CHECK(v[18]=="nan");CHECK(v[23]=="-1");
    input.co_valid=true;input.environment_valid=true;analysis_result_t analysis{};analysis.valid=true;analysis.oxygen_percent=21;analysis.helium_percent=35;analysis.nitrogen_percent=44;
    CHECK(result(row,sizeof(row),101,input,&analysis));v=fields(row);CHECK(v[8]=="1");CHECK(v[11]=="21.000000");CHECK(v[12]=="35.000000");CHECK(v[14]=="99.000");CHECK(v[16]=="40.000");
    CHECK(!result(row,12,102,input,&analysis));
}
int main() {state_tests();early_pause_tests();failure_tests();queue_tests();unmount_tests();rollover_failure_tests();concurrent_tests();filesystem_tests();format_tests();std::printf("SD production core/files/format: %u assertions passed\n",checks);}
