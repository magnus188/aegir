#include "acquisition_engine.h"
#include <cmath>

namespace gas_acquisition {
namespace { constexpr uint32_t retry_ms=5000, oxygen_warmup_ms=10000, helium_warmup_ms=180000; }
Engine::Engine(ads122::Transport &io,Outputs &outputs,bool latched)
    :io_(io),outputs_(outputs),oxygen_(io,ads122::kOxygenAddress),helium_(io,ads122::kHeliumAddress),excitation_latched_(latched) {
    outputs_.heater(false);
}
uint32_t Engine::fault(ads122::Result r) {
    switch(r) {
    case ads122::Result::Ok:return 0;
    case ads122::Result::Clipped:return GAS_FAULT_CLIPPED;
    case ads122::Result::ConfigMismatch:case ads122::Result::InvalidConfig:return GAS_FAULT_ADC_CONFIG;
    default:return GAS_FAULT_TRANSPORT;
    }
}
gas_raw_sample_t Engine::sample(const ads122::Sample &s,ads122::Result r,uint32_t extra,bool warmed) {
    gas_raw_sample_t raw{};
    raw.faults=fault(r)|extra; raw.voltage_v=raw.faults ? NAN:s.volts;
    raw.adc_code=s.code; raw.gain=s.gain; raw.timestamp_ms=r==ads122::Result::Ok ? s.timestamp_ms:io_.now_ms();
    raw.sequence=sequence_; raw.warmed=warmed;
    raw.temperature_c=raw.humidity_pct=raw.pressure_bar=raw.excitation_v=raw.bias_v=NAN;
    return raw;
}
void Engine::stop() { outputs_.heater(false); heater_on_=false; }
Frame Engine::step(const Request &req) {
    Frame out; out.generation=req.generation; out.sequence=++sequence_;
    if (outputs_.stopping()) { stop(); return out; }
    uint32_t now=io_.now_ms();
    if (!request_seen_ || generation_!=req.generation || selection_!=req.oxygen_channel || probe_!=req.probe) {
        generation_=req.generation; selection_=req.oxygen_channel; probe_=req.probe;
        request_seen_=true; oxygen_start_=now; out.reset_history=true;
    }
    if ((selection_>=0 || probe_) && !oxygen_ready_ && (!oxygen_attempted_ || now-oxygen_retry_>=retry_ms)) {
        oxygen_attempted_=true; oxygen_retry_=now;
        oxygen_ready_=oxygen_.initialize()==ads122::Result::Ok;
        if (oxygen_ready_) { oxygen_start_=io_.now_ms(); out.reset_history=true; }
    }
    if (!helium_ready_ && (!helium_attempted_ || now-helium_retry_>=retry_ms)) {
        helium_attempted_=true; helium_retry_=now;
        helium_ready_=helium_.initialize()==ads122::Result::Ok;
        if (helium_ready_ && !excitation_latched_ && !outputs_.stopping()) {
            outputs_.heater(true); heater_on_=true; heater_start_=io_.now_ms();
            out.reset_history=true; io_.delay_ms(20);
        }
    }
    ads122::Sample a{},b{},h{},e{},v{};
    auto ra=ads122::Result::Transport, rb=ra, rh=ra, re=ra, rv=ra;
    if (oxygen_ready_ && !outputs_.stopping()) {
        if (probe_ || selection_==GAS_CAL_AO2) ra=oxygen_.measure(ads122::kOxygenA,a);
        if (!outputs_.stopping() && (probe_ || selection_==GAS_CAL_JJCCR)) rb=oxygen_.measure(ads122::kOxygenB,b);
    }
    if (helium_ready_ && !outputs_.stopping()) {
        if (!excitation_latched_) rh=helium_.measure(ads122::kHelium,h);
        if (!excitation_latched_ && !outputs_.stopping()) re=helium_.measure(ads122::kExcitation,e);
        if (!outputs_.stopping()) rv=helium_.measure(ads122::kBias,v);
    }
    if (outputs_.stopping()) { stop(); return out; }
    now=io_.now_ms();
    const float excitation=re==ads122::Result::Ok ? e.volts*2:NAN;
    const float bias=rv==ads122::Result::Ok ? v.volts:NAN;
    uint32_t common=fault(rv), he_faults=fault(re);
    if (rv==ads122::Result::Ok && (bias<1.4f || bias>1.9f)) common|=GAS_FAULT_BIAS;
    if (re==ads122::Result::Ok && (excitation<2.9f || excitation>3.1f)) excitation_latched_=true;
    if (excitation_latched_) he_faults|=GAS_FAULT_EXCITATION;
    // Bias is measured by U502 even for O2: without that diagnostic O2 retains
    // its raw voltage for troubleshooting but is not accepted as a valid gas.
    const bool o2_warm=now-oxygen_start_>=oxygen_warmup_ms;
    out.sampled[GAS_CAL_AO2]=probe_ || selection_==GAS_CAL_AO2;
    out.sampled[GAS_CAL_JJCCR]=probe_ || selection_==GAS_CAL_JJCCR;
    out.sampled[GAS_CAL_HELIUM]=true;
    out.channels[GAS_CAL_AO2]=sample(a,ra,common,o2_warm);
    out.channels[GAS_CAL_JJCCR]=sample(b,rb,common,o2_warm);
    out.channels[GAS_CAL_HELIUM]=sample(h,rh,he_faults|common,heater_on_ && now-heater_start_>=helium_warmup_ms);
    for (auto &r:out.channels) { r.excitation_v=excitation; r.bias_v=bias; }
    const bool oxygen_transport=(out.sampled[0] && ra!=ads122::Result::Ok && ra!=ads122::Result::Clipped) ||
        (out.sampled[1] && rb!=ads122::Result::Ok && rb!=ads122::Result::Clipped);
    if (oxygen_transport) { oxygen_ready_=false; oxygen_retry_=now; }
    if (fault(rh) || he_faults || common) stop();
    if (common || (!excitation_latched_ && (fault(rh) || fault(re)))) {
        helium_ready_=false; helium_retry_=now;
    }
    out.excitation_latched=excitation_latched_;
    return out;
}
}
