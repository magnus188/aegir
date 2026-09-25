#include "sensors/co_qualification.h"
#include <iostream>
#include <limits>

namespace {
unsigned passed=0,failed=0;
void check(bool ok,const char *text){std::cout<<(ok?"PASS ":"FAIL ")<<text<<'\n';ok?++passed:++failed;}
void frame(co_qualification::Monitor &m,uint32_t now,unsigned tenths=25,bool good=true) {
    uint8_t bytes[]={0xff,4,3,1,uint8_t(tenths>>8),uint8_t(tenths),0x13,0x88,0};
    for(unsigned i=1;i<8;++i)bytes[8]-=bytes[i];
    if(!good)++bytes[8];
    for(auto b:bytes)m.push(b,now);
}
}
int main(){
    using namespace co_qualification;
    Monitor m;uint32_t now=1234;
    Environment env{true,now,20,50};frame(m,now);
    auto r=m.sample(now,env);
    check(!r.valid && std::isnan(r.ppm) && (r.faults&InterfaceUnavailable),"Disabled interface ignores frames and cannot qualify CO");
    m.interface_available(true,now);const auto enabled=now;
    now=enabled+kWarmupMs-1;env.timestamp_ms=now;frame(m,now);
    check(m.sample(now,env).faults==Warming,"One millisecond short of full five-minute interface wait remains warming");
    ++now;env.timestamp_ms=now;m.interface_available(true,now);
    r=m.sample(now,env);check(r.valid && r.ppm==2.5f,"Full warm-up qualifies a fresh frame; repeated successful interface commands do not reset time");
    for(float t:{-10.0f,55.0f})for(float h:{15.0f,90.0f}) {
        env.temperature_c=t;env.humidity_percent=h;
        check(m.sample(now,env).valid,"Manufacturer temperature and humidity endpoints are inclusive");
    }
    env={true,now,20,50};
    for(float t:{std::nextafter(-10.0f,-INFINITY),std::nextafter(55.0f,INFINITY)}) {
        env.temperature_c=t;r=m.sample(now,env);
        check(!r.valid && std::isnan(r.ppm) && (r.faults&TemperatureRange),"Just outside either temperature limit suppresses CO");
    }
    env.temperature_c=20;
    for(float h:{std::nextafter(15.0f,-INFINITY),std::nextafter(90.0f,INFINITY)}) {
        env.humidity_percent=h;r=m.sample(now,env);
        check(!r.valid && std::isnan(r.ppm) && (r.faults&HumidityRange),"Just outside either humidity limit suppresses CO");
    }
    for(float bad:{NAN,INFINITY,-INFINITY}) {
        env={true,now,bad,50};check(m.sample(now,env).faults&EnvironmentNonfinite,"Nonfinite temperature rejected");
        env={true,now,20,bad};check(m.sample(now,env).faults&EnvironmentNonfinite,"Nonfinite humidity rejected");
    }
    env={false,now,20,50};r=m.sample(now,env);
    check(!r.valid && (r.faults&EnvironmentUnavailable),"Plausible numbers cannot override missing environment validity");
    env={true,now-kEnvironmentMaxAgeMs,20,50};check(m.sample(now,env).valid,"Environment at existing three-second freshness limit is accepted");
    --env.timestamp_ms;check(m.sample(now,env).faults&EnvironmentStale,"One millisecond beyond environment freshness suppresses CO");
    env.timestamp_ms=now+1;check(m.sample(now,env).faults&EnvironmentStale,"Future timestamp cannot hide stale environment");
    env={true,now,20,50};frame(m,now,5000);r=m.sample(now,env);
    check(r.valid && r.ppm==500.0f,"Valid full-scale CO remains reported, not silently suppressed");
    now+=3001;env.timestamp_ms=now;frame(m,now,25,false);
    check(m.sample(now,env).faults&FrameUnavailable,"Fresh environment and bad checksum cannot refresh expired CO");
    frame(m,now);check(m.sample(now,env).valid,"Fresh valid frame recovers independently after a data fault");
    m.interface_available(false,now);m.interface_available(true,now);frame(m,now);
    check(m.sample(now,env).faults&Warming,"Interface loss clears prior frame and restarts full qualification wait");
    Monitor wrap;const uint32_t start=UINT32_MAX-kWarmupMs+10;wrap.interface_available(true,start);
    now=start+kWarmupMs;env={true,now-1000,20,50};frame(wrap,now);
    check(wrap.sample(now,env).valid,"Warm-up and fresh environment survive uint32 clock wrap");
    env.timestamp_ms=now-3001;check(wrap.sample(now,env).faults&EnvironmentStale,"Stale environment across wrap remains invalid");
    check(std::string(fault_message(HumidityRange)).find("15..90")!=std::string::npos &&
          std::string(fault_message(FrameUnavailable)).find("frame")!=std::string::npos,
          "Diagnostic messages distinguish environment range from missing UART evidence");
    std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
