#include "sd_log_files.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace sd_log {
Files::Files(Volume &volume,const char *root,uint32_t boot_id,const char *firmware)
    :volume_(volume),boot_id_(boot_id) {
    std::snprintf(root_,sizeof(root_),"%s",root); std::snprintf(firmware_,sizeof(firmware_),"%s",firmware);
}
bool Files::fail(const char *operation) {
    std::snprintf(error_,sizeof(error_),"%s (error %d); existing files preserved",operation,errno); return false;
}
bool Files::mount(uint64_t &capacity) {
    if(!volume_.mount(capacity)) { std::snprintf(error_,sizeof(error_),"%s",volume_.error()); return false; }
    return true;
}
bool Files::open(uint32_t session,Mode mode,char *path,size_t size) {
    char directory[128]; std::snprintf(directory,sizeof(directory),"%s/TRIMIX",root_);
    if(mkdir(directory,0777)!=0 && errno!=EEXIST) return fail("SD log directory");
    // 8.3 names work without enabling long filenames. A collision always chooses
    // a new directory; neither an old file nor an incomplete session is reused.
    bool created=false;
    for(unsigned attempt=0;attempt<32;++attempt) {
        std::snprintf(directory,sizeof(directory),"%s/TRIMIX/%08lx",root_,static_cast<unsigned long>(boot_id_));
        if(mkdir(directory,0777)!=0 && errno!=EEXIST) return fail("SD boot directory");
        std::snprintf(directory,sizeof(directory),"%s/TRIMIX/%08lx/%08lx",root_,static_cast<unsigned long>(boot_id_),static_cast<unsigned long>(session));
        if(mkdir(directory,0777)==0) { created=true; break; }
        if(errno!=EEXIST) return fail("SD session directory");
        ++boot_id_;
    }
    if(!created) return fail("SD session collision limit");
    const char *names[]={"RESULTS.CSV","RAW.CSV","EVENTS.CSV"};
    const char *headers[]={
        "dropped_total,uptime_ms,sample_ms,sequence,source,selection,selection_generation,oxygen_calibration_revision,sensor_status,analysis_valid,measured_o2_percent,measured_he_percent,analysis_o2_percent,analysis_he_percent,n2_percent,co_ppm,co_valid,temperature_c,humidity_percent,pressure_bar,environment_valid,oxygen_calibrated,helium_calibrated,calibration_unvalidated,gas_mode,planned_depth_m,mod_m,severity,helium_calibration_revision",
        "dropped_total,uptime_ms,sample_ms,channel,selection,sequence,selection_generation,adc_code,voltage_v,gain,excitation_v,bias_v,temperature_c,humidity_percent,pressure_bar,environment_valid,observed_calibration_revision,fault_mask,warmed,acquisition_revision",
        "dropped_total,uptime_ms,event,channel,result,calibration_revision,acquisition_revision,reference_o2_percent,reference_he_percent,reference_uncertainty_pp,slope,intercept"
    };
    for(unsigned i=0;i<3;++i) {
        char filename[160]; std::snprintf(filename,sizeof(filename),"%s/%s",directory,names[i]);
        const int fd=::open(filename,O_WRONLY|O_CREAT|O_EXCL,0666);
        if(fd<0) { fail("SD create exclusive file"); close(); return false; }
        files_[i]=fdopen(fd,"w");
        if(!files_[i]) { ::close(fd); fail("SD file stream"); close(); return false; }
        if(std::fprintf(files_[i],"# Trimix CSV schema=2 firmware=%s mode=%s session=%lu boot=%08lx time=monotonic_no_UTC\n# Bench data; invalid values are nan; missing records are not interpolated.\n%s\n",
            firmware_,mode==Mode::Analysis?"analysis":"calibration",static_cast<unsigned long>(session),static_cast<unsigned long>(boot_id_),headers[i])<0) {
            fail("SD header write"); close(); return false;
        }
    }
    std::snprintf(path,size,"%s",directory); return true;
}
bool Files::append(Kind kind,const char *row) {
    const unsigned index=static_cast<unsigned>(kind);
    if(index>=3 || !files_[index]) { errno=EBADF; return fail("SD file unavailable"); }
    const size_t size=std::strlen(row);
    return std::fwrite(row,1,size,files_[index])==size || fail("SD write incomplete");
}
bool Files::flush() {
    bool ok=true;
    for(auto *file:files_) if(file && (std::fflush(file)!=0 || fsync(fileno(file))!=0)) ok=false;
    return ok || fail("SD sync failed");
}
bool Files::close() {
    bool ok=flush();
    for(auto &file:files_) if(file) { if(std::fclose(file)!=0) ok=false; file=nullptr; }
    return ok || fail("SD close failed");
}
bool Files::unmount() { return volume_.unmount(); }
}
