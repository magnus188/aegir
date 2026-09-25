#pragma once
#include "ads122c04.h"
#include "services/gas_calibration_service.h"

namespace gas_acquisition {
class Outputs {
public:
    virtual ~Outputs()=default;
    virtual void heater(bool on)=0;
    virtual bool stopping() const=0;
};
struct Request { int oxygen_channel=-1; bool probe=false; uint32_t generation=0; };
struct Frame {
    gas_raw_sample_t channels[GAS_CAL_CHANNEL_COUNT]{};
    bool sampled[GAS_CAL_CHANNEL_COUNT]{};
    bool reset_history=false;
    bool excitation_latched=false;
    uint32_t generation=0, sequence=0;
};
// Runs the actual initialization/retry/measurement/rail-check sequence on both
// target and host. GPIO, transport and time are injected for fault testing.
class Engine {
public:
    Engine(ads122::Transport &io,Outputs &outputs,bool excitation_latched=false);
    Frame step(const Request &request);
    void stop();
private:
    static uint32_t fault(ads122::Result result);
    gas_raw_sample_t sample(const ads122::Sample &,ads122::Result,uint32_t extra,bool warmed);
    ads122::Transport &io_;
    Outputs &outputs_;
    ads122::Adc oxygen_,helium_;
    bool oxygen_ready_=false,helium_ready_=false,heater_on_=false,excitation_latched_=false;
    bool oxygen_attempted_=false,helium_attempted_=false,request_seen_=false;
    uint32_t oxygen_retry_=0,helium_retry_=0,heater_start_=0,oxygen_start_=0;
    uint32_t generation_=0,sequence_=0;
    int selection_=-1;
    bool probe_=false;
};
}
