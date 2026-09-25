#include "sd_log_core.h"
#include <cstdio>
#include <cstring>

namespace sd_log {
const char *state_label(State state) {
    switch(state) {
    case State::Starting:return "SD starting";
    case State::Ready:return "SD ready";
    case State::Recording:return "SD recording";
    case State::Pausing:return "SD closing";
    case State::Paused:return "SD paused";
    case State::Ejected:return "SD safe to remove";
    case State::Unavailable:return "SD unavailable";
    }
    return "SD unavailable";
}
void Logger::mode(Mode value) {
    std::lock_guard<std::mutex> guard(lock_);
    if(mode_!=value) { mode_=value; ++generation_; }
    requested_.store(mode_!=Mode::Idle && !paused_ && !eject_);
}
void Logger::end_mode(Mode expected) {
    std::lock_guard<std::mutex> guard(lock_);
    if(mode_==expected) { mode_=Mode::Idle; ++generation_; requested_.store(false); }
}
bool Logger::submit(Kind kind,const char *text) {
    if(!requested_.load())return false;
    std::unique_lock<std::mutex> guard(lock_,std::try_to_lock);
    if(!guard.owns_lock()) { ++contention_drops_; return false; }
    if(mode_==Mode::Idle || paused_) return false;
    if(!text || std::strlen(text)>=kRowBytes || count_==kCapacity || status_.state==State::Unavailable || eject_) {
        ++status_.dropped; return false;
    }
    auto &row=rows_[(head_+count_)%kCapacity];
    row.session=generation_; row.mode=mode_; row.kind=kind;
    std::snprintf(row.text,sizeof(row.text),"%s",text);
    ++count_; ++status_.accepted; status_.closed=false;
    return true;
}
void Logger::pause(bool eject) {
    std::lock_guard<std::mutex> guard(lock_);
    paused_=true; held_=held_||!eject; eject_=eject_||eject;
    requested_.store(false);
    if(!status_.closed || count_) status_.state=State::Pausing;
}
void Logger::resume() {
    std::lock_guard<std::mutex> guard(lock_);
    held_=false;
    if(paused_ && !eject_) { paused_=false; ++generation_; }
    requested_.store(mode_!=Mode::Idle && !paused_ && !eject_);
}
void Logger::retry() {
    std::lock_guard<std::mutex> guard(lock_);
    if(!held_ && status_.closed && count_==0) { retry_=true; eject_=false; paused_=false; ++generation_; requested_.store(mode_!=Mode::Idle); }
}
void Logger::startup_failed() {
    std::lock_guard<std::mutex> guard(lock_);
    status_.dropped+=count_;count_=0;status_.closed=true;status_.state=State::Unavailable;
    std::snprintf(status_.detail,sizeof(status_.detail),"SD writer unavailable; local analyzer remains available");
}
bool Logger::quiescent() const {
    std::lock_guard<std::mutex> guard(lock_);
    const bool eject_done=!eject_ || status_.state==State::Ejected || status_.state==State::Unavailable;
    return paused_ && status_.closed && count_==0 && !busy_.load() && eject_done;
}
Status Logger::status() const {
    std::lock_guard<std::mutex> guard(lock_);
    auto out=status_; out.activity=mode_; out.dropped+=contention_drops_.load(); return out;
}
void Logger::failure(const char *message) {
    char saved[96]; std::snprintf(saved,sizeof(saved),"%s",message);
    backend_.close(); mounted_=!backend_.unmount(); open_generation_=0;
    std::lock_guard<std::mutex> guard(lock_);
    status_.dropped+=count_; count_=0;
    status_.state=State::Unavailable; status_.closed=true;
    std::snprintf(status_.detail,sizeof(status_.detail),"%s",saved);
}
bool Logger::close_current() {
    if(open_generation_) {
        const bool ok=backend_.close(); open_generation_=0;
        if(!ok) { failure("SD close failed; recent data may be incomplete"); return false; }
        std::lock_guard<std::mutex> guard(lock_); status_.synced=status_.written;
    }
    std::lock_guard<std::mutex> guard(lock_);
    status_.closed=count_==0;
    return true;
}
void Logger::pump(uint32_t now) {
    {
        std::lock_guard<std::mutex> guard(lock_);
        // Once maintenance has drained the writer, ordinary worker wakeups do
        // no I/O and must not make the acknowledged quiescent state transient.
        const bool eject_done=!eject_ || status_.state==State::Ejected || status_.state==State::Unavailable;
        if(paused_ && status_.closed && count_==0 && !open_generation_ && eject_done) return;
        busy_.store(true);
    }
    struct Finish { std::atomic<bool> &busy; ~Finish() { busy.store(false); } } finish{busy_};
    bool mount_requested=false;
    {
        std::lock_guard<std::mutex> guard(lock_);
        if(retry_ && !paused_) { retry_=false; mount_requested=true; status_.closed=false; }
    }
    if(mount_requested) {
        if(!close_current()) return;
        if(mounted_ && !backend_.unmount()) { failure("SD unmount failed; retry later, removal not confirmed"); return; }
        uint64_t capacity=0;
        if(!backend_.mount(capacity)) { failure(backend_.error()); return; }
        mounted_=true;
        std::lock_guard<std::mutex> guard(lock_);
        status_.capacity_bytes=capacity; status_.state=State::Ready;
        status_.closed=count_==0; std::snprintf(status_.detail,sizeof(status_.detail),"Mounted existing card; contents preserved");
    }
    Row row{}; bool has_row=false, close_needed=false, eject=false;
    {
        std::lock_guard<std::mutex> guard(lock_);
        if(paused_ && !mounted_ && count_) {
            // A shutdown before the first mount cannot wait for media startup.
            // These RAM-only records never reached a file; count their loss.
            status_.dropped+=count_;count_=0;status_.closed=true;
            std::snprintf(status_.detail,sizeof(status_.detail),"SD startup interrupted; queued rows were not saved");
        }
        if(count_ && mounted_) { row=rows_[head_]; head_=(head_+1)%kCapacity; --count_; has_row=true; status_.closed=false; }
        else close_needed=status_.state!=State::Unavailable && (paused_ || mode_==Mode::Idle || (open_generation_ && open_generation_!=generation_));
        eject=eject_;
    }
    if(has_row) {
        if(open_generation_!=row.session) {
            if(!close_current()) {
                // failure() counted queued rows, but this row was already
                // removed from the queue. Never reopen through a retained,
                // quarantined mount after a failed close/unmount sequence.
                std::lock_guard<std::mutex> guard(lock_); ++status_.dropped;
                return;
            }
            if(!mounted_) return;
            char path[96]{};
            if(!backend_.open(row.session,row.mode,path,sizeof(path))) {
                { std::lock_guard<std::mutex> guard(lock_); ++status_.dropped; }
                failure(backend_.error()); return;
            }
            open_generation_=row.session; last_flush_=now;
            std::lock_guard<std::mutex> guard(lock_);
            status_.session=row.session; status_.closed=false; status_.state=State::Recording;
            std::snprintf(status_.path,sizeof(status_.path),"%s",path);
        }
        // Every row carries cumulative queue loss. A missing sequence or growing
        // dropped count is evidence of a gap, never synthesized measurements.
        char annotated[kRowBytes+32]; const auto current=status();
        std::snprintf(annotated,sizeof(annotated),"%llu,%s\n",static_cast<unsigned long long>(current.dropped),row.text);
        if(!backend_.append(row.kind,annotated)) {
            { std::lock_guard<std::mutex> guard(lock_); ++status_.dropped; }
            failure(backend_.error()); return;
        }
        { std::lock_guard<std::mutex> guard(lock_); ++status_.written; }
    }
    if(open_generation_ && uint32_t(now-last_flush_)>=1000) {
        if(!backend_.flush()) { failure(backend_.error()); return; }
        { std::lock_guard<std::mutex> guard(lock_); status_.synced=status_.written; }
        last_flush_=now;
    }
    if(close_needed) {
        if(!close_current()) return;
        if(eject && mounted_) {
            if(!backend_.unmount()) { failure("SD unmount failed; retry later, removal not confirmed"); return; }
            mounted_=false;
        }
        std::lock_guard<std::mutex> guard(lock_);
        if(count_==0 && status_.closed && status_.state!=State::Unavailable) {
            status_.state=eject ? State::Ejected:paused_ ? State::Paused:State::Ready;
        }
    }
}
}
